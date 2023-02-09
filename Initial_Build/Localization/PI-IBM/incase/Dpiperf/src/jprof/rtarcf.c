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

#include "jprof.h"
#include "tree.h"
#include "utils/pi_time.h"
#include "plugin/JprofPluginInterface.h"
#ifdef HAVE_PERF_SUPPORT
#include "perf_event.h"
#endif

//----------------------------------------------------------------------
// Tree Reset:
//
// The full reset is performed via the following algorithm:
//
// This Reset thread will set the global Reset flag, then loop thru the list
// of threads until no thread has its working flag set.  As each non-working
// thread is encountered, all of its resources are freed, except for the
// thread structure itself.  After all threads have been reset, all invalid
// thread structures are deleted and the global Reset flag is cleared.
//
// Event processing on each thread first checks the global Reset flag and
// returns immediately if is it on.  Next, it sets the Working flag and
// checks the Reset flag once more.  Finally, when the event processing is
// complete it resets the Working flag in the thread structure.
//
// Whenever a thread stop event occurs, that thread structure is marked as
// invalid and kept until the next Reset, when it is freed.
//----------------------------------------------------------------------

void treset( int reset )
{
   thread_t  * tp;
   char        fResetDone;
   char        fSamplersWorking;
   int         i;
   int         n;
   char      * p;
   int         numWorking;
   int         numSamplersWorking;
   hash_iter_t iter;

   static char * strReset[6] =
   {
      "SYNC",
      "STACK",
      "ZEROBASE",
      "STATE",
      "FULL",
      "DESTROY"
   };

   // which commands
   // start start_wob zerobase reset2stack reset
   // fgc flush heapdump

   OptVMsgLog("Entering RTD_LOCK : treset(%s)\n", strReset[ reset - 1]);

   enter_lock(XrtdLock, RTDRIVERLOCK);

   ge.reset = reset;
   memory_fence();

   OptVMsgLog("  Inside RTD_LOCK : treset(%s)\n", strReset[ reset - 1]);


   hashIter( &ge.htThreads, &iter );    // Iterate thru the thread table

   while ( ( tp = (thread_t *)hashNext( &iter ) ) )
   {
      if ( tp->tid )                    // SCS_Summary_Counts dummy thread has its own reset
      {
         tp->fResetNeeded = 1;
      }
   }

   gv->addbase = 0;                     // Stop adding to base

   if ( GE_RESET_FULL == ge.reset )     // Full reset
   {
      gv->rton    = 0;                  // tree off
      gv->started = 0;                  // EE2 off
      EnableEvents2x(0);                // disable Events

      if ( gd.gen )
      {
         if ( 0 == gv->g2se )
         {
            fclose( gd.gen );
         }
         gd.gen = 0;
      }

#if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)

      gv->scs_active = 0;
      memory_fence();

   #ifndef _LEXTERN
      if (ge.scs)
      {
         int rc;
         int hh, mm, ss;

         get_current_time(&hh, &mm, &ss, NULL, NULL, NULL);
         msg_log("**INFO** (%02d:%02d:%02d) pausing driver SCS notifications\n", hh, mm, ss);

         rc = ScsPause();
         if (rc != 0)
         {
            WriteFlush(gc.msg, "**ERROR** Sampling notify mode failed to pause. rc = %d (%s)\n", rc, RcToString(rc));
         }

         // @@@@@ COUNTERS CAN'T BE CLEARED HERE BECAUSE THAT MESSES UP THE
         // @@@@@ RESULTS IN scs_terminate().
         // @@@@@ WHY ARE THEY BEING CLEARED ANYWAY?
         //scs_clear_counters();
      }
   #endif
#endif

      if ( gv->CallStackCountsByFrames )
      {
         memset( gv->CallStackCountsByFrames, 0, ( MAX_CALL_STACK_COUNT + 1 ) * sizeof(UINT32) );
         memset( gv->CallStackCyclesByFrames, 0, ( MAX_CALL_STACK_COUNT + 1 ) * sizeof(UINT64) );
      }
      object_table_free();
   }
   numWorking         = 0;
   numSamplersWorking = 0;

   for (;;)                             // Until fResetDone is true
   {
      fResetDone = 1;

      hashIter( &ge.htThreads, &iter ); // Iterate thru the thread table

      while ( ( tp = (thread_t *)hashNext( &iter ) ) )
      {
//       OptVMsg( ">> ge.reset = %s, tid = %p, valid = %d, working = %d, dirty = %d\n",
//                strReset[ge.reset - 1], tp->tid, tp->fValid, tp->fWorking, tp->fDirty );

         if ( tp->fValid )
         {
            fSamplersWorking = 0;

#ifndef _LEXTERN
            if ( ge.scs_thread_count )
            {
               for ( i = 0; i < ge.scs_thread_count; i++ )
               {
                  if ( ge.fSamplerWorking[i] )
                  {
                     fSamplersWorking = 1;
                     break;
                  }
               }
            }
#endif

            if ( ( 0 == fSamplersWorking
                      && 0 == tp->fWorking
                      && ( tp->fResetNeeded
                           || GE_RESET_SYNC == ge.reset ) ) )
            {
               MVMsg( gc_mask_Show_Locks, ( ">> ge.reset = %s, tid = %p\n", strReset[ge.reset - 1], tp->tid ));

               if ( GE_RESET_SYNC != ge.reset )   // Wait for all threads to be quiet
               {
                  if ( GE_RESET_STATE == ge.reset )   // Reset state information for thread
                  {
                     tp->flags &= ~thread_callstack;   // Indicate new call stack needed

                     tp->currm  = tp->currT;

                     tp->mtype_depth = 0;   // Start the mtype_stack again
                     tp->depth       = 0;   // Start the depth again

                     tp->mt1   = gv_type_Other;
                     tp->x12   = gv_ee_Other;
                  }

                  else if ( GE_RESET_FULL == ge.reset )   // Free everything except thread root node
                  {
                     tp->currm = tp->currT;

                     MVMsg( gc_mask_Malloc_Free, ("Free the Callflow Tree\n") );

                     treeKiller(&freeLeaf, tp->currT);

                     tp->flags &= ~thread_callstack;   // Indicate new call stack needed

                     tp->stack_walk_error_node = NULL; // Should have been freed by treeKiller
                  }

#ifdef RESET2STACK
                  else if ( GE_RESET_STACK == ge.reset )   // Free all but stac, zero stack
                  {
                     tree_stack(tp);
                  }
#endif

                  else if ( GE_RESET_ZEROBASE == ge.reset )   // Zero data, keep all nodes
                  {
                     treeWalker(&zeroNode, tp->currT);
                  }
                  threadDataReset(tp);

                  tp->fResetNeeded = 0;
               }

               MVMsg( gc_mask_Show_Locks, ( "<< ge.reset = %s, tid = %p\n", strReset[ge.reset - 1], tp->tid ));
            }
            else if ( tp->fWorking )
            {
               if ( numWorking < gv->msglimit )
               {
                  MVMsg( gc_mask_Show_Locks, ( "treset(%s): tid = %p, event = %4d, Thread Working: %s\n",
                                               strReset[ge.reset - 1], tp->tid, tp->etype, tp->thread_name ));
               }
               numWorking++;
               fResetDone = 0;
            }
            else if ( fSamplersWorking )
            {
               if ( numSamplersWorking < gv->msglimit )
               {
                  MVMsg( gc_mask_Show_Locks, ( "treset(%s): tid = %p, event = %4d, Samplers Working: %s\n",
                                               strReset[ge.reset - 1], tp->tid, tp->etype, tp->thread_name ));
               }
               numSamplersWorking++;
               fResetDone = 0;
            }
         }
         else                           // Thread not valid, but still in hash table, delete what we can
         {
            if ( tp->tid                // Not SUMMARY_COUNTS
                 && tp->metEntr         // Allocations not yet freed
                 && GE_RESET_FULL == ge.reset )
            {
               OptVMsgLog("Freeing invalid thread: %s\n",
                          tp->thread_name);

               tp->currm = tp->currT;

#ifndef _LEXTERN
               stknode_table_free(tp);
#endif

               treeKiller(&freeLeaf, tp->currT);

               tp->stack_walk_error_node = NULL; // Should have been freed by treeKiller

               freeFrameBuffer(tp);

               if ( tp->mtype_stack )
               {
                  p = (char *)tp->mtype_stack;
                  n = tp->mtype_size;

                  tp->mtype_size  = 0;
                  tp->mtype_stack = 0;

                  xFree( p, n );
               }

               p = (char *)tp->metEntr;

               tp->metEntr     = 0;
               tp->metExit     = 0;
               tp->metDelt     = 0;
               tp->metAppl     = 0;
               tp->metInst     = 0;
               tp->metJprf     = 0;
               tp->metSave     = 0;
               tp->metGC       = 0;

               xFree( p, 8 * ge.sizeMetrics );
            }
         }
      }
      if ( fResetDone )
      {
         break;
      }
      sleep(50);
   }
   if ( numWorking > gv->msglimit )
   {
      OptVMsgLog( "treset(%s): %d Thread Working messages suppressed\n",
                  strReset[ge.reset - 1], numWorking - gv->msglimit );
   }

   if ( GE_RESET_FULL == ge.reset )
   {
      if ( ge.listDeadThreads )
      {
         enter_lock(ThreadLock, THREADLOCK);

         tp = ge.listDeadThreads;

         while ( tp )
         {
            OptVMsgLog("Freeing dead thread: %s\n",
                       tp->thread_name);

            treeKiller(&freeLeaf, tp->currT);

            tp->stack_walk_error_node = NULL; // Should have been freed by treeKiller

            if ( tp->metEntr )
            {
               xFree( tp->metEntr, 8 * ge.sizeMetrics );
            }

            if ( tp->mtype_stack )
            {
               xFree( tp->mtype_stack, tp->mtype_size );
            }
            xFreeStr( tp->thread_name );

            p  = (char *)tp;

            tp = tp->nextInBucket;

            xFree( p, ge.htThreads.itemsize );
         }
         ge.listDeadThreads = 0;

         leave_lock(ThreadLock, THREADLOCK);
      }
   }

   OptVMsgLog(" Leaving RTD_LOCK : treset(%s)\n", strReset[ ge.reset - 1]);

   ge.reset = 0;
   memory_fence();

   leave_lock(XrtdLock, RTDRIVERLOCK);
}

/******************************/
void reh_Out(void)
{
   if ( gv_rton_Trees == gv->rton || gv->scs_active )
   {
      xtreeOut();
   }
}

/******************************/
void TreeOut(void)
{
   if ( gv_rton_Trees == gv->rton || gv->scs_active )
   {
      enter_lock(XrtdLock, RTDRIVERLOCK);
      fprintf(gc.msg, " >> RTD_LOCK : flush\n");

      reh_Out();

      fprintf(gc.msg, " << RTD_LOCK : flush\n");
      leave_lock(XrtdLock, RTDRIVERLOCK);
   }
}

/******************************/
int procSockCmd( thread_t * tp, char * nline )
{
   int    rc = JVMTI_ERROR_NONE;
   int    miss;
   char * line;
   char   command[MAX_PATH];
   char   buf[MAX_PATH];
   int    n;
   char   logRtdCmds;
   int    tid;
   char * p;
#if defined(_WINDOWS) || defined(_LINUX)
   char * saveptr = NULL;
#endif

   // nline : native(ASCII/EBCDIC) string

   n = (int)strlen(nline);

   if ( n > MAX_PATH-1 )
   {
      n = MAX_PATH-1;
   }

   line    = strncpy(command, nline, n);   // Save input line in command
   line[n] = 0;

   Force2Native( line );                // Convert ASCII to EBCDIC, if needed

   if ( ',' == line[0] )
   {
     memmove(line, line+1, n);           // Remove leading ','
   }

   p = strcpy(buf, line);

   while ( *p )
   {
      *p = (char)toupper(*p);
      p++;
   }

#ifndef _LEXTERN

   if ( 0 == gv && 0 == gc.ShuttingDown )
   {
      int hh, mm, ss;

      get_current_time(&hh, &mm, &ss, 0, 0, 0);
      OptVMsgLog("\n (%02d:%02d:%02d) RTDriver Cmd: <%s>\n", hh, mm, ss, nline);

      if ( 0 == strcmp("SHUTDOWN", buf) )
      {
         err_exit(0);
      }

      ErrVMsgLog("Only SHUTDOWN, and EXIT_RTDRIVER are allowed now\n");
      rc = JVMTI_ERROR_INTERNAL;

      return( rc );
   }
#endif //_LEXTERN

   if ( 0 == tp )
   {
      tp = readTimeStamp(CurrentThreadID());
   }

   logRtdCmds = gv->logRtdCmds;

   if ( logRtdCmds )
   {
      if ( logRtdCmds < 2
           && ( 0 == strncmp( "functione", command, 9 )   // Possible functionentry/exit=
                || 0 == strncmp( ",functione", command, 10 ) ) )   // Possible functionentry/exit=
      {
         logRtdCmds = 0;                // Suppress output for this frequent command
      }
      else
      {
         int hh, mm, ss;

         get_current_time(&hh, &mm, &ss, 0, 0, 0);
         OptVMsgLog("\n (%02d:%02d:%02d) RTDriver Cmd: <%s>\n", hh, mm, ss, command);
      }
   }

#if defined(_WINDOWS)
   line = strtok_s(command, ",", &saveptr);
#elif defined(_LINUX)
   line = strtok_r(command, ",", &saveptr);
#else
   line = strtok(command, ",");
#endif
   while (line != NULL)
   {
      miss = 0;

      if ( '/' == *line )
      {
         line++;
      }

      if ( logRtdCmds )
      {
         OptVMsgLog(" (0x%08X:%08X) RTDriver Token: <%s>\n",
                    (UINT32)(tp->timeStamp >> 32), (UINT32)tp->timeStamp, line);
      }

      p = strcpy(buf, line);

      while ( *p )
      {
         *p = (char)toupper(*p);
         p++;
      }

      while ( 'G' == buf[0] && 'C' == buf[1] )
      {
         if ( ge.onload )
         {
            msg_se_log(" Requesting GC\n");

            (*ge.jvmti)->ForceGarbageCollection( ge.jvmti );
         }
         line += 2;
         memmove(buf, buf+2, strlen(buf)-1);
      }

      if ( buf[0] )
      {
         gc.posNeg = 0;
         rc = ParseCommand( tp, buf, line );

         if ( 0 == gv )
         {
            break;
         }
      }
#if defined(_WINDOWS)
      line = strtok_s(NULL, ",", &saveptr);
#elif defined(_LINUX)
      line = strtok_r(NULL, ",", &saveptr);
#else
      line = strtok(NULL, ",");
#endif
   }

   fflush(gc.msg);
   return(rc);
}

int procSockCmdFromPlugin( void * tp, char * nline )
{
   return procSockCmd((thread_t *)tp, nline);
}

/******************************/
// generic : method entry/exit events
void genPrint1( thread_t * tp )
{
   char * c;
   char * m;

   char entryexit = '>';
   char pre[4]    = " :";

   int  met    = gv->mapmet[0];         // FIX ME:  Need parm for other than first metric

   UINT64 value   = tp->metDelt[met];
   int    adjust  = 0;

   tp->flags |= thread_resetdelta;

   if ( tp->mp )
   {
      pre[0] = gv->types[ tp->mt3 ];

      if ( tp->x23 != gv_ee_Entry )
      {
         pre[0] = gv->types[ tp->mt2 ];
         entryexit = '<';
      }

      if ( 0 == gv->methodTypes )
      {
         pre[0] = '\0';
      }

      getMethodName( tp->mp, &c, &m);

      // NB. need thrd stack space for NodeName
      //     use c m & s instead

      if ( gv->msf[met] )               // Scaling required
      {
         value = (UINT64)( ((double)value) / gv->msf[met] );
      }
      WriteFlush( gd.gen, "%8" _P64 "d %c %s%s.%s %s\n",
                  value,
                  entryexit,
                  pre, c, m,
                  tp->thread_name);
   }
   else
   {
      WriteFlush( gd.gen, " Error: NULL method pointer\n" );
   }
}

/******************************/
// generic : non-method events
// treat as arcf impulse events
void genPrint2(thread_t * tp)
{
   char * enm;
   int etype;
   UINT64 tm;

   // why is "e" needed
   // etype (isnt that in tp by now ??)

   // 2 problems
   // 1 : etype in tp ???
   // 2 : oeh_cnm : does it need e

   tm = tp->metDelt[0];

   // solving problem 1
   //etype = e->event_type;
   etype = tp->etype;

   enm = get_event_name(etype);

   fprintf(gd.gen, "%8" _P64 "d ? ev_%d_%s %s\n",
              tm, etype, enm, tp->thread_name);
}

//----------------------------------------------------------------------
// Future Consideration:
//
//   ---> A --->    When A calls B, we get a timestamp (TS) somewhere
//       / \        between A and B, with Entry overheads E1 and E2.
//   E1 /   \ X2    When B returns to A, we get similar eXit overheads
//     /     \      X1 and X2.  We can solve for the values of these
// TS *       * TS  overheads by recording the minimum values of:
//     \     /        EE = E2+E1 = Entry followed by Entry
//   E2 \   / X1      EX = E2+X1 = Entry followed by Exit
//       \ /          XE = X2+E1 = Exit  followed by Entry
//        B           XX = X2+X1 = Exit  followed by Exit
//
// EE = E1 + E2 +  0 +  0       EE = 1 1 0 0 x E1
// EX =  0 + E2 + X1 +  0  ==>  EX   0 1 1 0   E2  Matrix not invertable
// XE = E1 +  0 +  0 + X2       XE   1 0 0 1   X1     ( NO SOLUTION )
// XX =  0 +  0 + X1 + X2       XX   0 0 1 1   X2
//
// This might be possible with assistance from the JVM.
//----------------------------------------------------------------------

//======================================================================
// Compute the minimum value of each metric for each combination of
// entry/exit and methodtype. This routine is NOT THREAD SAFE, but the only
// harm this can do is to cause a non-fatal autocal error or lose a
// count in the history array.
//
// As noted above, it is necessary to compute minima for all sequences
// mt1 -x12-> mt2 -x23-> mt3.  This is graphically shown as:
//
// Entry-Entry => mt1             Exit-Entry =>   mt2
//                  \                             / \
//                  mt2                         mt1 mt3
//                    \
//                    mt3
//
//
// Exit-Exit   =>     mt3         Entry-Exit => mt1 mt3
//                    /                           \ /
//                  mt2                           mt2
//                  /
//                mt1
//======================================================================

void check_mtype_stack( thread_t * tp )
{
   char x23 = tp->x23;
   char mt3 = tp->mt3;

   if ( (unsigned char)mt3 > gv_type_Other )
   {
      mt3     = gv_type_Other;
      tp->mt3 = mt3;
   }

   if ( 0 == ( tp->flags & thread_mtypedone ) )
   {
      if ( tp->mtype_stack )
      {
         if ( gv_ee_Entry == x23 )
         {
            if ( ( gv_type_jitted == mt3
                   || gv_type_Jitted == mt3 )
                 && tp->mp
                 && ( tp->mp->flags & method_flags_rejitted ) )
            {
               mt3 += 2;                // Convert Jitted to ReJitd

               tp->mt3 = mt3;
            }

            if ( tp->mtype_depth >= tp->mtype_size )
            {
               tp->mtype_stack = xRealloc( "tp->mtype_stack",
                                           tp->mtype_size + STACK_DEPTH_INC,
                                           tp->mtype_stack,
                                           tp->mtype_size );
               tp->mtype_size += STACK_DEPTH_INC;
            }
            tp->mtype_stack[ tp->mtype_depth ] = mt3;

            tp->mtype_depth++;
         }
         else                           // METHOD_EXIT, get from mtype_stack
         {
            mt3 = gv_type_Other;

            if ( tp->mtype_depth > 0 )
            {
               tp->mtype_depth--;       // Discard the current entry
            }
            if ( tp->mtype_depth > 0 )
            {
               mt3 = tp->mtype_stack[ tp->mtype_depth - 1 ];   // Get parent
            }
            tp->mt3 = mt3;
         }
//          OptVMsgLog("x23:mt3 = %d:%d, depth:type[depth:-1,0,+1] = %d:%d,%d,%d\n",
//                     (int)tp->x23, (int)tp->mt3, tp->mtype_depth,
//                     (int)tp->mtype_stack[tp->mtype_depth-1],
//                     (int)tp->mtype_stack[tp->mtype_depth  ],
//                     (int)tp->mtype_stack[tp->mtype_depth+1]);
      }
      else if ( gv_ee_Entry != x23 )    // Exit w/o mtype_stack
      {
         mt3 = gv_type_Other;

         if ( ((pNode)(tp->currm))->par )   // Method has parent
         {
            mt3 = ((pNode)(tp->currm))->par->ntype;   // Get type of parent
         }
         if ( mt3 > gv_type_Other )
         {
            mt3 = gv_type_Other;        // Handle return to Thread node
         }
         tp->mt3 = mt3;
      }
      tp->flags |= thread_mtypedone;    // Indicate mtype_stack check complete

      if ( gv_ee_Exit == x23
           && ( tp->flags & thread_exitpopped ) )
      {
         tp->x23 = gv_ee_ExitPop;
      }
   }
}

/******************************/
void gen_xfer(thread_t * tp, void * mid)
{
   tp->metDelt[0] = 0;                  // FIX ME:  Rework for new metrics

   if (gv->calltrace)
   {
      fprintf(gd.gen, " ### %s\n", "I2J_undo");
      tp->mp  = insert_method( mid, 0 );

      tp->mt3 = gv_type_Interp;
      tp->x23 = gv_ee_Exit;
      genPrint1( tp );                  // exit  I:a

      tp->mt3 = gv_type_Jitted;
      tp->x23 = gv_ee_Entry;
      genPrint1( tp );                  // enter J:a

      fprintf(gd.gen, " ### %s\n", "I2J_undo_completed");
   }
}

/******************************/
void rehmethods(thread_t * tp)
{
   int   rc;
   int   sn = tp->tsn;

   jmethodID  mid;
   method_t * mp = tp->mp;

   if ( 0 == mp )
   {
      err_exit( "rehmethods: Missing method pointer" );
   }
   tp->flags &= ~thread_mtypedone;      // Indicate mtype not yet done

   if ( gc.mask & gc_mask_Show_Methods )
   {
      OptVMsgLog( "%s: tid = %p, ts = %" _P64 "x, mid = %p, name = %s.%s\n",
                  ( gv_ee_Entry == tp->x23 )
                  ? "ENTR"
                  : "EXIT",
                  tp->tid,
                  tp->timeStamp,
                  tp->mp->method_id,
                  tp->mp->class_entry ? tp->mp->class_entry->class_name : "invalid_class",
                  tp->mp->method_name );
   }

   if ( gv_type_Compiling == tp->mt3 )
   {
      if ( gv->recomp )                 // Recompilation detection enabled
      {
         if ( gv_ee_Entry == tp->x23 )
         {
            tp->mp->flags += method_flags_compiled;

            if ( tp->mp->flags > method_flags_MASK )   // More than all valid bits
            {
               OptVMsgLog( "Compile #%d, mid = %p, name = %s.%s\n",
                           tp->mp->flags >> method_flags_rejshift,
                           tp->mp->method_id,
                           tp->mp->class_entry->class_name,
                           tp->mp->method_name );

               if ( gd.gen )
               {
                  fprintf( gd.gen, "0 - Compile#%d:%s.%s %s\n",
                           tp->mp->flags >> method_flags_rejshift,
                           tp->mp->class_entry->class_name,
                           tp->mp->method_name,
                           tp->thread_name );
               }
            }
         }
      }
   }

   if ( 0 == gv->rton )
   {
      return;
   }

   // skip some threads (see thread beg/end option)
   if ( gv->tbeg )
   {
      if ( tp->tsn < gv->tbeg || tp->tsn > gv->tend )
      {
         return;
      }
   }

   // TREE structure

   if ( gv->trees )
   {
      char  x23;
      char  mt3;

      int   i;
      int   n;

      if ( tp->currm == tp->currT       // No current method on thread
           && 0 == ( tp->flags & thread_callstack ) )   // Callstack never acquired
      {
         x23      = tp->x23;
         mt3      = tp->mt3;            // Save the thread fields

         threadDataReset( tp );

         if ( 0 == gv->noJvmCallStacks )   // JVM can retrieve callstacks
         {
            OptVMsg( "Acquiring initial call stack for %s\n", tp->thread_name );

            get_callstack(tp,tp);       // tp->frame_buffer has the stack frames

            OptVMsg( "%2d stack frame(s) acquired\n", tp->num_frames );

            if ( gv_ee_Entry == x23 )
            {
               n = 1;                   // Skip last frame for ENTRY
            }
            else
            {
               n = 0;                   // Use all frames for EXIT
            }

            if ( tp->num_frames > n )
            {
               tp->x23 = gv_ee_Entry;
               tp->mt3 = gv_type_Other; // Simulate entries of type other

               OptVMsg( "%2d stack frame(s) to be processed\n", tp->num_frames - n );

               for ( i = tp->num_frames - 1; i >= n; i-- )
               {
                  if ( ge.getStackTraceExtended )
                  {
                     mid = ((jvmtiFrameInfoExtended *)tp->frame_buffer)[i].method;

                     if ( COM_IBM_STACK_FRAME_EXTENDED_JITTED
                          == ((jvmtiFrameInfoExtended *)tp->frame_buffer)[i].type )
                     {
                        tp->mt3 = gv_type_Jitted;
                     }
                     else if ( -1 == ((jvmtiFrameInfoExtended *)tp->frame_buffer)[i].location )
                     {
                        tp->mt3 = gv_type_Native;
                     }
                     else
                     {
                        tp->mt3 = gv_type_Interp;
                     }

                     tp->mp = insert_method( mid, 0 );

                     if ( tp->mp->flags & method_flags_native )
                     {
                        tp->mt3 = gv_type_Native;   // Mark as native method
                     }
                     if ( tp->mp->flags & method_flags_static )
                     {
                        tp->mt3--;      // Mark as static method
                     }
                  }
                  else
                  {
                     mid    = ((jvmtiFrameInfo *)tp->frame_buffer)[i].method;
                     tp->mp = insert_method( mid, 0 );
                  }

                  // Override tp->mt3 here, if known

                  RTEnEx( tp );         // Add it to the tree

                  if (gv->calltrace)
                  {
                     genPrint1( tp );
                  }
                  tp->flags &= ~thread_mtypedone;   // Indicate mtype not yet done

                  tp->mt1 = tp->mt2;    // Remember history of types
                  tp->x12 = tp->x23;    // Remember history of transitions
                  tp->mt2 = tp->mt3;    // Remember history of types
               }

               if ( 0 == n              // Event is an Exit
                    && ( mp != tp->mp   // Last method on stack does not match
                         || JVM_SOV == ge.JVMversion ) )   // JVM is SOV
               {
                  tp->mp = mp;          // Add the missing method

                  RTEnEx( tp );         // Add it to the tree

                  if (gv->calltrace)
                  {
                     genPrint1( tp );
                  }
                  tp->flags &= ~thread_mtypedone;   // Indicate mtype not yet done
               }
            }
         }
         tp->flags |= thread_callstack; // Don't attempt to get call stack again

         tp->x23 = x23;
         tp->mt3 = mt3;
         tp->mp  = mp;                  // Restore the thread fields
      }
   }

   rc = RTEnEx( tp );                   // structure

   if (gv->calltrace)
   {
      genPrint1( tp );
   }
}

/******************************/
int reh_Init(void)
{
   int met;
   int metricID;
   int metrics[8] = {0};

   int rc         = 0;

   OptVMsg(" > reh_Init\n");

   fprintf(gc.msg, "PTT Init\n");
   fprintf(gc.msg, "  gv->usermets   %d\n", gv->usermets);
   fprintf(gc.msg, "  gv->physmets   %d\n", gv->physmets);
   fflush(gc.msg);

#ifndef HAVE_PERF_SUPPORT
   if ( gv->physmets )
#else
   if ( 0 )
#endif
   {
      // show mminit options
      for ( met = 0; met < gv->physmets; met++ )
      {
         WriteFlush(gc.msg, "    m[%d] %4d\n", met, gv->physmet[met]);
      }

      WriteFlush(gc.msg, "\nCalling PttInit\n");

      rc = PttInit(gv->physmets, gv->physmet, 0);

      WriteFlush(gc.msg, "PttInit rc = %d\n", rc);

      if ( rc )
      {
         ErrVMsgLog("\n**ERROR** PttInit() failure. rc = %d. JProf exiting.\n", rc);
         ErrVMsgLog("    - If re-running TINSTALL does not help, check platform limitations.\n");
         ErrVMsgLog("\n  Max number of metrics:      %d\n", PTT_MAX_METRICS);
         ErrVMsgLog("  Number of selected metrics: %d\n", gv->physmets);

         for ( met = 0; met < gv->physmets; met++ )
         {
            metricID = gv->physmet[met];

            ErrVMsgLog("  - metric[%d] %4d", met, metricID);

            if (ge.mmnames[metricID] != NULL)
            {
               ErrVMsgLog(" %s", ge.mmnames[metricID]);
            }
            ErrVMsgLog("\n");
         }
         err_exit("PttInit Failure");
      }
      else
      {
         fprintf(gc.msg, " JPROF PttInit Success\n");
      }
   }

   OptVMsg(" < reh_Init\n");
   return(int)JVMTI_ERROR_NONE;
}

/******************************/
// default : calltree => tree building.
// socket : rtdriver("start")
// jperf.RTDriver.command("start")

// f=1 base f=2 wo base
void RTArcfTraceStart( thread_t * tp, int fAddBase )
{
   char   parm[MAX_BUFFER_SIZE];

   // lock to insure only 1
   // start start_wob reset reset2stack zerobase flush stop)

   if (gv->started == 0)
   {
      treset( GE_RESET_STATE );         // Reset state of all threads

      if ( gv->calltrace )
      {
         if ( gv->g2se )
         {
            gd.gen = stderr;            // divert gen to stderr
         }
         else
         {
            ge.seqGen++;
            gd.gen = file_open( "gen", ge.seqGen );
         }

         fprintf(gd.gen, " Units :: %s\n\n", getScaledName( parm, 0 ));

         if (ge.cpuSpeed)
         {
            fprintf(gd.gen, " Cycles/Sec %10.2f\n\n", (double)ge.cpuSpeed);
         }

         if ( gv->legend )
         {
            MethodTypeLegend( gd.gen );
         }
         WriteFlush( gd.gen, "\n" );
      }

      enter_lock(XrtdLock, RTDRIVERLOCK);

      fprintf(gc.msg, " >> RTD_LOCK : RTArcfTraceStart(%d)\n", fAddBase);

      gv->addbase = (char)fAddBase;

      EnableEvents2x(1);

      if ( gv->calltrace )
      {
         gv->rton   = gv_rton_Trace;
         fprintf(gc.msg, " Begin generating trace\n");
      }
      if ( gv->calltree )
      {
         gv->rton   = gv_rton_Trees;
         fprintf(gc.msg, " Begin generating trees\n");
      }
      gv->started   = 1;

      if ( 0 == tp || 0 == tp->timeStamp )
      {
         tp = readTimeStamp( tp ? PtrToUint32(tp->tid) : CurrentThreadID() );
      }

      gv->timeStampStart = tp->timeStamp;

#ifndef _LEXTERN
   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
      if (ge.scs)
      {
         ScsOn();
      }
   #endif
#endif

      fprintf(gc.msg, " << RTD_LOCK : RTArcfTraceStart(%d)\n", fAddBase);

      leave_lock(XrtdLock, RTDRIVERLOCK);
   }
   else
   {
      fprintf(gc.msg, " RTArcfTraceStart. Already On !!!\n");
   }

   fflush(gc.msg);
}

/******************************/
void msg_out(void)
{
   int    i, j;

   if ( gc.verbose )
   {
      LogAllHashStats();
   }
   logAllocStats();

   OptVMsgLog( "Number of GCs: %"_P64"d\n",
               ge.numberGCs );

   // lock usage
   fprintf(gc.msg, "\n LOCK STATS\n");
   for (i = 1, j = 0; i <= LC_MAXIMUM; i = 2 * i, j++)
   {
      if (ge.lockcnt[i] > 0)
      {
         fprintf(gc.msg, " ge.lockcnt[%02X] = %10d %s\n",
                 i, ge.lockcnt[i], locklab[j]);
      }
   }
}

