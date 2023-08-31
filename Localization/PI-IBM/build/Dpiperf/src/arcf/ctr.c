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

int    onStack(struct HKDB * hp);

STRING * tokarr;
int      start = 0;
char     knm[32] = "K:kernel";

/* ************************************************************ */
int rectype(int tcnt)
{
   int rc = 0;

   if (start == 0)
   {
      if (strcmp(tokarr[0], "======") == 0)
      {
         start = 1;
      }
      return(rc);
   }

   if (tcnt >= 6)
   {
      if (atoi(tokarr[0]) >= 1)
      {
         rc = 1;
      }
   }
   if (rc != 1)
   {
      if (strcmp(tokarr[0], "Process") == 0)
      {
         rc = 2;
      }
      else if (strcmp(tokarr[1], "KERNEL") == 0)
      {
         rc = 3;
      }
      else if (strcmp(tokarr[0], "***") == 0)
      {
         rc = 0;
      }
      else if (strcmp(tokarr[0], "---") == 0)
      {
         rc = 0;
      }
      else
      {
         rc = -1;
      }
   }
   return(rc);
}

/* ************************************************************ */
void slast(STRING nm)
{
   int len;

   len = (int)strlen(nm);
   nm[len - 1] = '\0';
}

/* ************************************************************ */
int parsemnm(STRING tok, int tcnt, STRING * mnmp)
{
   /* e.g. ...[0]simple_lock */
   char c;
   char * p;
   char * offp = NULL;
   int  offset = 0;
   int  i;

   p = tok;
   c = *p;

   while (c != '\0')
   {
      if (c == '[')
      {
         p++;
         offp = p;
      }
      if (c == ']')
      {
         *p = '\0';                     /* term offset str */
         offset = atoi(offp);
         p++;
         *mnmp = p;
         break;
      }
      p++;
      c = *p;
   }

   if (tcnt > 6)
   {
      for (i = 6; i < tcnt; i++)
      {
         p = tokarr[i] - 1;
         *p = '_';
      }
   }

   return(offset);
}

/* svc name resolution */
/*
if(svc == 1) {
strcat(mnmb, "::");
strcat(mnmb, gv.ptp->svcnm);
svc = 0;
}
 */

/* ************************************************************ */
void proc_ctrace(STRING fname)
{
   FILE   * cin;
   char   buf[4096];                    /* input data */
   char   pnmbuf[64]  = "";             /* process name buffer */
   char   spnmbufn[64] = "";            /* new */
   char   spnmbufp[64] = "";            /* prev */
   char   mnmb[2048]  = "";
   char   ktynm[128]  = "";
   char   itynm[128]  = "";
   int    newpt = 0;
   STRING mnm;
   STRING mod;
   STRING pid, tid, pnm;
   struct HKDB * hp = NULL;

   unsigned int rec = 0;
   int  tcnt, i;
   int  offset;
   int  ex, ty;
   int  rc = 0;
   int  dotree;
   int  ktype,   itype,   inter,  tot_my, tot_trc;
   int  kinstrs, iinstrs, kstate, istate, kstart, istart, istop;
   int  irec1,   irec2,   irec3;
   int  ocum,    odel,    cum,    del,    intcnt, inti;

   ktype   = itype   = inter  = tot_my = tot_trc = 0;
   kinstrs = iinstrs = kstate = istate = kstart = istart = istop = 0;
   irec1   = irec2   = irec3  = 0;
   ocum    = odel    = cum    = del    = intcnt = inti   = 0;

   /* mod + mnm => mnm */

   cin = fopen(fname, "rb");
   if (cin == NULL)
   {
      printf(" ctrace.c : can't open %s\n", fname);
      pexit("can't open ARCF ctrace input file");
   }

   if (gv.uflag != 1)
   {                                    /* set units */
      gv.Units = Remember("INSTRUCTIONS");
   }
   gv.dtime = 0;                        /* use incremental updates */

   while ((fgets(buf, 2048, cin) != NULL) && ((int)gv.hkstp > rec))
   {
      rec++;

      /* cases
    0 : app mode
    1 : kernel mode 1st instr
         accum kinstrs
    2 : kernel mode later instr
         accum kinstrs
         recognize type
         save pidtid for sw on exit
    3 : interrupt: state_save -> flih -> resume/backtrack
       */

      tokarr = tokenize(buf, &tcnt);
      ty = rectype(tcnt);

      switch (ty)
      {
      case 0:                           /* skip records */
         break;

      case 1:                           /* bb rec */
         dotree = 1;
         cum = atoi(tokarr[1]);
         del = atoi(tokarr[4]);

         mod = tokarr[2];
         offset = parsemnm(tokarr[5], tcnt, &mnm);

         if (kstate == 0)
         {                              /* not in Kernel */
            if (strcmp(mod, "glink.s") == 0)
            {
               dotree = 0;
               strcpy(gv.ptp->svcnm, mnm);   /* svc coloring */
            }
            else if (strcmp(mod, "ptrgl.s") == 0)
            {
               dotree = 0;
            }
         }

         if (kstate == 1)
         {                              /* 1st instr in K */
            kstart = cum - del;
            istate = 0;

            if (strcmp(mnm, "svc_instr") == 0)
            {
               OptMsg(" svc_inst recognition \n");
               ktype = 1;
               ktynm[0] = 0;
               strcat(ktynm, mnm);
               strcat(ktynm, "_");
               strcat(ktynm, gv.ptp->svcnm);
            }
            else
            {
               ktype = 2;
               ktynm[0] = 0;
               strcat(ktynm, mnm);
               strcat(ktynm, "_K");
            }
         }

         if (kstate >= 1)
         {                              /* in the kernel */
            if (strcmp(mnm, "state_save_ctrace") == 0)
            {
               istate = 1;
               itype  = 0;
               istart = cum - del;
               irec1  = rec;
            }
            else if (strcmp(mnm, "dec_flih") == 0)
            {
               if (istate == 1)
               {
                  istate = 2;
                  irec2  = rec;
                  itype = 1;
                  OptMsg(" itype 1 DEC\n");
                  strcpy(itynm, "Decrementer");
               }
            }
            else if (strcmp(mnm, "ex_flih_sys") == 0)
            {
               if (istate == 1)
               {
                  istate = 2;
                  irec2  = rec;
                  itype = 2;
                  OptMsg(" itype 2 EXTER\n");
                  strcpy(itynm, "External");
               }
            }
            else if (strcmp(mnm, "ds_flih") == 0)
            {
               if (istate == 1)
               {
                  istate = 2;
                  irec2  = rec;
                  itype = 3;
                  OptMsg(" itype 2 PF\n");
                  strcpy(itynm, "PageFault");
               }
            }
            else if (strcmp(mnm, "ds_flih64") == 0)
            {
               if (istate == 1)
               {
                  istate = 2;
                  irec2  = rec;
                  itype = 3;
                  OptMsg(" itype 3 PF\n");
                  strcpy(itynm, "PageFault");
               }
            }
            else if (strcmp(mnm, "resume_fixup") == 0)
            {
               if (offset == 0)
               {
                  istop = cum;
                  if (istate == 2)
                  {
                     istate = 3;        /* bingo */
                     irec3 = rec;
                  }
               }
            }
            else if (strcmp(mnm, "back_track") == 0)
            {
               if (offset == 0)
               {
                  istop = cum;
                  if (istate == 2)
                  {
                     istate = 3;        /* bingo */
                     irec3 = rec;
                  }
               }
            }
         }

         /* no tree for glink, ptrgl or kernel mode */
         /* output Interrupt or K: @ <== KERNEL  */

         if ( (dotree == 1) && (kstate == 0) )
         {
            /* full name */
            strcpy(mnmb, mnm);
            strcat(mnmb, ":");
            strcat(mnmb, mod);

            hp = hkdb_find(mnmb, 1);

            /* > < @  */
            if (offset != 0)
            {
               rc = onStack(hp);        /* pop K:kernel => notOnStack */

               if (rc >= 1) ex = 4;     /* onStack => @ */
               else
               {
                  if (rc == -1)
                  {
                     gv.ptp->ctp = gv.ptp->rtp;   /* return to pidtid */
                  }
                  ex = 0;
               }

               /* non-zero offset cases:
                *             rc  ex  action
                *            === ===  ======
                * onStk        1   4   @ (pop to)
                *
                * !onStk
                *   pid        0   0   >
                *   pid + 1   -1   0   pop(to pidtid) & >   ****
                *  (pid + 1   -1   0   splice between pid & top)
                *   pid + 2   -2   0   >
                *   pid + 3+  -n   0   >
                */
            }
            else
            {
               ex = 0;                  /* call */
            }

            arc_out(ex, hp);            /* goto mnm via call or @ */
         }

         /* time */
         if (kstate == 0)
         {
            tot_trc = cum;
            tot_my += del;
            gv.dtime = (UNIT)tot_my;
            acct2();                    /* time accounting */
         }

         if (kstate == 1) kstate = 2;

         ocum = cum;
         odel = del;
         break;

      case 2:                           /* Process switch */
         pid = tokarr[5];
         tid = tokarr[8];
         pnm = tokarr[2];

         slast(pid);
         slast(pnm);

         strcpy(pnmbuf, pnm);
         strcat(pnmbuf, "_");
         strcat(pnmbuf, pid);
         strcat(pnmbuf, "_");
         strcat(pnmbuf, tid);

         if (kstate == 0)
         {                              /* pidtid switch now */
            if (strcmp(gv.apt, pnmbuf) != 0)
            {                           /* pidtid change */
               gv.apt = Remember(pnmbuf);
               gv.ptp = ptptr(pnmbuf);  /* find/add pidtid */
               gv.apt = gv.ptp->apt;
               arc_out(3, hp);
            }
            acct2();                    /* avoids stt problem */
         }
         else
         {                              /* save new pidtid */
            strcpy(spnmbufn, pnmbuf);
            newpt = 1;
         }
         break;

      case 3:                           /* KERNEL switch */
         if (strcmp(tokarr[0], "<==") == 0)
         {                              /* <= K */
            if (kstate >= 1)
            {
               if (istate == 3)
               {                        /* interrupt detected */
                  intcnt++;
                  iinstrs = istop - istart;
                  inti += iinstrs;
                  /***********/

                  OptVMsg(" -INT- %4d (%5d %5d %5d) %8d %8d\n",
                          intcnt, irec1, irec2, irec3, iinstrs, inti);

                  gv.ptp = ptptr("Interrupt");   /* find/add pidtid */
                  gv.apt = gv.ptp->apt;
                  arc_out(3, NULL);     /* pid switch */

                  /* push DEC PFT EXT OTHER */
                  if (itype == 0)
                  {
                     strcpy(itynm, "Other");
                  }
                  hp = hkdb_find(itynm, 1);
                  arc_out(0, hp);       /* push itype */

                  /* time */
                  tot_my  += iinstrs;
                  gv.dtime = (UNIT)tot_my;
                  acct2();

                  arc_out(1, hp);       /* pop itype */

                  /* switch back to prev pidtid */
                  gv.ptp = ptptr(spnmbufp);
                  gv.apt = gv.ptp->apt;
                  arc_out(3, NULL);
               }

               /* was K visit all interrupt ?? */
               kinstrs = ocum - kstart;
               if (istate == 3)
               {
                  kinstrs -= iinstrs;
               }
               if (kinstrs >= 1)
               {
                  /* record Kernel activity */
                  hp = hkdb_find(knm, 1);
                  arc_out(0, hp);       /* push Kernel */

                  hp = hkdb_find(ktynm, 1);
                  arc_out(0, hp);       /* push svc or kem */

                  /* time */
                  tot_my  += kinstrs;
                  gv.dtime = (UNIT)tot_my;
                  acct2();

                  arc_out(1, hp);       /* pop svc or kem */

                  hp = hkdb_find(knm, 1);
                  arc_out(1, hp);       /* pop Kernel */
               }

               if (newpt == 1)
               {
                  newpt = 0;
                  gv.ptp = ptptr(spnmbufn);
                  gv.apt = gv.ptp->apt;
                  arc_out(3, NULL);     /* switch to new */
               }

               kstate = kinstrs = 0;
            }
            else
            {                           /*  abnormal K exit */
               /* at most once per pidtid */
               gv.ptp->ctp = gv.ptp->rtp;   /* pop to pidtid */
               hp = hkdb_find(knm, 1);
               arc_out(1, hp);          /* exit kernel */
            }

            kstate = 0;
            istate = 0;
         }
         else
         {                              /* => K */
            strcpy(spnmbufp, gv.apt);   /* save prev pidtid */
            kstate = 1;
         }
         break;

      default:                          /* ERROR or not id'ed yet */
         fprintf(stderr, " UNKNOWN_REC_TYPE ????\n");
         for (i = 0; i < tcnt; i++)
         {
            fprintf(stderr, " %s", tokarr[i]);
         }
         fprintf(stderr, "\n");
         break;
      }
   }                                    /* while neof */

   /* flush Kernel activity */
   OptVMsg(" End ctrace: kstate %d ktype %d\n",
           kstate, ktype);

   /* make subroutine. simulated <== */
   if (kstate >= 1)
   {
      if (istate == 3)
      {                                 /* interrupt detected */
         gv.ptp = ptptr("Interrupt");   /* find/add pidtid */
         gv.apt = gv.ptp->apt;
         arc_out(3, NULL);              /* pid switch */

         /* push DEC PFT EXT OTHER */
         if (itype == 0)
         {
            strcpy(itynm, "Other");
         }
         hp = hkdb_find(itynm, 1);
         arc_out(0, hp);                /* push itype */

         /* time */
         iinstrs = istop - istart;
         tot_my  += iinstrs;
         gv.dtime = (UNIT)tot_my;
         acct2();

         arc_out(1, hp);                /* pop itype */

         /* switch back to prev pidtid */
         gv.ptp = ptptr(spnmbufp);
         gv.apt = gv.ptp->apt;
         arc_out(3, NULL);
      }

      /* was K visit all interrupt ?? */
      kinstrs = ocum - kstart;
      if (istate == 3)
      {
         kinstrs -= iinstrs;
      }
      if (kinstrs >= 1)
      {
         /* record Kernel activity */
         hp = hkdb_find(knm, 1);
         arc_out(0, hp);                /* push Kernel */

         hp = hkdb_find(ktynm, 1);
         arc_out(0, hp);                /* push svc or kem */

         /* time */
         tot_my  += kinstrs;
         gv.dtime = (UNIT)tot_my;
         acct2();

         arc_out(1, hp);                /* pop svc or kem */

         hp = hkdb_find(knm, 1);
         arc_out(1, hp);                /* pop Kernel */
      }

      if (newpt == 1)
      {
         newpt = 0;
         gv.ptp = ptptr(spnmbufn);
         gv.apt = gv.ptp->apt;
         arc_out(3, NULL);              /* switch to new */
      }
      kstate = kinstrs = 0;
   }
}                                       /* end of proc_ctrace */
