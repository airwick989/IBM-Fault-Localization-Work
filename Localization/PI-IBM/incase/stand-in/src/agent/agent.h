#ifndef agent_h
#define agent_h

//Includes...
#include <jvmti.h>
#include <jni.h>

#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>

struct ThreadData {
	char *tName;
	long long int readTime;
	void *pid;
};

extern bool using_thread_localStorage;

jvmtiError check_Deallocate(jvmtiEnv* env, unsigned char* mem);

/**
 * write_method_signature will get and write the method class, name, and signature
 * to the provided output stream
 */
jvmtiError write_method_signature(FILE *method_file, jvmtiEnv *jvmti_env, jmethodID method);

jvmtiError write_monitor_Info(FILE *file, jvmtiEnv *jvmti_env, JNIEnv* jni_env, jobject object);

jvmtiError set_jvmti_callbacks(jvmtiEnv *jvmti, jvmtiEventCallbacks &callbacks);

jthread get_new_jthread(JNIEnv* jni_env, const char *thName);

/**
 * These functions initialize and set the thread local storage and deallocate them
 * They are meant to be used as jvmti event callbacks
 * Setting of ThreadData members such as the thread name is not guaranteed. They
 * should be checked before use
 */
void JNICALL
threadStart_initData(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread);

void JNICALL
threadEnd_deallocData(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread);

/**
 * These functions are used to quickly
 * This useful when you dont want to handle deallocation of the memory or the jvmti
 * calls and errors
 * sets the str_ptr to null if the local storage is not set or there is an error
 * errors from jvmti calls are returned
 */

/**
 * Use to easily get the thread name. will check the thread local storage first
 * If not set then will request from jvmti.
 * If pdealloc is returned true, then the string must be freed
 */
jvmtiError getThreadName(jvmtiEnv *jvmti_env, jthread thread, char **str_ptr, bool *pdealloc);
jvmtiError getThreadReadTime(jvmtiEnv *jvmti_env, jthread thread, long long int *pint);

/**
 * setThreadReadTime has a special case where it will return JVMTI_ERROR_NOT_AVAILABLE
 * if the local storage was not initialized
 */
jvmtiError setThreadReadTime(jvmtiEnv *jvmti_env, jthread thread, long long int nanos);
jvmtiError getThreadElapsedTime(jvmtiEnv *jvmti_env, jthread thread, long long int *pint);


#if !defined(_WINDOWS)
  #define SOCKET            int
  #define SOCKET_ERROR      -1
  #define INVALID_SOCKET    -1
#endif

struct AgentGV
{
// Agent Settings. Not all are used by all agents

	uint32_t	pid;

	jvmtiEnv	* jvmti_env;

	char		* hostname;

	SOCKET		socketServer;
	SOCKET		socketClient;

	int			port_number;

	jvmtiError	Init(const char *agentName);
	int			InitRtServer();
};

extern struct AgentGV agv;

#endif
