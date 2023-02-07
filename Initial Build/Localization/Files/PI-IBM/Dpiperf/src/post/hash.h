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

#ifndef __HASH_H
   #define __HASH_H

   #include "encore.h"

// Threads *************************
typedef struct _thread_t thread_t;
struct _thread_t
{
   bucket_t   * nextInBucket;
   void       * tid;                    // thread ID (key)
   unsigned int pid;                    /* process ID */
   int          valid;                  /* valid thread */
   char *       thread_name;            /* thread name */
   thread_t *   next;                   // linked list of all threads

   uint64     to;                       // last too
   uint64   from;                       // last from
   int        oc;                       // last opcode type (1=call,2=ret,3=jump,4=iret,5=other)
   int     cfcnt;                       // call flow count (by thread)
   int        ci;                       // bufferred insts
   int      ring;                       //
   int      cpu;                        //
   int    offset;                       //

   // omnm omodnm mnm & modnm all contained in sd
   char *   omnm;                       // a2n space
   char * omodnm;                       // a2n space

   // below arrays probably can be removed when converting
   // all itrace to new " -t itrace " arcf format.
   char mnm[2048];                      // method name
   char modnm[2048];                    // module name

   pNode currT;
   pNode currm;

   SYMDATA sd;                          // A2N structure
   void *  sd_addr;                     // addr used in getSymbol
   int     sd_rc;                       // rc from getSymbol
   UINT64  tmend;                       // time of end of thread
};

thread_t *  lookup_thread  (unsigned int tid, unsigned int pid);
thread_t *  insert_thread  (unsigned int tid, unsigned int pid);
thread_t *  get_thread_head(void);

// Processes *************************
typedef struct _process_t process_t;
struct _process_t
{
   bucket_t * nextInBucket;
   void     * pid;                      // process ID (key)
   void     * jit_ll;                   // Linked list of JITA2N records
   void     * tnm_ll;                   // Linked list of JTNM records
   int      flags;                      // flags
   int      read;                       // read flag
   int      trcid;                      // largest trcid
   int      jitUsed;                    // JITA2N records used
   int      jitSaved;                   // JITA2N records saved
   int      jtnmUsed;                   // JTNM records used 
   int      jtnmSaved;                  // JTNM records saved
};


// Various per-cpu data for the current trace record 

typedef struct _post_cpu_data POST_CPU_DATA;
struct _post_cpu_data
{
   int  pid;                            // pid
   int  tid;                            // tid
   int  stt;                            // dispatch occurred
   int  hk;
   thread_t * tp;                       // pointer to the thread with this tid, pid 		
};

process_t *  lookup_process  (int pid);
process_t *  insert_process  (int pid);

#endif
