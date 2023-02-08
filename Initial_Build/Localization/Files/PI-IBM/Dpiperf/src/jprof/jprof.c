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

// STJ 20051109 Completed HEAPFILTER= support
// STJ 20051130 Completed JVMTI support equivalent to JVMPI support
// STJ 20060301 New calibration algorithm and reduced memory footprint
// EMP 20060324 New callstack sampling implementation

#include "version.h"        /* !!!!! change version number here !!!!! */

#include "jprof.h"
#include "tree.h"
#include "sdate.h"

#include "perfutil.h"
#include "utils/pi_time.h"

#include "plugin/JprofPluginInterface.h"

#if defined(_WINDOWS)
#define REQUIRED_PERFUTIL_VERSION  0x09050004   // v9.5.4 or greater
#endif

extern char strRtdriverListener[24];

void JNICALL cbVMDeath( jvmtiEnv * jvmti, JNIEnv * env);

int   JProfInitialized = 0;

char *DisableEnableRequest[3] =
{
   "Disable",
   " Enable",
   "Request"
};

#if defined(_WINDOWS)
static DWORD tls_index;
#endif

#if defined(_LINUX)
   #ifndef _LEXTERN
extern int gv_scs_harvest_on_load;    // scs.c
extern int gv_kernel_frame_pointers;  // scs.c
   #endif //_LEXTERN
#endif

//TSS: #if defined(_LINUX) || defined(_AIX) || defined(_ZOS)
//TSS: static pthread_key_t tss_key;
//TSS: #endif

char * locklab[9] =
{
   "Object_Tbl",
   "Thread_Tbl",
   "Class_Tbl",
   "Method_Tbl",
   "ObjectLock",
   "RtDrvrLock",
   "ObjectTags",
   "Tree",
   "StkNode"
};
char evEnDiVec[MaxEvent] = {0};         // enabled events
char mkEnDiVec[MaxEvent] = {0};         // enabled events
int  edarr[MaxEvent]     = {0};         // enable(1) disable(-1) specific events

// file global

int    ThreadDump = 0;

void sendInt( int n );
void CloseSocket(void);

struct GD gd = {0};                     // gv.h - file descriptors
struct GE ge = {0};                     // gv.h - persistent environment
GV      * gv =  0;                      // gv.h - allocated environment

char version_string[32];

//----------------------------------------------------------------------
// MUST be the first routine in the first file.
//----------------------------------------------------------------------

void getMyLoadAddress()
{
#ifndef _LEXTERN
   OptVMsgLog( "\nVersion for IBM Internal Use Only\n" );
#endif

#if defined(_WINDOWS)
   gc.myLoadAddress = (char *)( -4096 & (uint64_t)getReturnAddress(0) ) - 4096;

   OptVMsgLog( "\nJPROF Load Address: %p\n\n", gc.myLoadAddress );
#endif
}

void freeFrameBuffer( thread_t * tp )
{
   char * p;
   int    size;

   if ( tp->frame_buffer )
   {
      p    = (char *)tp->frame_buffer;
      size = tp->max_frames * sizeof( jvmtiFrameInfoExtended );

      tp->num_frames   = 0;
      tp->max_frames   = 0;
      tp->frame_buffer = 0;

      xFree( p, size );
   }
}

/******************************/
char * get_event_name(int etype)
{
   if ((etype > 0) && (etype < MaxEvent) )
   {
      return(ge.evstr[etype]);
   }
   return("-OutOfRange-");
}

/******************************/
void init_event_names(void)
{
   int i;

   if ( ge.mmnames[0] )
   {
      return;                           // Already done
   }

   for (i = 0; i < MODE_RAW_CYCLES; i++)
   {
      ge.mmnames[i] = "undefined";
      ge.mmdescr[i] = "undefined";
   }

   // add additional ptt mnemonics here

#if !defined(_WINDOWS)
   ge.mmnames[MODE_PTT_COUNTER0] = ("COUNTER0");
   ge.mmnames[MODE_PTT_COUNTER1] = ("COUNTER1");
#endif
   ge.mmnames[MODE_PTT_INSTS   ] = ("PTT_INSTS");
   ge.mmnames[MODE_PTT_CYCLES  ] = ("PTT_CYCLES");
   ge.mmnames[MODE_RAW_CYCLES  ] = ("RAW_CYCLES");
   ge.mmnames[MODE_LAT_CYCLES  ] = ("LAT_CYCLES");
   ge.mmnames[MODE_CPI         ] = ("CPI");
   ge.mmnames[MODE_NOMETRICS   ] = ("NOMETRICS");
#ifdef HAVE_PERF_SUPPORT
   ge.mmnames[MODE_CACHE_MISSES   ] = ("CACHE_MISSES");
   ge.mmnames[MODE_BRANCH_INSTRUCTIONS   ] = ("BRANCH_INSTS");
   ge.mmnames[MODE_BRANCH_MISSES   ] = ("BRANCH_MISSES");
#endif

#if !defined(_WINDOWS)
   ge.mmdescr[MODE_PTT_COUNTER0] = ("Performance Counter 0");
   ge.mmdescr[MODE_PTT_COUNTER1] = ("Performance Counter 1");
#endif
   ge.mmdescr[MODE_PTT_INSTS   ] = ("Per Thread Instructions");
   ge.mmdescr[MODE_PTT_CYCLES  ] = ("Per Thread Cycles");
   ge.mmdescr[MODE_RAW_CYCLES  ] = ("Raw Cycles");
   ge.mmdescr[MODE_LAT_CYCLES  ] = ("Latency Cycles (Raw - Per Thread)");
   ge.mmdescr[MODE_CPI         ] = ("Cycles Per Instruction");
   ge.mmdescr[MODE_NOMETRICS   ] = ("Counter of Events Received");
#ifdef HAVE_PERF_SUPPORT
   ge.mmdescr[MODE_CACHE_MISSES   ] = ("CACHE MISSES");
   ge.mmdescr[MODE_BRANCH_INSTRUCTIONS   ] = ("BRANCH Instructions");
   ge.mmdescr[MODE_BRANCH_MISSES   ] = ("BRANCH_MISSES");
#endif

   // query availability of namedCounterSupport
   // per Platform
   // perfutil.h to compiling
   // API stubs w rc
#if defined(_WINDOWS) || defined(_LINUX)
   {
      const PERFCTR_EVENT_INFO * list = NULL;
      int len, i;

   #if defined(_WINDOWS)
      int  num_ctrs;
      char **ctr_names = NULL;

      if (!IsHyperThreadingEnabled())
      {
         // Counters only allowed if HTT not enabled
         num_ctrs = GetPerfCounterNameList(&ctr_names);
         OptVMsg(" PerfCountersSupported       %d\n", num_ctrs);
         for (i = 0; i < num_ctrs; i++)
         {
            ge.mmnames[i] = xStrdup("PerfCounters",ctr_names[i]);
            ge.mmdescr[i] = ge.mmnames[i];
         }
      }
   #endif

      len = GetPerfCounterEventsInfo( &list );
      OptVMsg(" PerfCounterEventsSupported  %d\n", len);

      for (i = 0; i < len; i++)
      {
         ge.mmnames[list[i].event_id] = xStrdup("mmnames",list[i].name);
         ge.mmdescr[list[i].event_id] = ge.mmnames[list[i].event_id];
      }
   }
#endif

   OptVMsgLog(" The following are valid metrics on this platform:\n");

   for (i = 0; i < MODE_TOTAL; i++)
   {
      if ( 0 != strcmp( "undefined", ge.mmnames[i] ) )
      {
         OptVMsgLog(" %4d <%s>\n", i, ge.mmnames[i]);
      }
   }
   // End of MM Names

   for (i = 0; i < MaxEvent; i++)
   {
      ge.evstr[i] = "-";
      ge.ee[i]    = gv_ee_Other;
      ge.type[i]  = gv_type_Error;
   }

   ge.ee[   JVMTI_EVENT_METHOD_ENTRY ] = gv_ee_Entry;
   ge.type[ JVMTI_EVENT_METHOD_ENTRY ] = gv_type_Interp;

   ge.ee[   JVMTI_EVENT_METHOD_EXIT ]  = gv_ee_Exit;
   ge.type[ JVMTI_EVENT_METHOD_EXIT ]  = gv_type_Other;

#if defined(IBM_JVM)
   ge.ee[   TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_INTERPRETED ] = gv_ee_Entry;
   ge.type[ TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_INTERPRETED ] = gv_type_Interp;

   ge.ee[   TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_COMPILED ] = gv_ee_Entry;
   ge.type[ TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_COMPILED ] = gv_type_Jitted;

   ge.ee[   TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_NATIVE ] = gv_ee_Entry;
   ge.type[ TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_NATIVE ] = gv_type_Native;

   ge.ee[   TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_PARTIAL_IN_LINE ] = gv_ee_Entry;
   ge.type[ TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_PARTIAL_IN_LINE ] = gv_type_inLine;

   ge.ee[   TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_IN_LINE ] = gv_ee_Entry;
   ge.type[ TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_IN_LINE ] = gv_type_inLine;
#endif

   // JVMTI Event Strings

   ge.evstr[JVMTI_EVENT_VM_INIT                  ] = ("VM_INIT");
   ge.evstr[JVMTI_EVENT_VM_DEATH                 ] = ("VM_DEATH");
   ge.evstr[JVMTI_EVENT_THREAD_START             ] = ("THREAD_START");
   ge.evstr[JVMTI_EVENT_THREAD_END               ] = ("THREAD_END");
   ge.evstr[JVMTI_EVENT_CLASS_FILE_LOAD_HOOK     ] = ("CLASS_FILE_LOAD_HOOK");
   ge.evstr[JVMTI_EVENT_CLASS_LOAD               ] = ("CLASS_LOAD");
   ge.evstr[JVMTI_EVENT_CLASS_PREPARE            ] = ("CLASS_PREPARE");
   ge.evstr[JVMTI_EVENT_VM_START                 ] = ("VM_START");
   ge.evstr[JVMTI_EVENT_EXCEPTION                ] = ("EXCEPTION");
   ge.evstr[JVMTI_EVENT_EXCEPTION_CATCH          ] = ("EXCEPTION_CATCH");
   ge.evstr[JVMTI_EVENT_SINGLE_STEP              ] = ("SINGLE_STEP");
   ge.evstr[JVMTI_EVENT_FRAME_POP                ] = ("FRAME_POP");
   ge.evstr[JVMTI_EVENT_BREAKPOINT               ] = ("BREAKPOINT");
   ge.evstr[JVMTI_EVENT_FIELD_ACCESS             ] = ("FIELD_ACCESS");
   ge.evstr[JVMTI_EVENT_FIELD_MODIFICATION       ] = ("FIELD_MODIFICATION");
   ge.evstr[JVMTI_EVENT_METHOD_ENTRY             ] = ("METHOD_ENTRY");
   ge.evstr[JVMTI_EVENT_METHOD_EXIT              ] = ("METHOD_EXIT");
   ge.evstr[JVMTI_EVENT_NATIVE_METHOD_BIND       ] = ("NATIVE_METHOD_BIND");
   ge.evstr[JVMTI_EVENT_COMPILED_METHOD_LOAD     ] = ("COMPILED_METHOD_LOAD");
   ge.evstr[JVMTI_EVENT_COMPILED_METHOD_UNLOAD   ] = ("COMPILED_METHOD_UNLOAD");
   ge.evstr[JVMTI_EVENT_DYNAMIC_CODE_GENERATED   ] = ("DYNAMIC_CODE_GENERATED");
   ge.evstr[JVMTI_EVENT_DATA_DUMP_REQUEST        ] = ("DATA_DUMP_REQUEST");
   ge.evstr[JVMTI_EVENT_MONITOR_WAIT             ] = ("MONITOR_WAIT");
   ge.evstr[JVMTI_EVENT_MONITOR_WAITED           ] = ("MONITOR_WAITED");
   ge.evstr[JVMTI_EVENT_MONITOR_CONTENDED_ENTER  ] = ("MONITOR_CONTENDED_ENTER");
   ge.evstr[JVMTI_EVENT_MONITOR_CONTENDED_ENTERED] = ("MONITOR_CONTENDED_ENTERED");
   ge.evstr[JVMTI_EVENT_GARBAGE_COLLECTION_START ] = ("GARBAGE_COLLECTION_START");
   ge.evstr[JVMTI_EVENT_GARBAGE_COLLECTION_FINISH] = ("GARBAGE_COLLECTION_FINISH");
   ge.evstr[JVMTI_EVENT_OBJECT_FREE              ] = ("OBJECT_FREE");
   ge.evstr[JVMTI_EVENT_VM_OBJECT_ALLOC          ] = ("VM_OBJECT_ALLOC");

#if defined(IBM_JVM)
   ge.evstr[TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_INTERPRETED] = ("METHOD_ENTRY_INTERP");
   ge.evstr[TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_COMPILED] = ("METHOD_ENTRY_JITTED");
   ge.evstr[TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_NATIVE] = ("METHOD_ENTRY_NATIVE");
   ge.evstr[TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_PARTIAL_IN_LINE ] = ("METHOD_ENTRY_PATIAL_INLINE");
   ge.evstr[TIExtOff + JVMTI_EVENT_METHOD_ENTRY + COM_IBM_METHOD_ENTRY_EXTENDED_IN_LINE ] = ("METHOD_ENTRY_INLINE");
#endif

   if ( gc.mask & gc_mask_JVMTI_info )
   {
      for ( i = 0; i < MaxEvent; i++ )
      {
         if ( ge.evstr[i][0] != '-' )
         {
            OptVMsg( "%4d: ee = %4d, type = %d, %s\n",
                     i, ge.ee[i], ge.type[i], ge.evstr[i] );
         }
      }
   }
}

//======================================================================
// Initialize global variable, options, and do critical early parsing
//======================================================================

char * initJprof( char * jopts )        // Options specified at load time
{
   char * p;
   char * options = jopts;

   if ( ! JProfInitialized )
   {
      getMyLoadAddress();               // Get JPROF load address for debugging

      ge.minJitCodeAddr = (char *)-1;
      ge.cant_use_a2n   = 1;            // Assume that A2N is not available until proven otherwise

      ge.pidx           = 1;            // Default is _pid extension on all log files
      ge.newHeader      = 1;

      ge.tagNextObject  = 0;

      gc.memoryUsageThreshold = 1 << 22;   // Set first memory threshold at 4MB
      
      ge.HealthCenter   = 0;

      if ( NULL != ( p = getenv("JPROFOPTS") )   // Upper case version has priority
           || NULL != ( p = getenv("jprofopts") ) )
      {
         options = (char *)xMalloc( "JPROFOPTS", strlen(jopts) + strlen(p) + 2 );

         strcpy( options, jopts );

         Java2Native( options );

         strcat( strcat( options, "," ), p );
      }
      else
      {
         options = dupJavaStr(jopts);
      }

      if ( 0 != strstr( options, "verbose" )
           || 0 != strstr( options, "VERBOSE" ) )
      {
         gc.verbose |= gc_verbose_stderr;
      }

      if ( 0 != strstr( options, "verbmsg" )
           || 0 != strstr( options, "VERBMSG" ) )
      {
         gc.verbose |= gc_verbose_logmsg;
      }

      if ( 0 != ( p = strstr( options, "mask=" ) )
           ||  0 != ( p = strstr( options, "MASK=" ) ) )
      {
         gc.mask     = strtoul(p+5, NULL, 16);
         gc.verbose |= gc_verbose_logmsg;
      }

      sprintf(version_string, "Version_%d.%d.%d", V_MAJOR, V_MINOR, V_REV);

#if (defined(_WINDOWS) && !defined(_IA64)) || defined(_LINUX) || defined(_AIX)

   #if defined(HAVE_GetTscSpeed)
      ge.cpuSpeed = GetTscSpeed();
   #else
      ge.cpuSpeed = GetProcessorSpeed();
   #endif

#endif

#if defined(_ZOS)
      if ( 0 != strstr( options, "asid" )
           || 0 != strstr( options, "ASID" ) )
      {
         ge.asid = 1;
      }
#endif

      ge.pid = CurrentProcessID();      // MUST follow test for ASID

      ge.socketJprof = INVALID_SOCKET;  // Socket we're listening on

      // initialize locks
      init_lock(ObjTableLock);          //   1  0x0001
      init_lock(ThreadLock);            //   2  0x0002
      init_lock(ClassLock);             //   4  0x0004
      init_lock(MethodLock);            //   8  0x0008
      init_lock(ObjectLock);            //  16  0x0010
      init_lock(XrtdLock);              //  32  0x0020
      init_lock(TagLock);               //  64  0x0040
      init_lock(TreeLock);              // 128  0x0080
#ifndef _LEXTERN
      init_lock(StkNodeLock);           // 256  0x0100
#endif //_LEXTERN

      JProfInitialized = 1;
   }
   return( options );
}

//----------------------------------------------------------------------
// StopJProf -- JProf termination processing
//----------------------------------------------------------------------

void StopJProf( int stopcode )
{
   jint  i;
   jint  rc;
   int   hh, mm, ss;
   int   YY, MM, DD;

   thread_t * tp;

   // See stop codes in jprof.h
   char * stop_code_name[NUM_STOP_CODES+1] =
   {
      "PROCESS_DETACH",                 // 0
      "JVMPI_EVENT_JVM_SHUT_DOWN",      // 1
      "JVMTI_VMDEATH",                  // 2
      "JPROF_DETACH",                   // 3
      "RTDRIVER",                       // 4
      "SHUTDOWN_EVENT",                 // 5
      "PLAYBACK_SHUTDOWN_EVENT",        // 6
      "PLAYBACK_SHUTDOWN",              // 7
      "ERROR_SHUTDOWN",                 // 8
      "SYSTEM_RESUMED_FROM_SLEEP",      // 9
      "UNKNOWN"
   };

   if ( 0 == gc.ShuttingDown )
   {
      gc.ShuttingDown = 1;

      if ( gv )
      {
         get_current_time(&hh, &mm, &ss, 0, 0, 0);
         OptVMsgLog(" (%02d:%02d:%02d) ", hh, mm, ss);

         if ( ge.gc_running )
         {
            OptVMsgLog( "JPROF termination during GC, stopcode = %d (%s)\n", stopcode, stop_code_name[stopcode]);
         }
         else
         {
            OptVMsgLog( "Normal JPROF termination, stopcode = %d (%s)\n", stopcode, stop_code_name[stopcode]);
         }

         logAllocStats();

         // FIX ME, Does this need RTDRIVERLOCK?
         if ( gv_jlm_Off != gv->jlm )   // JLM tracing is active
         {
            tiJLMdump();
         }

         ge.fActive = 0;                // Stop processing events

         tp = readTimeStamp(CurrentThreadID());

         gv->timeStampFlush = tp->timeStamp;

         EnableEvents1x(0);             // Disable required events

         procSockCmd(0,"END");          // Disable all other events and write files as needed

#ifndef _LEXTERN
   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)

         if ( ge.scs )
         {
            scs_terminate();
         }
   #endif
#endif

         OptVMsg("\n");

         for ( i = 1; i <= LC_MAXIMUM; i = 2*i )
         {
            if ( ge.lockcnt[i] > 0 )
            {
               OptVMsg( "ge.lockcnt[%02X] = %d\n", i, ge.lockcnt[i] );
            }
         }

         OptVMsg("gv->tnodes              = %d\n", gv->tnodes);
         OptVMsg("ge.nodeSize             = %d\n", ge.nodeSize);
         OptVMsg("ge.offNodeCounts        = %d\n", ge.offNodeCounts);
         OptVMsg("ge.offEventBlockAnchor  = %d\n", ge.offEventBlockAnchor);

         OptVMsg("gv->tobjs               = %d\n", gv->tobjs );
         OptVMsg("gv->tbytes              = %d\n", gv->tbytes);
         OptVMsg("gv->lobjs               = %d\n", gv->lobjs );
         OptVMsg("gv->lbytes              = %d\n", gv->lbytes);

         OptVMsg( "\nGlobal Data:   %d\n",
                  sizeof(gv) + sizeof(gd) + sizeof(ge) );

         if ( gd.gen )
         {
            fclose(gd.gen);
            gd.gen = 0;
         }

         if ( gv->physmets )
         {
            PttTerminate();
         }

         if ( gv->selListThreads )
         {
            xFree( gv->selListThreads, *(UINT32 *)gv->selListThreads );
         }
         if ( gv->selListMethods )
         {
            xFree( gv->selListMethods, *(UINT32 *)gv->selListMethods );
         }
         if ( gv->selListClasses )
         {
            xFree( gv->selListClasses, *(UINT32 *)gv->selListClasses );
         }

         xFreeStr( gv->options         );

         xFree( gv->CallStackCountsByFrames, ( MAX_CALL_STACK_COUNT + 1 ) * sizeof(UINT32) );
         xFree( gv->CallStackCyclesByFrames, ( MAX_CALL_STACK_COUNT + 1 ) * sizeof(UINT64) );

         object_table_free();
         method_table_free();
         class_table_free();

         hashStats( &ge.htThreads );

         MVMsg( gc_mask_Malloc_Free, ("Free the Global Variables\n") );

         xFree( gv, sizeof(GV) );
         gv = 0;
         memory_fence();

         logAllocStats();

         OptVMsgLog( "\nNumber of GCs: %"_P64"d\n", ge.numberGCs );

         get_current_time(&hh, &mm, &ss, &YY, &MM, &DD);

         msg_se_log("\n JPROF ending at %02d:%02d:%02d on %04d/%02d/%02d\n\n",
                    hh, mm, ss, YY, MM, DD );

#ifndef _LEXTERN
         if ( ge.fSystemResumed )
         {
            msg_se_log("*****************************************************************\n");
            msg_se_log("**** WARNING *** WARNING *** WARNING *** WARNING *** WARNING ****\n");
            msg_se_log("*****************************************************************\n\n");
            msg_se_log("  A system power state transition (sleep/resume) occurred.\n\n");
            msg_se_log("  Data collection was stopped when the system entered the sleep\n");
            msg_se_log("  state. Results, if any, were saved when the system resumed and\n");
            msg_se_log("  should be OK up to that point. Be sure to check for other\n");
            msg_log(   "  error/warning messages in this file.\n\n");
            msg_se(    "  error/warning messages in the log-msg file.\n\n");
            msg_se_log("*****************************************************************\n");
            msg_se_log("**** WARNING *** WARNING *** WARNING *** WARNING *** WARNING ****\n");
            msg_se_log("*****************************************************************\n\n");
         }
#endif //_LEXTERN
      }

      if ( stopcode >= sjp_RTDRIVER )
      {
         if ( ge.piClient )
         {
            // Tell RTDriver we're finished. send() fails if no RTDriver is listening
            // on the other end, but it doesn't matter because we're going away. Only
            // necessary if RTDriver sent a "shutdown" command.
            sendInt(0);
         }

         exit(0);
      }
      gc.ShuttingDown = 0;
      memory_fence();
   }
}

void UpdateFNM()
{
   strcpy( ge.fnm, ge.logPath );

#if defined(_WINDOWS)
   strcat(ge.fnm, "\\");
#else
   strcat(ge.fnm, "/");
#endif
   strcat( ge.fnm, ge.logPrefix );

   ErrVMsgLog("\n Path and prefix for log files: %s\n", ge.fnm);
}

//----------------------------------------------------------
// addClass - common routine for JVMTI
//----------------------------------------------------------

class_t * addClass( jobject cid,
                    char * name )
{
   class_t * cp;

   if ( 0 == cid )
   {
      err_exit( "addClass: ClassID is 0" );
   }
   cp = insert_class( cid );            // cp always non-zero or err_exit

   if ( name )
   {
      if ( cp->flags & class_flags_nameless )   // Minimize possible timing hole
      {
         cp->class_name = dupJavaStr( name );   // Can use err_exit
         cp->flags     &= ~class_flags_nameless;

         MVMsg(gc_mask_ObjectInfo,
               ("addClass: tag = %08X, name = %s\n",
                cid, cp->class_name));
      }
   }
   else
   {
      WriteFlush(gc.msg, " NULL class_name\n");
   }

   return( cp );
}

//----------------------------------------------------------
// addMethod - common routine for JVMTI
//----------------------------------------------------------

method_t *addMethod( class_t   * cp,
                     jmethodID   mid,
                     char      * name,
                     char      * sig )
{
   method_t * mp;
   char     * p;

   mp = insert_method(mid,0);           // mp always non-zero or err_exit

   if ( mp->flags & method_flags_nameless )
   {
      mp->class_entry = cp;

      Force2Native(name);
      Force2Native(sig);                // FIX ME:  Do we own this string?

      gc.allocMsg     = name;
      p               = xMalloc( "method_name", (int)( strlen(name) + strlen(sig) + 1 ) );

      strcat( strcpy( p, name ), sig );

      mp->method_name = p;
      mp->flags      &= ~method_flags_nameless;
   }
   return( mp );
}



// Add method type legend to a file
void MethodTypeLegend( FILE * fd )
{
   if ( gv->methodTypes )
   {
      WriteFlush(fd, "\n Method Types:\n\n");

      if ( 2 == gv->statdyn )
      {
         WriteFlush(fd, " : i = interp   (static) \n");
         WriteFlush(fd, " : I = Interp   (non-static)\n");
         WriteFlush(fd, " : j = jitted   (static) \n");
         WriteFlush(fd, " : J = Jitted   (non-static)\n");

         if ( gv->recomp )
         {
            WriteFlush(fd, " : r = reJitted (static) \n");
            WriteFlush(fd, " : R = ReJitted (non-static)\n");
         }
         WriteFlush(fd, " : n = native   (static)\n");
         WriteFlush(fd, " : N = Native   (non-staic)\n");
         WriteFlush(fd, " : b = builtin  (static)\n");
         WriteFlush(fd, " : B = Builtin  (non-static)\n");
      }
      else
      {
         WriteFlush(fd, " : I = Interp\n");
         WriteFlush(fd, " : J = Jitted\n");

         if ( gv->recomp )
         {
            WriteFlush(fd, " : R = ReJitted\n");
         }
         WriteFlush(fd, " : N = Native\n");
         WriteFlush(fd, " : B = Builtin\n");
      }
      WriteFlush(fd, " : C = Compiling\n");
      WriteFlush(fd, " : O = Other\n");

#ifndef _LEXTERN

      if ( ge.scs )
      {
         WriteFlush(fd, " : M = Method\n");

         if ( ge.scs_a2n )
         {
            WriteFlush(fd, " : U = User\n");
            WriteFlush(fd, " : K = Kernel\n");

            if ( gv->lockTree )
            {
               WriteFlush(fd, " : a = List of Unresolved Addresses\n");
               WriteFlush(fd, " : u = Raw User Address\n");
               WriteFlush(fd, " : k = Raw Kernel Address\n");
            }
         }
      }
#endif
   }
}

/******************************/
/*
 * Name: ParseJProfOptions
 *
 * Options: (too many to list)
 *
 *  Parm   : option str
 *  Return : JVMTI_ERROR_NONE or JVMTI_ERROR_INTERNAL
 ******************************/
jint ParseJProfOptions(char * options)
{
   char * p1;
   char * s;
   char   parm[MAX_BUFFER_SIZE];
   int    i, j, n;
   int    nodeSize;

   char * p;
   int    metric;
   int    cpu;

   char   buf[READ_SIZE];               // File input buffer

   int    hh, mm, ss;
   int    YY, MM, DD;

   char   c;

   // local vars to handle option overrides
   int builtin = -1;                    // -1 to distinguish from 0 or 1

   OptVMsg(" => ParseJProfOptions\n");

   ge.ObjectInfo   = 0;                 // Not really persistent (in GE for Hookit)

   if ( 0 == gv )
   {
      gv = (GV *)zMalloc( "Global Variables", sizeof(GV) );
   }

   get_current_time(&hh, &mm, &ss, &YY, &MM, &DD);
   gv->run_start_ts = ReadCycles();
   gv->run_start_ms = (ss * GV_MS_PER_SECOND) + (mm * GV_MS_PER_MINUTE) + (hh * GV_MS_PER_HOUR);
#if defined(_AIX)
   gv->ts_ticks_per_msec = pi_msec_to_tbticks(1);
#else
   gv->ts_ticks_per_msec = Uint64ToInt32(ge.cpuSpeed / 1000);
#endif

   gv->options     = options;

   gv->GCInfo      = 1;                 // Always track GCs

   gv->msglimit    = 10;                // Maximum number of error messages of each kind

   gv->rtdriver    = 1;                 // Start the RTDriver listener thread
   gv->legend      = 1;                 // Include legend in log-rt/gen

   gv->addbase     = 1;                 // allows on/off accum of base via rtdriver

   gv->revtree     = 1;                 // add nodes at the end (time order in log-rt)

   gv->tagging     = 1;                 // Use Tagging, if available
   gv->useTagging  = 1;                 // Use Tagging, if available

   if ( 0 == ge.logPath )
   {
      if ( 0 != ( p = getenv("IBMPERF_JPROF_LOG_FILES_PATH") ) )
      {
         ge.logPath   = xStrdup("IBMPERF_JPROF_LOG_FILES_PATH",p);
      }
      else
      {
         ge.logPath = xStrdup( "LOGPATH=", "." );   // Allocate it so it can be safely freed
      }

      if ( 0 != ( p = getenv("IBMPERF_JPROF_LOG_FILES_NAME_PREFIX") ) )
      {
         ge.logPrefix = xStrdup("IBMPERF_JPROF_LOG_FILES_NAME_PREFIX",p);
      }
      else
      {
         ge.logPrefix = xStrdup( "LOGPREFIX=", "log" );   // Allocate it so it can be safely freed
      }

      if ( 0 != ( p = getenv("IBMPERF_JPROF_LOG_FILES_NAME_SUFFIX") ) )
      {
         ge.logSuffix = xStrdup("IBMPERF_JPROF_LOG_FILES_NAME_SUFFIX",p);
      }
      else
      {
         ge.logSuffix = xStrdup( "LOGSUFFIX=", "" );   // Allocate it so it can be safely freed
      }

      UpdateFNM();
   }

   strcpy(gv->version, "\n JPROF ");
   strcat(gv->version, version_string);
   strcat(gv->version, " (JVMTI");

   p = getJprofPath();

   if ( p )
   {
      strcat( strcat(gv->version, ")\n\n  (OnDisk   : "), p );
      xFreeStr( p );
   }
   strcat(strcat(gv->version, ")\n\n  (Built    : "), BUILDDATE);
   strcat(strcat(gv->version, ")\n\n  (Source   : "), SOURCEDATE);

   // defaults

#ifndef _LEXTERN
   ge.scs             = 0;              // Callstack Sampling mode
#endif

   gv->gatherMetrics   = 1;             // Gather time and metrics, overridden by SCS
   gv->prunecallstacks = 1;             // Prune unreported methods from call stacks, overridden by SCS

   gv->exitpop         = 1;             // +/- Unique exit via pop after exception
   gv->recomp          = 0;             // +/- Detect recompilation
   gv->statdyn         = 0;             // +/- Detect static/dynamic methods

   gv->exitNoRc        = 1;             // +/- Use MethodExitNoRc event, if possible
   gv->methodTypes     = 1;             // +/- Detect and report method types in log-rt

   gv->methInline      = 1;             // +/- Set AllowMethodInliningWithMethodEnterExit, if possible
   gv->directJNI       = 0;             // +/- Use AllowDirectJNIWith MethodEnterExit, if possible

   gv->logRtdCmds      = 1;             // +/- Log all RTDriver commands received

   ge.chashb           = 64007;         // Number of hash buckets in class   table
   ge.ohashb           = 1000003;       // Number of hash buckets in object  table
   ge.mhashb           = 20011;         // Number of hash buckets in method  table
   ge.thashb           = 2053;          // Number of hash buckets in thread  table
#ifndef _LEXTERN
   ge.shashb           = 17;            // Number of hash buckets in stknode table (per thread)
#endif //_LEXTERN

   if ( JVM_J9 == ge.JVMversion )
   {
      gv->statdyn = 1;                  // Detect static/dynamic methods
   }
   gv->BuiltinEntryExit = 1;            // BUILTIN_METHOD_ENTRY GEN_BUILTIN_METHOD

   OptVMsg(" lockdis default %x\n", ge.lockdis);

#if defined(_AIX)
   gv->systemPtt   = 1;                 // Use system code for PTT, if available
#endif

   init_event_names();                  // mminit strs & numbers

   p1 = options;

   gv->options = xStrdup( "options", options );

   OptVMsg(" JPROF Options: %s\n\n", gv->options);

   gv->separator = 0;                   // Assume no separator

   while ( p1 )                         // Process all parms, plus one null parm
   {
      while ( ',' == *p1 )
      {
         p1++;                          // Skip null parameters
      }

      if ( 0 == *p1 )                   // End of parameters
      {
         if ( gc.mHelp )
         {
            msg_se("%s)\n", gv->version);

            // Display general help text

            parm[0]       = 0;          // Force help output
            gv->pNextParm = 0;          // Exit after one help pass
            gv->separator = 0;          // Guarantee no use of pNextParm
         }
         else
         {
            break;                      // All done
         }
      }
      else
      {
         // Extract one parameter from the options and compute pNextParm

         gv->pNextParm = p1;

         while ( 0 != *gv->pNextParm    // Find end of parm
                 && ',' != *gv->pNextParm )
         {
            gv->pNextParm++;
         }
         gv->separator  = *gv->pNextParm;
         *gv->pNextParm = 0;            // Replace separator with 0 ==> p1 is a token

         if ( gv->separator )
         {
            gv->pNextParm++;            // Advance to next parameter
         }

         // Put upper case copy of parameter in parm

         s = strcpy(parm, p1);

         while ( 0 != *s )
         {
            *s = (char)toupper(*s);
            s++;
         }
      }

//----------------------------------------------------------------------
// Handle common prefixes: +, -, ALL, and NO ==> Set gc.posNeg
//----------------------------------------------------------------------

      p = parm;

      gc.posNeg      = 0;               // Assume no +/- specified

      if ( *p )
      {
         if ( '+' == *p )               // Positive prefix found
         {
            gc.posNeg = 1;              // Preset positive match
            p++;                        // Skip prefix, use default positive match
         }
         else if ( '-' == *p )          // Negative prefix found
         {
            gc.posNeg = -1;             // Preset negative match
            p++;                        // Skip prefix
         }
         else if ( 'N' == *p && 'O' == p[1] )
         {
            gc.posNeg = -1;             // Preset negative match
            p += 2;                     // Skip prefix
         }
         else if ( 'A' == *p && 'L' == p[1] && 'L' == p[2] )
         {
            gc.posNeg = 1;              // Preset positive match
            p += 3;                     // Skip prefix
         }

         if ( p != parm )
         {
            if ( 0 == *p )
            {
               ErrVMsgLog( "Prefix without option in %s\n", p1 );
               return(JVMTI_ERROR_INTERNAL);
            }
            strcpy(parm, p);
         }

         n = (int)strlen(parm);

         if ( n )
         {
            if ( '+' == parm[n-1] )
            {
               gc.posNeg = 1;           // Preset positive match
               parm[n-1] = 0;
            }
            else if ( '-' == parm[n-1] )
            {
               gc.posNeg = -1;          // Preset negative match
               parm[n-1] = 0;
            }

            if ( 0 == parm[0] )
            {
               ErrVMsgLog( "Suffix without option in %s\n", p1 );
               return(JVMTI_ERROR_INTERNAL);
            }
         }
      }

      if ( JVMTI_ERROR_NONE != ParseOption( parm, p1 ) )
      {
         return(JVMTI_ERROR_INTERNAL);
      }

      if ( gd.config && 0 == gv->separator )   // Reading config and end of current line
      {
         n = 0;

         while ( fgets( buf, READ_SIZE, gd.config ) != NULL )
         {
            p = buf;

            while ( 0 != ( c = *p ) )
            {
               if ( '\n' == c || '\r' == c )
               {
                  *p = 0;
                  break;
               }
               p++;
            }
            n = (int)( p - buf );

            if ( n
                 && '/' != buf[0]
                 && '/' != buf[1] )
            {
               break;
            }
         }

         if ( n )
         {
            gv->pNextParm = buf;
            gv->separator = 0;
         }
         else
         {
            fclose( gd.config );
            gd.config     = 0;

            gv->pNextParm = gv->savedParms;
            gv->separator = gv->savedSep;
         }
      }
      p1 = gv->pNextParm;
   }

   if ( gc.mHelp )
   {
      msg_se( "\n" );

      gc.mHelp = 0;

      if ( ge.fEchoToSocket )
      {
         return(JVMTI_ERROR_INTERNAL);
      }
      else
      {
         err_exit(0);
      }
   }

   if ( gv->showVersion )
   {
      msg_se("%s)\n", gv->version);

      if ( ge.fEchoToSocket )
      {
         return(JVMTI_ERROR_INTERNAL);
      }
      else
      {
         err_exit(0);
      }
   }

//----------------------------------------------------------------------
// Options all decoded. Resolve any interaction/ordering issues
//----------------------------------------------------------------------

#ifndef _LEXTERN
   if ( ge.scs && 0 == ge.jvmti )
   {
      ge.EnExCs = 1;                    // Use Entry/Exit callstacks with SCS and Hookit
   }
#endif

   gv->types = ( gv->statdyn == 2 ) ? gv_TypesSD : gv_TypesNSD;

   nxShowList( gv->selListThreads );
   nxShowList( gv->selListMethods );
   nxShowList( gv->selListClasses );

   if ( gv->calltrace && gv->trees )
   {
      gv->calltree      = 1;
   }

   if ( gv->start )
   {
      if ( ge.ObjectInfo )
      {
         ge.ObjectInfo = ge_objectinfo_ALL;
      }
   }

   if ( ge.pidx )
   {
      sprintf(ge.pidext, "_%d", ge.pid);
   }
   else
   {
      ge.pidext[0] = 0;
   }

   strcat(gv->version, ")\n\n  (Platform : ");
   strcat(gv->version, _PLATFORM_STR);

   p = get_platform_info();

   if ( p )
   {
      strcat(gv->version, p);
      xFreeStr( p );
   }

   if ( 0 == ge.hostname )
   {
      if ( 0 == gethostname( buf, READ_SIZE ) )
      {
         ge.hostname = xStrdup( "hostname", buf );
      }
      else
      {
         ge.hostname = "UNKNOWN";
      }
   }
   strcat( strcat( gv->version, ", Hostname=" ), ge.hostname );

   strcat(gv->version, ")\n\n  (JVM Info : ");
   strcat(gv->version, ge.jvmver);
   strcat(gv->version, ")\n\n");

   sprintf( gv->loadtime, " JPROF started at %02d:%02d:%02d on %04d/%02d/%02d\n\n",
            hh, mm, ss, YY, MM, DD );

#if defined(_WINDOWS)

   hh = (int)GetDLLVersionEx();         // Make sure PerfUtil.dll is at a version we can work with

   if (hh < REQUIRED_PERFUTIL_VERSION)
   {
      ErrVMsgLog("\nERROR: Incorrect PerfUtil.dll version. Current: v%d.%d.%d. Required: v%d.%d.%d. Quitting.\n\n",
                 (hh >> 24),
                 ((hh & 0x00FF0000) >> 16),
                 (hh & 0x0000FFFF),
                 (REQUIRED_PERFUTIL_VERSION >> 24),
                 ((REQUIRED_PERFUTIL_VERSION & 0x00FF0000) >> 16),
                 (REQUIRED_PERFUTIL_VERSION & 0x0000FFFF));
      return(JVMTI_ERROR_INTERNAL);
   }
#endif


   if ( strstr( _PLATFORM_STR, "64" ) )
   {
      ge.is64bitCPU = 1;
   }

   if ( 0 == ge.cpuSpeed )
   {
      ge.cpuSpeed = 2000000000;

      ErrVMsgLog( "\nMissing cpuSpeed being defaulted to 2Ghz\n" );
   }

   if ( gd.syms )
   {
#if defined(_AIX)

      mode_t  mask;

      sprintf(buf, "/tmp/Java%"_P64"d.syms\0", (uint64_t)ge.pid);
      mask    = umask(077);
      gd.syms = OpenJprofFile(buf, "w+", "SYMS");
      umask(mask);

#elif defined(_WINDOWS)

      strcpy( buf, "test.syms" );
      gd.syms = OpenJprofFile(buf, "w+", "SYMS");

#else
      ErrVMsgLog( "SYMS option not supported on this platform\n" );
      return(JVMTI_ERROR_INTERNAL);
#endif

      if ( 0 == gd.syms )
      {
         ErrVMsgLog("**ERROR** Unable to open %s\n", buf);
         return(JVMTI_ERROR_INTERNAL);
      }
      WriteFlush( gd.syms, "Symlib/AIX - JPROF %s %s - gensyms format\n",
                  version_string,
                  ge.jvmti ? "JVMTI" : "NO JVMTI!!" );

      WriteFlush( gd.syms,
                  "Java: %s %6"_P64"d %s\n",
                  ge.is64bitCPU ? "64bit" : "32bit",
                  (uint64_t)ge.pid,
                  ge.hostname );
   }

   for ( metric = 0; metric < gv->usermets; metric++ )
   {
      if ( gv->sf[metric] > 0 )
      {
         gv->msf[metric] = 1.0;

         for ( i = 0; i < gv->sf[metric]; i++ )
         {
            gv->msf[metric] *= 10.0;    // msf = 10 ** sf
         }
      }
      else if ( gv->sf[metric] < 0 )
      {
         gv->msf[metric] = 1.0;
         j              = 0 - ( 1 + gv->sf[metric] );

         for ( i = 0; i < j; i++ )
         {
            gv->msf[metric] *= 10.0;    // msf = 10 ** abs( 1 + sf )
         }
         gv->msf[metric] = ge.cpuSpeed / gv->msf[metric];   // msf = speed * ( 10 ** ( 1 + sf ) )
      }
      OptVMsgLog( "gv->msf[%d] = %f\n", metric, gv->msf[metric] );
   }

//----------------------------------------------------------------------
// Write the banner and open the log-msg file
//----------------------------------------------------------------------

   writeStderr(gv->version);
   writeStderr(gv->loadtime);
   writeStderr(" JPROF options: ");
   writeStderr(gv->options);
   writeStderr("\n\n");

   if ( 0 == gc.msg )
   {
      if ( gv->g2se )
      {
         gc.msg = stderr;

         if ( gc.verbose )
         {
            fprintf(stderr, "Writing to stderr instead of log-msg\n");
         }
      }
      else
      {
         gc.msg = file_open( "msg", 0 );
      }
      writeDelayedMsgs();
   }

   if ( gv->selListMethods
        && 0 == ge.clearMethodSelectiveEntryExitNotify )
   {
      ErrVMsgLog( "\nMethod selectivity not recommended with this JVM\n");
   }

   // validate callstack sampling options (if any)
   if (JVMTI_ERROR_NONE != scs_check_options(&gv->scs_opts))
   {
      return(JVMTI_ERROR_INTERNAL);
   }

   if (0 == gd.a2n && gv->JITA2N)
   {
      gd.a2n = file_open("jita2n",0);

      WriteFlush(gd.a2n, " JitA2N\n\n");
      WriteFlush(gd.a2n, " BINARY DATA IN PRINTABLE HEX\n");
      WriteFlush(gd.a2n, " Process_Number_ID=%u\n", ge.pid);
   }
   if (0 == gd.jtnm && ge.ThreadInfo)
   {
      gd.jtnm = file_open("jtnm",0);
   }

   // create calltree file name: gv->rtfnm
   // live-dump files "-d%d" added to file names
   // **** This may change if rtdriver command "fnm=" changes the
   // **** path to the log-rt files.
   //STJ strcat( strcat( strcpy(gv->rtfnm, ge.fnm), "-rt"), pidex);

   /* disable SUN as default */
   // TBD
   // Cant turn CompiledEntryExit off since this code overrides

   if ( ( gv->jstem == 0
          && ( gv->calltree || gv->calltrace || ge.EnExCs ) ) )
   {
      // CHECK OVERRIDES HERE

      // explicit enable 2000 method entry/exit hooks
      gv->Methods           = 1;        // METHOD_...

      gv->CompiledEntryExit = 1;        // COMPILED_METHOD_ENTRY _EXIT
      // GEN_COMPILED_METHOD HOOK_JITTED_METHOD_ALLOWED
      gv->InlineEntryExit   = 1;        // INLINE_METHOD_ENTRY GEN_INLINE_METHOD
      gv->NativeEntryExit   = 1;        // NATIVE_METHOD_ENTRY

      ge.ThreadInfo         = 1;
   }

   if (gv->JITA2N)
   {
      gv->CompileLoadInfo   = 1;        // COMPILED_METHOD _LOAD UNLOAD
   }

   if ( gv->sobjs
        && ( gv->scs_allocations > 1
             || gv->scs_allocBytes > 1 ) )
   {
      ErrVMsgLog( "SOBJS not supported with thresholds\n" );
      return(JVMTI_ERROR_INTERNAL);
   }

   if ( ge.ObjectInfo )
   {
      gv->scs_event_object_classes = 1;

      if ( gv->scs_allocations > 1
           || gv->scs_allocBytes > 1 )
      {
         ErrVMsgLog( "\n********************************************************************\n" );
         ErrVMsgLog( "WARNING: OBJECTINFO sampling with thresholds misses many allocations\n" );
         ErrVMsgLog( "********************************************************************\n" );
      }
   }

   if ( gv->scs_event_object_classes )
   {
      gv->ClassLoadInfo = 1;            // FIX ME:Required for JVMTI until new algorithm created
   }

   if ( JVMTI_ERROR_NONE != SetMetricMapping() )
   {
      return(JVMTI_ERROR_INTERNAL);
   }

   if ( gv->calltrace
        && MODE_CPI     == gv->usermet[0] )
   {
      ErrVMsgLog( "Calltrace processing invalid if first metric is composite\n" );
      return(JVMTI_ERROR_INTERNAL);
   }

// Compute the size of a node

   gv->sizePhysMets = gv->physmets * sizeof(UINT64);
   ge.sizeMetrics  = ( gv->physmets  + 1 ) * sizeof(UINT64);

   ge.offTimeStamp        = 0;
   ge.offNodeCounts       = 0;
   ge.offLineNum          = 0;
   ge.offEventBlockAnchor = 0;

   nodeSize = offsetof( Node, bmm ) + ge.sizeMetrics;   // Includes calibrated metrics

   if ( gv->scs_allocations
        && 0 == gv->scs_event_object_classes )
   {
      nodeSize += sizeof(UINT64);       // Make room for extra allocations metric
   }

   if ( gv->TSnodes )
   {
      ge.offTimeStamp        = nodeSize;
      nodeSize              += sizeof(UINT64);   // TimeStamp of Node Creation
   }

#ifndef _LEXTERN
   if ( ge.scs )
   {
      ;
   }
   else
#endif
   {
      ge.offNodeCounts       = nodeSize;
      nodeSize              += sizeof(UINT64) * 4;   // Array of EnEx counts
   }

   if ( ge.ExtendedCallStacks )
   {
#if defined(_64BIT)

      ge.offLineNum          = offsetof( Node, lineNum );   // Line Number used with ExtendedCallStacks option

#else

      ge.offLineNum          = nodeSize;
      nodeSize              += sizeof(UINT32);   // Line Number used with ExtendedCallStacks option

#endif
   }

   if ( gv->scs_event_object_classes )
   {
      ge.offEventBlockAnchor = nodeSize;
      nodeSize              += sizeof(RTEventBlock *);   // Event Block list anchor

      if ( ge.ObjectInfo )
      {
         ge.sizeRTEventBlock = sizeof(RTEventBlock);
      }
      else
      {
         ge.sizeRTEventBlock = offsetof( RTEventBlock, liveObjects );
      }
   }

   gv->sizeClassBlock = sizeof(class_t);

   ge.nodeSize = nodeSize;

   OptVMsgLog( "Node    Size = %d\n", ge.nodeSize          );
   OptVMsgLog( "Class   Size = %d\n", gv->sizeClassBlock   );
   OptVMsgLog( "Method  Size = %d\n", sizeof(method_t)     );
   OptVMsgLog( "Thread  Size = %d\n", sizeof(thread_t)     );
   OptVMsgLog( "RTEvent Size = %d\n", ge.sizeRTEventBlock );

   // Initialize A2N. Create the java process node because the JVM starts
   // can jitting before it sends the VM_INIT event and we need to be
   // be able to handle those jitted methods.

#ifndef _LEXTERN
   if ( ge.scs_a2n )
   {
      scs_a2n_init();                   // This does not work with 32-bit JVM on 64-bit Windows
   }
#endif

   if ( gv->scs_active )
   {
      thread_t      * tp = readTimeStamp(CurrentThreadID());
      gv->timeStampStart = tp->timeStamp;
   }

   OptVMsg(" lockdis aft opts %x\n", ge.lockdis);

   WriteFlush(gc.msg, " Logical Metrics    = %d\n", gv->usermets);
   WriteFlush(gc.msg, " Physical Metrics   = %d\n", gv->physmets);

   OptVMsg(" <= ParseJProfOptions\n");
   return(JVMTI_ERROR_NONE);
}

int parseJProfOptionsFromPlugin(char * options)
{
   int rc = 0;
   char *mutableOptions = dupJavaStr(options);
   rc = ParseJProfOptions(mutableOptions);
   if ( JVMTI_ERROR_NONE != rc )
   {
      logFailure(rc);
      return( JNI_ERR );
   }
}
