#ifndef a2n_h
#define a2n_h

//Includes...
#include <jvmti.h>
#include <jni.h>

#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>


//Headers
void JNICALL
a2n_compiledMethodLoad(jvmtiEnv *jvmti_env,
                             jmethodID method,
                             jint code_size,
                             const void* code_addr,
                             jint map_length,
                             const jvmtiAddrLocationMap* map,
                             const void* compile_info);

void JNICALL
a2n_dynamicCodeGenerated(jvmtiEnv *jvmti_env,
                               const char* name,
                               const void* address,
                               jint length);

FILE *perf_map_open(pid_t pid);

int perf_map_close(FILE *fp);

void perf_map_write_entry(FILE *method_file, const void* code_addr, unsigned int code_size, const char* class_name, const char* method_name, const char* method_signature);

#endif
