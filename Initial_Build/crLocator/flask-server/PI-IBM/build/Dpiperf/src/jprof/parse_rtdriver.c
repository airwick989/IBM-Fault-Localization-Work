#include "jprof.h"
#include "tree.h"

//----------------------------------------------------------------------
// Parse one RTDRIVER command with automated help generation
//----------------------------------------------------------------------

int ParseCommand( thread_t * tp,
                  char     * parm,      // Upper case option
                  char     * line )     // Unmodified option
{
   int    rc = JVMTI_ERROR_NONE;
   int    n;
   int    sec;
   char * p;
#ifndef _LEXTERN
   int    hh, mm, ss;
#endif

   if ( helpMsg( parm, 0, "\nRTDriver HELP:\n" ) )
   {
      ;
   }

   //==========================================================
   // Override fnm= from java command line (if any).
   // This will only affect the location of any log-rt,
   // log-jlm and log-hd* file(s) created in the future.
   // All other types of log-* files have already been opened.
   //==========================================================

   else if ( helpTest( parm,
                       0,
                       "FNM=",
                       "?Override the default path and prefix for future log files"
                     ))
   {
      strcpy(ge.fnm, line+4);

      ErrVMsgLog("\n Path and prefix for log files: %s\n", ge.fnm);
   }

   else if ( helpTest( parm,
                       0,
                       "LOGPATH=",
                       "Override the default path for future log files"
                     ))
   {
      if ( line[8] )
      {
         p = ge.logPath;
         ge.logPath = xStrdup("LOGPATH=",line+8);
         xFreeStr( p );

         UpdateFNM();
      }
   }

   else if ( helpTest( parm,
                       0,
                       "LOGPREFIX=",
                       "Override the default prefix for future log files"
                     ))
   {
      if ( line[10] )
      {
         p = ge.logPrefix;
         ge.logPrefix = xStrdup("LOGPREFIX=",line+10);
         xFreeStr( p );

         UpdateFNM();
      }
   }

   else if ( helpTest( parm,
                       0,
                       "LOGSUFFIX=",
                       "Override the default suffix for future log files"
                     ))
   {
      p = ge.logSuffix;
      ge.logSuffix = xStrdup("LOGSUFFIX=",line+10);   // Possible tiny memory leak to simplify code
      xFreeStr( p );
   }

   else if ( helpTest2( parm,
                        0,
                        "FGC",
                        "FORCEGC",
                        "Force Garbage Collection.  You can add GC as a prefix to any command,\n\tto perform the function immediately after the GC"
                      ))
   {
      OptVMsg(" FORCE GC \n");

      if ( ge.onload )
      {
         (*ge.jvmti)->ForceGarbageCollection( ge.jvmti );
      }
   }

   else if ( helpTest2( parm,
                        0,
                        "BEGIN",
                        "START",
                        "Begin profiling"
                      ))
   {

      if (line[5] == '=')
      {
         sec = atoi(&line[6]);
         sec *= KSLEEP;
         sleep(sec);
      }
      if ( gv->calltree || gv->calltrace
           || ( gv->scs_allocBytes
#ifndef _LEXTERN
                | ge.scs
#endif
                | gv->scs_classLoad | gv->scs_monitorEvent ) )
      {
         RTArcfTraceStart(tp,1);
      }

#ifndef _LEXTERN
   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
      if (ge.scs)
      {
         // If notifications aren't already happening for this JVM then
         // get them going now.
         if (!gv->scs_active)
         {
            get_current_time(&hh, &mm, &ss, NULL, NULL, NULL);
            msg_log("**INFO** (%02d:%02d:%02d) resuming driver SCS notifications\n", hh, mm, ss);

            rc = ScsResume();

            if (rc != 0)
            {
               gv->scs_active = 0;
               WriteFlush(gc.msg, "**ERROR** Sampling notify mode failed to resume. rc = %d (%s)\n", rc, RcToString(rc));
            }
            else if ( 0 == ge.fNeverActive )
            {
               gv->scs_active = 1;
            }
         }
         else if ( 0 == ge.fNeverActive )
         {
            gv->scs_active = 1;
         }
      }
   #endif
#endif

      if ( 0 == ge.fNeverActive )
      {
#if defined(_LINUX)
         gv->scs_active = 1;            // FIX ME:  Is this really unconditional?
#else
         if ( gv->scs_allocBytes | gv->scs_classLoad | gv->scs_monitorEvent )
         {
            gv->scs_active = 1;
         }
#endif
      }
   }

   else if ( helpTest( parm,
                       0,
                       "FLUSH",
                       "Write log-rt, but do not reset the data in the trees"
                     ))
   {
      gv->timeStampFlush = tp->timeStamp;
      TreeOut();
   }

   else if ( helpTest( parm,
                       0,
                       "RESET",
                       "Discard all data collected, including the calltrees"
                     ))
   {
      treset( GE_RESET_FULL );
   }

   else if ( helpTest( parm,
                       0,
                       "ZEROBASE",
                       "Discard all data collected, but keep the calltrees"
                     ))
   {
      treset( GE_RESET_ZEROBASE );
   }

   else if ( helpTest2( parm,
                        0,
                        "END",
                        "STOP",
                        "End profiling: Disable events, FLUSH data gathered, then RESET trees"
                      ))
   {
      int  m         = gv->scs_active;
      gv->scs_active = 0;

      n              = gv->rton;
      gv->rton       = 0;

      EnableEvents2x(0);                // Disable tracing events

#ifndef _LEXTERN
   #if defined(_AIX) || defined(_LINUX)
      ge.scs_end_cmd = 1;
      memory_fence();
   #endif

   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
      if (ge.scs)
      {
         int rc;

         get_current_time(&hh, &mm, &ss, NULL, NULL, NULL);
         msg_log("**INFO** (%02d:%02d:%02d) pausing driver SCS notifications\n", hh, mm, ss);

         rc = ScsPause();
         if (rc != 0)
         {
            WriteFlush(gc.msg, "**ERROR** Sampling notify mode failed to pause. rc = %d (%s)\n", rc, RcToString(rc));
         }
      }
   #endif
#endif

      treset( GE_RESET_SYNC );

      gv->timeStampFlush = tp->timeStamp;

      if ( n
           || ( m
                && ( gv->scs_allocBytes
                     || gv->scs_classLoad
                     || gv->scs_monitorEvent ) ) )
      {
         enter_lock(XrtdLock, RTDRIVERLOCK);
         fprintf(gc.msg, " >> RTD_LOCK : flush\n");

         xtreeOut();

         fprintf(gc.msg, " << RTD_LOCK : flush\n");
         leave_lock(XrtdLock, RTDRIVERLOCK);
      }

#ifndef _LEXTERN
   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)

      else if ( m && ge.scs )
      {
         enter_lock(XrtdLock, RTDRIVERLOCK);
         fprintf(gc.msg, " >> RTD_LOCK : flush\n");

         xtreeOut();

         fprintf(gc.msg, " << RTD_LOCK : flush\n");
         leave_lock(XrtdLock, RTDRIVERLOCK);
      }
   #endif
#endif

      treset( GE_RESET_FULL );

#if defined(_AIX) || defined(_LINUX)
      ge.scs_end_cmd = 0;
      memory_fence();
#endif
   }

   // ***********
   // rtdriver | java(RTDriver)
   //   o start stop flush off fgc
   //   o [+- ]base, reset, reset2stack, zerobase, start_wob
   // ***********

   else if ( helpTest( parm,
                       0,
                       "START_WOB",
                       "?Start profiling without base"
                     ))
   {
      if ( gv->calltree || gv->calltrace )
      {
         RTArcfTraceStart(tp,0);
      }
   }

   else if ( helpTest( parm,
                       0,
                       "ASYNC_MODE",
                       "?Respond to RTDRIVER commands upon completion (delayed response)"
                     ))
   {
      ge.immediate_response = 0;
   }

   else if ( helpTest( parm,
                       0,
                       "SYNC_MODE",
                       "?Respond to RTDRIVER commands upon acceptance (immediate response)"
                     ))
   {
      ge.immediate_response = 1;
   }

   // base +base -base
   else if (strcmp("-BASE", parm) == 0)
   {
      gv->addbase = 0;
   }
   else if (strcmp("BASE", parm) == 0)
   {
      gv->addbase = 1;
   }
   else if (strcmp("+BASE", parm) == 0)
   {
      gv->addbase = 1;
   }

   else if (strcmp("FLUSHAFTERGC", parm) == 0 )   // Experimental
   {
      gv->flushAfterGC = 1;
   }

   else if (strcmp("NOFLUSHAFTERGC", parm) == 0 )   // Experimental
   {
      gv->flushAfterGC = 0;
   }

   // ***********
   // JPROF ESCAPE RTRIVER COMMANDS
   // ***********

   else if ( helpTest( parm,
                       0,
                       "VERBOSE",
                       "Toggle the VERBOSE option"
                     ))
   {
      gc.verbose ^= gc_verbose_stderr;
   }
   else if ( helpTest( parm,
                       0,
                       "VERBMSG",
                       "Toggle the VERBMSG option"
                     ))
   {
      gc.verbose ^= gc_verbose_logmsg;
   }
   else if ( helpTest( parm,
                       0,
                       "MASK=",
                       "?Override the current debug output mask"
                     ))
   {
      gc.mask     = strtoul(line+5, NULL, 16);

      if ( gc.mask )
      {
         gc.verbose |= gc_verbose_logmsg;
      }
      else
      {
         gc.verbose &= ~gc_verbose_logmsg;
      }
   }

#ifdef AMBIVALENT
   else if (strncmp("RAISE=", parm, 6) == 0)
   {
      gv->raise = atoi(line + 6);

      OptVMsg(" bef RAISE(%d)\n", gv->raise);

      raise(gv->raise);

      OptVMsg(" aft RAISE\n");
   }
#endif

   else if ( helpTest( parm,
                       0,
                       "SLEEP=",
                       "Sleep for nn seconds before executing the next command on this line"
                     ))
   {
      sec = atoi(line+6);
      sec *= KSLEEP;
      sleep(sec);
   }


   // ************
   // JLM Commands
   // jlmstartlite jlmstart jlmdump jlmreset jlmstop
   // ************

   else if ( helpTest2( parm,
                        0,
                        "JLM",
                        "JLMSTART",
                        "Start JLM data collection WITH hold time accounting."
                      ))
   {
      if ( ge.onload )
      {
         gv->jlm = gv_jlm_Time;

         if ( 0 != ge.setVMLockMonitor )   // If JVMTI and ...
         {
            (ge.setVMLockMonitor)(ge.jvmti, COM_IBM_JLM_START_TIME_STAMP);
         }

         gv->tm_stt = read_cycles_on_processor(0);
      }
   }

   else if ( helpTest2( parm,
                        0,
                        "JLML",
                        "JLMSTARTLITE",
                        "Start JLM data collection WITHOUT hold time accounting."
                      ))
   {
      if ( ge.onload )
      {
         gv->jlm = gv_jlm_Lite;

         if ( 0 != ge.setVMLockMonitor )
         {
            (ge.setVMLockMonitor)(ge.jvmti, COM_IBM_JLM_START);
         }

         gv->tm_stt = read_cycles_on_processor(0);
      }
   }

   else if ( helpTest2( parm,
                        0,
                        "JD",
                        "JLMDUMP",
                        "Request a JLM dump"
                      ))
   {
      OptVMsgLog(" bef Req JLM_DUMP\n");

      if ( ge.onload )
      {
         if (line[7] == '=' && strlen( line+8 ) < MAX_PATH )
         {
            strcpy(gv->tempfn, line+8); // One-time Override of filename
         }

         tiJLMdump();                // Perform JLM dump using JVMTI
      }

      OptVMsgLog(" aft Req JLM_DUMP\n");
   }

   else if ( helpTest2( parm,
                        0,
                        "JRESET",
                        "JLMRESET",
                        "Reset the data collected by JLM"
                      ))
   {
      if ( ge.onload )
      {
         if ( 0 != ge.setVMLockMonitor )   // JVMTI and ...
         {
            if ( gv->jlm != gv_jlm_Off )
            {
               (ge.setVMLockMonitor)(ge.jvmti,
                                     ( gv->jlm == gv_jlm_Lite )
                                     ? COM_IBM_JLM_STOP
                                     : COM_IBM_JLM_STOP_TIME_STAMP );
            }

            if ( gv->jlm != gv_jlm_Lite )
            {
               gv->jlm = gv_jlm_Time;
            }

            (ge.setVMLockMonitor)(ge.jvmti,
                                  ( gv->jlm == gv_jlm_Lite )
                                  ? COM_IBM_JLM_START
                                  : COM_IBM_JLM_START_TIME_STAMP );
         }
         gv->tm_stt = read_cycles_on_processor(0);
      }
   }

   else if ( helpTest2( parm,
                        0,
                        "JSTOP",
                        "JLMSTOP",
                        "Turn off JLM"
                      ))
   {
      if ( ge.onload )
      {
         if ( 0 != ge.setVMLockMonitor )   // JVMTI and ..
         {
            if ( gv->jlm != gv_jlm_Off )
            {
               (ge.setVMLockMonitor)(ge.jvmti,
                                     ( gv->jlm == gv_jlm_Lite )
                                     ? COM_IBM_JLM_STOP
                                     : COM_IBM_JLM_STOP_TIME_STAMP );
            }
         }
         gv->jlm = gv_jlm_Off;
      }
   }

#if defined(_WINDOWS) || defined(_LINUX)

   else if ( helpTest( parm,
                       0,
                       "ITRACE_ON",
                       "Start instruction trace"
                     ))
   {
      ITraceEnableCurrent();
   }

   else if ( helpTest( parm,
                       0,
                       "ITRACE_OFF",
                       "Stop instruction trace"
                     ))
   {
      ITraceOff();
   }
#endif

   else if ( 0 == strcmp( "LOGRTDCMDS", parm ) )
   {
      gv->logRtdCmds = 1;
      OptVMsgLog("Reporting of RTDriver commands has been enabled.\n");
   }

   else if ( 0 == strcmp( "+LOGRTDCMDS", parm ) )
   {
      gv->logRtdCmds = 2;
      OptVMsgLog("Reporting of all RTDriver commands has been enabled.\n");
   }

   else if ( 0 == strcmp( "NOLOGRTDCMDS", parm )
             || 0 == strcmp( "-LOGRTDCMDS", parm ) )
   {
      gv->logRtdCmds = 0;
      OptVMsgLog("Reporting of RTDriver commands has been disabled.\n");
   }

   else if ( strncmp(    "FUNCTIONENTRY=", parm, 14 ) == 0
             || strncmp( "FUNCTIONEXIT=",  parm, 13 ) == 0 )
   {
      char     * s;
      method_t * mp;
      pNode      pn;

      // FIX ME: Does this need to handle RESET commands?

      if ( gv->rton )
      {
         if ( '=' == line[12] )
         {
            s       = line + 13;
            tp->x23 = gv_ee_Exit;
         }
         else
         {
            s       = line + 14;
            tp->x23 = gv_ee_Entry;
         }

         // unlike reh_method

// Generate a hash value for this name string to use in place of mid

         mp = insert_method( 0, s );

// To handle the problem of xrtdriver in the tree as the parent of the
// new dummy node, a duplicate copy of its node is pushed after the dummy node.
// This allows xrtdriver to return to the new dummy node.  On exit, the
// duplicate node must be exited before exiting the dummy node, allowing the
// actual return from xrtdriver to exit properly.

// FIX ME:  Time must be adjusted within this sequence to avoid dupicate accounting

         pn = (pNode)tp->currm;      // Duplicate node to Push for Entry

         if ( gv_ee_Entry == tp->x23 )
         {
            tp->mt3    = gv_type_Other;
            tp->flags &= ~thread_mtypedone;   // Indicate mtype not yet done
            tp->mp     = mp;

            RTEnEx( tp );            // Push the dummy node

            if (gv->calltrace)
            {
               genPrint1( tp );
            }

            tp->mt3    = pn->ntype;
            tp->flags &= ~thread_mtypedone;   // Indicate mtype not yet done
            tp->mp     = pn->id;

            RTEnEx( tp );            // Push the duplicate node

            if (gv->calltrace)
            {
               genPrint1( tp );
            }
         }
         else
         {
            tp->mt3    = pn->ntype;
            tp->flags &= ~thread_mtypedone;   // Indicate mtype not yet done
            tp->mp     = pn->id;

            RTEnEx( tp );            // Pop the duplicate node

            if (gv->calltrace)
            {
               genPrint1( tp );
            }

            tp->mt3    = gv_type_Other;
            tp->flags &= ~thread_mtypedone;   // Indicate mtype not yet done
            tp->mp     = mp;

            RTEnEx( tp );            // Pop the dummy node

            if (gv->calltrace)
            {
               genPrint1( tp );
            }
         }
      }
   }

   else if ( helpTest( parm,
                       0,
                       "JPROFINFO",
                       "Display JPROF version information"
                     ))
   {
      writeStderr(gv->version);
      writeStderr(gv->loadtime);
      writeStderr(" JPROF Options: ");
      writeStderr(gv->options);
      writeStderr("\n\n");
   }

   else if ( helpTest( parm,
                       0,
                       "HELP",
                       "Display help for common RTDriver commands"
                     ))
   {
      gc.mHelp = gc_mHelp_Help;
      parm[0]  = 0;

      ParseCommand( tp, parm, line );
      gc.mHelp = 0;
   }

   else if ( helpTest( parm,
                       0,
                       "ALLHELP",
                       "Display help for all RTDriver commands"
                     ))
   {
      gc.mHelp = gc_mHelp_AllHelp;
      parm[0]  = 0;

      ParseCommand( tp, parm, line );
      gc.mHelp = 0;
   }

   else if ( helpTest( parm,
                       0,
                       "JPROFHELP",
                       "Display help for common JPROF options"
                     ))
   {
      gc.mHelp = gc_mHelp_Help;
      parm[0]  = 0;

      ParseOption( parm, line );
      gc.mHelp = 0;
   }

   else if ( helpTest( parm,
                       0,
                       "ALLJPROFHELP",
                       "Display help for all JPROF options"
                     ))
   {
      gc.mHelp = gc_mHelp_AllHelp;
      parm[0]  = 0;

      ParseOption( parm, line );
      gc.mHelp = 0;
   }

   else if ( helpTest( parm,
                       0,
                       "SHUTDOWN",
                       "END profiling, then terminate the JVM"
                     ))
   {
      StopJProf( sjp_RTDRIVER );
   }

   else if ( helpMsg( parm, 0, "EXIT_RTDRIVER\n\tExit RTDRIVER without terminating the JVM" ) )
   {
      ;
   }

   else if ( strlen(parm) > 0 )
   {
      ErrVMsgLog(" UNSUPPORTED RTDRIVER COMMAND: '%s'\n", line);
      ErrVMsgLog(" Use HELP or ALLHELP to display valid commands\n");
      rc = JVMTI_ERROR_INTERNAL;
   }

   return(rc);
}
