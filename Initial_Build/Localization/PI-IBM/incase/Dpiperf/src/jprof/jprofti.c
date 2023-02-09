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
*   the GNU Lesser General Public License formore details.
*
*   You should have received a copy of the GNU Lesser General Public License
*   along with this library; ifnot, write to the Free Software
*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// STJ 20051102 Completed OBJECTINFO support
// STJ 20051130 Completed Heap Dump support
// STJ 20060202 Completed JVMTI support

//#define JVMTI_FUNC_PTR(env,f) (*((*(env))->f))
//#define JNI_FUNC_PTR(env,f)   (*((*(env))->f))

#if defined(_ZOS)
   #pragma export(Agent_OnLoad)
#endif

#include "jprof.h"
#include "hash.h"
#include "tree.h"
#include "utils/pi_time.h"

#include "plugin/JprofPluginInterface.h"

#if defined(_AIX)
   #include <sys/time.h>
#endif

void XJNICALL RTWaitForClient(void * x);
int start_worker_thread(pu_worker_thread_args *args);

// Strings which may need to be converted to EBCDIC (rounded to 4-byte boundaries)

char strJavaLangClass[16]     = "java/lang/Class";
char strJavaLangThread[20]    = "java/lang/Thread";
char strSetName[8]            = "setName";
char sigSetName[24]           = "(Ljava/lang/String;)V";
char strRtdriverListener[24]  = "RtdriverListenerThread";
char strHCListener[20]        = "HealthCenterThread";
char strLJavaLangClass[20]    = "Ljava/lang/Class;";
char strTYPE[8]               = "TYPE";
char strInit[8]               = "<init>";
char strV[4]                  = "()V";
char strJavaVmName[16]        = "java.vm.name";
char strJavaVmVersion[16]     = "java.vm.version";
char strJavaVmInfo[16]        = "java.vm.info";
char strJavaVmVendor[16]      = "java.vm.vendor";
char strComma[4]              = ",";
char strPuWorkerThread[20]    = "PuWorkerThread";

char reflectClassNames[9][20] =
{
   "java/lang/Byte",
   "java/lang/Boolean",
   "java/lang/Short",
   "java/lang/Character",
   "java/lang/Integer",
   "java/lang/Float",
   "java/lang/Long",
   "java/lang/Double",
   "java/lang/Void"
};

/***Globals*******************/
int db = 1;

char rtlab[8][16] =
{
   "",
   "JniGRef",
   "SysClass",
   "Monitor",
   "StkLocal",
   "JniLRef",
   "Thrd",
   "Other"
};

char reflab[12][16] =
{
   "-0-",
   "Ref_Cl",
   "Ref_Fld",
   "Ref_ArrEl",
   "Ref_ClLdr",
   "Ref_Signers",
   "Ref_ProDom",
   "Ref_InFa",
   "Ref_StFld",
   "Ref_KPool",
   "-10-",
   "-11-"
};

/***endGlobals****************/

// *********************
// JVMTI CODE START
// *********************

// jvmti - unique call per Event

typedef unsigned   TableIndex;
typedef TableIndex ObjectIndex;
typedef TableIndex SiteIndex;

jvmtiError setExtensionEventCallback( int fEnable, jint id, jvmtiExtensionEvent cb )
{
   jvmtiError rc;

   rc = (*ge.jvmti)->SetExtensionEventCallback( ge.jvmti,
                                                id,
                                                fEnable
                                                ? cb
                                                : (jvmtiExtensionEvent)NULL );

   OptVMsgLog( "SetExtensionEventCallback: rc = %d, %s %d:%s\n",
               rc,
               DisableEnableRequest[fEnable],
               id,
               ge.evstr[id] );

   return( rc );
}

/***************************/
// single ClassPrepare Event - End of Class Loading
void JNICALL
cbClassPrepare( jvmtiEnv * jvmti,
                JNIEnv   * env,
                jthread    thread,
                jclass     klass)
{
   thread_t * tp;

   tp = tie_prolog( 0, JVMTI_EVENT_CLASS_PREPARE, "cbClassPrepare");
   if (tp == NULL) return;

   getClassEntry(tp, klass);

   timeJprofExit(tp);
}

#ifndef _LEXTERN
/***************************/
// single ArrayClassLoad Event - Start of Class Loading
void JNICALL
cbArrayClassLoad( jvmtiEnv * jvmti,
                  JNIEnv   * env,
                  jthread    thread,
                  jclass     klass,
                  ...)
{
   thread_t * tp;

   tp = tie_prolog( 0, ge.arrayClassLoad, "cbArrayClassLoad");
   if (tp == NULL) return;

   getClassEntry(tp, klass);

   timeJprofExit(tp);
}
#endif

/***************************/
// single ClassLoad Event - Start of Class Loading
void JNICALL
cbClassLoad( jvmtiEnv * jvmti,
             JNIEnv   * env,
             jthread    thread,
             jclass     klass)
{
   thread_t * tp;

   tp = tie_prolog( 0, JVMTI_EVENT_CLASS_LOAD, "cbClassLoad");
   if (tp == NULL) return;

   getClassEntry(tp, klass);

   if ( gv->scs_classLoad )
   {
      checkEventThreshold( tp, (jobject)klass, klass, 1 );

      tp->fWorking = 0;
      memory_fence();
      return;
   }

   timeJprofExit(tp);
}

/***************************/
// Event
void JNICALL
cbDynamicCodeGenerated( jvmtiEnv   * jvmti,
                        const char * name,
                        const void * address,
                        jint         length )
{
   char * mnm;
   char * cp;
   char c;

   thread_t * tp;

   tp = tie_prolog( 0, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, "cbDynamicCodeGenerated");
   if (tp == NULL) return;

   OVMsg(( " ==> cbDynamicCodeGenerated: name = %p, addr = %p, size = %x\n",
           name,
           address,
           length ));

   mnm = dupJavaStr((char *)name);

   // fill blanks
   cp = mnm;
   while ((c = *cp) != 0)
   {
      if (c == ' ')
      {
         *cp = '_';
      }
      cp++;
   }

   // merge TI with PI into log-jita2n
   jmLoadCommon( (jmethodID)0,
                 (char *)address,
                 length,
                 2,                     // MMI:2 noC:0
                 "",                    // class with path
                 "",                    // method
                 "",                    // signature
                 mnm,                   // MMI name
                 "");                   // period between Class & Method

   OVMsg(( " <== cbDynamicCodeGenerated\n" ));

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbCompiledMethodLoad( jvmtiEnv   * jvmti,
                      jmethodID    mid,
                      jint         code_size,
                      const void * code_addr,
                      jint         map_length,
                      const jvmtiAddrLocationMap * map,
                      const void * compile_info)
{
   thread_t * tp;
   jvmtiError rc;

   jclass     dclass;                   // Class which declared this method
   char     * mname;                    // Method Name
   char     * sig;                      // Signature
   class_t  * cp;

   tp = tie_prolog( 0, JVMTI_EVENT_COMPILED_METHOD_LOAD, "cbCompiledMethodLoad");

   if (tp == NULL) return;

   dclass = NULL;                       // Class which declared this method
   mname  = "unknown_method";           // Method Name
   sig    = "()V";                      // Signature

   rc = (*jvmti)->GetMethodName(jvmti, mid, &mname, &sig, 0);

   if ( rc == JVMTI_ERROR_NONE )
   {
#if defined(_CHAR_EBCDIC)
      Java2Native( mname );             // Translate method name to EBCDIC
      Java2Native( sig );               // Translate signature   to EBCDIC
#endif

      rc = (*jvmti)->GetMethodDeclaringClass(jvmti, mid, &dclass);

      if ( rc != JVMTI_ERROR_NONE )
      {
         ErrVMsgLog("ERROR: GetMethodDeclaringClass: mid = %x, rc = %d\n", mid, rc );
      }
      else
      {
         char * endAddr = (char *)code_addr + code_size;

         cp = getClassEntry( tp, dclass );

         if ( tp->env_id )
         {
            (*tp->env_id)->DeleteLocalRef(tp->env_id, dclass);
         }
         if ( (char *)code_addr < ge.minJitCodeAddr )
         {
            ge.minJitCodeAddr = (char *)code_addr;
         }
         if ( endAddr > ge.maxJitCodeAddr )
         {
            ge.maxJitCodeAddr = endAddr;
         }

         // merge TI with PI
         jmLoadCommon( mid,
                       (char *)code_addr,
                       code_size,
                       0,               // code not included
                       cp->class_name,
                       mname,
                       sig,
                       "",
                       ".");
      }

      (*jvmti)->Deallocate(jvmti, (unsigned char *)mname);
      (*jvmti)->Deallocate(jvmti, (unsigned char *)sig);
   }
   else
   {
      ErrVMsgLog("ERROR: GetMethodName: mid = %x, rc = %d\n", mid, rc );
   }
   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbCompiledMethodUnload( jvmtiEnv   * jvmti,
                        jmethodID    mid,
                        const void * code_addr )
{
   thread_t * tp;

   tp = tie_prolog( 0, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, "cbCompiledMethodUnload");
   if (tp == NULL) return;

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbGarbageCollectionStart( jvmtiEnv * jvmti )
{
   thread_t  * tp;

   tp = tie_prolog( 0, JVMTI_EVENT_GARBAGE_COLLECTION_START, "cbGarbageCollectionStart");

   ge.gc_running = 1;
   ge.numberGCs++;

   if ( tp )
   {
      timeJprofExit(tp);
   }
}

/***************************/
void JNICALL
cbGarbageCollectionFinish( jvmtiEnv * jvmti )
{
   thread_t  * tp;

   tp = tie_prolog( 0, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, "cbGarbageCollectionFinish");

   ge.gc_running = 0;

   if ( tp )
   {
      GCInfo(tp, 0, 0, 0);

      if ( gv->flushAfterGC )
      {
         gv->timeStampFlush = tp->timeStamp;
         TreeOut();
      }

      timeJprofExit(tp);
   }
   else
   {
      tp  = lookup_thread( CurrentThreadID() );

      if ( tp )
      {
         GCInfo(tp, 0, 0, 0);
      }
   }
}

/***************************/
void JNICALL
cbVMObjectAlloc( jvmtiEnv * jvmti,
                 JNIEnv   * env,
                 jthread    thread,
                 jobject    object,
                 jclass     objclass,
                 jlong      size)
{
   thread_t  * tp;

   tp = tie_prolog(env, JVMTI_EVENT_VM_OBJECT_ALLOC, "cbVMObjectAlloc");
   if (tp == NULL) return;

   if ( (UINT32)size < gv->minAlloc )
   {
      tp->fWorking  = 0;
      memory_fence();
      return;                           // Ignore allocations below the specified threshold
   }

   if ( gv->scs_allocBytes )
   {
      if ( ge.JVMversion != JVM_J9 )
      {
         size = (size + 12 + 7) & 0xfffffff8;   // use round up for SUN/SOV
      }
      checkEventThreshold( tp, object, objclass, (UINT32)size );

      tp->fWorking = 0;
      memory_fence();
      return;
   }

   doObjectAlloc( tp, jvmti, object, objclass, size );

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbInstrumentableObjectAlloc( jvmtiEnv * jvmti,
                             JNIEnv   * env,
                             jthread    thread,
                             jobject    object,
                             jclass     objclass,
                             jlong      size,
                             ... )
{
   thread_t * tp;

   tp = tie_prolog( 0, ge.instrumentableObjectAlloc, "cbInstrumentableObjectAlloc");
   if (tp == NULL) return;

   if ( (UINT32)size < gv->minAlloc )
   {
      tp->fWorking  = 0;
      memory_fence();
      return;                           // Ignore allocations below the specified threshold
   }

   if ( gv->scs_allocBytes )
   {
      if ( ge.JVMversion != JVM_J9 )
      {
         size = (size + 12 + 7) & 0xfffffff8;   // use round up for SUN/SOV
      }
      checkEventThreshold( tp, object, objclass, (UINT32)size );

      tp->fWorking = 0;
      memory_fence();
      return;
   }

   doObjectAlloc( tp, jvmti, object, objclass, size );

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbObjectFree( jvmtiEnv * jvmti,
              jlong      tag)
{
   thread_t  * tp;

   tp = tie_prolog( 0, JVMTI_EVENT_OBJECT_FREE, "cbObjectFree");
   if (tp == NULL) return;

   doObjectFree( tp, tag );

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbMonitorWait( jvmtiEnv * jvmti,
               JNIEnv   * env,
               jthread    thread,
               jobject    object,
               jlong      timeout )
{
   thread_t  * tp;

   tp = tie_prolog(env, JVMTI_EVENT_MONITOR_WAIT, "cbMonitorWait");
   if (tp == NULL) return;

   if ( gv->scs_monitorEvent )
   {
      checkEventThreshold( tp, object, 0, 1 );

      tp->fWorking = 0;
      memory_fence();
      return;
   }

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbMonitorWaited( jvmtiEnv * jvmti,
                 JNIEnv   * env,
                 jthread    thread,
                 jobject    object,
                 jboolean   timedout )
{
   thread_t  * tp;

   tp = tie_prolog(env, JVMTI_EVENT_MONITOR_WAITED, "cbMonitorWaited");
   if (tp == NULL) return;

   if ( gv->scs_monitorEvent )
   {
      checkEventThreshold( tp, object, 0, 1 );

      tp->fWorking = 0;
      memory_fence();
      return;
   }

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbMonitorContendedEnter( jvmtiEnv * jvmti,
                         JNIEnv   * env,
                         jthread    thread,
                         jobject    object )
{
   thread_t  * tp;

   tp = tie_prolog(env, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, "cbMonitorContendedEnter");
   if (tp == NULL) return;

   if ( gv->scs_monitorEvent )
   {
      checkEventThreshold( tp, object, 0, 1 );

      tp->fWorking = 0;
      memory_fence();
      return;
   }

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbMonitorContendedEntered( jvmtiEnv * jvmti,
                           JNIEnv   * env,
                           jthread    thread,
                           jobject    object )
{
   thread_t  * tp;

   tp = tie_prolog(env, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, "cbMonitorContendedEntered");
   if (tp == NULL) return;

   if ( gv->scs_monitorEvent )
   {
      checkEventThreshold( tp, object, 0, 1 );

      tp->fWorking = 0;
      memory_fence();
      return;
   }

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbCompilingStart( jvmtiEnv * jvmti,
                  jmethodID  mid,
                  ... )
{
   thread_t  * tp;

   tp = tie_prolog( 0, ge.compilingStart, "cbCompilingStart");
   if (tp == NULL) return;

   tp->mp = insert_method( mid, 0 );

   rehmethods(tp);

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbCompilingEnd( jvmtiEnv * jvmti,
                jmethodID  mid,
                ... )
{
   thread_t  * tp;

   tp = tie_prolog( 0, ge.compilingEnd, "cbCompilingEnd");
   if (tp == NULL) return;

   tp->mp = insert_method( mid, 0 );

   rehmethods(tp);

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbMethodEntryExtended( jvmtiEnv  * jvmti,
                       JNIEnv    * env,
                       jthread     thread,
                       jmethodID   mid,
                       jint        mtype,
                       ... )
{
   thread_t  * tp;

// va_list     args;
//
// JNIEnv    * env;                     // Fixed parameters taken from var_args
// jthread     thread;
// jmethodID   mid;
// jint        mtype;                   // Type indicator: see gv_type_XXX

// va_start( args, jvmti );
//
// env    = va_arg( args, JNIEnv *  );
// thread = va_arg( args, jthread   );
// mid    = va_arg( args, jmethodID );
// mtype  = va_arg( args, jint      );
//
// va_end( args );

   tp = tie_prolog(env,
                   TIExtOff + JVMTI_EVENT_METHOD_ENTRY + mtype,   // Extra offset w/type
                   "cbMethodEntryExtended");

   if ( tp == NULL ) return;

   if ( ge.EnExCs )
   {
      if ( tp->num_frames >= tp->max_frames )
      {
         allocEnExCs( tp );
      }
      ((jmethodID *)tp->frame_buffer)[ tp->num_frames++ ] = mid;

      tp->fWorking = 0;
      memory_fence();
      return;
   }

   if ( gv->statdyn )
   {
      if ( tp->mt3 <= gv_type_maxDynamic )
      {
         tp->mp = insert_method( mid,0 );

         if ( tp->mp->flags & method_flags_native )
         {
            tp->mt3 = gv_type_Native;   // Mark as native method
         }
         if ( tp->mp->flags & method_flags_static )
         {
            tp->mt3--;                  // Mark as static method
         }
      }
   }

   tp->mp = insert_method( mid, 0 );

   rehmethods(tp);

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbMethodEntry( jvmtiEnv * jvmti,
               JNIEnv   * env,
               jthread    thread,
               jmethodID  mid)
{
   thread_t  * tp;

   tp = tie_prolog(env, JVMTI_EVENT_METHOD_ENTRY, "cbMethodEntry");
   if (tp == NULL) return;

   if ( ge.EnExCs )
   {
      if ( tp->num_frames >= tp->max_frames )
      {
         allocEnExCs( tp );
      }
      ((jmethodID *)tp->frame_buffer)[ tp->num_frames++ ] = mid;

      tp->fWorking = 0;
      memory_fence();
      return;
   }

   tp->mp = insert_method( mid, 0 );

   rehmethods(tp);

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbMethodExit( jvmtiEnv * jvmti,
              JNIEnv   * env,
              jthread    thread,
              jmethodID  mid,
              jboolean   was_popped_by_exception,
              jvalue     rc)
{
   thread_t  * tp;

   tp = tie_prolog(env, JVMTI_EVENT_METHOD_EXIT, "cbMethodExit");
   if (tp == NULL) return;

   if ( ge.EnExCs )
   {
      if ( tp->num_frames > 0 )
      {
         tp->num_frames--;
      }
      tp->fWorking = 0;
      memory_fence();
      return;
   }

   if ( ge.jvmti != jvmti )
   {
      ErrVMsgLog( "ge.jvmti = %p, jvmti = %p\n", ge.jvmti, jvmti );
   }

   tp->flags &= ~thread_exitpopped;

   if ( was_popped_by_exception )
   {
      if ( gc.mask )                    // Check debug mask
      {
         WriteFlush(gd.gen, " # poppedByEXCEPTION mid %x\n", mid);
         fprintf(   gc.msg, " # poppedByEXCEPTION\n");
      }

      if ( gv->exitpop )
      {
         tp->flags |= thread_exitpopped;
      }
   }

   tp->mp = insert_method( mid, 0 );

   rehmethods(tp);

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbMethodExitNoRc( jvmtiEnv * jvmti,
                  JNIEnv   * env,
                  jthread    thread,
                  jmethodID  mid,
                  jboolean   was_popped_by_exception,
                  ... )
{
   thread_t  * tp;

   tp = tie_prolog(env, JVMTI_EVENT_METHOD_EXIT, "cbMethodExitNoRc");
   if (tp == NULL) return;

   if ( ge.EnExCs )
   {
      if ( tp->num_frames > 0 )
      {
         tp->num_frames--;
      }
      tp->fWorking = 0;
      memory_fence();
      return;
   }

   if ( ge.jvmti != jvmti )
   {
      ErrVMsgLog( "ge.jvmti = %p, jvmti = %p\n", ge.jvmti, jvmti );
   }

   tp->flags &= ~thread_exitpopped;

   if ( was_popped_by_exception )
   {
      if ( gc.mask )                    // Check debug mask
      {
         WriteFlush(gd.gen, " # poppedByEXCEPTION mid %x\n", mid);
         fprintf(   gc.msg, " # poppedByEXCEPTION\n");
      }

      if ( gv->exitpop )
      {
         tp->flags |= thread_exitpopped;
      }
   }

   tp->mp = insert_method( mid, 0 );

   rehmethods(tp);

   timeJprofExit(tp);
}

/***************************/
void JNICALL
cbAsyncEvent( jvmtiEnv * jvmti,
              JNIEnv   * env,
              jthread    thread,
              ... )
{
   int num_frames = 0;
   thread_t * tp;

   if ( ge.jvmti != jvmti )
   {
      ErrVMsgLog( "ge.jvmti = %p, jvmti = %p\n", ge.jvmti, jvmti );
   }

   tp         = insert_thread( CurrentThreadID() );
   tp->mt3    = gv_type_Other;
   tp->x23    = gv_type_Other;
   num_frames = pushThreadCallStack(tp, tp, 0);

#if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
   tp->sampler_active = 0;              // Reset sampler_active for thread
   memory_fence();
#endif
}

/***************************/
void JNICALL
cbVMDeath( jvmtiEnv * jvmti,
           JNIEnv * env)
{
   StopJProf( sjp_JVMTI_VMDEATH );
}

/***************************/
void JNICALL
cbThreadEnd( jvmtiEnv * jvmti,
             JNIEnv   * env,
             jthread    thread)
{
   thread_t * tp;
   UINT32     tid;

   tp = tie_prolog(env, JVMTI_EVENT_THREAD_END, "cbThreadEnd");

   if ( tp )
   {
      if ( ge.jvmti != jvmti )
      {
         ErrVMsgLog( "ge.jvmti = %p, jvmti = %p\n", ge.jvmti, jvmti );
      }

      tp = readTimeStamp(tp->tid);

      endThreadName(tp);

      timeJprofExit(tp);

      if ( ge.scs )
      {
         ScsUninitThread(tp->tid);
      }

      if ( gv->gatherMetrics && gv->physmets && tp->ptt_enabled)
      {
         PttUnInitThread(tp->tid);
      }
   }
   else
   {
      tid = CurrentThreadID();
      tp  = lookup_thread( tid );

      if ( tp )
      {
         endThreadName(tp);
      }
   }
}

/***************************/
void JNICALL
cbThreadStart( jvmtiEnv * jvmti,
               JNIEnv   * env,
               jthread    thread)
{
   int tid = CurrentThreadID();
   thread_t * tp = insert_thread(tid);
   if (tp == NULL) return;

   if ( gv->gatherMetrics && gv->physmets && tp->ptt_enabled == 0)
   {
      int rc = PttInitThread(tid);
      if (rc == PU_SUCCESS)
         tp->ptt_enabled = 1;
   }

   tp = tie_prolog(env, JVMTI_EVENT_THREAD_START, "cbThreadStart");
   if (tp == NULL) return;

   if ( ge.jvmti != jvmti )
   {
      ErrVMsgLog( "ge.jvmti = %p, jvmti = %p\n", ge.jvmti, jvmti );
   }

   if ( 0 == tp->thread )
   {
      doThreadStart( jvmti, env, thread, tp->tid );
   }

   if ( ge.scs )
   {
      int rc = ScsInitThread(gv->scs_active, tp->tid);
      if (rc = PU_SUCCESS)
         tp->scs_enabled = 1;
   }

   timeJprofExit(tp);
}


/***************************/
void JNICALL
cbListenerWrapper( jvmtiEnv * jvmti,
                   JNIEnv   * env,
                   void     * x)
{
   thread_t * tp;
   UINT32     tid = CurrentThreadID();

   tp = insert_thread( tid );

   tp->env_id = env;

   RTWaitForClient(x);
}

/***************************/
void JNICALL
cbHealthCenterListenerWrapper( jvmtiEnv * jvmti,
                   JNIEnv   * env,
                   void     * x)
{
   thread_t * tp;
   UINT32     tid = CurrentThreadID();

   tp = insert_thread( tid );

   tp->env_id = env;

   HealthCenterListenerLoop(x);
}

/***************************/
/* JVMTI callback function */
jvmtiIterationControl JNICALL
cbHeapObject( jlong                    tagClass,
              jlong                    size,
              jlong                  * pTag,
              void                   * user_data)
{
   thread_t  * tp;
   class_t   * cp;
   jlong       tag;
   jint        isArray;

   if ( 0 == pTag || 0 == tagClass )
   {
      OptVMsgLog("cbHeapObject: Ignored: pTag = %p, tagClass = %08"_P64"X, size = %"_P64"d\n",
                 pTag, tagClass, size );
   }
   else
   {
      tag     = *pTag;

      if ( 0 == tag )
      {
         tag   = getNewTag();
         *pTag = tag;

         MVMsg(gc_mask_ObjectInfo,
               ("cbHeapObject: Set new tag = %08"_P64"X\n", tag));
      }

      tp      = insert_thread( CurrentThreadID() );

      isArray = 0;

      cp      = insert_class( getIdFromTag( tagClass ) );

      updateInstanceSize( cp, size );

      if ( '[' == cp->class_name[0] )   // Array object found
      {
         if ( ']' == cp->class_name[1] )   // Already converted from [Lxxx to []xxx
         {
            isArray = JVMTI_TYPE_JCLASS;
         }
         else
         {
            tagClass = 0;

            switch ( cp->class_name[1] )
            {
            case 'Z':
               isArray = JVMTI_TYPE_JBOOLEAN;
               break;
            case 'B':
               isArray = JVMTI_TYPE_JBYTE;
               break;
            case 'C':
               isArray = JVMTI_TYPE_JCHAR;
               break;
            case 'S':
               isArray = JVMTI_TYPE_JSHORT;
               break;
            case 'I':
               isArray = JVMTI_TYPE_JINT;
               break;
            case 'J':
               isArray = JVMTI_TYPE_JLONG;
               break;
            case 'F':
               isArray = JVMTI_TYPE_JFLOAT;
               break;
            case 'D':
               isArray = JVMTI_TYPE_JDOUBLE;
               break;
            }
         }
      }

      objectAllocInfo(tp,
                      (jint)size,
                      (jobject)cp,
                      getIdFromTag(tag),
                      isArray);

      MVMsg(gc_mask_ObjectInfo,
            ("HeapObjAlloc: tag = %8"_P64"X, size = %8"_P64"d, class = %s\n",
             tag, size, cp->class_name ));
   }
   return JVMTI_ITERATION_CONTINUE;
}

/***************************/
void JNICALL
cbVMInit( jvmtiEnv * jvmti,
          JNIEnv   * env,
          jthread thread)
{
   thread_t    * tp;
   jthread       new_thread;
   jvmtiError    rc;

   int           tid = CurrentThreadID();

   OptVMsgLog(" ==> cbVMInit, jvmti = %p, tid = %p\n", jvmti, tid);

   tp         = insert_thread( tid );
   tp->env_id = env;

   if ( !ge.HealthCenter )
   {
      RestartTiJprof( tp );
   }

   jprof_worker_thread_extra_args extraArgs;
   extraArgs.jvmti = jvmti;
   extraArgs.env = env;
   extraArgs.thread = thread;

   rc = init_pu_worker(start_worker_thread, (void *)&extraArgs);
   if (rc != 0) {
      err_exit("**ERROR** Failed to start worker thread.");
   }

   //-----------------------------------
   // Setup Socket and Listener
   //-----------------------------------

   if ( gv->rtdriver )
   {
      OVMsg((" Initializing Socket\n"));

      rc = RTCreateSocket();

      OptVMsg(" RTCreateSocket, rc = %d\n", rc);

      if ( 0 == rc )
      {
         new_thread = create_java_thread( jvmti,
                                          env,
                                          cbListenerWrapper,
                                          NULL,
                                          JVMTI_THREAD_MAX_PRIORITY,
                                          strRtdriverListener );

         if (new_thread == NULL)
         {
            err_exit(" **ERROR** Unable to create RTDriver Listener thread. ");
         }
      }
      else
      {
         err_exit(" **ERROR** Unable to find socket to listen on. ");
      }
   }
   
   if ( ge.HealthCenter )
   {
      OVMsg((" Initializing Health Center Listener\n"));

      new_thread = create_java_thread( jvmti,
                                       env,
                                       cbHealthCenterListenerWrapper,
                                       NULL,
                                       JVMTI_THREAD_MAX_PRIORITY,
                                       strHCListener );

      if (new_thread == NULL)
      {
         err_exit(" **ERROR** Unable to create Health Center Listener thread. ");
      }
   }

   OptVMsgLog(" <== cbVMInit\n");
}

static void ShowCaps( jvmtiCapabilities *caps )
{
   unsigned char *ucp = (unsigned char *)caps;
   int           i;

   OptVMsgLog( "%d = can_tag_objects\n",                          caps->can_tag_objects );
   OptVMsgLog( "%d = can_generate_field_modification_events\n",   caps->can_generate_field_modification_events );
   OptVMsgLog( "%d = can_generate_field_access_events\n",         caps->can_generate_field_access_events );
   OptVMsgLog( "%d = can_get_bytecodes\n",                        caps->can_get_bytecodes );

   OptVMsgLog( "%d = can_get_synthetic_attribute\n",              caps->can_get_synthetic_attribute );
   OptVMsgLog( "%d = can_get_owned_monitor_info\n",               caps->can_get_owned_monitor_info );
   OptVMsgLog( "%d = can_get_current_contended_monitor\n",        caps->can_get_current_contended_monitor );
   OptVMsgLog( "%d = can_get_monitor_info\n",                     caps->can_get_monitor_info );

   OptVMsgLog( "%d = can_pop_frame\n",                            caps->can_pop_frame );
   OptVMsgLog( "%d = can_redefine_classes\n",                     caps->can_redefine_classes );
   OptVMsgLog( "%d = can_signal_thread\n",                        caps->can_signal_thread );
   OptVMsgLog( "%d = can_get_source_file_name\n",                 caps->can_get_source_file_name );

   OptVMsgLog( "%d = can_get_line_numbers\n",                     caps->can_get_line_numbers );
   OptVMsgLog( "%d = can_get_source_debug_extension\n",           caps->can_get_source_debug_extension );
   OptVMsgLog( "%d = can_access_local_variables\n",               caps->can_access_local_variables );
   OptVMsgLog( "%d = can_maintain_original_method_order\n",       caps->can_maintain_original_method_order );

   OptVMsgLog( "%d = can_generate_single_step_events\n",          caps->can_generate_single_step_events );
   OptVMsgLog( "%d = can_generate_exception_events\n",            caps->can_generate_exception_events );
   OptVMsgLog( "%d = can_generate_frame_pop_events\n",            caps->can_generate_frame_pop_events );
   OptVMsgLog( "%d = can_generate_breakpoint_events\n",           caps->can_generate_breakpoint_events );

   OptVMsgLog( "%d = can_suspend\n",                              caps->can_suspend );
   OptVMsgLog( "%d = can_redefine_any_class\n",                   caps->can_redefine_any_class );
   OptVMsgLog( "%d = can_get_current_thread_cpu_time\n",          caps->can_get_current_thread_cpu_time );
   OptVMsgLog( "%d = can_get_thread_cpu_time\n",                  caps->can_get_thread_cpu_time );

   OptVMsgLog( "%d = can_generate_method_entry_events\n",         caps->can_generate_method_entry_events );
   OptVMsgLog( "%d = can_generate_method_exit_events\n",          caps->can_generate_method_exit_events );
   OptVMsgLog( "%d = can_generate_all_class_hook_events\n",       caps->can_generate_all_class_hook_events );
   OptVMsgLog( "%d = can_generate_compiled_method_load_events\n", caps->can_generate_compiled_method_load_events );

   OptVMsgLog( "%d = can_generate_monitor_events\n",              caps->can_generate_monitor_events );
   OptVMsgLog( "%d = can_generate_vm_object_alloc_events\n",      caps->can_generate_vm_object_alloc_events );
   OptVMsgLog( "%d = can_generate_native_method_bind_events\n",   caps->can_generate_native_method_bind_events );
   OptVMsgLog( "%d = can_generate_garbage_collection_events\n",   caps->can_generate_garbage_collection_events );

   OptVMsgLog( "%d = can_generate_object_free_events\n",          caps->can_generate_object_free_events );

   OptVMsgLog( "Caps:" );

   for ( i = 0; i < sizeof(jvmtiCapabilities); i++ )
   {
      OptVMsgLog( " %02X", *ucp++ );
   }
   OptVMsgLog( "\n" );
}

/******************************/
JNIEXPORT jint JNICALL
Agent_OnLoad( JavaVM * vm,
              char * jopts,
              void * rsv)
{
   jint                rc, i, j;
   jvmtiCapabilities   capsPotential;
   jvmtiEventCallbacks cbs;

   char              * p;
   char              * q;
   char              * pMUTF8;
   char                buf[READ_SIZE];
   char              * options;

   jint                         xcnt;
   jvmtiExtensionFunctionInfo * exfn;
   jvmtiExtensionEventInfo    * exev;

   jvmtiExtensionFunctionInfo * fi;
   jvmtiExtensionEventInfo    * ei;
   jvmtiParamInfo             * pi;
   jvmtiError                 * ep;

   jvmtiExtensionFunction       addExtendedCapabilities          = 0;
   jvmtiExtensionFunction       getExtendedCapabilities          = 0;
   jvmtiExtensionFunction       getPotentialExtendedCapabilities = 0;

//------------------------------------------------ WOW64 socket error 10041 circumvention
#if defined(_WINDOWS)
   SOCKET s;

   s = socket(AF_INET, SOCK_STREAM, 0);

   if (s != INVALID_SOCKET)
   {
      closesocket(s);
   }
#endif
//------------------------------------------------ WOW64 socket error 10041 circumvention

   options = initJprof( jopts );

   if ( ge.jvmti )
   {
      ErrVMsgLog("\nERROR: JProf already initialized, -agentlib:jprof= ignored\n");
      return( JNI_OK );
   }
   ge.onload = 1;                       // loaded by JVM, not Hook.java or RTDriver.java

   OptVMsg(">>> Agent_OnLoad\n");       // OptVMsg must follow initOptions
   OptVMsg("  VM = %p, Options <%s>\n", vm, options );

   rc = (*vm)->GetEnv(vm, (void **)&ge.jvmti, JVMTI_VERSION_1_0);

   if (rc != JNI_OK)
   {
      ErrVMsgLog("ERROR: Unable to create jvmtiEnv, GetEnv failed, rc=%d\n", rc);
      return -1;
   }
   OptVMsg ("JVMTI Environment: %p\n", ge.jvmti);

#if defined(_CHAR_EBCDIC)

   Native2Java( strJavaLangClass );     // Handle EBCDIC, if necessary
   Native2Java( strJavaLangThread );    // Handle EBCDIC, if necessary
   Native2Java( strSetName );           // Handle EBCDIC, if necessary
   Native2Java( sigSetName );           // Handle EBCDIC, if necessary
   Native2Java( strRtdriverListener );  // Handle EBCDIC, if necessary
   Native2Java( strLJavaLangClass);     // Handle EBCDIC, if necessary
   Native2Java( strTYPE );              // Handle EBCDIC, if necessary
   Native2Java( strInit );              // Handle EBCDIC, if necessary
   Native2Java( strV );                 // Handle EBCDIC, if necessary
   Native2Java( strJavaVmName );        // Handle EBCDIC, if necessary
   Native2Java( strJavaVmVersion );     // Handle EBCDIC, if necessary
   Native2Java( strJavaVmInfo );        // Handle EBCDIC, if necessary
   Native2Java( strJavaVmVendor );      // Handle EBCDIC, if necessary
   Native2Java( strComma );             // Handle EBCDIC, if necessary

   for ( i = 0; i < 9; i++ )
   {
      Native2Java( reflectClassNames[i] );   // Handle EBCDIC, if necessary
   }

#endif

//--------------------------------------
// Display all system properties
//--------------------------------------

   if ( gc.mask & gc_mask_JVMTI_info )
   {
      jint   numProperties;
      char **Property;

      rc = (*ge.jvmti)->GetSystemProperties(ge.jvmti, &numProperties, &Property);

      if ( JVMTI_ERROR_NONE != rc )
      {
         ErrVMsgLog("\nGetSystemProperties: rc = %d\n", rc);
      }
      else
      {
         OptVMsg("\nSystem Properties:\n\n");

         for ( i = 0; i < numProperties; i++ )
         {
            if ( 0 == (*ge.jvmti)->GetSystemProperty(ge.jvmti, Property[i], &pMUTF8) )
            {
               Force2Native( Property[i] );
               Force2Native( pMUTF8 );

               OptVMsg("%40s = %s\n", Property[i], pMUTF8);

               (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)pMUTF8);
            }
            (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)Property[i]);
         }
         (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)Property);

         OptVMsg("\n");
      }
   }

//--------------------------------------
// Get JVM version information
//--------------------------------------

   buf[0] = 0;

   if ( 0 == (*ge.jvmti)->GetSystemProperty(ge.jvmti, strJavaVmName, &pMUTF8) )
   {
      OptVMsg("VM Name      = %s\n", pMUTF8);

      strcat( strcat( buf, strComma ) , pMUTF8 );

      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)pMUTF8);
   }
   if ( 0 == (*ge.jvmti)->GetSystemProperty(ge.jvmti, strJavaVmVersion, &pMUTF8) )
   {
      OptVMsg("VM Version   = %s\n", pMUTF8);

      strcat( strcat( buf, strComma ) , pMUTF8 );

      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)pMUTF8);
   }
   if ( 0 == (*ge.jvmti)->GetSystemProperty(ge.jvmti, strJavaVmInfo, &pMUTF8) )
   {
      OptVMsg("VM Info      = %s\n", pMUTF8);

      strcat( strcat( buf, strComma ) , pMUTF8 );

      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)pMUTF8);
   }
   if ( 0 == (*ge.jvmti)->GetSystemProperty(ge.jvmti, strJavaVmVendor, &pMUTF8) )
   {
      OptVMsg("VM Vendor    = %s\n", pMUTF8);

      strcat( strcat( buf, strComma ) , pMUTF8 );

      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)pMUTF8);
   }
// if ( 0 == (*ge.jvmti)->GetSystemProperty(ge.jvmti, "java.library.path", &pMUTF8) )
// {
//    OptVMsg("Library Path = %s\n", pMUTF8);
//
//    (*ge.jvmti)->Deallocate(ge.jvmti, pMUTF8);
// }
// if ( 0 == (*ge.jvmti)->GetSystemProperty(ge.jvmti, "java.class.path", &pMUTF8) )
// {
//    OptVMsg("Class Path   = %s\n", pMUTF8);
//
//    (*ge.jvmti)->Deallocate(ge.jvmti, pMUTF8);
// }

   Java2Native( buf );                  // Translate name to EBCDIC, as needed

   ge.jvmver = xStrdup( "ge.jvmver", (',' == buf[0]) ? buf+1 : buf );

   if ( 0 == (*ge.jvmti)->GetVersionNumber(ge.jvmti, &ge.jvmtiVersion) )
   {
      OptVMsgLog("JVMTI Ver    = %X\n", ge.jvmtiVersion);
   }

//--------------------------------------
// Manage Extension Functions
//--------------------------------------

   rc = (*ge.jvmti)->GetExtensionFunctions(ge.jvmti, &xcnt, &exfn);

   if ( JVMTI_ERROR_NONE != rc )
   {
      ErrVMsgLog("\nGetExtensionFunctions: rc = %d\n", rc);
   }

#if defined(_CHAR_EBCDIC)

   MVMsg( gc_mask_JVMTI_info,("Translating function names from ASCII to EBCDIC\n"));

   fi = exfn;

   for (i = 0; i < xcnt; i++)
   {
      Java2Native( fi->short_description );
      Java2Native( fi->id );            // Translate name to EBCDIC

      pi = fi->params;

      for (j = 0; j < fi->param_count; j++)
      {
         Java2Native( pi->name );       // Translate name to EBCDIC

         pi++;
      }
      fi++;
   }

#endif

   if ( gc.mask & gc_mask_JVMTI_info )
   {
      OptVMsgLog("\nGetExtensionFunctions: Count = %d\n", xcnt);

      fi = exfn;

      for (i = 0; i < xcnt; i++)
      {
         OptVMsgLog("\nFunction # %d: %s\n",
                    i,
                    fi->short_description);
         OptVMsgLog(" Ptr = %p, Parms = %d, Id = %s\n",
                    fi->func,
                    fi->param_count,
                    fi->id );

         pi = fi->params;

         for (j = 0; j < fi->param_count; j++)
         {
            OptVMsgLog("  Parm # %d: %s\n",
                       j,
                       pi->name );
            OptVMsgLog("   Kind = %d, Btype = %d, Null OK = %d\n",
                       pi->kind,
                       pi->base_type,
                       pi->null_ok );

            pi++;
         }

         OptVMsgLog("  Error count = %d",
                    fi->error_count);

         if ( fi->error_count > 0 )
         {
            ep = fi->errors;

            OptVMsgLog(":  %d", *ep++);

            for (j = 1; j < fi->error_count; j++)
            {
               OptVMsgLog(", %d", *ep++);
            }
         }
         OptVMsgLog("\n");

         fi++;
      }
      OptVMsgLog("\nGetExtensionFunctions: Done\n");
   }

   // Cleanup after GetExtensionFunctions while extracting information

   fi = exfn;

   for (i = 0; i < xcnt; i++)
   {
      if ( 0 == strcmp( fi->id, "com.ibm.AddExtendedCapabilities" ) )
      {
         addExtendedCapabilities = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.GetExtendedCapabilities" ) )
      {
         getExtendedCapabilities = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.GetPotentialExtendedCapabilities" ) )
      {
         getPotentialExtendedCapabilities = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.SetVmJlm" ) )
      {
         ge.setVMLockMonitor  = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.SetVmJlmDump" ) )
      {
         ge.dumpVMLockMonitor = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.AllowMethodInliningWithMethodEnterExit" ) )
      {
         ge.allowMethodInlining = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.AllowDirectJNIWithMethodEnterExit" ) )
      {
         ge.allowDirectJNI      = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.SetVmAndCompilingControlOptions" ) )
      {
         ge.setVmAndCompilingControlOptions   = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.jvmtiSetMethodSelectiveEntryExitNotification" ) )
      {
         ge.setMethodSelectiveEntryExitNotify = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.jvmtiClearMethodSelectiveEntryExitNotification" ) )
      {
         ge.clearMethodSelectiveEntryExitNotify = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.jvmtiSetExtendedEventNotificationMode" ) )
      {
         ge.setExtendedEventNotificationMode  = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.GetOSThreadID" ) )
      {
         ge.getOSThreadID = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.GetStackTraceExtended" ) )
      {
         ge.getStackTraceExtended = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.GetAllStackTracesExtended" ) )
      {
         ge.getAllStackTracesExtended = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.GetThreadListStackTracesExtended" ) )
      {
         ge.getThreadListStackTracesExtended = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.SignalAsyncEvent" ) )
      {
         ge.signalAsyncEvent = fi->func;
      }
      else if ( 0 == strcmp( fi->id, "com.ibm.CancelAsyncEvent" ) )
      {
         ge.cancelAsyncEvent = fi->func;
      }
#ifndef _LEXTERN
      else if ( 0 == strcmp( fi->id, "com.ibm.JlmDumpStats" ) )
      {
         ge.dumpJlmStats = fi->func;
      }
#endif

      // Cleanup

      pi = fi->params;

      for (j = 0; j < fi->param_count; j++)
      {
         (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)pi->name);

         pi++;
      }
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)fi->id);
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)fi->short_description);
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)fi->params);
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)fi->errors);

      fi++;
   }
   (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)exfn);

//--------------------------------------
// Common Initialization
//--------------------------------------

   if ( ge.dumpVMLockMonitor )
   {
      ge.JVMversion = JVM_J9;
      OptVMsg(" JVMversion = J9\n");
   }
   else
   {
      ge.JVMversion = JVM_SUN;
      OptVMsg(" JVMversion = SUN\n");
   }
   init_event_names();

//--------------------------------------
// Other common initialization
//--------------------------------------

   rc = ParseJProfOptions(options);

   if ( JVMTI_ERROR_NONE != rc )
   {
      logFailure(rc);
      return( JNI_ERR );
   }

   OptVMsg(" => SetupJVMTI\n");

   if ( gv->calltree || gv->calltrace || ge.EnExCs || ge.HealthCenter )
   {
      if ( gv->selListMethods )
      {
         if (ge.setVmAndCompilingControlOptions)
         {
            OptVMsg("Enabling Selective Method Entry/Exit\n");

            (ge.setVmAndCompilingControlOptions) (ge.jvmti, COM_IBM_ENABLE_SELECTIVE_METHOD_ENTRY_EXIT_NOTIFICATION );   // set selective mode
         }
         else
         {
            ErrVMsgLog( "Selective Method Entry/Exit not supported");

            xFree( gv->selListMethods, *(UINT32 *)gv->selListMethods );
            gv->selListMethods = 0;
            return JVMTI_ERROR_INTERNAL;
         }
      }

      rc = reh_Init();

      if ( JVMTI_ERROR_NONE != rc )
      {
         ErrVMsgLog("reh_Init failure, rc = %d\n", rc);

         return( JNI_ERR );
      }
   }

   OptVMsg(" <= SetupJVMTI\n");

//--------------------------------------
// Manage Extension Events
//--------------------------------------

   rc = (*ge.jvmti)->GetExtensionEvents(ge.jvmti, &xcnt, &exev);

#if defined(_CHAR_EBCDIC)

   MVMsg( gc_mask_JVMTI_info,("Translating event names from ASCII to EBCDIC\n"));

   ei = exev;

   for (i = 0; i < xcnt; i++)
   {
      Java2Native( ei->id );            // Translate name to EBCDIC

      pi = ei->params;

      for (j = 0; j < ei->param_count; j++)
      {
         Java2Native( pi->name );       // Translate name to EBCDIC

         pi++;
      }
      ei++;
   }

#endif

   if ( gc.mask & gc_mask_JVMTI_info )
   {
      OptVMsgLog("\nGetExtensionEvents: Count = %d\n",
                 xcnt);

      ei = exev;

      for (i = 0; i < xcnt; i++)
      {
         OptVMsgLog("\nEvent # %d:\n",
                    i,
                    ei->short_description);

         OptVMsgLog(" Index = %d, Parms = %d, Id = %s, \n",
                    ei->extension_event_index,
                    ei->param_count,
                    ei->id );

         pi = ei->params;

         for (j = 0; j < ei->param_count; j++)
         {
            OptVMsgLog("  Parm # %d: %s\n",
                       j,
                       pi->name);
            OptVMsgLog("   Kind = %d, Btype = %d, Null OK = %d\n",
                       pi->kind,
                       pi->base_type,
                       pi->null_ok );

            pi++;
         }
         ei++;
      }
      OptVMsgLog("\nGetExtensionEvents: Done\n");
   }

   // Cleanup after GetExtensionEvents while extracting information

   ei = exev;

   for (i = 0; i < xcnt; i++)
   {
      if ( gv->calltree || gv->calltrace || ge.EnExCs || ge.HealthCenter )
      {
         if ( 0 == strcmp( ei->id, "com.ibm.MethodEntryExtended" ) )
         {
            ge.methodEntryExtended = ei->extension_event_index;

            OptVMsg( "Extension event found: %d, %s\n",
                     ge.methodEntryExtended, ei->id );

            ge.evstr[ ge.methodEntryExtended ] = ("TI_METHOD_ENTRY_EXTENDED");
         }

         if ( gv->exitNoRc )
         {
            if ( 0 == strcmp( ei->id, "com.ibm.MethodExitNoRc" ) )
            {
               ge.methodExitNoRc = ei->extension_event_index;

               OptVMsg( "Extension event found: %d, %s\n",
                        ge.methodExitNoRc, ei->id );

               ge.evstr[ ge.methodExitNoRc ] = ("TI_METHOD_EXIT_NO_RC");
            }
         }
      }

      if ( 0 == strcmp( ei->id, "com.ibm.AsyncEvent" ) )
      {
         ge.asyncEvent = ei->extension_event_index;

         OptVMsg( "Extension event found: %d, %s\n",
                  ge.asyncEvent, ei->id );

         ge.evstr[ ge.asyncEvent ] = ("TI_ASYNC_EVENT");
      }

#ifndef _LEXTERN
      if ( ge.ObjectInfo || gv->scs_allocBytes || ge.HealthCenter )
      {
         if ( 0 == strcmp( ei->id, "com.ibm.InstrumentableObjectAlloc" ) )
         {
            ge.instrumentableObjectAlloc = ei->extension_event_index;

            OptVMsg( "Extension event found: %d, %s\n",
                     ge.instrumentableObjectAlloc, ei->id );

            ge.evstr[ ge.instrumentableObjectAlloc ] = ("TI_INSTRUMENTABLE_OBJECT_ALLOC");
         }


         if ( 0 == strcmp( ei->id, "com.ibm.ArrayClassLoad" ) )
         {
            ge.arrayClassLoad = ei->extension_event_index;

            OptVMsg( "Extension event found: %d, %s\n",
                     ge.arrayClassLoad, ei->id );

            ge.evstr[ ge.arrayClassLoad ] = ("TI_ARRAY_CLASS_LOAD");
         }
      }
#endif

      if ( gv->CompilingStartEnd || ge.HealthCenter )
      {
         if ( 0 == strcmp( ei->id, "com.ibm.CompilingStart" ) )
         {
            ge.compilingStart = ei->extension_event_index;

            OptVMsg( "Extension event found: %d, %s\n",
                     ge.compilingStart, ei->id );

            ge.evstr[  ge.compilingStart ] = ("TI_COMPILING_START");
            ge.ee[     ge.compilingStart ] = gv_ee_Entry;
            ge.type[   ge.compilingStart ] = gv_type_Compiling;
         }

         if ( 0 == strcmp( ei->id, "com.ibm.CompilingEnd" ) )
         {
            ge.compilingEnd   = ei->extension_event_index;

            OptVMsg( "Extension event found: %d, %s\n",
                     ge.compilingEnd, ei->id );

            ge.evstr[ ge.compilingEnd ] = ("TI_COMPILING_END");
            ge.ee[    ge.compilingEnd ] = gv_ee_Exit;
            ge.type[  ge.compilingEnd ] = gv_type_Compiling;
         }
      }

      // Cleanup

      pi = ei->params;

      for (j = 0; j < ei->param_count; j++)
      {
         (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)pi->name);

         pi++;
      }
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)ei->id);
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)ei->short_description);
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)ei->params);

      ei++;
   }
   (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)exev);

//--------------------------------------
// Manage JVM Capabilities
//--------------------------------------

// if ( 0 != getPotentialExtendedCapabilities )
// {
//    memset( &ge.caps, 0, sizeof(ge.caps) );
//    rc = (*getPotentialExtendedCapabilities)(ge.jvmti, &ge.caps);
//    OptVMsg( "GetPotentialExtendedCapabilities, rc = %d\n", rc );
//    ShowCaps( &ge.caps );
// }

   // Get Potential Capabilities
   rc = (*ge.jvmti)->GetPotentialCapabilities(ge.jvmti, &capsPotential);
   OptVMsg( "GetPotentialCapabilities, rc = %d\n", rc );
   // ShowCaps( &capsPotential );

   // Get Current Capabilities
   rc = (*ge.jvmti)->GetCapabilities(ge.jvmti, &ge.caps);
   OptVMsg( "GetCapabilities, rc = %d\n", rc );

   // Set all of the Capabilities that we might use

   if ( gv->JITA2N || gv->CompileLoadInfo || gv->jitinfo || ge.HealthCenter )
   {
      ge.caps.can_generate_compiled_method_load_events  = 1;
   }

   if ( gv->calltree || gv->calltrace || ge.EnExCs || ge.HealthCenter )
   {
      if ( 0 == ge.methodEntryExtended )
      {
         ge.caps.can_generate_method_entry_events    = 1;
      }
      if ( 0 == ge.methodExitNoRc )
      {
         ge.caps.can_generate_method_exit_events     = 1;
      }
   }

   if ( gv->ClassLoadInfo || gv->DistinguishLockObjects || ge.HealthCenter )
   {
      ge.caps.can_tag_objects                        = 1;
   }

   if ( ge.ObjectInfo || gv->scs_allocBytes || ge.HealthCenter )
   {
      ge.caps.can_generate_vm_object_alloc_events    = 1;
   }

   if ( ge.ObjectInfo || ge.HealthCenter )
   {
      ge.caps.can_generate_object_free_events        = 1;
   }

   if ( gv->GCInfo || ge.HealthCenter )
   {
      ge.caps.can_generate_garbage_collection_events = 1;
   }

   if ( gv->scs_monitorEvent || ge.HealthCenter )
   {
      ge.caps.can_generate_monitor_events            = 1;
   }

   if ( gd.syms || ge.ExtendedCallStacks || ge.HealthCenter )
   {
      ge.caps.can_get_source_file_name               = 1;
   }

//    ge.caps.can_get_current_thread_cpu_time        = 1;
//    ge.caps.can_get_thread_cpu_time                = 1;
//    ge.caps.can_generate_native_method_bind_events = 1;
//    ge.caps.can_generate_field_modification_events = 1;
//    ge.caps.can_generate_field_access_events       = 1;
//    ge.caps.can_get_bytecodes                      = 1;
//    ge.caps.can_get_synthetic_attribute            = 1;
//    ge.caps.can_pop_frame                          = 1;
//    ge.caps.can_redefine_classes                   = 1;
//    ge.caps.can_signal_thread                      = 1;

   if ( ge.ExtendedCallStacks || ge.HealthCenter )
   {
      ge.caps.can_get_line_numbers                   = 1;
   }

//    ge.caps.can_get_source_debug_extension         = 1;
//    ge.caps.can_access_local_variables             = 1;
//    ge.caps.can_maintain_original_method_order     = 1;
//    ge.caps.can_generate_single_step_events        = 1;
//    ge.caps.can_generate_exception_events          = 1;
//    ge.caps.can_generate_frame_pop_events          = 1;
//    ge.caps.can_generate_breakpoint_events         = 1;
//    ge.caps.can_suspend                            = 1;

   // Mask off the Capabilities that cannot be supported by the system

   p = (char *)&capsPotential;
   q = (char *)&ge.caps;

   for ( i = 0; i < sizeof(jvmtiCapabilities); i++ )
   {
      q[i] = p[i] & q[i];
   }

   // Add the desired Capabilities

   rc = (*ge.jvmti)->AddCapabilities(ge.jvmti, &ge.caps);

   if ( gc.mask & gc_mask_JVMTI_info )
   {
      ShowCaps( &capsPotential );
      ShowCaps( &ge.caps );

      OptVMsgLog("AddCapabilities, rc = %d\n", rc );
   }

   if ( ge.caps.can_tag_objects == 0 )
   {
      gv->useTagging = 0;
   }

//------------------------------------------------------
// Register CallBacks (Handlers) for each enabled event
//------------------------------------------------------

   memset(&cbs, 0, sizeof(cbs));

   cbs.VMInit                  = cbVMInit;
   cbs.VMDeath                 = cbVMDeath;

   if ( 0 == ge.methodEntryExtended )
   {
      cbs.MethodEntry          = cbMethodEntry;
   }
   if ( 0 == ge.methodExitNoRc )
   {
      cbs.MethodExit           = cbMethodExit;
   }
   cbs.CompiledMethodLoad      = cbCompiledMethodLoad;
   cbs.CompiledMethodUnload    = cbCompiledMethodUnload;
   cbs.ClassLoad               = cbClassLoad;
   cbs.ClassPrepare            = cbClassPrepare;
   cbs.ObjectFree              = cbObjectFree;
   cbs.VMObjectAlloc           = cbVMObjectAlloc;
   cbs.DynamicCodeGenerated    = cbDynamicCodeGenerated;
   cbs.ThreadStart             = cbThreadStart;
   cbs.ThreadEnd               = cbThreadEnd;
   cbs.GarbageCollectionStart  = cbGarbageCollectionStart;
   cbs.GarbageCollectionFinish = cbGarbageCollectionFinish;
   cbs.MonitorWait             = cbMonitorWait;
   cbs.MonitorWaited           = cbMonitorWaited;
   cbs.MonitorContendedEnter   = cbMonitorContendedEnter;
   cbs.MonitorContendedEntered = cbMonitorContendedEntered;

// cbs.ClassFileLoadHook       = cbClassFileLoadHook;
// cbs.DataDumpRequest         = cbDataDumpRequest;

   rc = (*ge.jvmti)->SetEventCallbacks(ge.jvmti, &cbs, sizeof(cbs));
   OptVMsg( "SetEventCallbacks, rc = %d\n", rc );

//---------------
// Enable Events
//---------------

   SetTIEventMode( ge.jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT,  NULL );
   SetTIEventMode( ge.jvmti, JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL );

   if ( ge.allowMethodInlining && gv->methInline )
   {
      (*(ge.allowMethodInlining))(ge.jvmti);   // Allow Method Inlining
   }

   if ( ge.allowDirectJNI      && gv->directJNI )
   {
      (*(ge.allowDirectJNI))(ge.jvmti); // Allow Direct JNI
   }

   if ( ge.HealthCenter == 0 )
   {
      EnableEvents1x(1);
   }

   if ( gv->Methods || ge.HealthCenter )
   {
      if ( ge.methodExitNoRc )
      {
         setExtensionEventCallback( JVMTI_ENABLE,
                                    ge.methodExitNoRc,
                                    (jvmtiExtensionEvent)cbMethodExitNoRc );

         setExtensionEventCallback( JVMTI_DISABLE,
                                    ge.methodExitNoRc,
                                    (jvmtiExtensionEvent)NULL );
      }

      if ( ge.methodEntryExtended )
      {
         setExtensionEventCallback( JVMTI_ENABLE,
                                    ge.methodEntryExtended,
                                    (jvmtiExtensionEvent)cbMethodEntryExtended );

         setExtensionEventCallback( JVMTI_DISABLE,
                                    ge.methodEntryExtended,
                                    (jvmtiExtensionEvent)NULL );
      }
   }

   if ( ge.ObjectInfo
        || gv->scs_allocBytes 
        || ge.HealthCenter )
   {

      if ( ge.instrumentableObjectAlloc )
      {
         setExtensionEventCallback( JVMTI_ENABLE,
                                    ge.instrumentableObjectAlloc,
                                    (jvmtiExtensionEvent)cbInstrumentableObjectAlloc );

         setExtensionEventCallback( JVMTI_DISABLE,
                                    ge.instrumentableObjectAlloc,
                                    (jvmtiExtensionEvent)NULL );
      }
   }
   
   setAgentInitialized();

   OptVMsgLog("<<< Agent_OnLoad\n");

   return JNI_OK;
}


//-----------------------------------------------------------------------
// Enable/Disable JVMTI Events
//-----------------------------------------------------------------------

/***************************/
void SetTIEventMode( jvmtiEnv * jvmti,
                     int        fEnable,
                     int        event,
                     jthread    thr )
{
   int   rc;

   rc = (*jvmti)->SetEventNotificationMode(jvmti, fEnable, event, thr );

   OptVMsgLog( "SetEventNotificationMode:  rc = %d, %s %s\n",
               rc,
               DisableEnableRequest[fEnable],
               ge.evstr[event] );

   if ( rc != JVMTI_ERROR_NONE )
   {
      ErrVMsgLog("ERROR: SetEventNotificationMode: rc = %d, %s %s\n",
                 rc,
                 DisableEnableRequest[fEnable],
                 ge.evstr[event] );
   }
}

void EnableEvents1x(int fEnable)
{
   OptVMsgLog(" EnableEvents1x: %s\n", DisableEnableRequest[fEnable]);

   if ( ge.ThreadInfo )
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_THREAD_START, NULL );

      if ( fEnable )                    // Thread end stays on
      {
         SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_THREAD_END, NULL );
      }
   }

   if ( gv->JITA2N || gv->CompileLoadInfo || gv->jitinfo )
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_COMPILED_METHOD_LOAD,   NULL );
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, NULL );

      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, NULL );
   }

   if (gv->ClassLoadInfo)
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_CLASS_LOAD,    NULL );
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_CLASS_PREPARE, NULL );

#ifndef _LEXTERN
      if ( ge.arrayClassLoad )
      {
         setExtensionEventCallback( fEnable,
                                    ge.arrayClassLoad,
                                    (jvmtiExtensionEvent)cbArrayClassLoad );
      }
#endif
   }

   if (gv->CompilingStartEnd && gv->CompiledEntryExit)
   {
      if ( ge.compilingStart )
      {
         setExtensionEventCallback( fEnable,
                                    ge.compilingStart,
                                    (jvmtiExtensionEvent)cbCompilingStart );
      }

      if ( ge.compilingEnd )
      {
         setExtensionEventCallback( fEnable,
                                    ge.compilingEnd,
                                    (jvmtiExtensionEvent)cbCompilingEnd );
      }
   }

   if ( gv->GCInfo )
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_GARBAGE_COLLECTION_START,  NULL );
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, NULL );
   }
   OptVMsgLog("<< EnableEvents1x(%d)\n", fEnable);
}

void EnableEvents2x(int fEnable)
{
   jint rc;

   OptVMsgLog(" EnableEvents2x: %s\n", DisableEnableRequest[fEnable]);

   // METHOD_EXIT should be enabled and disabled before METHOD_ENTRY
   // to minimize errors in the trees.

   if (gv->Methods)
   {
      if ( fEnable
           && gv->selListThreads
           && ( gv->calltree || gv->calltrace || ge.EnExCs ) )
      {
         // Disable all threads to allow them to be enabled at thread start

         if ( ge.methodExitNoRc )
         {
            setExtensionEventCallback( JVMTI_ENABLE,
                                       ge.methodExitNoRc,
                                       (jvmtiExtensionEvent)cbMethodExitNoRc );

            if ( ge.setExtendedEventNotificationMode )
            {
               // Disable all threads after implicit enable when setting callback

               rc = (ge.setExtendedEventNotificationMode)( ge.jvmti,
                                                           JVMTI_DISABLE,
                                                           ge.methodExitNoRc,
                                                           NULL );

               OptVMsgLog( "SetExtendedEventNotificationMode, Disable, rc = %d\n", rc );
            }
         }
         else
         {
            SetTIEventMode( ge.jvmti, JVMTI_DISABLE, JVMTI_EVENT_METHOD_EXIT, NULL );
         }

         if ( ge.methodEntryExtended )
         {
            setExtensionEventCallback( JVMTI_ENABLE,
                                       ge.methodEntryExtended,
                                       (jvmtiExtensionEvent)cbMethodEntryExtended );

            if ( ge.setExtendedEventNotificationMode )
            {
               // Disable all threads after implicit enable when setting callback

               rc = (ge.setExtendedEventNotificationMode)( ge.jvmti,
                                                           JVMTI_DISABLE,
                                                           ge.methodEntryExtended,
                                                           NULL );

               OptVMsgLog( "SetExtendedEventNotificationMode, Disable, rc = %d\n", rc );
            }
         }
         else
         {
            SetTIEventMode( ge.jvmti, JVMTI_DISABLE, JVMTI_EVENT_METHOD_ENTRY, NULL );
         }
      }
      else
      {
         if ( ge.methodExitNoRc )
         {
            setExtensionEventCallback( fEnable,
                                       ge.methodExitNoRc,
                                       (jvmtiExtensionEvent)cbMethodExitNoRc );
         }
         else
         {
            SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_METHOD_EXIT, NULL );
         }

         if ( ge.methodEntryExtended )
         {
            setExtensionEventCallback( fEnable,
                                       ge.methodEntryExtended,
                                       (jvmtiExtensionEvent)cbMethodEntryExtended );
         }
         else
         {
            SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_METHOD_ENTRY, NULL );
         }
      }
   }

   if (gv->GCInfo)
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_GARBAGE_COLLECTION_START,  NULL );
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, NULL );
   }

   if ( ge.ObjectInfo || gv->scs_allocBytes )
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_VM_OBJECT_ALLOC, NULL );

      if ( ge.instrumentableObjectAlloc )
      {
         setExtensionEventCallback( fEnable,
                                    ge.instrumentableObjectAlloc,
                                    (jvmtiExtensionEvent)cbInstrumentableObjectAlloc );
      }
   }

   if ( gv->scs_monitorEvent == JVMTI_EVENT_MONITOR_WAIT )
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_MONITOR_WAIT, NULL );
   }
   else if ( gv->scs_monitorEvent == JVMTI_EVENT_MONITOR_WAITED )
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_MONITOR_WAITED, NULL );
   }
   else if ( gv->scs_monitorEvent == JVMTI_EVENT_MONITOR_CONTENDED_ENTER )
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL );
   }
   else if ( gv->scs_monitorEvent == JVMTI_EVENT_MONITOR_CONTENDED_ENTERED )
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL );
   }

   if ( ge.ObjectInfo )
   {
      SetTIEventMode( ge.jvmti, fEnable, JVMTI_EVENT_OBJECT_FREE,     NULL );
   }
   OptVMsgLog("<< EnableEvents2x(%d)\n", fEnable);
}
