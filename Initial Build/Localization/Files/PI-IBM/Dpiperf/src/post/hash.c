/*   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2007
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "post.h"
#include "a2n.h"
#include "hash.h"

void * constNode(void);

/******************************/
static thread_t * thread_head = NULL;
thread_t * get_thread_head()
{
   return thread_head;
}

/******************************/
// need additional pid to lookup_thread

thread_t * lookup_thread( unsigned int tid, unsigned int pid )
{
   thread_t * tp;

   tp = (thread_t *)hashLookup(&gv.thread_table, Uint32ToPtr(tid));

   while ( tp )
   {
      if ( 1 == tp->valid
           && tid == PtrToUint32(tp->tid)
           && pid == tp->pid )
      {
         return( tp );
      }
      tp = (thread_t *)tp->nextInBucket;
   }
   return tp;
}

/******************************/
thread_t * insert_thread( unsigned int tid, unsigned int pid )
{
   thread_t * tp;
   char       buf[64];

   if ( 0 == gv.thread_table.bucks )
   {
      hashInit(&gv.thread_table, 1031, sizeof(thread_t), "thread_table", 0, 0);
   }

   tp = lookup_thread( tid, pid );

   if ( NULL == tp )
   {
      tp = (thread_t *)hashAdd(&gv.thread_table, Uint32ToPtr(tid));   // Sets key as tid

      if ( 0 == tp )
      {
         err_exit( "insert_thread: Out of Memory" );
      }

      tp->pid    = pid;
      tp->valid  = 1;

      tp->currT  = (void *)constNode();
      tp->currm  = tp->currT;

      tp->thread_name = "";             // Use immediate because it is never freed

      // thread name for tree
      sprintf(buf, "%x_%x", pid, tid);
      tp->currm->mod  = xStrdup("module", buf);
      tp->currm->mnm  = xStrdup("method", "thrd");

      tp->sd_rc = 3;                    // invalidate sym cache

      // thread linked list
      tp->next    = thread_head;
      thread_head = tp;

   }
   return( tp );
}

// ************** Process Section ***********************

/******************************/
process_t * lookup_process(int pid)
{
   return( (process_t *)hashLookup(&gv.process_table, Uint32ToPtr(pid)) );
}

/******************************/
process_t * insert_process( int pid )
{
   process_t * pp;

   if ( 0 == gv.process_table.bucks )
   {
      hashInit(&gv.process_table, 1031, sizeof(process_t), "process_table", 0, 0);
   }

   pp = hashLookup(&gv.process_table, Uint32ToPtr(pid));

   if ( NULL == pp )
   {
      gc.allocMsg = "insert_process";

      pp = (process_t *)hashAdd(&gv.process_table, Uint32ToPtr(pid));

      if ( NULL == pp )
      {
         err_exit("table_put in insert_process");
      }
   }
   return( pp );
}
