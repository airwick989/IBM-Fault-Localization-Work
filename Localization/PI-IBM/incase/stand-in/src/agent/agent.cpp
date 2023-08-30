//Includes...
#include <jvmti.h>
#include <jni.h>

#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "agent.h"
#include "common/fileLogging.h"

// constants

// globals
bool using_thread_localStorage = false;

// functions
jvmtiError check_Deallocate(jvmtiEnv* env, char* mem) {
	jvmtiError err = JVMTI_ERROR_NONE;
	if (mem) {
		env->Deallocate((unsigned char *)mem);
		if ( err != JVMTI_ERROR_NONE ) {
			log_to_file(PI_VL_CRITICAL,"Error deallocating memory. Code %d\n", err);
		}
	}
	return err;
}

jvmtiError write_method_signature(FILE *method_file, jvmtiEnv *jvmti_env, jmethodID method) {
	jvmtiError err = JVMTI_ERROR_NONE;

	jclass j_class;
	char* generic_ptr = NULL;
	char* class_name = 0;
	char *method_name = 0;
	char *method_signature = 0;

	err = (jvmti_env)->GetMethodDeclaringClass(method, &j_class);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL, "Error getting declared class. Code: %d\n",
				err);
		goto wms_end;
	}

	err = (jvmti_env)->GetClassSignature(j_class, &class_name, &generic_ptr);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL, "Error getting class signature. Code %d\n",
				err);
		goto wms_end;
	}

	//We should have the className pointer setup nicely here.
	err = (jvmti_env)->GetMethodName(method, &method_name, &method_signature,
			&generic_ptr);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL, "Error getting method name. Code: %d\n",
				err);
		goto wms_end;
	}

	if (method_file) {
		fprintf(method_file, "%s %s %s", class_name, method_name, method_signature);
	}

wms_end:
	jvmti_env->Deallocate((unsigned char *)generic_ptr);
	jvmti_env->Deallocate((unsigned char *)class_name);
	jvmti_env->Deallocate((unsigned char *)method_name);
	jvmti_env->Deallocate((unsigned char *)method_signature);
	return err;
}

jvmtiError write_monitor_Info(FILE *file, jvmtiEnv *jvmti_env, JNIEnv* jni_env, jobject object) {
	jvmtiError err = JVMTI_ERROR_NONE;
	char* class_name = 0;
	char* generic_ptr = NULL;
	char *ownerName = 0;
	bool dealloc = false;
	jvmtiMonitorUsage jmonUsage;
	jclass object_klass = jni_env->GetObjectClass(object);

	err = jvmti_env->GetObjectMonitorUsage(object, &jmonUsage);
	if ( err != JVMTI_ERROR_NONE ) {
		log_to_file(PI_VL_CRITICAL,"Failed to get monitor usage info! Code: %d\n", err);
		return err;
	}
	jvmti_env->Deallocate((unsigned char *)jmonUsage.notify_waiters);
	jvmti_env->Deallocate((unsigned char *)jmonUsage.waiters);

	if (jmonUsage.owner) {
		err = getThreadName(jvmti_env, jmonUsage.owner, &ownerName, &dealloc);
		if ( err != JVMTI_ERROR_NONE ) {
			return err;
		}
	}

	err = (jvmti_env)->GetClassSignature (object_klass, &class_name, &generic_ptr);
	if ( err != JVMTI_ERROR_NONE ) {
		log_to_file(PI_VL_CRITICAL,"Error getting class signature. Code %d\n", err);
		if (dealloc)
			jvmti_env->Deallocate((unsigned char *)ownerName);
		return err;
	}

	if (file) {
		fprintf(file, "Class:%s", class_name);
		if (jmonUsage.owner)
			fprintf(file, "\tOwnerThread:%s", ownerName);
		if (jmonUsage.waiter_count)
			fprintf(file, "\tContendingThreads:%d", jmonUsage.waiter_count);
		if (jmonUsage.notify_waiter_count)
			fprintf(file, "\tWaitingThreads:%d", jmonUsage.notify_waiter_count);
	}

	jvmti_env->Deallocate((unsigned char *)class_name);
	jvmti_env->Deallocate((unsigned char *)generic_ptr);
	if (dealloc)
		jvmti_env->Deallocate((unsigned char *)ownerName);
	return err;
}

jvmtiError set_jvmti_callbacks(jvmtiEnv *jvmti, jvmtiEventCallbacks &callbacks) {
	jvmtiError err = JVMTI_ERROR_NONE;
	jvmtiCapabilities capabilities_ptr;
	jvmtiEvent events[20];
	int length = 0;

	memset(&capabilities_ptr, 0, sizeof(capabilities_ptr));

	// a2n callbacks
	if (callbacks.CompiledMethodLoad) {
		capabilities_ptr.can_generate_compiled_method_load_events = 1;
		events[length] = JVMTI_EVENT_COMPILED_METHOD_LOAD;
		length++;
	}
	if (callbacks.DynamicCodeGenerated) {
		events[length] = JVMTI_EVENT_DYNAMIC_CODE_GENERATED;
		length++;
	}

	// scs callbacks
	if (callbacks.VMObjectAlloc) {
		capabilities_ptr.can_generate_vm_object_alloc_events = 1;
		events[length] = JVMTI_EVENT_VM_OBJECT_ALLOC;
		length++;
	}
	// monitor callbacks
	if (callbacks.MonitorContendedEnter) {
		capabilities_ptr.can_generate_monitor_events = 1;
		capabilities_ptr.can_get_monitor_info = 1;
		events[length] = JVMTI_EVENT_MONITOR_CONTENDED_ENTER;
		length++;
	}
	if (callbacks.MonitorWait) {
		capabilities_ptr.can_generate_monitor_events = 1;
		capabilities_ptr.can_get_monitor_info = 1;
		events[length] = JVMTI_EVENT_MONITOR_WAIT;
		length++;
	}
	// thread callbacks
	if (callbacks.ThreadStart) {
		// assuming for thread local storage, add cpu time capabilities
		capabilities_ptr.can_get_thread_cpu_time = 1;
		events[length] = JVMTI_EVENT_THREAD_START;
		length++;
	}
	if (callbacks.ThreadEnd) {
		events[length] = JVMTI_EVENT_THREAD_END;
		length++;
	}
	if (callbacks.VMInit) {
		events[length] = JVMTI_EVENT_VM_INIT;
		length++;
	}

	err = (jvmti)->AddCapabilities(&capabilities_ptr);
	if ( err != JVMTI_ERROR_NONE ) {
		log_to_file(PI_VL_CRITICAL,"Error setting capabilities. Code: %d\n", err);
		return err;
	}

	err = (jvmti)->SetEventCallbacks(&callbacks, (jint)sizeof(callbacks));
	if ( err != JVMTI_ERROR_NONE ) {
		log_to_file(PI_VL_CRITICAL,"Error setting callback function. Code: %d\n", err);
		return err;
	}

	// events must be set after capabilities have been added
	for (int i=0; i < length; i++) {
		if(!events[i]) {
			continue;
		}
		err = (jvmti)->SetEventNotificationMode(JVMTI_ENABLE, events[i], NULL);
		if ( err != JVMTI_ERROR_NONE ) {
			log_to_file(PI_VL_CRITICAL,"Error setting notification mode. Code %d\n", err);
		}
	}

	return err;
}

jthread get_new_jthread(JNIEnv* jni_env, const char *thName) {
	jclass thrClass = 0;
	jmethodID constrId = 0;
	jthread thread = 0;
	jstring jstr = 0;

	jstr = jni_env->NewStringUTF(thName);

	thrClass = jni_env->FindClass("java/lang/Thread");
	if(!thrClass)
		return 0;

	constrId = jni_env->GetMethodID(thrClass, "<init>", "(Ljava/lang/String;)V");
	if(!thrClass)
		return 0;

	thread = jni_env->NewObject(thrClass, constrId, jstr);
	return thread;
}

void JNICALL
threadStart_initData(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread) {
	unsigned char *mem_ptr = 0;
	ThreadData *tdata;
	jvmtiError err;

	err = jvmti_env->Allocate(sizeof(ThreadData), &mem_ptr);
	if (err != JVMTI_ERROR_NONE || !mem_ptr) {
		log_to_file(PI_VL_CRITICAL,"Error allocating memory for thread data. Code: %d\n", err);
		return;
	}
	tdata = (ThreadData *)mem_ptr;

	jvmtiThreadInfo threadInfo;
	err = jvmti_env->GetThreadInfo(thread, &threadInfo);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL,
				"Error getting thread info. Code: %d\n", err);
		tdata->tName = 0;
	}
	else {
		tdata->tName = threadInfo.name;
	}

	err = jvmti_env->GetThreadCpuTime(thread, &tdata->readTime);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL,
				"Error getting thread CPU time. Code: %d\n", err);
		tdata->readTime = 0;
	}

	err = jvmti_env->SetThreadLocalStorage(thread, (const void *)tdata);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL,
				"Error setting thread local storage. Code: %d\n", err);
		// must clear the memory
		err = jvmti_env->Deallocate((unsigned char *)tdata->tName);
		err = jvmti_env->Deallocate((unsigned char *)tdata);
		return;
	}
	if (tdata->tName) {
		log_to_file(PI_VL_INFO,"Thread Start:\t%s\n", tdata->tName);
	}
	using_thread_localStorage = true;
}

void JNICALL
threadEnd_deallocData(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread) {
	ThreadData *tdata = 0;
	jvmtiError err;

	err = jvmti_env->GetThreadLocalStorage(thread, (void **)&tdata);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL,
				"Error getting thread local storage. Code: %d\n", err);
	}
	// must clear the memory
	if (tdata) {
		if (tdata->tName) {
			log_to_file(PI_VL_INFO,"Thread End:\t%s\n", tdata->tName);
		}
		err = jvmti_env->Deallocate((unsigned char *)tdata->tName);
		err = jvmti_env->Deallocate((unsigned char *)tdata);
	}
}


jvmtiError getThreadName(jvmtiEnv *jvmti_env, jthread thread, char **str_ptr, bool *pdealloc) {
	ThreadData *tdata = 0;
	jvmtiError err;
	if (!pdealloc)
		return JVMTI_ERROR_NULL_POINTER;

	err = jvmti_env->GetThreadLocalStorage(thread, (void **)&tdata);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL,
				"Error getting thread local storage. Code: %d\n", err);
		tdata = 0;
	}

	if (tdata && tdata->tName) {
		*str_ptr = tdata->tName;
	}
	else {
		jvmtiThreadInfo threadInfo;
		err = jvmti_env->GetThreadInfo(thread, &threadInfo);
		if (err != JVMTI_ERROR_NONE) {
			log_to_file(PI_VL_CRITICAL, "Error getting thread info. Code: %d\n", err);
			*str_ptr = 0;
			*pdealloc = false;
			return err;
		}
		*pdealloc = true;
		*str_ptr = threadInfo.name;
	}
	return err;
}

jvmtiError getThreadReadTime(jvmtiEnv *jvmti_env, jthread thread, long long int *pint) {
	ThreadData *tdata = 0;
	jvmtiError err;

	err = jvmti_env->GetThreadLocalStorage(thread, (void **)&tdata);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL,
				"Error getting thread local storage. Code: %d\n", err);
		tdata = 0;
	}

	if (tdata) {
		*pint = tdata->readTime;
	}
	else {
		*pint = 0;
	}
	return err;
}

jvmtiError setThreadReadTime(jvmtiEnv *jvmti_env, jthread thread, long long int nanos) {
	ThreadData *tdata = 0;
	jvmtiError err;

	err = jvmti_env->GetThreadLocalStorage(thread, (void **)&tdata);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL,
				"Error getting thread local storage. Code: %d\n", err);
		return err;
	}

	if (!tdata) {
		log_to_file(PI_VL_CRITICAL,
						"Thread local storage not set!\n");
		return JVMTI_ERROR_NOT_AVAILABLE;
	}
	tdata->readTime = nanos;
	return err;
}

jvmtiError getThreadElapsedTime(jvmtiEnv *jvmti_env, jthread thread, long long int *pint) {
	jvmtiError err = getThreadReadTime(jvmti_env, thread, pint);
	if (err != JVMTI_ERROR_NONE) {
		return err;
	}
	if (!*pint)
		return err;

	long long int nanos;
	err = jvmti_env->GetThreadCpuTime(thread, &nanos);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL, "Error getting thread CPU time. Code: %d\n", err);
		*pint = 0;
		return err;
	}
	*pint = nanos - *pint;
	return err;
}

