/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2010
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef JPROF_H
   #define JPROF_H

   #define JNI_VERSION_1_1 0x00010001
   #define JNI_VERSION_1_2 0x00010002
   #define KSLEEP 1

   #include "utils/common.h"
   #include "a2n.h"

   #include <stdio.h>
   #include <stdarg.h>
   #include <stddef.h>

extern int JProfInitialized;

//
// Java string/string conversion functions:
// ****************************************
//
char * dupJavaStr(char * jstr);
char * tmpJavaStr(char * jstr, char * buf);

   #if defined(_CHAR_EBCDIC)
void    Force2Java(  char * str);
void    Force2Native(char * str);
void    Java2Native( char * str);
void    Native2Java( char * str);
   #else
      #define Force2Java(s)
      #define Force2Native(s)
      #define Java2Native(s)
      #define Native2Java(s)
   #endif
//
// ****************************************

// Get current date and time
void get_current_time( int * hh, int * mm, int * ss,
                       int * YY, int * MM, int * DD );

// Read cycles (TSC/TB/STCK) on given processor
UINT64 read_cycles_on_processor(int cpu);

// Create log-* files
FILE * OpenJprofFile( char *filename, char *filemode, char *whyExit );
FILE * file_open( char * ext, int cnt );

// Parse a list of period delimited integers
int parseIntList( char *str, int *array, int limit );

// Return system information string (for file header)
char * get_platform_info(void);

char * getJprofPath();
void   logFailure(int rc);

   #if defined(_AIX) || defined(_LINUX)
      #define MAX_PATH  PATH_MAX
   #endif
   #if defined(_ZOS)
      #define MAX_PATH _POSIX_PATH_MAX
   #endif

   #if defined(_AIX)
      #include <pthread.h>
      #include <semaphore.h>
      #include <sys/systemcfg.h>
      #include <perfutil.h>
      #include "pmapi.h"

int getpid(void);
      #define CurrentProcessID getpid
      #define CurrentThreadID  thread_self
      #define THREAD_DUMP 0
      #define stricmp strcasecmp
      #define INFINITE          -1
      #define WAIT_FAILED       -1

   #elif defined(_ZOS)
      #define _XOPEN_SOURCE_EXTENDED 1 /* haig    10/14/2003  for strdup */
      #include <pthread.h>
      #include <arpa/inet.h>  /* ntohl & ntohs : RJU 11/10/2003 */
      #include <unistd.h>     /* sleep :         RJU 11/10/2003 */

int getpid(void);
int getCurrentZOSProcessID();
//STJ #define CurrentProcessID getpid
      #define CurrentProcessID getCurrentZOSProcessId
      #define CurrentThreadID  getTidFromChar8
      #define THREAD_DUMP 0
      #define Sleep sleep
      #define stricmp strcasecmp
      #define INFINITE          -1

   #elif defined(_LINUX)
      #include <pthread.h>
      #include <perfutil.h>

      #include <linux/unistd.h>
      #include <sys/types.h>
      #include <signal.h>
      #include <dlfcn.h>

unsigned int sleep(unsigned int time);
int getppid(void);
      #define THREAD_DUMP 3
      #define CurrentProcessID  getpid
      #define CurrentThreadID   pi_gettid
      #define stricmp           strcasecmp
      #define INFINITE          -1
      #define WAIT_FAILED       -1

   #elif defined(_WIN32)  // set for IA32 & IA64
      #include "perfutil.h"
      #pragma warning(disable : 4028)
      #pragma warning(default : 4028)

      #define CurrentProcessID GetCurrentProcessId
      #define CurrentThreadID  GetCurrentThreadId
      #define THREAD_DUMP 21

// problem in units. WIN is msecs, others are secs
      #undef KSLEEP
      #define KSLEEP 1000
      #define sleep Sleep
   #endif

// if PTT_MAX_METRICS not defined => backward comp switch
   #ifndef PTT_MAX_METRICS
      #define PTT_MAX_METRICS 1
   #endif

/* Multiple Metric Macros */   
   #define MaxEvent 2300
   #define TIExtOff JVMTI_MAX_EVENT_TYPE_VAL

// Memory fencing
   #if defined(_WINDOWS)
      #define memory_fence()
      #define instruction_fence()
   #elif defined(_LINUX) && defined(_PPC)
//      #define memory_fence()  __sync_synchronize()
      #define memory_fence() __asm__ __volatile__("sync")
      #define instruction_fence()
   #elif defined(_AIX)
      #define memory_fence()       __lwsync()
      #define instruction_fence()  __isync()
   #elif defined(_ZOS)
// @@@@@ WHAT IS THE CORRECT INSTRUCTION TO USE?
      #define memory_fence()
      #define instruction_fence()
   #else
      #define memory_fence()
      #define instruction_fence()
   #endif

   #if defined IBM_JVM
      #include <ibmjvmti.h>
      #define MONITOR_JAVA JVMTI_MONITOR_JAVA
      #define MONITOR_RAW JVMTI_MONITOR_RAW
   #else
      #include <jvmti.h>
      #define MONITOR_JAVA 0x01
      #define MONITOR_RAW  0x02
   #endif


typedef void (* GETMTE_FUNC)(void);
typedef int  (* GETSYM_FUNC)(uint, uint64, SYMDATA *);

typedef struct _RTEventBlock RTEventBlock;
typedef struct _Node         Node;
typedef struct _DeltaNode    DeltaNode;
typedef struct _Node *       pNode;

   #include <hash.h>

   #include "scs.h"
   #if defined(_WINDOWS) || defined(_LINUX)
      #if defined(_LINUX)
         #include <semaphore.h>
         #include <sys/time.h>
         #include <sys/resource.h>
         #define set_semaphore(s)    sem_post(&s)
         #define reset_semaphore(s)
         #define wait_for_sem(s)     sem_wait(&s)
         #define close_sem(s)        sem_destroy(&s)
      #else // _WINDOWS
         #define set_semaphore(s)    SetEvent(s)
         #define reset_semaphore(s)  ResetEvent(s)
         #define wait_for_sem(s)     WaitForSingleObject(s, INFINITE)
         #define close_sem(s)        CloseHandle(s)
      #endif
   #endif
   #if defined(_AIX)
      #include <sys/time.h>
      #define set_semaphore(s)    sem_post(&s)
      #define reset_semaphore(s)
      #define wait_for_sem(s)     sem_wait(&s)
      #define close_sem(s)        sem_destroy(&s)
   #endif

   #include "gv.h"

   #define DEFAULT_TLS_SIZE            4096

   #define MAX_BUFFER_SIZE  1024
   #define MAX_FILE_STRING  MAX_PATH
   #define MAX_TIME_STRING  25

// Major/Minors are from jperfhooks.h
   #define METHOD_ENTRY_MAJ            0x30
   #define METHOD_ENTRY_MIN            0x01

   #define CLASS_LOAD_MAJ              0x40
   #define CLASS_LOAD_MIN              0x01
   #define CLASS_UNLOAD_MIN            0x02

   #define METHOD_LOAD_MAJ             0x50
   #define METHOD_LOAD_MIN             0x01
   #define METHOD_UNLOAD_MIN           0x02
   #define COMPILED_METHOD_LOAD_MIN    0x03
   #define COMPILED_METHOD_UNLOAD_MIN  0x04

extern int               ThreadDump;

void JVM_Shutdown(void);

/* Event data access macros */
   #define mXmid(e)  ((e)->u.method.method_id)
   #define mEmid(e)  ((e)->u.method.method_id)

void RTArcfTraceStart(thread_t * tp, int fAddBase);

/****** JProfTI Helper prototypes ******/
void objectAllocInfo(thread_t *tp,
                     jint size,
                     jobject class_id,
                     jobject obj_id,
                     jint is_array);

void objectFreeInfo(jobject obj_id);

void GCInfo(thread_t *tp,
            jlong used_objects,
            jlong used_object_space,
            jlong total_object_space);

// RTArcflow Event Handlers
int  reh_Init(void);
void reh_Out(void);

// prototypes
void genPrint1(thread_t * tp);
void genPrint2(thread_t * tp);
char * get_event_name(int);
int  RTCreateSocket(void);              // Socket API

char *locklab[9];
char evEnDiVec[MaxEvent];
char mkEnDiVec[MaxEvent];

   // NULL XJNICALL
   #ifdef XJNICALL
      #undef XJNICALL
   #endif
   #define XJNICALL

   #define OBJTABLELOCK 0x0001
   #define THREADLOCK   0x0002
   #define CLASSLOCK    0x0004
   #define METHODLOCK   0x0008
   #define OBJECTLOCK   0x0010
   #define RTDRIVERLOCK 0x0020
   #define TAGLOCK      0x0040
   #define TREELOCK     0x0080
   #define STKNODELOCK  0x0100

// locks: define init enter leave
   #if defined(_AIX) || defined(_LINUX) || defined(_ZOS)
      #define  define_lock(n)   pthread_mutex_t n
      #define  init_lock(n)     pthread_mutex_init(&n, NULL)
      #define  enter_lock(_n_, _m_) if(!(_m_ & ge.lockdis)) { \
               MVMsg( gc_mask_Show_Locks, ("enter_lock %X\n",_m_)); \
               pthread_mutex_lock(&_n_); \
               ge.lockcnt[_m_]++; \
      }
      #define  enter_lock1(n)   pthread_mutex_lock(&n)
      #define  leave_lock(_n_, _m_) if(!(_m_ & ge.lockdis)) { \
               pthread_mutex_unlock(&_n_); \
               MVMsg( gc_mask_Show_Locks, ("leave_lock %X\n",_m_)); \
      }
      #define  leave_lock1(n)   pthread_mutex_unlock(&n)
      #define  delete_lock(n)   pthread_mutex_destroy(&n)

   #elif defined(XXXX_ZOS)
      #define  define_lock(n)
      #define  init_lock(n)
      #define  enter_lock(n, m)
      #define  enter_lock1(n)
      #define  leave_lock(n, m)
      #define  leave_lock1(n)

   #elif defined(_WIN32)
      #define  define_lock(n)   CRITICAL_SECTION n
      #define  init_lock(n)     InitializeCriticalSection(&n)
      #define  enter_lock(_n_, _m_) if(!(_m_ & ge.lockdis)) { \
               MVMsg( gc_mask_Show_Locks, ("enter_lock %X\n",_m_)); \
               EnterCriticalSection(&_n_); \
               ge.lockcnt[_m_]++; \
      }
      #define  enter_lock1(n)   EnterCriticalSection(&n)
      #define  leave_lock(_n_, _m_) if(!(_m_ & ge.lockdis)) { \
               LeaveCriticalSection(&_n_); \
               MVMsg( gc_mask_Show_Locks, ("leave_lock %X\n",_m_)); \
      }
      #define  leave_lock1(n)   LeaveCriticalSection(&n)
      #define  delete_lock(n)   DeleteCriticalSection(&n)
   #endif

// EventLock literal
define_lock(ObjTableLock);
define_lock(ThreadLock);
define_lock(ClassLock);
define_lock(MethodLock);
define_lock(ObjectLock);
define_lock(XrtdLock);
define_lock(TagLock);
define_lock(TreeLock);
define_lock(StkNodeLock);

/***** TREE NODE STRUCTURE *****/
// nodename = NodeName(id)
//
//   type      index/type    id      name
//   ========  ============  ======= ==========
//   method    <= maxMethod  mp      mp->method_name
//   thrd      T             tp      tp->tnm
//   addr      A             addr    addr_%p
//   LUAddrs   a             pLUA    n/a

//   LATER INTEGRATION
//   event     E             env_id  _ev_%d
//   other     O             op      op->nm

   #define    node_flags_Stack   0x01   // Mark as stack element re currm
   #define    node_flags_Static  0x02   // Mark as static method

struct _Node
{
   pNode      sib;                      // next sibling (must be first field for efficient chaining)
   pNode      par;                      // parent
   pNode      chd;                      // 1st child

   void     * id;                       // type specific

   char       ntype;                    // Node Type, see table above
   char       flags;                    // Node Flags
   unsigned char numDispatches;         // Number of Dispatches (saturated)
   unsigned char numInterrupts;         // Number of Interrupts (saturated)

   #if defined(_64BIT)
   UINT32     lineNum;                  // Line number used with ExtendedCallStacks option
   #endif

   UINT64     bmm[1];                   // all metrics (made in Node constructor)

   // type = LUAddrs -> bmm[0] = ptr to List of Unresolved Addresses
};

struct _DeltaNode                       // Minimal node used during DELTA gathering
{
   pNode      sib;                      // next sibling (must be first field for efficient chaining)
   pNode      par;                      // parent
   pNode      chd;                      // 1st child
   void     * id;                       // type specific
   char       ntype;                    // Node Type, see table above
};

// Describes one object class associated with events at this node

struct _RTEventBlock
{
   RTEventBlock * next;
   class_t      * cp;                   // Class entry associated with this event
   UINT64         totalBytes;           // This is totalEvents for SCS=<eventname>
   UINT64         totalObjects;         // This is event object tag when DLO is specified

   UINT64         liveObjects;          // Optional fields, used with OBJECTINFO
   UINT64         liveBytes;
};
   #define totalEvents totalBytes
   #define tagEventObj totalObjects

// Describes the frame information used by JPROF

typedef struct _FrameInfo FrameInfo;

struct _FrameInfo
{
   void         * ptr;                  // Pointer varies by type field, usually (method_t *)
   UINT32         linenum;              // Line number
   char           type;                 // Method type
};

   #define NNBuf       2048

void * thread_node(thread_t * tp);
char * NodeName(pNode n, char * buf);
void   freeFrameBuffer( thread_t * tp );

   #ifdef _WIN32
int __stdcall cmpxchg(void * mem, void * old, void * new);
   #endif

int procSockCmd(thread_t * tp, char * nline);

void tree_stack(thread_t * tp);
void tree_free0(thread_t * tp);
void initMethodInfo( thread_t * tp, method_t * mp );

void doThreadStart( jvmtiEnv * jvmti,
                    JNIEnv   * env,
                    jthread    thread,
                    jlong      tid );

void doObjectAlloc( thread_t * tp,
                    jvmtiEnv * jvmti,
                    jobject    object,
                    jclass     object_klass,
                    jlong      size);

void doObjectFree( thread_t * tp, jlong tag );

void updateInstanceSize( class_t * cp, jlong size );

void treset( int reset );

typedef struct jprof_worker_thread_extra_args {
   jvmtiEnv * jvmti;
   JNIEnv   * env;
   jthread    thread;
} jprof_worker_thread_extra_args;

void EnableEvents1x(int f);
void EnableEvents2x(int f);

void MethodTypeLegend(FILE * fd);

   #if defined(_AIX)

void __cdecl AixGetEarlyThreadData( UINT32   * metrics,
                                    UINT64   * cycles );

int  __cdecl AixUseEarlyThreadData( thread_t * tp,
                                    UINT32   * metrics,
                                    UINT64     cycles );

int  __cdecl AixGetLateThreadData(  thread_t * tp );

   #endif

thread_t * readTimeStamp(int tid);
thread_t * timeJprofEntry();
void       timeAccum(thread_t * tp , UINT64 *metrics);
void       timeApply(     thread_t * tp );
void       timeJprofExit( thread_t * tp );

thread_t * tie_prolog(JNIEnv * e, jint type, char * msg);

void   RestartTiJprof(    thread_t * tp );

char * initJprof(char *);
void   init_event_names();
jint   ParseJProfOptions(char *);
int    ParseOption(  char     * parm,   // Upper case option
                     char     * p1 );   // Unmodified option
int    ParseCommand( thread_t * tp,
                     char     * parm,   // Upper case command
                     char     * line ); // Unmodified command
void   UpdateFNM();

void   set_gf_start(void);
void   rehmethods(thread_t * tp);
void   jmLoadCommon(jmethodID mb, char * caddr, int size, int flag,
                    char * c, char * m, char * s, char * cnm, char * pnm);
void   endThreadName(thread_t * tp);
void   setThreadName(thread_t * tp, char * tstr);
char * getThreadName(thread_t * tp);



void   err_exit(char * str);
jint   get_linenumber(       jmethodID method, jlocation location );
char * get_sourcefilename( jclass clazz );

void   getFrameInfo( void                 * frame_buffer,
                     int                    num_frames,
                     int                    fb_index,
                     FrameInfo            * fi );

void   allocEnExCs(         thread_t * tp );
void   dump_callstack(      thread_t * tp );
int    get_callstack(       thread_t * tp, thread_t * self_tp );
Node * push_callstack(      thread_t * tp );
int    pushThreadCallStack( thread_t * tp, thread_t * self_tp, jmethodID mid );

void   monitor_dump_event(char *stt, char *end);

void check_mtype_stack( thread_t * tp );

void tiJLMdump();
void SetTIEventMode( jvmtiEnv * jvmti, int fEnable, int event, jthread thr );


   #define getIdFromTag(x) ((jobject)(UINTPTR)x)

jlong  getNewTag();

char * getClassNameFromTag( thread_t * tp, jlong tag, char * buffer );

UINT64        *getNodeCounts(pNode);
UINT64        *getTimeStamp(pNode);
RTEventBlock **getEventBlockAnchor(pNode);
UINT32        *getLineNum(pNode);

void           checkEventThreshold( thread_t * tp,
                                    jobject    object,
                                    jclass     objclass,
                                    UINT32     events );

typedef void (JNICALL *PFN_JAVA_THREAD)(jvmtiEnv *, JNIEnv *, void *);
jthread create_java_thread(jvmtiEnv * jvmti,
                           JNIEnv * env,
                           PFN_JAVA_THREAD thread,   // Thread to create and start
                           void * thread_arg,   // Thread's argument
                           jint priority,   // Thread priority
                           char * name);   // Can be NULL for unnamed thread

class_t  *addClass(jobject cid, char *name );
method_t *addMethod( class_t * cp, jmethodID mid, char * name, char * sig );

void msg_out();

void TreeOut();

int    SetMetric( int metric );
int    SetMetricMapping();
char * getScaledName( char * namebuf, int metric );
void   StopJProf( int stopcode );       // See sjp_??? codes below

   #define sjp_PROCESS_DETACH             0
   #define sjp_JVMPI_EVENT_JVM_SHUT_DOWN  1
   #define sjp_JVMTI_VMDEATH              2
   #define sjp_JPROF_DETACH               3
   #define sjp_RTDRIVER                   4
   #define sjp_SHUTDOWN_EVENT             5
   #define sjp_PLAYBACK_SHUTDOWN_EVENT    6
   #define sjp_PLAYBACK_SHUTDOWN          7
   #define sjp_ERROR_SHUTDOWN             8
   #define sjp_SYSTEM_RESUMED_FROM_SLEEP  9
   #define NUM_STOP_CODES  10

void   nxAddEntries(  char ** pSelList, char * str );
int    nxCheckString( char *   selList, char * str );
void   nxShowList(    char *   selList );
char * nxGetString();

   #define csStatsProlog()  { UINT64 _csTimeStamp = 0; if ( gc.mask & gc_mask_CallStack_Stats ) _csTimeStamp = CSStatsProlog();
   #define csStatsEpilog(x) if ( gc.mask & gc_mask_CallStack_Stats ) CSStatsEpilog(_csTimeStamp,(x)); }

UINT64 CSStatsProlog();
void   CSStatsEpilog( UINT64 csTimeStamp, int numFrames );

   #ifndef _ReadWriteBarrier
      #define _ReadWriteBarrier()
   #endif

char    * sig2name( char * sig,
                    char * userbuf );

void queryClassMethods( thread_t * tp,
                        class_t  * cp,
                        jclass     klass );

void incrEventBlock( thread_t * tp,
                     jlong      tagMonitor,
                     class_t  * cp,
                     jlong      size );

void decrEventBlock( object_t * op );

#endif
