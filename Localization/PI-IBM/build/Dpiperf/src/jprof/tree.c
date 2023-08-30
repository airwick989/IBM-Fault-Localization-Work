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

#include "jprof.h"
#include "tree.h"
#include "scs.h"

#if defined(_LINUX)
   #include <execinfo.h>
#endif


// Declared in scs.c
extern int gv_scs_a2n_method;



/******************************/
void ts_to_time(UINT64 ts, char * ts_time)
{
   UINT64 delta_ms, cur_ms;
   int hh, mm, ss;

   delta_ms = (ts - gv->run_start_ts) / gv->ts_ticks_per_msec;
   cur_ms = gv->run_start_ms + delta_ms;
   hh = Uint64ToInt32(cur_ms / GV_MS_PER_HOUR);
   cur_ms -= (hh * GV_MS_PER_HOUR);
   mm = Uint64ToInt32(cur_ms / GV_MS_PER_MINUTE);
   cur_ms -= (mm * GV_MS_PER_MINUTE);
   ss = Uint64ToInt32(cur_ms / GV_MS_PER_SECOND);
   cur_ms  -= (ss * GV_MS_PER_SECOND);
   if (hh > 23) {
      hh = 0;
   }
   sprintf(ts_time, "    %02d:%02d:%02d.%03d", hh, mm, ss, cur_ms);
   return;
}


//-----------------------------------------------------------------------------
// Support routines for Nodes
//-----------------------------------------------------------------------------

Node * constructNode( FrameInfo * fi );
void gen_xfer(thread_t * tp, void * mid);
static void get_c_symbol_name(Node * np, char * buffer);

int intLog2( uint64_t x )
{
   int n = -1;

   while ( x )
   {
      n++;
      x >>= 1;
   }
   return( n );
}

/******************************/
void freeNodeObjs(Node * p, int useOldSizes)
{
   int offset;

   if ( useOldSizes )
   {
      offset = ge.oldOffEventBlockAnchor;
   }
   else
   {
      offset = ge.offEventBlockAnchor;
   }

   if ( offset )
   {
      RTEventBlock * q;
      RTEventBlock * x;
      int            ebsize = useOldSizes ? ge.sizeRTEventBlock : ge.sizeRTEventBlock;

      enter_lock(ObjectLock, OBJECTLOCK);

      q = *(RTEventBlock **)( offset + (char *)p );

      *(RTEventBlock **)( offset + (char *)p ) = 0;

      while ( q )
      {
         x = q->next;
         xFree(q, ebsize);
         q = x;
      }
      leave_lock(ObjectLock, OBJECTLOCK);
   }
}

/******************************/
void zeroNode( Node * p, int level)
{
   if ( p )
   {
      Node * par;
      Node * chd;
      Node * sib;
      void * id;
      char   ntype;

      par      = p->par;
      chd      = p->chd;
      sib      = p->sib;
      id       = p->id;
      ntype    = p->ntype;

      freeNodeObjs(p, 0);

      memset( (char *)p, 0, ge.nodeSize );

      p->par   = par;
      p->chd   = chd;
      p->sib   = sib;
      p->id    = id;
      p->ntype = ntype;
   }
}

/******************************/
void freeNode(Node * p, int level)
{
   if ( p )
   {
      freeNodeObjs(p, 0);

      xFree( p, ge.nodeSize );
   }
   else
   {
      WriteFlush(gc.msg, " freeNode: node pointer already NULL\n");
   }
}

//----------------------------------------------------------------------
// freeLeaf:  Free node if not thread root, else reset node
//----------------------------------------------------------------------

void freeLeaf(Node * pn, int level)
{
   if ( gv_type_Thread == pn->ntype )
   {
      zeroNode(pn,level);

      pn->par = 0;
      pn->chd = 0;
      pn->sib = 0;
   }
   else
   {
      freeNode(pn,level);
   }
}

/******************************/
static void zero_bmm(Node * p)
{
   int met;

   for (met = 0; met < gv->physmets; met++)
   {
      p->bmm[ met ] = 0;
   }
}

#ifdef RESET2STACK
/******************************/
// depth 1st, free all except currT
// disable method events
// On 1st event after restart
// Free entire tree, except currT
/******************************/
void tree_free0(thread_t * tp)
{
   Node * t;

   t = tp->currT;

   tree_free(t, 0);

   t->chd = t->sib = NULL;
   tp->currm = t;                       // currm = currT
   zeroNode(t,0);
}

// d1st, with freeing (! thrd-node & ! stack-node)
// reset & reset2stack
/******************************/
Node * tree_free(Node * cx, int lev)
{
   Node * nx;

   // pre
   nx = cx->chd;

   // body
   while (nx != NULL)
   {
      nx = tree_free(nx, lev + 1);
   }

   // post
   nx = cx->sib;                        // => sib before free
   if (lev > 0)
   {                                    // dont free thread-level = 0
      if ( 0 == ( cx->flags & node_flags_Stack ) )
      {                                 // free if not marked "stack"
         freeNode(cx,0);
         cx = NULL;
      }
   }

   return(nx);
}
#endif

/******************************/
void showstack(thread_t * tp, char * s)
{
   Node * t;
   Node * m;
   char   nn[NNBuf];

   if ( gc.verbose )
   {
      t = tp->currT;
      m = tp->currm;

      WriteFlush(gc.msg, "\n Showstack: tid %x, %s\n", tp->tid, s);

      while (m != t)
      {                                 // stack size
         WriteFlush(gc.msg, " %s\n", NodeName(m, nn));
         m = m->par;
      }
      WriteFlush(gc.msg, " %s\n\n", NodeName(m, nn));
   }
}

#ifdef RESET2STACK
// reduce thrd tree to current Stack
/******************************/
void tree_stack(thread_t * tp)
{
   Node   * t;
   Node   * m;
   Node   * c = NULL;
   Node * * arr;
   int      scnt = 0;
   int      sx;

   // 1. mark the stack & copy addr to array
   // 2. free all except marked
   // 3. rewire stack

   t = tp->currT;
   m = tp->currm;

   showstack(tp, "BEG-reset-stack");

   // 1) count stack : currm -> currT
   while (m != t)
   {
      scnt++;
      m = m->par;
   }

   // array of Node *'s
   arr = (Node * *)zMalloc( "treestack", sizeof(Node *) * ( scnt + 1 ) );

   // 2) mark & copy stack
   sx = 0;
   m = tp->currm;
   while (m != t)
   {
      m->flags |= node_flags_Stack;
      arr[sx] = m;
      sx++;
      m = m->par;
      zeroNode(m,0);
   }
   m->flags |= node_flags_Stack;
   arr[sx]  = m;
   // stack in arr[0] -> arr[scnt]

   // 3) free all except stack
   //tree_free(t, 0);

   // 4) restore stack (currT=scnt-1 currm=0)
   m = tp->currm;
   m->chd = NULL;                       // currm last stack element
   m->sib = NULL;                       // no sibs on stack
   m->flags &= ~node_flags_Stack;

   for (sx = 1; sx <= scnt; sx++)
   {
      m = arr[sx];
      m->sib = NULL;                    // no sib in stack
      m->chd = arr[sx - 1];             // m is chd of par
      m->flags &= ~node_flags_Stack;
   }

   showstack(tp, "END-reset-stack");
}
#endif

//void hexDump( char * name, void ** p, int n ) // Dump n pointers at p
//{
//   int i;
//
//   fprintf(stderr, "%s =", name);
//
//   for ( i = 0; i < 8; i++ )
//   {
//      fprintf(stderr, " %p", *p++);
//   }
//
//   fprintf(stderr,"\n");
//}

void writeBaseForHeader( int met, char * name )
{
   if ( met )
   {
      fprintf(gd.tree, "  BASE-%d :: %s\n",
              met, name );
   }
   else
   {
      fprintf(gd.tree, "  BASE   :: %s\n",
              name );
   }
}

void writeColumnLabel( char * name, char * desc )
{
   if ( ge.newHeader )
   {
      fprintf(gd.tree, "  %-10s :: %s\n", name, desc );
   }
   else
   {
      fprintf(gd.tree, " : %-10s = %s\n", name, desc );
   }
}

void writeColumnForHeader( char * metNames, char * name, char * desc )
{
   char buffer[16];

   writeColumnLabel( name, desc );

   sprintf( buffer, " %10s", name );

   strcat( metNames, buffer );
}

/******************************/
void DefaultRTFileHeader()
{
   int    i       = 0;
   int    n;
   int    met     = 0;
   double value   = 0.1;
   UINT64 cycs;
   char   namebuf[256];

   UINT64 metAppl[gv_MaxMetrics];       // Metrics during Application
   UINT64 metInst[gv_MaxMetrics];       // Metrics during JVM Instrumentation
   UINT64 metJprf[gv_MaxMetrics];       // Metrics during Jprof
   UINT64 metGC  [gv_MaxMetrics];       // Metrics during GC

   char   metNames[gv_MaxMetrics * 16]; // Names used in headers
   char   metricName[16];               // Buffer for building scaled metric name

   hash_iter_t iter;
   thread_t  * tp;

   metNames[0] = 0;                     // Initialize headers to empty string

   if ( ge.newHeader )
   {
      fprintf(gd.tree, "\n Base Values:\n\n");

#ifndef _LEXTERN
   #if !defined(_ZOS)
      if ( ge.scs )
      {
         writeColumnLabel("SAMPLES","Callstack Samples");
      }
      else
   #endif
#endif
      if ( gv->scs_allocBytes )
      {
         if ( gv->objrecs )
         {
            writeColumnLabel("ALLOC_BYTE","Callstack Allocated Bytes samples");
         }
      }
      else if (gv->scs_classLoad)
      {
         writeColumnLabel("CLASS_LOAD","Callstack Class Load samples");
      }
      else if ( gv->scs_monitorEvent )
      {
         writeColumnLabel("EVENT","Callstack Monitor Event samples");
      }
      else if ( 0 == gv->scs_allocations )
      {
         for ( met = 0; met < gv->usermets; met++ )
         {
            if ( MODE_CPI != gv->usermet[met] )
            {
               if ( gv->sf[met] )
               {
                  strcpy( metricName, "SCALED_x" );
                  metricName[7] = (char)( '0' + met );

                  writeColumnLabel( metricName, getScaledName( namebuf, met ) );
               }
               else
               {
                  writeColumnLabel( ge.mmnames[gv->usermet[met]],
                                    ge.mmdescr[gv->usermet[met]] );
               }
            }
         }
      }

      if ( 0 == gv->objrecs )
      {
         if ( 0 == ( gv->scs_classLoad | gv->scs_monitorEvent ) )
         {
            if ( gv->scs_allocBytes )
            {
               if ( gv->scs_allocations || gv->scs_event_object_classes )
               {
                  if ( gv->scs_allocations > 1 || gv->scs_allocBytes > 1 )
                  {
                     writeColumnLabel("AO_SAMPLES","Callstack Allocated Object Samples");
                  }
                  else
                  {
                     writeColumnLabel("AO","Callstack Allocated Objects");
                  }
               }

               if ( gv->scs_allocBytes > 1 )
               {
                  writeColumnLabel("AB_SAMPLES","Callstack Allocated Bytes Samples");
               }
               else
               {
                  writeColumnLabel("AB","Callstack Allocated Bytes");
               }

               if ( ge.ObjectInfo )
               {
                  if ( gv->scs_allocations > 1 || gv->scs_allocBytes > 1 )
                  {
                     writeColumnLabel("LO_SAMPLES","Callstack Live Object Samples");
                     writeColumnLabel("LB_SAMPLES","Callstack Live Bytes Samples");
                  }
                  else
                  {
                     writeColumnLabel("LO","Callstack Live Objects");
                     writeColumnLabel("LB","Callstack Live Bytes");
                  }
               }
            }
            else if ( gv->scs_event_object_classes )
            {
               writeColumnLabel("AO","Callflow Allocated Objects");
               writeColumnLabel("AB","Callflow Allocated Bytes");

               if ( ge.ObjectInfo )
               {
                  writeColumnLabel("LO","Callflow Live Objects");
                  writeColumnLabel("LB","Callflow Live Bytes");
               }
            }
         }
      }
   }
   else                                 // Old Header
   {
#ifndef _LEXTERN
   #if !defined(_ZOS)
      if ( ge.scs )
      {
         writeBaseForHeader( met++, "Callstack Samples");
      }
      else
   #endif
#endif
         if ( gv->scs_allocBytes )
      {
         if ( gv->vpa63 > 1 )
         {
            writeBaseForHeader( met++, "RAW_CYCLES (2+Log2 of Callstack Allocated Bytes samples)");
         }
         else if ( gv->vpa63 )
         {
            writeBaseForHeader( met++, "RAW_CYCLES (Callstack Allocated Bytes samples)");
         }
         else if ( gv->objrecs )
         {
            writeBaseForHeader( met++, "Callstack Allocated Bytes samples");
         }
      }
      else if (gv->scs_classLoad)
      {
         writeBaseForHeader( met++, "Callstack Class Load samples");
      }
      else if ( gv->scs_monitorEvent )
      {
         writeBaseForHeader( met++, "Callstack Monitor Event samples");
      }
      else
      {
         for ( met = 0; met < gv->usermets; met++ )
         {
            writeBaseForHeader( met, getScaledName(namebuf,met) );
         }
      }

      if ( 0 == gv->objrecs )
      {
         if ( 0 == ( gv->scs_classLoad | gv->scs_monitorEvent ) )
         {
            if ( gv->scs_event_object_classes )
            {
               if ( gv->scs_allocBytes )
               {
                  writeBaseForHeader( met++, "Callstack Allocated Objects");
                  writeBaseForHeader( met++, "Callstack Allocated Bytes");

                  if ( ge.ObjectInfo )
                  {
                     writeBaseForHeader( met++, "Callstack Live Objects");
                     writeBaseForHeader( met++, "Callstack Live Bytes");
                  }
               }
               else
               {
                  writeBaseForHeader( met++, "Callflow Allocated Objects");
                  writeBaseForHeader( met++, "Callflow Allocated Bytes");

                  if ( ge.ObjectInfo )
                  {
                     writeBaseForHeader( met++, "Callflow Live Objects");
                     writeBaseForHeader( met++, "Callflow Live Bytes");
                  }
               }
            }
            else if ( gv->scs_allocBytes )
            {
               writeBaseForHeader( met++, "Callstack Allocated Bytes samples");
            }
         }
      }
   }                                    // End of oldHeader section

#if defined(_WIN32)
   fprintf(gd.tree, "\n Cycles_Per_sec : %10.2f\n" , (double)ge.cpuSpeed);
#endif

   fprintf(gc.msg, "gv->calltree  = %d\n", gv->calltree );
   fprintf(gc.msg, "gv->calltrace = %d\n", gv->calltrace );
   fprintf(gc.msg, "gv->usermets  = %d\n", gv->usermets);
   fprintf(gc.msg, "gv->physmets  = %d\n", gv->physmets);
   fprintf(gc.msg, "gv->start     = %d\n", gv->start   );
   fflush(gc.msg);

   if ( gv->legend )
   {
      fprintf(gd.tree, "\n Column Labels:\n\n");

      writeColumnLabel( "LV","Level of Nesting       (Call Depth)");

      if ( 0 == ge.newHeader
           || ( 0 == gv->scs_classLoad
                && 0 == gv->scs_monitorEvent
#ifndef _LEXTERN
                && 0 == ge.scs
#endif
                && 0 == gv->scs_allocBytes ) )
      {
         writeColumnLabel("CALLS","Calls to this Method   (Callers)");
         writeColumnLabel("CEE"  ,"Calls from this Method (Callees)");
      }

      if ( 0 == ge.newHeader )
      {
         writeColumnLabel("BASE","Metrics observed");
      }
      else
      {
#ifndef _LEXTERN
   #if !defined(_ZOS)
         if ( ge.scs )
         {
            writeColumnForHeader( metNames, "SAMPLES", "Callstack Samples");
         }
         else
   #endif
#endif
            if ( gv->scs_allocBytes )
         {
            if ( gv->objrecs )
            {
               writeColumnForHeader( metNames, "ALLOC_BYTE", "Callstack Allocated Bytes samples");
            }
         }
         else if (gv->scs_classLoad)
         {
            writeColumnForHeader( metNames, "CLASS_LOAD", "Callstack Class Load samples");
         }
         else if ( gv->scs_monitorEvent )
         {
            writeColumnForHeader( metNames, "EVENT", "Callstack Monitor Event samples");
         }
         else if ( 0 == gv->scs_allocations )
         {
            for ( met = 0; met < gv->usermets; met++ )
            {
               if ( gv->sf[met] )
               {
                  strcpy( metricName, "SCALED_x" );
                  metricName[7] = (char)( '0' + met );

                  writeColumnForHeader( metNames, metricName, getScaledName( namebuf, met ) );
               }
               else
               {
                  writeColumnForHeader( metNames,
                                        ge.mmnames[gv->usermet[met]],
                                        ge.mmdescr[gv->usermet[met]] );
               }
            }
         }

         if ( 0 == gv->objrecs )
         {
            if ( 0 == ( gv->scs_classLoad | gv->scs_monitorEvent ) )
            {
               if ( gv->scs_allocBytes )
               {
                  if ( gv->scs_allocations || gv->scs_event_object_classes )
                  {
                     if ( gv->scs_allocations > 1 || gv->scs_allocBytes > 1 )
                     {
                        writeColumnForHeader( metNames, "AO_SAMPLES","Callstack Allocated Object Samples");
                     }
                     else
                     {
                        writeColumnForHeader( metNames, "AO","Callstack Allocated Objects");
                     }
                  }

                  if ( gv->scs_allocBytes > 1 )
                  {
                     writeColumnForHeader( metNames, "AB_SAMPLES","Callstack Allocated Bytes Samples");
                  }
                  else
                  {
                     writeColumnForHeader( metNames, "AB","Callstack Allocated Bytes");
                  }

                  if ( ge.ObjectInfo )
                  {
                     if ( gv->scs_allocations > 1 || gv->scs_allocBytes > 1 )
                     {
                        writeColumnForHeader( metNames, "LO_SAMPLES","Callstack Live Object Samples");
                        writeColumnForHeader( metNames, "LB_SAMPLES","Callstack Live Bytes Samples");
                     }
                     else
                     {
                        writeColumnForHeader( metNames, "LO","Callstack Live Objects");
                        writeColumnForHeader( metNames, "LB","Callstack Live Bytes");
                     }
                  }
               }
               else if ( gv->scs_event_object_classes )
               {
                  writeColumnForHeader( metNames, "AO","Callflow Allocated Objects");
                  writeColumnForHeader( metNames, "AB","Callflow Allocated Bytes");

                  if ( ge.ObjectInfo )
                  {
                     writeColumnForHeader( metNames, "LO","Callflow Live Objects");
                     writeColumnForHeader( metNames, "LB","Callflow Live Bytes");
                  }
               }
            }

         }
      }

      if ( gv->TSnodes )
      {
         writeColumnLabel("TS","Timestamp at Node Creation");
      }

      if ( ge.ObjectInfo && gv->objrecs )
      {
         writeColumnLabel("AO","Allocated Objects");
         writeColumnLabel("AB","Allocated Bytes");
         writeColumnLabel("LO","Live Objects");
         writeColumnLabel("LB","Live Bytes");
      }
      if ( gv->pnode )
      {
         if ( gv->idpnode )
         {
            writeColumnLabel("ID","ID of Method or Thread");
         }
         else
         {
            writeColumnLabel("pNode","Address of Tree Node");
         }
      }
      writeColumnLabel("NAME","Name of Method or Thread");

      MethodTypeLegend( gd.tree );
   }

   OptVMsgLog( "Start at %" _P64 "x, Flush at %" _P64 "x\n\n",
               gv->timeStampStart,
               gv->timeStampFlush );

#ifndef _LEXTERN
   if ( ge.fSystemResumed )
   {
      cycs = ge.system_sleep_timestamp - gv->timeStampStart;   // Compute elapsed cycles
   }
   else
#endif //_LEXTERN
   {
      cycs = gv->timeStampFlush - gv->timeStampStart;   // Compute elapsed cycles
   }

#ifndef _LEXTERN
   if ( gv->stknodes )
   {
      if ( gc_mask_CallStack_Stats & gc.mask )
      {
         hashIter( &ge.htThreads, &iter );   // Iterate thru the thread table

         while ( ( tp = (thread_t *)hashNext( &iter ) ) )
         {
            if ( tp->fValid )
            {
               OptVMsgLog( "%s\n", tp->thread_name );

               hashStats( &tp->htStkNodes );
            }
         }
      }
   }
#endif //_LEXTERN

   fprintf(gd.tree, "\n Process_Number_ID=%u\n", (unsigned int)ge.pid);

   fprintf(gd.tree, "\n  LV");

   if ( 0 == ge.newHeader
        || ( 0 == gv->scs_classLoad
             && 0 == gv->scs_monitorEvent
#ifndef _LEXTERN
             && 0 == ge.scs
#endif
             && 0 == gv->scs_allocBytes ) )
   {
      if ( ge.newHeader )
      {
         fprintf(gd.tree, "      CALLS        CEE");
      }
      else
      {
         fprintf(gd.tree, " CALLS        CEE");
      }
   }

   if ( 0 == ge.newHeader )
   {
      fprintf(gd.tree, "       BASE");

      n = gv->usermets;

      if ( 0 == gv->objrecs )
      {
         if ( gv->scs_event_object_classes )
         {
            if ( ge.ObjectInfo )
            {
               n += 4;
            }
            else if ( gv->scs_allocBytes )
            {
               n += 2;
            }
            else
            {
               n++;
            }

            if ( gv->scs_monitorEvent
                 || gv->scs_classLoad
                 || gv->scs_allocBytes )
            {
               n--;                     // First metric already included
            }
         }
      }

      for (i = 1; i < n; i++)
      {
         fprintf(gd.tree, "     BASE-%d", i);
      }
   }
   else
   {
      fprintf( gd.tree, metNames );
   }

   if ( gv->TSnodes )
   {
      fprintf(gd.tree, "               TS");
   }

   if ( ge.ObjectInfo && gv->objrecs )
   {
      fprintf(gd.tree, "         AO         AB         LO         LB");
   }

   if ( 0 == gv->pnode )
   {
      fprintf(gd.tree, "  NAME\n");
   }
   else if (gv->idpnode)
   {
      fprintf(gd.tree,
              ( ge.is64bitCPU
                ? "                ID  NAME\n"
                : "        ID  NAME\n" ) );
   }
   else
   {
      fprintf(gd.tree,
              ( ge.is64bitCPU
                ? "             pNode  NAME\n"
                : "     pNode  NAME\n" ) );
   }
   fflush(gd.tree);
}


//
// remove_blanks_inplace()
// ***********************
//
// Removes all blank characters from str, in place.
//
void remove_blanks_inplace(char * str)
{
   char * c, * s;

   // Find first blank, if any
   c = strchr(str, ' ');
   if (!c)
      return;

   // Remove beginning at the first blank
   s = c;
   do {
      if (*s != ' ') {
         *(c++) = *s;
      }
   } while (*(s++));

   return;
}

/******************************/
char * NodeName(Node * np, char * buf)
{
   char       pre[4] = "x:";
   char     * c;
   char     * m;
   char     * srcName = NULL;

   if (np == NULL)
   {
      sprintf(buf, "-");
      WriteFlush(gc.msg, "NULL node ptr in NodeName\n");
      return(buf);
   }
   pre[0] = '\0';

   if (gv->methodTypes && (np->ntype != gv_type_Thread))
   {
      pre[0] = gv->types[np->ntype];    // Create the "X:" prefix
   }

   //OVMsg(("NodeName: np->id = %p, np->ntype = %d\n", np->id, np->ntype ));

   switch (np->ntype)
   {
   case gv_type_Thread:

      strcpy(buf, ((thread_t *)np->id)->thread_name);
      break;

   case gv_type_LUAddrs:                // Should never be seen

      strcpy(buf,  "a:List_of_Unresolved_Addresses");
      break;

   case gv_type_RawUserAddr:            // Should never be seen

      sprintf(buf, "u:addr_%p", np->id);
      break;

   case gv_type_RawKernelAddr:          // Should never be seen

      sprintf(buf, "k:addr_%p", np->id);
      break;

   case gv_type_Stem:                   // jstem

      sprintf(buf, "A:addr_%p", np->id);
      break;

   case gv_type_interp:
   case gv_type_Interp:
   case gv_type_jitted:
   case gv_type_Jitted:
   case gv_type_reJitted:
   case gv_type_ReJitted:
   case gv_type_native:
   case gv_type_Native:
   case gv_type_builtin:
   case gv_type_Builtin:
   case gv_type_Compiling:
   case gv_type_Other:
   case gv_type_inLine:
   case gv_type_scs_Method:

      getMethodName( (method_t *)np->id, &c, &m);

      if ( ge.ExtendedCallStacks )
      {
         method_t * mp = (method_t *)np->id;

         if (mp && mp->class_entry && mp->class_entry->source_name)
         {
            srcName = mp->class_entry->source_name;
         }
      }
      if ( srcName )
      {
         if ( *getLineNum(np) )
         {
            sprintf(buf, "%s%s.%s@%s:%d", pre, c, m, srcName, *getLineNum(np));
         }
         else
         {
            sprintf(buf, "%s%s.%s@%s", pre, c, m, srcName);
         }
      }
      else
      {
         sprintf(buf, "%s%s.%s", pre, c, m);
      }
      break;

   case gv_type_scs_User:
   case gv_type_scs_Kernel:

      sprintf(buf, "%s%s", pre, ((method_t *)np->id)->method_name);
      break;

   case gv_type_Information:

      strcpy(buf, ((method_t *)np->id)->method_name);
      break;

   default:

      sprintf(buf, "%saddr_%p", pre, np->id);
      break;
   }

   if ( gc.mask                         // Debugging mode
        && NNBuf < 1 + strlen(buf) )
   {
      WriteFlush(gc.msg, " NodeName ERROR: name > %d characters\n", NNBuf);
   }
   //OVMsg(("NodeName: %s\n", buf));
   return(buf);
}

/******************************/
void * thread_node(thread_t * tp)
{
   FrameInfo fi;

   fi.ptr     = tp;
   fi.type    = gv_type_Thread;
   fi.linenum = 0;

   return( constructNode( &fi ) );
}

/******************************/
UINT64 *getNodeCounts(Node * p)
{
   return( (UINT64 *)( ge.offNodeCounts + (char *)p ) );
}

/******************************/
UINT64 *getTimeStamp(Node * p)
{
   return( (UINT64 *)( ge.offTimeStamp + (char *)p ) );
}

/******************************/
RTEventBlock **getEventBlockAnchor(Node * p)
{
   return( (RTEventBlock **)( ge.offEventBlockAnchor + (char *)p ) );
}

/******************************/
UINT32 *getLineNum(Node * p)
{
   return( (UINT32 *)( ge.offLineNum + (char *)p ) );
}

/******************************/
Node * constructNode( FrameInfo * fi )  // Construct node from FrameInfo
{
   Node * np = (Node *)zMalloc( "constructNode", ge.nodeSize );

   np->id    = fi->ptr;
   np->ntype = fi->type;

   if ( gv )
   {
      if ( gv_type_Thread != fi->type )
      {
         gv->tnodes++;
      }
      if ( ge.ExtendedCallStacks )
      {
         *getLineNum(np) = fi->linenum;
      }
   }
   return( np );
}

/******************************/
int testNode( Node * np, FrameInfo *fi )
{
   return( ( np->id == fi->ptr )
           && ( ( 0 == gv->methodTypes )
                || ( np->ntype       == fi->type ) )   // FIX ME:  Is this still valid?
           && ( ( 0 == ge.ExtendedCallStacks )
                || ( *getLineNum(np) == fi->linenum ) ) );
}

//----------------------------------------------------------------------
// Find a child of the parent that matches pComp, adding it if necessary
//----------------------------------------------------------------------

Node * push( Node      * pParent,       // Parent of possible new node
             FrameInfo * fi )           // Fake node for comparison
{
   Node * pCurChild;
   Node * pLastChild;
   Node * pNewChild;

   if ( gv->calltrace && 0 == gv->calltree )
   {
      return( pParent );                // Only update thread nodes if NOTREES
   }

   pCurChild  = pParent->chd;           // Current child starts with first child

   while ( pCurChild )                  // Loop optimized to find node already in tree (last child not remembered)
   {
      if ( testNode( pCurChild, fi ) )
      {
         return(pCurChild);             // QUICK RETURN:  This node is already a child of pParent
      }
      pCurChild = pCurChild->sib;       // Make next sibling the current child
   }

   pNewChild      = constructNode( fi );   // Construct the new node
   pNewChild->par = pParent;

//-----------------------------------------------------------------------------

   if ( gv->lockTree )                  // Contention is possible when flushing while profiling
   {
      enter_lock( TreeLock, TREELOCK );

      pLastChild = 0;                   // Last child of this parent (may not yet exist)
      pCurChild  = pParent->chd;        // Current child starts with first child

      while ( pCurChild )               // Check once more, in case it was added since we checked
      {
         if ( testNode( pCurChild, fi ) )
         {
            leave_lock( TreeLock, TREELOCK );

            xFree( pNewChild, ge.nodeSize );   // Free the node we just allocated, since we no longer need it

            return( pCurChild );        // QUICK RETURN:  This node is already a child of pParent
         }
         pLastChild = pCurChild;        // Save current child as the previous sibling
         pCurChild  = pCurChild->sib;   // Make next sibling the current child
      }

      if ( gv->revtree )                // Add nodes in time order ( THIS IS THE DEFAULT CASE )
      {
         if ( pLastChild )              // There are already children of pParent
         {
            pLastChild->sib = pNewChild;   // Add as the newest sibling (POSSIBLE to lose a branch without lock)
         }
         else
         {
            pParent->chd    = pNewChild;   // Add new node as the only child (POSSIBLE to lose a branch without lock)
         }
      }
      else                              // New nodes at the start of the sibling chain
      {
         pNewChild->sib = pParent->chd; // New child becomes first sibling
         pParent->chd   = pNewChild;    // New child becomes first child
      }
      leave_lock( TreeLock, TREELOCK );
   }

//-----------------------------------------------------------------------------

   else if ( 0 == pParent->chd )        // No children
   {
      pParent->chd   = pNewChild;       // New child becomes only child
   }

   else if ( 0 == gv->revtree )         // Order not important
   {
      pNewChild->sib = pParent->chd;    // New child becomes first sibling
      pParent->chd   = pNewChild;       // New child becomes first child
   }

   else                                 // Add nodes in time order ( THIS IS THE DEFAULT CASE )
   {
      pLastChild = 0;                   // ( Needed to avoid warning )
      pCurChild  = pParent->chd;        // Current child starts with first child

      while ( pCurChild )               // Find the last child, which we already know exists
      {
         pLastChild = pCurChild;        // Save current child as the previous sibling
         pCurChild  = pCurChild->sib;   // Make next sibling the current child
      }
      pLastChild->sib = pNewChild;      // Add as the newest sibling (POSSIBLE to lose a branch without lock)
   }
   // Code will return after this

   if ( gv->checkNodeDepth )
   {
      if ( 0 == ( gv->tnodes % gv->checkNodeDepth ) )
      {
         method_t * mp = (method_t *)pNewChild->id;
         Node     * np = pNewChild;
         int        n  = 0;             //Node depth
         char       buffer[MAX_PATH];

         while ( np->ntype != gv_type_Thread )
         {
            np = np->par;
            n++;
         }

         OptVMsgLog( "Node# %8d, Depth: %4d, %s\n",
                     gv->tnodes,
                     n,
                     NodeName( np, buffer ) );
      }
   }

   return( pNewChild );
}

Node * popCurrentNode( thread_t * tp )
{
   static int foundMismatch = 0;
   Node     * pCurMethod;

   pCurMethod = tp->currm;

   if ( gv->calltree )
   {
      tp->currm = pCurMethod->par;      // Set current method back to parent
   }

   if ( 0 == ( tp->flags & thread_mtypedone ) )
   {
      if ( tp->mtype_depth > 0 )
      {
         tp->mtype_depth--;             // Discard the current entry
      }
   }
   tp->depth--;

   if ( tp->depth != tp->mtype_depth )
   {
      if ( 0 == foundMismatch )
      {
         ErrVMsgLog( "DEPTH mismatch: tid = %p, mid = %p, depth = %d, mtype_depth = %d\n",
                     tp->tid, tp->mp->method_id, tp->depth, tp->mtype_depth );
         foundMismatch = 1;
      }
   }

   return( (Node *)tp->currm );
}

/******************************/
int RTEnEx( thread_t * tp )
{
   char      buf1[NNBuf];
   char      buf2[NNBuf];

   Node *    pCurMethod = (Node *)(tp->currm);   // Current method on thread
   Node *    p;

   FrameInfo fi;

   UINT64  * counts;
   int       met;

   if ( gv->trees )
   {
      fi.ptr     = tp->mp;
      fi.type    = tp->mt3;
      fi.linenum = 0;

//----------------------------------------------------------------------
// Correct any exit node mismatches before calibration
//----------------------------------------------------------------------

      if ( gv_ee_Entry != tp->x23       // METHOD EXIT
           && pCurMethod != tp->currT   // Valid Method node (not the Thread node)
           && tp->mp != pCurMethod->id )   // Method not current method in tree
      {
         static int infomsg = 0;

         if ( infomsg < gv->msglimit || gc.verbose )
         {
            Node n;

            n.id    = tp->mp;
            n.ntype = tp->mt3;

            infomsg++;

            OptVMsgLog(" INFO - Exiting non-current method on %s\n   Cur %p %s\n   mid %p %s\n",
                       tp->thread_name,
                       pCurMethod->id,    NodeName(pCurMethod, buf1),
                       tp->mp->method_id, NodeName(&n,         buf2));

            while ( pCurMethod != tp->currT   // Search back toward Thread node
                    && pCurMethod->id != tp->mp )
            {
               WriteFlush(gc.msg, " popping: %s\n", NodeName(pCurMethod, buf1));

               tp->flags &= ~thread_mtypedone;   // Allow popping of mtype_stack

               pCurMethod = popCurrentNode( tp );
            }

            if ( pCurMethod == tp->currT )
            {
               OptVMsgLog(" Method not found on thread stack, current set to thread level\n");

               tp->depth       = 0;
               tp->mtype_depth = 0;
            }
         }
      }

      check_mtype_stack( tp );

      timeApply(tp);                    // Apply base times to the nodes

//----------------------------------------------------------------------
// Report event in same format as trace.c
//----------------------------------------------------------------------

      if ( gc.mask & gc_mask_Show_Methods )
      {
         OVMsg(( "%s: tid = %p, cpu = %d, ts = %" _P64 "x, mid = %p, type = %c",
                 ( gv_ee_Entry == tp->x23 )
                 ? "EN"
                 : "EX",
                 tp->tid,
                 tp->cpuNum,
                 tp->timeStamp,
                 tp->mp->method_id,
                 gv->types[ tp->mt3 ] ));

         OVMsg(( ", delmm =" ));

         for ( met = 0; met <= gv->physmets; met++ )
         {
            OVMsg(( " %6d", (int)tp->metDelt[met] ));
         }

         // FIX ME:  Possibly add the node name to this output

         OVMsg(( "\n" ));
      }

//----------------------------------------------------------------------
// Process a METHOD_ENTRY event
//----------------------------------------------------------------------

      if ( tp->x23 == gv_ee_Entry )     // METHOD ENTRY
      {
         tp->depth++;

         if ( gv->calltree )
         {
            pCurMethod = push( pCurMethod, &fi );   // Add to the current call stack

            tp->currm = pCurMethod;     // Set new current method on thread

            if ( tp->currm != tp->currT )   // Not at the Thread node
            {
               if ( gv->addbase )
               {
                  counts = getNodeCounts(pCurMethod);

                  if ( gv_ee_Entry == tp->x12 )
                  {
                     counts[ 2 * gv_ee_Entry + gv_ee_Entry ]++;
                  }
                  else
                  {
                     counts[ 2 * gv_ee_Exit  + gv_ee_Entry ]++;   // Assume exen if prev is unknown
                  }
               }
            }
         }
      }

//----------------------------------------------------------------------
// Process a METHOD_EXIT event
//----------------------------------------------------------------------

      else                              // METHOD EXIT
      {
         if ( pCurMethod != tp->currT ) // Valid Method node, not the Thread node
         {
            if ( gv->addbase )
            {
               counts = getNodeCounts(pCurMethod);

               if ( gv_ee_Entry == tp->x12 )
               {
                  counts[ 2 * gv_ee_Entry + gv_ee_Exit ]++;
               }
               else
               {
                  counts[ 2 * gv_ee_Exit  + gv_ee_Exit ]++;   // Use exex if prev is unknow
               }
            }
            popCurrentNode( tp );       // Set current method to parent
         }

         //----------------------------------------------------------------
         // Method Exit when current method is already the Thread node:
         // -----------------------------------------------------------
         // This is the normal case for handling a disjoint tree branch
         // due to a late start.  It assumes that any nodes beyond this
         // one in the tree are really called by the method we are now
         // exiting.  To correct this, a new method node will be inserted
         // between the thread node and the first of the old method nodes,
         // if any.
         //
         // The new Node, p, will become the new Thread Node, tp->currT.
         // The old Thread Node, pCurMethod, becomes a real method node.
         //----------------------------------------------------------------

         else if ( gv->calltree )       // pCurMethod == tp->currm == tp->currT
         {
            OVMsg(( "Inserting method node for %s", tp->thread_name ));

            p      = thread_node(tp);   // Create new Thread node, all zero
            p->chd = tp->currT;         // Make old Thread node the first child

            pCurMethod->id    = tp->mp; // Id   of exiting method
            pCurMethod->ntype = tp->mt3;   // Type of exiting method

            pCurMethod->par   = p;      // Attach it to new Thread node

            tp->currT         = p;      // Establish new node as Thread Node
            tp->currm         = p;      // Logically return to new Thread Node

            OVMsg(( ": %s\n", NodeName( pCurMethod, buf1 ) ));

            // Record the entry for this exit (default to entry after exit)

            counts = getNodeCounts( pCurMethod );

            counts[ 2 * gv_ee_Exit + gv_ee_Entry ] = 1;
         }
      }

//----------------------------------------------------------------------
// Add a timeStamp to the node, if requested
//----------------------------------------------------------------------

      if ( gv->TSnodes
           && tp->currm
           && 0 == *(getTimeStamp(tp->currm)) )
      {
         *(getTimeStamp(tp->currm)) = tp->timeStamp;

         if ( tp->currT
              && 0 == *(getTimeStamp(tp->currT)) )
         {
            *(getTimeStamp(tp->currT)) = tp->timeStamp;
         }
      }
   }

   return(0);
}

/******************************/
void xtreeLine(Node * pn, int level)
{
   RTEventBlock * pRTEB;
   char           nmbuf[NNBuf];
   UINT64         base;
   UINT64         abase;
   UINT64         callees = 0;
   UINT64         calleze[gv_ee_ExitPop+1][gv_type_Other+1] = { 0};   // Counts by x12 and mt3
   Node         * pnChild;              // Pointer for walking child nodes

   UINT64 delta[gv_MaxMetrics];         // Delta per Metric

   UINT64 totalObjects = 0;
   UINT64 totalBytes   = 0;
   UINT64 liveObjects  = 0;
   UINT64 liveBytes    = 0;

   UINT64 xbase;                        // Extra base value for composite metric

   int    usermet;                      // Logical Metric Index
   int    physmet;                      // Physical Metric Index

   int    typeParent;
   int    typeSelf;
   int    typeChild;

   uint32_t id;

   UINT64 *counts;
   char ts_time[32];                    // Holds converted timestamp

#ifndef _LEXTERN
   if ( ge.scs )
   {
      if ( ge.jvmti )
      {
         resolveLUAddrs( pn );          // Resolve any List of Unresolved Addresses
      }
      else if ( ge.EnExCs )             // Hookit with Raw Addresses
      {
         resolveRawAddrs( pn );
      }
   }
#endif

   if ( 0 == pn->chd )                  // No children
   {
      for ( usermet = 0;
          usermet < gv->usermets && 0 == pn->bmm[usermet];
          usermet++ )
      {
      }
      if ( usermet >= gv->usermets )    // No metric data
      {
         if ( 0 == gv->scs_event_object_classes
              || 0 == *getEventBlockAnchor(pn) )   // No event blocks
         {
            return;                     // There is no reason to report this node
         }
      }
   }

   typeParent = gv_type_Other;

   if ( pn->par )                       // Method has parent
   {
      typeParent = pn->par->ntype;      // Get type of parent

      if ( typeParent > gv_type_Other )
      {
         typeParent = gv_type_Other;
      }
   }

   typeSelf = pn->ntype;                // Get type of method

   if ( typeSelf > gv_type_Other )
   {
      typeSelf = gv_type_Other;
   }

   if ( 0 == level && gv->HumanReadable )
   {
      WriteFlush( gd.tree, "\n" );
   }

   WriteFlush( gd.tree, "%4d", level ); //*****  LV

   if ( gv->scs_monitorEvent
#ifndef _LEXTERN
        || ge.scs
#endif
        || gv->scs_classLoad
        || gv->scs_allocBytes )         // Calls and Callees not used by SCS
   {
      if ( 0 == ge.newHeader )
      {
         WriteFlush( gd.tree, "     0          0" );   //*****      CALLS        CEE
      }
   }
   else
   {
      pnChild = pn->chd;

      while ( NULL != pnChild )
      {
         typeChild = pnChild->ntype;    // Get type of child

         if ( typeChild > gv_type_Other )
         {
            typeChild = gv_type_Other;
         }
         counts = getNodeCounts(pnChild);

         calleze[gv_ee_Entry][typeChild] += counts[ 2 * gv_ee_Entry + gv_ee_Entry ];
         callees                         += counts[ 2 * gv_ee_Entry + gv_ee_Entry ];

         calleze[gv_ee_Exit ][typeChild] += counts[ 2 * gv_ee_Exit  + gv_ee_Entry ];
         callees                         += counts[ 2 * gv_ee_Exit  + gv_ee_Entry ];

         pnChild  = pnChild->sib;       // Get next child of current node
      }

      counts = getNodeCounts(pn);

      WriteFlush( gd.tree,
                  ge.newHeader
                  ? " %10" _P64d " %10" _P64d   //*****      CALLS        CEE
                  : " %5" _P64d " %10" _P64d,   //***** CALLS        CEE
                  counts[   2 * gv_ee_Entry + gv_ee_Entry ]
                  + counts[ 2 * gv_ee_Exit  + gv_ee_Entry ],
                  callees);
   }

   if ( gv->objrecs
        || 0 == ( gv->scs_monitorEvent | gv->scs_classLoad | gv->scs_allocBytes )
        || ( 0 == gv->scs_event_object_classes && 0 == gv->scs_allocations ) )
   {
      //***** BASE  ...  BASE-n
      for ( usermet = 0; usermet < gv->usermets; usermet++ )
      {
         physmet = gv->mapmet[ usermet ];

         abase = pn->bmm[physmet];
         base  = pn->bmm[physmet];

         delta[usermet] = 0;

         if (base > 0)
         {
            if ( 0 == usermet )
            {
               if ( 0 != ( gc.mask & gc_mask_EnEx_Details ) )
               {
                  counts = getNodeCounts(pn);

                  WriteFlush( gc.msg,
                              "%4d %5" _P64d " %5" _P64d " %5" _P64d " %5" _P64d " %5" _P64d " %6" _P64d " %6" _P64d " %s\n\n",
                              level,
                              counts[ 2 * gv_ee_Entry + gv_ee_Entry ],
                              counts[ 2 * gv_ee_Exit  + gv_ee_Entry ],
                              counts[ 2 * gv_ee_Entry + gv_ee_Exit  ],
                              counts[ 2 * gv_ee_Exit  + gv_ee_Exit  ],
                              callees,
                              delta[usermet],
                              abase,
                              NodeName(pn, nmbuf));
               }
            }
         }

         if ( MODE_CPI == gv->usermet[usermet] )   // Cycles per Instruction
         {
            xbase = pn->bmm[gv->indexPTTinsts];   // xbase is PTT_INSTS

            WriteFlush(gd.tree, " %10.2f",
                       xbase
                       ? (double)abase / (double)xbase
                       : 0 );
         }
         else
         {

            if ( gv->vpa63 > 1 && gv->scs_allocBytes )
            {
               abase = (UINT64)( 2 + intLog2( abase ) );
            }
            else if ( gv->msf[usermet] )   // Scaling required
            {
               abase = (UINT64)( ((double)abase) / gv->msf[usermet] );
            }
            WriteFlush(gd.tree, " %10" _P64 "d",   //***** BASE[i]
                       abase);          // i-th metric
         }
      }
   }

   if ( gv->scs_event_object_classes )
   {
      if ( 0 == gv->objrecs )
      {
         if ( ge.ObjectInfo )
         {
            WriteFlush(gd.tree,
                       "          0          0          0          0" );
         }
         else if ( gv->scs_allocBytes )
         {
            WriteFlush(gd.tree,
                       "          0          0" );
         }
         else
         {
            WriteFlush(gd.tree,
                       "          0" );
         }
      }
   }
   else if ( gv->scs_allocations )
   {
      WriteFlush(gd.tree,
                 " %10" _P64d " %10" _P64d,
                 pn->bmm[0], pn->bmm[1] );
   }

   if ( gv->TSnodes )
   {
      if (gv->TSnodes_HR) {
         ts_to_time(*(getTimeStamp(pn)), ts_time);
         WriteFlush(gd.tree, " %s", ts_time);
      }
      else {
         WriteFlush( gd.tree, " %16" _P64 "X", *(getTimeStamp(pn)) );
      }
   }

   if ( ge.ObjectInfo && gv->objrecs )
   {
      totalObjects = 0;
      totalBytes   = 0;
      liveObjects  = 0;
      liveBytes    = 0;

      pRTEB = *getEventBlockAnchor(pn);

      while ( pRTEB )
      {
         totalObjects += pRTEB->totalObjects;
         totalBytes   += pRTEB->totalEvents;
         liveObjects  += pRTEB->liveObjects;
         liveBytes    += pRTEB->liveBytes;

         pRTEB = pRTEB->next;
      }

      //***** AO AB LO LB
      WriteFlush(gd.tree, " %10"_P64d" %10"_P64d" %10"_P64d" %10"_P64d,
                 totalObjects,
                 totalBytes,
                 liveObjects,
                 liveBytes );
   }

   if ( gv->pnode )                     //***** pNode
   {
      id = PtrToUint32( pn );

      if ( gv->idpnode && pn )
      {
         id = PtrToUint32( pn->id );

         if ( id && gv_type_Thread == pn->ntype )
         {
            id = PtrToUint32( ( (thread_t *)(pn->id) )->tid );
         }
      }

      WriteFlush(gd.tree, "  %p",
                 id );
   }
   WriteFlush(gd.tree, "  %s\n",        //*****  NAME
              NodeName(pn, nmbuf));

//----------------------------------------------------------------------
// Display Object Info Related to Events
//----------------------------------------------------------------------

   if ( gv->scs_event_object_classes )
   {
      pRTEB = *getEventBlockAnchor(pn);

      if ( pRTEB )
      {
         if ( gv->HumanReadable )
         {
            WriteFlush( gd.tree, "\n" );
         }
         while ( pRTEB )
         {
            if ( gv->objrecs )
            {
               if ( ge.ObjectInfo )
               {
                  WriteFlush(gd.tree,
                             "  -- %10" _P64d " %10" _P64d " %10" _P64d " %10" _P64d,
                             pRTEB->totalObjects,
                             pRTEB->totalEvents,
                             pRTEB->liveObjects,
                             pRTEB->liveBytes );
               }
               else if ( gv->scs_allocBytes )
               {
                  WriteFlush(gd.tree,
                             "  -- %10" _P64d " %10" _P64d,
                             pRTEB->totalObjects,
                             pRTEB->totalEvents );
               }
               else
               {
                  WriteFlush(gd.tree,
                             "  -- %10" _P64d,
                             pRTEB->totalEvents );
               }

#ifndef _LEXTERN
               if ( gv->DistinguishLockObjects )
               {
   #if defined(_64BIT)
                  WriteFlush(gd.tree,
                             "  %s@%.16lX\n",
                             pRTEB->cp->class_name,
                             pRTEB->tagEventObj );
   #else
                  WriteFlush(gd.tree,
                             "  %s@%.8X\n",
                             pRTEB->cp->class_name,
                             pRTEB->tagEventObj );
   #endif //_64BIT
               }
               else
#endif //_LEXTERN
               {
                  WriteFlush( gd.tree,
                              "  %s\n",
                              pRTEB->cp->class_name );
               }
            }
            else                        // NOOBJRECS
            {
               WriteFlush( gd.tree, "%4d", level+1 );   //*****  LV

               if ( 0 == ge.newHeader
                    || ( 0 == gv->scs_classLoad
                         && 0 == gv->scs_monitorEvent
#ifndef _LEXTERN
                         && 0 == ge.scs
#endif
                         && 0 == gv->scs_allocBytes ) )
               {
                  if ( ge.newHeader )
                  {
                     WriteFlush( gd.tree, "          0          0" );   //*****      CALLS        CEE
                  }
                  else
                  {
                     WriteFlush( gd.tree, "     0          0" );   //***** CALLS        CEE
                  }
               }

               if ( 0 == ( gv->scs_monitorEvent | gv->scs_classLoad | gv->scs_allocBytes )
                    || 0 == gv->scs_event_object_classes )
               {
                  for ( usermet = 0; usermet < gv->usermets; usermet++ )
                  {
                     WriteFlush(gd.tree, "          0" );   //***** BASE[i], i-th metric
                  }
               }

               if ( ge.ObjectInfo )
               {
                  WriteFlush(gd.tree,
                             " %10" _P64d " %10" _P64d " %10" _P64d " %10" _P64d,
                             pRTEB->totalObjects,
                             pRTEB->totalEvents,
                             pRTEB->liveObjects,
                             pRTEB->liveBytes );
               }
               else if ( gv->scs_allocBytes )
               {
                  WriteFlush(gd.tree,
                             " %10" _P64d " %10" _P64d,
                             pRTEB->totalObjects,
                             pRTEB->totalEvents );
               }
               else
               {
                  WriteFlush(gd.tree,
                             " %10" _P64d,
                             pRTEB->totalEvents );
               }

               if ( gv->TSnodes )
               {
                  WriteFlush( gd.tree, "                0" );
               }

               if ( gv->pnode )         //***** pNode
               {
                  WriteFlush( gd.tree,
                              ge.is64bitCPU
                              ? "                 0"
                              : "         0" );
               }

#ifndef _LEXTERN
               if ( gv->DistinguishLockObjects )
               {
   #if defined(_64BIT)
                  WriteFlush(gd.tree,
                             "  (%s@%.16lX)\n",
                             pRTEB->cp->class_name,
                             pRTEB->tagEventObj );
   #else
                  WriteFlush(gd.tree,
                             "  (%s@%.8X)\n",
                             pRTEB->cp->class_name,
                             pRTEB->tagEventObj );
   #endif //_64BIT
               }
               else
#endif //_LEXTERN
               {
                  WriteFlush( gd.tree,
                              "  (%s)\n",
                              pRTEB->cp->class_name );
               }
            }
            pRTEB = pRTEB->next;
         }
         if ( gv->HumanReadable )
         {
            WriteFlush( gd.tree, "\n" );
         }
      }
   }
}

//----------------------------------------------------------------------
// treeWalker(func,node) (replaced xtree recursive function, 20060215)
//----------------------------------------------------------------------
//
//  root = 1          This routine walks a tree of nodes which contain
//        / \         parent, child, and sibling pointers shown in the
//      --   --       graph to the left.  On entry:
//     /       \
//    2         5     func = Function to perform on each node.
//   / \       / \           func(currentNode,currentLevel) is invoked.
//  /   \     /   \   node = root node of tree to walk.
// 3     4   6     7  (Nodes processed in numerical order)
//
//----------------------------------------------------------------------

void treeWalker( void (*func)(Node *,int), Node * pn )
{
   char fDescending = 1;
   int  level       = 0;

   MVMsg( gc_mask_Show_Locks,
          (">>treeWalker(%p,%p)\n",func,pn));

   (*func)(pn,level);                   // Process the root

   for (;;)
   {
      if ( fDescending && pn->chd )
      {
         pn = pn->chd;
         level++;
         (*func)(pn,level);             // Process the child
      }
      else if ( pn->sib )
      {
         pn = pn->sib;
         (*func)(pn,level);             // Process the sibling
         fDescending = 1;
      }
      else if ( level > 0 )
      {
         pn = pn->par;
         level--;                       // Return to parent
         fDescending = 0;
      }
      else
      {
         break;
      }
   }
   MVMsg( gc_mask_Show_Locks,
          ("<<treeWalker(%p,%p)\n",func,pn));
}

//----------------------------------------------------------------------
// treeKiller(func,node) (replaced tree_free recursive function, 20060525)
//----------------------------------------------------------------------
//
//  root = 7          This routine walks a tree of nodes which contain
//        / \         parent, child, and sibling pointers shown in the
//      --   --       graph to the left.  On entry:
//     /       \
//    3         6     func = Function to perform on each node.
//   / \       / \           func(currentNode,currentLevel) is invoked.
//  /   \     /   \   node = root node of tree to walk, allowing deletion.
// 1     2   4     5  (Nodes processed in numerical order)
//
//----------------------------------------------------------------------

void treeKiller( void (*func)(Node *,int), Node * pn )
{
   char   fDescending = 1;
   int    level       = 0;
   Node * p;

   MVMsg( gc_mask_Show_Locks,
          (">>treeKiller(%p,%p)\n",func,pn));

   for (;;)
   {
      if ( gc.ShuttingDown )
      {
         return;                        // Stop freeing data if already shutting down
      }
      if ( fDescending && pn->chd )
      {
         pn = pn->chd;
         level++;
      }
      else if ( pn->sib )
      {
         p  = pn;
         pn = pn->sib;
         (*func)(p,level);              // Process this sibling before advancing
         fDescending = 1;
      }
      else if ( level > 0 )
      {
         p  = pn;
         pn = pn->par;
         (*func)(p,level);              // Process last child before ascending
         level--;
         fDescending = 0;
      }
      else
      {
         break;
      }
   }

   (*func)(pn,level);                   // Process the root (Allow special processing of root)

   MVMsg( gc_mask_Show_Locks,
          ("<<treeKiller(%p,%p)\n",func,pn));
}

/******************************/
void xtreeOut()
{
   thread_t  * tp;
   Node      * np;
   UINT64      csum  = 0;
   UINT64      csum2 = 0;
   int         i;
   int         n;
   hash_iter_t iter;

   if ( ge.reset                        // Only output trees when active
        || 0 == gv->trees )             // Only output trees if they exist
   {
      return;
   }

   if ( gv->treeout == 0 )              // Prevent recursion
   {
      gv->treeout = 1;

      if ( gv->scs_active )
      {
         gv->lockTree = 1;              // Lock needed during flush while profiling active
      }

      ge.seqno++;
      OptVMsgLog("\n Begin TREE %d\n\n",ge.seqno);

      gd.tree = file_open( "rt" , ge.seqno );

      DefaultRTFileHeader();

      if ( 0 != ( gc.mask & gc_mask_EnEx_Details ) )
      {
         WriteFlush(gc.msg,  "\n  LV  enen  exen  enex  exex calls ADJUST  ABASE NAME\n\n");
      }

      // output thread trees
      gv->overcal = 0;

      ge.flushing = 1;

      hashIter( &ge.htThreads, &iter ); // Iterate thru valid and invalid threads

      while ( ( tp = (thread_t *)hashNext( &iter ) ) )
      {
         if ( 0 == gv->selListThreads
              || tp->flags & thread_selected )
         {
            np = tp->currT;

            if ( np )
            {
               treeWalker(&xtreeLine,np);
            }
         }
      }

      tp = ge.listDeadThreads;          // Iterate thru dead threads

      while ( tp )
      {
         if ( 0 == gv->selListThreads
              || tp->flags & thread_selected )
         {
            np = tp->currT;

            if ( np )
            {
               if ( np->chd )
               {
                  n = 1;
               }
               else
               {
                  n = 0;

                  for ( i = 0; i < gv->physmets; i++ )
                  {
                     if ( np->bmm[i] )
                     {
                        n++;
                     }
                  }
               }

               if ( n )
               {
                  treeWalker(&xtreeLine,np);
               }
            }
         }
         tp = tp->nextInBucket;
      }

      ge.flushing = 0;

      if ( ge.flushDelayed )
      {
         flushDelayedCleanup();

         ge.flushDelayed = 0;
      }

      fclose(gd.tree);
      WriteFlush(gc.msg,"\n End Tree %d\n\n", ge.seqno);
      WriteFlush(gc.msg," OverCal        %8d\n", gv->overcal);
      WriteFlush(gc.msg," Tree Nodes     %8d\n", gv->tnodes);
      WriteFlush(gc.msg," Node Size      %8d\n", ge.nodeSize);
      WriteFlush(gc.msg," Class Size     %8d\n", gv->sizeClassBlock);
      WriteFlush(gc.msg," Event Size     %8d\n", ge.sizeRTEventBlock);
      WriteFlush(gc.msg," Tree Size      %8d\n", gv->tnodes * ge.nodeSize);

#ifndef _LEXTERN
      if ( gv->stknodes )
      {
         WriteFlush(gc.msg,"\n" );
         WriteFlush(gc.msg,"Number of CallStacks requested from JVMTI:        %"_P64"d\n", ge.numSnGetCallStack );
         WriteFlush(gc.msg,"Number of Tree Nodes retrieved from StkNodes:     %"_P64"d\n", ge.numSnGetStkNode );
         WriteFlush(gc.msg,"Number of CallStacks pushed to validate StkNodes: %"_P64"d\n", ge.numSnPushStkNode );
         WriteFlush(gc.msg,"Number of CallStacks pushed to locate Tree Nodes: %"_P64"d\n", ge.numSnPushCallStack );
         WriteFlush(gc.msg,"Number of Tree Nodes located using StkNodes:      %"_P64"d\n", ge.numSnSelectNode );
      }
#endif

      if ( gv->CallStackCountsByFrames )
      {
         UINT64 totalCycles = 0;
         int    i;

         WriteFlush(gc.msg,"\nCallStack Statistics by Frames\n\nFrames CallStacks      TotalCycles PerCallStack PerFrame\n\n" );

         for ( i = 0; i <= MAX_CALL_STACK_COUNT; i++ )
         {
            if ( gv->CallStackCountsByFrames[i] )
            {
               WriteFlush(gc.msg,"%6d %10d %16"_P64"d %12d %8d\n",
                          i,
                          gv->CallStackCountsByFrames[i],
                          gv->CallStackCyclesByFrames[i],
                          (UINT32)( gv->CallStackCyclesByFrames[i]
                                    / (UINT64)gv->CallStackCountsByFrames[i] ),
                          i ? (UINT32)( gv->CallStackCyclesByFrames[i]
                                        / (UINT64)( i * gv->CallStackCountsByFrames[i] ) )
                          : 0 );

               totalCycles += gv->CallStackCyclesByFrames[i];
            }
         }
         WriteFlush(gc.msg,"\nTotal time gathering CallStacks = %"_P64"d cycles ( %.2f sec )\n",
                    totalCycles,
                    (double)( (INT64)totalCycles / (double)ge.cpuSpeed ) );
      }

      msg_out();

      gv->lockTree = 0;
      gv->treeout  = 0;
   }
}

#ifndef _LEXTERN

///============================================================================
/// Support routines for Lists of Unresolved Addresses (LUAddrs)
///
/// LUAddrs structures are used to record non-Java addresses called by Java
/// methods.  This consists of a ring3 user address and possibly a ring0
/// kernel address.  We will never encounter a ring0 address without a ring3
/// address, since the transition to the kernel will always be made by
/// non-Java code.
///
/// Each LUAddrs structure can contain up to 32 ring3 addresses or up to 31
/// ring0 addresses with a single ring3 address, depending on the flags.  If
/// still more addresses need to be recorded, LUAddrs structures can be
/// chained.  This method was chosen to reduce the number of memory
/// allocations.
///============================================================================

   #define MAX_LUA_ENTRIES 32

typedef struct _LUAddrs LUAddrs;

struct _LUAddrs
{
   LUAddrs * next;

   UINT32    flags;                     // 32 flags            ( 0 = User, 1 = Kernel )
   UINT32    index;                     // Next index in array ( 0 - 32 ) (size guarantess alignment)

   char    * addr[ MAX_LUA_ENTRIES ];   // Array of code pointers, both Kernel and User
};

// Update the node containing the List of Unresolved Addresses

void updateLUAddrs( thread_t * tp,
                    UINT64     sample_r3_eip,
                    UINT64     sample_r0_eip )
{
   UINT64     cur_r3_eip;
   LUAddrs  * pLUA;
   LUAddrs  * pULnew;
   Node     * np = tp->currm->chd;
   FrameInfo  fi;
   int        flags    = 0;
   int        lockTree = gv->lockTree;

   if ( 0 == sample_r3_eip )
   {
      ErrVMsgLog( "R3 Address is 0\n" );
      return;                           // FIX ME
   }

   scs_debug_msg(( "updateLUAddrs: R3 = %p, R0 = %p\n", sample_r3_eip, sample_r0_eip ));

   while ( np )                         // Find the LUAddrs node, if it already exists
   {
      if ( gv_type_LUAddrs == np->ntype )
      {
         break;
      }
      np = np->sib;
   }

   if ( 0 == np )                       // Create a new LUAddrs node, if it doesn't exist
   {
      Node * npNew;

      fi.ptr     = 0;
      fi.type    = gv_type_LUAddrs;
      fi.linenum = 0;

      npNew = constructNode( &fi );

      npNew->par = tp->currm;

      if ( lockTree )                   // Contention is possible when flushing while profiling
      {
         enter_lock( TreeLock, TREELOCK );

         while ( np )                   // Find the LUAddrs node, if it already exists
         {
            if ( gv_type_LUAddrs == np->ntype )
            {
               leave_lock( TreeLock, TREELOCK );

               xFree( npNew, ge.nodeSize );

               break;
            }
            np = np->sib;
         }

         if ( 0 == np )
         {
            np             = npNew;
            np->sib        = tp->currm->chd;
            tp->currm->chd = np;        // Add the LUAddrs node as the first child of the current node

            leave_lock( TreeLock, TREELOCK );
         }
      }
      else
      {
         np             = npNew;
         np->sib        = tp->currm->chd;
         tp->currm->chd = np;           // Add the LUAddrs node as the first child of the current node
      }
   }

   if ( lockTree )
   {
      enter_lock( TreeLock, TREELOCK );
   }

   tp->currm = np;                      // Make the LUAddrs node the current node

   pLUA = (LUAddrs *)np->id;

   if ( 0 == pLUA                       // No existing entry
        || pLUA->index >= MAX_LUA_ENTRIES   // All addrs in use
        || ( pLUA->index == ( MAX_LUA_ENTRIES - 1 )   // Not enough room for 2 addrs
             && sample_r0_eip
             && ( sample_r3_eip != PtrToUint64( pLUA->addr[ MAX_LUA_ENTRIES - 1 ] ) ) ) )
   {
      pULnew       = (LUAddrs *)zMalloc( "LUAddrs", sizeof( LUAddrs ) );

      pULnew->next = pLUA;

      pLUA         = pULnew;

      np->id       = pLUA;              // Add to the head of the list
   }

   cur_r3_eip = PtrToUint64( pLUA->addr[ MAX_LUA_ENTRIES - 1 ] );   // cur_r3_eip does not carry from previous entry

   if ( sample_r0_eip )
   {
      if ( cur_r3_eip != sample_r3_eip )
      {
         pLUA->addr[ pLUA->index++ ] = Uint64ToPtr( sample_r3_eip );

         cur_r3_eip = sample_r3_eip;
      }
      pLUA->flags |= ( 1 << pLUA->index );

      pLUA->addr[ pLUA->index++ ] = Uint64ToPtr( sample_r0_eip );
   }
   else
   {
      pLUA->addr[ pLUA->index++ ] = Uint64ToPtr( sample_r3_eip );

      cur_r3_eip = sample_r3_eip;
   }
   np->bmm[0]++;                        // Record one sample in this list

   if ( pLUA->index < MAX_LUA_ENTRIES )
   {
      pLUA->addr[ MAX_LUA_ENTRIES - 1 ] = Uint64ToPtr( cur_r3_eip );   // cur_r3_eip stored in last entry
   }

   if ( lockTree )
   {
      leave_lock( TreeLock, TREELOCK );
   }
}

//-----------------------------------------------------------------------------
// Resolve any Lists of Unresolved Addresses
//-----------------------------------------------------------------------------

void resolveLUAddrs( Node * parent )
{
   LUAddrs * pLUA;
   LUAddrs * pLUAold;
   UINT32    flags;
   UINT32    i;
   char    * cur_r3_eip;
   Node    * np;
   Node    * npR3;
   Node    * npR0;

   if ( gv->lockTree )
   {
      return;                           // Do not perform during "hot" flush
   }

   np  = parent->chd;

   while ( np )                         // Find the LUAddrs node
   {
      if ( gv_type_LUAddrs == np->ntype )
      {
         break;
      }
      np = np->sib;
   }

   if ( 0 == np
        || 0 == np->id )
   {
      return;                           // No addresses to resolve
   }

//-----------------------------------------------------------------------------

   if ( gv->lockTree )
   {
      enter_lock( TreeLock, TREELOCK );

      pLUA    = (LUAddrs *)np->id;
      np->id  = 0;                      // Detach the list from the LUAddrs node

      leave_lock( TreeLock, TREELOCK );
   }
   else
   {
      pLUA    = (LUAddrs *)np->id;
      np->id  = 0;                      // Detach the list from the LUAddrs node
   }
   np->bmm[0] = 0;                      // Reset count in node

   while ( pLUA )
   {
      flags      = pLUA->flags;
      cur_r3_eip = 0;
      npR3       = 0;
      npR0       = 0;

      for ( i = 0; i < pLUA->index; i++ )
      {
         if ( flags & 1 )               // Kernel address
         {
            npR0       = pushUAddr( npR3, pLUA->addr[i], 1 );
            npR0->bmm[0]++;             // Update count in Kernel node
         }
         else                           // User address
         {
            cur_r3_eip = pLUA->addr[i]; // Update current user address

            npR3       = pushUAddr( parent, cur_r3_eip, 0 );

            if ( 0 == ( flags & 2 ) )   // Next address not a kernel address
            {
               npR3->bmm[0]++;          // Update count in User node
            }
         }
         flags >>= 1;
      }
      pLUAold = pLUA;

      pLUA    = pLUA->next;             // Get next list

      xFree( pLUAold, sizeof( LUAddrs ) );
   }
}

// Resolve user/kernel addresses and push them after the specified node
//     Search nearby nodes to avoid call to A2N, if possible

Node * pushUAddr( Node * np, char * addr, int kernel )
{
   FrameInfo    fi;
   char         buffer[MAX_PATH];
   int          i;
   int          rc;
   method_t   * mp;

   char       * sym_addr   = 0;
   unsigned int sym_length = 0;

   SYMDATA      sd;

   Node       * npMatch = checkNearbyNodes2( np, addr );

   if ( npMatch )
   {
      mp = (method_t *)npMatch->id;     // This method already exists
   }
   else
   {
      if ( ge.cant_use_a2n || 0 == ge.scs_a2n )
      {
         sprintf( buffer, "unresolved_0x%p", addr );
      }
      else
      {
         rc = ge.pfnA2nGetSymbol( ge.pid, PtrToUint64(addr), &sd );

         if ( rc > A2N_NO_MODULE )
         {
            msg_log("**ERROR** A2nGetSymbol() rc=%d for 0x%"_L64X"\n", rc, addr);
            sprintf( buffer, "a2n_error_0x%p", addr );
         }
         else
         {
            // Now we have a native symbol name (which can be a java method) that is
            // different from the leaf node method name. Push it under the leaf node.
            // Symbol name we make up looks like: module(symbol)

            sym_addr   = Uint64ToPtr( sd.sym_addr );
            sym_length = sd.sym_length;

            if (sd.flags & SDFLAG_SYMBOL_JITTED)
            {
               strcpy(buffer, sd.sym_name);
            }
            else
            {
               strcpy(buffer, sd.mod_name);

   #if defined(_WINDOWS)

               i = 0;

               while ( buffer[i] )
               {
                  buffer[i] = (char)tolower( buffer[i] );   // Windows modules are case-insensitive
                  i++;
               }

   #endif

               strcat(buffer, "(");
               strcat(buffer, sd.sym_name);

               if ( rc != A2N_SUCCESS)
               {
                  sprintf( buffer + strlen(buffer), "_0x%p", addr);
               }
               strcat(buffer, ")");
            }
         }
      }

      mp = insert_method( 0, buffer );

      if ( 0 == mp->code_address )
      {
         mp->code_address = sym_addr;
         mp->code_length  = sym_length;
      }
   }

   // Add NativeSymbol node (under the leaf node)
   fi.ptr     = mp;
   fi.type    = kernel ? gv_type_scs_Kernel : gv_type_scs_User;
   fi.linenum = 0;

   np = push( np, &fi );

   return( np );
}

//-----------------------------------------------------------------------------
// Resolve Raw Address child nodes
//-----------------------------------------------------------------------------

void resolveRawAddrs( Node * parent )
{
   Node * np;
   Node * prev;
   Node * leaf;
   Node * children;

   if ( gv->lockTree )                  // Remove comments below if this is not true
   {
      return;                           // Do not perform during "hot" flush
   }

   for (;;)                             // Repeat until there are no more RawAddr nodes
   {
      prev = (Node *)&parent->chd;      // Use child pointer as virtual node, since sib is first

      while ( 0 != ( np = prev->sib )   // Find any RawAddr nodes
              && gv_type_RawUserAddr != np->ntype
              && gv_type_RawKernelAddr != np->ntype )
      {
         prev = np;
      }

      if ( 0 == np )
      {
         return;                        // No remaining RawAddr nodes
      }

//-----------------------------------------------------------------------------

//    if ( gv->lockTree )
//    {
//       enter_lock( TreeLock, TREELOCK );
//
//       prev = (Node *)&parent->chd;   // Use child pointer as virtual node, since sib is first
//
//       while ( 0 != ( np = prev->sib )   // Find any RawAddr nodes
//               && gv_type_RawUserAddr != np->ntype
//               && gv_type_RawKernelAddr != np->ntype )
//       {
//          prev = np;
//       }
//
//       if ( 0 == np )
//       {
//          leave_lock( TreeLock, TREELOCK );
//
//          return;                     // No remaining RawAddr nodes
//       }
//
//       prev->sib = np->sib;           // Detach np subtree from the children of parent
//       children  = np->chd;
//       np->chd   = 0;                 // Detach children from np
//
//       leave_lock( TreeLock, TREELOCK );
//    }
//    else
//    {
      prev->sib = np->sib;              // Detach np subtree from the children of parent
      children  = np->chd;
      np->chd   = 0;                    // Detach children from np
//    }

//-----------------------------------------------------------------------------

      leaf = pushUAddr( parent,
                        np->id,
                        ( gv_type_RawKernelAddr == np->ntype ) ? 1 : 0 );

      leaf->bmm[0] += np->bmm[0];       // Merge np count into leaf node (not updated hot)

      xFree( np, ge.nodeSize );         // FIX ME: Thread safe? sleep(0) needed?

      prev = (Node *)&leaf->chd;        // Use child inside leaf node as a virtual node, since sib is first

//    if ( gv->lockTree )               // Contention is possible when flushing while profiling
//    {
//       enter_lock( TreeLock, TREELOCK );
//
//       while ( 0 != ( np = prev->sib ) )
//       {
//          prev = np;
//       }
//       prev->sib = children;          // Add children to leaf node
//
//       enter_lock( TreeLock, TREELOCK );
//    }
//    else
//    {
      while ( 0 != ( np = prev->sib ) ) // This is initially the first child, which is also the first sibling
      {
         prev = np;
      }
      prev->sib = children;             // Add children of RawNode as the last children of this leaf node
//    }
   }
}


//
// get_c_symbol_name_with_backtrace_symbols()
// ******************************************
//
// This is a last-ditch attempt to resolve symbols for stripped executables.
// backtrace_symbols() uses the .dynamic section to get symbols so
// it may give us a symbol, which is better than no symbols.
//
// Strings coming back from backtrace_symbols() look something like this:
// 1) "/lib/libc.so.6 [0xb7890600]"               - return: [0xb7890600]-@-/lib/libc.so.6
// 2) "/lib/libc.so.6() [0xb7890600]"             - return: [0xb7890600]-@-/lib/libc.so.6
// 3) "/lib/libc.so.6(+0xdcf39) [0xf770ef39]      - return: [0xf770ef39]-@-/lib/libc.so.6
// 4) "/lib/libc.so.6(usleep+0x3d) [0xb77f6fdd]"  - return: !!usleep-@-/lib/libc.so.6
// 5) "[0xffffe424]"                              - return: [0xffffe424]
//
static void get_c_symbol_name_with_backtrace_symbols(void * addr, char * buffer)
{
#if defined(_LINUX)
   char * * symbols;
   char * symbol;
   char * s;
   char * p;

   symbols = backtrace_symbols(&addr, 1);
   if (symbols == NULL) {
      sprintf(buffer, "[%p]", addr);
      return;
   }

   symbol = symbols[0];
   if (*symbol == '[') {
      // 5) "[0xffffe424]"  - return: [0xffffe424]
      sprintf(buffer, "[%p]", addr);
      return;
   }

   s = strchr(symbol, '(');
   if (s) {
      *s = '\0';
      s++;
      if (*s == ')' || *s == '+') {
         // 2) "/lib/libc.so.6() [0xb7890600]"  - return: [0xb7890600]-@-/lib/libc.so.6
         // 3) "/lib/libc.so.6(+0xdcf39) [0xf770ef39]      - return: [0xf770ef39]-@-/lib/libc.so.6
         sprintf(buffer, "[%p]-@-%s", addr, symbol);
      }
      else {
         // 4) "/lib/libc.so.6(usleep+0x3d) [0xb77f6fdd]"  - return: !!usleep-@-/lib/libc.so.6
         p = strchr(s, '+');
         *p = '\0';
         sprintf(buffer, "!!%s-@-%s", s, symbol);
      }
   }
   else {
      // 1) "/lib/libc.so.6 [0xb7890600]"  - return: [0xb7890600]-@-/lib/libc.so.6
      p = strrchr(symbol, ' ');
      *p = '\0';
      sprintf(buffer, "[%p]-@-%s", addr, symbol);
   }
#endif
   return;
}


//
// get_c_symbol_name()
// *******************
//
static void get_c_symbol_name(Node * np, char * buffer)
{
#if defined(_LINUX)
	SYMDATA sd;
	uint64_t addr;
	int rc;


	addr = PtrToUint64(np->id);
	if (ge.cant_use_a2n || !ge.scs_a2n) {
      // Give it one more shot with backtrace_symbols()
      get_c_symbol_name_with_backtrace_symbols(Uint64ToPtr(addr), buffer);
      return;
	}

	if (ge.pfnA2nGetSymbolEx)
		rc = ge.pfnA2nGetSymbolEx(addr, &sd);
	else
		rc = ge.pfnA2nGetSymbol(ge.pid, addr, &sd);

	if (rc == A2N_SUCCESS) {
		// Have everything
		sprintf(buffer, "%s-@-%s", sd.sym_name, sd.mod_name);
      return;
	}

   if (rc == A2N_NO_SYMBOLS) {
      // No symbols. Give it one more shot with backtrace_symbols()
      get_c_symbol_name_with_backtrace_symbols(Uint64ToPtr(addr), buffer);
   }
   else if (rc == A2N_NO_SYMBOL_FOUND || rc == A2N_NO_MODULE) {
      // A2N_NO_SYMBOL_FOUND or A2N_NO_MODULE
      sprintf(buffer, "%s[%p]-@-%s", sd.sym_name, Uint64ToPtr(addr), sd.mod_name);
   }
   else {
      // Error
      msg_log("**ERROR** A2nGetSymbol() rc=%d for 0x%"_L64X"\n", rc, addr);
      sprintf(buffer, "A2N_ERROR [%p]", Uint64ToPtr(addr));
   }
#endif
	return;
}


///============================================================================
/// Callstack Drift Checking:
///
/// The current node may be changed if the thread has drifted to a nearby node
/// before the callstack could be acquired.  To enable this, every method
/// block includes the address and length of the method.
///
/// If the reported address falls within the bounds of the current node, this
/// routine simply returns the current node.
///
/// If not, we check to see if the address falls withing the bounds of the
/// calling method, the one associated with the parent node.  If so, we assume
/// that the code drifted from the parent to the current node and make the
/// parent node the new current node.
///
/// If not, we check to see if the address falls withing the bounds of one of
/// the methods called by the current method.  If so, we assume that the code
/// drifted from the child back to the current node and make the child node
/// the new current node.
///
/// If none of these cases are true, we return a zero to indicate that special
/// processing is required.
///
/// When checking non-Java addresses, the cost of resolving the address to a
/// symbol is much higher and we look for even more distant nodes before
/// calling A2N.  After calling checkNearbyNodes as described above, the
/// checkNearbyNodes2 routine checks grandparents, siblings, and grandchildren
/// of the current node, those which are two nodes away in the call tree.
///============================================================================

Node * checkNearbyNodes( Node * curNode, char * addr )   // Check nodes at distance 1
{
   Node     * np;
   method_t * mp;

   if ( 0 == curNode )
   {
      ErrVMsgLog( "checkNearbyNodes( 0, %p )\n", addr );
      return( 0 );
   }

   if ( curNode->ntype != gv_type_Thread )   // Current node is not the thread node
   {
      mp = (method_t *) curNode->id;

      if ( mp
           && mp->code_address          // Address must be present
           && addr >= mp->code_address  // Still executing self, no update necessary
           && addr < ( mp->code_address + mp->code_length ) )
      {
         return( curNode );
      }

      np = curNode->par;

      if ( np->ntype != gv_type_Thread )   // Parent is not the thread node
      {
         mp = (method_t *) np->id;

         if ( mp
              && mp->code_address
              && addr >= mp->code_address   // Were executing parent
              && addr < ( mp->code_address + mp->code_length ) )
         {
            return( np );               // Match is parent
         }
      }
   }

   np = curNode->chd;                   // First child

   while ( np )
   {
      if ( np->ntype <= gv_type_maxMethod )
      {
         mp = (method_t *) np->id;

         if ( mp
              && mp->code_address
              && addr >= mp->code_address   // Were executing this child
              && addr < ( mp->code_address + mp->code_length ) )
         {
            return( np );               // Match is child
         }
      }
      np = np->sib;
   }

   return( 0 );                         // No matching node was found
}

Node * checkNearbyNodes2( Node * curNode, char * addr )   // Check nodes at distance 2
{
   method_t * mp;
   Node     * np;
   Node     * child;

   if ( 0 == curNode )
   {
      ErrVMsgLog( "checkNearbyNodes2( 0, %p )\n", addr );
      return( 0 );
   }

   np = checkNearbyNodes( curNode, addr );

   if ( np )
   {
      return( np );
   }

   if ( curNode->ntype != gv_type_Thread )   // Current node is not the thread node
   {
      np = curNode->par;

      if ( np->ntype != gv_type_Thread )   // Parent is not the thread node
      {
         np = np->par;

         if ( np->ntype != gv_type_Thread )   // Grandparent is not the thread node
         {
            mp = (method_t *) np->id;

            if ( mp
                 && mp->code_address
                 && addr >= mp->code_address   // Were executing grandparent
                 && addr < ( mp->code_address + mp->code_length ) )
            {
               return( np );            // Match is grandparent
            }
         }
      }

      np = curNode->par->chd;           // First sibling (possibly self)

      while ( np )
      {
         if ( np != curNode
              && np->ntype <= gv_type_maxMethod )
         {
            mp = (method_t *) np->id;

            if ( mp
                 && mp->code_address
                 && addr >= mp->code_address   // Were executing this sibling
                 && addr < ( mp->code_address + mp->code_length ) )
            {
               return( np );            // Match is sibling
            }
         }
         np = np->sib;
      }
   }

   child = curNode->chd;                // First child

   while ( child )
   {
      np = child->chd;                  // Grandchild

      while ( np )
      {
         if ( np->ntype <= gv_type_maxMethod )
         {
            mp = (method_t *) np->id;

            if ( mp
                 && mp->code_address
                 && addr >= mp->code_address   // Were executing this grandchild
                 && addr < ( mp->code_address + mp->code_length ) )
            {
               return( np );            // Match is grandchild
            }
         }
         np = np->sib;
      }
      child = child->sib;
   }

   return( 0 );                         // No matching node was found
}
#endif
