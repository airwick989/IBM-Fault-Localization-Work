/*
 * arcflow.cpp
 *
 *  Created on: Sep 29, 2016
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

#include "arcflow.h"
#include "agent/agent.h"
#include "common/fileLogging.h"
#include "common/kernel.h"

//Static Variables...
FILE *trace_file = NULL;

//Code..
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {

	init_file_log("arc",PI_VL_INFO);

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
	if (false) {
		callbacks.VMObjectAlloc = &scs_VMObjectAlloc;
	}
	if (false) {
		callbacks.MonitorContendedEnter = &scs_MonitorContendedEnter;
	}
	if (true) {
		callbacks.VMInit = &scs_VMInit_timedTrace;
	}

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

FILE *trace_file_open(pid_t pid) {
   char filename[500];
   snprintf(filename, sizeof(filename), "arc-%d.trace", pid);

   FILE * res = fopen(filename, "w");

   if (!res) error(0, errno, "Couldn't open %s.", filename);
   return res;
}

int trace_file_close(FILE *fp) {


