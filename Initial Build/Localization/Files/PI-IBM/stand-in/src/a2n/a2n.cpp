//Includes...
#include <jvmti.h>
#include <jni.h>

#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "a2n.h"
#include "agent/agent.h"
#include "common/fileLogging.h"
#include "common/kernel.h"

//Static Variables...
FILE *perf_map_file = NULL;


//Code..
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {

	init_file_log("a2n",PI_VL_INFO);

   jvmtiError err = JVMTI_ERROR_NONE;


   //Get jvmtiEnv object...
   jvmtiEnv *jvmti = NULL;
   (vm)->GetEnv((void **)&jvmti, JVMTI_VERSION_1);

   if ( jvmti == NULL )
	  {
	  log_to_file(PI_VL_CRITICAL,"Null jvmti agent...\n");
	  return 1;
	  }


   //Enable capabilites...
   jvmtiCapabilities capabilities_ptr;

   memset(&capabilities_ptr, 0, sizeof(capabilities_ptr));
   capabilities_ptr.can_generate_compiled_method_load_events = 1;
   err = (jvmti)->AddCapabilities(&capabilities_ptr);

   if ( err != JVMTI_ERROR_NONE )
	  {
	  log_to_file(PI_VL_CRITICAL,"Error setting capabilities. Code: %d\n", err);
	  return 1;
	  }


   //Set callbacks...
   jvmtiEventCallbacks callbacks;

   memset(&callbacks, 0, sizeof(callbacks));
   callbacks.CompiledMethodLoad  = &a2n_compiledMethodLoad;
   callbacks.DynamicCodeGenerated = &a2n_dynamicCodeGenerated;
   err = (jvmti)->SetEventCallbacks(&callbacks, (jint)sizeof(callbacks));

   if ( err != JVMTI_ERROR_NONE )
	  {
	  log_to_file(PI_VL_CRITICAL,"Error setting callback function. Code: %d\n", err);
	  return 1;
	  }


   //Set events that we want to be notified of...
   err = (jvmti)->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, NULL);
   if ( err != JVMTI_ERROR_NONE )
	  {
	  log_to_file(PI_VL_CRITICAL,"Error setting notification mode. Code %d\n", err);
	  return 1;
	  }

   err = (jvmti)->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, NULL);
   if ( err != JVMTI_ERROR_NONE )
	  {
	  log_to_file(PI_VL_CRITICAL,"Error setting notification mode. Code %d\n", err);
	  return 1;
	  }

   perf_map_file = perf_map_open(getpid());

   log_to_file(PI_VL_INFO,"A2N Agent Loaded successfully\n");
   return 0;
}

JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm) {

   perf_map_close(perf_map_file);
   log_to_file(PI_VL_INFO,"A2N Agent Unloaded successfully\n");
   close_file_log();
}

void JNICALL
a2n_compiledMethodLoad(jvmtiEnv *jvmti_env,
							 jmethodID method,
							 jint code_size,
							 const void* code_addr,
							 jint map_length,
							 const jvmtiAddrLocationMap* map,
							 const void* compile_info) {

   jvmtiError err = JVMTI_ERROR_NONE;

   if (perf_map_file) {
	   int64_t time = get_system_time_millis();
	   fprintf(perf_map_file, "%lx %x ", (unsigned long) code_addr, code_size);
	   write_method_signature(perf_map_file, jvmti_env, method);
	   fprintf(perf_map_file, " %ld\n", time);
   }
}

void JNICALL
a2n_dynamicCodeGenerated(jvmtiEnv *jvmti_env,
							   const char* name,
							   const void* address,
							   jint length) {

	if (perf_map_file) {
		int64_t time = get_system_time_millis();
		fprintf(perf_map_file, "%lx %x %s %ld\n", (unsigned long) address, length, name, time);
	}
}

FILE *perf_map_open(pid_t pid) {
   char filename[500];
   snprintf(filename, sizeof(filename), "a2n-%d.map", pid);

   FILE * res = fopen(filename, "w");

   if (!res) error(0, errno, "Couldn't open %s.", filename);
   return res;
}

int perf_map_close(FILE *fp) {
	if (fp)
		return fclose(fp);
	else
		return 0;
}

