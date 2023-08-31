#include "jprof.h"
#include "tree.h"

//======================================================================
// ProcessOption - Process one JPROF option
//======================================================================

int ParseOption( char * parm,           // Upper case option
                 char * p1 )            // Unmodified option
{
   char * p;
   char * q;
   int    rc;
   int    i;

   if ( helpMsg( parm, 0, "\nJProf HELP:\n" ) )
   {
      ;
   }
   
   else if ( helpTest2( parm,
                       &ge.HealthCenter,
                       "HC",
                       "HEALTHCENTER",
                       "Run Jprof in HealthCenter mode"
                      ))
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "CONFIG=",
                       "Read configuration information from the selected file"
                     ))
   {
      if ( gd.config )
      {
         fprintf(stderr, "\n**ERROR** CONFIG files can not be nested.\n\n");
         return JVMTI_ERROR_INTERNAL;
      }

      gd.config = OpenJprofFile( (char *)p1+7, "r", 0 );

      if ( 0 == gd.config )
      {
         fprintf(stderr, "\n**ERROR** Could not open CONFIG file.\n\n");
         return JVMTI_ERROR_INTERNAL;
      }

      gv->savedParms = gv->pNextParm;
      gv->savedSep   = gv->separator;
      gv->separator  = 0;
   }

   else if ( helpTest( parm,
                       0,
                       "VERBMSG",
                       "Enable verbose output to log-msg"
                     ))
   {
      gc.verbose |= gc_verbose_logmsg;
   }

   else if ( helpTest( parm,
                       0,
                       "VERBOSE",
                       "Enable verbose output to stderr"
                     ))
   {
      gc.verbose |= gc_verbose_stderr;
   }

   else if ( helpTest( parm,
                       0,
                       "MASK=",
                       "?Flag mask used to enable extra debug output\n\t0100 = Show events"
                     ))
   {
      p1 += 5;
      gc.mask     = strtoul(p1, NULL, 16);
      gc.verbose |= gc_verbose_logmsg;
   }

   else if ( helpTest( parm,
                       0,
                       "FNM=",
                       "?DEPRECATED: Use LOGPATH= and LOGPREFIX= instead of FNM="
                     ))
   {
      strcpy(ge.fnm, p1+4);
      ErrVMsgLog( "DEPRECATED: Use LOGPATH= and LOGPREFIX= instead of FNM=\n" );
   }

   else if ( helpTest( parm,
                       0,
                       "LOGPATH=",
                       "Path to be used with all log files"
                     ))
   {
      if ( p1[8] )
      {
         p = ge.logPath;
         ge.logPath = xStrdup("LOGPATH=",p1+8);
         xFreeStr( p );

         UpdateFNM();
      }
   }

   else if ( helpTest( parm,
                       0,
                       "LOGPREFIX=",
                       "Prefix to replace 'log' in all log file names"
                     ))
   {
      if ( p1[10] )
      {
         p = ge.logPrefix;
         ge.logPrefix = xStrdup("LOGPREFIX=",p1+10);
         xFreeStr( p );

         UpdateFNM();
      }
   }

   else if ( helpTest( parm,
                       0,
                       "LOGSUFFIX=",
                       "Suffix to be appended to all log file names"
                     ))
   {
      if ( p1[10] )
      {
         p = ge.logSuffix;
         ge.logSuffix = xStrdup("LOGSUFFIX=",p1+10);   // Possible tiny memory leak to simplify code
         xFreeStr( p );
      }
   }

   else if ( helpTest( parm,
                       &ge.pidx,
                       "PIDX",
                       "Add/remove _pid suffix on output files (-pidx to remove)"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "PORT=",
                       "Select the port number to be used by RTDRIVER"
                     ))
   {
      ge.port_number = atoi( p1 + 5 );

      if (ge.port_number == 0)
      {
         fprintf(stderr, "**ERROR** Invalid PORT value. Must be a numeric, non-zero, positive value.\n");
         return(JVMTI_ERROR_INTERNAL);
      }
   }

   else if ( helpTest2( parm,
                        0,
                        "TPROF",
                        "ITRACE",
                        "Turn on options required for TPROF or ITRACE\n\t.Implies JITA2N, THREADINFO, and JINSTS"
                      ))
   {
      gv->JITA2N     = 1;
      ge.ThreadInfo  = 1;
      gv->jinsts     = 1;
   }

   else if ( helpTest( parm,
                       &gv->JITA2N,
                       "JITA2N",
                       "Write address-to-name information to log-jita2n"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &ge.ThreadInfo,
                       "THREADINFO",
                       "Produce thread information file log-jtnm"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->jinsts,
                       "JINSTS",
                       "Include bytes of instructions in log-jita2n"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gd.syms,
                       "JPA",
                       "Perform the function of libjpa on AIX to produce /tmp/JavaPID.syms"
                     ))
   {
      gv->JITA2N     = 1;
   }

   else if ( helpTest( parm,
                       &gv->vpa63,
                       "VPA63",
                       "Ensure compatability with VPA version 6.3 (NONEWHEADER,OBJRECS,NOHR)\n\tVPA63+ also scales SCS=ALLOCATED_BYTES"
                     ))
   {
      ge.newHeader      = 0;
      gv->objrecs       = 1;
      gv->HumanReadable = 0;

      if ( gc.posNeg > 0 )
      {
         gv->vpa63      = 2;
      }
   }

   else if ( helpMsg( parm, 0, "\nCallflow options:\n" ) )
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->calltree,
                       "CALLTREE",
                       "Perform basic calltree profiling and produce log-rt files\n\tImplies NOMETRICS (requires no driver)"
                     ))
   {
      gc.posNeg  = 0;
      SetMetric( MODE_NOMETRICS );

      gv->CompilingStartEnd = 1;        // COMPILING_START _END

      gv->trees             = 1;
   }

   else if ( helpTest( parm,
                       &gv->calltree,
                       "CALLFLOW",
                       "Perform call flow profiling and produce log-rt\n\tImplies METRICS=PTT_INSTS+PTT_CYCLES"
                     ))
   {

      gc.posNeg = 0;
      SetMetric( MODE_PTT_INSTS );
      gc.posNeg = 1;
      SetMetric( MODE_PTT_CYCLES );

      gv->CompilingStartEnd = 1;        // COMPILING_START _END

      gv->trees             = 1;
   }

   else if ( helpTest2( parm,
                        &gv->calltrace,
                        "CALLTRACE",
                        "GENERIC",
                        "Perform calltrace profiling and produce log-gen\n\tImplies METRICS=PTT_INSTS"
                      ))
   {
      // calltrace wo calltree

      gc.posNeg = 0;
      SetMetric( MODE_PTT_INSTS );

      gv->CompilingStartEnd = 1;        // COMPILING_START _END

   }

   else if ( helpTest( parm,
                       &gv->calltree,
                       "RTARCF",
                       "?Perform real-time call flow profiling and produce log-rt\n\tImplies METRICS=PTT_INSTS"
                     ))
   {
      gc.posNeg = 0;
      SetMetric( MODE_PTT_INSTS );

      gv->CompilingStartEnd = 1;        // COMPILING_START _END

      gv->trees             = 1;
   }

   else if ( helpTest( parm,
                       &ge.ObjectInfo,
                       "OBJECTINFO",
                       "Report object allocations/frees in log-rt files\n\tImplies NOMETHODTYPES"
                     ))
   {
      if ( 1 == gc.posNeg )             // ALLOBJECTINFO - mark all objects at START
      {
         ge.ObjectInfo = ge_objectinfo_ALL;
      }
      gv->methodTypes  = 0;
   }

   else if ( helpTest( parm,
                       &gv->objrecs,
                       "OBJRECS",
                       "Report object allocations as records with AO, AB, LO, and LB fields,\n\tinstead of reporting these fields as logical metrics."
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "MINALLOC=",
                       "Ignore all allocations less than N bytes\n\tUsed with OBJECTINFO and/or SCS=ALLOCATED_BYTES"
                     ))
   {
      gv->minAlloc = atoi(p1+9);
   }

   else if ( helpTest( parm,
                       &gv->methodTypes,
                       "METHODTYPES",
                       "Use NOMETHODTYPES or -METHODTYPES to remove method types from reports"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->sobjs,
                       "SOBJS",
                       "Display finer object information by grouping objects of the same size\n\tUsed with OBJECTINFO and/or SCS=ALLOCATED_BYTES+"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "FGC=",
                       "?Force GC each time the specified allocation threshold is reached"
                     ))
   {
      gv->fgcThreshold = atoi(p1+4);
      gv->fgcCountdown = gv->fgcThreshold;
   }

   else if ( helpTest( parm,
                       &gv->flushAfterGC,
                       "FLUSHAFTERGC",
                       "?Flush the trees to a new log-rt file after every GC"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->jstem,
                       "JSTEM",
                       "?Perform JSTEM processing\n\tImplies METRICS=PTT_INSTS"
                     ))
   {
      gv->calltree          = 1;        // calltree w/o Method event

      ge.ThreadInfo         = 1;
      gv->CompilingStartEnd = 1;        // COMPILING_START _END

      gc.posNeg = 0;
      SetMetric( MODE_PTT_INSTS );
   }

   else if ( helpMsg( parm, 0, "\nMetric Selection options:\n\n\
All metrics can use :n as a suffix to shift out the last n decimal digits.\n\
All metrics ending with _CYCLES can be scaled to seconds, milliseconds,\n\
microseconds, or nanoseconds using :SEC, :MSEC, :USEC, or :NSEC.\n\
Composite metrics have no delta columns in log-rt.\n" ) )
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "METRICS=",
                       "List of one or more of the metrics below, using '+' as a separator"
                     ))
   {
      if ( 0 == p1[8] )
      {
         ErrVMsgLog( "No metrics listed on METRICS=\n" );
         return(JVMTI_ERROR_INTERNAL);
      }
      gv->nxList = parm+8;              // Parse the upper case copy

      gv->NoMetrics   = 0;
      gv->usermets    = 0;              // Always start a new list

      gv->physmets = 0;

      for (;;)
      {
         p = nxGetString();

         if ( 0 == *p )
         {
            break;
         }

         if ( *gv->nxModifier )
         {
            ErrVMsgLog( "Invalid syntax: METRICS= does not use modifiers\n" );
            return(JVMTI_ERROR_INTERNAL);
         }

         if ( '-' == gv->nxInclExcl )
         {
            ErrVMsgLog( "METRICS= is an inclusive list, not an inclusive/exclusive list\n" );
            return(JVMTI_ERROR_INTERNAL);
         }

//----------------------------------------------------------------------
// Handle common suffixes:  :scaleFactor and _CYCLES:[M|U|N]SEC
//----------------------------------------------------------------------

         gv->scaleFactor = 0;           // Assume no scaling factor

         q = p;

         while ( 0 != *q                // Find end of parm
                 && ':' != *q )
         {
            q++;
         }

         if ( *q )                      // Suffix found
         {
            *q++ = 0;                   // Remove suffix from parm

            if ( '0' <= *q && '9' >= *q )   // Decimal scaling factor
            {
               while ( '0' <= *q && '9' >= *q )
               {
                  gv->scaleFactor = ( gv->scaleFactor * 10 ) + ( *q - '0' );
                  q++;
               }
            }
            else
            {
               i = (int)strlen( p );

               if ( i > 7
                    && 0 == strcmp( "_CYCLES", p + i - 7 ) )
               {
                  if ( 0 == strcmp( q, "SEC" ) )   // Seconds
                  {
                     gv->scaleFactor = gv_sf_SECONDS;
                  }
                  else if ( 0 == strcmp( q, "MSEC" ) )   // Milliseconds
                  {
                     gv->scaleFactor = gv_sf_MILLISECONDS;
                  }
                  else if ( 0 == strcmp( q, "USEC" ) )   // Microseconds
                  {
                     gv->scaleFactor = gv_sf_MICROSECONDS;
                  }
                  else if ( 0 == strcmp( q, "NSEC" ) )   // Nanoseconds
                  {
                     gv->scaleFactor = gv_sf_NANOSECONDS;
                  }
                  else
                  {
                     ErrVMsgLog( "%s is an invalid metric scaling factor\n", q );
                     return(JVMTI_ERROR_INTERNAL);
                  }
               }
            }
            if ( gv->scaleFactor )
            {
               OptVMsgLog( " Parsed: :%s = Scaling Factor\n", q );
            }
         }

//----------------------------------------------------------------------

         for ( i = 0; i <= MODE_CPI; i++ )
         {
            if ( 0 == strcmp( p, ge.mmnames[i] ) )
            {
               if ( 0 != strcmp( "undefined", ge.mmnames[i] ) )
               {
                  gc.posNeg = 1;

                  if ( JVMTI_ERROR_NONE != SetMetric( i ) )
                  {
                     return(JVMTI_ERROR_INTERNAL);
                  }
                  break;
               }
            }
         }
         if ( i > MODE_CPI )
         {
            ErrVMsgLog( "Invalid metric specified: %s\n", p );
            return(JVMTI_ERROR_INTERNAL);
         }
      }
   }

   else if ( helpTest( parm,
                       0,
                       "PTT_INSTS",
                       "Per Thread Time with metric = Instructions Completed"
                     ))
   {
      if ( JVMTI_ERROR_NONE != SetMetric( MODE_PTT_INSTS ) )
      {
         return(JVMTI_ERROR_INTERNAL);
      }
   }

   else if ( helpTest( parm,
                       0,
                       "PTT_CYCLES",
                       "Per Thread Time with metric = Cycles"
                     ))
   {
      if ( JVMTI_ERROR_NONE != SetMetric( MODE_PTT_CYCLES ) )
      {
         return(JVMTI_ERROR_INTERNAL);
      }
   }

   else if ( helpTest( parm,
                       0,
                       "CPI",
                       "Composite metric = PTT_CYCLES / PTT_INSTS (Cycles per Instruction)"
                     ))
   {
      if ( JVMTI_ERROR_NONE != SetMetric( MODE_CPI ) )
      {
         return(JVMTI_ERROR_INTERNAL);
      }
   }

   else if ( helpTest( parm,
                       0,
                       "METRICS",
                       "Use NOMETRICS or -METRICS when executing without a device driver\n\tUse ALLMETRICS or +METRICS to get all of the metrics above"
                     ))
   {
      if ( 1 == gc.posNeg )             // All metrics
      {
         gc.posNeg = 0;
         SetMetric( MODE_PTT_INSTS );
         gc.posNeg = 1;
         SetMetric( MODE_PTT_CYCLES );
         gc.posNeg = 1;
         SetMetric( MODE_CPI );
      }
      else if ( -1 == gc.posNeg )       // No metrics
      {
         gc.posNeg  = 0;
         SetMetric( MODE_NOMETRICS );
      }
      else                              // Just metrics
      {
         ErrVMsgLog( "METRICS is not defined, use ALLMETRICS, NOMETRICS, or METRICS=list\n" );
         return(JVMTI_ERROR_INTERNAL);
      }
   }

#if defined(_AIX)

   else if ( helpTest( parm,
                       &gv->interruptCounting,
                       "INTERRUPTCOUNTING",
                       "?Count metrics even during interrupts\n\tDefault is NOINTERRUPTCOUNTING"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->systemPtt,
                       "SYSTEMPTT",
                       "?Use NOSYSTEMPTT or -SYSTEMPTT to avoid using system code for Per Thread Time"
                     ))
   {
      ;
   }
#endif

   else if ( helpMsg( parm, 0, "\nMiscellaneous options:\n" ) )
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "DELAY_START",
                       "?DEPRECATED: Delay data collection until START command from RTDriver\n\tEquivalent to NOSTART.  This is the default."
                     ))
   {
      if ( -1 == gc.posNeg )            // -DELAY_START or NODELAY_START
      {
         gv->start = 1;
      }
      else
      {
         gv->start = 0;                 // DELAY_START is the opposite of START
      }
   }

   else if ( helpTest2( parm,
                        &gv->start,
                        "START",
                        "BEGIN",
                        "Start data collection immediately.  Default is NOSTART"
                      ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->GCInfo,
                       "GCINFO",
                       "Enable Garbage Collection Start/End events"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->ClassLoadInfo,
                       "CLASSLOADINFO",
                       "?Enable Class Load Info"
                     ))
   {
      ;
   }

   else if ( helpTest2( parm,
                        &gv->CompilingStartEnd,
                        "COMPILINGSTARTEND",
                        "COMPILING",
                        "?Enable Compiling Start/End events"
                      ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->CompileLoadInfo,
                       "COMPILELOADINFO",
                       "?Enable Compile Load Info"
                     ))
   {
      ;
   }

   else if ( helpTest2( parm,
                        &gv->HumanReadable,
                        "HR",
                        "HUMANREADABLE",
                        "Make output Human Readable with extra padding"
                      ))
   {
      ;
   }

   else if ( helpTest2( parm,
                        0,
                        "JLM",
                        "JLMSTART",
                        "Start Java Lock Monitoring with timestamps"
                      ))
   {
      gv->jlm = gv_jlm_Time;

      if ( 0 != ge.setVMLockMonitor )
      {
         rc = (*ge.setVMLockMonitor)(ge.jvmti, COM_IBM_JLM_START_TIME_STAMP);

         OptVMsg( "SetVMLockMonitor, rc = %d\n", rc );
      }

      gv->tm_stt = read_cycles_on_processor(0);
   }

   else if ( helpTest2( parm,
                        0,
                        "JLML",
                        "JLMSTARTLITE",
                        "Start Java Lock Monitoring without timestamps"
                      ))
   {
      gv->jlm = gv_jlm_Lite;


      if ( 0 != ge.setVMLockMonitor )
      {
         rc = (*ge.setVMLockMonitor)(ge.jvmti, COM_IBM_JLM_START);

         OptVMsg( "SetVMLockMonitor, rc = %d\n", rc );
      }

      gv->tm_stt = read_cycles_on_processor(0);
   }

   else if ( helpTest( parm,
                       &gv->logRtdCmds,
                       "LOGRTDCMDS",
                       "?Enable/disable logging of RtDriver commands (supported via RtDriver)\n\t+LOGRTDCMDS displays even hidden commands"
                     ))
   {
      if ( gc.posNeg > 0 )
      {
         gv->logRtdCmds = 2;
      }
   }

   else if ( helpTest( parm,
                       &gv->exitNoRc,
                       "EXITNORC",
                       "?Use the MethodExitNoRc event with JVMTI, if possible"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->methInline,
                       "METHODINLINING",
                       "?Set AllowMethodInliningWithMethodEnterExit, if possible\n\tDefault is on"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->directJNI,
                       "DIRECTJNI",
                       "?Set AllowDirectJNIWithMethodEnterExit, if possible\n\tDefault is off"
                     ))
   {
      ;
   }

   else if ( helpTest2( parm,
                        &ge.sun,
                        "SUN",
                        "SUNJVM",
                        "?Treat this JVM as a SUN JVM"
                      ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->CompilerGCInfo,
                       "COMPILERGCINFO",
                       "?Enable/Disable compiler GCInfo events"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->CompiledEntryExit,
                       "COMPILED",
                       "?Enable/Disable compiled entry/exit events"
                     ))
   {
      ;
   }

   else if ( helpTest2( parm,
                        &gv->rtdriver,
                        "RTDRIVER",
                        "SOCKET",
                        "?Enable RTDRIVER support by starting the listener thread\n\tThis is the default"
                      ))
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "LOCKDIS=",
                       "?Disable specified locks"
                     ))
   {
      p1 += 8;
      ge.lockdis = (int)strtol(p1, NULL, 16);
   }

   else if ( helpTest( parm,
                       &gv->exitpop,
                       "EXITPOP",
                       "?Enable calibration category for exit via pop by exception"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->Force,
                       "FORCE",
                       "?Force continuation in spite of errors in special cases"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->deallocFrameBuffer,
                       "DEALLOC_FRAME_BUFFER",
                       "?Dealloc the frame buffer after getting callstacks"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "JITFLAGS=",
                       "?JIT compiling control flags (-JITFLAGS= used only with disable)"
                     ))
   {
      p1 += 9;

      gv->setjflgs = 1;

      if ( -1 == gc.posNeg )
      {
         gv->setjflgs = 2;
      }
      gv->jitflags = (int)strtoul(p1, NULL, 16);
   }

#if defined(_ZOS)
   else if ( helpTest( parm,
                       &ge.asid,
                       "ASID",
                       "?Use ASID instead of PID in file names (ZOS only)"
                     ))
   {
      ;
   }
#endif

   else if (strncmp(parm, "PIDLIST=", 8) == 0)   // FIX ME:  Is this DEAD?
   {
      OptVMsg(" Parsed : PIDLIST=");

      gv->plcnt = parseIntList( p1+8, gv->pl, MAX_PIDLIST );

      if ( gv->plcnt )
      {
         OptVMsg( "%d", gv->pl[0] );

         for ( i = 1; i < gv->plcnt; i++ )
         {
            OptVMsg( ".%d", gv->pl[i] );
         }
      }
      OptVMsg("\n");
   }

   else if ( helpTest2( parm,
                        0,
                        "SCS=",
                        "SAMPLING=",
                        "Enable call stack sampling for EVENT[=N][+], where EVENT is the name\n\t\
of a JVM event and N is the number of occurences before sampling.\n\t\
The default value of N is 1.  Use START to begin immediately.\n\t\
The + displays the class of the object associated with each event\n\t\
and implies NOMETHODTYPES.\n\t\
Events supported are:\n\t\t\
ALLOCATED_BYTES\n\t\t\
ALLOCATIONS[=M:N] (M and N are alloc and byte thresholds)\n\t\t\
MONITOR_WAIT\n\t\t\
MONITOR_WAITED\n\t\t\
MONITOR_CONTENDED_ENTER\n\t\t\
MONITOR_CONTENDED_ENTERED\n\t\t\
CLASS_LOAD"
                      ))
   {
      if ( '=' == *(parm+3) )
      {
         parm += 4;
      }
      else
      {
         parm += 9;
      }
      gv->scs_event_threshold = 1;

      if ( 0 != ( p = strchr( parm, '=' ) ) )
      {
         *p++ = 0;
         gv->scs_event_threshold = atoi(p);
      }

      if ( 1 == gc.posNeg )
      {
         if ( 0 == ge.jvmti )
         {
            ErrVMsgLog( "SCS=<event>+ requires JVMTI\n" );
            return(JVMTI_ERROR_INTERNAL);
         }
         gv->scs_event_object_classes = 1;
         gv->methodTypes              = 0;
      }

      if ( 0 == strcmp( parm, "ALLOCATED_BYTES" ) )
      {
         gv->scs_allocBytes = gv->scs_event_threshold;   // Non-zero guaranteed by check below
      }
      else if ( 0 == strcmp( parm, "ALLOCATIONS" ) )
      {
         if ( gv->scs_event_threshold < 1 )
         {
            ErrVMsgLog( "Event threshold must be greater than zero\n" );
            return(JVMTI_ERROR_INTERNAL);
         }
         gv->scs_allocations     = gv->scs_event_threshold;
         gv->scs_event_threshold = 1;

         if ( p )
         {
            if ( 0 != ( q = strchr( p, ':' ) ) )
            {
               *q++ = 0;
               gv->scs_event_threshold = atoi(q);
            }
            else
            {
               ErrVMsgLog( "SCS=ALLOCATIONS=M:N requires both alloc and byte thresholds\n" );
               return(JVMTI_ERROR_INTERNAL);
            }
         }
         gv->scs_allocBytes  = gv->scs_event_threshold;   // Use ALLOCATED_BYTES path
      }
      else if ( 0 == strcmp( parm, "CLASS_LOAD" ) )
      {
         gv->scs_classLoad  = gv->scs_event_threshold;
         gv->ClassLoadInfo  = 1;        // CLASS_LOAD_UNLOAD
      }
      else if ( 0 == strcmp( parm, "MONITOR_WAIT" ) )
      {
         gv->scs_monitorEvent = JVMTI_EVENT_MONITOR_WAIT;
      }
      else if ( 0 == strcmp( parm, "MONITOR_WAITED" ) )
      {
         gv->scs_monitorEvent = JVMTI_EVENT_MONITOR_WAITED;
      }
      else if ( 0 == strcmp( parm, "MONITOR_CONTENDED_ENTER" ) )
      {
         gv->scs_monitorEvent = JVMTI_EVENT_MONITOR_CONTENDED_ENTER;
      }
      else if ( 0 == strcmp( parm, "MONITOR_CONTENDED_ENTERED" ) )
      {
         gv->scs_monitorEvent = JVMTI_EVENT_MONITOR_CONTENDED_ENTERED;
      }
      else
      {
         ErrVMsgLog( "%s is not a valid sampling event\n", parm );
         return(JVMTI_ERROR_INTERNAL);
      }

      if ( gv->scs_event_threshold < 1 )
      {
         ErrVMsgLog( "Event threshold must be greater than zero\n" );
         return(JVMTI_ERROR_INTERNAL);
      }
      gv->scs_opts.any++;
      gv->NoMetrics = 1;
      gv->trees     = 1;
   }

#if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)

   #ifndef _LEXTERN
   else if ( helpTest2( parm,
                        0,
                        "SCS",
                        "SAMPLING",
                        "Enable sampling of Call Stacks with default options\n\tImplies JITA2N"
                      ))
   {
      ge.scs              = 1;
      gv->prunecallstacks = 0;
      gv->scs_opts.any++;

      gv->JITA2N          = 1;
   }

   else if ( helpTest( parm,
                       0,
                       "SCS_RATE=",
                       "Sampling rate for SCS, default is about 32 samples per second"
                     ))
   {
      gv->scs_opts.rate = 1;
      gv->scs_opts.rate_val = atoi(p1+9);
      gv->scs_opts.any++;
      gv->scs_opts.any_other++;
   }

   else if ( helpTest( parm,
                       0,
                       "SCS_FRAMES=",
                       "Maximum number of SCS frames, default is 2048"
                     ))
   {
      gv->scs_opts.frames = 1;
      gv->scs_opts.frames_val = atoi(p1+11);
      gv->scs_opts.any++;
      gv->scs_opts.any_other++;
   }

      #if defined(_WINDOWS) || defined(_LINUX)

   else if ( helpTest( parm,
                       0,
                       "SCS_A2N",
                       "SCS tries to resolve symbols in native methods, where possible"
                     ))
   {
      gv->scs_opts.a2n = 1;
      gv->scs_opts.any++;
      gv->scs_opts.any_other++;
   }

   else if ( helpTest( parm,
                       &gv->threadListSampling,
                       "THREADLISTSAMPLING",
                       "?Use GetThreadListStackTraces to acquire callstacks"
                     ))
   {
      gv->scs_opts.any++;
      gv->scs_opts.any_other++;

      if ( 0 == ge.getThreadListStackTracesExtended )
      {
         ErrVMsgLog("THREADLISTSAMPLING requires GetThreadListStackTracesExtended\n");
         return(JVMTI_ERROR_INTERNAL);
      }
   }

      #endif

   #endif //_LEXTERN

#endif

   else if ( helpTest( parm,
                       &gv->prunecallstacks,
                       "PRUNECALLSTACKS",
                       "?Prune unreported methods from call stacks\n\tDefault is TRUE, unless SCS is specified"
                     ))
   {
      ;
   }

   else if ( helpTest2( parm,
                        &ge.ExtendedCallStacks,
                        "ECS",
                        "EXTENDEDCALLSTACKS",
                        "?Include Java line numbers and source file names in call stacks (JVMTI only)\n\tEXPERIMENTAL"
                      ))
   {
      ;
   }

#ifndef _LEXTERN
   else if ( helpTest2( parm,
                        &gv->DistinguishLockObjects,
                        "DLO",
                        "DISTINGUISHLOCKOBJECTS",
                        "?Distinguish individual objects in JLM and SCS reports (JVMTI only; requires JVM support)\n\tUsed with JLM and SCS=MONITOR_<event>+\n\tEXPERIMENTAL"
                      ))
   {
      if ( 0 == ge.dumpJlmStats )
      {
         ErrVMsgLog("DISTINGUISHLOCKOBJECTS not supported by this JVM\n");
         return(JVMTI_ERROR_INTERNAL);
      }
   }
#endif //_LEXTERN

   else if ( helpTest( parm,
                       &gv->recomp,
                       "RECOMP",
                       "?Enables recompilation reporting (Not compatible with JINSIGHT)"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->statdyn,
                       "STATDYN",
                       "?Use +STATDYN to display static vs non-static methods\n\tUse -STATDYN to disable static vs non-static method detection"
                     ))
   {
      if ( 1 == gc.posNeg )             // +STATDYN
      {
         gv->statdyn = 2;
      }
   }

   else if ( helpTest( parm,
                       &gv->legend,
                       "LEGEND",
                       "Use -LEGEND to suppress the method type legend in output files"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &ge.newHeader,
                       "NEWHEADER",
                       "?Generate log-rt files with the new header format"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->pnode,
                       "PNODE",
                       "?Report the address of each node in log-rt"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->idpnode,
                       "IDPNODE",
                       "?Report the method or thread ID in the PNODE field of log-rt\n\tImplies PNODE"
                     ))
   {
      gv->pnode = 1;
   }

   else if ( helpTest( parm,
                       &gv->TSnodes,
                       "TSNODES",
                       "Timestamp the creation of call flow tree nodes with raw\n\t(64-bit) timestamps."
                     ))
   {
      ;
   }

#if defined(_AIX) || defined(_X86) || defined(_X64)
   else if ( helpTest( parm,
                       &gv->TSnodes_HR,
                       "TSNODES_HR",
                       "Timestamp the creation of call flow tree nodes with human readable\n\ttimestamps (HH:MM:SS.mSec)."
                     ))
   {
      gv->TSnodes = 1;
   }
#endif

//----------------------------------------------------------------------
// Selection Lists
//----------------------------------------------------------------------

   else if ( helpMsg( parm, 0, "\nSelection Lists:\n\n\
   All lists use '+' or '-' separators for inclusive or exclusive entries.\n\
   A trailing '*' on any selection is a wildcard that turns the entry into\n\
   a prefix.  The default selection is ALL, unless the first entry begins\n\
   with '+' or no leading separator, to allow selection of a single item.\n" ) )
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "THREADS=",
                       "Add entries of the form threadname to the selection list"
                     ))
   {
      nxAddEntries( &gv->selListThreads, p1+8 );
   }

   else if ( helpTest( parm,
                       0,
                       "CLASSMETHODS=",
                       "Add entries of the form classname.methodname to the selection list"
                     ))
   {
      nxAddEntries( &gv->selListMethods, p1+13 );
   }

   else if ( helpTest( parm,
                       0,
                       "OBJECTCLASSES=",
                       "Add entries of the form classname to the selection list"
                     ))
   {
      nxAddEntries( &gv->selListClasses, p1+14 );
   }

//----------------------------------------------------------------------
// Debug Control
//----------------------------------------------------------------------

   else if ( helpMsg( parm, 0, "?\nDebugging options:\n" ) )
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->prototype,
                       "PROTOTYPE",
                       "?Enable prototype features"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->tagging,
                       "TAGGING",
                       "?Use Tagging, if available in the JVM.  Use NOTAGGING to disable"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->g2se,
                       "STDERR",
                       "?Redirect log-msg and log-gen to stderr"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "CHECKNODEDEPTH=",
                       "?Node allocation threshold at which to check node depth"
                     ))
   {
      gv->checkNodeDepth = atoi(p1+15);
   }

   else if ( helpTest( parm,
                       0,
                       "MSGLIMIT=",
                       "?Maximum number of error messages of each kind"
                     ))
   {
      gv->msglimit = atoi(p1+9);
   }

   else if ( helpTest( parm,
                       0,
                       "SHUTDOWN_EVENT=",
                       "?Shutdown the JVM after event M is handled N times (M.N)"
                     ))
   {
      p = strchr( p1+15, '.' );

      if ( p )
      {
         *p++ = 0;

         gv->shutdown_count = atoi(p);
      }
      else
      {
         gv->shutdown_count = 1;
      }
      gv->shutdown_event = atoi(p1+15);
   }

   else if ( helpTest( parm,
                       &gv->JNIGlobalRefInfo,
                       "JNIGLOINFO",
                       "?Add JNI Global Reference events when OUTPUT is selected"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->JNIWeakGlobalRefInfo,
                       "JNIWEAKGLOINFO",
                       "?Add JNI Weak Global Reference events when OUTPUT is selected"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->ArenaInfo,
                       "ARENAINFO",
                       "?Add Arena events when OUTPUT is selected"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->Methods,
                       "METHODENTRYEXIT",
                       "?Enable/disable method entry/exit events"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->InlineEntryExit,
                       "INLINEENTRYEXIT",
                       "?Enable/disable inline method entry/exit events"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->BuiltinEntryExit,
                       "BUILTINENTRYEXIT",
                       "?Enable/disable builtin method entry/exit events"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->NativeEntryExit,
                       "NATIVEENTRYEXIT",
                       "?Enable/disable native method entry/exit events"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->noJvmCallStacks,
                       "NJCS",
                       "?Disable JVM Call Stack handling"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "CHASHB=",
                       "?Sets the number of hash buckets in the class table, default = 64007"
                     ))
   {
      ge.chashb = atoi(p1+7);

      if ( 0 == ge.chashb )
      {
         ErrVMsgLog( "Invalid number of hash buckets\n" );
         return(JVMTI_ERROR_INTERNAL);
      }
   }

   else if ( helpTest( parm,
                       0,
                       "OHASHB=",
                       "?Sets the number of hash buckets in the object table, default = 1000003"
                     ))
   {
      ge.ohashb = atoi(p1+7);

      if ( 0 == ge.ohashb )
      {
         ErrVMsgLog( "Invalid number of hash buckets\n" );
         return(JVMTI_ERROR_INTERNAL);
      }
   }

   else if ( helpTest( parm,
                       0,
                       "MHASHB=",
                       "?Sets the number of hash buckets in the method table, default = 20011"
                     ))
   {
      ge.mhashb = atoi(p1+7);

      if ( 0 == ge.mhashb )
      {
         ErrVMsgLog( "Invalid number of hash buckets\n" );
         return(JVMTI_ERROR_INTERNAL);
      }
   }

#ifndef _LEXTERN
   else if ( helpTest( parm,
                       0,
                       "SHASHB=",
                       "?Sets the number of hash buckets in the per thread StkNode table, default = 17"
                     ))
   {
      ge.shashb = atoi(p1+7);

      if ( 0 == ge.shashb )
      {
         ErrVMsgLog( "Invalid number of hash buckets\n" );
         return(JVMTI_ERROR_INTERNAL);
      }
   }
#endif //_LEXTERN

   else if ( helpTest( parm,
                       0,
                       "THASHB=",
                       "?Sets the number of hash buckets in the thread table, default = 2053"
                     ))
   {
      ge.thashb = atoi(p1+7);

      if ( 0 == ge.thashb )
      {
         ErrVMsgLog( "Invalid number of hash buckets\n" );
         return(JVMTI_ERROR_INTERNAL);
      }
   }

   else if ( helpTest( parm,
                       0,
                       "LOCKDIS=",
                       "?Disables locks: event:1 thread:2 class:4 method:8 object:16"
                     ))
   {
      ge.lockdis = (int)strtol(p1+8, NULL, 16);
   }

   else if ( helpTest( parm,
                       &gv->revtree,
                       "REVTREE",
                       "?Add nodes to the end for time order in log-rt (default)"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       &gv->addbase,
                       "BASE",
                       "?Add to the base values in nodes (default)"
                     ))
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "TBE=",
                       "?Restricts threads to the range of TBE=first.last by thread sequence numbers."
                     ))
   {
      p1 += 4;
      p   = p1;

      while ( *p )
      {
         if ( '.' == *p )
         {
            *p++    = 0;

            gv->tend = atoi(p);

            break;
         }
         p++;
      }
      gv->tbeg = atoi(p1);

      if ( 0 == gv->tend )
      {
         gv->tend = gv->tbeg;
      }
      OptVMsg("gv->tbeg = %d, gv->tend = %d\n", gv->tbeg, gv->tend );

      if ( gv->tbeg > gv->tend )
      {
         ErrVMsgLog( "Invalid values in TBE=\n" );
         return(JVMTI_ERROR_INTERNAL);
      }
   }

#ifndef _LEXTERN
   else if ( helpTest( parm,
                       0,
                       "SCS_VERBOSE",
                       "?Enable verbose output for SCS"
                     ))
   {
      gc.mask |= gc_mask_SCS_Verbose;
   }

   else if ( helpTest( parm,
                       0,
                       "SCS_DEBUG",
                       "?Enable debug output for SCS"
                     ))
   {
      gc.mask |= gc_mask_SCS_Debug;
   }

   #if !defined(_AIX)
   else if ( helpTest( parm,
                       &ge.scs_debug_trace,
                       "SCS_DEBUG_TRACE",
                       "?Enable debug trace output for SCS"
                     ))
   {
      gc.mask |= gc_mask_SCS_Debug;
   }
   #endif
#endif

   else if ( helpTest( parm,
                       &ge.fNeverActive,
                       "NEVERACTIVE",
                       "?Never allow JPROF to process events"
                     ))
   {
   }

   else if ( helpTest( parm,
                       &ge.EnExCs,
                       "ENEXCS",
                       "?Maintain callstacks by entry/exit tracking"
                     ))
   {
      ;
   }

//----------------------------------------------------------------------
// Final Help Processing
//----------------------------------------------------------------------

   else if ( helpMsg( parm, 0, "\nHelp options:\n" ) )
   {
      ;
   }

   else if ( helpTest( parm,
                       0,
                       "HELP",
                       "Display help listing for common options\nALLHELP\n\tDisplay help listing for all options"
                     ))
   {
      if ( 1 == gc.posNeg )             // +HELP, HELP+, or ALLHELP
      {
         gc.mHelp |= gc_mHelp_AllHelp;
      }
      else if ( -1 == gc.posNeg )       // -HELP, HELP-, or NOHELP
      {
         gc.mHelp  = 0;
      }
      else
      {
         gc.mHelp |= gc_mHelp_Help;
      }
   }

   else if ( helpTest( parm,
                       &gv->showVersion,
                       "VERSION",
                       "Display only the version information from the top of the help listing"
                     ))
   {
      ;
   }

   else if ( *parm )
   {
      ErrVMsgLog("\n**ERROR** Unknown JPROF option: \"%s\"\n\n", p1 );
      return JVMTI_ERROR_INTERNAL;
   }
   return( JVMTI_ERROR_NONE );
}
