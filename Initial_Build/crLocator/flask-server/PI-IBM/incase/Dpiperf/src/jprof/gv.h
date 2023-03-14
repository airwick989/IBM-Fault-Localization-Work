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

#ifndef GV_H
   #define GV_H

// Debug output mask definitions

   #define gc_mask_Show_Events         0x00000100
   #define gc_mask_Show_Methods        0x00000200
   #define gc_mask_Show_Metrics        0x00000400
   #define gc_mask_Show_Classes        0x00000800
   #define gc_mask_JVMTI_info          0x00001000
   #define gc_mask_Jinsight            0x00002000
   #define gc_mask_CallStack_Stats     0x00004000
   #define gc_mask_Show_Stack_Traces   0x00008000
   #define gc_mask_Raw_Data            0x00010000
   #define gc_mask_EnEx_Details        0x00020000
   #define gc_mask_ObjectInfo          0x00040000
   #define gc_mask_Show_Locks          0x00080000
   #define gc_mask_SCS_Debug           0x00100000
   #define gc_mask_SCS_Verbose         0x00200000

// #define gc_mask_Malloc_Free         0x10000000
// #define gc_mask_Show_Write_Format   0x20000000

//----------------------------------------------------------------------
// Values used by global variables
//----------------------------------------------------------------------

   #define gv_MaxMetrics      8
   #define READ_SIZE          4096

// Method Entry/Exit indicators, plus additional events effected by Reset.
// Events marked Other are not effected by Reset operations and can continue
// to be processed normally.

   #define gv_ee_Entry        0
   #define gv_ee_Exit         1
   #define gv_ee_ExitPop      2
   #define gv_ee_Reset        3
   #define gv_ee_Other        4

// Method type identifiers

   #define gv_TypesNSD        "IIJJRRNNBBFCOMUKTLSExaukc"
   #define gv_TypesSD         "iIjJrRnNbBFCOMUKTLSExaukc"

   #define gv_type_interp           0
   #define gv_type_Interp           1
   #define gv_type_jitted           2
   #define gv_type_Jitted           3
   #define gv_type_reJitted         4
   #define gv_type_ReJitted         5
   #define gv_type_native           6
   #define gv_type_Native           7
   #define gv_type_builtin          8
   #define gv_type_Builtin          9

   #define gv_type_maxDynamic       9

   #define gv_type_Function        10
   #define gv_type_Compiling       11
   #define gv_type_Other           12
   #define gv_type_scs_Method      13
   #define gv_type_scs_User        14
   #define gv_type_scs_Kernel      15

   #define gv_type_maxMethod       15

   #define gv_type_Thread          16
   #define gv_type_inLine          17
   #define gv_type_Stem            18
   #define gv_type_Error           19
   #define gv_type_Information     20
   #define gv_type_LUAddrs         21
   #define gv_type_RawUserAddr     22
   #define gv_type_RawKernelAddr   23
   #define gv_type_CSymbol         24

// JLM states

   #define gv_jlm_Off         0
   #define gv_jlm_Lite        1
   #define gv_jlm_Time        2

//======================================================================
// global environment (persistent)
//======================================================================

struct GE                               // Global Environment variables (Persistent)
{
   UINT64  numberGCs;                   // Number of GCs observed
   UINT64  cpuSpeed;                    // Processor speed in cycles

   UINT64  system_sleep_timestamp;      // Cycles at which the system went to sleep

   #ifndef _LEXTERN
   UINT64  numSnGetCallStack;           // Number of CallStacks requested from JVMTI
   UINT64  numSnGetStkNode;             // Number of Tree Nodes retrieved from StkNodes
   UINT64  numSnPushStkNode;            // Number of CallStacks pushed to validate StkNodes
   UINT64  numSnPushCallStack;          // Number of CallStacks pushed to locate Tree Nodes
   UINT64  numSnSelectNode;             // Number of Tree Nodes located using StkNodes
   #endif //_LEXTERN

   char  * minJitCodeAddr;              // Minimum JitCode address
   char  * maxJitCodeAddr;              // Maximum JitCode address

   volatile int reset;                  // Reset in progress: See GE_RESET_???

   #define GE_RESET_SYNC     1
   #define GE_RESET_STACK    2
   #define GE_RESET_ZEROBASE 3
   #define GE_RESET_STATE    4
   #define GE_RESET_FULL     5

   #define DefaultMaxFrames       2048

   int     maxframes;                   // Maximum number of stack frames to get for each CPU

   jlong   tagNextObject;               // Next tag to be allocated

   hash_t  htClasses;
   hash_t  htMethods;
   hash_t  htObjects;
   hash_t  htThreads;

   int    chashb;                       // Number of buckets in class   hash table
   int    ohashb;                       // Number of buckets in object  hash table
   int    mhashb;                       // Number of buckets in method  hash table
   int    shashb;                       // Number of buckets in stknode hash table
   int    thashb;                       // Number of buckets in thread  hash table

   class_t * cpJavaLangClass;           // Address of the class block for java/lang/Class

   UINT32  pid;                         // Process ID

   int     seqno;                       // Sequence number for log-rt files
   int     seqGen;                      // Sequence number for log-gen files
   int     seqJLM;                      // Sequence number for JLM files

   int     seqnoClass;                  // Current Class  sequence number for Jinsight
   int     seqnoObject;                 // Current Object sequence number for Jinsight
   int     seqnoThread;                 // Current Thread sequence number for Jinsight
   int     seqnoTBE;                    // Sequence number for threads using TBE

   thread_t    * listDeadThreads;       // List of Dead Threads to be deleted during next full reset

   jvmtiEnv        * jvmti;             // JVMTI Environment

   char  * hostname;                    // Name of the current machine
   char  * jvmver;                      // JVM Version Information

   #define   JVM_J9   1
   #define   JVM_SOV  2
   #define   JVM_SUN  3
   #define   JVM_NONE 4

   int    JVMversion;                   // Uses JVM_xxx
   jint   jvmtiVersion;                 // See jvmti.h

   #if !defined(_WINDOWS)
      #define SOCKET            int
      #define SOCKET_ERROR      -1
      #define INVALID_SOCKET    -1
   #endif

   SOCKET  socketJprof;                 // Socket used by Jprof
   SOCKET  socketClient;                // Socket used by Client

   jvmtiExtensionFunction   setVMLockMonitor;
   jvmtiExtensionFunction   dumpVMLockMonitor;
   jvmtiExtensionFunction   setVmAndCompilingControlOptions;
   jvmtiExtensionFunction   setMethodSelectiveEntryExitNotify;
   jvmtiExtensionFunction   clearMethodSelectiveEntryExitNotify;
   jvmtiExtensionFunction   setExtendedEventNotificationMode;
   jvmtiExtensionFunction   getOSThreadID;
   jvmtiExtensionFunction   getStackTraceExtended;
   jvmtiExtensionFunction   getAllStackTracesExtended;
   jvmtiExtensionFunction   getThreadListStackTracesExtended;
   jvmtiExtensionFunction   signalAsyncEvent;
   jvmtiExtensionFunction   cancelAsyncEvent;
   jvmtiExtensionFunction   setAsyncEvent;
   jvmtiExtensionFunction   allowMethodInlining;
   jvmtiExtensionFunction   allowDirectJNI;
   #ifndef _LEXTERN
   jvmtiExtensionFunction   dumpJlmStats;
   #endif

   // Required A2N functions
   PFN_A2nGetSymbol                 pfnA2nGetSymbol;
   PFN_A2nGenerateHashValue         pfnA2nGenerateHashValue;
   PFN_A2nAddModule                 pfnA2nAddModule;
   PFN_A2nAddJittedMethod           pfnA2nAddJittedMethod;
   PFN_A2nSetErrorMessageMode       pfnA2nSetErrorMessageMode;
   PFN_A2nSetSymbolQualityMode      pfnA2nSetSymbolQualityMode;
   PFN_A2nSetDemangleCppNamesMode   pfnA2nSetDemangleCppNamesMode;
   PFN_A2nSetSymbolGatherMode       pfnA2nSetSymbolGatherMode;
   PFN_A2nSetCodeGatherMode         pfnA2nSetCodeGatherMode;
   PFN_A2nSetSystemPid              pfnA2nSetSystemPid;
   PFN_A2nCreateProcess             pfnA2nCreateProcess;
   PFN_A2nSetProcessName            pfnA2nSetProcessName;
   // Optional A2N functions
   PFN_A2nSetReturnLineNumbersMode  pfnA2nSetReturnLineNumbersMode;
   PFN_A2nLoadMap                   pfnA2nLoadMap;
   PFN_A2nSetMultiThreadedMode      pfnA2nSetMultiThreadedMode;
   PFN_A2nGetSymbolEx               pfnA2nGetSymbolEx;
   PFN_A2nSetGetSymbolExPid         pfnA2nSetGetSymbolExPid;

   #if defined(_WINDOWS)
   HMODULE                        a2n_handle;
   #elif defined(_LINUX)
   void *                         a2n_handle;
   #endif

   jint                     methodEntryExtended;
   jint                     methodExitNoRc;
   jint                     instrumentableObjectAlloc;
   jint                     arrayClassLoad;
   jint                     compilingStart;
   jint                     compilingEnd;
   jint                     asyncEvent;

   #define MODE_PTT_COUNTER0  0
   #define MODE_PTT_COUNTER1  1
   #define MODE_PTT_INSTS     254
   #define MODE_PTT_CYCLES    255
   #define MODE_CACHE_MISSES  256
   #define MODE_BRANCH_INSTRUCTIONS 257
   #define MODE_BRANCH_MISSES 258
   #define MODE_RAW_CYCLES    259
   #define MODE_LAT_CYCLES    260
   #define MODE_CPI           261
   #define MODE_NOMETRICS     262
   
   #define MODE_TOTAL         263

   #define RAW_CYCLES         0
   #define PTT_CYCLES         1
   #define PTT_COUNTER0       2
   #define PTT_COUNTER1       3
   #define PTT_INSTS          4
   #define PTT_MULTIPLE       5
   #define PTT_CACHE_MISSES   6
   #define PTT_BRANCH_INSTS   7
   #define PTT_BRANCH_MISSES  8

   char  * mmnames[MODE_TOTAL];         // Used by init_event_names
   char  * mmdescr[MODE_TOTAL];         // Used by init_event_names

   char  * evstr[MaxEvent];             // largest event_type
   char    ee   [MaxEvent];             // entry:0 exit:1 other:2, Uses gv_ee_???
   char    type [MaxEvent];             // method type I:0 J:1 ... Uses gv_type_???

   int     port_number;                 // Port (socket) number to use for RTDriver listener. -1 = error

   #define ge_objectinfo_ALL  2

   int     ObjectInfo;                  // Referenced by Hookit as an integer, ALL -> tag all at start

   int     nodeSize;                    // Tree node size
   int     sizeRTEventBlock;            // RTEventBlock size
   int     offNodeCounts;               // Offset to Node Counts of Calls or EnEx
   int     offTimeStamp;                // Offset to TimeStamp of Node Creation
   int     offEventBlockAnchor;         // Offset to RTEventBlock List Anchor
   int     offLineNum;                  // Offset to Line Number for ExtendedCallStacks

   int     oldNodeSize;                 // Size of Node structure before last DETACH, for realloc of thread nodes
   int     oldSizeRTEventBlock;         // Size of RTEventBlock   before last DETACH, for realloc of thread nodes
   int     oldOffNodeCounts;            // Offset to Node Counts of Calls or EnEx
   int     oldOffTimeStamp;             // Offset to TimeStamp of Node Creation
   int     oldOffEventBlockAnchor;      // Offset to RTEventBlock List Anchor
   int     oldOffLineNum;               // Offset to Line Number for ExtendedCallStacks

   int     swtraceSeqNo;                // Sequence number for JITA2N/TPROF

   char  * logPath;                     // Path   used to construct fnm      //MF
   char  * logPrefix;                   // Prefix used to construct fnm      //MF
   char  * logSuffix;                   // Prefix used to construct fnm      //MF

   char    fnm[MAX_PATH];               // base filename
   char    pidext[32];                  // pid extension (regenerated based on pidx)

   #define LC_MAXIMUM       0x0100      // Maximum lock mask

   int     lockdis;                     // disable=1 event:1 thrd:2 class:4 method:8 obj:16
   int     lockcnt[LC_MAXIMUM+1];       // Lock counters selected

   int     sizeMetrics;                 // Size of table of metrics

   volatile int flushing;               // Flush in progress, delay moves to the dead list
   int     flushDelayed;                // At least one thread move was delayed by flush

   #ifndef _LEXTERN
   int    scs;                          // (SCS) Callstack sampling mode

   int    scs_a2n;                      // (SCS_A2N) Log EIP/RIP when sampling and resolve addresses to symbols

   int    scs_thread_count;             // Number of sampling threads (1 per processor)

   struct _scs_counters *scs_counters;

   #endif //_LEXTERN

   #if defined(_LINUX)
   pthread_mutex_t inc_lock;            // Use to implement atomic increment function, temporary
   #endif

   jvmtiCapabilities caps;              // JVMTI Capabilities enabled at VMInit

   volatile char fSamplerWorking[MAX_CPUS];   // Per Processor Working Flags for Samplers

   char    fActive;                     // Profiling initialized and active
   char    immediate_response;          // 1=respond on acceptance (immediate), 0=respond on completion (delayed)
   char    sun;                         // Treat this as a SUN JVM

   char    onload;                      // Profiler loaded via JVM (vs javastem)
   char    pidx;                        // Make pidx selection persist thru restarts
   char    fNeverActive;                // Never allow event processing to become active

   char    piClient;                    // Connected to RTDRIVER in piClient mode
   char    fEchoToSocket;               // Echo all messages to socket
   char    socketCreated;               // JProf socket to RTDRIVER is created
   char    is64bitCPU;                  // 32 vs 64 bit CPU

   char    ThreadInfo;                  // Enable thread start/end events
   char    newHeader;                   // Enable new log-rt header
   char    EnExCs;                      // Maintain callstacks via entry/exit tracking

   char    a2n_initialized;
   char    ExtendedCallStacks;          // Enable line/file info in call stacks

   char    scs_debug_trace;             // Enable debug trace for SCS
   char    cant_use_a2n;                // A2N not available
   char    gc_running;                  // GC is running (we saw GC_START and haven't seen GC_FINISH) ???

   #ifndef _LEXTERN
   char    fSystemResumed;              // System has resumed from sleep
   #endif //_LEXTERN

   #if defined(_AIX) || defined(_LINUX)
   char    scs_end_cmd;
   #endif

   #if defined(_ZOS)
   char    asid;                        // Substitute ASID for PID, everywhere
   #endif
   
   char    HealthCenter;                // Running Jprof With HealthCenter
};
extern struct GE ge;

//======================================================================
// global variables (allocated)
//    Pointers marked as MF = Must Free, NA = Not Allocated, ?? = Unknown
//======================================================================

typedef struct _GV
{
   UINT64 tm_stt;                       // JLM vars
   UINT64 tm_lstt;
   UINT64 tm_curr;
   UINT64 tm_delta;

   UINT64 timeStampStart;               // Adjusted Cycles at start of trace
   UINT64 timeStampFlush;               // Adjusted Cycles at flush of trace

   UINT64 tbytes;                       // Total bytes allocated (AB)

   UINT64 timeStamp;                    // Global timeStamp used with NOMETRICS, protected by THREADLOCK

   uint64_t null64;                     // Buffer for writing null values

   UINT64 scs_total_cycles;             // Total number of cycles while acquiring call stacks

   UINT64 offJinsight;                  // Current offset in log-jinsight

   UINT64 totalMetrics[ gv_MaxMetrics ];   // Total Metrics (Used by computeTotalMetrics)

   // Calibration information


   #define GV_MS_PER_SECOND  (1000)
   #define GV_MS_PER_MINUTE  (60 * GV_MS_PER_SECOND)
   #define GV_MS_PER_HOUR    (60 * GV_MS_PER_MINUTE)

   UINT64 run_start_ts;                 // timestamp at run start
   UINT64 run_start_ms;                 // milliseconds at run start
   int ts_ticks_per_msec;               // timestamp ticks per millisecond

   double msf[ gv_MaxMetrics ];         // Scaling factor for this metric

   #define MAX_itThreads 4096

   jthread itThreads[MAX_itThreads];    // Threads passed to GetThreadListStackTraces
   jint    itThread;                    // Index of next thread in itThreads (count of threads)
   int     max_max_frames;              // Maximum number of frames in any thread callstack

   UINT32 totalTicks;                   // Total ticks for ticksDelta computations

   UINT32 fgcThreshold;                 // Allocation threshold at which GC will be requested
   UINT32 fgcCountdown;                 // Remaining allocated bytes before FGC

   UINT32 minAlloc;                     // Ignore all allocations less than minAlloc bytes

   int    sizePhysMets;                 // Size of table of physical metrics

   int    jitopts;                      // NUMA support options, // 1:binaryCod 2:  ???

   uint   jitflags;                     // J9 ???
   uint   setjflgs;                     // J9 ???
   int    treeout;                      // Prevent recursion when writing out the tree

   // state variables
   int    started;                      // enable/disable EE2. bld/dont-bld trees ???

   int    jstem;                        // jstem (byte code instrumentation) ???
   int    Adjusted;                     // adjust for instrumentation time   ???

   char   loadtime[48];                 // "JPROF Loaded at HH:MM:SS on YYYY/MM/DD\n\n"
   char   version[READ_SIZE];           // DLL Version String/File Header

   int    sockx;                        // various rtdriver commands (JSTEM)
   int    raise;                        //

   int    adjd;                         // disp  adjustment
   int    adji;                         // intr  adjustment
   int    adj2;                         // autocal metric2
   int    adjbcnt;                      // autocal both (disp & int) cnt
   int    overcal;                      // autocal over calibration
   int    i2j;                          // convert I: to J: in special case

   #define gv_rton_Trace 1
   #define gv_rton_Trees 2

   int    rton;                         // Tree building enabled

   int    mind;                         // dispatch  autocal
   int    mini;                         // interrupt autocal
   int    minb;                         // both (dsp/int) autocal

   int    tnodes;                       // tree nodes created
   int    checkNodeDepth;               // Node allocation threshold at which to check node depth

   int    sizeClassBlock;               // class block size

   int    jlm;                          // jlm (0-off 1-lite 2-time), Uses gv_jlm_???

   // Multiple Metrics
   int    cpi;                          // select CPI mode. 2 metrics, inst:0 cycs:1
   #define MAX_PIDLIST 8
   int    plcnt;                        // pidlist size
   int    pl[MAX_PIDLIST];              // pid list

   int    physmets;                     // Physical metrics
   int    usermets;                     // Metrics in output

   int    mapmet[  gv_MaxMetrics ];     // Mapping from recorded metrics

   int    physmet[ gv_MaxMetrics ];     // Type of Physical metric (255=cycles, ...)
   int    usermet[ gv_MaxMetrics ];     // Type of User metric     (255=cycles, ...)
   int    sf[      gv_MaxMetrics ];     // Raw scaling factor for this metric

   #define gv_sf_SECONDS         -1
   #define gv_sf_MILLISECONDS    -4
   #define gv_sf_MICROSECONDS    -7
   #define gv_sf_NANOSECONDS     -10

   int    indexPTTinsts;                // Index of PTT_INSTS for CPI divisor
   int    scaleFactor;                  // Scaling Factor for helpTest response
                                        // sf > 0 = divide value by 10**sf
                                        // sf < 0 = convert to nanoseconds,
                                        //          then perform scaling

   int    ecnt;                         // cnt of JVMTI events

   int    tbeg;                         // only build trees & generic output
   int    tend;                         // when : tbeg <= tp->tsn <= tend

   int    stack_frame_size;             // Size of one stack frame

   int    scs_total_stacks;             // Total number of call stacks acquired ???
   int    scs_total_frames;             // Total number of frames acquired      ???

// Fields related to samplecallstacks

   #if defined(_AIX)
   #define DefaultSampleRate 50
   #else
   #define DefaultSampleRate 32
   #endif

   #ifndef _LEXTERN

   int    scs_rate;                     // (SCS_RATE) Rate (samples/sec) or Event count
   int    scs_sampling_mode;            // Time or EventId

   #endif


   #if defined(_AIX)
   int    max_priority;                 // Maximum priority for threads in JVM process scheduling policy
   char   pm_event_name[32];
   #endif

   int    scs_active;                   // Notifications are active

   UINT32 scs_allocations;              // SCS to use Allocations to sample
   int    scs_allocBytes;               // SCS to use Allocated bytes to sample
   int    scs_classLoad;                // SCS to use Class Load events to sample
   int    scs_monitorEvent;             // SCS to use Monitor events to sample

   UINT32 scs_event_threshold;          // SCS Event threshold
   int    scs_event_object_classes;     // SCS Events display associated object classes

   int     megabytes;                   // MB of Heap in Use
   int     prevMegabytes;               // Previous MB of Heap in Use
   int     heapUsage;                   // % of Heap in Use
   int     prevHeapUsage;               // Previous % of Heap in Use

   int     shutdown_event;              // Event which will trigger shutdown
   int     shutdown_count;              // Number of occurrences before shutdown
   int     msglimit;                    // Maximum number of error messages of each kind

   UINT32  tobjs;                       // Total objects
   UINT32  lobjs;                       // Live objects
   UINT32  lbytes;                      // Live bytes

   int     maxObjectSize;               // Maximum Object Size in Heap

   char  * types;                       // String of method types, based on statdyn //NA

   char  * options;                     // Copy of jprof parameters                 //MF

//------------------------------------------------------------------------------
// Selection Lists are a list of strings with leading UINT32 flag/length field.
// After masking, the length is the length of the string which follows.
// The string is padded to align the next flag/length field on a 32-bit boundary.
// INCLUDE implies that '+' was specified for this string, while PREFIX implies
// a trailing '*' as a wildcard.  SUFFIX implies that a suffix entry follows
// the string.  The UINT32 header is the total length of the Selection List.
//
//   xxxxxxxx Total Size of Selection List, including header
//
//   xxxxxxxx Flags + Size of Current String (flag/length field)
//   cc....cc String (padded to 32-bit boundary)

   #define SELLIST_FLAG_INCLUDE  0x4000
   #define SELLIST_FLAG_PREFIX   0x2000
   #define SELLIST_FLAG_SUFFIX   0x1000
   #define SELLIST_MASK_LENGTH   0x0FFF

   char  * selListThreads;              // Selection List for THREADS=       //MF
   char  * selListMethods;              // Selection List for CLASSMETHODS=  //MF
   char  * selListClasses;              // Selection List for OBJECTCLASSES= //MF

   char  * nxList;                      // Current Incl/Excl List            //NA
   char  * nxModifier;                  // Current Incl/Excl Modifier        //NA

//------------------------------------------------------------------------------

   char    tempfn[MAX_PATH];            // Temporary filename
   char    buf[READ_SIZE];

   char  * pNextParm;                   // Used by ParseJProfOptions          //NA
   char  * savedParms;                  // Used by ParseJProfOptions          //NA

   #define MAX_CALL_STACK_COUNT 1024

   UINT32 * CallStackCountsByFrames;    // Used with ge_mask_CallStack_Stats //MF
   UINT64 * CallStackCyclesByFrames;    // Used with ge_mask_CallStack_Stats //MF

   struct  scs_options scs_opts;        // Used by ParseJProfOptions

   char    recomp;                      // Enable/Disable Recompilation detection
   char    statdyn;                     // Enable/Disable Static/Dynamic detection

   char    tagging;                     // Enable/Disable Use of Tagging support, if available

   char    g2se;                        // generic output to stderr
   char    addbase;                     // add delta-metrics to nodes
   char    revtree;                     // add Node at end (Ben's way)

   char    savedSep;                    // Used by ParseJProfOptions
   char    separator;                   // Separator for the last parm
   char    nxInclExcl;                  // Leading  separator from current Incl/Excl list item
   char    nxSeparator;                 // Trailing separator from current Incl/Excl list item

   char    exitpop;                     // Enables ExitPop category
   char    DistinguishLockObjects;      // Dump object information in JLM and SCS reports
   char    objrecs;                     // Produce old object records in log-rt files

   char    calltree;                    // Calltree output
   char    calltrace;                   // generic calltrace output
   char    start;                       // Start profiling immediately
   char    jinsts;                      // 1 : added jitted instruction to jita2n file

   char    sobjs;                       // dont rollup objs for log-rt
   char    exitNoRc;                    // +/- Use the ExitNoRc event for JVMTI
   char    methInline;                  // +/- Set AllowMethodInliningWithMethodEnterExit, if possible
   char    directJNI;                   // +/- Set AllowDirectJNIWithMethodEnterExit, if possible

   char    logRtdCmds;                  // +/- Log all RTDriver commands received
   char    idpnode;                     // +/- Display ID instead of PNODE in log-rt
   char    methodTypes;                 // +/- Method/node types and prefixes
   #ifndef _LEXTERN
   char    stknodes;                    // +/- SCS with StkNodes to map ESP/EIP to Tree Nodes
   char    threadListSampling;          // +/- SCS with Thread List Sampling
   #endif

   char    prototype;                   // Enable prototype features
   char    lockTree;                    // Lock required during flush while profiling active

   char    rtdriver;                    // Start the RTDriver listener thread
   char    trees;                       // Build trees while RECORD
   char    pnode;                       // Display pNode column in log-rt
   char    legend;                      // Include legend in log-rt/gen

   char    NoMetrics;                   // Use a per thread counter instead of metrics
   char    prunecallstacks;             // Prune unreported methods from call stacks, overridden by SCS
   char    deallocFrameBuffer;          // Dealloc the frame buffer after getting call stacks
   char    gatherMetrics;               // Gather metrics (including time)

   char    useTagging;                  // Actually use Tagging based on other options

   char    scs_sample_array;            // Enable Single Sampler
   char    scs_sample_array1;           // Enable Single Sampler -- back to back call stack requests

   char    havePrimArrays;              // Primitive Array classes have been defined                  ???

   char    HumanReadable;               // Enable Human Readable output
   char    vpa63;                       // Ensure compatability with VPA

   char    CompilingStartEnd;
   char    CompilerGCInfo;
   char    showVersion;

   char    Methods;
   char    ClassLoadInfo;               // Get all class and method information
   char    CompiledEntryExit;

   char    GCInfo;
   char    flushAfterGC;                // Flush the current tree after every GC
   char    ArenaInfo;

   char    InlineEntryExit;
   char    BuiltinEntryExit;
   char    NativeEntryExit;
   char    CompileLoadInfo;

   char    JNIGlobalRefInfo;
   char    JNIWeakGlobalRefInfo;

   char    Force;                       // Force continuation in spite of errors
   char    jitinfo;                     // enable jit-inline-info event
   char    noJvmCallStacks;             // No JVM Call Stacks for RTARCF
   char    TSnodes;                     // TimeStamp Node Creation (raw timestamp)
   char    TSnodes_HR;                  // TimeStamp Node Creation (hh:mm:ss:msec)

   // Flag Variables (were M_??? variables in jprof.c)
   char    JITA2N;                      // jita2n specified

   #if defined(_AIX)
   char    interruptCounting;           // Count metrics during interrupts (AIX only)
   char    systemPtt;                   // Use system code for PTT         (AIX only)
   #endif
} GV;
extern GV * gv;

extern char *DisableEnableRequest[3];

struct GD
{                                       // global file descriptors
   FILE   * tree;                       // used by xtreeOut (protected by gv.treeout)
   FILE   * gen;                        // generic
   FILE   * a2n;                        // ja2n
   FILE   * jtnm;                       // jvm tnm
   FILE   * config;                     // Configuration file containing JPROF options
   FILE   * syms;                       // Symbol file used by AIX Symlib
   FILE   * jinsight;                   // File used by Jinsight
};
extern struct GD gd;
#endif
