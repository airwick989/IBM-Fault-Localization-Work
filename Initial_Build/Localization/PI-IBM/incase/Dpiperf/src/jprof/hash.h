/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2009
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

#ifndef JPROF_HASH_H
   #define JPROF_HASH_H

   #include <jvmti.h>

typedef struct _thread_t thread_t;

// for passing 3 part java names
typedef struct _rtname RTName;
typedef RTName * pRTName;

struct _rtnames
{
   char * c;                            // class
   char * m;                            // method
   char * s;                            // signature
};

   #define class_flags_nameless 0x0001
   #define class_flags_filtered 0x0002
   #define class_flags_selected 0x0004

   #define class_flags_idshift  3

typedef struct _class_t
{
   void    * nextInBucket;
   jobject class_id;                  // 32-bit JVMTI tag/identifier

   char    * class_name;                //MF
   int       flags;                     // uses class_flags_????, shift used to get Jinsight ID

   jint      min_size;                  // Minumum size of an instance of this object

   char    * source_name;               //MF
} class_t;

void      class_table_free(void);
char    * class_name  (jobject cid, int type);
class_t * lookup_class(jobject cid);
class_t * insert_class(jobject cid);

   #define method_flags_nameless 0x0001
   #define method_flags_static   0x0002
   #define method_flags_native   0x0004
   #define method_flags_nonjava  0x0008
// The compiled flag is additive and MUST be the highest valid flag
// The mask is used to check for multiple compiles
// rejshift is used to convert the flags into a compile count
   #define method_flags_compiled 0x0010
   #define method_flags_rejshift 4
   #define method_flags_rejitted 0xFFE0
   #define method_flags_MASK     0x001F

typedef struct _method_t method_t;
struct _method_t
{
   void    * nextInBucket;
   jmethodID method_id;                 /* id from JVM,  -1 on class unload */

   char    * method_name;               // Combined name and signature of method
   class_t * class_entry;               /* jvm class id */
   int       flags;                     // uses method_flags_????

   // Only present if SCS specified

   int       code_length;               // Length of the compiled method
   char    * code_address;              // Address of the compiled method
};

void       method_table_free(void);
void       getMethodName(   method_t * mp, char **, char ** );
jobject  method_class_id( jmethodID mid );
method_t * lookup_method(   jmethodID mid, char * name );
method_t * insert_method(   jmethodID mid, char * name );

// Object Section *************************
typedef struct object_t
{
   void       * nextInBucket;
   void       * addr;                   // object handle

   class_t    * cp;                     // pointer to the associated class_t structure

   void       * pRTEB;                  // Event block associated with this object
   UINT64       bytes;                  // bytes
   UINT16       flags;                  // Uses object_flags_xxxx
   UINT32       objectID;               // Global tag
} object_t;

   #define object_flags_alloc_threshold 0x01
   #define object_flags_bytes_threshold 0x02
   #define object_flags_extra_threshold 0x04

void       object_table_free(void);
object_t * lookup_object(void * addr);
object_t * insert_object(void * addr);
void       remove_object(void * addr);

   #ifndef _LEXTERN

// StkNode Section *************************

typedef struct _stknode_t stknode_t;
struct _stknode_t
{
   void       * nextInBucket;
   void       * esp;                    // ESP of node (primary key)
   void       * eip;                    // EIP of node (secondary key)

   Node       * np;                     // Pointer to the associated tree node, 0 = invalid
   UINT64       numRefs;                // Number of times referenced, 0 = invalid due to non-unique mapping
};

      #define snWarmup  8                     // StkNode Minimum reliability
      #define snCheckup 127                   // StkNode Reliabilty validation mask

void        stknode_table_free( thread_t * tp );
stknode_t * insert_stknode(     thread_t * tp, void * esp, void * eip);

   #endif //_LEXTERN

struct _thread_t
{
   void      * nextInBucket;
   void      * tid;                     // thread ID

   int        ptt_enabled;
   int        scs_enabled;

   UINT64     timeStamp;                // Last TimeStamp on this thread
   UINT64     timeStampRec;             // Last TimeStamp recorded on this thread
   UINT64     csTimeStamp;              // TimeStamp used by gv_mask_CallStack_Stats

   UINT64     progressCycles;           // Last Cycles for which progress was recorded

   UINT64     oldcycs;                  // latency
   UINT64     newcycs;                  // latency

   UINT64     prev_abytes;              // allocated bytes sample
   UINT64     abytes;                   // allocated bytes

   UINT64     tagObject;                // Tag of the current object for this event
   UINT64     tagClass;                 // Tag of the class of the current object

   //##### * can also union these with newmm, oldmm, etc. since we can't
   //#####   do per-thread time and scs together?

   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
   int        sampler_active;
   int        no_stack_cnt;             // Number of times no stack was returned
   #endif

   // Metric vars (allocated as a single block)
   UINT64   * metEntr;                  // Metrics at Jprof Entry          //MF as a group
   UINT64   * metExit;                  // Metrics at Jprof Exit
   UINT64   * metDelt;                  // Metrics Delta
   UINT64   * metAppl;                  // Metrics during Application
   UINT64   * metInst;                  // Metrics during JVM instrumentation
   UINT64   * metJprf;                  // Metrics during Jprof
   UINT64   * metSave;                  // Metrics save area
   UINT64   * metGC;                    // Metrics during GC

   #ifndef _LEXTERN
   hash_t      htStkNodes;              // Hash table of ESP/EIP Callstack to Tree Node mappings
   stknode_t * stknode;                 // Current StkNode                 //NA (pointer into htStkNodes)
   #endif //_LEXTERN

   jlong        tagMonitor;

   #if defined(_AIX)

   pm_data_t        pmdata;             // Used by Aix{Get|Use}{Early|Late}ThreadData
   timebasestruct_t tbs;                // Used by Aix{Get|Use}{Early|Late}ThreadData

   #endif

   volatile jthread thread;             // Java thread object, used by JVMTI

   int        cpuNum;                   // Last CPU number on which this executed
   int        progressCPU;              // Last CPU number for which progress was recorded
   int        tsn;                      // thread sequence number
   JNIEnv   * env_id;                   // java env of thread              //NA
   char     * thread_name;              // jvm thread name                 //MF
   int        usage;                    // number of reuses
// void     * mid;                      // Java Method ID for delta trace  //NA
   method_t * mp;                       // Pointer to method block for mid //NA

   object_t * op;                       // Pointer to current object block //NA

   Node     * currT;                    // Tree node of thread             //NA (always present, root of tree)
   Node     * currm;                    // Current node on thread          //NA (pointer into tree)

   Node     * stack_walk_error_node;    // Stack walking error collector node

   #define STACK_DEPTH_INC 16

   void     * frame_buffer;             // Callstack frame buffer          //MF
   int        num_frames;               // Last number of frames acquired
   int        max_frames;               // Max  number of frames in buffer

   char     * mtype_stack;              // Stack of method types           //MF
   int        mtype_size;               // Size  of mtype_stack
   int        mtype_depth;              // Depth of mtype_stack

   int        depth;                    // Depth of the current node in the tree
   uint32_t   incr;                     // Current increment
   uint32_t   incr2;                    // Current increment (secondary)

   int        etype;                    // event type

// //##### * tls and tls_sz aren't used. maybe we can get rid of those?
// char     * tls;                      // thread local storage for trees etc.
// int      * tls_sz;                   // remaining sz of tls

   UINT32     abytes_sample_count;      // sample count in process

   UINT32     scs_events;               // Number of times an SCS event occurred (modulo scs_event_threshold)
   UINT32     scs_allocs;               // Number of times an SCS allocation event occurred (modulo scs_allocations)

   void     * selMethodId;              // Method id selected              //NA
   int        selMethodDepth;           // Recursion level of selected method

   int        flags;                    // Uses thread_xxx, defined below

   #if defined(_WINDOWS)
   HANDLE       handle;                 // Thread handle
   #elif defined(_AIX) || defined(_LINUX)
   pthread_t    pthread;
   #endif

   char       recFlags;                 // Record flags work area for trace.c

   #define thread_callstack  0x01
   #define thread_stackmode  0x02
   #define thread_selected   0x04
   #define thread_exitpopped 0x08
   #define thread_mtypedone  0x10
   #define thread_resetdelta 0x20
   #define thread_filtered   0x40

   // These fields are used by calibration to track the history of
   // methods and transitions.  That is, mt1 -x12-> mt2 -x23-> mt3

   char       mt1;                      // Method type 1  (IJNSO...), old mt2
   char       x12;                      // Transition 1-2 (entry or exit), old x23
   char       mt2;                      // Method type 2  (IJNSO...), old mt3
   char       x23;                      // Transition 2-3 (entry or exit)
   char       mt3;                      // Method type 3  (IJNSO...)

   char       ignoreCallStacks;         // Mark thread to never get call stacks
   char       fDelayed;                 // Move to dead list delayed by Flush
   char       fValidCallStack;          // Saved callstack is valid

   volatile char fValid;                // Should not be deleted by reset
   volatile char fWorking;              // Should not be modified by reset
   volatile char fResetNeeded;          // Reset has not yet been performed on this thread
   volatile char fSampling;             // Sampling in progress
#ifdef HAVE_PERF_SUPPORT
   int fd[2];  // for perf_event
#endif
};

thread_t *  insert_thread(UINT32 tid);
thread_t *  lookup_thread(UINT32 tid);
void        flushDelayedCleanup();
void        threadDataReset(thread_t *);
   #if defined(_WINDOWS)
void        close_threads(void);
   #endif

void LogAllHashStats();

class_t * getClassEntry(thread_t * tp, jclass klass);
   #ifndef _LEXTERN
Node    * push_stknode( thread_t * tp );
   #endif //_LEXTERN

#endif
