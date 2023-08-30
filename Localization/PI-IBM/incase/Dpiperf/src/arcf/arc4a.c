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

/* ************************************************************ */
int dump_rad(int cnt)
{
   /* dummy routine. used by parx. called in arc.c */
   return(cnt);
}

/* ************************************************************ */
struct HKDB * str2hptr(STRING s)
{
   return(hkdb_find(s, 1));
}

/* ************************************************************ */
int tagcmp(struct HKDB * h1, struct HKDB * h2)
{
   char * s1 = h1->hstr;
   char * s2 = h2->hstr;

   if ( (s1[1] == ':') && (s2[1] == ':') )
   {
      if (strcmp(&s1[2], &s2[2]) == 0) return(1);
   }
   return(0);
}

/* ************************************************************ */
int hkcomp(struct HKDB * h1, struct HKDB * h2, int push)
{

   if (h1 == h2) return(1);

   /* popping java types: JIBNC) */
   if ( (push == 0) && (gv.tags == 1) )
   {
      return(tagcmp(h1, h2));
   }

   return(0);
}

/* ************************************************************ */
struct HKDB * hp_x2e(struct HKDB * hx)
{
   return(hx);
}

/* ************************************************************ */
/* dummy routine */
void pt_init(pPIDTID p)
{
   p = (pPIDTID)0;
}

/* ************************************************************ */
void acctMPb(pPIDTID ptp, UNIT tm)
{
   UNIT del;

   /* init allocs/bytes to first occurance on pt */
   if (gv.ptp->first == 0)
   {
      gv.ptp->first = 1;
      gv.ptp->stptime = tm;
      OptVMsg(" init %s stptimeTo %f\n",
              gv.ptp->apt, gv.ptp->stptime);
   }

   if (ptp == NULL)
   {
      return;
   }

   del = tm - ptp->stptime;
   if (del <= .125)
   {
      del = 0;
   }

   /* OptVMsg(" del %.0f tm %.0f stptime %.0f\n",
      del, tm, ptp->stptime);
    */
   ptp->stptime = tm;
   ptp->atime += del;                   /* pidtid time */
   /* pt time for arcflow */

   gv.tottime += del;

   /* TREE. increment curr node time */
   ptp->ctp->btm += del;
   ptp->ctp->btmx += del;
}

/* ************************************************************ */
void acctMP(pPIDTID ptp)
{
   static int first = 0;
   static UNIT ztime;
   UNIT del;
   /*pPIDTID ptp = gv.aptp[cpu];*/

   if (first == 0)
   {
      first = 1;
      ztime = gv.dtime;
   }

   if (ptp == NULL)
   {
      return;
   }

   /* this var is misused in MP mode ?? */
   gv.deltime = del = gv.dtime - ptp->stptime;

   ptp->stptime = gv.dtime;
   ptp->atime += del;                   /* pidtid time */
   /* pt time for arcflow */

   gv.tottime = gv.dtime - ztime;

   /* TREE. increment curr node time */
   ptp->ctp->btm += del;
   ptp->ctp->btmx += del;
}

/* ************************************************************ */
void acct2ft(void)
{
   static first = 0;
   UNIT del;

   if (first == 0)
   {
      gv.odtime = gv.dtime;
      first = 1;
   }

   if (gv.dtime < gv.odtime) gv.dtime = gv.odtime;

   gv.deltime = del = gv.dtime - gv.odtime;
   gv.odtime  = gv.dtime;
   gv.ptp->atime += del;                /* pidtid time */
   /* pt time for arcflow */
   gv.tottime += del;
}

/* ************************************************************ */
void acct2(void)
{
   static first = 0;
   UNIT del;

   if (first == 0)
   {
      gv.odtime = gv.dtime;
      first = 1;
   }

   if (gv.dtime < gv.odtime) gv.dtime = gv.odtime;

   gv.deltime = del = gv.dtime - gv.odtime;
   gv.odtime  = gv.dtime;
   gv.ptp->atime += del;                /* pidtid time */
   /* pt time for arcflow */
   gv.tottime += del;

   /* TREE. increment curr node time */
   gv.ptp->ctp->btm += del;
   gv.ptp->ctp->btmx += del;
}

/* ************************************************************ */
void acct2l(int tflg)
{                                       /* 1 => time filter */
   static first = 0;
   UNIT del;

   if (first == 0)
   {
      gv.odtime = gv.dtime;
      first = 1;
   }

   if (gv.dtime < gv.odtime) gv.dtime = gv.odtime;

   gv.deltime = del = gv.dtime - gv.odtime;
   gv.odtime = gv.dtime;
   gv.ptp->atime += del;                /* pidtid time */
   /* pt time for arcflow */
   gv.tottime += del;

   /* TREE. increment curr node time */
   if (!tflg || gv.ptp->lflg == 1)
   {
      gv.ptp->ctp->btm += del;
      gv.ptp->ctp->btmx += del;
   }
}

/* ************************************************************ */
void acct2m(void)
{
   static first = 0;
   UNIT del;

   if (first == 0)
   {
      gv.odtime = gv.dtime;
      first = 1;
   }

   /* if(gv.dtime < gv.odtime) gv.dtime = gv.odtime; */

   gv.deltime = del = gv.dtime - gv.odtime;
   gv.odtime = gv.dtime;
   gv.ptp->atime += del;                /* pidtid time */
   /* pt time for arcflow */
   gv.tottime += del;

   /* TREE. increment curr node time */
   gv.ptp->ctp->btm += del;
   gv.ptp->ctp->btmx += del;
}

