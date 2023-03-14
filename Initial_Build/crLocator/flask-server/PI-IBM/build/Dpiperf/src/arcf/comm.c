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

void walk6(struct TREE * p);
struct HKDB * str2hptr(STRING s);
void cond_out(struct TREE * t);

/* COMMON DEFINE GLOBAL STUFF */
FILE * nrm;                             /* nrm file */
FILE * arc;                             /* output for awk arcflow */
FILE * sum;                             /* summary output */
FILE * plist;

STRING nrmfile = {""};
STRING sumfile = {""};

pPIDTID  PTarr[MAX_PTARR] = { NULL};

struct Global  gv = {0};                /* clean up in init_global */
/*struct Dekhdr  dh = {0};*/            /* single hook data   */

struct HKDB *  hkdb[HKDBSZ]   = {0};    /* unique hooks       */

// malloc to allow more entries
//UNIT hctm[MPRUNE] = {0};

/* delete this show stuff ?? */
void show_hctm(void);

/* file global hack for bigx */
struct HKDB * hppbig = NULL;
int bigx = 0;

/* ************************************************************ */
/* Return pointer to pidtid struct */
pPIDTID ptptr(STRING apt)
{
   int i, j;
   pPIDTID p;
   int nx = 0;
   struct TREE * tp;

   /* PIDTID array sorted by String value */

   /* linear search. insert sort new if not found */
   /* *** change to BINARY search. May have 1000's of threads now */

   if (gv.PTcnt > 0)
   {

      for (i = 0; i < gv.PTcnt; i++)
      {
         p = PTarr[i];

         /* rtarcf fixup. late thread name resolution in generic */
         /* the "%08x_" case */
         /* not for mtreeN or mtreeO ??? */
         if ( (gv.ftype != 21) && (gv.ftype != 24) )
         {

            if (strcmp( (p->apt) + 9, "unknown_thread") == 0)
            {
               if (strncmp(apt, p->apt, 9) == 0)
               {                        /* tid match */
                  if (strcmp(apt, p->apt) != 0)
                  {                     /* !equal */
                     /* found replacement for unkown thread */
                     /*
                     OptVMsg(" tid match on unknown thread <%s>\n",
                      apt);
                      */
                     p->apt = Remember(apt);   /* replace entry */
                     tp = p->rtp;
                     tp->hp->hstr = zMalloc(" Root Node(pt) Name", 256);
                     sprintf(tp->hp->hstr, "%s_pidtid", apt);
                     tp->hp->hstr2 = tp->hp->hstr;
                     /*
                     OptVMsg(" Changed tnm to: <%s>\n", tp->hp->hstr2);
                      */
                     return(p);
                  }
               }
            }

            /* rtarcf fixup. late thread name resolution in generic */
            /* "%04x_" case */
            if (strcmp( (p->apt) + 5, "unknown_thread") == 0)
            {
               if (strncmp(apt, p->apt, 5) == 0)
               {                        /* tid match */
                  if (strcmp(apt, p->apt) != 0)
                  {                     /* !equal */
                     /* found replacement for unkown thread */
                     /*
                     OptVMsg(" tid match on unknown thread <%s>\n",
                      apt);
                      */
                     p->apt = Remember(apt);   /* replace entry */
                     tp = p->rtp;
                     tp->hp->hstr = zMalloc(" Root Node(pt) Name",256);
                     sprintf(tp->hp->hstr, "%s_pidtid", apt);
                     tp->hp->hstr2 = tp->hp->hstr;
                     /*
                     OptVMsg(" Changed tnm to: <%s>\n", tp->hp->hstr2);
                      */
                     return(p);
                  }
               }
            }
         }

         if (strcmp(apt, p->apt) >= 0)
         {
            if (strcmp(apt, p->apt) == 0)
            {                           /* found */
               return(p);
            }
            else
            {                           /* apt > p->apt */
               /* move & insert new j-th entry */
               nx = i;
               for (j = gv.PTcnt; j > i; j--)
               {
                  PTarr[j] = PTarr[j - 1];
               }
               break;
            }
         }
      }
      if (i == gv.PTcnt) nx = i;
   }

   /* ADDING new pidtid entry */

   /* inline const_pt code */
   p = (pPIDTID) zMalloc("PidTid entry", sizeof(PIDTID));

   PTarr[nx] = p;                       /* array of ptrs to pidtid struct's */

   /* init pidtid entry */
   p->apt = Remember(apt);              /* add apt to pidtid struct */

   p->atime    = 0;                     /* init acc time  */
   p->stttime  = gv.dtime;              /* init stt time  */
   p->stptime  = gv.dtime;              /* init stp/last time  */

   gv.PTcnt++;                          /* number of pidtids */
   if ( gv.PTcnt > MAX_PTARR )
   {
      fprintf( stderr, "PTarr overflow in ptptr\n" );
      exit(1);
   }
   gv.ptp = p;                          /* current pidtid struct */

   /* TREE */
   tp = const_tree();                   /* new pidtid. root node */

   p->ctp = tp;                         /* curr node for this pidtid */
   p->rtp = tp;                         /* root node for this pidtid */

   tp->calls = 1;                       /* always 1 for root */
   tp->lev = 0;                         /* level 0 for pidtid */
   tp->par = 0;                         /* no parent */

   /* make a hkdb record with hstr in it */
   /* makes tree node handling consistant for pidtid & hk based nodes */

   tp->hp = const_hkdb(0);
   tp->hp->mat = 1;

   tp->hp->hstr = zMalloc(" Root Node(pt) Name",256);
   sprintf(tp->hp->hstr, "%s_pidtid", apt);
   tp->hp->hstr2 = tp->hp->hstr;
   tp->hp->hstr3 = tp->hp->hstr;

   pt_init(p);                          /* parx/arcf uniqueness */

   return(p);
}

/*****************************************************/
void units(FILE * fn)
{
   if (ga_err == 1)
   {
      fprintf(fn, "\n   **********************************************\n");
      fprintf(fn, "%s\n", "   WARNING INVALID DATA");
      fprintf(fn, "%s\n",
              "     ENTRY/EXIT ERRORS. SEE STANDARD ERROR");
      fprintf(fn, "   **********************************************\n");
   }

   fprintf(fn, "\n");
   fprintf(fn, " Units :: %s\n", gv.Units);

   if (gv.raw <= 1)
   {
      fprintf(fn, " Total :: %.0f\n", gv.tottime);
   }
   else
   {                                    /* scaled */
      fprintf(fn, " Scaled Total :: %.0f\n", gv.tottime / gv.raw);
      fprintf(fn, "    Raw Total :: %.0f\n", gv.tottime);
   }
   fprintf(fn, "\n");
}

/*****************************************************/
/* read plist file. profile parents */
/*****************************************************/
void out_plist(void)
{
   char buf[256];
   char * bptr;
   int i;
   struct HKDB * hpp;                   /* parent hkdb ptr */
   pPIDTID ppt;                         /* pittid Array ptr */

   /*
      1) walk3 accuming to hkdb
      2) copy to pha
      3) sort
      4) output
      5) clear hkdb slots for next pass
    */

   if (plist != NULL)
   {
      units(xgiven);
      units(xbecof);

      while (fgets(buf, 256, plist) != NULL)
      {
         /* p/P - Profile 1/All Pidtid(s)
          * h   - Profile/Reverse Profile 1 Hook
          * H   - Profile All Hooks
          * B   - Reverse profile All Hooks
          */

         /* tokenize 2nd parm */
         bptr = buf;
         while (bptr++)
         {
            if ( (*bptr == '\r') || (*bptr == '\n') ) break;
         }
         *bptr = CZERO;

         switch (buf[0])
         {
         case 't':
            break;

         case 'p':                      /* Prof 1 Pidtid */
            gv.ptp = ptptr(&buf[2]);    /* pt ptr */
            hpp = gv.ptp->rtp->hp;
            profile(hpp, 0);            /* profile 1 pidtid */
            break;

         case 'P':                      /* Prof All Pidtids */
            for (i = 0; i < gv.PTcnt; i++)
            {
               ppt = PTarr[i];
               hpp = ppt->rtp->hp;
               profile(hpp, 0);
            }
            break;

         case 'h':                      /* Prof & Rprof 1 Hook */
            /* find the hptr for this ascii definition */
            hpp = str2hptr(buf + 2);
            profile(hpp, 0);            /* profile hook, scan all pidtids */
            profile(hpp, 1);            /* anti-profile 1 hook*/
            break;

         case 'x':                      /* bigx */
            /* find the hptr for this ascii definition */
            hppbig = str2hptr(buf + 2);
            bigx = 1;
            OptVMsg(" bigx = %d\n", bigx);
            OptVMsg(" bigx target <%s>\n", hppbig->hstr);
            break;

         case 'H':                      /* All Hooks */
            for (i = 0; i < nhu; i++)
            {
               hpp = php[i];            /* php is profile sorted by ctm */
               if (hpp == NULL)
               {
                  OptVMsg(" H. Null hpp. i = %d\n", i);
                  panic(" Null Ptr in profile() in arc.c");
               }

               if ( (hpp->ctm != 0) || (hpp->calls != 0) )
               {
                  profile(hpp, 0);      /* profile 1 pidtid */
               }
            }
            break;

         case 'B':                      /* Reverse Profile All Hooks */
            for (i = 0; i < nhu; i++)
            {
               hpp = php[i];            /* php is profile sorted by ctm */
               if ( (hpp->ctm != 0) || (hpp->calls != 0) )
               {
                  profile(hpp, 1);      /* anti-profile 1 hook*/
               }
            }
            break;

         default:
            break;
         }
      }
   }
}

/* ************************************** */
void xflush(void)
{
   fflush(xprof); fflush(xprofp); fflush(xarc); fflush(xarcp); fflush(xtree);
   fflush(xtreep); fflush(xgiven); fflush(xbecof); fflush(xflow);
   fflush(xcondu); fflush(xbtree);
}

/* ************************************** */
void show_hctm(void)
{
   int i;

   for (i = 0; i < MPRUNE; i++)
   {
      if (hctm[i] >= 1)
      {
         OptVMsg(" %2d %10.0f\n", i, hctm[i]);
      }
      else
      {
         return;
      }
   }
}

/* ************************************** */
void top_ctm(UNIT ctm, UNIT btm)
{
   int i;
   static int once = 0;

   // c histogram for clipping
   if (once == 0)
   {
      once = 1;
      gv.chist[20].ctm = gv.tottime;

      for (i = 19; i >= 0; i--)
      {
         gv.chist[i].ctm = gv.chist[i + 1].ctm / 2;
      }
   }

   for (i = 1; i <= 20; i++)
   {
      if (ctm < gv.chist[i].ctm)
      {
         gv.chist[i - 1].btm += btm;
         gv.chist[i - 1].nodes++;
         break;
      }
   }

   // CRUDE. insert sort for keeping top 60 nodes by cum
   if ( (ctm >= 1) && (ctm > hctm[MPRUNE - 1]) )
   {
      for (i = MPRUNE - 2; i >= 0; i--)
      {
         if (ctm > hctm[i])
         {
            hctm[i + 1] = hctm[i];
         }
         else
         {
            hctm[i + 1] = ctm;
            return;
         }
      }
      hctm[0] = ctm;
   }
}

/* ************************************** */
/* HKDB CONSTRUCTOR */
/* db: 1 => add to hkdb, 0 =>  return ptr only */
struct HKDB * const_hkdb(int dbase)
{
   struct HKDB * hp;

   if (gv.hkcnt >= HKDBSZ)
   {
      panic("HKDB Over Size Limit");
   }

   hp = (struct HKDB *) zMalloc("HKDB entry",sizeof(struct HKDB));

   if (dbase == 1)
   {                                    /* add to hkdb */
      hkdb[gv.hkcnt] = hp;
      gv.hkcnt++;                       /* entry in hkdb */
      if ( (gv.hkcnt % 1000) == 0)
      {
         OptVMsg("gv.hkcnt %d\n", gv.hkcnt);
      }
   }
   hp->hstr = gv.nstr;

   return(hp);
}

/* ************************************** */
/* TREE CONSTRUCTOR */
struct TREE * const_tree(void)
{
   struct TREE * tp;

   tp = (struct TREE *) zMalloc("TREE entry",sizeof(struct TREE));
   gv.trcnt++;                          /* number of tree entries */
   if ( (gv.trcnt % 1000) == 0)
   {
      OptVMsg("TreeNodes %d\n", gv.trcnt);
   }

   tp->crttm = (UNIT)gv.hkccnt;         /* node creation record number */
   tp->stt   = gv.dtime;                /* Condensed start time */
   tp->hdstr = gv.nstr;                 /* Condensed entry data string */
   tp->ptp   = gv.ptp;                  /* pidtid ptp */
   return(tp);
}

/* ************************************************************ */
int LKCONV qcmp_pt(const void * p1, const void * p2)
{
   pPIDTID r;
   pPIDTID s;

   r = *(pPIDTID *)p1;
   s = *(pPIDTID *)p2;

   // use r->rtp->ctm instead of atime
   r->atime = r->rtp->ctm;
   s->atime = s->rtp->ctm;

   // sorts by atime (time on pidtid). should recompute via tree walk
   if (r->atime >= s->atime)
   {
      /*if((aint)r->atime > (aint)s->atime) {*/
      if (r->atime > (s->atime + .125) )
      {
         return(-1);
      }
      else
      {                                 /* == case : use pt to break ties */
         if (strcmp(r->apt, s->apt) >= 0)
         {
            if (strcmp(r->apt, s->apt) > 0)
            {
               return(-1);
            }
            else
            {
               return(0);
            }
         }
         return(1);
      }
   }
   else
   {
      return(1);
   }
}

/*****************************************************/
void out_pt(void)
{
   int i;
   pPIDTID p;
   pPIDTID pa;
   struct TREE * tp;
   char buf[32];

   /* calc print width */
   if (gv.raw >= 1)
   {
      gv.pwid = sprintf(buf, "%.0f", gv.tottime / gv.raw);
      if (gv.pwid < 7) gv.pwid = 7;
   }

   /* pop each pidtid stack to root */
   for (i = 0; i < gv.PTcnt; i++)
   {
      p = PTarr[i];
      tp = p->ctp;
      while (tp != p->rtp)
      {
         if (gv.cond >= 1) cond_out(tp);
         tp->wtm += gv.dtime - tp->stt;
         tp = tp->par;
      }

      assert(tp == p->rtp);
      tp->wtm += gv.dtime - tp->stt;
   }

   /* xtree_min: prune & recalc total */
   /// -t xtree_min mode Only
   if (gv.xmin == 1)
   {
      gv.tottime = 0;
      for (i = 0; i < gv.PTcnt; i++)
      {
         p = PTarr[i];
         walk1min(p->rtp, 0);           /* xtree_min */
      }
   }

   /* create cumA for xtree */
   /// -t xtree_sub mode Only
   if (gv.deltree == 1)
   {
      for (i = 0; i < gv.PTcnt; i++)
      {
         p = PTarr[i];
         walk1abs(p->rtp, 0, xtree);    /* ctma */
      }
   }

   if ( 0 == gv.tottime )
   {
      gv.tottime = 0.1;                 // Eliminate divide-by-zero errors
   }

   units(xtree);
   units(xtreep);
   units(xbtree);


   /// per Thread loop
   /// cum lev rlev nxme pruning
   for (i = 0; i < gv.PTcnt; i++)
   {
      p = PTarr[i];
      walk1(p->rtp, 0, xtree);          /* ctm rdep nxme */
   }

   // loop thru threads & set up
   if (gv.prune == 0)
   {
      gv.pcum = hctm[MPRUNE - 1];
   }
   else
   {
      gv.pcum = gv.tottime * gv.prune / (UNIT)100.0;
      OptVMsg(" Total Time         %12.0f\n", gv.tottime);
      OptVMsg(" Calc Pruning Level %12.0f\n", gv.pcum);
   }

   /* sort pidtid by 1) acc_time & 2) pidtid */
   // use ctm of lv = 0;
   qsort((void *)PTarr, gv.PTcnt, sizeof(pPIDTID), qcmp_pt);

   /* tree walks after sorting pidtid */

   /// by method : calls, base, cum, cum2
   /// optional child sort
   for (i = 0; i < gv.PTcnt; i++)
   {
      p = PTarr[i];
      if ( (p->rtp->ctm >= 1) || (p->rtp->ctma >= 1) )
      {
         walk2(p->rtp, 0, xtree);       /* ascii tree out */
      }
   }

   if (bigx != 1)
   {
      if (gv.nobtree == 0)
      {
         /* Composite Tree */
         tp = const_tree();             /* new pidtid. root node. calloc -> 0's */

         /* pidtid entry for all pt */
         pa = (pPIDTID) zMalloc("PidTid entry",sizeof(PIDTID));
         PTarr[gv.PTcnt] = pa;          /* hide this pt @ end of PTarr */
         gv.ptp = pa;
         gv.ptp->rtp = tp;              /* root node for this pidtid */

         tp->hp = const_hkdb(0);
         tp->hp->hstr = Remember("bigtree_pidtid");
         tp->hp->hstr2 = tp->hp->hstr;
         tp->ptp = pa;

         /* build big tree nodes */
         for (i = 0; i < gv.PTcnt; i++)
         {
            gv.ptp->ctp = gv.ptp->rtp;  /* start at bigtree root */
            p = PTarr[i];               /* next pidtid tree anchor */
            /*if(p->rtp->ctm >= 1) {*/
            walk5(p->rtp);              /* stt @ root of each pidtid tree */
            /*}*/
         }

         /* big tree walk */
         p = PTarr[gv.PTcnt];           /* tricky: anchor @ end of PTarr */
         walk1pabs(p->rtp, 0);          /* ctm absolute */
         walk1p(p->rtp, 0);             /* ctm rdep */
         walk2p(p->rtp, 0, xbtree);     /* ascii xbtree out */
      }
   }

   else
   {                                    /* use the btree file for now */
      /* Composite Tree (bigx) */
      tp = const_tree();                /* new pidtid. root node. calloc -> 0's */

      /* use -P option & pfile to obtain the hptr for target Node */
      OptVMsg(" bigx = %d\n", bigx);
      OptVMsg(" bigx target <%s>\n", hppbig->hstr);

      if (0 == 0)
      {
         /* pidtid entry for all pt */
         pa = (pPIDTID) zMalloc("PidTid entry",sizeof(PIDTID));
         PTarr[gv.PTcnt] = pa;          /* hide this pt @ end of PTarr */
         gv.ptp = pa;
         gv.ptp->rtp = tp;              /* root node for this pidtid */

         tp->hp = const_hkdb(0);
         tp->hp->hstr = Remember("bigx_pidtid");
         tp->hp->hstr2 = tp->hp->hstr;
         tp->ptp = pa;

         /* build big tree nodes */
         for (i = 0; i < gv.PTcnt; i++)
         {
            gv.ptp->ctp = gv.ptp->rtp;  /* start at bigtree root */
            p = PTarr[i];               /* next pidtid tree anchor */
            if (p->rtp->ctm >= 1)
            {
               walk6(p->rtp);           /* stt @ root of each pidtid tree */
            }
         }

         /* big tree walk */
         p = PTarr[gv.PTcnt];           /* tricky: anchor @ end of PTarr */
         walk1p(p->rtp, 0);             /* ctm rdep */
         walk2p(p->rtp, 0, xbtree);     /* ascii xbtree out */
      }
   }
}
