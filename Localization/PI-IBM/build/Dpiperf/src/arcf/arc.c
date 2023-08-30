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

FILE * xtree;                           /* tree */
FILE * xtreep;                          /* pruned tree */
FILE * xbtree;                          /* tree */
FILE * xprof;                           /* flat profile */
FILE * xprofp;                          /* summary flat profile (by ctm & btm) */
FILE * xarc;                            /* arcflow(callers/callees) */
FILE * xarcp;                           /* pruned xarc */
FILE * xgiven;                          /* arcflow(x given y) */
FILE * xbecof;                          /* arcflow(x because of y) */
FILE * xflow;                           /* indented flow */
FILE * aflow;                           /* arcf equivalent of xflow */
FILE * xcondu;                          /* condensed flow */
FILE * xarcn;                           /* xarc w par & child trees */

struct HKDB * * php;                    /* used hooks & pidtids */
struct HKDB * * pha;                    /* arcflow tmp usage */
int nhu;                                /* number of non zero(hooks or pidtids) */
void cond_out(struct TREE * t);
struct HKDB * next_rad(int pid, aint addr);
void pop(struct HKDB * hp, int flg);

int ga_err = 0;
int ParLev = 0;
double ParCum = 0;
double ParBase = 0;
//
#define MAX_SARR 4096
struct TREE * sarr[MAX_SARR];           // should be malloc'ed to largest to date
char iistr[64][64];
char * plab[4] = {"Par", "rPar", "Chd", "rChd"};
int freed = 0;
int addnode = 0;                        // 1 if stkpush adds node (new unique guy)

/* ************************************************************ */
void accum(struct TREE * new, struct TREE * par)
{
   new->calls += par->calls;
   new->btm   += par->btm;
   new->ctm   += par->ctm;
}

/* ************************************************************ */
char * get_ind(int n)
{
   if (n < 50)
   {
      return(iistr[n]);
   }
   return(iistr[50]);
}

//***********************
struct TREE * stkpush(struct TREE * cp, struct HKDB * hp)
{
   struct TREE * np;
   struct TREE * sp;

   addnode = 0;
   np = cp->chd;                        // 1st child
   sp = 0;

   while (np != 0)
   {
      // parm3 = 0 : allow diff tags on entry / exit
      if (hkcomp(hp, np->hp, 0) == 1)
      {
         return(np);                    // match
      }
      else
      {
         sp = np;                       // last sib
         np = np->sib;                  // next sib
      }
   }

   // No Match. Add Node
   addnode = 1;
   np = const_tree();
   np->par = cp;
   np->hp  = hp;                        // hp of entry

   if (sp == 0)
   {                                    // 1st child
      cp->chd = np;
   }
   else
   {                                    // end sib list
      sp->sib = np;
   }
   return(np);                          // match
}

/* ************************************************************ */
struct TREE * walkchild(struct TREE * tp, int lev,
                        int maxlev, struct TREE * tq)
{

   struct TREE * p;                     // tree 1 (the input)
   struct TREE * q;                     // tree 2 (the composite)
   struct HKDB * hp;                    /* child hkdb ptr */
   int d = 0;                           // debug

   // output child rec on entry (descent)
   // lev 0 is the original self

   if (d) fprintf(xarc, " walkchd: %4d tp: %p tq: %p\n", lev, tp, tq);

   if (lev == 1) if (d) fprintf(xarc, "\n");
   if (lev >= 1)
   {
      // show child tree Node
      hp = tp->hp;
      if (d)
      {
         fprintf(xarc, " %7"_P64"d %8.0f %10.0f %4d) %s %s\n",
                 tp->calls, tp->btm, tp->ctm,
                 hp->pind, get_ind(lev), hp->hstr2);
      }
      // push that Node in dual
      q = stkpush(tq, hp);
      accum(q, tp);
   }
   else
   {
      q = tq;
   }

   p = tp->chd;

   if ( (lev + 1) <= gv.cdep)
   {
      while (p != NULL)
      {
         p = walkchild(p, lev + 1, maxlev, q);
      }
   }

   p = tp->sib;                         // get sib before free
   return(p);
}

/******************************/
struct TREE * tree_free(struct TREE * cx)
{
   struct TREE * nx;

   nx = cx->chd;
   while (nx != NULL)
   {
      nx = tree_free(nx);
   }

   nx = cx->sib;                        // get sib before free
   freed++;
   free(cx);

   return(nx);
}

/* ************************************************************ */
/* sort increasing using TREE entries by 1) ctm, 2) btm 3) calls */
int LKCONV qcmp_part_up(const void * p1, const void * p2)
{
   struct TREE * r;
   struct TREE * s;

   r = *(struct TREE **)p1;
   s = *(struct TREE **)p2;

   if ( (r->ctm - s->ctm) > .5) return(1);
   if ( (s->ctm - r->ctm) > .5) return(-1);

   if ( (r->btm - s->btm) > .5) return(1);
   if ( (s->btm - r->btm) > .5) return(-1);

   if (r->calls > s->calls) return(1);
   if (r->calls < s->calls) return(-1);

   return(0);
}

/* ************************************************************ */
/* sort using TREE entries by 1) ctm, 2) btm 3) calls */
int LKCONV qcmp_part_down(const void * p1, const void * p2)
{
   struct TREE * r;
   struct TREE * s;

   r = *(struct TREE **)p1;
   s = *(struct TREE **)p2;

   if ( (r->ctm - s->ctm) < .5) return(1);
   if ( (s->ctm - r->ctm) < .5) return(-1);

   if ( (r->btm - s->btm) < .5) return(1);
   if ( (s->btm - r->btm) < .5) return(-1);

   if (r->calls < s->calls) return(1);
   if (r->calls > s->calls) return(-1);

   return(0);
}

//****************************
// parent / child xarcn tree
//****************************
void walktree(struct TREE * p, int lev, FILE * fn, int r)
{
   struct TREE * q;
   struct HKDB * hp;
   int cnt = 0, i;
   char * hstr;
   double pcb, pcc;

   // sort child on descent
   q = p->chd;
   while (q != 0)
   {
      sarr[cnt] = q;
      cnt++;
      q = q->sib;
   }
   if ( cnt > MAX_SARR )
   {
      fprintf( stderr, "sarr overflow in walktree\n" );
      exit(1);
   }

   if (cnt >= 2)
   {
      // qsort(increasing) by cum, base, calls
      if (r <= 1)
      {                                 // par - increasing
         qsort((void *)sarr, cnt, sizeof(struct TREE *), qcmp_part_up);
      }
      else
      {
         qsort((void *)sarr, cnt, sizeof(struct TREE *), qcmp_part_down);
      }

      // sarr back to TREE
      p->chd = sarr[0];

      for (i = 0; i < cnt - 1; i++)
      {
         q = sarr[i];
         q->sib = sarr[i + 1];
      }
      q = sarr[cnt - 1];
      if (q != 0)
      {
         q->sib = 0;
      }
      else
      {
         fprintf(fn, " ZERO ?? sarr[%d] = %p\n", i, sarr[i]);
         fflush(fn);
      }
   }

   if (r >= 2)
   {                                    // Child on entry
      if (lev >= 0)
      {
         hp = p->hp;
         hstr = hp->hstr2;
         if (gv.snms == 1) hstr = hp->hstr3;

         if (lev >= 0)
         {
            if (p->ctm > gv.cclip)
            {
               if (lev == 0)
               {
                  fprintf(fn, "\n");
               }
               pcb = (100.0 * p->btm) / gv.tottime;
               pcc = (100.0 * p->ctm) / gv.tottime;
               fprintf(fn, " %6s %7"_P64"d %8.2f %10.2f %4d) %s %s\n",
                       plab[r], p->calls, pcb, pcc, hp->pind,
                       get_ind(lev), hstr);
            }
         }
      }
   }

   q = p->chd;
   while (q != 0)
   {
      walktree(q, lev + 1, fn, r);
      q = q->sib;
   }

   if (r <= 1)
   {                                    // Par on exit
      if (lev >= 0)
      {
         hp = p->hp;
         hstr = hp->hstr2;
         if (gv.snms == 1) hstr = hp->hstr3;

         if (p->ctm > gv.pclip)
         {
            if (lev >= ParLev)
            {
               fprintf(fn, "\n");
            }
            if ( (lev == 0) || (ParCum != p->ctm) || (ParBase != p->btm) )
            {
               pcb = (100.0 * p->btm) / gv.tottime;
               pcc = (100.0 * p->ctm) / gv.tottime;
               fprintf(fn, " %6s %7"_P64"d %8.2f %10.2f %4d) %s %s\n",
                       plab[r], p->calls, pcb, pcc, hp->pind,
                       get_ind(lev), hstr);
            }
            else
            {
               fprintf(fn, " %6s %5s %8s %10s %4d) %s %s\n",
                       plab[r], "", "", "", hp->pind,
                       get_ind(lev), hstr);
            }

            ParLev  = lev;
            ParCum  = p->ctm;
            ParBase = p->btm;
            fflush(fn);
         }
      }
      else
      {
         ParCum = 0;
      }
   }
}

//****************************
// parent / child xarcn tree
//****************************
void walkpc(struct TREE * p, int lev, FILE * fn, int * ptrp, int * ptrc)
{
   struct TREE * q;
   struct HKDB * hp;
   int cnt = 0;
   char * hstr;

   q = p->chd;

   if (lev >= 0)
   {
      if (lev == 0) *ptrp = (*ptrp) + 1;
      if (lev == 1) *ptrc = (*ptrc) + 1;

      hp = p->hp;
      hstr = hp->hstr2;
      if (gv.snms == 1) hstr = hp->hstr3;

      if (lev >= 0)
      {
         if (lev == 0)
         {
            fprintf(fn, "\n");
         }
         fprintf(fn, " %4d %s %s\n",
                 hp->pind, get_ind(lev + 1), hstr);
      }
   }

   q = p->chd;
   while (q != 0)
   {
      walkpc(q, lev + 1, fn, ptrp, ptrc);
      q = q->sib;
   }
}

//****************************
// parent / child xarcn tree
//****************************
void walkconf(struct TREE * p, int lev, FILE * fn, int r)
{
   struct TREE * q;
   struct HKDB * hp;
   int cnt = 0, i;
   char * hstr;
   double pcb, pcc;

   // sort child on descent
   q = p->chd;
   while (q != 0)
   {
      sarr[cnt] = q;
      cnt++;
      q = q->sib;
   }
   if ( cnt > MAX_SARR )
   {
      fprintf( stderr, "sarr overflow in walkconf\n" );
      exit(1);
   }

   if (cnt >= 2)
   {
      // qsort(increasing) by cum, base, calls
      if (r <= 1)
      {                                 // par - increasing
         qsort((void *)sarr, cnt, sizeof(struct TREE *), qcmp_part_up);
      }
      else
      {
         qsort((void *)sarr, cnt, sizeof(struct TREE *), qcmp_part_down);
      }

      // sarr back to TREE
      p->chd = sarr[0];

      for (i = 0; i < cnt - 1; i++)
      {
         q = sarr[i];
         q->sib = sarr[i + 1];
      }
      q = sarr[cnt - 1];
      if (q != 0)
      {
         q->sib = 0;
      }
      else
      {
         fprintf(fn, " ZERO ?? sarr[%d] = %p\n", i, sarr[i]);
         fflush(fn);
      }
   }

   //if(r >= 2) { // Child on entry
   if (lev >= 0)
   {
      hp = p->hp;
      hstr = hp->hstr2;
      //if(gv.snms == 1) hstr = hp->hstr3;

      if (lev >= 0)
      {
         //if(p->ctm > gv.cclip) {
         if (lev == 0)
         {
            fprintf(fn, "\n");
         }
         pcb = (100.0 * p->btm) / gv.tottime;
         pcc = (100.0 * p->ctm) / gv.tottime;
         fprintf(fn, " %6s %7"_P64"d %8.2f %10.2f %4d) %s %s\n",
                 plab[r], p->calls, pcb, pcc, hp->pind,
                 get_ind(lev), hstr);
         //}
      }
   }
   //}

   q = p->chd;
   while (q != 0)
   {
      walktree(q, lev + 1, fn, r);
      q = q->sib;
   }
}

/*****************************************************/
/* Initial tree node for condensed format output */
/*****************************************************/
void init_cond(struct TREE * t)
{
   size_t slen;

   t->chc = gv.hkccnt;
   slen = strlen(gv.hdstr);
   if (slen >= 1)
      t->hdstr = Remember(gv.hdstr);
   else
      t->hdstr = gv.nstr;
}

/* ************************************************************ */
void pop_ring0s(void)
{
   struct TREE * x;                     /* curr */

   x = gv.ptp->ctp;                     /* curr node for pt */

   while (x != NULL)
   {
      if (x->hp->ring == 0)
      {
         if (x->par != NULL)
         {                              /* dont pop thread node */
            OptVMsg(" POP-ring0 <%s>\n", x->hp->hstr2);
            /*gv.ptp->ctp = x->par;*/
            arc_out(1, x->hp);
         }
      }
      else
      {
         return;
      }
      x = x->par;
   }
   return;
}

/* ************************************************************ */
int onStack(struct HKDB * hp)
{
   int rc = 1;                          /* rc = 1      : OnStack
                               * rc =- 1     : Null Stack
                               * rc = -(d+1) : Not on Stack(d Deep Stack)
                               */
   struct TREE * x;                     /* curr */

   x = gv.ptp->ctp;                     /* curr node for pt */

   /*
   OptVMsg(" <%s> on stack hp=%x\n", hp->hstr2, hp);
    */

   /* NULL stk(pidtid only) goes thru while ONE time => rc = 0 */

   while (x != NULL)
   {
      /*
      OptVMsg(" stk: <%s> x->hp=%x \n", x->hp->hstr2, x->hp);
       */
      if (strcmp(x->hp->hstr, "K:kernel") == 0)
      {
         return(-10);                   /* k barrier */
      }

      if (hkcomp(hp, x->hp, 0) == 1)
      {
         return(2 - rc);                /* onStack (1 => Top) */
      }
      rc--;
      x = x->par;
   }

   return(rc);                          /* stack has -rc elements */
}

/* ************************************************************ */
struct TREE * pushName(struct TREE * cp, struct HKDB * hp)
{
   struct TREE * np;
   struct TREE * sp;

   sp = np = cp->chd;                   // 1st child

   while (np)
   {
      if (np->hp == hp)
      {
         return(np);
      }
      else
      {
         np = np->sib;                  // next sib
      }
   }

   np = const_tree();
   np->par = cp;
   np->hp  = hp;
   np->sib = sp;                        // old front

   cp->chd = np;                        // new front

   return(np);
}

/* ************************************************************ */
void push(struct HKDB * hp)
{
   struct TREE * cp;                    /* curr node for pt */
   struct TREE * rp;                    /* root node for pt */
   struct TREE * np;                    /* search node */
   struct TREE * sp;                    /* sib node */
   /* rev */
   struct TREE * stt;                   /* starting node */

   int hit = 0;
   int rc;

   /* struct HKDB * rh; */              /* recursion */

   dbgmsg(">> push(hp=%p)\n", hp);

   cp = gv.ptp->ctp;                    /* curr node for pt */
   rp = gv.ptp->rtp;                    /* root node for pt */
   sp = 0;
   stt = cp;

   /* Anti-recursion: hp onStack => pop */
   /*  Forgot the DesignPoint ???
    *   - if anywhere on stack then pop ???
    *   - if on top perhaps just stay here
    *   - if !top stay ??? push ??? pop ???
    *
    */

   if (gv.norecur)
   {
      rc = onStack(hp);
      if (rc == 1)
      {
         OptMsg(" ANTI-Recursion. Popping instead of pushing\n");
         OptVMsg("  Method : %s\n", hp->hstr2);
         pop(hp, 1);                    /* generates @ hp */
         cp = gv.ptp->ctp;              /* curr node for pt */
         cp->calls++;
         dbgmsg("<< push(hp=%p)\n", hp);
         return;
      }
   }

   /* key already child of tp ? */
   np = cp->chd;                        /* 1st child of cp */

   while ( (np != 0) && (hit == 0) )
   {
      if (hkcomp(hp, np->hp, 1) == 1)
      {
         hit = 1;                       /* found existing node */
         gv.ptp->ctp = np;
         np->calls++;

         np->stt = gv.dtime;            /* invocation start time */
         np->btmx = 0;                  /* invocation base time */
         if (gv.cond >= 1) init_cond(np);   /* rec entry of node */
      }
      else
      {
         sp = np;                       /* save last sib */
         np = np->sib;                  /* goto next sib */
      }
   }

   if (hit == 0)
   {
      /* create new node */
      np = const_tree();                /* during a push */

      np->calls = 1;
      np->par = cp;
      np->hp  = hp_x2e(hp);             /* get hp of entry */

      /* lev not needed during tree building */
      /* np->lev = cp->lev + 1; */      /* 1 greater than parent */

      np->stt = gv.dtime;               /* invocation start time */
      np->btmx = 0;                     /* invocation base time */
      if (gv.cond >= 1) init_cond(np);

      /* reversing order possible here */
      if (gv.rev == 1)
      {
         if (sp == 0)
         {                              /* new node is 1st child */
            cp->chd = np;
         }
         else
         {                              /* new node added at end ?? */
            np->sib = stt->chd;
            stt->chd = np;
            /* sp->sib = np; */
         }
      }
      else
      {
         if (sp == 0)
         {                              /* new node is 1st child */
            cp->chd = np;
         }
         else
         {                              /* new node added at end ?? */
            sp->sib = np;
         }
      }

      gv.ptp->ctp = np;                 /* new current */
   }
   np->lev = cp->lev + 1;               /* 1 greater than parent */

   dbgmsg("<< push(hp=%p)\n", hp);
   return;
}

/* ************************************************************ */
void pop(struct HKDB * hp, int flg)
{
   struct TREE * x;                     /* curr */
   struct TREE * oldx = NULL;           /* curr */
   struct TREE * y;                     /* prev */
   int hit    = 0;
   int popcnt = 0;
   int eind   = 0;
   int same = 0;
   struct HKDB * ht;                    /* top of stk */
   int cpu;

   /*
    * flg == 0 => "<"  (return from)
    * Good missing_@_pt for @ begins @ level 1
    * Bad  missing_@_pt for @ begins @ level >= 2
    *
    * flg == 1 => "@"  (return to)
    * Good missing_@_pt for < begins @ level 0
    * Bad  missing_@_pt for < begins @ level >= 1
    *
    */

   dbgmsg(">> pop(hp=%p, flg=%d)\n", hp, flg);

   x  = gv.ptp->ctp;                    /* curr node for pt */
   ht = x->hp;

   if (flg == 1)
   {                                    /* return_to */
      if (strcmp(hp->hstr2, x->hp->hstr2) == 0)
      {
         dbgmsg("<< pop(hp=%p) no tree change\n", hp);
         return;                        /* @a & curr = a => no tree change */
      }
      if (gv.mod == 1)
      {
         /* if modnm == modnm */
         if (strcmp(hp->modnm, ht->modnm) == 0) same = 1;
      }
   }

   while (hit == 0)
   {
      y = x;                            /* y is now curr */
      /* x will become the parent of y */
      if (popcnt == 1)
      {                                 /* 2nd pop => show top-of-stack */
         OptVMsg(" Missing Pop(%d): hkcnt %d time %.0f : TOP: %s\n",
                 0, gv.hkccnt, gv.dtime, oldx->hp->hstr2);
         OptVMsg("  ExitingMethod : %s\n", hp->hstr2);

         if (gv.flow == 1)
         {
            if (gv.group_disp) {
               cpu = gv.curcpu;
               if (gv.allrecs) {
                  dbgmsg(" *h* buffering CPU%d:  %10s %*s ** %s\n", cpu, "", oldx->lev, "", oldx->hp->hstr2);
                  fprintf(xflow, " *h* buffering CPU%d:  %10s %*s ** %s\n", cpu, "", oldx->lev, "", oldx->hp->hstr2);
               }

               fprintf(gv.ftempcpu[cpu], " %10s %*s ** %s\n", "", oldx->lev, "", oldx->hp->hstr2);
               gv.ftemp_cnt[cpu]++;
            }
            else {
               fprintf(xflow, " %10s %*s ** %s\n", "", oldx->lev, "", oldx->hp->hstr2);
            }
         }
      }

      if (x->par != 0)
      {                                 /* !@ pidtid */
         if (gv.cond >= 1) cond_out(x); /* Condensed. Leaving x */

         if (popcnt >= 1)
         {                              /* 2nd & beyond */
            ga_err = 1;
            OptVMsg(" Missing Pop(%d): hkccnt %d time %.0f : %s\n",
                    popcnt, gv.hkccnt, gv.dtime, x->hp->hstr2);

            if (gv.flow == 1)
            {
               if (gv.group_disp) {
                  cpu = gv.curcpu;
                  if (gv.allrecs) {
                     dbgmsg(" *h* buffering CPU%d:  %10s %*s ** %s\n", cpu, "", x->lev, "", x->hp->hstr2);
                     fprintf(xflow, " *h* buffering CPU%d:  %10s %*s ** %s\n", cpu, "", x->lev, "", x->hp->hstr2);
                  }

                  fprintf(gv.ftempcpu[cpu], " %10s %*s ** %s\n", "", x->lev, "", x->hp->hstr2);
                  gv.ftemp_cnt[cpu]++;
               }
               else {
                  fprintf(xflow, " %10s %*s ** %s\n", "", x->lev, "", x->hp->hstr2);
               }
            }
         }
         popcnt++;

         oldx = x;                      /* remember popped from */
         x = x->par;                    /* pop x, y curr, x parent-of-y */
      }

      else
      {                                 /* @ root: => missing tree node => OK | error */
         char emess[2][10] = {"OK", "ERR"};

         /* @ root w/o a pop => OK */
         if ( (flg == 0) && (popcnt >= 1) ) eind = 1;   /* < */
         if ( (flg == 1) && (popcnt >= 2) ) eind = 1;   /* @ */

         OptVMsg(" Missing @pt(%s / %d): hkccnt %d time %.0f : %s\n",
                 emess[eind], popcnt, gv.hkccnt, gv.dtime, y->hp->hstr2);
         if (gv.flow == 1)
         {
            if (eind == 1)
            {
               if (gv.group_disp) {
                  // buffer this up in the current cpu
                  cpu = gv.curcpu;
                  if (gv.allrecs) {
                     dbgmsg(" *e* buffering CPU%d:  %10s <> @pt %s %d\n", cpu, "", emess[eind], popcnt);
                     fprintf(xflow, " *e* buffering CPU%d:  %10s <> @pt %s %d\n", cpu, "", emess[eind], popcnt);
                  }

                  fprintf(gv.ftempcpu[cpu], " %10s <> @pt %s %d\n", "", emess[eind], popcnt);
                  gv.ftemp_cnt[cpu]++;
               }
               else {
                  fprintf(xflow, " %10s <> @pt %s %d\n", "", emess[eind], popcnt);
               }
            }
         }
         if (gv.cond >= 1)
         {
            cond_out(y);                /* xcondu : Leaving y */
         }

         /* new root node(pidtid node) */
         /* copy relevant data from y */

         /* extra flag to always do the BAD thing */
         if (eind == 0 && (gv.emode == 0))
         {                              /* OKAY case */
            /* change curr pt node(y) to node for exiting mnm */
            /* create new pt node */

            x = const_tree();
            x->calls = 1;
            x->par = 0;
            x->chd = y;
            x->hp  = y->hp;             /* this is the pidtid array */
            /* pt nodes not linked via sib ptr */
            x->lev = 0;
            x->stt = y->stt;
            x->btmx = 0;

            /* ptr old pt node to this new node */
            y->par = x;
            if (flg == 1) y->lev = 1;

            /* tree hp = entry hp */
            y->hp = hp_x2e(hp);         /* get hp of entry */

            gv.ptp->rtp = x;            /* new root */

            if (gv.cond >= 1) init_cond(y);   /**XCONDFIX**/

            if (flg == 0)
            {
               /* < case */
               y->lev = 1;              /* needed by cond_out */
               if (gv.cond >= 1) cond_out(y);   /**XCONDFIX**/
               gv.ptp->ctp = x;         /* new current */
            }
            else
            {
               /* @ case */
               gv.ptp->ctp = y;         /* new current */
            }

            hit = 1;
            dbgmsg("<< pop(hp=%p) hit == 1\n", hp);
            return;                     /* avoid ctp = x at bottom of while */
         }

         else
         {                              /* ERROR case */
            OptMsg(" missing Exit(NOT at pt)\n");
            gv.ptp->ctp = gv.ptp->rtp;  /* leave at pidtid level */
            push(hp);                   /* pushed for structure, not left there */

            if (flg == 0)
            {
               y = gv.ptp->ctp;         /* pick up current */
               if (gv.cond >= 1) cond_out(y);   /**XCONDFIX**/
               gv.ptp->ctp = gv.ptp->rtp;   /* return to pidtid */
            }
            else
            {
               /* gv.ptp->ctp = x; */   /* leave at pushed level */
            }

            dbgmsg("<< pop(hp=%p) missing exit (not at pt)\n", hp);
            return;                     /* avoid ctp = x at bottom of while */
         }
      }

      if (hit == 0)
      {
         /* at the right node yet ? */
         if (flg == 0)
         {                              /* popping from target node */
            hit = hkcomp(hp, y->hp, 0);
         }
         else
         {                              /* popping to target node */
            hit = hkcomp(hp, x->hp, 0);

            /* may be done popping ? */
            if ( (hit == 0) && (same == 1) )
            {
               if (strcmp(ht->modnm, x->hp->modnm) != 0)
               {
                  /* just return leaving in last node with same mod */
                  dbgmsg("<< pop(hp=%p) hit == 0 && same == 1\n", hp);
                  return;
               }
            }
         }
      }

      /* normal pop. if hit==1 then done */
      gv.ptp->ctp = x;                  /* pop: curr = parent-of-curr */
   }

   dbgmsg("<< pop(hp=%p) fell out while loop\n", hp);
   return;
}

/* ************************************************************ */
void showpt(void)
{
   int i;

   OptMsg("\n PT Table\n");
   for (i = 0; i < gv.PTcnt; i++)
   {
      OptVMsg(" ## %10.0f %2d %s\n",
              PTarr[i]->atime, i, PTarr[i]->apt);
   }
   OptMsg("\n");
}

/* ************************************************************ */
/* resort PTarr to correct a name change */
void ptsort(void)
{
   int i;
   pPIDTID tmp;

   /* 1 pass bubble sort */
   for (i = 0; i < gv.PTcnt - 1; i++)
   {
      if (strcmp(PTarr[i]->apt, PTarr[i + 1]->apt) < 0)
      {
         tmp = PTarr[i];
         PTarr[i] = PTarr[i + 1];
         PTarr[i + 1] = tmp;
      }
   }
}

/* ************************************************************ */
int LKCONV qcmp_chdcmp(const void * p1, const void * p2)
{
   struct TREE * r;
   struct TREE * s;

   r = *(struct TREE **)p1;
   s = *(struct TREE **)p2;

   if (r->ctm <= s->ctm)
   {
      if ( (s->ctm - r->ctm) > .125) return(1);
      return(0);
   }
   return(-1);
}

/* ************************************************************ */
int LKCONV qcmp_hkbtm(const void * p1, const void * p2)
{
   struct HKDB * r;
   struct HKDB * s;

   r = *(struct HKDB **)p1;
   s = *(struct HKDB **)p2;

   if (r->btm <= s->btm)
   {
      /* if(r->btm < s->btm) {*/
      if ( (s->btm - r->btm) > .125)
      {
         return(1);
      }
      else
      {                                 /* equal */
         /* return(0); */
         if (r->ctm <= s->ctm)
         {
            if ( (s->ctm - r->ctm) > .125)
            {
               return(1);
            }
            else
            {
               return(0);
            }
         }
         else
         {
            return(-1);
         }
      }
   }
   else return(-1);
}

/* ************************************************************ */
int LKCONV qcmp_hkctm(const void * p1, const void * p2)
{
   struct HKDB * r;
   struct HKDB * s;

   r = *(struct HKDB **)p1;
   s = *(struct HKDB **)p2;

   if (r->ctm <= s->ctm)
   {
      if ( (s->ctm - r->ctm) > .125)
      {
         return(1);
      }
      else
      {
         if (r->btm <= s->btm)
         {
            if ( (s->btm - r->btm) > .125)
            {
               return(1);
            }
            else
            {
               if (r->calls <= s->calls)
               {
                  if (r->calls < s->calls)
                  {
                     return(1);
                  }
                  else
                  {
                     if (strcmp(r->hstr2, s->hstr2) >= 0)
                     {
                        if (strcmp(r->hstr2, s->hstr2) > 0)
                        {
                           return(1);
                        }
                        else
                        {
                           return(0);
                        }
                     }
                     else
                     {
                        return(-1);
                     }
                  }
               }
               else
               {
                  return(-1);
               }
            }
         }
         else
         {
            return(-1);
         }
      }
   }
   else return(-1);
}

/* ************************************************************ */
/* sort used hkdb entries by 1) ctm, 2) btm 3) calls */
int LKCONV qcmp_par(const void * p1, const void * p2)
{
   struct HKDB * r;
   struct HKDB * s;

   r = *(struct HKDB **)p1;
   s = *(struct HKDB **)p2;

   if ( (r->tctm - s->tctm) > .5) return(1);
   if ( (s->tctm - r->tctm) > .5) return(-1);

   if ( (r->tbtm - s->tbtm) > .5) return(1);
   if ( (s->tbtm - r->tbtm) > .5) return(-1);

   if (r->tcalls > s->tcalls) return(1);
   if (r->tcalls < s->tcalls) return(-1);

   return(0);
}

/* ************************************************************ */
/* sort used hkdb entries by 1) ctm, 2) btm 3) calls */
int LKCONV qcmp_par2(const void * p1, const void * p2)
{
   struct HKDB * r;
   struct HKDB * s;

   r = *(struct HKDB **)p1;
   s = *(struct HKDB **)p2;

   if (r->tctm >= s->tctm)
   {
      if ( (r->tctm - s->tctm) > .125)
      {
         return(1);
      }
      else
      {                                 /* = ctm */
         /* if( (r->tbtm - s->tbtm) > .125) {*/
         if (r->tbtm >= s->tbtm)
         {
            if ( (r->tbtm - s->tbtm) > .125)
            {
               return(1);
            }
            else
            {                           /* = btm */
               if (r->calls >= s->calls)
               {
                  if (r->calls > s->calls)
                  {
                     return(1);
                  }
                  else return(0);
               }
               else return(-1);
            }
         }
         else
         {
            return(-1);
         }
      }
   }
   else return(-1);
}

/*****************************************************/
/* Create Indent String For xflow & xcondu */
STRING get_indent(int lev)
{
   static char istr[80] = "----+----+----+----+----+----+----+----+----+----+----+----+";
   static int olev = 0;
   int maxdent = 50;

   /* TRICKY: restore nulled slot from previous call */
   /* null slot in istr to produce correct string out */

   if (4 == (olev % 5) ) istr[olev] = '+';
   else istr[olev] = '-';

   if (lev >= maxdent) lev = maxdent;
   istr[lev] = CZERO;
   olev = lev;

   return(istr);
}

/*****************************************************/
/* Condensed format output */
/*****************************************************/
void cond_out(struct TREE * t)
{
   UNIT   deltm;
   STRING istr;
   char   c;
   static first = 0;

   if (first == 0)
   {
      first = 1;
      fprintf(xcondu,
              "%8d %10s %10s %8s %8s %s\n",
              0, "Start", "Stop", "Delta", "Base", "Method");
      fprintf(xcondu,
              "%8d %10s %10s %8s %8s %s\n",
              1, "======", "======", "=====", "=====", "======");
   }

   deltm = gv.dtime - t->stt;
   t->wtm += deltm;

   /* add hk desc str & indenting */
   if (gv.cond >= 1)
   {                                    /* redundant check ???? */
      if (t->lev >= 1)
      {                                 /* do nothing for pidtids here */
         //istr = get_indent(t->lev);
         istr = get_ind(t->lev);

         if (t->hp->mat == 1)
         {                              /* matched exit hook */
            if (gv.hdstr[0] != CZERO) c = ':';
            else c = ' ';

            /* output at entry time w both entry & exit data */
            fprintf(xcondu,
                    "%8d %10.0f %10.0f %8.0f %8.0f %s %s %s %c %s\n",
                    4 * t->chc, t->stt, gv.dtime, deltm, t->btmx, istr,
                    t->hp->hstr2, t->hdstr, c, gv.hdstr);
            /* t->hdstr : entry data string */

            /* output at exit also */
            if (gv.cond == 2)
            {
               fprintf(xcondu, "%8d %10.0f %10s %8s %8s %s %s\n",
                       4 * gv.hkccnt - 3, gv.dtime, "", "", "",
                       istr, t->hp->hstr2);
            }
         }
         else
         {                              /* unmatched */
            fprintf(xcondu,
                    "%8d %10.0f %10.0f %8.0f %8.0f %s %s %s\n",
                    4 * t->chc, t->stt, gv.dtime, deltm, t->btmx,
                    istr, t->hp->hstr2, gv.hdstr);
         }
         if (t->hdstr[0] != CZERO) free(t->hdstr);
      }
   }
}

/*****************************************************/
/*****************************************************/
void stackout(struct TREE * p, FILE * fn)
{
   struct TREE * arr[128];
   int i, slev;

   /* collect node pointers back to root */
   gv.tlines = 0;

   slev = p->lev;

   if (p->lev < 128)
   {
      fprintf(fn, "\n");

      for (i = slev - 1; i >= 0; i--)
      {
         p = p->par;
         arr[i] = p;
      }

      for (i = 0; i < slev; i++)
      {
         p = arr[i];
         /* Change crttm to input record number */
         /*
         fprintf(fn, " %7.2f %2d %2d %9s %*s %s\n",
            100 * p->crttm / gv.tottime, p->lev, p->rlev,
            "", p->lev, "", p->hp->hstr2);
          */
         fprintf(fn, " %7.0f %2d %2d %9s %*s %s\n",
                 p->crttm, p->lev, p->rlev,
                 "", p->lev, "", p->hp->hstr2);
      }
   }
   else
   {
      fprintf(fn, "\n Stack > 128. NO stack trace!\n");
   }
   fprintf(fn, "\n");
}

/* ************************************************************ */
void profile(struct HKDB * hpp, int flg)
{
   int i;
   int cnt = 0;
   struct HKDB  * hp;
   pPIDTID ppt;                         /* PitTid Array ptr */
   FILE * xfile;

   if (flg == 0) xfile = xgiven;
   else         xfile = xbecof;

   if ( (hpp->calls >= 1) || (hpp->ctm >= 1) )
   {
      for (i = 0; i < gv.PTcnt; i++)
      {
         ppt = PTarr[i];
         if (flg == 0) walk3(ppt->rtp, hpp);   /* xgiven */
         else         walk4(ppt->rtp, hpp);   /* xbecof */
      }

      /* copy non-zero ptrs from php to pha */
      cnt = 0;
      for (i = 0; i < nhu; i++)
      {
         hp = php[i];
         if ( (hp->tctm != 0) || (hp->tcalls != 0) )
         {
            pha[cnt] = hp;
            cnt++;
         }
      }

      qsort((void *)pha, cnt, sizeof(struct HKDB *), qcmp_par);

      fprintf(xfile, "\n %10s %10s %10s %10s\n",
              "Calls", "Base", "Cum", "Hook");
      fprintf(xfile, " %10s %10s %10s %10s\n",
              "==========", "==========", "==========", "==========");
      fprintf(xfile, " %10d %10.0f %10.0f [%d] %s\n\n",
              hpp->calls, hpp->btm, hpp->ctm, hpp->pind, hpp->hstr2);

      for (i = cnt - 1; i >= 0; i--)
      {
         hp = pha[i];
         if ( (hp->tctm != 0) || (hp->tcalls != 0) )
         {
            fprintf(xfile, " %7"_P64"d %10.0f %10.0f %s\n",
                    hp->tcalls, hp->tbtm, hp->tctm, hp->hstr2);
         }
         /* clear for next pass */
         hp->tctm   = hp->tbtm    = 0;
         hp->tcalls = hp->nrcalls = 0;
      }
   }
}

/* ************************************************************ */
void xarc_line(FILE * fh, char * type, UINT64 calls, UNIT pbase, UNIT pcum)
{
   /* without method name & \n */

   if (gv.raw == -1)
   {                                    /* raw-unscaled only */
      fprintf(fh, " %10s %7"_P64"d %7.2f %7.2f", type, calls,
              pbase, pcum);
   }
   else
   {
      fprintf(fh, " %10s %7"_P64"d %7.2f %7.2f", type, calls,
              100 * pbase / gv.tottime, 100 * pcum  / gv.tottime);
   }

   if (gv.raw >= 1)
   {
      fprintf(fh, " %*.0f %*.0f",
              gv.pwid, pbase / gv.raw, gv.pwid, pcum  / gv.raw);
   }
}

/* ************************************************************ */
void xarc_line2(FILE * fh, int ind, char * mnm)
{
   if ( gv.html )
   {
      fprintf(fh, " <A HREF=\"#T_%d\">%d)</A> %s\n", ind, ind, mnm);
   }
   else
   {
      fprintf(fh, " %4d) %s\n", ind, mnm);
   }
}

/* ************************************************************ */
void arc_child(struct HKDB * hp, FILE * fh, UNIT clip)
{
   struct HKDB * p;
   struct TREE * tp;
   struct TREE * tpp;                   /* child tree node */
   struct HKDB * hpp;                   /* child hkdb ptr */
   int ucnt = 0;                        /* number of child tree nodes */
   int i;

   static char buf[256];
   static char tbuf[64];

   // child trees
   int slev = 0;
   struct TREE * root;                  // Child tree
   struct TREE * rroot;                 // rChild tree
   struct TREE * tstk;                  //

   tp = hp->nxm;                        /* first tree node with this hkkey */

   /* accumulate into hkdb structs */
   while (tp != 0)
   {                                    /* more entries on next me list */
      tpp = tp->chd;
      while (tpp != 0)
      {
         ucnt++;
         hpp = tpp->hp;                 /* accumulate in hkdb structs */
         hpp->tcalls += tpp->calls;
         hpp->tbtm   += tpp->btm;
         hpp->tctm   += tpp->ctm;

         if (tp->rlev == 1)
         {
            hpp->nrcalls += tpp->calls;
            hpp->nrbtm   += tpp->btm;
            hpp->nrctm   += tpp->ctm;
         }
         tpp = tpp->sib;
      }
      tp = tp->nxm;
   }

   if (ucnt >= 1)
   {
      ucnt = 0;

      /* copy to pha array */
      for (i = 0; i < nhu; i++)
      {
         p = php[i];
         if ( (p->tctm != 0) || (p->tcalls != 0) )
         {
            pha[ucnt] = p;
            ucnt++;
         }
      }

      qsort((void *)pha, ucnt, sizeof(struct HKDB *), qcmp_par);

      /* output in reversed sort order */
      for (i = ucnt - 1; i >= 0; i--)
      {
         p = pha[i];
         if ( (p->tctm != 0) || (p->tcalls != 0) )
         {
            if ( (p->nrctm != 0) || (p->nrcalls != 0) )
            {
               if (p->nrctm > clip)
               {
                  xarc_line(fh, "Child", p->nrcalls, p->nrbtm, p->nrctm);
                  /*fprintf(fh, " %4d) %s\n", p->pind, p->hstr2);*/
                  xarc_line2(fh, p->pind, p->hstr2);
               }
            }
            if ( (p->tctm - p->nrctm) || (p->tcalls - p->nrcalls) )
            {
               if (p->nrctm > clip)
               {
                  xarc_line(fh, "rChild",
                            p->tcalls - p->nrcalls,
                            p->tbtm - p->nrbtm,
                            p->tctm - p->nrctm);
                  xarc_line2(fh, p->pind, p->hstr2);
               }
            }
            /* clear for next pass */
            p->nrctm   = p->nrbtm  = 0;
            p->tctm    = p->tbtm   = 0;
            p->nrcalls = p->tcalls = 0;
         }
      }
   }

   ///// Begin Child Trees
   if (gv.cdep >= 1)
   {
      if (fh == xarc)
      {                                 // vs. xarc.p
         root  = const_tree();
         rroot = const_tree();

         tp = hp->nxm;                  /* 1st tree node with this Self mnm */

         while (tp != 0)
         {                              // nxm loop
            // walk child to "cdep" level, build composite
            // tp : next Self in nmx
            if (tp->rlev == 1) tstk = root;
            else              tstk = rroot;

            walkchild(tp, 0, gv.cdep, tstk);

            tp = tp->nxm;
         }

         walktree(root,  -1, xarc, 2);
         walktree(rroot, -1, xarc, 3);
         fprintf(xarc, "\n");

         // remove child-trees
         freed = 0;
         tree_free(root);

         freed = 0;
         tree_free(rroot);
      }
   }
   ///// End Child Trees
}

/* ************************************************************ */
// called once per Self to accum the Parent data
void arc_parent(struct HKDB * hp, FILE * fh, UNIT clip)
{

   struct HKDB * p;
   struct TREE * tp;
   struct TREE * tpp;                   /* parent tree node */
   struct TREE * tpar;                  /* parent tree node */
   struct HKDB * hpp;                   /* parent hkdb ptr */
   int ucnt = 0;                        /* number of parent tree nodes */
   int i;

   static char buf[256];
   static char tbuf[64];

   // parent trees
   int slev = 0;
   struct TREE * root;                  // Par tree
   struct TREE * rroot;                 // par tree
   struct TREE * tstk;                  // par tree

   if ( gv.html )
   {
      fprintf(fh, "<A NAME=\"T_%d\"></A>\n", hp->pind);
   }

   sprintf(buf, " %10s %7s %7s %7s",
           "=======", "=====", "=====", "=====");
   if (gv.raw >= 1)
   {
      sprintf(tbuf, " %*s %*s",
              gv.pwid, "=======", gv.pwid, "=======");
      strcat(buf, tbuf);
   }
   sprintf(tbuf, " %10s", "==========");
   strcat(buf, tbuf);
   fprintf(fh, "%s\n", buf);

   /* follow next me pointers for hk */
   /* accumulate parent arcs into hkdb structs(via pha) */

   ///// Begin Parent Trees
   if (gv.pdep >= 1)
   {
      if (fh == xarc)
      {                                 // vs. xarc.p
         root  = const_tree();
         rroot = const_tree();

         tp = hp->nxm;                  /* first tree node with this hkkey */

         while (tp != 0)
         {                              // nxm loop
            slev = 0;

            tpar = tpp = tp->par;       // parent node

            // per nxm
            if (tp->rlev == 1) tstk = root;
            else              tstk = rroot;

            while (tpp != 0)
            {                           // par stk loop
               if (slev < gv.pdep)
               {
                  hpp  = tpp->hp;       // name struct of tree Node
                  tstk = stkpush(tstk, hpp);   // push 1 parent stack
                  accum(tstk, tp);

                  tpp = tpp->par;
                  slev++;
               }
               else break;
            }

            tp = tp->nxm;
         }

         walktree(rroot, -1, xarc, 1);
         walktree(root,  -1, xarc, 0);
         fprintf(xarc, "\n");

         freed = 0;
         tree_free(rroot);
         tree_free(root);
      }
   }
   ///// End Parent Trees


   tp = hp->nxm;                        /* first tree node with this hkkey */

   while (tp != 0)
   {
      tpp = tp->par;                    /* parent tree node */
      if (tpp != 0)
      {
         ucnt++;
         hpp = tpp->hp;                 /* accum in ORIG hkdb structs */

         hpp->tcalls += tp->calls;
         hpp->tbtm   += tp->btm;
         hpp->tctm   += tp->ctm;

         if (tp->rlev == 1)
         {
            hpp->nrcalls += tp->calls;
            hpp->nrbtm   += tp->btm;
            hpp->nrctm   += tp->ctm;
         }
      }
      tp = tp->nxm;
   }

   /* copy used to pha */
   if (ucnt >= 1)
   {
      int cnt = 0;

      /* copy FROM php TO pha array */
      for (i = 0; i < nhu; i++)
      {
         p = php[i];                    /* tricky. Only php ptrs can be non-zero */
         if ( (p->tctm != 0) || (p->tcalls != 0) )
         {
            pha[cnt] = p;
            cnt++;
         }
      }

      qsort((void *)pha, cnt, sizeof(struct HKDB *), qcmp_par);

      /* output in sort order */
      for (i = 0; i < cnt; i++)
      {
         p = pha[i];
         if ( (p->tctm != 0) || (p->tcalls != 0) )
         {
            if ( (p->nrctm != 0) || (p->nrcalls != 0) )
            {
               if (p->nrctm > clip)
               {
                  xarc_line(fh, "Parent", p->nrcalls, p->nrbtm, p->nrctm);
                  xarc_line2(fh, p->pind, p->hstr2);
               }
            }
            /* rParent output */
            /* if any difference: (total vs nonrecursive) (ctm & calls) */
            if ( (p->tctm - p->nrctm) || (p->tcalls - p->nrcalls))
            {
               /* if(p->nrctm > clip) { */
               if ( (p->tctm - p->nrctm) > clip)
               {
                  xarc_line(fh, "rParent",
                            p->tcalls - p->nrcalls,
                            p->tbtm - p->nrbtm,
                            p->tctm - p->nrctm);
                  xarc_line2(fh, p->pind, p->hstr2);
               }
            }
            /* moved 3-lines from here to below */
         }

         /* clear for next pass */
         p->nrctm   = p->nrbtm  = 0;
         p->tctm    = p->tbtm   = 0;
         p->nrcalls = p->tcalls = 0;
      }
   }
}

/* ************************************************************ */
void arc_self(struct HKDB * hp, FILE * fh, UNIT clip)
{
   UNIT   tbtm;                         // Total base time
   UNIT   tctm;                         // Total cum  time
   UNIT   nrbtm;                        // Non-recursive base time
   UNIT   nrctm;                        // Non-recursive cum  time
   UINT64 tcalls;                       // Total calls
   UINT64 nrcalls;                      // Non-recursive calls

   struct TREE * ts;                    // tree self

   // new for CNode analysis (confounding Node)
   struct HKDB * hpp;                   // parent name struct
   struct HKDB * hpc;                   // child  name struct

   struct TREE * tp;
   struct TREE * tc;                    // tree child
   struct TREE * txr;                   // cnode root
   struct TREE * txruc;                 // cnode root for unique children
   struct TREE * txp;                   // cnode - par
   struct TREE * txc;                   // cnode - chd

   struct TREE * ttmp;
   // new for CNode analysis

   tbtm = tctm = nrbtm = nrctm = 0;
   tcalls = nrcalls = 0;

   ts = hp->nxm;                        /* 1st nxm node */

   /* recalc. separate total & non-recursive */
   while (ts != 0)
   {
      tcalls += ts->calls;
      tbtm   += ts->btm;
      tctm   += ts->ctm;
      if (ts->rlev == 1)
      {
         nrcalls += ts->calls;
         nrbtm   += ts->btm;
         nrctm   += ts->ctm;
      }
      ts = ts->nxm;
   }

   if (tctm >= clip)
   {
      fprintf(fh, "\n");
      xarc_line(fh, "Self", nrcalls, nrbtm, nrctm);
      fprintf(fh, " [%d]  %s\n", hp->pind, hp->hstr2);

      /* CTM2 CHANGE */
      if (tctm != nrctm)
      {
         xarc_line(fh, "rSelf", tcalls - nrcalls,
                   tbtm - nrbtm, tctm - nrctm);
         fprintf(fh, "\n");
      }
      fprintf(fh, "\n");
   }

   // output Par,Chd pairs per self (triplets when self considered)
   if ( (gv.cnode == 1) && (fh == xarc) )
   {
      int upar, uchd, cnum = 0;

      if (gv.pdep + gv.cdep >= 1)
      {
         txr   = const_tree();
         txruc = const_tree();

         ts = hp->nxm;                  /* 1st nxm node */
         fprintf(xarcn, "\n Self %4d %s\n", hp->pind, hp->hstr2);
         fflush(xarcn);

         while (ts != NULL)
         {                              // while next me
            tp = ts->par;               // parent node
            tc = ts->chd;               // child node

            if (tp != NULL)
            {                           // parent exists
               hpp = tp->hp;            // parent name
               while (tc != NULL)
               {                        // child loop
                  hpc = tc->hp;

                  // pc tree here
                  ttmp = stkpush(txr,  hpp);   // parent
                  upar = addnode;       // new parent

                  ttmp = stkpush(ttmp, hpc);   // child

                  // c tree here
                  stkpush(txruc, hpc);  // child
                  uchd = addnode;       // new child

                  if ( (upar + uchd) == 2)
                  {
                     cnum++;
                     fprintf(xarc, " - %4d %4d %s == %s\n",
                             hpp->pind, hpc->pind, hpp->hstr3, hpc->hstr3);
                  }

                  fprintf(xarcn, " -- %4d %4d %s ==== %s\n",
                          hpp->pind, hpc->pind, hpp->hstr3, hpc->hstr3);
                  tc = tc->sib;
               }
            }
            ts = ts->nxm;
         }

         {
            int pcnt = 0;
            int ccnt = 0;
            int tmp;

            if (hp->pind == 0)
            {
               fprintf(xarc, " CNode %3s %3s %3s\n\n",
                       "CN#", "P", "C");
               fprintf(xarcn, " CNode %3s %3s %3s [%4s] %s\n\n",
                       "CN#", "P", "C", "Ind", "Name");
            }

            fprintf(xarcn, " << Parent-Self-Child Tree Walk\n");
            walkpc(txr, -1, xarcn, &pcnt, &ccnt);

            // unique children
            fprintf(xarcn, " << Unique Child\n");
            walkpc(txruc, -1, xarcn, &tmp, &tmp);

            fprintf(xarc, " CNode %3d %3d %3d\n\n",
                    cnum, pcnt, ccnt);
            fprintf(xarcn, " CNode %3d %3d %3d [%4d] %s\n",
                    cnum, pcnt, ccnt, hp->pind, hp->hstr2);
            fprintf(xarcn, " << After Tree Walking\n");
         }
      }
   }

////////////
#ifdef xxxx
   root  = const_tree();
   rroot = const_tree();

   tp = hp->nxm;                        /* first tree node with this hkkey */

   while (tp != 0)
   {                                    // nxm loop
      slev = 0;

      tpar = tpp = tp->par;             // parent node

      // per nxm
      if (tp->rlev == 1) tstk = root;
      else              tstk = rroot;

      while (tpp != 0)
      {                                 // par stk loop
         if (slev < gv.pdep)
         {
            hpp  = tpp->hp;             // name struct of tree Node
            tstk = stkpush(tstk, hpp);  // push 1 parent stack
            accum(tstk, tp);

            tpp = tpp->par;
            slev++;
         }
         else break;
      }

      tp = tp->nxm;
   }

   walktree(rroot, -1, xarc, 1);
   walktree(root,  -1, xarc, 0);
   fprintf(xarc, "\n");

   freed = 0;
   tree_free(rroot);
   tree_free(root);
#endif
////////////


   // only if the xarcn options selected
   if (0 == 1)
   {                                    // adding some xarc self stuff
      // Prototype. not working
      // confounding node analysis
      txr  = const_tree();

      if (fh == xarc)
      {                                 // vs. xarc.p
         ts = hp->nxm;                  /* 1st nxm node */

         while (ts != 0)
         {
            // non-resursive selfs only ???
            if (ts->rlev == 1)
            {
               tp  = ts->par;           // parent node
               if (tp != NULL)
               {
                  hpp = tp->hp;         // parent name
                  txp = stkpush(txr, hpp);   // Push parent

                  fprintf(xarcn, " - push par %4d %s\n",
                          hpp->pind, hpp->hstr2);

                  // children of Self
                  tc = ts->chd;

                  while (tc != NULL)
                  {
                     hpp = tc->hp;
                     txc = stkpush(txp, hpp);   // push child

                     fprintf(xarcn, " -- push Child %4d %s\n",
                             hpp->pind, hpp->hstr2);

                     accum(txc, tc);

                     tc = tc->sib;
                  }
               }
            }
            ts = ts->nxm;
         }
         // output to xarcn
         fprintf(xarcn, " bef walkconf(txr)\n");
         walkconf(txr, 0, xarcn, 4);
         fprintf(xarcn, " aft walkconf(txr)\n");
      }
      tree_free(txr);
   }
}

/* ************************************************************ */
void arc_hdr(FILE * fh)
{

   units(fh);

   fprintf(fh, "\n ArcFlow Output\n");
   fprintf(fh, "    Base - Units directly in function\n");
   fprintf(fh, "    Cum  - Units directly & indirectly in function\n\n");

   fprintf(fh, "    Parent, Self & Child entries are sums over all non-recursive\n");
   fprintf(fh, "      nodes for a given method\n");
   fprintf(fh, "    rParent, rSelf & rChild entries are sums over all recursive \n");
   fprintf(fh, "      nodes for the same method\n\n");

   fprintf(fh, " ArcFlow Invarients:\n");
   fprintf(fh, "    1) Sum(Parent(Calls)) = Self(Calls)\n");
   fprintf(fh, "    2) Sum(Parent(Base))  = Self(Base)\n");
   fprintf(fh, "    3) Sum(Parent(Cum))   = Self(Cum)\n");
   fprintf(fh, "    4) Sum(Child(Cum))    = Self(Cum) - Self(Base)\n\n");

   fprintf(fh, "    5) Sum(rParent(Calls)) = rSelf(Calls)\n");
   fprintf(fh, "    6) Sum(rParent(Base))  = rSelf(Base)\n");
   fprintf(fh, "    7) Sum(rParent(Cum))   = rSelf(Cum)\n");
   fprintf(fh, "    8) Sum(rChild(Cum))    = rSelf(Cum) - rSelf(Base)\n");
   fprintf(fh, "\n");

   fprintf(fh, " %10s %7s %7s %7s",
           "Source", "Calls", "%Base", "%Cum");
   if (gv.raw >= 1)
   {
      fprintf(fh, "%*s %*s",
              gv.pwid, "Base", gv.pwid, "Cum");
   }
   fprintf(fh, " %10s\n", "Function");

}

/* ************************************************************ */
/* min: 1) prune to pass = 3 & recalc gv.tottime */
/* ************************************************************ */
void walk1min(struct TREE * p, int lev)
{
   struct TREE * pc;

   if (p->pass != 3)
   {
      p->btm = 0;
   }
   gv.tottime += p->btm;

   pc = p->chd;

   while (pc != 0)
   {
      walk1min(pc, lev + 1);
      pc = pc->sib;
   }
}

/* ************************************************************ */
/* min: 1) prune to pass = 3 & recalc gv.tottime */
/* ************************************************************ */
int walk1minx(struct TREE * p, int lev)
{
   struct TREE * pc;
   int rc;

   if (p->pass == 3)
   {
      gv.tottime += p->btm;

      pc = p->chd;
      while (pc != 0)
      {
         rc = walk1minx(pc, lev + 1);
         if (rc == 1)
         {                              /* remove pc from tree */
            /* rewrite the chd or sib contact */
         }
         pc = pc->sib;
      }
   }
   else
   {
      /* remove this node from the tree */
      p->btm = 0;
      return(1);
   }
   return(0);
}

/* ************************************************************ */
/* calc: 1) cum-abs only */
/* ************************************************************ */
UNIT walk1abs(struct TREE * p, int lev, FILE * fn)
{
   struct TREE * pc;

   p->ctma = p->btm;
   if (p->ctma < 0) p->ctma = -p->btm;

   pc = p->chd;
   while (pc != 0)
   {                                    /* par accums from child */
      p->ctma += walk1abs(pc, lev + 1, fn);
      pc = pc->sib;
   }

   return(p->ctma);
}

/* ************************************************************ */
/* calc: 1) cum 2) rec_depth 3) nextme */
/* ************************************************************ */
UNIT walk1(struct TREE * p, int lev, FILE * fn)
{
   struct TREE * tp1;
   struct TREE * pc;
   static int mlev;

   /// debug. watch max level grow
   if (lev > mlev)
   {
      mlev = lev;
      if (0 == mlev % 16)
      {
         OptVMsg(" maxlev(walk1) %3d\n", mlev);
      }
   }

   p->ctm  = p->btm;

   p->hp->rcnt++;                       /* increment rcnt for key */
   p->rlev = p->hp->rcnt;
   p->lev = lev;


   /// Optimization : dont need "if" here
   /// the degenerate thread Node case works
   /// save Null, add Node, restore Null to p->nxm
   if (p->par != 0)
   {                                    /* avoid root node */
      /*** next me ***/
      tp1 = p->hp->nxm;                 /* save present */
      p->hp->nxm = p;                   /* connect new to hkdb */
      p->nxm = tp1;                     /* new to old head */
   }
   else
   {                                    /// this is a thread Node (only 1 item on nxme chain)
      p->hp->nxm = p;                   /* connect new to hkdb */
      p->nxm = 0;                       /* new to old head */
   }

   pc = p->chd;
   while (pc != 0)
   {
      /* par accums from child */
      p->ctm  += walk1(pc, lev + 1, fn);
      pc = pc->sib;
   }

   p->hp->rcnt--;                       /* decrement rcnt for key */
   /* current recursive depth */

   // 2 approachs 1) find top 60 cums 2) filter by %total
   top_ctm(p->ctm, p->btm);             /* gather cum histo ?? */

   return(p->ctm);
}

/* ************************************************************ */
void xtree_hdr(FILE * fn)
{
   fprintf(fn, "\n %2s %2s %7s %7s %7s",
           "Lv", "RL",  "Calls", "%Base", "%Cum");
   if (gv.raw >= 1)
   {
      fprintf(fn, " %*s %*s",
              gv.pwid, "Base", gv.pwid, "Cum");
      if (gv.deltree == 1)
      {
         fprintf(fn, " %*s",
                 gv.pwid, "CumA");
      }
   }
   fprintf(fn, " %s\n", "Indent HkKey_HkName");
}

/* ************************************************************ */
void xtree_line(FILE * fn, struct TREE * p, STRING ind)
{

   if (gv.deltree == 1)
   {
      if (p->ctma == 0) return;
   }

   fprintf(fn, " %2d %2d %7"_P64"d %7.2f %7.2f",
           p->lev, p->rlev, p->calls,
           100 * p->btm / gv.tottime, 100 * p->ctm / gv.tottime);

   if (gv.raw >= 1)
   {
      fprintf(fn, " %*.0f %*.0f",
              gv.pwid, p->btm / gv.raw, gv.pwid, p->ctm / gv.raw);

      if (gv.deltree == 1)
      {
         fprintf(fn, " %*.0f ",
                 gv.pwid, p->ctma / gv.raw);
      }
   }

   /* parx doesnt have dptr or ind in gv struct */
#ifndef PARX
   if (p->pass >= 1)
   {
      fprintf(fn, " %d", p->pass);
   }

   if (gv.ind == 1)
   {
      fprintf(fn, " %s", ind);
   }
#endif

   fprintf(fn, " %s\n", p->hp->hstr2);
   /* clock time ???
   if(gv.raw >= 1) {
      fprintf(fn, " %*.0f\n",
    gv.pwid, p->stt);
   }
    */
}

/* ************************************************************ */
/* sort wrapper */
void childsort(struct TREE * p)
{
   struct TREE * pc;                    /* active child ptr */
   static struct TREE * pp[64 * 1024];
   int i = 0, num = 0;

   pc = p->chd;

   /* copy to array & count */
   while (pc != NULL)
   {
      pp[i] = pc;
      i++;
      pc = pc->sib;
   }
   num = i;

   if (num >= 2)
   {
      qsort((void *)pp, num, sizeof(struct TREE *), qcmp_chdcmp);

      p->chd = pp[0];

      for (i = 0; i < num - 1; i++)
      {
         pp[i]->sib = pp[i + 1];
      }
      pp[num - 1]->sib = NULL;
   }
}

/* ************************************************************ */
/*  1) xtree & xtree.p 2) calc hk level, calls, btm, ctm */
/* ************************************************************ */
void walk2(struct TREE * p, int lev, FILE * fn)
{
   struct TREE * pc;                    /* active child ptr */
   STRING istr;

   istr = get_ind(lev);

   //if(lev >= 1) {
   // nothing here. change to if(lev ==0)
   //}
   //else {

   if (lev == 0)
   {
      xtree_hdr(fn);
      if (p->ctm >= gv.pcum)
      {
         xtree_hdr(xtreep);
      }
   }

   // add crttm to each line of xtree
   if (gv.db == 1) fprintf(fn, " %10f", p->crttm);

   xtree_line(fn, p, istr);
   if (p->ctm >= gv.pcum)
   {
      xtree_line(xtreep, p, istr);
   }

   /* accum calls, base, cum, cum2, wtm to hp */
   p->hp->calls += p->calls;
   p->hp->btm   += p->btm;
   p->hp->wtm   += p->wtm;

   /* avoid recursive overcounting */
   if (p->rlev == 1)
   {
      p->hp->ctm  += p->ctm;
   }
   if (p->rlev >= 2)
   {
      p->hp->ctm2  += p->ctm;
   }

   /* sort option here */
   if (gv.tsort == 1)
   {
      childsort(p);
   }

   pc = p->chd;
   while (pc != 0)
   {
      walk2(pc, lev + 1, fn);
      pc = pc->sib;
   }
}

/* ************************************************************ */
/*  profile a hook  */
/* ************************************************************ */
void walk3(struct TREE * p, struct HKDB * hpar)
{
   struct TREE * pc;                    /* active child ptr */
   struct HKDB * hchd;                  /* child  hkdb ptr */

   /* entering a node */
   hchd = p->hp;                        /* childs hkdb ptr */

   if (hpar == hchd)
   {                                    /* entering a parent node */
      hpar->nrcalls++;
   }
   else
   {
      if (hpar->nrcalls >= 1)
      {
         hchd->nrcalls++;
         hchd->tcalls += p->calls;
         hchd->tbtm += p->btm;
         if (hchd->nrcalls == 1)
         {
            hchd->tctm += p->ctm;
         }
      }
   }

   /* recusive walk */
   pc = p->chd;
   while (pc != 0)
   {
      walk3(pc, hpar);
      pc = pc->sib;
   }

   /* returning from a node */
   if (hchd->nrcalls >= 1) hchd->nrcalls--;
}

/* ************************************************************ */
/*  anti profile a hook  */
/* ************************************************************ */
void walk4(struct TREE * p, struct HKDB * hptarg)
{
   struct TREE * pc;                    /* active tree node ptr */
   struct HKDB * hpcurr;                /* curr hkdb ptr */
   struct HKDB * hpt;                   /* tmp hkdb ptr */

   /* entering a node */
   hpcurr = p->hp;                      /* hkdb ptr from tree node */

   if (hptarg == hpcurr)
   {                                    /* at a target node */
      /* crawl up stack accuming */
      pc = p->par;

      while (pc != NULL)
      {
         hpt = pc->hp;
         if (hpt != hptarg)
         {
            if (pc->rlev == 1)
            {
               hpt->tcalls += p->calls;
               hpt->tbtm += p->btm;
               if (p->rlev == 1)
               {                        /* starting from non-recursive targ */
                  hpt->tctm += p->ctm;
               }
            }
         }
         pc = pc->par;
      }
   }

   /* recusive walk */
   pc = p->chd;
   while (pc != 0)
   {
      walk4(pc, hptarg);
      pc = pc->sib;
   }

   /* returning from a node */
}

/* ************************************************************ */
/*  create 1 new tree from all original trees  */
/* ************************************************************ */
void walk5(struct TREE * p)
{

   /* tricky dual walk
    * existing tree is walked recursively(depth 1st)
    * new tree is constructed from global pts
    *   - gv.ptp->ctp (curr Node of curr PT (btree PT))
    *
    *
    */

   gv.ptp->ctp->calls += p->calls;
   gv.ptp->ctp->btm   += p->btm;

   p = p->chd;                          /* orig tree child */
   while (p != 0)
   {
      /* push new */
      push(p->hp);                      /* new tree push with orig tree hkkey */
      gv.ptp->ctp->calls--;             /* push increments calls */

      /* walk down old */
      walk5(p);                         /* orig tree child/sib */
      p = p->sib;                       /* orig tree sib */
   }

   gv.ptp->ctp = gv.ptp->ctp->par;      /* pop new  tree */
   return;                              /* pop orig tree */
}

/* ************************************************************ */
/*  create 1 new tree from all original trees  */
/*  Starting at given node ??? */
/* ************************************************************ */
void walk6(struct TREE * p)
{

   /* tricky dual walk
    * existing tree is walked recursively(depth 1st)
    * new tree is constructed from global pts
    *   - gv.ptp->ctp (curr Node of curr PT (btree PT))
    *
    *
    */

   gv.ptp->ctp->calls += p->calls;
   gv.ptp->ctp->btm   += p->btm;

   p = p->chd;                          /* orig tree child */
   while (p != 0)
   {
      /* push new */
      push(p->hp);                      /* new tree push with orig tree hkkey */
      gv.ptp->ctp->calls--;             /* push increments calls */

      /* walk down old */
      walk5(p);                         /* orig tree child/sib */
      p = p->sib;                       /* orig tree sib */
   }

   gv.ptp->ctp = gv.ptp->ctp->par;      /* pop new  tree */
   return;                              /* pop orig tree */
}

/* ************************************************************ */
/* calc: 1) ctma */
/* ************************************************************ */
UNIT walk1pabs(struct TREE * p, int lev)
{
   struct TREE * pc;

   p->ctma = p->btm;
   if (p->ctma < 0) p->ctma = -p->btm;

   pc = p->chd;
   while (pc != 0)
   {
      p->ctma += walk1pabs(pc, lev + 1);
      pc = pc->sib;
   }

   return(p->ctma);
}

/* ************************************************************ */
/* calc: 1) ctm 2) rec_depth for xbtree */
/* ************************************************************ */
UNIT walk1p(struct TREE * p, int lev)
{
   struct TREE * pc;

   p->ctm = p->btm;

   p->hp->rcnt++;                       /* increment rcnt for key */
   p->rlev = p->hp->rcnt;
   p->lev = lev;

   pc = p->chd;
   while (pc != 0)
   {
      p->ctm += walk1p(pc, lev + 1);
      pc = pc->sib;
   }

   p->hp->rcnt--;                       /* decrement rcnt for key */

   return(p->ctm);
}

/* ************************************************************ */
/*  output xbtree  */
/* ************************************************************ */
void walk2p(struct TREE * p, int lev, FILE * fn)
{
   char c;
   STRING istr;

   //istr = get_indent(lev);
   istr = get_ind(lev);

   if (lev == 0)
   {
      xtree_hdr(fn);
      /*fprintf(fn, "\n %2s %2s %6s %9s %9s %s\n",
    "Lv", "RL",  "Calls", "Base", "Cum", "Indent HkKey_HkName");
       */
   }

   c = gv.indent[lev];                  /* save char */
   gv.indent[lev] = CZERO;

   xtree_line(fn, p, istr);

   gv.indent[lev] = c;                  /* restore char */

   p = p->chd;
   while (p != 0)
   {
      walk2p(p, lev + 1, fn);
      p = p->sib;
   }
}

/* ************************************************************ */
void init_arc(void)
{
   int i;

   OptMsg(" Initializing ARCF\n");

   xtree  = xopen("xtree",   "wb");
   xtreep = xopen("xtree.p", "wb");
   xbtree = xopen("xbtree",  "wb");
   xprof  = xopen("xprof",   "wb");
   xprofp = xopen("xprof.p", "wb");
   xarc   = xopen("xarc",    "wb");
   xarcp  = xopen("xarc.p",  "wb");
   xarcn  = xopen("xarcn",   "wb");

   OptMsg(" Default files open\n");

   for ( i = 0; i < 64; i++ )
   {
      strcpy( iistr[i], "----+----+----+----+----+----+----+----+----+----+----+----+" );
      iistr[i][i] = 0;
   }

   OptMsg(" iistr initialized\n");

   if (MPRUNE >= 1)
   {
      hctm = zMalloc( "Create_hctm_array", MPRUNE * sizeof(UNIT) );
   }

   if (gv.flow == 1)
   {
      xflow  = xopen("xflow",   "wb");
   }

   if (gv.cond >= 1)
   {
      xcondu  = xopen("xcondu",   "wb");
   }

   if (plist != NULL)
   {
      xgiven = xopen("xgiven",  "wb");
      xbecof = xopen("xbecof",  "wb");
   }
   OptMsg(" Init completed\n");
}

/* ************************************************************ */
void prof_hdr(FILE * fh)
{
   fprintf(fh, " %7s %7s %7s %5s",
           "Calls", "%Base", "%Cum", "%Cum2");
   if (gv.raw >= 1)
   {
      fprintf(fh, " %*s %*s", gv.pwid, "Base", gv.pwid, "Cum");
   }
   fprintf(fh, "  Ind  Name\n");

   fprintf(fh, "   =====   =====   ===== =====");

   if (gv.raw >= 1)
   {
      fprintf(fh, " %*s %*s", gv.pwid, "=====", gv.pwid, "=====");
   }
   fprintf(fh, " ====  =====\n");
}

/* ************************************************************ */
void prof_line(FILE * fh, struct HKDB * hp)
{
   static char f1[32] = " %7"_P64"d %7.2f %7.2f %5.0f";
   static char f2[32] = " %*.0f %*.0f";
   static char f3[32] = " %4d  %s\n";
   static char f4[32] = " <A HREF=\"#T_%d\"%4d</A>  %s\n";

   fprintf(fh, f1,
           hp->calls,
           100 * hp->btm  / gv.tottime,
           100 * hp->ctm  / gv.tottime,
           100 * hp->ctm2 / gv.tottime);
   if (gv.raw >= 1)
   {
      fprintf(fh, f2,
              gv.pwid, hp->btm / gv.raw, gv.pwid, hp->ctm / gv.raw);
   }

   /* 2nd metric */
   /* raw base & cum */
   if (0 == 1)
   {
      fprintf(fh, f2,
              gv.pwid, hp->btmv / gv.raw, gv.pwid, hp->ctmv / gv.raw);
   }

   if ( gv.html )
   {
      fprintf(fh, " <A HREF=\"xarc.html#T_%d\">%d</A> %s\n",
              hp->pind, hp->pind, hp->hstr2);
   }
   else
   {
      fprintf(fh, f3, hp->pind, hp->hstr2);
   }
}

/*****************************************************/
void out_prof(void)
{                                       /* xprof, xprof.p, xarc, xarc.p via tree */
   struct HKDB * hp;                    /* entry hook ptr */

   int    i;
   UINT64 tcalls;
   UNIT   tbtm;
   int    cnt = 0;
   UNIT   lclip = (UNIT)-1000000000000.0;
   int    ncum, nds;
   double clev = 1.0 / (1024.0 * 1024.0);
   UNIT   btm;

   tbtm   = 0;
   tcalls = 0;
   nhu    = 0;

   // output c-histo (cum histogram ?)
   ncum = 0;
   for (i = 0; i <= 20; i++)
   {
      nds = gv.chist[i].nodes;
      ncum += nds;
      gv.chist[i].nodes = ncum;

      btm = gv.chist[i].btm;
      gv.chist[i].btm = gv.chist[i - 1].btm + btm;
   }
   gv.chist[21].nodes = ncum;

   OptVMsg(" %8s %12s %8s %8s %8s\n", "Clip%",
           "ClipVal", "Nodes", "%Nodes", "%BTM");

   for (i = 0; i <= 20; i++)
   {
      if (i >= 1)
      {
         nds = ncum - gv.chist[i - 1].nodes;
         btm = gv.tottime - gv.chist[i - 1].btm;
      }
      else
      {
         nds = ncum;
         btm = gv.tottime;
      }

      OptVMsg(" %9.5f%% %12.0f %8d %8.2f%% %8.2f%%\n",
              100 * clev, gv.chist[i].ctm, nds,
              100.0 * (double)nds / (double)ncum, 100.0 * btm / gv.tottime);
      clev *= 2;
   }

   /* assemble all sources of hp's
    * 1) hkdb (normal strace/parx hooks)
    * 2) pidtid's
    * 3) bbfile symbols used
    */

   /* unique mnm */
   for (i = 0; i < gv.hkcnt; i++)
   {
      hp = hkdb[i];

      nhu++;
      /* if( (hp->calls >= 1) || (hp->ctm >= 1) ) { */
      /* } */
   }

   /* count pidtids */
   for (i = 0; i < gv.PTcnt; i++)
   {
      hp = PTarr[i]->rtp->hp;

      nhu++;
      /* if(hp->ctm >= 1) {  */
      /* }                   */
   }

   php = (struct HKDB * *)zMalloc( "UsedHooks",    sizeof(struct HKDB *) * nhu);
   pha = (struct HKDB * *)zMalloc( "Parent/Child", sizeof(struct HKDB *) * nhu);

   /* copy used hk ptrs to php */
   for (i = 0; i < gv.hkcnt; i++)
   {
      hp = hkdb[i];

      /* if( (hp->calls >= 1) || (hp->ctm >= 1) ) {  */
      php[cnt] = hp;
      cnt++;
      tcalls += hp->calls;
      tbtm   += hp->btm;
      /* }     */
   }

   /* copy pidtid ptrs to php */
   for (i = 0; i < gv.PTcnt; i++)
   {
      hp = PTarr[i]->rtp->hp;
      /* if(hp->ctm >= 1) {  */
      php[cnt] = hp;
      cnt++;
      tcalls += hp->calls;
      tbtm   += hp->btm;
      /* }     */
   }

   /* pass tcalls to prof_hdr for variable calls field */
   if ( gv.html )
   {
      fprintf(xprof,  "<pre>");
      fprintf(xprofp, "<pre>");
      fprintf(xarc,   "<pre>");
      fprintf(xarcp,  "<pre>");
   }

   // total : tbtm
   // resolve clipping levels
   gv.pclip = gv.ptclip * tbtm / 100.0;
   gv.cclip = gv.ctclip * tbtm / 100.0;

   units(xprof);
   prof_hdr(xprof);                     /* xprof  header */
   units(xprofp);
   prof_hdr(xprofp);                    /* xprofp header */

   arc_hdr(xarc);                       /* xarc   header */
   arc_hdr(xarcp);                      /* xarcp  header */

   /* sort primary: ctm, secondary btm */
   /* tertiary on calls ???? */
   qsort((void *)php, nhu, sizeof(struct HKDB *), qcmp_hkctm);

   for (i = 0; i < nhu; i++)
   {
      hp = php[i];
      hp->pind = i;
   }

   for (i = 0; i < nhu; i++)
   {
      hp = php[i];

      prof_line(xprof, hp);
      if (i < gv.pruneSize) prof_line(xprofp, hp);

      /* xarc report */
      /* -.5 added to get 0 ctm to output */
      /*** TEMP ****/
      /* allow 0 and negative to work */
      if (hp->ctm >= lclip)
      {
         arc_parent(hp, xarc, lclip);
         arc_self  (hp, xarc, lclip);
         arc_child (hp, xarc, lclip);
      }

      /* xarc.p report */
      if ( (hp->ctm - gv.pcum) > .125)
      {
         arc_parent(hp, xarcp, gv.pcum);
         arc_self  (hp, xarcp, gv.pcum);
         arc_child (hp, xarcp, gv.pcum);
      }
   }

   // multiple selfs OR confounding Nodes here ?

   fprintf(xprofp, " %7s\n", "=====");
   fprintf(xprofp, " %7"_P64"d\n", tcalls);
   fprintf(xprof, " %7s\n", "=====");
   fprintf(xprof, " %7"_P64"d\n", tcalls);

   // WARNING. qsort below disrupts xarc sort order

   /* sort primary: btm, secondary ctm */
   qsort((void *)php, nhu, sizeof(struct HKDB  *), qcmp_hkbtm);

   units(xprofp);
   fprintf(xprofp, "\n Sorted by Base (Top %d)\n", gv.pruneSize);
   prof_hdr(xprofp);                    /* xprof  header */

   /* xprofp(top n by btm) */
   for (i = 0; (i < nhu) && (i < gv.pruneSize); i++)
   {
      prof_line(xprofp, php[i]);
   }

   if ( gv.html )
   {
      fprintf(xprof,  "</pre>");
      fprintf(xprofp, "</pre>");
      fprintf(xarc,   "</pre>");
      fprintf(xarcp,  "</pre>");
   }

   /* restore to sort by ctm for plist */
   qsort((void *)php, nhu, sizeof(struct HKDB *), qcmp_hkctm);
}

/* ************************************************************ */
/** opc:  0(call:>), 1(rf:<), 2(call/rf), 3(pt), 4(pop to:@), 5(event:=)
 ** hp:   struct HKDB * hp
 **          arcf: entry/exit combined in hkdb
 **/
void arc_out(int opc, struct HKDB * hp)
{
   int lev = 0;
   struct TREE * cp;                    /* curr node for pt */
   struct TREE * ocp;                   /* orig curr node for pt */
   int cpu;

   dbgmsg(">> arc_out(%d, %p)\n", opc, hp);

   ocp = gv.ptp->ctp;

   /* arcflow tree management */
   /* get level from tree node */
   switch (opc)
   {
   case 0:                              /* call */
      push(hp);
      cp = gv.ptp->ctp;                 /* pre node */
      lev = cp->lev;                    /* pre lev  */
      break;

   case 1:                              /* return_from */
      cp = gv.ptp->ctp;                 /* post node */
      lev = cp->lev;                    /* post lev  */
      pop(hp, 0);
      break;

   case 2:                              /* impulse(call/return_from) */
      push(hp);
      cp = gv.ptp->ctp;                 /* post node */
      lev = cp->lev;                    /* post lev  */
      pop(hp, 0);
      break;

   case 3:                              /* pidtid */
      /* reset curr to lev 0 */
      break;

   case 4:                              /* return_to */
      /* lev ???? */
      pop(hp, 1);
      cp = gv.ptp->ctp;                 /* post node */
      lev = cp->lev;                    /* post lev  */
      break;

   case 5:                              /* event(=) time, not structure */
      break;

   case 6:                              /* event(-) pop to top */
      gv.ptp->ctp = gv.ptp->rtp;
      break;

   case 7:                              /* time @ curr */
      cp  = ocp;
      lev = ocp->lev;
      hp  = ocp->hp;
      break;

   default:                             /* error */
      break;
   }

   /* xflow */
   if (gv.flow == 1)
   {
      char * istr;
      char cdir[8] = {'>', '<', '?', ' ', '@', '-', ' ', '@'};

      switch (opc)
      {
      case 0:                           /* call */
      case 1:                           /* return_from */
      case 2:                           /* impulse(call/return_from) */
      case 4:                           /* return_to) */
      case 7:                           /* @ curr */
         istr = get_indent(lev);
         if (gv.group_disp) {
            cpu = gv.curcpu;
            if (gv.allrecs) {
               dbgmsg(" *%d* buffering CPU%d:  %10.0f %s %c %s %s\n", opc, cpu, gv.dtime, istr, cdir[opc], hp->hstr2, gv.hdstr);
               fprintf(xflow, " *%d* buffering CPU%d:  %10.0f %s %c %s %s\n", opc, cpu, gv.dtime, istr, cdir[opc], hp->hstr2, gv.hdstr);
            }
            fprintf(gv.ftempcpu[cpu], " %10.0f %s %c %s %s\n", gv.dtime, istr, cdir[opc], hp->hstr2, gv.hdstr);
            gv.ftemp_cnt[cpu]++;
         }
         else {
            fprintf(xflow, " %10.0f %s %c %s %s\n", gv.dtime, istr, cdir[opc], hp->hstr2, gv.hdstr);
         }
         break;

      case 3:                           /* pidtid */
         cpu = gv.curcpu;
         if (gv.group_disp) {
            if (gv.ftemp_cnt[cpu]) {
               if (gv.allrecs) {
                  dbgmsg(" *3*  ignoring CPU%d:  %10.0f  pidtid %s\n", cpu, gv.dtime, gv.ptp->apt);
                  fprintf(xflow, " *3*  ignoring CPU%d:  %10.0f  pidtid %s\n", cpu, gv.dtime, gv.ptp->apt);
               }
            }
            else {
               if (gv.allrecs) {
                  dbgmsg(" *3* buffering CPU%d:  %10.0f  pidtid %s\n", cpu, gv.dtime, gv.ptp->apt);
                  fprintf(xflow, " *3* buffering CPU%d:  %10.0f  pidtid %s\n", cpu, gv.dtime, gv.ptp->apt);
               }

               if (gv.append_cpu_pidtid)
                  fprintf(gv.ftempcpu[cpu], "\n %10.0f  pidtid %s%d\n", gv.dtime, gv.ptp->apt, cpu);
               else
                  fprintf(gv.ftempcpu[cpu], "\n %10.0f  pidtid %s\n", gv.dtime, gv.ptp->apt);

               gv.ftemp_cnt[cpu]++;
            }
         }
         else {
            if (gv.append_cpu_pidtid)
               fprintf(xflow, "\n %10.0f  pidtid %s%d\n", gv.dtime, gv.ptp->apt, cpu);
            else
               fprintf(xflow, "\n %10.0f  pidtid %s\n", gv.dtime, gv.ptp->apt);
         }
         break;

      case 6:                           /* @ pidtid */
         cpu = gv.curcpu;
         if (gv.group_disp) {
            if (gv.ftemp_cnt[cpu]) {
               if (gv.allrecs) {
                  dbgmsg(" *6*  ignoring CPU%d:  %10.0f @ pidtid %s\n", cpu, gv.dtime, gv.ptp->apt);
                  fprintf(xflow, " *6*  ignoring CPU%d:  %10.0f @ pidtid %s\n", cpu, gv.dtime, gv.ptp->apt);
               }
            }
            else {
               if (gv.allrecs) {
                  dbgmsg(" *6* buffering CPU%d:  %10.0f @ pidtid %s\n", cpu, gv.dtime, gv.ptp->apt);
                  fprintf(xflow, " *6* buffering CPU%d:  %10.0f @ pidtid %s\n", cpu, gv.dtime, gv.ptp->apt);
               }

               if (gv.append_cpu_pidtid)
                  fprintf(gv.ftempcpu[cpu], " %10.0f @ pidtid %s%d\n", gv.dtime, gv.ptp->apt, cpu);
               else
                  fprintf(gv.ftempcpu[cpu], " %10.0f @ pidtid %s\n", gv.dtime, gv.ptp->apt);
               gv.ftemp_cnt[cpu]++;
            }
         }
         else {
            if (gv.append_cpu_pidtid)
               fprintf(xflow, "\n %10.0f  pidtid %s%d\n", gv.dtime, gv.ptp->apt, cpu);
            else
               fprintf(xflow, "\n %10.0f  pidtid %s\n", gv.dtime, gv.ptp->apt);
         }
         break;

      default:                          /* error */
         break;
      }
   }

   /* xcondu */
   if (gv.cond >= 1)
   {
      static int off = 1;

      switch (opc)
      {
      case 0:                           /* call */
      case 1:                           /* return_from */
      case 2:                           /* impulse(call/return_from) */
         /* setup in push/pop */
         /* output in pop */
         break;

      case 3:                           /* pidtid */
         fprintf(xcondu, "%8d %10.0f\n",
                 4 * gv.hkccnt - 2, gv.dtime);   /* pt space */
         fprintf(xcondu, "%8d %10.0f %10s %8s %8s %s %s\n",
                 4 * gv.hkccnt - 1, gv.dtime, "", "", "", "", gv.ptp->apt);
         off = off | 2;
         break;

      default:                          /* error */
         break;
      }
   }

   dbgmsg("<< arc_out(%d, %p)\n", opc, hp);
   return;
}

/************************************************************* */
int main(int argc, STRING *argv)
{
   int i;

   gc.msg = xopen("xlog","wb");

   for ( i = 1; i < argc; i++ )
   {
      if ( 0 == strcmp( argv[i], "-verbose" ) )
      {
         gv.verbose = 1;                // Check this early to get all optional messages
         gc.verbose = gc_verbose_maximum;   // Optional messages to stderr and logmsg

         break;
      }
   }
   init_global();                       /* Initialization */

   hdr(stdout);                         /* Report version */

   Scan_Args(argc, argv);               /* */

   init_arc();                          /* arcflow Initialization */

   /// Process Input Based on Options
   if (nrmfile != NULL)
   {
      /* ascii formats */
      OptVMsg( "File type = %d\n", gv.ftype );

      /* ctrace */
      if (gv.ftype == 100) proc_ctrace(nrmfile);

      /* itrace directly to arcf */
      if (gv.ftype == 101) proc_itrace(nrmfile);

      /* javaos-strace-binary */
      if (gv.ftype >= 1)
      {
         proc_inp(nrmfile, memflag, g_lopt, g_ostr);
      }

      /* javaos-strace-binary */
      if (gv.ftype == 0) proc_jraw(nrmfile);

      /* dekko-strace-binary */
      if (gv.ftype == -1) proc_nrm(nrmfile, g_oopt);

      if (gv.Units[0] != '\0')
      {
         OptVMsg(" Units : %s\n", gv.Units);
      }

      // not used
#ifdef _AIX32
      /* aix-trace-binary. */
      /* Need new aix trace for both 32 & 64 bits */
      if (gv.ftype == -2)
      {
         proc_aixbin(nrmfile, gv.hkstp, gv.db != 0);
         if (gv.Units == '\0') gv.Units = "NanoSeconds";
      }
#endif
   }

   /// Reports
   out_pt();                            /* arcflow here */
   out_prof();
   out_plist();

   OptVMsg("\n hkccnt %8d\n", gv.hkccnt);

   OptVMsg("\nTree Node Size:  %8d\n", sizeof(struct TREE));
   OptVMsg("Number of Nodes: %8d\n", gv.trcnt );
   OptVMsg("Total Tree Size: %8d MB\n",
           ( sizeof(struct TREE) * gv.trcnt + ( 1024 * 1024 ) - 1 )
           / ( 1024 * 1024 ) );

   hashStats( &gv.jstack_table );
   hashStats( &gv.method_table );

   logAllocStats();

   return(0);
}
