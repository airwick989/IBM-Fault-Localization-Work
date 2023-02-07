#include "jprof.h"
#include "hash.h"
#include "utils/pi_time.h"

extern char strPuWorkerThread[];
extern char strRtdriverListener[];
extern char strJavaLangThread[];
extern char strInit[];
extern char strSetName[];
extern char strV[];
extern char sigSetName[];

/***************************/
jthread create_java_thread( jvmtiEnv        * jvmti,
                            JNIEnv          * env,
                            PFN_JAVA_THREAD   thread,   // Thread to create and start
                            void            * thread_arg,   // Thread's argument
                            jint              priority,   // Thread priority
                            char            * name )   // NULL for unnamed thread
{
   jclass           cls;
   jthread          obj;
   jmethodID        mid;
   jstring          str;
   jvmtiError       rc;

   cls = (*env)->FindClass(env, strJavaLangThread);

   if ( NULL == cls )
   {
      return(NULL);
   }

   mid = (*env)->GetMethodID(env, cls, strInit, strV);
   if ( NULL == mid )
   {
      return(NULL);
   }
   obj = (*env)->NewObject(env, cls, mid);

   if ( NULL == obj )
   {
      return(NULL);
   }

   if ( name )
   {
      mid = (*env)->GetMethodID(env, cls, strSetName, sigSetName);

      if (mid)
      {
         str = (*env)->NewStringUTF(env, name);
         (*env)->CallVoidMethod(env, obj, mid, str);
      }

      if ( (*env)->ExceptionOccurred(env) )
      {
         (*env)->ExceptionDescribe(env);
         (*env)->ExceptionClear(env);
         ErrVMsgLog("create_java_thread: could not set thread name %s\n", name);
      }
   }

   rc = (*jvmti)->RunAgentThread(jvmti, obj, thread, thread_arg, priority);

   if (rc != JVMTI_ERROR_NONE)
   {
      return(NULL);
   }

   return(obj);
}

char * getThreadName( thread_t * tp )
{
   jvmtiThreadInfo  ti;
   jvmtiError       rc;
   char           * tname = 0;
   char           * p;

   if ( ge.jvmti )
   {
      rc = (*ge.jvmti)->GetThreadInfo( ge.jvmti,
                                       tp->thread,
                                       &ti );

      OptVMsg("GetThreadInfo(%p), rc = %d\n", tp->thread, rc);

      if (rc == JVMTI_ERROR_NONE)
      {
         Java2Native( ti.name );

         tname = xStrdup( "ThreadName", ti.name );

         p = tname;

         while ( 0 != *p )
         {                              // space or " => _
            if ( ' ' == *p )
            {
               *p = '_';
            }
            p++;
         }

         if ( gc.verbose )
         {
            msg("  thrdname %s\n", tname);
            msg("  priority %d\n", ti.priority);
            msg("  isdaemon %d\n", ti.is_daemon);
            msg("  thrd_grp %p\n", ti.thread_group);
            msg("  cntxCLdr %p\n", ti.context_class_loader);
         }

         if ( ti.is_daemon
              || 0 == strcmp( tname, strRtdriverListener  ) )
         {
            tp->num_frames       = 0;
            tp->ignoreCallStacks = 1;
         }

         (*ge.jvmti)->Deallocate( ge.jvmti, (unsigned char *)ti.name);

         tp = insert_thread( CurrentThreadID() );

         if ( tp->env_id )
         {
            (*tp->env_id)->DeleteLocalRef( tp->env_id, ti.thread_group );
            (*tp->env_id)->DeleteLocalRef( tp->env_id, ti.context_class_loader );
         }
      }
   }
   return( tname );
}

/***************************/
// Common routine for cbThreadStart and cbVMInit

void doThreadStart( jvmtiEnv * jvmti,
                    JNIEnv   * env,
                    jthread    thread,
                    jlong      tid )
{
   thread_t       * tp = NULL;
   char           * tname;
   jvmtiError       rc;
   int              metric;

   tp = readTimeStamp((int)tid);        // FIX ME, thread id should be 64 bits

   if ( tp )
   {
      tp->thread = (*env)->NewWeakGlobalRef(env, thread);

      tname = getThreadName( tp );

      if ( tname )
      {
         setThreadName(tp, tname);
      }

      /*
      if ( ge.scs )
      {
         int rc = ScsInitThread(gv->scs_active, tp->tid);
         if (rc == PU_SUCCESS)
            tp->scs_enabled = 1;
      }
      */

      if ( gv->gatherMetrics && gv->physmets && tp->ptt_enabled == 0)
      {
         int rc = PttInitThread(tp->tid);
         if (rc == PU_SUCCESS)
            tp->ptt_enabled = 1;
      }

      if ( ( tp->flags & thread_selected )   // Implies gv->selListThreads
           && ( gv->calltree || gv->calltrace || ge.EnExCs ) )
      {
         if ( ge.methodExitNoRc
              && ge.setExtendedEventNotificationMode )
         {
            rc = (ge.setExtendedEventNotificationMode)( jvmti,
                                                        JVMTI_ENABLE,
                                                        ge.methodExitNoRc,
                                                        thread );

            OptVMsgLog( "SetExtendedEventNotificationMode, rc = %d\n", rc );
         }
         else
         {
            SetTIEventMode( jvmti,
                            JVMTI_ENABLE,
                            JVMTI_EVENT_METHOD_EXIT,
                            thread );
         }

         if ( ge.methodEntryExtended
              && ge.setExtendedEventNotificationMode )
         {
            rc = (ge.setExtendedEventNotificationMode)( jvmti,
                                                        JVMTI_ENABLE,
                                                        ge.methodEntryExtended,
                                                        thread );

            OptVMsgLog( "SetExtendedEventNotificationMode, rc = %d\n", rc );
         }
         else
         {
            SetTIEventMode( jvmti,
                            JVMTI_ENABLE,
                            JVMTI_EVENT_METHOD_ENTRY,
                            thread );
         }
      }
   }
}


/******************************/
int writeTnmHook(UINT64 ctime, int mode, int tid, char * tnm)
{
   int rc           = -1;
   static int trcid = 0;

#if defined(_X86) || defined(_X86_64)
   {
   #ifdef HAVE_TraceGetIntervalId
      {
         int ntrcid = TraceGetIntervalId();

         if (ntrcid != trcid)
         {
            trcid = ntrcid;
            fprintf(gd.jtnm, " SeqNo %d\n", ntrcid);
         }
      }
   #endif

    // All data in hook => file not needed after init sync
    // TODO: Change num parameters in TraceHook
    // TODO: set the major codes are enums
      // rc = TraceHook(0x19, 0x42, 6, 1,
      //               trcid, ge.pid, tlo, thi, tid, mode, (int)strlen(tnm), tnm);
    rc = -1;
      if (rc != 0)
      {
         static once = 0;

         if (once == 0)
         {
            once = 1;
            OptVMsg(" Can't write thread name hook (0x19/0x42). rc = %d\n", rc);
         }
      }
   }
#endif
   return(rc);
}

/******************************/
void setThreadName(thread_t * tp, char * tstr)
{
   char   tnm[MAX_BUFFER_SIZE];
   char * p;

   sprintf( tnm, "%08x_%s", PtrToUint32(tp->tid), tstr );

   // put '_' in place of Space or DoubleQuote

   p = tnm;

   while (*p != '\0')
   {                                    // space or " => _
      if ((*p == ' ') || (*p == '"'))
      {
         *p = '_';
      }
      p++;
   }

   p               = tp->thread_name;
   tp->thread_name = xStrdup("thread_name",tnm);
   xFreeStr( p );

   if (gd.jtnm)
   {
      UINT64 ctime;

      ctime = get_timestamp();

#if defined(_ZOS)
      ctime = ctime >> 13;              // For compatibility with application Tprof
#endif

      p = tp->thread_name + 9;          // Skip over 00000000_ from tid

      writeTnmHook(ctime, 0, PtrToUint32(tp->tid), p);

      WriteFlush(gd.jtnm, "TNM: %16llx %8x %s START\n",
                 ctime, tp->tid, p);
      OVMsg(("TNM: %16llx %8x %s START\n",
             ctime, tp->tid, p));
   }

   if (gd.gen)
   {
      fprintf(gd.gen, " 0 pidtid %s\n", tp->thread_name);
   }

   if ( gv->selListThreads )
   {
      p = tnm;

      while ( '_' != *p++ )             // Skip thread id
      {
         ;
      }

      if ( nxCheckString( gv->selListThreads, p ) )
      {
         OptVMsgLog("Thread selected: %s\n", tnm);

         tp->flags |= thread_selected;
      }

      tp->flags |= thread_filtered;
   }
}

/******************************/
void endThreadName(thread_t * tp)
{
   UINT64 xtime;
   char * p;
   int    i;

   if (gd.jtnm)
   {
      xtime = get_timestamp();

#if defined(_ZOS)
      xtime = xtime >> 13;              // For compatibility with application Tprof
#endif

      p = tp->thread_name;

      i = ( p[8] == '_' ) ? 9 : 0;

      writeTnmHook(xtime, 1, PtrToUint32(tp->tid), p + i);

      WriteFlush(gd.jtnm, "TNM: %16llx %8x %s END\n",
                 xtime, tp->tid, p + i);

      OVMsg(("TNM: %16llx %8x %s END\n",
           xtime, tp->tid, p + i));
   }

   tp->num_frames       = 0;
   tp->fValid           = 0;
   tp->ignoreCallStacks = 1;            // Eliminate from idle sampling

   if ( ge.jvmti )
   {
      if ( tp->thread )
      {
         jthread    thread;
         thread_t * self_tp;

         self_tp = insert_thread( CurrentThreadID() );

         if ( self_tp->env_id )
         {
            thread     = tp->thread;
            tp->thread = 0;

            for (;;)
            {
               if ( 0 == tp->fSampling )
               {
                  (*self_tp->env_id)->DeleteWeakGlobalRef(self_tp->env_id, thread);

                  break;
               }
               sleep(30);
            }
         }
      }
   }

   freeFrameBuffer(tp);
}


static void JNICALL PuWorkerThreadWrapper(jvmtiEnv * jvmti,
                                          JNIEnv   * env,
                                          void     * x)
{
   return perfutil_worker((void *)env);
}

int start_worker_thread(pu_worker_thread_args *args)
{
   int rc = 0;
   jthread new_thread = NULL;
   jprof_worker_thread_extra_args *extraArgs = args->extraArgs;


   if (extraArgs && extraArgs->jvmti) {
      new_thread = create_java_thread(extraArgs->jvmti,
                                      extraArgs->env,
                                      PuWorkerThreadWrapper,
                                      NULL,
                                      JVMTI_THREAD_MAX_PRIORITY,
                                      strPuWorkerThread);
      rc = (new_thread == NULL) ? JNI_ERR : 0;
      *(args->thread_handle) = (void *)new_thread;
   }
   else {
      err_exit("**ERROR** Not jvmti.");
   }

   return rc;
}
