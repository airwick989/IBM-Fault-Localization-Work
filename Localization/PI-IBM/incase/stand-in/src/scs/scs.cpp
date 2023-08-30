/*
 * scs.cpp
 *
 *  Created on: Sep 13, 2016
 *      Author: mahmoud
 */


//Includes...
#include <jvmti.h>
#include <jni.h>

#include <stdlib.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "scs.h"
#include "agent/agent.h"
#include "common/fileLogging.h"
#include "common/kernel.h"
#include "common/utils.h"

//Static Variables...
const char *trace_thread_name = "SCS Agent Sampling Thread";
char *agent_name = "scs";
FILE *trace_file = NULL;
long _period = 100000; // default = 100k us = 100 ms

void bad_option(char *current, char *next) {
	const size_t buff_size = 500;
	char buff[buff_size];
	size_t len = buff_size-1;
	if (next){
		len = next - current;
	}
	strncpy(buff, current, len);
	buff[len] = 0;

	log_to_file(PI_VL_WARNING, "Invalid command line option: %s\n", buff);
}

void parse_options(jvmtiEventCallbacks &callbacks, char *options) {
	const char del[] = {","};
	char *current = options;
	char *temp = options;
	//const size_t buff_size = 500;
	//char buff[buff_size];
	bool use_default = true;

	strToUpper(current);

	while (current) {
		printf("%s\n", current);
		char *next = strstr(current, del);
		if (strncmp(current, "VM_OBJECT_ALLOC", 15) == 0) {
			callbacks.VMObjectAlloc = &scs_VMObjectAlloc;
			use_default = false;
		}
		else if (strncmp(current, "MONITOR_CONTENDED_ENTER", 23) == 0) {
			callbacks.MonitorContendedEnter = &scs_MonitorContendedEnter;
			use_default = false;
		}
		else if (strncmp(current, "MONITOR_WAIT", 23) == 0) {
			callbacks.MonitorWait = &scs_MonitorWait;
			use_default = false;
		}
		else if (strncmp(current, "TIMED_TRACE", 11) == 0) {
			callbacks.VMInit = &scs_VMInit_timedTrace;
			use_default = false;
		}
		else if (strncmp(current, "PERIOD=", 7) == 0) {
			temp = current + 7;
			long p = (long) parse_long(temp);
			if (p)
				_period = p;
			else
				bad_option(current, next);
		}
		else {
			bad_option(current, next);
		}

		if (next)
			current = next + 1; // take into account the ','
		else
			break;
	}

	if (use_default) {
		log_to_file(PI_VL_INFO, "Using default configuration\n");
		callbacks.VMInit = &scs_VMInit_timedTrace;
	}
}

//Code..
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {

	init_file_log(agent_name,PI_VL_INFO);

	jvmtiError err = JVMTI_ERROR_NONE;


	//Get jvmtiEnv object...
	jvmtiEnv *jvmti = NULL;
	(vm)->GetEnv((void **)&jvmti, JVMTI_VERSION_1);

	if ( jvmti == NULL )
	{
		log_to_file(PI_VL_CRITICAL,"Null jvmti agent...\n");
		return 1;
	}

	jvmtiEventCallbacks callbacks;
	memset(&callbacks, 0, sizeof(callbacks));
	// always use thread local storage
	callbacks.ThreadStart = &threadStart_initData;
	callbacks.ThreadEnd = &threadEnd_deallocData;
	parse_options(callbacks, options);

	err = set_jvmti_callbacks(jvmti, callbacks);
	if (err != JVMTI_ERROR_NONE) {
		return 1;
	}

	trace_file = trace_file_open(getpid());

	log_to_file(PI_VL_INFO,"SCS Agent Loaded successfully\n");
	return 0;
}


JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm) {

   trace_file_close(trace_file);
   log_to_file(PI_VL_INFO,"SCS Agent Unloaded successfully\n");
   close_file_log();
}

// JVMTI Callbacks ...
void JNICALL
scs_VMObjectAlloc(jvmtiEnv *jvmti_env,
		JNIEnv* jni_env,
		jthread thread,
		jobject object,
		jclass object_klass,
		jlong size) {
	write_stack_trace(jvmti_env, jni_env, thread);

	jvmtiError err = JVMTI_ERROR_NONE;
	char* class_name = 0;
	char* generic_ptr = NULL;

	err = (jvmti_env)->GetClassSignature (object_klass, &class_name, &generic_ptr);
	if ( err != JVMTI_ERROR_NONE ) {
		log_to_file(PI_VL_CRITICAL,"Error getting class signature. Code %d\n", err);
		return;
	}

	if (trace_file) {
		fprintf(trace_file, "OBJECT| Size:%lld\tCLass:%s\n", size, class_name);
	}

	jvmti_env->Deallocate((unsigned char *)class_name);
	jvmti_env->Deallocate((unsigned char *)generic_ptr);
}

void JNICALL
scs_MonitorContendedEnter(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread,
            jobject object) {
	write_stack_trace(jvmti_env, jni_env, thread);

	// write monitor info: class name and owner thread
	if (trace_file) {
		fprintf(trace_file, "MONITOR| ");
		write_monitor_Info(trace_file, jvmti_env, jni_env, object);
		fprintf(trace_file, "\n");
	}
}

void JNICALL
scs_MonitorWait(jvmtiEnv *jvmti_env,
        JNIEnv* jni_env,
        jthread thread,
        jobject object,
        jlong timeout) {
	write_stack_trace(jvmti_env, jni_env, thread);

	// write monitor info: class name and owner thread
	if (trace_file) {
		fprintf(trace_file, "MONITOR| ");
		write_monitor_Info(trace_file, jvmti_env, jni_env, object);
		fprintf(trace_file, "\tTimeout:%lld ms\n", timeout);
	}
}

void JNICALL
scs_VMInit_timedTrace(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread) {
	jthread jth = get_new_jthread(jni_env, trace_thread_name);
	if (!jth) {
		log_to_file(PI_VL_CRITICAL, "Failed to initialize jthread object!\n");
		return;
	}

	jvmtiError err = jvmti_env->RunAgentThread(jth, &scs_timedTraceThread, 0, 5);
	if ( err != JVMTI_ERROR_NONE ) {
		log_to_file(PI_VL_CRITICAL,"Failed to launch agent thread! Code: %d\n", err);
	}
}

void JNICALL
scs_timedTraceThread(jvmtiEnv* jvmti_env,
			JNIEnv* jni_env,
			void* arg) {
	jthread *threads = 0;
	jvmtiError err;
	jthread th_this;
	jvmti_env->GetCurrentThread(&th_this);

	bool shouldRun = true;
	jvmtiPhase phase = JVMTI_PHASE_ONLOAD;
	jvmti_env->GetPhase(&phase);

	long period = _period;
	while (phase != JVMTI_PHASE_LIVE) {
		if (phase == JVMTI_PHASE_DEAD) {
			log_to_file(PI_VL_WARNING, "VM died before profiling started!\n");
			return;
		}
		sleep_micros(period);
		jvmti_env->GetPhase(&phase);
	}

	log_to_file(PI_VL_INFO, "%s starting sampling with period %ld\n", trace_thread_name, period);

	while (phase == JVMTI_PHASE_LIVE && shouldRun) {
		bool iterate = true;
		jint numThreads = 16;

		jint numCpus = 1;
		int perioddiv = 0;
		jvmti_env->GetAvailableProcessors(&numCpus);
		perioddiv = period / numCpus;

		for(int t=0; t<numCpus && iterate; t++) {
			if (numThreads < 16)
				numThreads = 16;
			jni_env->PushLocalFrame(2*numThreads);

			err = jvmti_env->GetAllThreads(&numThreads, &threads);
			if ( err != JVMTI_ERROR_NONE ) {
				log_to_file(PI_VL_CRITICAL,"Error getting jvm threads. Code: %d\n", err);
				iterate = false;
				jni_env->PopLocalFrame(0);
				break;
			}

			long long int *elapsedTimes = 0;
			long long int totalTime = 0;
			err = jvmti_env->Allocate(sizeof(long long int)*numThreads, (unsigned char **)&elapsedTimes);
			if ( err != JVMTI_ERROR_NONE ) {
				log_to_file(PI_VL_CRITICAL,"Error allocating memory. Code: %d\n", err);
				jvmti_env->Deallocate((unsigned char *)threads);
				iterate = false;
				jni_env->PopLocalFrame(0);
				break;
			}

			for(int i=0; i<numThreads; i++) {
				long long int elapsedTime = 0;
				if (jni_env->IsSameObject(th_this, threads[i]) == JNI_TRUE)
					elapsedTime = 0;
				else
					getThreadElapsedTime(jvmti_env, threads[i], &elapsedTime);
				elapsedTimes[i] = elapsedTime;
				totalTime += elapsedTime;
			}

			int randTime = rand() % totalTime;
			jthread pickedThread = 0;
			int j=0;
			for (int i=0; i<numThreads && j<numThreads; i++) {
				/*jvmtiThreadInfo th_info;
				jvmtiThreadGroupInfo g_info;
				jvmti_env->GetThreadInfo(threads[i], &th_info);
				jvmti_env->GetThreadGroupInfo(th_info.thread_group, &g_info);
				fprintf(trace_file, "%d %lld %s %d %s\n", i, elapsedTimes[i], th_info.name, th_info.is_daemon, g_info.name);*/
				if (randTime < elapsedTimes[i]) {
					pickedThread = threads[i];
					jint frame_count = write_stack_trace(jvmti_env, jni_env, pickedThread);
					if (frame_count) {
						break;
					}
					else {
						i = (i + 1) % numThreads;
						j++;
					}
				}
				randTime -= elapsedTimes[i];
			}

			jvmti_env->Deallocate((unsigned char *)threads);
			threads = 0;
			jvmti_env->Deallocate((unsigned char *)elapsedTimes);
			sleep_micros(perioddiv);
			jni_env->PopLocalFrame(0);
		}

		if (!iterate) {
			sleep_micros(period);
			iterate = true;
		}

		jvmti_env->GetPhase(&phase);
	}
}

// common methods
jint write_stack_trace(jvmtiEnv *jvmti_env,
		JNIEnv* jni_env,
		jthread thread) {
	const jint maxFrames = 20;
	jvmtiFrameInfo frames[maxFrames];
	jint count = 0;
	jvmtiError err;

	jni_env->PushLocalFrame(2*maxFrames);
	err = jvmti_env->GetStackTrace(thread, 0, maxFrames, frames, &count);
	if ( err != JVMTI_ERROR_NONE ) {
		log_to_file(PI_VL_CRITICAL,"Error (%d) getting stack trace from thread\n", err);
		jni_env->PopLocalFrame(0);
		return count;
	}
	if (!count) {
		jni_env->PopLocalFrame(0);
		return count;
	}

	// write thread name and time
	ThreadData *tdata = 0;
	long long int th_time = 0;
	char *th_name = 0;
	long long int elapsed = 0;
	err = jvmti_env->GetThreadCpuTime(thread, &th_time);
	if (err != JVMTI_ERROR_NONE) {
		log_to_file(PI_VL_CRITICAL,
				"Error getting thread CPU time. Code: %d\n", err);
		th_time = 0;
	}
	if (using_thread_localStorage) {
		// thread local storage may be available, use it
		err = jvmti_env->GetThreadLocalStorage(thread, (void **)&tdata);
		if (err != JVMTI_ERROR_NONE) {
			log_to_file(PI_VL_CRITICAL,
					"Error getting thread local storage. Code: %d\n", err);
		}
		if (tdata) {
			// update the thread cpu read time
			elapsed = th_time - tdata->readTime;
			tdata->readTime = th_time;
			if (tdata->tName) {
				// grab the name from there
				th_name = tdata->tName;
			}
			else {
				// set to null to show that name was not available
				tdata = 0;
			}
		}
	}
	if (!tdata) {
		// not using local storage or is not set, need to request thread name
		jvmtiThreadInfo threadInfo;
		err = jvmti_env->GetThreadInfo(thread, &threadInfo);
		if (err != JVMTI_ERROR_NONE) {
			log_to_file(PI_VL_CRITICAL,
					"Error getting thread info. Code: %d\n", err);
			jni_env->PopLocalFrame(0);
			return count;
		}
		th_name = threadInfo.name;
	}
	if (trace_file) {
		int64_t time = get_system_time_micros();
		fprintf(trace_file, "\n\nTime:%ld\tth_time:%lld\tElapsed:%lld\tThread:%s\n", time, th_time/1000, elapsed/1000, th_name);
	}
	if (!tdata) {
		// we have a new pointer to th_name, must deallocate
		err = jvmti_env->Deallocate((unsigned char *)th_name);
	}

	for (int i = 0; i < count; i++) {
		jmethodID method = frames[i].method;

		write_method_signature(trace_file, jvmti_env, method);
		if (trace_file) {
			fprintf(trace_file, "\n");
		}
	}

	jni_env->PopLocalFrame(0);
	return count;
}

FILE *trace_file_open(pid_t pid) {
   char filename[500];
   snprintf(filename, sizeof(filename), "scs-%d.trace", pid);

   FILE * res = fopen(filename, "w");

   if (!res) error(0, errno, "Couldn't open %s.", filename);
   return res;
}

int trace_file_close(FILE *fp) {
	if (fp)
		return fclose(fp);
	else
		return 0;
}

