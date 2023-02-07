/*
 * scs.h
 *
 *  Created on: Sep 13, 2016
 *      Author: mahmoud
 */

#ifndef SCS_H_
#define SCS_H_

//Includes...
#include <jvmti.h>
#include <jni.h>

#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>

void JNICALL
scs_VMObjectAlloc(jvmtiEnv *jvmti_env,
		JNIEnv* jni_env,
		jthread thread,
		jobject object,
		jclass object_klass,
		jlong size);

void JNICALL
scs_MonitorContendedEnter(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread,
            jobject object);

void JNICALL
scs_MonitorWait(jvmtiEnv *jvmti_env,
        JNIEnv* jni_env,
        jthread thread,
        jobject object,
        jlong timeout);

void JNICALL
scs_VMInit_timedTrace(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread);

void JNICALL
scs_timedTraceThread(jvmtiEnv* jvmti_env,
			JNIEnv* jni_env,
			void* arg);

jint write_stack_trace(jvmtiEnv *jvmti_env,
		JNIEnv* jni_env,
		jthread thread);

FILE *trace_file_open(pid_t pid);

int trace_file_close(FILE *fp);

#endif /* SCS_H_ */
