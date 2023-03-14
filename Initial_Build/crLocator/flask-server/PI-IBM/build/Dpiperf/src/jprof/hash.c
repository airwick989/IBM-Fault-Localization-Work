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

#include "jprof.h"

///============================================================================
/// General Hashing:
///
/// All hash lookups are done without locking, because all new entries are 
/// added to the head of the list for that bucket.  If the entry is not found 
/// and an insertion is needed, a second lookup is done after the lock has 
/// been acquired.
///============================================================================

/******************************/
// Class Block Section
/******************************/

void * alloc_class()
{
   class_t * cp = (class_t *)zMalloc( 0, ge.htClasses.itemsize );

   if ( cp )
   {
      cp->flags      = class_flags_nameless;
      cp->class_name = "UNKNOWN_CLASS";
   }
   return( cp );
}

void free_class( void * p )
{
   class_t * cp = (class_t *)p;

   MVMsg( gc_mask_Malloc_Free, ("Free class: tag = %d\n",PtrToUint32(cp->class_id)) );

   if ( 0 == ( cp->flags & class_flags_nameless ) )
   {
      xFreeStr( cp->class_name );
   }
   if ( ge.ExtendedCallStacks )
   {
      if ( cp->source_name )
      {
         xFreeStr( cp->source_name );
      }
   }
   xFree( cp, gv->sizeClassBlock );
}

void class_table_free(void)
{
   MVMsg( gc_mask_Malloc_Free, ("Free the Class Table\n") );

   enter_lock(ClassLock, CLASSLOCK);

   hashFree( &ge.htClasses );

   leave_lock(ClassLock, CLASSLOCK);
}

/******************************/
class_t * lookup_class(jobject class_id)
{
   return( (class_t *) hashLookup(&ge.htClasses, class_id) );
}

/******************************/
class_t * insert_class(jobject cb)
{
   class_t * cp;

   if ( 0 == ge.htClasses.bucks )
   {
      enter_lock(ClassLock, CLASSLOCK);

      if ( 0 == ge.htClasses.bucks )
      {
         hashInit(&ge.htClasses, ge.chashb, gv->sizeClassBlock, "ClassTable", &alloc_class, &free_class);
      }
      leave_lock(ClassLock, CLASSLOCK);
   }

   cp = hashLookup(&ge.htClasses, cb);

   if (cp == NULL)
   {
      enter_lock(ClassLock, CLASSLOCK);

      cp = hashLookup(&ge.htClasses, cb);

      if (cp == NULL)
      {
         gc.allocMsg = "ClassBlock";

         cp = (class_t *)hashAdd(&ge.htClasses, (void *)cb);

         if (cp == NULL)
         {
            leave_lock(ClassLock, CLASSLOCK);
            err_exit("table_put in insert_class");
         }
      }
      leave_lock(ClassLock, CLASSLOCK);
   }
   return cp;
}

/******************************/
// Method Section
/******************************/

void * alloc_method()
{
   method_t * mp = (method_t *)zMalloc( 0, ge.htMethods.itemsize );

   if ( mp )
   {
      mp->flags       = method_flags_nameless;
      mp->method_name = "UNKNOWN_METHOD";
   }
   return( mp );
}

void free_method( void * p )
{
   method_t * mp = (method_t *)p;

   if ( 0 == ( mp->flags & method_flags_nameless ) )
   {
      xFreeStr( mp->method_name );
   }

   xFree( mp, ge.htMethods.itemsize );
}

void method_table_free(void)
{
   MVMsg( gc_mask_Malloc_Free, ("Free the Method Table\n") );

   enter_lock(MethodLock, METHODLOCK);

   hashFree( &ge.htMethods );

   leave_lock(MethodLock, METHODLOCK);
}

/******************************/
method_t * lookup_method( jmethodID mid, char * name )
{
   hash_t * table = &ge.htMethods;
   void * key;
   int    i;
   UINT32 hv;

   method_t * mp;

   if ( mid )
   {
      key = mid;
   }
   else
   {
      hv = 0;
      i  = 0;

      while ( name[i] )
      {
         hv = name[i++] + 31 * hv;      // Hash the name to create a method id
      }
      key = Uint32ToPtr( hv );
   }

   if ( table->bucks )
   {
      mp = (method_t *) table->buckv[ PtrToUint32(key) % table->bucks ];

      while ( mp )
      {
         if ( key == mp->method_id )
         {
            if ( method_flags_nonjava & mp->flags )   // Non-java method
            {
               if ( name )
               {
                  if ( 0 == strcmp( name, mp->method_name ) )   // method_id may not be unique for non-Java methods
                  {
                     return( mp );
                  }
               }
            }
            else                        // Java method
            {
               if ( mid )               // method_id is unique for Java methods
               {
                  return( mp );
               }
            }
         }
         mp = mp->nextInBucket;
      }
   }
   return( 0 ); 
}

/******************************/
void getMethodName( method_t * mp, char ** c, char ** n )
{
   char * cnm = "unknown_class";

   if ( 0 == mp )
   {
      err_exit( "getMethodName: mp is 0" );
   }

   if ( method_flags_nonjava & mp->flags )
   {
      cnm = "Function";
   }
   else
   {
      if ( mp->class_entry && mp->class_entry->class_name )
      {
         cnm = mp->class_entry->class_name;
      }
   }

   if (c) *c = cnm;
   if (n) *n = mp->method_name;

   return;
}

/******************************/
jobject method_class_id(jmethodID mid)
{
   method_t * mp;

   mp = lookup_method(mid,0);

   if (mp)
   {
      return(mp->class_entry->class_id);
   }
   return((jobject)NULL);
}

///----------------------------------------------------------------------------
/// insert_method finds a method entry, adding it if it does not already 
/// exist.  The method can be found by a unique method id (mid) or a name 
/// which generates a potentially non-unique hash code.  One of the parameters 
/// is always 0.  
///----------------------------------------------------------------------------

method_t * insert_method( jmethodID mid, char * name )
{
   method_t * mp;
   int        size;

   if ( 0 == ge.htMethods.bucks )
   {
      enter_lock(MethodLock, METHODLOCK);

      if ( 0 == ge.htMethods.bucks )
      {
#ifndef _LEXTERN
         if ( ge.scs )
         {
            size = sizeof(method_t);
         }
         else
#endif
         {
            size = offsetof( method_t, code_length );
         }
         hashInit(&ge.htMethods, ge.mhashb, size, "MethodTable", &alloc_method, &free_method);
      }
      leave_lock(MethodLock, METHODLOCK);
   }

   mp = lookup_method( mid, name );

   if ( 0 == mp )
   {
      enter_lock(MethodLock, METHODLOCK);

      mp = lookup_method( mid, name );

      if ( mp )
      {
         leave_lock(MethodLock, METHODLOCK);
      }
      else
      {
         gc.allocMsg = "MethodBlock";

         if ( mid )
         {
            mp = (method_t *)hashAdd(&ge.htMethods, (void *)mid);   // Creates empty structure
         }
         else
         {
            int    i;
            UINT32 hv;

            hv = 0;
            i  = 0;

            while ( name[i] )
            {
               hv = name[i++] + 31 * hv;   // Hash the name to create a method id
            }
            mp = (method_t *)hashAdd(&ge.htMethods, Uint32ToPtr( hv ));   // Creates empty structure
         }

         if ( 0 == mp )
         {
            leave_lock(MethodLock, METHODLOCK);
            err_exit("table_put in insert_method");
         }

         if ( 0 == mid )
         {
            mp->method_name = xStrdup( "nonjava", name );
            mp->flags       = method_flags_nonjava;
         }

         leave_lock(MethodLock, METHODLOCK);

         if ( mid )
         {
            initMethodInfo( 0, mp );    // Get method information without requiring ClassLoadInfo // Lock needed?
         }
      }
   }
   return( mp );
}

///----------------------------------------------------------------------------
/// Thread Hash Table:
///
/// Access to the thread table must never be done under a lock, since this is 
/// the most time critical operation in JPROF.  Caching of the most recent 
/// thread id and entry is done to further improve performance.  The cache is 
/// reset whenever the thread table changes.
///
/// When threads die, they are marked as invalid.  Once a thread id is reused, 
/// the dead entry can be safely moved from its hash bucket to the dead list, 
/// because the only the new valid entry will be found by the search.  Since 
/// insert_thread is always used instead of lookup_thread, the worst that can 
/// happen during a removal is that the search will hit the end of the dead 
/// list and the search will restart at the beginning of the bucket under a 
/// lock.  Once profiling has ended and the thread information has been 
/// reported in the log-rt file, entries on the dead list can be deleted.  
///----------------------------------------------------------------------------

#if defined(_WINDOWS)
/******************************/
void close_threads(void)
{
   int        i;
   thread_t * tp;

   for (i = 0; i < (int)ge.htThreads.bucks; i++)
   {
      tp = (thread_t *) ge.htThreads.buckv[i];

      while (tp)
      {
         if ( tp->fValid && tp->handle )
         {
            CloseHandle(tp->handle);
         }
         freeFrameBuffer(tp);

         tp = tp->nextInBucket;
      }
   }
   return;
}
#endif

/******************************/
void threadDataReset(thread_t * tp)
{
   int    metric;

   tp->mt1   = gv_type_Other;
   tp->x12   = gv_ee_Other;
   tp->mt2   = gv_type_Other;
   tp->x23   = gv_ee_Other;
   tp->mt3   = gv_type_Other;

   if ( tp->metEntr )
   {
      for (metric = 0; metric <= gv->physmets; metric++)
      {
         tp->metEntr[metric] = 0;
         tp->metExit[metric] = 0;
         tp->metDelt[metric] = 0;
         tp->metAppl[metric] = 0;
         tp->metInst[metric] = 0;
         tp->metJprf[metric] = 0;
         tp->metSave[metric] = 0;
         tp->metGC  [metric] = 0;
      }
   }

   tp->mtype_depth     = 0;             // Start the mtype_stack again
   tp->depth           = 0;             // Start the depth again

   tp->progressCPU     = 0;
   tp->progressCycles  = 0;
   tp->fValidCallStack = 0;
}

void flushDelayedCleanup()
{
   thread_t * tp;
   thread_t * prev_tp;
   int        ind;

   enter_lock(ThreadLock, THREADLOCK);

   for ( ind = 0; ind < (int)ge.htThreads.bucks; ind++ )
   {
      prev_tp = (thread_t *)&ge.htThreads.buckv[ind];   // Use bucket pointer as virtual thread_t pointer
      tp      = (thread_t *)ge.htThreads.buckv[ind];

      while ( tp )                      // Guarantee that thread is still not in table
      {
         if ( tp->fDelayed )
         {
            OptVMsg( "Thread %s being moved to dead list after delay\n", tp->thread_name );

            prev_tp->nextInBucket = tp->nextInBucket;   // Remove old entry from bucket

            tp->nextInBucket      = ge.listDeadThreads;
            ge.listDeadThreads    = tp; // Add entry to head of list of dead threads

            tp      = prev_tp->nextInBucket;
         }
         else
         {
            prev_tp = tp;               // Move forward to next candidate entry
            tp      = tp->nextInBucket;
         }
      }
   }
   leave_lock(ThreadLock, THREADLOCK);
}

/******************************/
thread_t * lookup_thread(UINT32 tid)
{
   thread_t * tp = 0;
   int        ind;

   if ( ge.htThreads.bucks )
   {
      ind = tid % ge.htThreads.bucks;
      tp  = (thread_t *)ge.htThreads.buckv[ind];

      while (tp)                        // See if thread already in table
      {
         if ( Uint32ToPtr(tid) == tp->tid )
         {
            if ( tp->fValid )
            {
               tp->mp = 0;              // Mark method pointer not acquired
               break;
            }
         }
         tp = tp->nextInBucket;
      }
   }
   return( tp );
}

/******************************/
thread_t * insert_thread(UINT32 tid)
{
   thread_t * tp;
   thread_t * prev_tp;
   char     * p;
   int        size;
   int        ind;
   char       buffer[2048];

   if ( 0 == ge.htThreads.bucks )
   {
      enter_lock(ThreadLock, THREADLOCK);

      if ( 0 == ge.htThreads.bucks )
      {
         hashInit(&ge.htThreads, ge.thashb, sizeof(thread_t), "ThreadTable", 0, 0);
      }
      leave_lock(ThreadLock, THREADLOCK);
   }

   ind = tid % ge.htThreads.bucks;
   tp  = (thread_t *)ge.htThreads.buckv[ind];

   while (tp)                           // See if thread already in table
   {
      if ( Uint32ToPtr(tid) == tp->tid )
      {
         if ( tp->fValid )
         {
            tp->mp = 0;                 // Mark method pointer not acquired
            return(tp);
         }
      }
      tp = tp->nextInBucket;
   }

   enter_lock(ThreadLock, THREADLOCK);

   prev_tp = (thread_t *)&ge.htThreads.buckv[ind];   // Use bucket pointer as virtual thread_t pointer
   tp      = (thread_t *)ge.htThreads.buckv[ind];

   while ( tp )                         // Guarantee that thread is still not in table
   {
      if ( Uint32ToPtr(tid) == tp->tid )
      {
         if ( tp->fValid )
         {
            leave_lock(ThreadLock, THREADLOCK);

            tp->mp = 0;                 // Mark method pointer not acquired
            return(tp);
         }
         break;
      }
      prev_tp = tp;
      tp      = tp->nextInBucket;
   }

   ge.seqnoTBE++;

   gc.allocMsg = "ThreadBlock";

   // Since all dead threads are already marked as invalid,
   // reused thread blocks can be safely moved to the dead list.
   // Code traversing the live thread list will automatically
   // retry the list after acquiring the thread lock.

   if ( tp )
   {
      if ( ge.flushing )
      {
         OptVMsg( "Thread %s being reused, move to dead list delayed\n", tp->thread_name );

         tp->fDelayed          = 1;
         ge.flushDelayed       = 1;
      }
      else
      {
         OptVMsg( "Thread %s being reused, moving old entry to dead list\n", tp->thread_name );

         prev_tp->nextInBucket = tp->nextInBucket;   // Remove old entry from bucket

         tp->nextInBucket      = ge.listDeadThreads;
         ge.listDeadThreads    = tp;    // Add entry to head of list of dead threads
      }
   }

   tp = (thread_t *)hashAdd(&ge.htThreads, Int32ToPtr(tid));

   if ( 0 == tp )
   {
      leave_lock(ThreadLock, THREADLOCK);
      err_exit("table_put in insert_thread");
   }
   OptVMsg( "New thread_t allocated for tid = %x\n", tid );

   // table_put allocates and zeroes contents of tp

// tp->tid = tid;                       // Done by hashAdd
   tp->tsn = ge.seqnoTBE;

   if ( 0 == ge.sizeMetrics )
   {
      ge.sizeMetrics  = ( gv_MaxMetrics + 1 ) * sizeof(UINT64);
   }

   size = 8 * ge.sizeMetrics;

   p = (char *)zMalloc( "thread_metrics", size );

   tp->metEntr = (UINT64 *)p; p += ge.sizeMetrics;
   tp->metExit = (UINT64 *)p; p += ge.sizeMetrics;
   tp->metDelt = (UINT64 *)p; p += ge.sizeMetrics;
   tp->metAppl = (UINT64 *)p; p += ge.sizeMetrics;
   tp->metInst = (UINT64 *)p; p += ge.sizeMetrics;
   tp->metJprf = (UINT64 *)p; p += ge.sizeMetrics;
   tp->metSave = (UINT64 *)p; p += ge.sizeMetrics;
   tp->metGC   = (UINT64 *)p; p += ge.sizeMetrics;

   p = (char *)zMalloc( "mtype_stack", STACK_DEPTH_INC );

   tp->mtype_stack = p;
   tp->mtype_size  = STACK_DEPTH_INC;
// tp->mtype_depth = 0;
// tp->depth       = 0;

#ifndef _LEXTERN

   if ( ge.scs )
   {
   #if defined(_WINDOWS)
      tp->handle = OpenThread((THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION | THREAD_SET_INFORMATION), FALSE, tid);


      if (tp->handle == NULL)
      {
         if (tid != 0)                  // Idle samples - fake thread id
         {
            WriteFlush(gc.msg, "**ERROR**: OpenThread(%d) failed.\n", tid);
         }
      }
   #elif defined(_AIX) || defined(_LINUX)
      tp->pthread = pthread_self();
   #endif
   }

#endif

   tp->mt1         = gv_type_Other;
   tp->x12         = gv_ee_Other;
   tp->mt2         = gv_type_Other;
   tp->x23         = gv_ee_Other;
   tp->mt3         = gv_type_Other;

   sprintf(buffer, "%08x_unknown_thread", PtrToUint32(tp->tid));   // Default thread name

   tp->thread_name = xStrdup("thread_name",buffer);

   tp->currT       = (void *)thread_node(tp);
   tp->currm       = tp->currT;

   tp->ptt_enabled = 0;
   tp->scs_enabled = 0;

   tp->fValid      = 1;

   leave_lock(ThreadLock, THREADLOCK);

   return( tp );
}

/******************************/
// OBJECT SECTION
/******************************/

/******************************/
object_t * lookup_object(void * addr)
{
   object_t * op;

   op = (object_t *)hashLookup(&ge.htObjects, addr);

   return( op );
}

/******************************/
object_t * insert_object(void * addr)   // FIX ME, 64 bits
{
   object_t * op;

   if ( 0 == ge.htObjects.bucks )
   {
      enter_lock(ObjTableLock, OBJTABLELOCK);

      if ( 0 == ge.htObjects.bucks )
      {
         hashInit(&ge.htObjects, ge.ohashb, sizeof(object_t), "ObjectTable", 0, 0);
      }
      leave_lock(ObjTableLock, OBJTABLELOCK);
   }

   op = hashLookup(&ge.htObjects, addr);

   if (op == NULL)
   {                                    // add if not found
      enter_lock(ObjTableLock, OBJTABLELOCK);

      op = hashLookup(&ge.htObjects, addr);

      if (op == NULL)
      {                                 // add if not found
         gc.allocMsg = "ObjectBlock";

         op = (object_t *)hashAdd(&ge.htObjects, (void *)addr);

         if (op == NULL)
         {
            leave_lock(ObjTableLock, OBJTABLELOCK);
            err_exit("table_put in insert_object");
         }

         op->objectID = PtrToUint32(op->addr);   // Addr is already the global tag

         MVMsg(gc_mask_ObjectInfo,
               ("insert_object: Object created, op = %p, next = %p, tag = %08X, id = %08X\n",
                op,
                op->nextInBucket,
                op->addr,
                op->objectID ));
      }
      leave_lock(ObjTableLock, OBJTABLELOCK);
   }
   return op;
}

/******************************/
void remove_object(void * addr)
{
   enter_lock(ObjTableLock, OBJTABLELOCK);

   hashRemove( &ge.htObjects, addr );

   leave_lock(ObjTableLock, OBJTABLELOCK);
}

/******************************/

void object_table_free(void)
{
   MVMsg( gc_mask_Malloc_Free, ("Free the Object Table\n") );

   enter_lock(ObjTableLock, OBJTABLELOCK);

   hashFree( &ge.htObjects );

   leave_lock(ObjTableLock, OBJTABLELOCK);
}

// Write hashing statistics to gc.msg

void LogAllHashStats()
{
   hashStats( &ge.htClasses  );
   hashStats( &ge.htMethods  );
   hashStats( &ge.htThreads  );
   hashStats( &ge.htObjects  );
}

//----------------------------------------------------------------------
// Create a class_t entry based on the class reference
//----------------------------------------------------------------------

class_t * getClassEntry( thread_t * tp, jclass klass )
{
   jlong      class_id;                 // Unique class id for Record (skip special class ids)

   class_t  * cp;

   hash_t   * ctp   = &ge.htClasses;
   int        index;

   int        i;
   int        n;
   int        hash  = 0;
   jlong      tagClass  = 0;

   char       cnmbuf[MAX_PATH];
   char     * sig;
   char     * cnm;
   jvmtiError rc;

   if ( gv->useTagging )
   {
      rc = (*ge.jvmti)->GetTag(ge.jvmti, klass, &tagClass);

      if ( rc != JVMTI_ERROR_NONE )
      {
         ErrVMsgLog("GetTag: rc = %d, klass = %p\n", rc, klass );
         err_exit(0);
      }

      if ( 0 == tagClass )
      {
         tagClass = getNewTag();

         rc = (*ge.jvmti)->SetTag(ge.jvmti, klass, tagClass);

         if ( rc != JVMTI_ERROR_NONE )
         {
            ErrVMsgLog( "SetTag: rc = %d, klass = %p\n", rc, klass );
            err_exit(0);
         }
         MVMsg(gc_mask_ObjectInfo,
               ("getClassEntry: Set new class tagClass = %08"_P64"X\n", tagClass));
      }

      cp  = lookup_class( getIdFromTag(tagClass) );

      if ( 0 == cp || ( cp->flags & class_flags_nameless ) )
      {
         rc = (*ge.jvmti)->GetClassSignature(ge.jvmti, klass, &sig, 0);

         if (rc != JVMTI_ERROR_NONE)
         {
            ErrVMsgLog("GetClassSignature: rc = %d, klass = %p\n", rc, klass);
            err_exit(0);
         }
         cnm = sig2name( sig, cnmbuf ); // Get the class name

         MVMsg(gc_mask_ObjectInfo,
               ("getClassEntry: tagClass = %08"_P64"X, name = %s(%s)\n", tagClass, cnm, sig ));

         (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)sig);

         if ( cp )
         {
            if ( cp->flags & class_flags_nameless )   // Minimize possible timing hole
            {
               cp->class_name = xStrdup( "getClassEntry", cnm );
               cp->flags     &= ~class_flags_nameless;
            }
         }
         else
         {
            cp = addClass( getIdFromTag(tagClass), cnm );
         }

         MVMsg(gc_mask_ObjectInfo,
               ("New class: tagClass = %08"_P64"X, name = %s\n", tagClass, cp->class_name));
      }
   }
   else
   {
      rc = (*ge.jvmti)->GetClassSignature(ge.jvmti, klass, &sig, 0);

      if ( rc != JVMTI_ERROR_NONE )
      {
         ErrVMsgLog("ERROR: GetClassSignature: klass = %x, rc = %d\n", klass, rc );
         err_exit(0);
      }
      cnm = sig2name( sig, cnmbuf );    // Convert to standard class name in cnmbuf

      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)sig);

      n = (int)strlen( cnm );

      for ( i = 0; i < n; i++ )         // Generate a 15-bit hash code
      {
         hash ^= ( cnm[i] << ( i & 7 ) );
      }

      if ( 0 == ctp->bucks )
      {
         enter_lock(ClassLock, CLASSLOCK);

         if ( 0 == ctp->bucks )
         {
            hashInit(ctp, ge.chashb, gv->sizeClassBlock, "ClassTable", &alloc_class, &free_class);
         }
         leave_lock(ClassLock, CLASSLOCK);
      }

      index = hash % ctp->bucks;
      cp    = (class_t *)ctp->buckv[index];

      while ( cp )
      {
         if ( 0 == strcmp( cnm, cp->class_name ) )   // Class name found
         {
            return( cp );
         }
         cp = cp->nextInBucket;
      }

// New entry required

      enter_lock(ClassLock, CLASSLOCK);

      cp    = (class_t *)ctp->buckv[index];

      while ( cp )
      {
         if ( 0 == strcmp( cnm, cp->class_name ) )   // Class name found
         {
            leave_lock(ClassLock, CLASSLOCK);

            return( cp );
         }
         cp = cp->nextInBucket;
      }

      class_id = getNewTag();           // For use by Record/Playback

      MVMsg(gc_mask_ObjectInfo,
            ("getClassEntry: set new tag = %08X\n",
             (int)class_id ));

      MVMsg( gc_mask_Show_Classes,( "New Class: %s\n", cnm ) );

      gc.allocMsg = "ClassBlock";

      cp = (class_t *)zMalloc( 0, gv->sizeClassBlock );

      if ( 0 == cp )
      {
         leave_lock(ClassLock, CLASSLOCK);
         err_exit( "Creating class block" );
      }

      cp->class_name = xStrdup( 0, cnm );   // Create a copy of the local buffer
      cp->flags     &= ~class_flags_nameless;

      if ( 0 == cp->class_name )
      {
         leave_lock(ClassLock, CLASSLOCK);
         err_exit( "Creating class name" );
      }

      cp->class_id   = getIdFromTag(class_id);

      cp->nextInBucket  = ctp->buckv[index];   // New nextInBucket is old head

      ctp->buckv[index] = (bucket_t *)cp;   // New bucket entry is new head
      ctp->elems++;

      leave_lock(ClassLock, CLASSLOCK);
   }

   if ( gv->ClassLoadInfo )
   {
      if ( tp->etype != JVMTI_EVENT_CLASS_LOAD )
      {
         queryClassMethods( tp, cp, klass );
      }

      if ( ge.ObjectInfo )              // Implies tagging
      {
         if ( 0 == lookup_object( getIdFromTag(tagClass) ) )
         {
            MVMsg(gc_mask_ObjectInfo,
                  ("getClassEntry: Creating instance of java/lang/Class for %08X:%s\n",
                   cp->class_id,
                   cp->class_name ));

            objectAllocInfo(tp,
                            32,
                            (jobject)ge.cpJavaLangClass,
                            getIdFromTag(tagClass),
                            0);
         }
      }
   }
   return( cp );
}

#ifndef _LEXTERN

//======================================================================
// StkNode Section
//======================================================================

stknode_t * insert_stknode( thread_t * tp, void * esp, void * eip )
{
   stknode_t * sp;
   uint32_t    ind;

   if ( 0 == tp->htStkNodes.bucks )
   {
      enter_lock(StkNodeLock, STKNODELOCK);

      if ( 0 == tp->htStkNodes.bucks )
      {
         hashInit(&tp->htStkNodes, ge.shashb, sizeof(stknode_t), "StkNodeTable", 0, 0);
      }
      leave_lock(StkNodeLock, STKNODELOCK);
   }

   ind = ( PtrToUint32(esp) ^ PtrToUint32(eip) ) % tp->htStkNodes.bucks;
   sp  = (stknode_t *)tp->htStkNodes.buckv[ind];

   while (sp)                           // See if entry already in table
   {
      if ( sp->esp == esp
           && sp->eip == eip )
      {
         return(sp);
      }
      sp = sp->nextInBucket;
   }

   enter_lock(StkNodeLock, STKNODELOCK);

   sp = (stknode_t *)tp->htStkNodes.buckv[ind];

   while ( sp )                         // Guarantee that entry is still not in table
   {
      if ( sp->esp == esp
           && sp->eip == eip )
      {
         leave_lock(StkNodeLock, STKNODELOCK);
         return(sp);
      }
      sp = sp->nextInBucket;
   }

   sp = (stknode_t *)zMalloc( "StkNode", tp->htStkNodes.itemsize );

   if ( 0 == sp )
   {
      err_exit("table_put in insert_stknode");
   }
   sp->nextInBucket          = tp->htStkNodes.buckv[ind];   // Pointer to old head of list
   sp->esp                   = esp;
   sp->eip                   = eip;
   sp->numRefs               = 1;       // 0 indicates non-unique mapping

   tp->htStkNodes.buckv[ind] = (bucket_t *)sp;   // New entry is new head of list
   tp->htStkNodes.elems++;

   leave_lock(StkNodeLock, STKNODELOCK);

   return sp;
}

//----------------------------------------------------------------------

void  stknode_table_free( thread_t * tp )
{
   MVMsg( gc_mask_Malloc_Free, ("Free the StkNode Table for tid=%p\n", tp->tid) );

   enter_lock(StkNodeLock, STKNODELOCK);
   hashFree( &tp->htStkNodes );
   leave_lock(StkNodeLock, STKNODELOCK);
}

//----------------------------------------------------------------------

Node * push_stknode( thread_t * tp )
{
   Node * np;

   if ( tp->stknode
        && tp->stknode->np )
   {
      np = tp->stknode->np;

      ge.numSnSelectNode++;             // Number of Tree Nodes located using StkNodes
   }
   else
   {
      np = push_callstack(tp);

      ge.numSnPushCallStack++;          // Number of CallStacks pushed to locate Tree Nodes
   }
   return( np );
}

#endif //_LEXTERN
