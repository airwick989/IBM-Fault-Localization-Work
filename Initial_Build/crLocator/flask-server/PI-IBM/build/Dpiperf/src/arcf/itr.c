/*
 *
 *  Copyright (c) IBM Corporation 1996.
 *  Property of IBM Corporation.
 *  All Rights Reserved.
 *
 *  Origin 30 (IBM)
 *
 */

#include "global.h"
#include "arc.h"
#include "string.h"

int    onStack(struct HKDB * hp);
void   pop_ring0s(void);

STRING * tokarr;

/* ************************************************************ */
void pushpop(struct HKDB * hp, UNIT del, char c, int offset,
             int loc, int ring)
{
   /* loc = CALL(1) RETURN(2) */
   /*
    *  enforce ring constraints
    */
   int ex = 0, rc = 0;

   dbgmsg(">> pushpop(hp=%p, del=%d, c=%c, offset=%d, loc=%d, ring=%d\n", hp, (int)del, (char)c, offset, loc, ring);

   if (c == '?')
   {                                    /* independent of ring */
      if ( (ring == 0) && (gv.ksum == 1) )
      {
         arc_out(4, hp);
         gv.dtime += del;
         acct2();
      }
      else
      {
         arc_out(0, hp);
         gv.dtime += del;
         acct2();
         arc_out(1, hp);
      }
      return;
   }


   /*  if ring3 pop ring 0's */
   if (ring == 1)
   {
      pop_ring0s();
   }

   rc = onStack(hp);
   /* 1(top) >=2(onStack, not Top) 0(null) -1(stk=1) ... */
   /* NotOn
    *   Stack  rc  Action
    *      0   0    >
    *      1   -1   !Return : < >
    *      1   -1   Return  : splice between pid & top)
    *      2+   2+  >
    */

   /* TOP of stack */
   if (rc == 1)
   {
      arc_out(4, hp);
   }

   /* somewhere onStack( ! TOP ) */
   else if (rc >= 2)
   {
      if (loc == 2)
      {                                 /* RETURN */
         arc_out(4, hp);                /* return via @ */
      }
      else if ( (loc == 1) || (offset == 0) )
      {
         arc_out(0, hp);                /* CALL (recursion) */
      }
      else
      {
         arc_out(4, hp);                /* return via @ */
      }
   }

   /* Not On Stack Cases */
   else if ( (loc == 1) || (offset == 0) )
   {                                    /* CALL | OFF */
      arc_out(0, hp);
   }

   /* StackDepth = 0 */
   else if (rc == 0)
   {
      arc_out(0, hp);                   /* CALL */
   }

   /* StackDepth = 1 */
   else if (rc == -1)
   {
      if (loc == 2)
      {                                 /* return */
         /* replace w splice */
         /* create arc API for splice */
         gv.ptp->ctp = gv.ptp->rtp;     /* pop to pidtid */
         arc_out(0, hp);                /* CALL */
      }
      else
      {
         gv.ptp->ctp = gv.ptp->rtp;     /* pop to pidtid */
         arc_out(0, hp);                /* CALL */
      }
   }

   /* StackDepth >= 2 */
   else
   {                                    /* !Call !(offset == 0) */
      arc_out(0, hp);                   /* CALL */
   }

   /* time */
   gv.dtime += del;
   acct2();

   dbgmsg("<< pushpop()\n");
   return;
}

/* ************************************************************ */
void flush_temp_cpu(int cpu)
{
   FILE * fin;
   char buf[4096];
   char   fn[32];

   fflush(gv.ftempcpu[cpu]);
   fclose(gv.ftempcpu[cpu]);

   sprintf(fn, "xflow_temp_%d", cpu);
   fin = fopen(fn, "rb");
   if (fin == NULL) {
      printf(" itrace.c : can't open %s\n", fn);
      pexit("can't open temp cpu buffer file");
   }

   if (gv.allrecs)
      fprintf(xflow, " !!! flushing buffer for CPU%d\n", cpu);

   while (fgets(buf, 2048, fin) != NULL) {
      fprintf(xflow, "%s", buf);
   }

   fclose(fin);
   remove(fn);

   gv.ftempcpu[cpu] = NULL;
   gv.ftemp_cnt[cpu] = 0;
   free(gv.curthr[cpu]);
   gv.curthr[cpu] = NULL;

   return;
}


//
// New support for -f itrace:
// -acpu
//     Append CPU number to records in x-files
//     Default:     493 - > _IO_puts:/lib/libc-2.4.so_0
//     With -acpu:  493 - > _IO_puts:/lib/libc-2.4.so_0_3
//
// -fcpu ###
//     Only process records for the given CPU.
//     Will display, for example, an xflow of all activity on one CPU.
//
// -fthr pid_tid
//     Only process records for the given pid_tid.
//     Will display, for example, an xflow of all activity on one thread.
//
// -gdisp
//     Group xflow by dispatches per CPU.
//     The default is to interleave the xflow with events as they occur
//     in the arc file. With -gdisp all events that occur between thread
//     dispatches in a CPU are grouped together. The flow is no longer a
//     true flow, but more a flow withing a dispatching interval.
//

/* ************************************************************ */
void proc_itrace(STRING fname)
{
   FILE   * cin;
   char   buf[4096];                    /* input data */
   unsigned char c;
   char   tempfn[32];

   STRING mnm;
   char mnmr[2048];                     /* mnm + "_ring" */
   STRING pnm;

   struct HKDB * hp = NULL;
   struct HKDB dummy_hp = {0};

   unsigned int rec = 0;
   int  tcnt;
   int  offset;
   int  rc = 0;
   int  ring;
   int  cpu;
   int  loc = 0;                        /* by thread / by ring */

   size_t filter_pidtid_len = 0;

   UNIT del = 0.0;
// char fmt[100] = " %d %6.0f %c %d %s %s %d %? %d %s\n";

   /***CODE***/

   cin = fopen(fname, "rb");
   if (cin == NULL)
   {
      printf(" itrace.c : can't open %s\n", fname);
      pexit("can't open ARCF itrace input file");
   }

   if (gv.filter_pidtid) {
      filter_pidtid_len = strlen(gv.filter_pidtid);
   }

   if (gv.uflag != 1)
   {                                    /* set units */
      gv.Units = Remember("-ITRACE-INSTRUCTIONS-");
   }
   gv.dtime = 0;                        /* incremental metric */

   while ((fgets(buf, 2048, cin) != NULL) && (gv.hkstp > rec))
   {
      rec++;

      if (gv.allrecs == 1)
      {
         /* print all lines. PrePend ### */
         if (gv.flow)
            fprintf(xflow, " ### %s", buf);
      }

      // if ((rec % 10000) == 0) {
      //    printf(".");
      //    fflush(stdout);
      // }

      /* 0 1 2 3 4 5     6       7    8 */
      /* C R L I @ O M:MOD P_T_PNM CALL */

      tokarr = tokenize(buf, &tcnt);

      if (tokarr[0][0] == '#')
         continue;          // Comment

      if (tcnt >= 7)
      {
         c = tokarr[4][0];
         cpu = gv.curcpu = atoi(tokarr[0]);
         ring = atoi(tokarr[1]);
         pnm  = tokarr[7];

         if (gv.filter_cpu != -1 && cpu != gv.filter_cpu) {
            continue;           // cpu filtering and no match
         }

         if (gv.filter_pidtid != NULL && strncasecmp(gv.filter_pidtid, pnm, filter_pidtid_len) != 0) {
            continue;           // tid filtering and no match
         }
      }
      else
      {
         c = 'x';
         continue;
      }


      /* convert prev loc, on PT, to a CALL */
      if (c == '>')
      {
         // cpu = atoi(tokarr[0]);
         // ring = atoi(tokarr[1]);
         // pnm  = tokarr[7];

         /* get on pit/tid */
         if (strcmp(gv.apt, pnm) != 0)
         {
            gv.ptp = ptptr(pnm);
            gv.apt = gv.ptp->apt;
            dummy_hp.cpu = cpu;
            dummy_hp.ring = ring;
            if (gv.allrecs && gv.flow) {
               fprintf(xflow, " !a! arc_out(3, &dummy_hp)\n");
            }
            //arc_out(3, hp);
            arc_out(3, &dummy_hp);
         }


         /* update last oc */
         if (ring == 1)
         {
            if (gv.ptp->loc3 != 1)
            {
               gv.ptp->loc3 = 1;
               OptVMsg(" Changing loc3 to 1 on %s\n", gv.apt);
            }
         }
         else
         {
            if (gv.ptp->loc0 != 1)
            {
               gv.ptp->loc0 = 1;
               OptVMsg(" Changing loc3 to 1 on %s\n", gv.apt);
            }
         }

         continue;
      }

      if ( (c == '>') || (c == '<') || (c == '?') || (c == '@') )
      {
         // cpu = atoi(tokarr[0]);
         // ring = atoi(tokarr[1]);
         del  = (UNIT)atof(tokarr[3]);
         if (strcmp(tokarr[5], "0") == 0)
            offset = 0;
         else
            offset = 1;
         mnm  = tokarr[6];
         // pnm  = tokarr[7];

         if (gv.flow && gv.group_disp) {
            if (c == '?' && strcmp(mnm, "callflow:disp") == 0) {
               // Dispatch on this processor

               // Flush current buffer if anything in it
               if (gv.ftemp_cnt[cpu] != 0) {
                  flush_temp_cpu(cpu);
               }

               // Start buffering
               if (gv.ftempcpu[cpu] == NULL) {
                  sprintf(tempfn, "xflow_temp_%d", cpu);
                  gv.ftempcpu[cpu] = fopen(tempfn, "wb");
                  if (gv.ftempcpu[cpu] == NULL) {
                     pexit("** Unable to open temp file for per-processor buffer **");
                  }
                  if (gv.allrecs) {
                     fprintf(xflow, " !!! begin buffering for %s on CPU%d\n", pnm, cpu);
                  }
                  gv.ftemp_cnt[cpu] = 0;
                  gv.curthr[cpu] = strdup(pnm);
               }
            }
         }

         /* pidtid switch : UP - seldom, SMP - often */
         if (strcmp(gv.apt, pnm) != 0)
         {
            if (gv.allrecs && gv.flow) {
               fprintf(xflow, " !!! gv.apt=%s   pnm=%s\n", gv.apt, pnm);
            }
            gv.ptp = ptptr(pnm);        /* find/add */
            gv.apt = gv.ptp->apt;
//          if (hp) {
//             arc_out(3, hp);
//          }
//          else {
               dummy_hp.cpu = cpu;
               dummy_hp.ring = ring;
               arc_out(3, &dummy_hp);
//          }
         }

         /* add ring to mnm */
         if ( (gv.ksum == 1) && (ring == 0) )
         {
            hp = hkdb_find("Kernel", 1);   /* curr method */
         }
         else
         {
            if (0 == 0)
            {
               /* strcpy(mnmr, mnm); */
               if (gv.append_cpu)
                  sprintf(mnmr, "%s_%d_%d", mnm, ring, cpu);
               else
                  sprintf(mnmr, "%s_%d", mnm, ring);
               hp = hkdb_find(mnmr, 1); /* curr method */
            }
            else
            {
               hp = hkdb_find(mnm, 1);  /* curr method */
            }
         }

         if (hp) {
            hp->ring = ring;
            hp->cpu = cpu;
         }

         /* using loc from last bb */
         /* loc : last bb in same ring */
         if (ring == 1) loc = gv.ptp->loc3;
         else          loc = gv.ptp->loc0;

         pushpop(hp, del, c, offset, loc, ring);

         /* setting oc per thread for next rec to use! */
         /* needs pid/tid & ring */
         loc  = atoi(tokarr[2]);        /* 0-6 : ? C R J IR ? */
         if (loc != 0)
         {
            if (ring == 1) gv.ptp->loc3 = loc;
            else          gv.ptp->loc0 = loc;
         }
      }
   }                                    /* while neof */

   printf("\n");

   // Flush any temp cpu buffers
   if (gv.flow && gv.group_disp) {
      // Flush current cpu first
      if (gv.curcpu >= 0) {
         if (gv.ftemp_cnt[gv.curcpu] != 0)
            flush_temp_cpu(gv.curcpu);
      }

      for (cpu = 0; cpu < ARCF_MAX_CPUS; cpu++) {
         if (cpu == gv.curcpu)
            continue;             // Already done

         if (gv.ftemp_cnt[cpu] != 0)
            flush_temp_cpu(cpu);
      }
   }

   fflush(stdout);
   return;
}                                       /* end proc_itrace */

