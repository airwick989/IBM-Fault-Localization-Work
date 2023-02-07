#include "jprof.h"
#include "tree.h"
#include "hash.h"
#include "utils/pi_time.h"

extern char strJavaLangClass[16];
extern char reflectClassNames[9][20];
extern char strTYPE[8];
extern char strLJavaLangClass[20];

jvmtiIterationControl JNICALL
cbHeapObject( jlong                    tagClass,
              jlong                    size,
              jlong                  * pTag,
              void                   * user_data);

//----------------------------------------------------------------------
// Generate unique tags in a thread-safe manner.
//----------------------------------------------------------------------

jlong getNewTag()
{
   jlong tag;

   enter_lock(TagLock, TAGLOCK);

   ge.tagNextObject += 1;

   tag = ge.tagNextObject;

   leave_lock(TagLock, TAGLOCK);

   MVMsg(gc_mask_ObjectInfo,
         ("getNewTag: tag = %08X\n",
          (int)tag ));

   return( tag );
}

jlong setObjectTag( jobject object )
{
   jint  rc;
   jlong tag;

   rc = (*ge.jvmti)->GetTag(ge.jvmti, object, &tag);

   if ( rc != JVMTI_ERROR_NONE )
   {
      ErrVMsgLog("GetTag: object = %p, rc = %d\n", object, rc);
      err_exit("GetTag failure");
   }

   if ( 0 == tag )
   {
      tag = getNewTag();

      rc = (*ge.jvmti)->SetTag(ge.jvmti, object, tag);

      if ( rc != JVMTI_ERROR_NONE )
      {
         ErrVMsgLog("SetTag: object = %p, rc = %d\n", object, rc);
         err_exit("SetTag failure");
      }
      MVMsg(gc_mask_ObjectInfo,
            ("setObjectTag: Set new tag = %08"_P64"X\n", tag));
   }
   return(tag);
}


//----------------------------------------------------------------------
// Perform processing common to VMInit and Restart for JVMTI
//----------------------------------------------------------------------

void RestartTiJprof( thread_t * self_tp )
{
   class_t     * cp;
   thread_t    * tp;
   jvmtiError    rc;
   jlong         threadid;
   jint          numthrds;
   jthread     * thrds = 0;
   Node        * np;

   jint          n;                     // Count of loaded classes
   jclass      * pClass;                // Pointer to array of loaded classes
   jint          i;
   hash_iter_t   iter;

   self_tp->etype  = JVMTI_EVENT_VM_INIT;

   // Enable DataDumpRequest Event
   //(*ge.jvmti)->SetEventNotificationMode(ge.jvmti, JVMTI_ENABLE, JVMTI_EVENT_DATA_DUMP_REQUEST, NULL);

   if ( 0 == ge.caps.can_tag_objects || 0 == gv->tagging )
   {
      gv->useTagging = 0;
   }

//----------------------------------------------------------------------
// Reallocate the thread nodes for all existing threads (SHOULD NOT NEED LOCK)
//----------------------------------------------------------------------

   if ( ge.oldNodeSize )
   {
      hashIter( &ge.htThreads, &iter ); // Iterate thru the thread table

      while ( ( tp = (thread_t *)hashNext( &iter ) ) )
      {
         np        = tp->currT;

         tp->currT = (Node *)thread_node(tp);
         tp->currm = tp->currT;

         freeNodeObjs( np, 1);

         xFree( np, ge.oldNodeSize );
      }
   }
   ge.oldNodeSize            = ge.nodeSize;
   ge.oldSizeRTEventBlock    = ge.sizeRTEventBlock;
   ge.oldOffNodeCounts       = ge.offNodeCounts;
   ge.oldOffTimeStamp        = ge.offTimeStamp;
   ge.oldOffEventBlockAnchor = ge.offEventBlockAnchor;
   ge.oldOffLineNum          = ge.offLineNum;

//----------------------------------------------------------------------
// Pre-define class entry for java/lang/Class
//----------------------------------------------------------------------

   if ( gv->ClassLoadInfo )
   {
      jclass   reflectClass;
      jfieldID fid;
      jclass   primitiveClass;

      reflectClass = (*self_tp->env_id)->FindClass( self_tp->env_id, strJavaLangClass );

      rc = (*ge.jvmti)->SetTag(ge.jvmti, reflectClass, (jlong)1);

      if ( rc != JVMTI_ERROR_NONE )
      {
         ErrVMsgLog("SetTag failed for java/lang/class: rc = %d\n", rc);
         err_exit(0);
      }

      ge.cpJavaLangClass = getClassEntry( self_tp, reflectClass );

      (*self_tp->env_id)->DeleteLocalRef(self_tp->env_id, reflectClass );

//----------------------------------------------------------------------
// Get the primitive classes
//----------------------------------------------------------------------

      OptVMsgLog( "Get Primitive Classes\n" );

      for ( i = 0; i < 9; i++ )
      {
         reflectClass   = (*self_tp->env_id)->FindClass(            self_tp->env_id, reflectClassNames[i] );
         fid            = (*self_tp->env_id)->GetStaticFieldID(     self_tp->env_id, reflectClass, strTYPE, strLJavaLangClass );
         primitiveClass = (*self_tp->env_id)->GetStaticObjectField( self_tp->env_id, reflectClass, fid );

         cp = getClassEntry( self_tp, primitiveClass );

         (*self_tp->env_id)->DeleteLocalRef(self_tp->env_id, primitiveClass );
         (*self_tp->env_id)->DeleteLocalRef(self_tp->env_id, reflectClass );
      }
   }

   if ( 0 == ge.fNeverActive )
   {
      ge.fActive = 1;                   // JPROF ready to process events
   }
   EnableEvents1x(1);

   //-------------------------------------------
   // Generate events we may have missed
   //-------------------------------------------

   if ( gv->JITA2N || gv->CompileLoadInfo || gv->jitinfo )
   {
      rc = (*ge.jvmti)->GenerateEvents(ge.jvmti, JVMTI_EVENT_COMPILED_METHOD_LOAD);

      OptVMsgLog("GenerateEvents(COMPILED_METHOD_LOAD), rc = %d\n", rc);

      rc = (*ge.jvmti)->GenerateEvents(ge.jvmti, JVMTI_EVENT_DYNAMIC_CODE_GENERATED);

      OptVMsgLog("GenerateEvents(DYNAMIC_CODE_GENERATED), rc = %d\n", rc);
   }

   //-------------------------------------------
   // Initialize info for classes already loaded
   //-------------------------------------------

   if ( gv->ClassLoadInfo )
   {
      rc = (*ge.jvmti)->GetLoadedClasses(ge.jvmti, &n, &pClass);

      if (rc != JVMTI_ERROR_NONE)
      {
         ErrVMsgLog("ERROR: GetLoadedClasses, rc = %d\n", rc);
         return;
      }
      else
      {
         OptVMsgLog( "GetLoadedClasses() found %d classes\n", n );
      }

      for ( i = 0; i < n; i++ )
      {
         cp = getClassEntry( self_tp, pClass[i] );

         (*self_tp->env_id)->DeleteLocalRef(self_tp->env_id, pClass[i]);
      }
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)pClass);

      hashStats( &ge.htClasses );
   }

   //--------------------------------------------
   // Initialize info for threads already started
   //--------------------------------------------

   if ( gv->start )
   {
      EnableEvents2x(1);              // the rest in calltree. output mode?
   }

#ifndef _LEXTERN
   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)

   scs_initialize(self_tp->env_id);

   #endif
#endif

   if ( ge.getOSThreadID )
   {
      rc = (*ge.jvmti)->GetAllThreads( ge.jvmti,
                                       &numthrds,
                                       &thrds );
      if ( rc != JVMTI_ERROR_NONE )
      {
         ErrVMsgLog("GetAllThreads, rc = %d\n", rc);
      }
      else
      {
         for ( i = 0; i < numthrds; i++ )
         {
            threadid = 0;

            rc = (ge.getOSThreadID)( ge.jvmti, thrds[i], &threadid );

            if ( rc != JVMTI_ERROR_NONE )
            {
               ErrVMsgLog( "\nGetOSThreadID failed, rc = %d\n", rc );
            }
            else
            {
               tp = lookup_thread( (uint32)threadid );

               if ( tp
                    && 0 == tp->thread )
               {
                  OptVMsgLog( "\nInitializing existing thread %x\n", (uint32)threadid );
                  tp = 0;
               }

               if ( 0 == tp )
               {
                  doThreadStart( ge.jvmti, self_tp->env_id, thrds[i], (int)threadid );
               }
            }
            (*self_tp->env_id)->DeleteLocalRef( self_tp->env_id, thrds[i] );
         }
         (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)thrds);
      }
   }

   //-----------------------------------
   // Start calltree
   //-----------------------------------

   if ( gv->calltree || gv->calltrace || ge.scs)
   {
      if ( gv->start )
      {
         procSockCmd(0,"START");

         if ( ge_objectinfo_ALL == ge.ObjectInfo )
         {
            rc = (*ge.jvmti)->IterateOverHeap( ge.jvmti,
                                               JVMTI_HEAP_OBJECT_EITHER,
                                               cbHeapObject,
                                               0 );   // No user data

            if ( JVMTI_ERROR_NONE != rc )
            {
               ErrVMsgLog("IterateOverHeap: rc = %d\n", rc);
            }
         }
      }
   }
}

void RestartTiJprofFromPlugin()
{
   thread_t *tp = lookup_thread(CurrentThreadID());
   RestartTiJprof(tp);
}

//-----------------------------------------------------------------------
// Common processing to check event thresholds
//-----------------------------------------------------------------------

void checkEventThreshold( thread_t * tp,
                          jobject    object,
                          jclass     objcl,
                          UINT32     events )
{
   jclass     objclass  = objcl;
   object_t * op        = 0;
   class_t  * cp        = 0;

   tp->incr  = 0;
   tp->incr2 = 0;

   if ( gv_rton_Trees == gv->rton || gv->scs_active )   // Perform only if building trees
   {
      if ( gv->selListClasses || gv->scs_event_object_classes )
      {
         if ( 0 == objclass )
         {
            objclass = (*tp->env_id)->GetObjectClass( tp->env_id, object );
         }
         cp = getClassEntry( tp, objclass );   // Always returns non-zero cp

#ifndef _LEXTERN
         if ( gv->DistinguishLockObjects )
         {
            jint       rc;

            if ( JVMTI_ERROR_NONE != ( rc = (*ge.jvmti)->GetTag( ge.jvmti, object, &tp->tagMonitor) ) )
            {
               ErrVMsgLog("GetTag: object = %p, class = %s, rc = %d\n", object, cp->class_name, rc);
               err_exit("GetTag failure");
            }

            if ( 0 == tp->tagMonitor )
            {
               tp->tagMonitor = getNewTag();

               if ( JVMTI_ERROR_NONE != ( rc = (*ge.jvmti)->SetTag( ge.jvmti, object, tp->tagMonitor ) ) )
               {
                  ErrVMsgLog("SetTag: object = %p, class = %s, rc = %d\n", object, cp->class_name, rc);
                  err_exit("SetTag failure");
               }
            }
         }
#endif //_LEXTERN

         if ( 0 == objcl )
         {
            (*tp->env_id)->DeleteLocalRef(tp->env_id, objclass);
         }
         if ( gv->selListClasses )
         {
            if ( cp->flags & class_flags_filtered )
            {
               if ( 0 == ( cp->flags & class_flags_selected ) )
               {
                  return;               // Class previously rejected
               }
               // Class already accepted, continue with this function
            }
            else
            {
               cp->flags |= class_flags_filtered;

               if ( 0 == nxCheckString( gv->selListClasses, cp->class_name ) )
               {
                  MVMsg(gc_mask_ObjectInfo,("Class rejected by selection list: %s\n", cp->class_name));
                  return;
               }
               cp->flags |= class_flags_selected;

               MVMsg(gc_mask_ObjectInfo,("Class accepted by selection list: %s\n", cp->class_name));
            }
         }
      }

      if ( gv->scs_allocations )
      {
         tp->scs_allocs++;

         if ( tp->scs_allocs >= gv->scs_allocations )   // Really just testing "=="
         {
            tp->incr2      = 1;         // tp->scs_allocs / gv->scs_allocations;
            tp->scs_allocs = 0;         // tp->scs_allocs % gv->scs_allocations;
         }
      }
      tp->scs_events += events;

      if ( tp->scs_events >= gv->scs_event_threshold )
      {
         tp->incr          = tp->scs_events / gv->scs_event_threshold;
         tp->scs_events    = tp->scs_events % gv->scs_event_threshold;
      }

      if ( tp->incr || tp->incr2 )
      {
         if ( gv->scs_event_object_classes )
         {
            if ( ge.ObjectInfo || gv->scs_allocBytes )
            {
               op          = insert_object( getIdFromTag( setObjectTag( object ) ) );
               tp->op      = op;
               op->bytes   = events;
               op->cp      = cp;
            }
         }

         tp->mt3            = gv_ee_Other;
         pushThreadCallStack(tp, tp, 0);

         if ( gv->scs_event_object_classes )
         {
            incrEventBlock( tp, tp->tagMonitor, cp, events );
         }
         else if ( gv->scs_allocations )
         {
            tp->currm->bmm[0] += tp->incr2;
            tp->currm->bmm[1] += tp->incr;
         }
         else
         {
            tp->currm->bmm[0] += tp->incr;
         }
      }
   }
}

//-----------------------------------------------------------------------
// Common subroutines for cbVMObjectAlloc and cbInstrumentableObjectAlloc
//-----------------------------------------------------------------------

void doObjectAlloc( thread_t * tp,
                    jvmtiEnv * jvmti,
                    jobject    object,
                    jclass     object_klass,
                    jlong      size)
{
   jlong       tag;
   jlong       tagClass;
   jvmtiError  rc;
   char      * sig;
   jint        isArray;
   class_t   * cp;

   tag      = 0;
   tagClass = 0;
   isArray  = 0;

   if ( gv->useTagging )
   {
      tag = getNewTag();

      rc = (*jvmti)->SetTag(jvmti,object,tag);

      if ( rc != JVMTI_ERROR_NONE )
      {
         ErrVMsgLog("SetTag: object = %p, rc = %d\n", object, rc );
         err_exit("SetTag failure");
      }
      MVMsg(gc_mask_ObjectInfo,
            ("doObjectAlloc: Set new tag = %08"_P64"X\n", tag));

      rc = (*jvmti)->GetTag(jvmti, object_klass, &tagClass);

      if ( rc != JVMTI_ERROR_NONE )
      {
         ErrVMsgLog("GetTag: object_klass = %p, rc = %d\n", object_klass, rc );
         err_exit("GetTag failure");
      }
      
      MVMsg(gc_mask_ObjectInfo, ("doObjectAlloc: object = %p, class = %p\n",
                                 Uint64ToPtr(tag),
                                 Uint64ToPtr(tagClass) ) );

      if ( tagClass )
      {
         cp = insert_class( getIdFromTag(tagClass) );
      }
      else
      {
         OptVMsgLog( "doObjectAlloc: Class not yet tagged, object tag = %p\n",
                     (jint)tag );

         cp = getClassEntry( tp, object_klass );   // Add as a result of an error

         tagClass = PtrToUint64(cp->class_id);
      }

      if ( cp->flags & class_flags_nameless )
      {
         queryClassMethods( tp, cp, object_klass );
      }
      sig = cp->class_name;

      if ( sig && '[' == sig[0] )       // Array object found
      {
         if ( ']' == sig[1] )           // Already converted from [Lxxx to []xxx
         {
            isArray = JVMTI_TYPE_JCLASS;
         }
         else
         {
            tagClass = 0;

            switch ( sig[1] )
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
   }
   else                                 // Can not tag objects
   {
      cp = getClassEntry( tp, object_klass );
   }
   updateInstanceSize( cp, size );

   MVMsg(gc_mask_ObjectInfo,
         ("doObjectAlloc: id = %p, class = %p, size = %8"_P64"d, tag = %08X, tagClass = %08X\n",
          object, object_klass, size, (jint)tag, (jint)tagClass ));

   objectAllocInfo(tp,
                   (jint)size,
                   (jobject)cp,
                   getIdFromTag(tag),
                   isArray);
}

//-----------------------------------------------------------------------
// Subroutines for cbObjectFree
//-----------------------------------------------------------------------

void doObjectFree( thread_t * tp, jlong tag )
{
   object_t  * op;

   if ( gv->scs_allocBytes )            // ge.ObjectInfo is implied by the event
   {
      op = lookup_object( Uint64ToPtr(tag) );

      decrEventBlock( op );

      return;
   }

   objectFreeInfo(getIdFromTag(tag));

   MVMsg(gc_mask_ObjectInfo,("ObjectFree:  tag = %08"_P64"X\n", tag));
}

//-----------------------------------------------------------------------
// Add/Update Event Block information to a node
//-----------------------------------------------------------------------

void updateRTEB( thread_t * tp, RTEventBlock * pRTEB, jlong size )
{
   if ( gv->scs_allocations > 1 || gv->scs_allocBytes > 1 )   // Samples
   {
      if ( tp->incr2 )                  // Alloc threshold crossed
      {
         if ( ge.ObjectInfo )
         {
            pRTEB->liveObjects++;       //  += tp->incr2;
            tp->op->flags |= object_flags_alloc_threshold;

            tp->op->pRTEB = pRTEB;      // Save this for decrEventBlock
         }
         pRTEB->totalObjects++;         // += tp->incr2;
      }

      if ( tp->incr )                   // Bytes threshold crossed
      {
         if ( ge.ObjectInfo )
         {
            pRTEB->liveBytes += tp->incr;
            tp->op->flags    |= object_flags_bytes_threshold;

            if ( tp->incr > ( size / gv->scs_allocBytes ) )
            {
               tp->op->flags |= object_flags_extra_threshold;
            }
            tp->op->pRTEB = pRTEB;      // Save this for decrEventBlock
         }
         pRTEB->totalBytes   += tp->incr;   // Total events => Total Bytes if not monitors
      }
   }
   else if ( ge.ObjectInfo )
   {
      pRTEB->totalObjects++;
      pRTEB->totalBytes  += size;
      pRTEB->liveObjects ++;
      pRTEB->liveBytes   += size;

      tp->op->pRTEB = pRTEB;            // Save this for decrEventBlock
   }
   else if ( gv->scs_allocBytes )
   {
      pRTEB->totalObjects++;
      pRTEB->totalBytes  += size;
   }
   else
   {
      pRTEB->totalEvents += tp->incr;   // Class Load or Monitor events
   }

   if ( gc_mask_ObjectInfo & gc.mask )
   {
      if ( ge.ObjectInfo )
      {
         OptVMsgLog( "incrEventBlock: pRTEB = %p, AO = %d, AB = %d, LO = %d, LB = %d\n",
                     pRTEB,
                     (unsigned int)pRTEB->totalObjects,
                     (unsigned int)pRTEB->totalBytes,
                     (unsigned int)pRTEB->liveObjects,
                     (unsigned int)pRTEB->liveBytes );
      }
      else
      {
         OptVMsgLog( "incrEventBlock: pRTEB = %p, AO = %d, AB = %d\n",
                     pRTEB,
                     (unsigned int)pRTEB->totalObjects,
                     (unsigned int)pRTEB->totalBytes );
      }
   }
}

void incrEventBlock( thread_t * tp,
                     jlong      tagMonitor,
                     class_t *  cp,
                     jlong      size )
{
   RTEventBlock * pRTEB;
   RTEventBlock * poRTEB = 0;

   // gv->scs_event_object_classes implied by being here

   pRTEB = *getEventBlockAnchor( tp->currm );   // Event Block list anchor

   while ( pRTEB )                      // Search for event class on event block list
   {
      if ( cp == pRTEB->cp              // Event class already in list
           && ( 0 == gv->DistinguishLockObjects
                || pRTEB->tagEventObj == tagMonitor ) )
      {
         if ( 0 == gv->scs_event_object_classes )
         {
            pRTEB->totalEvents += tp->incr;
            return;                     // done
         }

         MVMsg( gc_mask_ObjectInfo, ( "incrEventBlock: size = %8"_P64"d, class = %s\n",
                                      size, cp->class_name ) );

         if ( 0 == gv->sobjs
              || ( (UINT64)size == ( pRTEB->totalEvents / pRTEB->totalObjects ) ) )
         {
            updateRTEB( tp, pRTEB, size );
            return;                     // done
         }
      }
      poRTEB = pRTEB;
      pRTEB  = pRTEB->next;
   }

   // No event block found

   gc.allocMsg = "EventBlock";

   pRTEB = (RTEventBlock *)zMalloc( 0, ge.sizeRTEventBlock );   // Not found, allocate a new event block

   if ( pRTEB == NULL )
   {
      err_exit("Out of Memory in incrEventBlock");
   }
   pRTEB->cp = cp;

   if ( poRTEB )
   {
      poRTEB->next = pRTEB;             // attach at end of chain
   }
   else
   {
      *getEventBlockAnchor( tp->currm ) = pRTEB;   // attach as first event block
   }

   updateRTEB( tp, pRTEB, size );

#ifndef _LEXTERN
   if ( gv->DistinguishLockObjects )
   {
      pRTEB->tagEventObj = tagMonitor;
   }
#endif
}

//-----------------------------------------------------------------------
// Remove Event Block information from a node
//-----------------------------------------------------------------------

void decrEventBlock( object_t * op )
{
   RTEventBlock * pRTEB;
   UINT64         bytes;
   int            objs;

   if ( op )
   {
      MVMsg( gc_mask_ObjectInfo, ( "decrEventBlock: op = %p, addr = %p, size = %8"_P64"d, class = %s\n",
                                   op, op->addr, op->bytes, op->cp->class_name ) );

      pRTEB = op->pRTEB;

      if ( pRTEB )                      // Object is associated with an Event Block of some tree node
      {
         if ( gv->scs_allocations > 1 || gv->scs_allocBytes > 1 )
         {
            if ( op->flags & object_flags_alloc_threshold )
            {
               objs = 1;
            }
            else
            {
               objs = 0;
            }

            if ( op->flags & object_flags_bytes_threshold )
            {
               bytes = op->bytes / (UINT64)gv->scs_allocBytes;   // Number of full thresholds

               if ( op->flags & object_flags_extra_threshold )
               {
                  bytes++;
               }
            }
            else
            {
               bytes = 0;
            }
         }
         else
         {
            objs  = 1;
            bytes = op->bytes;
         }

         if ( pRTEB->liveObjects  >= objs
              && pRTEB->liveBytes >= bytes )
         {
            MVMsg( gc_mask_ObjectInfo, ( "decrEventBlock: pRTEB = %p, objs = %d, bytes = %d, objs = %d, bytes = %d\n",
                                         pRTEB, objs, (unsigned int)bytes,
                                         (unsigned int)pRTEB->liveObjects,
                                         (unsigned int)pRTEB->liveBytes ) );

            pRTEB->liveObjects -= objs;
            pRTEB->liveBytes   -= bytes;
         }
         else
         {
            OptVMsgLog( "Invalid object freed: op = %p, addr = %p, class = %s\n",
                        op, op->addr, op->cp->class_name );
         }
      }
      remove_object( op->addr );        // Forget about this object
   }
   else
   {
//    OptVMsgLog( "Attempted to remove an object that was never allocated\n" );
//    ENIO: Maybe we can count these and put out a summary count at the end?
      MVMsg( gc_mask_ObjectInfo, ( "Attempted to remove an object that was never allocated\n" ) );
   }
}

//-----------------------------------------------------------------------
// common prolog for JVMTI Events
//-----------------------------------------------------------------------
thread_t * tie_prolog( JNIEnv    * env_id,
                       jint        etype,
                       char      * xmsg )
{
   thread_t * tp;

   if ( 0 == ge.fActive )
   {
      return(NULL);
   }

   tp = timeJprofEntry();               // get metric w FIXED pathlength

   if ( 0 == tp->env_id )
   {
      tp->env_id = env_id;
   }

#ifndef _LEXTERN
   if ( ge.scs )
   {
      MVMsg(gc_mask_Show_Events, ("SCS:%21s:[%d] tid = %p, cpu = %d, ts = %"_P64"x\n",
                                  xmsg,
                                  etype,
                                  tp->tid,
                                  tp->cpuNum,
                                  tp->timeStamp ));

      tp->x23  = ge.ee[etype];
      tp->mt3  = ge.type[etype];
      gv->ecnt++;

      return(tp);
   }
#endif

   if (ge.reset && ( gv_ee_Reset >= ge.ee[etype] ))
   {
      return(NULL);
   }

   MVMsg(gc_mask_Show_Events, ("%21s:[%d] tid = %p, cpu = %d, ts = %"_P64"x\n",
                               xmsg,
                               etype,
                               tp->tid,
                               tp->cpuNum,
                               tp->timeStamp ));

   tp->fWorking = 1;
   memory_fence();

   instruction_fence();
   if (ge.reset && ( gv_ee_Reset >= ge.ee[etype] ))
   {
      tp->fWorking = 0;
      memory_fence();
      return(NULL);
   }

   if ( 0 == tp->env_id )
   {
      tp->env_id = env_id;              // Ensure JNI environment is correct, use prev if not passed
   }

   //e->event_type = e->event_type & 0xfff; // in range (0-4k)
   tp->etype  = etype;
   tp->x23    = ge.ee[etype];           // Entry/Exit event
   tp->mt3    = ge.type[etype];         // IJCBLN flags

   gv->ecnt++;

   if ( gc.mask & gc_mask_Show_Events )
   {                                    // show all events
      if (gd.gen)
      {                                 // add to log-gen w generic option
         fprintf(gd.gen, " %8d %016"_P64"X %016"_P64"X %4d:%s\n",
                 gv->ecnt, tp->metEntr[0], tp->metDelt[0], etype, ge.evstr[etype]);
      }
   }

   if (gd.gen)
   {
      if (tp->x23 == gv_ee_Reset)
      {                                 // non method events. meh_methods for methods
         genPrint2(tp);              // Event w/o Handler(ehs_) (addr, ascii, ...)
      }
   }

   return(tp);
}


//----------------------------------------------------------------------
// Convert a class signature to a non-volatile class name
//----------------------------------------------------------------------

char * sig2name( char * sig, char * userbuf )
{
   int  i;
   int  j;
   char cnmbuf[MAX_PATH];

   char * buffer;

   if ( userbuf )
   {
      buffer = userbuf;
   }
   else
   {
      buffer = cnmbuf;
   }

#if defined(_CHAR_EBCDIC)
   #define MAX_BRACES 24  // Maximum braces at start of class name

   sig = strcpy( buffer + MAX_BRACES, sig );

   Java2Native( sig );                  // Translate signature to EBCDIC
#endif

   if ( 0 == strcmp( sig, "[Z" ) )
   {
      sig = "BOOLEAN[]";
   }
   else if ( 0 == strcmp( sig, "[B" ) )
   {
      sig = "BYTE[]";
   }
   else if ( 0 == strcmp( sig, "[C" ) )
   {
      sig = "CHAR[]";
   }
   else if ( 0 == strcmp( sig, "[D" ) )
   {
      sig = "DOUBLE[]";
   }
   else if ( 0 == strcmp( sig, "[F" ) )
   {
      sig = "FLOAT[]";
   }
   else if ( 0 == strcmp( sig, "[I" ) )
   {
      sig = "INT[]";
   }
   else if ( 0 == strcmp( sig, "[J" ) )
   {
      sig = "LONG[]";
   }
   else if ( 0 == strcmp( sig, "[S" ) )
   {
      sig = "SHORT[]";
   }
   else                                 // Handle all arrays and classes
   {
      i = 0;
      j = 0;

      while ( '[' == sig[i]             // Change leading "[" to "[]"
#if defined(_CHAR_EBCDIC)
              && i < MAX_BRACES
#endif
            )
      {
         buffer[j++] = '[';
         buffer[j++] = ']';
         i++;
      }
      if ( 'L' == sig[i] )              // Remove "L" and ";" from class signatures
      {
         strcpy( buffer+j, sig+i+1 );
         buffer[ strlen(buffer)-1 ] = 0;
      }
      else
      {
         strcpy( buffer+j, sig+i );
      }

      if ( userbuf )
      {
         sig = buffer;
      }
      else
      {
         sig = xStrdup("ClassSignature",buffer);
      }
   }
   return( sig );
}

/***************************/
char * getClassNameFromTag( thread_t * tp, jlong tag, char * buffer )
{
   jint       dstCnts;
   jobject  * dstObjs;
   jlong    * dstTags;
   jclass     klass;
   char     * sig;
   char     * class_name = 0;
   jvmtiError rc;

   rc = (*ge.jvmti)->GetObjectsWithTags(ge.jvmti,
                                        1,
                                        &tag,
                                        &dstCnts,
                                        &dstObjs,
                                        &dstTags );

   if ( JVMTI_ERROR_NONE == rc )
   {
      if ( dstObjs )
      {
         if ( dstCnts )
         {
            klass = (jobject)*dstObjs;

            rc = (*ge.jvmti)->GetClassSignature(ge.jvmti, klass, &sig, 0);

            if (rc == JVMTI_ERROR_NONE)
            {
               class_name = sig2name(sig,buffer);   // Get name in buffer

               (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)sig);
            }
            else
            {
               OptVMsgLog("GetClassSignature: rc = %d, tag = %08"_P64"X, klass = %p\n", rc, tag, klass);
            }

            if ( 0 == tp )
            {
               tp = insert_thread( CurrentThreadID() );
            }

            if ( tp->env_id )
            {
               (*tp->env_id)->DeleteLocalRef(tp->env_id, *dstObjs);
            }
         }
         else
         {
            OptVMsgLog( "GetObjectsWithTags returned no objects for tag = %X\n", tag );
         }
         (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)dstObjs);
      }
      else
      {
         OptVMsgLog( "GetObjectsWithTags returned no array of objects for tag = %X\n", tag );
      }
      if ( dstTags )
      {
         (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)dstTags);
      }
   }
   else
   {
      OptVMsgLog("GetObjectsWithTags: rc = %d, tag = %08"_P64"X\n", rc, tag);
   }
   return( class_name );
}

/***************************/
void updateInstanceSize( class_t * cp, jlong size )
{
   if ( cp )
   {
      if ( 0 == cp->min_size            // Uninitialized
           || ( (jint)size < cp->min_size   // Or too large
                && (jint)size > 0 ) )
      {
         cp->min_size = (int)size;

         MVMsg(gc_mask_ObjectInfo,
               ("Updated min_size in class table, tag = %08X, size = %8d, sig = %s\n",
                cp->class_id,
                (jint)size,
                cp->class_name ));
      }
   }
}

//----------------------------------------------------------------------
// Get the full method class, name, and signature without requiring ClassLoadInfo
//----------------------------------------------------------------------

void initMethodInfo( thread_t * tp, method_t * mp )
{
   jvmtiError rc;

   jclass     dclass;                   // Class which declared this method
   char     * mname = 0;                // Method Name
   char     * sig   = 0;                // Signature
   jint       modifiers;                // Method modifiers

   if ( 0 == ge.jvmti )
   {
      return;
   }

   dclass = NULL;                       // Class which declared this method

#if defined(_WINDOWS)
   __try
   {
#endif

      rc = (*ge.jvmti)->GetMethodName(ge.jvmti, mp->method_id, &mname, &sig, 0);

#if defined(_WINDOWS)
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      ErrVMsgLog( "Exception in GetMethodName\n");
      rc = 1;
   }
#endif

   if ( rc == JVMTI_ERROR_NONE )
   {
#if defined(_CHAR_EBCDIC)
      Java2Native( mname );             // Translate method name to EBCDIC
      Java2Native( sig );               // Translate signature   to EBCDIC
#endif

      rc = (*ge.jvmti)->GetMethodDeclaringClass(ge.jvmti, mp->method_id, &dclass);

      if ( rc != JVMTI_ERROR_NONE )
      {
         ErrVMsgLog("ERROR: GetMethodDeclaringClass: mid = %x, rc = %d\n", mp->method_id, rc );
      }
      else
      {
         if ( 0 == tp )
         {
            tp = insert_thread( CurrentThreadID() );
         }

         addMethod( getClassEntry( tp, dclass ),
                    mp->method_id,
                    mname,
                    sig );

         if ( ge.ExtendedCallStacks )   // ExtendedCallStacks: try to get the source file name
         {
            if ( 0 == mp->class_entry->source_name )
            {
               mp->class_entry->source_name = get_sourcefilename(dclass);
            }
         }

         if ( tp->env_id )
         {
            (*tp->env_id)->DeleteLocalRef(tp->env_id, dclass);
         }
      }

      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)mname);
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)sig);

      (*ge.jvmti)->GetMethodModifiers(ge.jvmti, mp->method_id, &modifiers);

      if ( modifiers & 8 )              // Method is static (ACC_STATIC)
      {
         mp->flags |= method_flags_static;
      }
      if ( modifiers & 256 )            // Method is native (ACC_NATIVE)
      {
         mp->flags |= method_flags_native;
      }
   }
   else
   {
      if ( mp->method_id )
      {
         ErrVMsgLog("ERROR: GetMethodName: mid = %x, rc = %d\n", mp->method_id, rc );
      }
      mp->method_name = xStrdup( "unknown_method()V", "unknown_method()V" );   // Method Name
   }
}

//-----------------------------------------------------------------------
// Enumerates the methods in a loaded class
//-----------------------------------------------------------------------

void queryClassMethods( thread_t * tp,
                        class_t  * cp,
                        jclass     klass )
{
   jvmtiError  rc;
   jint        mcnt;
   jmethodID * mptrs;

   char      * mnm;
   char      * sig;
   method_t  * mp = NULL;

   int         i;
   int         k;
   jmethodID   mid;
   jint        modifiers;

   int         selected;

   char      * name;
   char        nmbuf[MAX_PATH];         // Also used to fix UNKNOWN_CLASS

   if ( cp->flags >> class_flags_idshift )
   {
      return;                           // Already done
   }

   if ( cp->flags & class_flags_nameless )
   {
      name = getClassNameFromTag( tp, PtrToUint64( cp->class_id ), nmbuf );

      if ( name )
      {
         cp->class_name = xStrdup("Missing class name",name);
         cp->flags     &= ~class_flags_nameless;

         MVMsg(gc_mask_ObjectInfo,
               ("queryClassMethods: Resolved missing class name: tag = %08X = %s\n",
                cp->class_id, cp->class_name));
      }
      else
      {
         OptVMsgLog("queryClassMethods: Unable to resolve class name for tag = %08X\n", cp->class_id );
      }
   }

   if ( gd.syms || ge.ExtendedCallStacks )
   {
      // ExtendedCallStacks: try to get the source file name
      if ( 0 == cp->source_name )
      {
         cp->source_name = get_sourcefilename(klass);
      }
   }

   if ( 0 == gd.syms
        && 0 == gv->selListMethods )
   {
      enter_lock(ClassLock, CLASSLOCK);

      k = ++ge.seqnoClass;

      cp->flags |= ( k << class_flags_idshift );   // Generated classID

      leave_lock(ClassLock, CLASSLOCK);

      return;
   }

   // class methods
   rc = (*ge.jvmti)->GetClassMethods(ge.jvmti, klass, &mcnt, &mptrs);

   if (rc != JVMTI_ERROR_NONE)
   {
      OptVMsgLog("GetClassMethods failed: rc = %d, class = <%s>\n", rc, cp->class_name);
      return;
   }
   enter_lock(ClassLock, CLASSLOCK);

   k = ++ge.seqnoClass;

   cp->flags |= ( k << class_flags_idshift );   // Generated classID

   if ( gd.syms )
   {
#if defined(_AIX)
      struct timeval    t;

      gettimeofday(&t,NULL);
#endif

      WriteFlush( gd.syms, "Class_load: %19.9f %08"_P64"x %08"_P64"x %d %s %s\n",
#if defined(_AIX)
                  (double)t.tv_sec + ((double)t.tv_usec/1000000),
#else
                  (double)tp->timeStamp/1000000,
#endif
                  PtrToUint64(tp->env_id),
                  PtrToUint64(cp->class_id),
                  mcnt,
                  cp->class_name,
                  cp->source_name ? cp->source_name : "NONE" );
   }

   for (i = 0; i < mcnt; i++)
   {
      mid = mptrs[i];

      rc = (*ge.jvmti)->GetMethodName(ge.jvmti, mid, &mnm, &sig, 0);

      if (rc != JVMTI_ERROR_NONE)
      {
         ErrVMsgLog("GetMethodName failed: rc = %d, mid = %d, class = %s\n",
                    rc, i, cp->class_name );

         (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)mptrs);

         leave_lock(ClassLock, CLASSLOCK);
         err_exit( 0 );
      }

      Force2Native(mnm);             // Jinsight (above) requires ASCII, not EBCDIC
      Force2Native(sig);

      if ( gd.syms )
      {
         WriteFlush( gd.syms, "%08"_P64"x m %08"_P64"x %08"_P64"x %s %s\n",
                     PtrToUint64(mid),
                     PtrToUint64(tp->env_id),
                     PtrToUint64(cp->class_id),
                     mnm,
                     sig );
      }

      selected = 2;                     // Default selection

      if ( gv->selListMethods )
      {
         if ( ge.setVmAndCompilingControlOptions
              && ge.setMethodSelectiveEntryExitNotify )
         {
            sprintf(nmbuf, "%s.%s%s", cp->class_name, mnm, sig );

            selected = nxCheckString( gv->selListMethods, nmbuf );
         }
      }

      if ( selected )                   // Selected by default or by list
      {
         mp = addMethod( cp, mid, mnm, sig );   // Converts to ASCII

         (*ge.jvmti)->GetMethodModifiers(ge.jvmti, mid, &modifiers);

         if ( modifiers & 8 )           // Method is static (ACC_STATIC)
         {
            mp->flags |= method_flags_static;
         }
         if ( modifiers & 256 )         // Method is static (ACC_NATIVE)
         {
            mp->flags |= method_flags_native;
         }

         if ( 1 == selected )           // Selected by list
         {
            OptVMsgLog("Method selected: %s\n",nmbuf);

            (ge.setMethodSelectiveEntryExitNotify)(ge.jvmti,mid);
         }
      }

      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)mnm);
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)sig);
   }
   (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)mptrs);

   leave_lock(ClassLock, CLASSLOCK);
}

//-----------------------------------------------------------------------
// Common subroutines for cbCompiledMethodLoad and cbDynamicCodeGenerated
//-----------------------------------------------------------------------

/******************************/
void jmLoadCommon( jmethodID mid,
                   char * caddr,        // Code address
                   int    size,         // Code size
                   int    flag,         // 2:mmi
                   char * c,            // Class
                   char * m,            // Method
                   char * s,            // Signature
                   char * cnm,          // Alternate name
                   char * pnm)          // .
{
   UINT64 ctime;
   int    tlo, thi;

   int    i;
   char * p;
   char * jbuf;
   int    jbufsize;
   int    len;
   char * endAddr = caddr + size;

   if (size == 0)
   {
      WriteFlush(gc.msg, " JitMethodLoad - 0 size (%s%s%s%s%s) at %p\n", c, pnm, m, s, cnm, caddr);
      return;                           // error message
   }

   MVMsg(gc_mask_Show_Methods,
         (" ==> jmLoadCommon: %s%s%s%s%s, addr = %p, size = %x\n",
          c, pnm, m, s, cnm, caddr, size ));

   flag |= gv->jinsts;                  // 0 1 2 3 (mmi=2, gv->insts=1)

   ctime = get_timestamp();

#if defined(_ZOS)
   ctime = ctime >> 13;                 // For compatibility with application Tprof
#endif

   thi = (int)(ctime >> 32);
   tlo = (int)(ctime & 0xffffffff);

   if ( gd.syms )
   {
      if ( gv->jinsts )                 // FIX ME: Atomically include the instructions
      {
         uint32_t * pInsts = (uint32_t *)caddr;

         jbufsize = 80 + (int)strlen( m )   // Size of A2N record (padded from 8 to 16 digits)
                    + 7 + 9             // Size of "Instr:\n" + one "\nxxxxxxxx" leader
                    + 9 * ( size >> 2 ) // " XXXXXXXX" for each instruction
                    + 9 * ( size >> 5 );   // "\nxxxxxxxx" offset for each group of 8 instructions (padded from 6 to 8 digits)

         jbuf = (char *)xMalloc( "JBUF", jbufsize );

         len = sprintf( jbuf,
                        "%08"_P64"x j %08"_P64"x %08"_P64"x %08x %s\nInstr:",
                        PtrToUint64(caddr),
                        PtrToUint64((char *)ge.jvmti),
                        PtrToUint64(mid),
                        size,
                        m );

         p = jbuf + len;

         for ( i = 0; i < size; i += 4 )
         {
            if ( i & 0x001F )
            {
               len = sprintf( p, " %08x", *pInsts++ );
            }
            else
            {
               len = sprintf( p, "\n%06x %08x", i, *pInsts++ );
            }
            p += len;
         }
         strcat( p, "\n" );

         WriteFlush( gd.syms, jbuf );

         xFree( jbuf, jbufsize );
      }
      else
      {
         WriteFlush( gd.syms, "%08"_P64"x j %08"_P64"x %08"_P64"x %08x %s\n",
                     PtrToUint64(caddr),
                     PtrToUint64((char *)ge.jvmti),
                     PtrToUint64(mid),
                     size,
                     m );
      }
   }

   if (gv->JITA2N)
   {                                    // log-jita2n File
      // write JITA2N trace Hook - IA NUMA clock skew
#ifdef HAVE_TraceGetIntervalId
      {
         int trcid, taddr, rc;

         trcid = TraceGetIntervalId();
         if (trcid != ge.swtraceSeqNo)
         {
            ge.swtraceSeqNo = trcid;
            WriteFlush(gd.a2n, " SeqNo %d\n", ge.swtraceSeqNo);
         }

         taddr = (int)(0xffffffff & PtrToUint32(caddr));

         // Maj Min #ints #strs
         // TODO: TraceHook Param
         //rc = TraceHook(0x19, 0x41, 5, 0, trcid, ge.pid, tlo, thi, taddr);
         rc = -1;
         if (rc != 0)
         {
            static once = 0;

            if (once == 0)
            {
               once = 1;
               OptVMsg(" Can't write jitted method sync hook (0x19/0x41). rc = %d\n", rc);
            }
         }
      }
#endif

#ifndef _LEXTERN
      if ( ge.scs && mid )
      {
         method_t * mp = insert_method( mid, 0 );

         if ( endAddr == mp->code_address )   // New area preceeds previous area
         {
            mp->code_length += size;    // Merge the two areas
         }
         else
         {
            mp->code_length  = size;    // Replace the old area
         }
         mp->code_address = caddr;
      }


      if ( 0 == ge.cant_use_a2n )       // A2N is available
      {
#if defined(_WINDOWS)
         scs_a2n_send_jitted_method(tls_index, (void *)caddr, size, c, m, s, cnm);
#elif defined(_LINUX) || defined(_AIX)
         scs_a2n_send_jitted_method(0, (void *)caddr, size, c, m, s, cnm);
#endif
      }
#endif

      if ( gv->jinsts )                 // Include bytes of code in log-jita2n
      {
         if ( 0 == caddr || 0 == size )
         {
            WriteFlush(gd.a2n, " %08x %08x %p %4x %d %s%s%s%s%s\n",   // Indicate no following data
                       thi, tlo, caddr, size, 0, c, pnm, m, s, cnm);
         }
         else
         {
            char * addr = caddr;
            int k, khi, klo;

            jbufsize = 2 * size + size / 32 + 2048;

            // write jitcode atomically with other jita2n data
            jbuf = xMalloc( "jitLoadEvent", jbufsize );

            len = sprintf(jbuf,
                          " %08x %08x %p %4x %d %s%s%s%s%s\n",
                          thi, tlo, caddr, size, flag, c, pnm, m, s, cnm);

            p = jbuf + len;

            for (i = 1; i <= size; i++)
            {
               k = *(unsigned char *)addr;
               khi = k >> 4;            // 0 - 15
               klo = k & 0xf;           // 0 - 15

               if (khi <= 9) *p++ = (char)('0' + khi);
               else          *p++ = (char)('a' + khi - 10);

               if (klo <= 9) *p++ = (char)('0' + klo);
               else          *p++ = (char)('a' + klo - 10);

               addr++;
               len += 2;
               if (0 == i % 32)
               {
                  *p++ = '\n';
                  len++;
               }
            }

            if (0 != size % 32)
            {
               *p = '\n';
               len++;
            }

            fwrite((void *)jbuf, 1, len, gd.a2n);
            fflush(gd.a2n);
            xFree( jbuf, jbufsize );
         }
      }
      else                              // Write only the Addr-to-Name record
      {
         WriteFlush(gd.a2n, " %08x %08x %p %4x %d %s%s%s%s%s\n",
                    thi, tlo, caddr, size, flag, c, pnm, m, s, cnm);
      }
   }
   MVMsg(gc_mask_Show_Methods,(" <== jmLoadCommon\n"));
}


//-----------------------------------------------------------------------
// Helpers for Object Allocation Information
//-----------------------------------------------------------------------

// alloc/live objs/bytes
struct _gcdata
{
   int aobjs;                           // alloc
   int abytes;
   int fobjs;                           // free
   int fbytes;
   int fdobjs;                          // dead
   int fdbytes;
   int fnobjs;                          // ?

   int nxgc_obj;
   int nxgc_bytes;
};
struct _gcdata GCD = {0};

void objectAllocInfo(thread_t *tp,
                     jint size,
                     jobject class_id,
                     jobject obj_id,
                     jint is_array)
{
   INT32 sz;
   UINT32 sz2;
   object_t * op;
   class_t *cp = NULL;
   void *addr  = NULL;

   MVMsg(gc_mask_ObjectInfo,("\n>> objectAllocInfo\n"));

   if ( 0 == gv->scs_allocBytes)   // Special processing for SCS_ALLOCATED_BYTES
   {
      if ( gv_rton_Trees != gv->rton ) return;   // trees only

      if ( 0 == gv->addbase ) return;   // gv->addbase applies to Objects also
   }

   if ( 0 == gv->trees )
   {
      return;                           // Don't allocate tree nodes
   }

   if ( (UINT32)size < gv->minAlloc )
   {
      return;                        // Ignore allocations below the specified threshold
   }
   cp = (class_t *)class_id;   // JVMTI code stores class_t* in class_id

   if ( !cp )
   {
      if ( 0 == gv->havePrimArrays ) // Primitive array classes need to be declared
      {
         cp             = insert_class( (jobject)JVMTI_TYPE_JBOOLEAN );
         cp->class_name = xStrdup( "PrimArray", "BOOLEAN[]" );

         cp             = insert_class( (jobject)JVMTI_TYPE_JBYTE );
         cp->class_name = xStrdup( "PrimArray", "BYTE[]" );

         cp             = insert_class( (jobject)JVMTI_TYPE_JCHAR );
         cp->class_name = xStrdup( "PrimArray", "CHAR[]" );

         cp             = insert_class( (jobject)JVMTI_TYPE_JSHORT );
         cp->class_name = xStrdup( "PrimArray", "SHORT[]" );

         cp             = insert_class( (jobject)JVMTI_TYPE_JINT );
         cp->class_name = xStrdup( "PrimArray", "INT[]" );

         cp             = insert_class( (jobject)JVMTI_TYPE_JLONG );
         cp->class_name = xStrdup( "PrimArray", "LONG[]" );

         cp             = insert_class( (jobject)JVMTI_TYPE_JFLOAT );
         cp->class_name = xStrdup( "PrimArray", "FLOAT[]" );

         cp             = insert_class( (jobject)JVMTI_TYPE_JDOUBLE );
         cp->class_name = xStrdup( "PrimArray", "DOUBLE[]" );

         gv->havePrimArrays = 1;
      }
      cp = lookup_class( Uint32ToPtr(is_array) );
   }

   if ( 0 == cp )
   {
      OptVMsgLog("Class can not be identified: object = %p\n",
                 (void *)obj_id );
      return;
   }

   if ( gv->selListClasses )
   {
      if ( cp->flags & class_flags_filtered )
      {
         if ( 0 == ( cp->flags & class_flags_selected ) )
         {
            return;                  // Class previously rejected
         }
         // Class already accepted, continue with this function
      }
      else
      {
         if ( gv->selListClasses )
         {
            cp->flags |= class_flags_filtered;

            if ( 0 == nxCheckString( gv->selListClasses, cp->class_name ) )
            {
               MVMsg(gc_mask_ObjectInfo,("Class rejected by selection list: %s\n", cp->class_name));
               return;
            }
            cp->flags |= class_flags_selected;

            MVMsg(gc_mask_ObjectInfo,("Class accepted by selection list: %s\n", cp->class_name));
         }
      }
   }

   addr = obj_id;     // FIX ME, must be 64 bits

   sz   = (INT32)size;

   // start FEL changes
   if (ge.JVMversion != JVM_J9)
   {
      sz2  = (sz + 12 + 7) & 0xfffffff8;   // use round up for SUN/SOV
   }
   else
   {
      sz2 = sz;                      // use real size for J9
   }
   // end FEL changes

   tp->abytes += sz2;

   // object hash table
   op = insert_object(addr);

   op->bytes   = sz2;
   op->cp      = cp;

   tp->op      = op;

   MVMsg(gc_mask_ObjectInfo,
         ("objectAllocInfo: tag = %p, size = %8d, class = %s\n",
          addr, sz2, cp->class_name ));

   if ( gv->trees )
   {
      tp->incr  = 0;
      tp->incr2 = 0;

      incrEventBlock( tp, 0, op->cp, op->bytes );   // Update object recs
   }

   // verboseGC cross check
   // total allocs
   // FIX ME: NOT THREAD SAFE - should consider fixing
   gv->tobjs++;
   gv->tbytes += sz2;
   // live
   gv->lobjs++;
   gv->lbytes += sz2;
   // 1-GC allocs
   GCD.aobjs++;
   GCD.abytes += sz2;

   if ( gv->fgcThreshold )
   {
      if ( ge.onload )
      {
         if ( gv->fgcCountdown > sz2 )
         {
            gv->fgcCountdown -= sz2;
         }
         else
         {
            gv->fgcCountdown  = gv->fgcThreshold;   // Start counting again

            (*ge.jvmti)->ForceGarbageCollection( ge.jvmti );
         }
      }
   }
}

void objectFreeInfo(jobject obj_id)
{
   INT32 sz;
   object_t * op;
   void *addr  = NULL;

   MVMsg(gc_mask_ObjectInfo,("\n>> objectFreeInfo\n"));

   if ( 0 == gv->scs_allocBytes)   // Special processing for SCS_ALLOCATED_BYTES
   {
      if ( gv_rton_Trees != gv->rton ) return;   // trees only

      if ( 0 == gv->addbase ) return;   // gv->addbase applies to Objects also
   }

   if ( 0 == gv->trees )
   {
      return;                           // Don't allocate tree nodes
   }

   addr = obj_id;
   op   = lookup_object(addr);       // object ptr

   if ( op )
   {
      sz   = (int) op->bytes;

      if ( op->objectID )            // Live object
      {
         if ( gv->trees )
         {
            decrEventBlock(op);
         }

         // GC validation
         gv->lobjs--;                // live
         gv->lbytes -= sz;
         GCD.fobjs++;                // 1-GC frees
         GCD.fbytes += sz;
      }
      else
      {
         // freeing dead obj ???
         GCD.fdobjs++;
         GCD.fdbytes += sz;
      }
      remove_object(addr);           // free object_t
   }
   else
   {
      GCD.fnobjs++;                  // free non object
   }
}

void GCInfo(thread_t *tp,
            jlong used_objects,
            jlong used_object_space,
            jlong total_object_space)
{
   UINT64 tabytes = 0;

   if ( gc.verbose & gc_verbose_logmsg )   // Optional messages to logmsg, if possible
   {
      OptVMsg(" GC_Finish:");
      OptVMsg("   no_objs   %" _P64 "d"  , used_objects);
      OptVMsg("   obj_bytes %" _P64 "d"  , used_object_space);
      OptVMsg("   tot_bytes %" _P64 "d\n", total_object_space);

      if ( ge.ObjectInfo )
      {
         thread_t  * tp;
         hash_iter_t iter;

         OptVMsg("   tot free dead objs  %8d\n", GCD.fdobjs);
         OptVMsg("   tot free dead bytes %8d\n", GCD.fdbytes);
         OptVMsg("   tot free non objs   %8d\n", GCD.fnobjs);

         OptVMsg("AO: %10d, FO: %10d, TO: %10d, LO: %10d\n",
                 GCD.aobjs, GCD.fobjs, gv->tobjs, gv->lobjs);
         OptVMsg("AB: %10d, FB: %10d, TB: %10d, LB: %10d\n",
                 GCD.abytes, GCD.fbytes, gv->tbytes, gv->lbytes);

         OptVMsg("\n");

         // dump alloc bytes by thread

         hashIter( &ge.htThreads, &iter );   // Iterate thru the thread table

         while ( ( tp = (thread_t *)hashNext( &iter ) ) )
         {
            UINT64 abytes = 0;

            if (tp->currT != NULL)
            {
               abytes = tp->abytes;

               if (abytes > 0)
               {
                  tabytes += abytes;

                  OptVMsg(" alloc %12" _P64 "d tid %d tnm %s\n",
                          abytes, tp->tid, tp->thread_name);
               }
            }
         }
         OptVMsg(" totalloc %12" _P64 "d\n", tabytes);

         GCD.aobjs = GCD.abytes = 0; // per GC allocs
         GCD.fobjs = GCD.fbytes = 0; // per GC bytes
      }
   }

   if ( gv->GCInfo
        && total_object_space )
   {
      gv->megabytes = (int)( ( used_object_space + (jlong)0x0FFFFF ) >> 20 );

      gv->heapUsage = (int)( ( used_object_space * 100 )
                             / total_object_space );

      if ( gc.verbose )
      {
         fprintf( stderr,
                  "\r\t\t\t\t\tGC#%d:\tHeap %2d percent full, %d MB    \r",
                  (int)ge.numberGCs,
                  gv->heapUsage,
                  gv->megabytes );
      }

      gv->prevMegabytes = gv->megabytes;
      gv->prevHeapUsage = gv->heapUsage;

      if ( gv->flushAfterGC )
      {
         gv->timeStampFlush = tp->timeStamp;
         TreeOut();
      }
   }
}


//-----------------------------------------------------------------------
// JLM
//-----------------------------------------------------------------------
void tiJLMdump()
{
   void     ** q;
   void      * p = 0;
   int         rc;

#ifndef _LEXTERN
   if ( ge.dumpJlmStats )
   {
      rc = (*ge.dumpJlmStats)(ge.jvmti, &p, ( gv->DistinguishLockObjects ? 1 : 0 ) );   // COM_IBM_JLM_DUMP_FORMAT_TAGS

      if ( 0 == p )
      {
         OptVMsg( "dumpJlmStats, p = %p, rc = %d\n", p, rc );
      }
   }
   else
#endif

      if ( ge.dumpVMLockMonitor )
   {
      rc = (*ge.dumpVMLockMonitor)(ge.jvmti, &p);

      if ( 0 == p )
      {
         OptVMsg( "dumpVMLockMonitor, p = %p, rc = %d\n", p, rc );
      }
   }

   if ( p )
   {
      q = (void **)p;

      OptVMsg( "JLM Dump, p = %p, [%p - %p], rc = %d\n", p, q[0], q[1], rc );

      monitor_dump_event( (char *)q[0], (char *)q[1] );
   }
}
