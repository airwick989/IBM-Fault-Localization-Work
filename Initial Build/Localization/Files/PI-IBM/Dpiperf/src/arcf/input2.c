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
#include "jhash.h"

int     onStack(struct HKDB * hp);
void    acct2l(int tflg);               /* time filter */
void    acct2(void);
void    acct2m(void);                   /* supports minus values */
void    acctMP(pPIDTID ptp);
void    acctMPb(pPIDTID ptp, UNIT tm);
void    acct2ft(void);                  // gv.tottime & gv.atime updated
int     getmm(STRING tok, aint * amj, aint * amn);
double  gettm(STRING tok);
STRING  getpt(STRING tok);
void    read_aixjit(STRING nrmfile);
STRING  fname = NULL;                   /* not right for OS2. need nullstr init */
FILE *  gen   = NULL;                   /* file global */
void xflowOut(char * bufx);

//STJ// The size of harr was never validated, causing overruns.  This fixes it.

struct HKDB **harr = 0;
int    harrsz      = 0;

void CheckHARR(int index)               // Guarantee that harr[index] is valid
{
   int           i,n,size;
   struct HKDB **p;

   size = index+1;

   if ( harrsz < size )                 // harr needs to be reallocated
   {
      if ( 0 == ( n = harrsz ) )
      {
         n = 128;                       // Minimum size of harr
      }
      while ( n < size )
      {
         n += n;                        // Keep doubling size until big enough
      }

      p = (struct HKDB **)xMalloc( "CheckHARR", n * sizeof(struct HKDB *) );

      for ( i = 0; i < harrsz; i++ )
      {
         p[i] = harr[i];                // Copy entries to reallocated array
      }
      for ( i = harrsz; i < n; i++ )
      {
         p[i] = 0;
      }

      if ( harr ) free(harr);           // Discard old array

      harr   = p;
      harrsz = n;
   }
}

UINT64 atoUINT64( char *p )
{
   UINT64 ui64 = 0;

   while ( *p )
   {
      ui64 = 10 * ui64 + (UINT64)( 0x0F & *p++ );
   }
   return( ui64 );
}

/* ************************************************************ */
void tnm_change(char * tnm)
{
   int i;
   char * p;

   for (i = 0; i < gv.tnms; i++)
   {
      p = strstr(tnm, gv.tnmarr[i]);
      if (p != NULL)
      {
         gv.apt = gv.tnmarr[i];
         return;
      }
   }
}

/* ************************************************************ */
void acctx(int sign)
{
   /* first: 0:subt 0:add -1:min */
   /* 2nd:   1:subt 0:add -1:min */
   /* 3+:    1:subt 0:add -1:min */

   UNIT del;

   gv.deltime = del = gv.dtime;

   if (sign == 1) del = -del;

   gv.ptp->atime += del;                /* pidtid time */
   /* pt time for arcflow */

   /* only accum tottime on + side */
   if (sign == 0) gv.tottime += del;

   /* TREE. increment curr node time */
   if (sign >= 0)
   {                                    /* subt | add */
      gv.ptp->ctp->btm  += del;
      gv.ptp->ctp->btmx += del;
   }
   else
   {                                    /* min */
      if (gv.ptp->ctp->btm >= 1)
      {                                 /* it exists from prev */
         if (del < gv.ptp->ctp->btm)
         {
            gv.ptp->ctp->btm = del;
         }
      }
      else
      {                                 /* btm == 0 ?? */
         if (sign == -1)
         {
            gv.ptp->ctp->btm = del;
         }
         else
         {
            gv.ptp->ctp->btm = 0;
         }
      }
   }

   /*
   OptVMsg(" del %.0f tot %0.f\n", gv.dtime, gv.tottime);
    */
}

/* ************************************************************ */
/* *************************** */
char * getpidtid(char * pt)
{
   static char b[512];
   char c;
   char * bp;

   bp = &b[0];
   *bp++ = '_'; *bp++ = 't'; *bp++ = 'h'; *bp++ = 'r';
   *bp++ = 'd'; *bp++= '_';

   while ((c = *pt) != '\n')
   {
      if (c == ' ') c = '_';
      if (c == '"') c = '_';
      *bp = c;
      pt++; bp++;
   }
   *bp = '\0';
   return(b);
}

/* ************************************************************ */
aint hstr2aint(STRING hstr)
{                                       /* hex cycles */
   aint num = 0;
   char c;
   int n = 0;

   while ((c = *hstr) != CZERO)
   {
      /* hex to dec */
      if ((c >= '0') && (c <= '9')) n = c - '0';
      if ((c >= 'a') && (c <= 'f')) n = c - 'a' + 10;
      num = (num << 4) | n;
      hstr++;
   }
   return(num);
}

/* ************************************************************ */
double gettm(STRING tok)
{                                       /* hex cycles */
   double tm = 0;
   char c;
   int n = 0;

   while ((c = *tok) != CZERO)
   {
      /* hex to dec */
      if ((c >= '0') && (c <= '9')) n = c - '0';
      if ((c >= 'a') && (c <= 'f')) n = c - 'a' + 10;
      tm = 16 * tm + n;
      tok++;
   }
   return(tm);
}

/* ************************************************************ */
/* jsformat javaos ascii format */
/* ??? still not right. " are optional on tidid name ??? */
STRING getpt(STRING tok)
{
   STRING s;

   /* tok = tok + 7; */                 /* skip NewTid: */

   if (*tok == '"')
   {
      tok++;
      s = tok;
      while (*tok != '"')
      {
         if (*tok == CZERO) *tok = '_';
         if (*tok == ' ') *tok = '_';
         tok++;
      }
      *tok = CZERO;
      return(s);
   }
   else return("?_PidTid");
}

/* ************************************************************ */
int getmm(STRING tok, aint * amj, aint * amn)
{
   STRING s;
   int cnt = 0;

   tok++;                               /* skip ( */
   s = tok;
   while (*tok != ':')
   {
      tok++;
      cnt++;
      if (cnt >= 4) return(0);
   }
   *tok = CZERO;
   *amj = hstr2aint(s);

   tok++;                               /* skip : */
   s = tok;
   while (*tok != ')')
   {
      tok++;
      cnt++;
      if (cnt >= 16) return(0);
   }
   *tok = CZERO;
   *amn = hstr2aint(s);                 /* aint */
   return(1);
}

/* ************************************************************ */
/** Process AIX Trace Record *****************/
/*  AIXTrace MP binary Format */
/*
   ex 0,1,2,4 mnm: nm => mnm
      0=>call 1=>retnFrom 2=>Impulse 4=>retnTo
   ex 3  pidtid:   nm => tid
   ex 5  finalize active pidtids
 */
/* ************************************************************ */
void proc_AixTrc(int ex, UNIT tm, int cpu, STRING nm)
{
   pPIDTID ptp;
   struct HKDB * hp = NULL;
   static int cpumax = 0;
   static pPIDTID optp = NULL;
   double del;

   /* this may be overkill. only important for MP & dangling threads */
   if (ex == 5)
   {                                    /* finalize active pidtids */
      /* active tids */
      for (cpu = 0; cpu <= cpumax; cpu++)
      {
         /* add tid time here, and include mode = 1 ?? */
         acctMP(gv.aptp[cpu]);
      }
      return;
   }

   gv.hkccnt++;
   del = tm - gv.dtime;
   gv.dtime = tm;
   if (cpu > cpumax) cpumax = cpu;

   /* accounting */
   ptp = gv.aptp[cpu];                  /* curr ptp for this cpu */

   /* account for the time */
   acctMP(ptp);

   if (ex != 3)
   {                                    /* mnm */
      hp = hkdb_find(nm, 1);
      gv.ptp = ptp;
      arc_out(ex, hp);                  /* structure */
   }

   else
   {                                    /* tnm */
      if ( (ptp == NULL) || strcmp(ptp->apt, nm) != 0)
      {
         gv.apt = Remember(nm);
         gv.ptp = ptptr(nm);            /* find/add pidtid */
         gv.aptp[cpu] = gv.ptp;         /* update cpu array (with pt ptr) */
      }
      gv.ptp = gv.aptp[cpu];            /* set curr ptp */

      gv.ptp->stptime = tm;             /* update time in pt struct */

      arc_out(3, hp);                   /* */
      fprintf(gen, " %10.0f pidtid %s\n", del, gv.ptp->apt);
   }
}

/* ************************************************************ */
/* ascii formats */
void proc_inp(STRING fname, int memflag, int lopt, char * ostr)
{
   char   buf[4096];                    /* input data */
   STRING * tokarr = 0;
   int    tcnt     = 0;
   STRING newpt;
   STRING methname;
   STRING className;                    /* mjd */
   FILE   * input  = NULL;
   FILE   * inarr[8] = {NULL};
   int    i;
   int    foff = 0;                     /* adjust for memalloc format */
   int    first = 0;
   struct HKDB * hp = NULL;
   int    ex = 0;
   int    files = 1;
   char * fnm;

   fname = gv.infiles[0];
   input = fopen(fname, "rb");
   if (input == NULL)
   {
      printf(" input.c : can't open %s\n", fname);
      pexit("can't open ARCF input file");
   }
   /*OptVMsg(" Opening < %s > fd %p\n", fname, input);*/

   if (gv.ninfiles >= 2)
   {
      inarr[0] = input;
      for (i = 1; i < gv.ninfiles; i++)
      {
         fnm = gv.infiles[i];
         inarr[i] = fopen(fnm, "rb");
         /*OptVMsg(" Opening < %s > fd %p\n", fnm, inarr[i]);*/

         if (inarr[i] == NULL)
         {
            printf(" input2.c : can't open %s\n", fnm);
            pexit("can't open an ARCF input file");
         }
      }
   }

   /**1*******************************************************/
   /* Read Lines of java -x -tm file */
   if (gv.ftype == 1)
   {

      if (gv.uflag == 0)
      {
         if (memflag == 0) gv.Units = Remember("ByteCodes");   /* jbytecodes */
         if (memflag == 1) gv.Units = Remember("Heap_Calls");   /* jheapcalls */
         if (memflag == 2) gv.Units = Remember("Heap_Bytes");   /* jheapbytes */
         if (memflag == 3) gv.Units = Remember("jra_Heap_Calls");   /* jra_heapcalls */
         if (memflag == 4) gv.Units = Remember("jra_Heap_Bytes");   /* jra_heapbytes */
         if (memflag == 5) gv.Units = Remember("jrf_Heap_Calls");   /* jrf_heapcalls */
         if (memflag == 6) gv.Units = Remember("jrf_Heap_Bytes");   /* jrf_heapbytes */
      }

      printf(" gv.hkstt %d\n", gv.hkstt);
      printf(" gv.hkstp %d\n", gv.hkstp);

      while ((fgets(buf, 2048, input) != NULL) && (gv.hkstp > gv.hkccnt) )
      {
         gv.hkccnt++;
         if (gv.hkccnt >= gv.hkstt)
         {
            tokarr = tokenize(buf, &tcnt);

            if (tcnt >= 10)
            {
               if (tokarr[0][0] == 'X')
               {
                  /* input format below */
                  /* memalloc: -m: add 1 to field */
                  /* 0-8 : X bccnt heapcalls heapbytes [javar[4]] # pt [n] > method */
                  /* 9-10 entry : (n) entered  */
                  /* 9    exit  : returning    */

                  if (first == 0)
                  {
                     first = 1;
                     if (tokarr[4][0] == '#') foff = 2;
                     else if (tokarr[8][0] == '#') foff = 6;
                  }

                  /* byte count or memalloc or javar */
                  gv.dtime = (UNIT)atoi(tokarr[1 + memflag]);

                  acct2();              /* time accounting. delt => current pidtid */

                  if (tokarr[5 + foff][0] == '>') ex = 0;   /* entry */
                  else ex = 1;          /* exit */

                  /* use method name to find hp */
                  methname = tokarr[6 + foff];
                  hp = hkdb_find(methname, 1);

                  newpt = tokarr[3 + foff];   /* pidtid */

                  if (strcmp(gv.apt, newpt) != 0)
                  {                     /* pidtid change */
                     printf(" PT change: %s -> %s\n", gv.apt, newpt);
                     gv.apt = Remember(newpt);
                     gv.ptp = ptptr(newpt);   /* find/add pidtid */
                     arc_out(3, hp);
                  }
                  arc_out(ex, hp);
               }
            }
         }
      }                                 /* while neof */
   }                                    /* java input file */

// /**3*******************************************************/
// /* OLD plumber */
// if (gv.ftype == 30000000)
// {
//    int freq = 0, ddtime = 0;
//    int hcnt = -1;                    /* stack size */
//    int fcnt;
//    STRING atype = NULL;
//    struct HKDB * harr[128];
//
//    gv.Units = Remember("Bytes");
//
//    gv.apt = Remember("MemLeak");
//    gv.ptp = ptptr(gv.apt);           /* find/add pidtid */
//    acct2();                          /* time accounting */
//
//    fcnt = 4;                         /* size of hdr */
//    while (fgets(buf, 2048, input) != NULL)
//    {
//       tokarr = tokenize(buf, &tcnt);
//       if (tcnt == 0) break;
//    }
//
//    while (fgets(buf, 2048, input) != NULL)
//    {
//       gv.hkccnt++;
//       tokarr = tokenize(buf, &tcnt);
//
//       if (tcnt == 4)
//       {                              /* start of hdr */
//          freq   = atoi(tokarr[0]);
//          ddtime = atoi(tokarr[1]);   /* delta bytes */
//          hcnt = 0;
//
//          for (i = 1; i < fcnt; i++)  /* skip hdr */
//             fgets(buf, 2048, input);
//          tokarr = tokenize(buf, &tcnt);
//          atype = Remember(tokarr[0]);
//          hcnt++;
//
//          continue;
//       }
//
//       if (tcnt == 1)
//       {
//          hp = hkdb_find(tokarr[0], 1);   /* method => hp */
//          harr[hcnt] = hp;
//          hcnt++;
//       }
//
//       if (tcnt == 0)
//       {                              /* end of stack */
//          /* cat atype to last method name */
//          hp = harr[hcnt - 1];
//          sprintf(buf, "%s--%s", hp->hstr2, atype);
//          Free(atype);
//          hp = hkdb_find(buf, 1);     /* method => hp */
//          harr[hcnt - 1] = hp;
//
//          for (i = 1; i < hcnt; i++)
//          {
//             hp = harr[i];
//             arc_out(0, hp);          /* updates calls by 1 */
//             gv.ptp->ctp->calls += freq - 1;   /* adj call cnt */
//          }
//
//          /* pop to top */
//          gv.dtime += ddtime;
//          acct2();                    /* time accounting */
//          gv.ptp->ctp = gv.ptp->rtp;
//       }
//    }                                 /* while neof */
//
//    if (hcnt >= 1)
//    {                                 /* finish the last stack */
//       /* cat atype to last method name */
//       hp = harr[hcnt - 1];
//       sprintf(buf, "%s--%s", hp->hstr2, atype);
//       Free(atype);
//       hp = hkdb_find(buf, 1);        /* method => hp */
//       harr[hcnt - 1] = hp;
//
//       for (i = 1; i < hcnt; i++)
//       {
//          hp = harr[i];
//          arc_out(0, hp);             /* updates calls by 1 */
//          gv.ptp->ctp->calls += freq - 1;   /* adj call cnt */
//       }
//
//       /* pop to top */
//       gv.dtime += ddtime;
//       acct2();                       /* time accounting */
//       gv.ptp->ctp = gv.ptp->rtp;
//    }
// }                                    /* memleak input file */

   /**3*******************************************************/
   /* NEW plumber */
   if (gv.ftype == 3)
   {
      int freq = 0, ddtime = 0;
      int hcnt = -1;                    /* stack size */
      int fcnt;

      //STJ//struct HKDB * harr[128];

      gv.Units = Remember("Bytes");

      gv.apt = Remember("MemLeak");
      gv.ptp = ptptr(gv.apt);           /* find/add pidtid */
      acct2();                          /* time accounting */

      fcnt = 3;                         /* size of hdr */
      /* spin thru first blank line */
      while (fgets(buf, 512, input) != NULL)
      {
         tokarr = tokenize(buf, &tcnt);
         if (tcnt == 0) break;
      }

      while (fgets(buf, 2048, input) != NULL)
      {
         tokarr = tokenize(buf, &tcnt);

         if (tcnt == 4)
         {                              /* hdr */
            freq   = atoi(tokarr[0]);
            ddtime = atoi(tokarr[1]);   /* delta bytes */
            hcnt = 0;

            if (fgets(buf, 2048, input));  /* Ignore return value */
            if (fgets(buf, 2048, input));  /* Ignore return value */
            continue;
         }
         gv.hkccnt++;

         if (tcnt == 1)
         {
            hp = hkdb_find(tokarr[0], 1);   /* method => hp */
            CheckHARR(hcnt);

            harr[hcnt] = hp;
            hcnt++;
         }

         if (tcnt == 0)
         {                              /* end of stack */
            if (gv.ptype == 1)
            {
               for (i = 0; i < hcnt; i++)
               {
                  hp = harr[i];
                  arc_out(0, hp);       /* updates calls by 1 */
                  gv.ptp->ctp->calls += freq - 1;   /* adj call cnt */
               }
            }
            else
            {
               for (i = hcnt - 1; i >= 0; i--)
               {
                  hp = harr[i];
                  arc_out(0, hp);       /* updates calls by 1 */
                  gv.ptp->ctp->calls += freq - 1;   /* adj call cnt */
               }
            }

            /* pop to top */
            gv.dtime += ddtime;
            acct2();                    /* time accounting */
            gv.ptp->ctp = gv.ptp->rtp;
         }
      }                                 /* while neof */

      if (hcnt >= 1)
      {                                 /* finish the last stack */
         if (gv.ptype == 1)
         {
            for (i = 0; i < hcnt; i++)
            {
               hp = harr[i];
               arc_out(0, hp);          /* updates calls by 1 */
               gv.ptp->ctp->calls += freq - 1;   /* adj call cnt */
            }
         }
         else
         {
            for (i = hcnt - 1; i >= 0; i--)
            {
               hp = harr[i];
               arc_out(0, hp);          /* updates calls by 1 */
               gv.ptp->ctp->calls += freq - 1;   /* adj call cnt */
            }
         }

         /* pop to top */
         gv.dtime += ddtime;
         acct2();                       /* time accounting */
         gv.ptp->ctp = gv.ptp->rtp;
      }
   }                                    /* memleak input file */

   /**4*******************************************************/
   /* stprof - DEPRECATED */
   if (gv.ftype == 4)
   {
      int freq, ddtime = 0;
      struct HKDB * hp;
      STRING apt;

      gv.Units = Remember("TICKS");

      while (fgets(buf, 2048, input) != NULL)
      {
         gv.hkccnt++;
         tokarr = tokenize(buf, &tcnt);

         if (tcnt == 2)
         {                              /* start of hdr */
            if (strcasecmp(tokarr[0], "TOP") == 0)
            {
               freq   = 1;              /* 1 stack at a time for now */
               ddtime = 1;              /* 1 ticks at a time for now */

               apt = tokarr[1];         /* pidtid */
               if (strcmp(gv.apt, apt) != 0)
               {                        /* pidtid change */
                  gv.apt = Remember(tokarr[1]);
                  gv.ptp = ptptr(tokarr[1]);   /* find/add pidtid */
               }
            }
            else
            {
               OptMsg(" Failed TOP test\n");
               exit(-1);
            }
         }

         if (tcnt == 1)
         {
            hp = hkdb_find(tokarr[0], 1);   /* method => hp */
            arc_out(0, hp);             /* build tree, calls++ */
         }

         if (tcnt == 0)
         {                              /* end of stack */
            gv.dtime += ddtime;
            acct2();                    /* time accounting */
            gv.ptp->ctp = gv.ptp->rtp;
         }
      }                                 /* while neof */

      /* last elem of last stack */
      gv.dtime += ddtime;
      acct2();                          /* time accounting */
      gv.ptp->ctp = gv.ptp->rtp;

   }                                    /* tprof stacks input file */

   /**5*******************************************************/
   /* jctree : Java HeapDump Class Tree */
   /*   object tree => class tree */
   if (gv.ftype == 5)
   {
      int i;
      int levx, olevx;
      int ddtime = 0;
      struct HKDB * hp = NULL;
      //STJ//struct HKDB * harr[HARRSZ];
      STRING cnm;

      gv.Units = Remember("Bytes");

      gv.apt = Remember("JavaHDClassTree");
      gv.ptp = ptptr(gv.apt);           /* find/add pidtid */
      arc_out(3, hp);

      gv.dtime = 0;

      olevx = 0;
      while (fgets(buf, 2048, input) != NULL)
      {
         gv.hkccnt++;
         tokarr = tokenize(buf, &tcnt);

         if ( (gv.hkccnt >= 5) && (tcnt >= 5) )
         {                              /* 5 or 6 tokens */
            gv.hkccnt++;

            cnm    = tokarr[tcnt - 1];  /* class name (last field) */
            ddtime = atoi(tokarr[3]);   /* units of bytes */
            levx   = atoi(tokarr[0]);

            CheckHARR(levx);

            acct2();                    /* time accounting */
            gv.dtime += ddtime;

            hp = hkdb_find(cnm, 1);     /* method => hp */

            if (olevx >= levx)
            {
               for (i = olevx; i >= levx; i--)
               {
                  arc_out(1, harr[i]);  /* EXIT */
               }
            }

            harr[levx] = hp;
            arc_out(0, hp);             /* ENTRY */

            olevx = levx;
         }
         acct2();                       /* last time accounting */

      }                                 /* while neof */

      OptMsg(" DONE\n");

   }                                    /* java heap tree */

   /**55*******************************************************/
   /* rewriting Class Tree 071698. See above */
   /* jctree : Java HeapDump Class Tree */
   if (gv.ftype == 55)
   {
      int i;
      int freq;
      int levx, olevx;
      int ddtime = 0;
      struct HKDB * hp = NULL;
      //STJ//struct HKDB * harr[128];
      STRING cnm;
      STRING istr;

      gv.Units = Remember("Bytes");
      gv.apt   = Remember("JavaClassTree");
      gv.ptp   = ptptr(gv.apt);         /* find/add pidtid */
      gv.ptp->ctp = gv.ptp->rtp;
      /* harr[0] = gv.ptp->hp; */
      olevx = 1;
      acct2m();                         /* time accounting */

      while (fgets(buf, 2048, input) != NULL)
      {
         gv.hkccnt++;
         tokarr = tokenize(buf, &tcnt);

         /* may add pidtid records <pidtid pppptttt> */
         if (tcnt == 4)
         {                              /* All Lines should have 4 tokens */
            istr   = tokarr[0];
            cnm    = tokarr[1];         /* class name */
            ddtime = atoi(tokarr[2]);   /* units of bytes */

            levx  = (int)strlen(istr);
            CheckHARR(levx);

            freq = 1;                   /* class tree with no collapsing */

            /* time accounting before tree walking */
            acct2m();                   /* time accounting */
            gv.dtime += ddtime;

            hp = hkdb_find(cnm, 1);     /* method => hp */

            /* POP first */
            if (olevx >= levx)
            {
               for (i = olevx; i >= levx; i--)
               {
                  arc_out(1, harr[i]);
               }
            }

            /* push */
            harr[levx] = hp;
            arc_out(0, hp);             /* build tree, calls++ */
            olevx = levx;
         }

         /* last tree elem */
         acct2m();                      /* time accounting */

      }                                 /* while neof */
   }                                    /* java heap tree */

   /**18*******************************************************/
   /* genericMP */
   if (gv.ftype == 18)
   {
      char dir;
      int cpu, ex = 0;
      pPIDTID ptp;
      int cpumax = 0;

      /* set units */
      /* allow -u override */
      gv.Units = Remember("Cycles");
      hp = NULL;

      while ((fgets(buf, 2048, input) != NULL) && (gv.hkstp > gv.hkccnt) )
      {
         gv.hkccnt++;
         if (gv.hkccnt >= gv.hkstt)
         {
            tokarr = tokenize(buf, &tcnt);
            if (tcnt == 4)
            {
               /* pidtid : time cpu "pidtid" ptnm */
               /* method : time cpu "> | <"  mnm */

               gv.dtime = (UNIT)atof(tokarr[0]);
               cpu      = atoi(tokarr[1]);
               if (cpu > cpumax) cpumax = cpu;

               acctMP(gv.aptp[cpu]);

               dir = tokarr[2][0];

               if ( (dir == '>') || (dir == '<') || (dir == '@') )
               {
                  if (dir == '>')      ex = 0;   /* call */
                  else if (dir == '<') ex = 1;   /* return from */
                  else if (dir == '@') ex = 4;   /* return to */

                  /* use method name to find hp */
                  methname = tokarr[3];
                  hp = hkdb_find(methname, 1);

                  gv.ptp = gv.aptp[cpu];

                  /*
                  OptVMsg(" ex = %d hp %8x mnm %s\n",
                     ex, hp, methname);
                   */

                  arc_out(ex, hp);
               }
               else
               {                        /* pidtid */
                  newpt = tokarr[3];
                  ptp = gv.aptp[cpu];   /* curr ptp for this cpu */

                  if ( (ptp == NULL) || strcmp(ptp->apt, newpt) != 0)
                  {
                     gv.apt = Remember(newpt);
                     gv.ptp = ptptr(newpt);   /* find/add pidtid */
                     /* update array */
                     gv.aptp[cpu] = gv.ptp;
                  }
                  gv.ptp = gv.aptp[cpu];
                  gv.ptp->stptime = gv.dtime;

                  arc_out(3, hp);
               }
            }
         }
      }                                 /* while neof */

      for (cpu = 0; cpu <= cpumax; cpu++)
      {
         acctMP(gv.aptp[cpu]);
      }
   }

   /*********************************************************/
   /**6**17**19**23**26**/
   /*
    * -t <option> ftype  Time  Fields   Function
    * =========== =====  ====  ======   =========
    * generic         6  abs     3      original
    * genericIJ      17  del     3      del + tags(e.g. I:)
    * genericDel     19  del     3      del + wo tags
    * generic2       23  del     4      del + pidtid(fld 4)
    * genericMod     26  del     3      modnm for popping
    *
    */
   if ( (gv.ftype == 6) || (gv.ftype == 17) ||
        (gv.ftype == 19) || (gv.ftype == 23) || (gv.ftype == 26) )
   {
      char dir;
      int  hit = 0;
      int  len;
      int  maxlen = 0;
      char buf2[2048];

      if ((gv.ftype == 17) || (gv.ftype == 23)) gv.tags = 1;

      /* set units */
      /* allow -u override */
      if (gv.uflag != 1)
      {
         gv.Units = Remember("Cycles");
      }
      gv.dtime = 0;

      while ((fgets(buf, 2048, input) != NULL) && (gv.hkstp > gv.hkccnt) )
      {
         gv.hkccnt++;                   /* count hooks */

         if (gv.hkccnt >= gv.hkstt)
         {
            if (gv.allrecs == 1)
            {
               /* print all lines. PrePend ### */
               if (gv.flow)
               {
                  fprintf(xflow, " ### %s", buf);
               }
            }

            tokarr = tokenize(buf, &tcnt);

            hit = 0;
            if (tcnt == 3) hit = 1;

            if (tcnt > 3)
            {                           /* various extra fields */
               /* check second token for key words */
               dir = tokarr[1][0];
               if ( (dir == '>') || (dir == '<') || (dir == '?') ||
                    (dir == '@') || (dir == '-') ||
                    (strncmp(tokarr[1], "pidtid", 6) == 0) )
               {
                  hit = 1;

                  /* generic2 */
                  if ( (tcnt >= 4) && (gv.ftype == 23) )
                  {
                     newpt = tokarr[3]; /* pidtid */
                     if (strcmp(gv.apt, newpt) != 0)
                     {                  /* pidtid change */
                        gv.apt = Remember(newpt);

                        /* rtarcf - generic fixup */
                        gv.ptp = ptptr(newpt);   /* find/add pidtid */
                        gv.apt = gv.ptp->apt;
                        arc_out(3, hp);
                     }
                  }
               }
            }

            if (hit == 1)
            {
               /* input format */
               /* method :  units  >|<   method */
               /* impulse : units   ?    name   */
               /* stay :    units   @    name */
               /* pidtid :  units pidtid new_pt_val */

               /* all this before determining type */
               if (gv.ftype == 6 || gv.abs )
               {                        /* absolute time */
                  gv.dtime = (UNIT)atof(tokarr[0]);
               }
               else
               {                        /* delta time */
                  /* delta tm : genericDel & genericIJ */
                  gv.dtime += (UNIT)atof(tokarr[0]);
               }

               acct2();                 /* time accounting. delt => current pidtid */

               dir = tokarr[1][0];

               if ( (dir == '>') || (dir == '<') || (dir == '?') ||
                    (dir == '@') )
               {
                  if (dir == '>')      ex = 0;   /* call */
                  else if (dir == '<') ex = 1;   /* return from */
                  else if (dir == '?') ex = 2;   /* event(no structure) */
                  else if (dir == '@')
                  {
                     ex = 4;            /* return to */
                  }

                  /* use method name to find hp */
                  methname = tokarr[2];

                  len = (int)strlen(methname);
                  /* longest method name */
                  if (len > maxlen)
                  {
                     maxlen = len;
                     /* sprintf(buf2, " RecNo %4d : len %d mnm %s\n",
                      gv.hkccnt, len, methname); */
                  }

                  hp = hkdb_find(methname, 1);

                  /* generic2 == 23 genericMod == 26 ???? */

                  if (gv.ftype == 26)
                  {                     /** new form of generic2 ??? **/
                     /* find modnm & add to hp struct */
                     char * mp = methname;
                     char * modnm = mp;

                     if (hp->modnm == NULL)
                     {
                        /* modnm is methnm aft the last : */
                        while (*mp != '\0')
                        {
                           if (*mp == ':') modnm = mp + 1;
                           mp++;
                        }
                        if (modnm == methname) modnm = mp;
                        OptVMsg(" mnm <%s> modnm <%s>\n",
                                methname, modnm);
                        hp->modnm = (char *)Remember(modnm);
                     }
                  }

                  /* itrace & ictrace fixup for @ not on stk */
                  if (ex == 4)
                  {                     /* @ */
                     int rc = 0;

                     /* K:kernel on stack
                      *  => retn notOnStack
                      */
                     rc = onStack(hp);

                     /* Because I changed onStack : positive => OnStack */
                     if (rc >= 1) rc = 1;

                     /*            rc  ex  action
                      * onStk       1+  4   @ (pop to)
                      *  (on stack => return to it or STAY)
                      *
                      * NotOnStk
                      *  pid        0   0   >
                      *  pid + 1   -1   0   pop(to pidtid) & >
                      *  pid + 2   -2   0   >
                      *  pid + 3+  -n   0   >
                      */

                     if (rc == -1)
                     {
                        gv.ptp->ctp = gv.ptp->rtp;
                     }
                     /* not on => CALL */
                     if (rc != 1) ex = 0;
                  }
                  arc_out(ex, hp);
               }

               else if (dir == '-')
               {                        /* pop to top */
                  gv.ptp->ctp = gv.ptp->rtp;
                  arc_out(6, hp);
               }

               else
               {                        /* pidtid */
                  if (strncmp(tokarr[1], "pidtid", 6) == 0)
                  {
                     newpt = tokarr[2]; /* pidtid */
                     if (strcmp(gv.apt, newpt) != 0)
                     {                  /* pidtid change */
                        gv.apt = Remember(newpt);

                        /* rtarcf - generic fixup */
                        gv.ptp = ptptr(newpt);   /* find/add pidtid */
                     }
                     arc_out(3, hp);
                  }
                  /* if not => unused. comment record */
               }
            }
         }
      }                                 /* while neof */

      OptVMsg("%s\n", buf2);
      /* pop all threads to pidtid level for XCOND */
   }

// mjd genericWAS     28  TOD     8+
// parms= [mm/dd/yy hh:mm:ss:msec CDT],thread_Id ,process_Name,action,class,method,entry/exit, misc
//           0            1          2      3           4         5       6     7    8          9

#define dteParm      0
#define timeParm     1
#define timeZoneParm 2
#define threadParm   3
#define pNameParm    4
#define actionParm   5
#define classParm    6
#define methodParm   7
#define entExitParm  8

   if (gv.ftype == gv_ftype_genericWAS)
   {
      char   dir;
      int    hit = 0;
      int    len;
      int    maxlen = 0;
      char   buf2[2048];
      char   qualName[256];
      STRING entryExit;
      STRING lastToken;

      //printf("in genericWAS\n");
      //printf(" gv.hkstt %d\n", gv.hkstt);
      //printf(" gv.hkstp %d\n", gv.hkstp);


      /* set units */
      gv.Units = Remember("Millisec");
      gv.dtime = 0;

      while ((fgets(buf, 2048, input) != NULL) && (gv.hkstp > gv.hkccnt) )
      {
         //printf("in while \n");
         gv.hkccnt++;                   /* count hooks */
         //printf(" gv.hkccnt %d\n", gv.hkccnt);

         if (gv.hkccnt >= gv.hkstt)
         {
            if (gv.allrecs == 1)
            {
               // printf("in if (gv.allrecs == 1)\n");
               /* print all lines. PrePend ### */
               if (gv.flow)
               {
                  // printf("in if (gv.flow)\n");
                  fprintf(xflow, " ### %s", buf);
               }
            }
            //printf("buf=%s",buf);
            tokarr = tokenize(buf, &tcnt);
            // printf("tokarr[tcnt]=%s\n",tokarr[tcnt]);

            hit = 0;
            /* check second token for key words */
            dir = tokarr[dteParm][0];
            /* printf("tokarr[dteParm]=%s\n",tokarr[dteParm]); */
            if ( (dir == '[') )         // '[' is the start of a timestamp, hopefully
            {
               /* printf("in if ( (dir == '[') )\n"); */
               hit = 1;

               newpt = tokarr[threadParm];   /* pidtid */
               if (strcmp(gv.apt, newpt) != 0)
               {                        /* pidtid change */
                  /* printf("in if (strcmp(gv.apt, newpt)\n"); */
                  gv.apt = Remember(newpt);

                  /* rtarcf - generic fixup */
                  gv.ptp = ptptr(newpt);   /* find/add pidtid */
                  gv.apt = gv.ptp->apt;
                  arc_out(3, hp);
               }
            }

            if (hit == 1)
            {
               /* absolute time */
               int    rc;
               float  h,m,s,ms;
               STRING comma;
               STRING paren;

               rc = sscanf(tokarr[timeParm], "%g:%g:%g:%g", &h, &m, &s, &ms);

               //gv.dtime = (UNIT)h*3600000000.0+m*60000000.0+s*1000000.0+1000.0*ms;
               gv.dtime = (UNIT)h*3600000.0+m*60000.0+s*1000.0+ms;

               acct2();                 /* time accounting. delt => current pidtid */

               dir = tokarr[actionParm][0];
               entryExit = tokarr[entExitParm];
               //printf("token count=%d\n",tcnt);
               //printf("tokarr[tcnt]=%s\n",tokarr[tcnt]);

               lastToken=tokarr[tcnt-1];
               // if (0==strcmp(methname,"getCmsSession")) {
               //   printf("last token for getcms=%s",lastToken);
               // printf("at position %d\n",lastToken-tokarr[0]);
               //  }

               if ( '>' == dir
                    && ( 0 == strcmp(lastToken,"Entry")
                         || 0 == strcmp(lastToken,"ENTRY") ) )
               {
                  if ( 0 != strcmp(entryExit,"Entry")
                       && 0 != strcmp(entryExit,"ENTRY") )
                  {
                     printf("fixed one entry for %s\n",tokarr[methodParm]);
                     entryExit = lastToken;
                  }
               }
               // printf("tokarr[tcnt]=%s\n",tokarr[tcnt]);
               // }

               if ( ( 0 == strcmp(entryExit,"Entry") || 0 == strcmp(entryExit,"Exit")
                      || 0 == strcmp(entryExit,"ENTRY") || 0 == strcmp(entryExit,"RETURN") )
                    && ( dir == '>' || dir == '<' || dir == '?' || dir == '@' ) )
               {
                  if (dir == '>')      ex = 0;   /* call */
                  else if (dir == '<') ex = 1;   /* return from */
                  else if (dir == '?') ex = 2;   /* event(no structure) */
                  else if (dir == '@')
                  {
                     ex = 4;            /* return to */
                  }

                  /* fix up the method name.  delete commas and parms in parens */
                  methname = tokarr[methodParm];
                  comma    = strchr(methname,',');
                  if ( comma ) comma[0]=0;   // don't want comma in method name
                  paren    = strchr(methname,'(');
                  if ( paren ) paren[0]=0;   // don't want method parms in qualified name.

                  className = tokarr[classParm];
                  strcpy(qualName,className);
                  strcat(qualName,".");
                  strcat(qualName,methname);

                  //printf("time=%s,qualName=%s\n",tokarr[timeParm],qualName);

                  strcat(qualName,"()");
                  //printf("qualName2=%s\n",qualName);
                  //printf(":%s\n",entryExit);

                  len = (int)strlen(qualName);
                  /* longest class name */
                  if (len > maxlen)
                  {
                     maxlen = len;
                     /* sprintf(buf2, " RecNo %4d : len %d mnm %s\n",
                     gv.hkccnt, len, qualName); */
                  }

                  hp = hkdb_find(qualName, 1);
                  arc_out(ex, hp);
               }

               else if (dir == '-')
               {                        /* pop to top */
                  gv.ptp->ctp = gv.ptp->rtp;
                  arc_out(6, hp);
               }

            }                           // end if hit==1
         }                              // end if (gv.hkccnt >= gv.hkstt)
      }                                 // end  while neof

      OptVMsg("%s\n", buf2);
      /* pop all threads to pidtid level for XCOND */
   }                                    // end genericWAS

   /**20*******************************************************/
   /* genericL for locks */
   if (gv.ftype == 20)
   {
      char dir;
      char * trglk = NULL;

      /* lopt 0x01 time, 0x02 lock as method */
      int tflg = 0x1 & lopt;            /* filter time */
      int mflg = 0x1 & (lopt >> 1);     /* treat lock as meth */

      /* set units */
      /* allow -u override */
      gv.Units = Remember("Cycles");
      gv.dtime = 0;

      while ((fgets(buf, 2048, input) != NULL) && (gv.hkstp > gv.hkccnt) )
      {
         gv.hkccnt++;                   /* each hook is significant */
         if (gv.hkccnt >= gv.hkstt)
         {
            tokarr = tokenize(buf, &tcnt);
            if (tcnt == 3)
            {
               /* input format */
               /* lock   : units lock   target_lock */
               /* pidtid : units pidtid new_pt_val */
               /* method : units  >|<   method */
               /* lock   : units  >m|<m lock   */

               gv.dtime = (UNIT)atof(tokarr[0]);

               acct2l(tflg);            /* time accounting. delt=>curr pidtid */

               dir = tokarr[1][0];

               if ( (dir == '>') || (dir == '<') || (dir == '@') )
               {
                  if (tokarr[1][1] == '\0')
                  {
                     if (dir == '>')      ex = 0;   /* call */
                     else if (dir == '<') ex = 1;   /* return from */
                     else if (dir == '@') ex = 4;   /* return to */

                     /* use method name to find hp */
                     methname = tokarr[2];
                     hp = hkdb_find(methname, 1);

                     arc_out(ex, hp);
                  }
                  else if (tokarr[1][1] == 'm')
                  {
                     /* lock enter or exit */
                     /* check for target lock */
                     if (strcmp(trglk, tokarr[2]) == 0)
                     {
                        if (dir == '>')
                        {
                           ex = 0;      /* call */
                           gv.ptp->lflg = 1;
                        }
                        else if (dir == '<')
                        {
                           ex = 1;      /* return from */
                           gv.ptp->lflg = 0;
                        }

                        /* use method name to find hp */
                        if (mflg == 1)
                        {
                           methname = tokarr[2];
                           hp = hkdb_find(methname, 1);

                           arc_out(ex, hp);
                        }
                     }
                  }
               }

               else if (strcmp(tokarr[1], "pidtid") == 0)
               {
                  newpt = tokarr[2];    /* pidtid */
                  if (strcmp(gv.apt, newpt) != 0)
                  {                     /* pidtid change */
                     gv.apt = Remember(newpt);
                     gv.ptp = ptptr(newpt);   /* find/add pidtid */
                  }
                  hp = NULL;
                  arc_out(3, hp);
               }
               else if (strcmp(tokarr[1], "lock") == 0)
               {
                  trglk = Remember(tokarr[2]);   /* target lock */
               }
            }
         }
      }                                 /* while neof */
   }

   /**7*******************************************************/
   /* javaos : Read JavaOS Ascii Dump from CCJ Reader */
   if (gv.ftype == 7)
   {
      aint mj, mn;
      static int first = 0;
      static double dtime0;
      double dtime;
      int lineno = 0;

      /* set units */
      gv.Units = Remember("Cycles");

      while ((fgets(buf, 2048, input) != NULL) && (gv.hkstp > gv.hkccnt) )
      {
         lineno++;
         tokarr = tokenize(buf, &tcnt);
         if (tcnt == 0) continue;

         /* beginning of hook */
         if (tokarr[0][0] == '(')
         {
            int rc;

            if (gv.hkccnt >= gv.hkstt)
            {
               dtime = gettm(tokarr[1]);   /* hex cycles */
               if (first == 0)
               {
                  first = 1;
                  dtime0 = dtime;
               }
               gv.dtime = (UNIT)( dtime - dtime0 );

               acct2();                 /* time accounting. delt => current pidtid */

               /*
               OptVMsg(" line %d tokarr[0] <%s>\n",
             lineno, tokarr[0]);
           */

               rc = getmm(tokarr[0], &mj, &mn);   /* (maj:mn) */

               if (rc == 1)
               {
                  switch (mj)
                  {
                  case 0x4:             /* interrupt */
                     /* suppress tprof interrupt to put in context */
                     if ((mn & 0x7fffffff) != 0x1)
                     {
                        gv.hkccnt++;
                        ex = 0;
                        if (mn & 0x80000000) ex = 1;

                        methname = tokarr[2];
                        hp = hkdb_find(methname, 1);

                        arc_out(ex, hp);
                     }
                     break;

                  case 0x10:            /* tprof */
                     /* hacked version for shashi */
                     /* if(1 == 1) { */
                     if (mn == 0x1c)
                     {
                        gv.hkccnt++;

                        ex = 0;
                        methname = tokarr[2];
                        hp = hkdb_find(methname, 1);

                        arc_out(2, hp);
                     }
                     /* } */
                     break;

                  case 0x12:            /* dispatch */
                     if ( (mn == 0x1) || (mn == 0x3) )
                     {
                        gv.hkccnt++;
                        newpt = getpt(tokarr[5]);   /* parse line for pt */
                        if (strcmp(gv.apt, newpt) != 0)
                        {               /* pidtid change */
                           gv.apt = Remember(newpt);
                           gv.ptp = ptptr(newpt);   /* find/add pidtid */
                        }

                        /* where is hp defined for 12 ??? */
                        /*
                           hp = hkdb_find(methname, 1);
                         */
                        hp = NULL;
                        arc_out(3, hp);
                     }
                     break;

                  case 0x30:            /* method */
                     gv.hkccnt++;

                     ex = 0;
                     if (strcmp(tokarr[3], "Exit") == 0)
                     {
                        ex = 1;
                     }
                     methname = tokarr[6];
                     hp = hkdb_find(methname, 1);

                     arc_out(ex, hp);
                     break;

                  default:
                     break;
                  }
               }
            }
         }
      }                                 /* while neof */
   }                                    /* java input file */

   /**8*******************************************************/
   /* proto javar heap. stack inputs */
   /* the long pipeline way. reads stacks => tree */
   if (gv.ftype == 8)
   {
      int freq = 0, ddtime = 0;
      int hcnt = 0;                     /* stack size */

      gv.Units = Remember("Bytes");
      gv.apt = Remember("JAVAR_HEAP_STACKS");
      gv.ptp = ptptr(gv.apt);           /* find/add pidtid */
      acct2();                          /* time accounting */

      while (fgets(buf, 2048, input) != NULL)
      {
         gv.hkccnt++;
         tokarr = tokenize(buf, &tcnt);

         if (tcnt == 2)
         {                              /* start of hdr */
            ddtime = atoi(tokarr[0]);   /* delta bytes */
            freq   = atoi(tokarr[1]);
         }

         else if (tcnt == 1)
         {
            hcnt++;
            hp = hkdb_find(tokarr[0], 1);   /* method => hp */
            arc_out(0, hp);             /* updates calls by 1 */
            gv.ptp->ctp->calls += freq - 1;   /* adj call cnt */
         }

         else if (tcnt == 0)
         {                              /* end of stack */
            if (hcnt >= 1)
            {
               gv.dtime += ddtime;
               acct2();                 /* time accounting */
               gv.ptp->ctp = gv.ptp->rtp;   /* go to root */
               hcnt = 0;
            }
         }
      }                                 /* while neof */

      if (hcnt >= 1)
      {                                 /* last stack */
         gv.dtime += ddtime;
         acct2();                       /* time accounting */
         gv.ptp->ctp = gv.ptp->rtp;
      }
   }                                    /* memleak input file */

   /**22 allocStacks***************************************************/
   /* xprof(alloc bytes) => static usage tree */
   /* bys m1 m2 m3 ... */
   if (gv.ftype == 22)
   {
      int freq, ddtime;
      int i, hcnt = 0;                  /* stack size */

      /*gv.Units = Remember("Bytes");*/
      gv.apt   = Remember("alloc");
      gv.ptp   = ptptr(gv.apt);         /* find/add pidtid */
      acct2();                          /* time accounting */

      while (fgets(buf, 2048, input) != NULL)
      {
         gv.hkccnt++;
         tokarr = tokenize(buf, &tcnt);

         if (tcnt >= 3)
         {
            /*if(tcnt >= 2) {*/
            ddtime = atoi(tokarr[0]);   /* bytes */
            freq   = 1;

            for (i = 1; i < tcnt; i++)
            {
               hcnt++;
               hp = hkdb_find(tokarr[i], 1);   /* method => hp */
               arc_out(0, hp);          /* updates calls by 1 */
               gv.ptp->ctp->calls += freq - 1;   /* adj call cnt */
            }

            gv.dtime += ddtime;
            acct2();                    /* time accounting */
            gv.ptp->ctp = gv.ptp->rtp;  /* go to root */
            hcnt = 0;
         }
      }                                 /* while neof */
   }

   /**9*******************************************************/
   /* proto javar heap. stack inputs */
   /* the short way. reads log & builds xfiles */
   if (gv.ftype == 9)
   {
      int  ddtime;
      int  i;
      char type = '-';
      char tarr[6][10] =
      {"MALLOC", "CALLOC", "REALLOC", "DUPSTR", "FREE"};
      char * tstr = NULL;


      gv.Units = Remember("Bytes");
      gv.apt = Remember("JAVAR_HEAP_STACKS2");
      gv.ptp = ptptr(gv.apt);           /* find/add pidtid */
      acct2();                          /* time accounting */

      while (fgets(buf, 2048, input) != NULL)
      {
         gv.hkccnt++;
         tokarr = tokenize(buf, &tcnt);

         if (tcnt >= 5)
         {
            if (tokarr[0][0] == 'M')
            {                           /* start of hdr */
               type = tokarr[1][0];     /* type (M C D R F) */
               if (type == 'M')      tstr = tarr[0];
               else if (type == 'C') tstr = tarr[1];
               else if (type == 'R') tstr = tarr[2];
               else if (type == 'D') tstr = tarr[3];
               else if (type == 'F') tstr = tarr[4];

               ddtime = atoi(tokarr[2]);   /* delta bytes */

               for (i = 4; i < tcnt; i++)
               {
                  if (tokarr[i] != 0)
                  {
                     hp = hkdb_find(tokarr[i], 1);   /* method => hp */
                     arc_out(0, hp);
                  }
               }

               hp = hkdb_find(tstr, 1); /* method => hp */
               arc_out(0, hp);

               gv.dtime += ddtime;
               acct2();                 /* time accounting */
               gv.ptp->ctp = gv.ptp->rtp;   /* go to root */
            }
         }
      }                                 /* while neof */

   }                                    /* javai/javar input file */

   /**10*******************************************************/
   /* jaix jit ascii format */
   if (gv.ftype == 10)
   {
      char dir;

      hp = NULL;

      /* set units */
      if (gv.uflag != 1)
      {
         OptMsg(" Use -u flag to set units\n");
         panic(" Use -u option to set UNITS(Cycles, Inst ...");
      }
      else
      {
         gv.Units = gv.ustr;
      }

      while ((fgets(buf, 2048 , input) != NULL) && (gv.hkstp > gv.hkccnt) )
      {
         gv.hkccnt++;                   /* each hook is significant */
         if (gv.hkccnt >= gv.hkstt)
         {
            tokarr = tokenize(buf, &tcnt);
            if (tcnt >= 6)
            {
               /* input format */
               /* # time pt pt <|> method ... */
               if (tokarr[0][0] != '#') continue;

               /* cycles */
               /* gv.dtime = atoi(tokarr[0]); */
               /* take low 8 hex and convert to gv.dtime(double) */
               gv.dtime = (UNIT)gettm(&tokarr[0][8]);

               acct2();                 /* time accounting. delt => current pidtid */

               /* pidtid first */
               newpt = tokarr[2];       /* pidtid */
               if (strcmp(gv.apt, newpt) != 0)
               {                        /* pidtid change */
                  gv.apt = Remember(newpt);
                  gv.ptp = ptptr(newpt);   /* find/add pidtid */
                  arc_out(3, hp);
               }

               dir = tokarr[4][0];
               if ( (dir == '>') || (dir == '<'))
               {
                  if (dir == '>') ex = 0;
                  else ex = 1;

                  /* use method name to find hp */
                  methname = tokarr[5];
                  hp = hkdb_find(methname, 1);
                  arc_out(ex, hp);
               }
            }
         }
      }                                 /* while neof */
   }                                    /* java input file */

   /**11*******************************************************/
   /* java aix jit binary file */
   if (gv.ftype == 11)
   {

      if (gv.uflag != 1)
      {
         OptMsg(" Use -u flag to set units\n");
         panic(" Use -u option to set UNITS(Cycles, Inst ...");
      }
      else
      {
         gv.Units = gv.ustr;
      }

#ifdef _AIX32
      read_aixjit(nrmfile);
#endif
   }

   /**12*******************************************************/
   /* javaos jtprof stacks */
   /* DEPRECATED */
   if (gv.ftype == 12)
   {
      int  ddtime = 0;
      int  stk = 0;

      gv.Units = Remember("Ticks");
      gv.apt = Remember("JAVAOS_JTPROF_STACKS");
      gv.ptp = ptptr(gv.apt);           /* find/add pidtid */
      acct2();                          /* time accounting */

      tokarr = tokenize(buf, &tcnt);

      while (fgets(buf, 2048, input) != NULL)
      {
         gv.hkccnt++;

         /* tokarr not set. bug ?? */
         if (strncmp(tokarr[0], "PID/TID", 7) == 0)
         {                              /* pidtid record */
            stk = 1;

            gv.apt = Remember(getpidtid(buf + 8));
            gv.ptp = ptptr(gv.apt);     /* find/add pidtid */
            arc_out(3, NULL);
         }

         tokarr = tokenize(buf, &tcnt);

         if (tcnt == 2)
         {
            if (tokarr[0][0] == 'F')
            {                           /* start of hdr */
               ddtime = 1;              /* ticks */
               stk = 1;

               hp = hkdb_find(tokarr[1], 1);   /* method => hp */
               arc_out(0, hp);
            }
         }
         if (tcnt == 0)
         {
            if (stk == 1)
            {
               stk = 0;
               gv.dtime += ddtime;
               acct2();                 /* time accounting */
               gv.ptp->ctp = gv.ptp->rtp;   /* go to root */
            }
         }
      }                                 /* while neof */

      if (stk == 1)
      {
         stk = 0;
         gv.dtime += ddtime;
         acct2();                       /* time accounting */
         gv.ptp->ctp = gv.ptp->rtp;     /* go to root */
      }
   }                                    /* javaos tprof stacks */

   /**13*14****************************************************/
   /* xtree add sub min */
   if ((gv.ftype == 13) || (gv.ftype == 14) || (gv.ftype == 27))
   {
      int i, nfs;
      int levx, olevx;
      int ddtime = 0;
      struct HKDB * hp = NULL;
      //STJ//struct HKDB * harr[HARRSZ];
      STRING cnm;
      int start = 0;
      int bfld = 3;                     /* Base field is 3 or 5 */
      UINT64 calls;
      int len;
      int sign = 0;

      if (gv.ftype == 13)
      {
         sign = 1;                      /* subtract */
         if (gv.ninfiles != 2)
         {
            fprintf(stderr,
                    " Error: Need 2 INFILES for -t xtree_sub\n");
            exit(-1);
         }
      }

      if (gv.ftype == 14)
      {
         sign = 0;                      /* add */
         if (gv.ninfiles < 2)
         {
            fprintf(stderr,
                    " Error: Need 2 or More INFILES for -t xtree_add\n");
            exit(-1);
         }
      }

      if (gv.ftype == 27)
      {
         sign = -1;                     /* min */
         if (gv.ninfiles < 2)
         {
            fprintf(stderr,
                    " Error: Need 2 or More INFILES for -t xtree_min\n");
            exit(-1);
         }
      }

      gv.dtime = 0;

      /* READ 1st file */
      olevx = 0;
      while (fgets(buf, 2048, input) != NULL)
      {
         gv.hkccnt++;
         tokarr = tokenize(buf, &tcnt);

         if (tcnt >= 6)
         {
            /* skip header */
            if (strcmp("Lv", tokarr[0]) == 0)
            {
               start = 1;
               if (strcmp("Base", tokarr[5]) == 0)
               {
                  bfld = 5;
               }
               continue;
            }
         }

         if (start == 0) continue;

         if (tcnt >= 5)
         {                              /* 5 or more fields */
            levx   = atoi(tokarr[0]);
            CheckHARR(levx);

            calls  = atoUINT64(tokarr[2]);
            ddtime = atoi(tokarr[bfld]);
            cnm    = tokarr[tcnt - 1];  /* class name (last field) */

            if (levx == 0)
            {                           /* pidtid */
               /* take the _pidtid off */
               len = (int)strlen(cnm);
               cnm[len - 7] = 0;
               gv.apt = Remember(cnm);
               gv.ptp = ptptr(gv.apt);  /* find/add pidtid */
               gv.ptp->ctp = gv.ptp->rtp;
               arc_out(3, hp);

               OptVMsg(" + pidtid %s\n", gv.apt);
            }
            else
            {
               hp = hkdb_find(cnm, 1);  /* method => hp */

               if (olevx >= levx)
               {
                  for (i = olevx; i >= levx; i--)
                  {
                     arc_out(1, harr[i]);   /* EXITs unwind stk */
                  }
               }

               harr[levx] = hp;
               arc_out(0, hp);          /* ENTRY */
               gv.ptp->ctp->calls = calls;
            }

            gv.dtime = (UNIT)ddtime;
            if (sign >= 0)
            {
               acctx(0);                /* time accounting */
            }
            else
            {
               acctx(-1);               /* time accounting */
            }
            if (sign != 0)
            {
               gv.ptp->ctp->pass = 1;
            }

            olevx = levx;
         }
      }                                 /* eof */

      /* In acctx: Only +base added to tottime */

      start = 0;
      olevx = 0;

      /* READ other file(s) */
      for (nfs = 1; nfs < gv.ninfiles; nfs++)
      {
         if (sign == -1) sign = -2;     /* min only */

         while (fgets(buf, 2048, inarr[nfs]) != NULL)
         {
            gv.hkccnt++;
            tokarr = tokenize(buf, &tcnt);

            if (tcnt >= 6)
            {
               /* skip header */
               if (strcmp("Lv", tokarr[0]) == 0)
               {
                  start = 1;
                  if (strcmp("Base", tokarr[5]) == 0)
                  {
                     bfld = 5;
                  }
                  continue;
               }
            }

            if (start == 0) continue;

            if (tcnt >= 5)
            {                           /* 5 or more fields */
               cnm    = tokarr[tcnt - 1];   /* class name (last field) */
               levx   = atoi(tokarr[0]);
               CheckHARR(levx);

               calls  = atoUINT64(tokarr[2]);
               ddtime = atoi(tokarr[bfld]);

               if (levx == 0)
               {                        /* pidtid */
                  len = (int)strlen(cnm);
                  /* take the _pidtid off */
                  cnm[len - 7] = 0;
                  gv.apt = Remember(cnm);
                  gv.ptp = ptptr(gv.apt);   /* find/add pidtid */
                  gv.ptp->ctp = gv.ptp->rtp;   /* get to pidtid root */
                  arc_out(3, hp);
                  OptVMsg(" - pidtid %s\n", gv.apt);
               }
               else
               {
                  hp = hkdb_find(cnm, 1);   /* method => hp */

                  if (olevx >= levx)
                  {
                     for (i = olevx; i >= levx; i--)
                     {
                        arc_out(1, harr[i]);   /* EXITs unwind stk */
                     }
                  }

                  harr[levx] = hp;
                  arc_out(0, hp);       /* ENTRY (calls++ happens) */

                  if (sign == 1)
                  {                     /* subtract */
                     gv.ptp->ctp->calls -= (calls + 1);
                  }
                  else
                  {
                     if (sign == 0)
                     {                  /* add */
                        gv.ptp->ctp->calls += (calls - 1);
                     }
                     else
                     {
                        gv.ptp->ctp->calls--;
                        if (gv.ptp->ctp->calls > calls)
                        {
                           gv.ptp->ctp->calls = calls;   /* min calls */
                        }
                     }
                  }
               }

               gv.dtime = (UNIT)ddtime;
               acctx(sign);             /* time accounting (minus) */
               if (sign != 0)
               {
                  gv.ptp->ctp->pass |= 2;
               }
               olevx = levx;
            }
         }                              /* eof */
      }

      OptMsg(" DONE\n");

   }                                    /* xbtree OR xtree input */

   /****21*24**********************************************/
   /* process header of log-rt                                       */
   /* 21-mtreeN                                                      */
   /* 24-mtreeO                                                      */
   /* LV CALLS  BC CYC NAME                                          */
   /*  0    0    1  18  main                                         */
   /*  1    1    2  68   java/lang/System.initializeSystemClass()V   */

   if ( gv.ftype == 21                  // mtreeN
        || gv.ftype == 24 )             // mtreeO
   {
      char fNewHeader = 0;
      int  lvflag     = 0;              // Header line found

      while ( fgets(buf, 2048, input) != NULL )   // Gather the bases from the header
      {
         gv.hkccnt++;
         gv.maxTokens = 3;
         tokarr       = tokenize(buf, &tcnt);

         if ( 2 == tcnt )
         {
            if ( 0 == strcmp(tokarr[0], "Base" )
                 && 0 == strcmp(tokarr[1], "Values:") )
            {
               fNewHeader = 1;
            }
            if ( 0 == strcmp(tokarr[0], "Column" )
                 && 0 == strcmp(tokarr[1], "Labels:") )
            {
               break;
            }
         }
         else if ( 3 == tcnt
                   && 0 == strcmp( tokarr[1], "::" ) )
         {
            if ( gv.numBases >= MAX_BASES )
            {
               panic( "Too many bases" );
            }
            gv.baseName[gv.numBases] = Remember( tokarr[0] );
            gv.baseDesc[gv.numBases] = Remember( tokarr[2] );   // Remainder of string, starting with 3rd token
            gv.numBases++;
         }
      }

      while ( fgets(buf, 2048, input) != NULL )   // Search for header line
      {
         gv.hkccnt++;
         tokarr = tokenize(buf, &tcnt);

         if ( 0 == strcmp( tokarr[0], "LV" )
              && 0 != strcmp( tokarr[1], "::" ) )   // Header line found
         {
            lvflag = 1;

            break;                      // ends header. begin tree records
         }
      }

      if (lvflag == 0)
      {
         fprintf(stderr, " Input File Format Error\n");
         fprintf(stderr, " Need Header Record Starting with \"LV\" \n");
         exit(-1);
      }

      if ( 0 == ostr )
      {
         ostr = gv.baseName[0];
      }

      if ( fNewHeader )                 // New log-rt header found
      {
         if ( 24 == gv.ftype )          // mtreeO
         {
            gv.ftype = 21;              // Change it to mtreeN

            fprintf(stderr, " New file format found.  Converting mtreeO to mtreeN.\n");
         }
      }

      for ( i = 0; i < gv.numBases; i++ )
      {
         if ( 0 == strcmp( ostr, gv.baseName[i] ) )
         {
            if ( 0 == strcmp( gv.Units, "UnDefined" ) )
            {
               gv.Units = gv.baseDesc[i];
            }
            break;
         }
      }

      if ( i >= gv.numBases )           // Base not found
      {
         if ( fNewHeader
              && 0 == strncmp( ostr, "BASE", 4 ) )
         {
            if ( 0 == ostr[4] )
            {
               i = 0;
            }
            else if ( '-' == ostr[4]
                      && '1' <= ostr[5]
                      && '9' >= ostr[5]
                      && 0 == ostr[6] )
            {
               i = ostr[5] - '0';
            }
            else
            {
               fprintf(stderr, " mtreeN option %s not found, must be a base name\n", ostr);
               exit(-2);
            }

            if ( i >= gv.numBases )
            {
               fprintf(stderr, " mtreeN option %s is invalid, no corresponding metric name\n", ostr);
               exit(-2);
            }

            fprintf(stderr, " New file format found.  Converting %s to %s.\n", ostr, gv.baseName[i]);

            ostr = gv.baseName[i];

            if ( 0 == strcmp( gv.Units, "UnDefined" ) )
            {
               gv.Units = gv.baseDesc[i];
            }
         }
         else
         {
            fprintf(stderr, " mtreeN option %s not found, must be a base metric.\n", ostr);
            exit(-2);
         }
      }
   }

   /****21*************************************************/
   /* log-rt from rtarcf */
   /* 21-mtreeN (mtreeO in new section below)
      LV CALLS  BC CYC NAME
       0    0    1  18  main
       1    1    2  68   java/lang/System.initializeSystemClass()V
    */

   if ( gv.ftype == 21 )                // mtreeN
   {
      int    i;
      int    levx, olevx = 0;
      int    mfld;                      /* metric field location */

      UINT64 calls  = 0;
      int    minfld = 0;                // subtract to get FREED ?
      int    callsField = 1;            /* mtreeO object record calls field */

      UNIT   ddtime = 0;

      struct HKDB * hp = NULL;
      //STJ//struct HKDB * harr[HARRSZ];
      STRING mnm;

      /* mtreeO stuff */
      int mlev     = 0;                 /* level of last Real Method */
      int methflag = 0;

      int  uname  = 0;                  /* 1 => deduce units name */
      int  ftflag = 0;
      uint dptr   = 0;

      /** CODE **/

      // mtreeN BASE or BASE-x can be found in log-rt file header
      // if -t mtreeN option is either :
      // BASE or BASE-x or // tokarr[2] when BASE or BASE-x
      // both log-rt metric field & gv.Units is determined
      // if match found with a log-rt header field use it
      // if match found replace UnDefined

      /* mnm in LastField(NF) */
      OptVMsg(" proc_inp: mflg %d lopt %d ostr <%s>\n",
              memflag, lopt, ostr);

      gv.apt = Remember("unknown_java_pidtid");
      gv.ptp = ptptr(gv.apt);           /* find/add pidtid */
      arc_out(3, hp);

      gv.dtime = 0;

      // find position of ostr in header

      mfld       = 0;
      callsField = 1;

      for ( i = 1; i < tcnt; i++ )
      {
         if ( 0 == strcasecmp(ostr, tokarr[i]) )
         {
            mfld = i;

            if ( 0 == strcmp( ostr, "AO" )
                 || 0 == strcmp( ostr, "LO" ) )
            {
               callsField = i;
            }
            if ( 0 == strcmp( ostr, "AB" )
                 || 0 == strcmp( ostr, "LB" ) )
            {
               callsField = i - 1;
            }

            OptVMsg(" metric field: %d %s\n", i + 1, ostr);   // found matching field in log-rt file

            break;
         }
      }

      if ( mfld == 0 )
      {
         if (strcasecmp(ostr, "FT") == 0)   // FROM / TO Mode :
         {
            // get base from FrTo Records
            ftflag = 1;
            mfld   = -1;
         }
         else
         {
            fprintf(stderr, " mtreeN option %s not found, must be a base name\n", ostr);
            exit(-2);
         }
      }

      // Start Parsing Tree Records
      // mtreeN mtreeO Objects whereIdleStacks
      levx = olevx = 0;
      while ((fgets(buf, 2048, input) != NULL) && (gv.hkstp > gv.hkccnt) )
      {
         char * cp;
         jsp_t * jp;

         gv.hkccnt++;

         if (gv.hkccnt >= gv.hkstt)
         {
            tokarr = tokenize(buf, &tcnt);

            /* Method record */
            if ( tcnt >= 3              // Minimum = LV base methodname
                 && '0' <= tokarr[0][0]
                 && '9' >= tokarr[0][0] )
            {
               methflag = 1;            // for Object accounting
               mlev = levx = atoi(tokarr[0]);
               CheckHARR(levx);

               acct2();                 /* time accounting */

               if (levx == 0)
               {                        /* pidtid */
                  gv.apt = Remember(tokarr[tcnt - 1]);
                  if (gv.tnms >= 1)
                  {
                     tnm_change(gv.apt);
                  }

                  if (gv.ctree >= 1)
                  {
                     int i;
                     for (i = 0; i < gv.ctree; i++)
                     {
                        cp = strstr(gv.apt, gv.ctstr[i]);
                        if (cp != NULL)
                        {
                           gv.apt = gv.ctstr[i];
                        }
                     }
                  }

                  // define curr pidtid
                  gv.ptp = ptptr(gv.apt);   /* find/add pidtid */

                  gv.ptp->ctp = gv.ptp->rtp;
                  // lv = 0 . tid record record
                  dptr = strtol(tokarr[tcnt - 2], NULL, 16);
                  gv.ptp->ctp->dptr = dptr;

                  jp = insert_jstack(dptr);
                  jp->tp = gv.ptp->ctp;
                  OptVMsg(" sz %4d %x %p\n",
                          gv.jstack_table.elems, jp->jsp, jp->tp);

                  arc_out(3, hp);       /* effects xflow & xcond only */
               }

               /* metric field based on option */
               /* atof range ??? */
               if ( ftflag == 0 )
               {
                  ddtime = (UNIT)atof(tokarr[mfld]);
               }
               else ddtime = 0;         // only count Fr or To for this mode

               gv.dtime += ddtime;

               if (levx >= 1)
               {
                  mnm = tokarr[tcnt - 1];   /* last field */

                  if (gv.nopre == 1)
                  {
                     if (mnm[1] == ':')
                     {
                        mnm = mnm + 2;
                     }
                  }

                  hp  = hkdb_find(mnm, 1);   /* method => hp */

                  if (olevx >= levx)
                  {
                     /* pop stack to new level - 1 */
                     for (i = olevx; i >= levx; i--)
                     {
                        arc_out(1, harr[i]);   /* EXIT */
                     }
                  }

                  if ((levx - 1) > olevx)
                  {
                     OptVMsg(" *1* TreeInputFormatError RN %d  levx=%d  olevx=%d \n", gv.hkccnt, levx, olevx);
                     for (i = 0; i < tcnt; i++) {
                        OptVMsg(" %2d: %s\n", i, tokarr[i]);
                     }
                     exit(0);
                  }

                  arc_out(0, hp);       /* PUSH entry */
                  harr[levx] = hp;

                  if ( '(' == mnm[0] )
                  {
                     calls  = atoUINT64(tokarr[callsField]);
                  }
                  else
                  {
                     calls  = atoUINT64(tokarr[1]);
                  }

                  /* arc_out => calls++ for normal entry / exits */
                  gv.ptp->ctp->calls += calls - 1;   /* for mtreeN */
                  // normal lv >=1 record

                  dptr = strtol(tokarr[tcnt - 2], NULL, 16);
                  gv.ptp->ctp->dptr = dptr;

                  jp = insert_jstack(dptr);
                  jp->tp = gv.ptp->ctp;

                  //OptVMsg(" sz %4d %x %p\n",
                  //      gv.jstack_table.elems, jp->jsp, jp->tp);
               }
               else
               {
                  /* levx == 0 => pidtid ?? */
               }
            }

            // process FR TO records inserted by wi.awk
            // need to pop back to where we statred
            else if ( (tcnt == 4) && (tokarr[0][0] == '+'))
            {
               methflag = 1;
               mlev = levx = atoi(tokarr[1]);
               CheckHARR(levx);

               acct2();                 /* time accounting */

               /* metric field based on option */
               /* atof range ??? */
               calls  = atoUINT64(tokarr[2]);

               if (ftflag == 0)
               {
                  ddtime = 0;
               }
               else
               {
                  ddtime = (UNIT)((INT64)calls);
               }

               gv.dtime += ddtime;

               // removed if levx >= 1
               mnm = tokarr[tcnt - 1];

               hp  = hkdb_find(mnm, 1); /* method => hp */

               // this needed to reset to old level
               if (olevx >= levx)
               {
                  /* pop stack to new level - 1 */
                  for (i = olevx; i >= levx; i--)
                  {
                     arc_out(1, harr[i]);   /* EXIT */
                  }
               }

               arc_out(0, hp);          /* PUSH entry */
               harr[levx] = hp;

               calls  = atoUINT64(tokarr[2]);

               /* arc_out => calls++ for normal entry / exits */
               // because arc_out gives you free 1 ???
               gv.ptp->ctp->calls += calls - 1;   /* for mtreeN */

               // Old design. awk adding records to log-rt
               gv.ptp->ctp->dptr = strtol(tokarr[tcnt - 2], NULL, 16);
            }
            olevx = levx;
         }
      }                                 /* while neof */
      acct2();                          /* last time accounting */

      // Process WHERE file here
      if (gv.where != 0)
      {
         int cnt;
         int s1, s2;
         char * pid1, * pid2, * tid1, * tid2;
         char * pnm1, * pnm2;
         static char ptrec[256];

         jsp_t * js1;
         jsp_t * js2;

         struct TREE * t1;
         struct TREE * t2;
         struct TREE * tt;
         struct TREE * tarr[256];
         struct TREE * tx;
         struct TREE * tb;              // base tree
         struct TREE * ta;              // add on tree

         char xnm[512];

         // read records
         // create additional stacks per recs
         // use FT option to determine metric
         while ((fgets(buf, 2048, gv.where) != NULL) )
         {
            tokarr = tokenize(buf, &tcnt);

            s1  =  s2 = 0;
            js1 = js2 = 0;
            t1  =  t2 = 0;

            cnt  = atoi(tokarr[0]);
            pid1 = tokarr[1];
            tid1 = tokarr[2];
            s1   = strtol(tokarr[3], NULL, 16);
            pnm1 = tokarr[4];
            pid2 = tokarr[5];
            tid2 = tokarr[6];
            s2   = strtol(tokarr[7], NULL, 16);
            pnm2 = tokarr[8];

            if (s1)
            {
               js1 = lookup_jstack(s1);
               if (js1)
               {
                  t1 = js1->tp;
               }
            }

            if (s2)
            {
               js2 = lookup_jstack(s2);
               if (js2)
               {
                  t2 = js2->tp;
               }
            }

            if (t1 && t2)
            {
               int i, j, tmax;
               char aft[2][4] = { "T_", "F_"};

               for (j = 0; j <= 1; j++)
               {                        // t1 (TO) t2 (FROM)
                  if (j == 0)
                  {
                     // base t1 addon t2
                     tb = t1;
                     ta = t2;
                     hp = hkdb_find("TO", 1);   /* method => hp */
                  }
                  else
                  {
                     // base t2 addon t1
                     tb = t2;
                     ta = t1;
                     hp = hkdb_find("FROM", 1);   /* method => hp */
                  }

                  tt = pushName(tb, hp);
                  tt->calls += cnt;

                  i = 0;
                  tarr[0] = ta;
                  tx = ta->par;
                  while (tx != 0)
                  {
                     i++;
                     tarr[i] = tx;
                     tx = tx->par;
                  }
                  tmax = i;

                  for (i = tmax; i >= 0; i--)
                  {
                     sprintf(xnm, "%s%s", aft[j], tarr[i]->hp->hstr2);
                     hp = hkdb_find(xnm, 1);   /* method => hp */
                     tt = pushName(tt, hp);
                     tt->calls += cnt;
                  }
                  tt->btm  += cnt;      // top of stack get base

                  gv.dtime += (UNIT)cnt;
                  // fix
                  acct2ft();            // keep gv.tottime updated
               }
            }

            if (t1 && !t2)
            {
               // simple TO + pid-tid-pnm @ t1
               sprintf(ptrec, "%s_%s_%s", pid2, tid2, pnm2);

               hp = hkdb_find("TO", 1); /* method => hp */
               tt = pushName(t1, hp);
               tt->calls += cnt;

               hp = hkdb_find(ptrec, 1);   /* method => hp */
               tt = pushName(tt, hp);
               tt->calls += cnt;
               tt->btm   += cnt;        // top of stack get base

               gv.dtime  += (UNIT)cnt;
               acct2ft();               // keep gv.tottime updated
            }

            if (t2 && !t1)
            {
               // simple FROM + pid-tid-pnm @ t1
               sprintf(ptrec, "%s_%s_%s", pid1, tid1, pnm1);

               hp = hkdb_find("FROM", 1);   /* method => hp */
               tt = pushName(t2, hp);
               tt->calls += cnt;

               hp = hkdb_find(ptrec, 1);   /* method => hp */
               tt = pushName(tt, hp);
               tt->calls += cnt;
               tt->btm   += cnt;        // top of stack get base

               gv.dtime  += (UNIT)cnt;
               acct2ft();               // keep gv.tottime updated
            }
         }
      }
   }                                    /* rtarcf log-rt */

   /****24**(DEPRECATED)*******************************/
   /* log-rt from rtarcf */
   /* 24-mtreeO (mtreeN in new section above)
      LV CALLS  BC CYC NAME
       0    0    1  18  main
       1    1    2  68   java/lang/System.initializeSystemClass()V
    */

   if ( gv.ftype == 24 )
   {
      int   i;
      int   levx, olevx = 0;
      int   mfld;                       /* metric field location */

      int   ofld = 0;                   /* mtreeO object record metric fld */
      UINT64 calls  = 0;
      int   minfld = 0;                 // subtract to get FREED ?
      int   callsField = 0;             /* mtreeO object record calls field */

      UNIT  ddtime = 0;

      struct HKDB * hp = NULL;
      //STJ//struct HKDB * harr[HARRSZ];
      STRING mnm;

      /* mtreeO stuff */
      int mlev = 0;                     /* level of last Real Method */
      int methflag = 0;
      char mbuf[1024];

      int uname = 0;                    /* 1 => deduce units name */
      // Symbolic names for mtreeO
      char dnm[6][32] =
      {
         "ALLOCATED-OBJECTS",
         "ALLOCATED-BYTES",
         "LIVE-OBJECTS",
         "LIVE-BYTES",
         "FREED-OBJECTS",
         "FREED-BYTES"
      };

      int  ftflag = 0;
      uint dptr = 0;

      /** CODE **/

      if (gv.Units)
      {
         OptVMsg(" gv.Units <%s>\n", gv.Units);
      }
      else
      {
         OptMsg(" gv.Units = NULL\n");
      }

      if (gv.Units)
      {
         if (strcmp(gv.Units, "UnDefined") == 0)
         {
            uname = 1;
            // mtreeN BASE or BASE-x can be found in log-rt file header
            // if -t mtreeN option is either :
            // BASE or BASE-x or // tokarr[2] when BASE or BASE-x
            // both log-rt metric field & gv.Units is determined
            // mtreeO name must be AO AB LO or LB
            // if match found with a log-rt header field use it
            // if match found replace UnDefined
         }
      }

      /* mnm in LastField(NF) */
      OptVMsg(" proc_inp: mflg %d lopt %d ostr <%s>\n",
              memflag, lopt, ostr);

      gv.apt = Remember("unknown_java_pidtid");
      gv.ptp = ptptr(gv.apt);           /* find/add pidtid */
      arc_out(3, hp);

      gv.dtime = 0;

      /* ostr != Null => find symbolic field */
      mfld = lopt - 1;                  /* 0 vs 1 origin */

      /* mtreeO must be symbolic */
      /* field in Object Record */
      if (strcmp(ostr, "AO") == 0) ofld = 1;
      else if (strcmp(ostr, "AB") == 0) ofld = 2;
      else if (strcmp(ostr, "LO") == 0) ofld = 3;
      else if (strcmp(ostr, "LB") == 0) ofld = 4;

      else if (strcmp(ostr, "FO") == 0)
      {
         ofld = 1; minfld = 1;
      }

      else if (strcmp(ostr, "FB") == 0)
      {
         ofld = 2; minfld = 1;
      }

      else
      {
         fprintf(stderr, " INCORRECT mtreeO option\n");
         fprintf(stderr, "   -t mtreeO [AO | AB | LO | LB | FO | FB]\n");
         exit(-1);
      }
      callsField = 1 + 2 * ((ofld - 1) / 2);   // call field (Objects)

      if (uname == 1)
      {
         OptMsg(" Selecting mtreeO Label\n");
         OptVMsg("  ofld %d minfld %d\n", ofld, minfld);
         if (minfld == 0)
         {
            gv.Units = Remember(dnm[ofld - 1]);
         }
         else
         {
            // Freed O or B
            gv.Units = Remember(dnm[ofld + 3]);
         }
         uname = 0;                     // we have a name
         OptVMsg("  mtreeO label %s\n", gv.Units);
      }

      // Start Parsing Tree Records
      // mtreeN mtreeO Objects whereIdleStacks
      levx = olevx = 0;
      while ((fgets(buf, 2048, input) != NULL) && (gv.hkstp > gv.hkccnt) )
      {
         char * cp;
         jsp_t * jp;

         gv.hkccnt++;

         if (gv.hkccnt >= gv.hkstt)
         {
            tokarr = tokenize(buf, &tcnt);

            /* Method record */
            if ( (tcnt >= 5) &&
                 (tokarr[0][0] <= '9') &&
                 (tokarr[0][0] >= '0') )
            {

               methflag = 1;            // for Object accounting
               mlev = levx = atoi(tokarr[0]);
               CheckHARR(levx);

               acct2();                 /* time accounting */

               if (levx == 0)
               {                        /* pidtid */
                  gv.apt = Remember(tokarr[tcnt - 1]);
                  if (gv.tnms >= 1)
                  {
                     tnm_change(gv.apt);
                  }

                  if (gv.ctree >= 1)
                  {
                     int i;
                     for (i = 0; i < gv.ctree; i++)
                     {
                        cp = strstr(gv.apt, gv.ctstr[i]);
                        if (cp != NULL)
                        {
                           gv.apt = gv.ctstr[i];
                        }
                     }
                  }

                  // define curr pidtid
                  gv.ptp = ptptr(gv.apt);   /* find/add pidtid */

                  gv.ptp->ctp = gv.ptp->rtp;
                  // lv = 0 . tid record record
                  dptr = strtol(tokarr[tcnt - 2], NULL, 16);
                  gv.ptp->ctp->dptr = dptr;

                  jp = insert_jstack(dptr);
                  jp->tp = gv.ptp->ctp;
                  OptVMsg(" sz %4d %x %p\n",
                          gv.jstack_table.elems, jp->jsp, jp->tp);

                  arc_out(3, hp);       /* effects xflow & xcond only */
               }

               /* metric field based on option */
               /* atof range ??? */
               ddtime = 0;              // only count Fr or To for this mode

               gv.dtime += ddtime;

               if (levx >= 1)
               {
                  mnm = tokarr[tcnt - 1];   /* last field */

                  if (gv.nopre == 1)
                  {
                     if (mnm[1] == ':')
                     {
                        mnm = mnm + 2;
                     }
                  }

                  hp  = hkdb_find(mnm, 1);   /* method => hp */

                  if (olevx >= levx)
                  {
                     /* pop stack to new level - 1 */
                     for (i = olevx; i >= levx; i--)
                     {
                        arc_out(1, harr[i]);   /* EXIT */
                     }
                  }

                  if ((levx - 1) > olevx)
                  {
                     OptVMsg(" *2* TreeInputFormatError RN %d  levx=%d  olevx=%d\n", gv.hkccnt, levx, olevx);
                     for (i = 0; i < tcnt; i++) {
                        OptVMsg(" %2d: %s\n", i, tokarr[i]);
                     }
                     exit(0);
                  }

                  arc_out(0, hp);       /* PUSH entry */
                  harr[levx] = hp;

                  calls  = atoUINT64(tokarr[1]);

                  /* arc_out => calls++ for normal entry / exits */
                  gv.ptp->ctp->calls += calls - 1;   /* for mtreeN */
                  // normal lv >=1 record

                  dptr = strtol(tokarr[tcnt - 2], NULL, 16);
                  gv.ptp->ctp->dptr = dptr;

                  jp = insert_jstack(dptr);
                  jp->tp = gv.ptp->ctp;

                  //OptVMsg(" sz %4d %x %p\n",
                  //      gv.jstack_table.elems, jp->jsp, jp->tp);
               }
               else
               {
                  /* levx == 0 => pidtid ?? */
               }
            }

            /* Object record (mtreeO only) */
            else if ((tcnt >= 6) && (tokarr[0][0] == '-'))
            {
               if (methflag == 1)
               {
                  methflag = 0;
                  /* tricky. dont incre curr method. Put on Objects */
                  /* successive -- records are all children of mnm */

                  gv.dtime = gv.odtime; /* remove prev method delta metric */
               }
               levx = mlev + 1;
               CheckHARR(levx);

               ddtime = (UNIT)atof(tokarr[ofld]);   /* metric select */
               if (minfld == 1)
               {                        // FREED
                  ddtime -= (UNIT)atof(tokarr[ofld + 2]);
               }

               gv.dtime += ddtime;      /* see acct2 */

               if (gv.sobjs == 0)
               {
                  sprintf(mbuf, "(%s)", tokarr[tcnt - 1]);
               }
               else
               {
                  int osize = 0;
                  int bys, objs;

                  objs = atoi(tokarr[1]);
                  bys  = atoi(tokarr[2]);
                  if (objs >= 1)
                  {
                     osize = bys / objs;
                  }
                  sprintf(mbuf, "(%s_%d)", tokarr[tcnt - 1], osize);
               }

               hp = hkdb_find(mbuf, 1); /* Object => hp */

               arc_out(0, hp);          /* PUSH Object */
               acct2();                 /* accting, ofld as metric */

               /* select "calls" field based on ofld */
               /*
                  AO | AB => AO : 1 | 2 => 1
                  LO | LB => LO : 3 | 4 => 3
                */
               calls = atoUINT64(tokarr[callsField]);

               /* leave calls for objects @ 1 */
               gv.ptp->ctp->calls += calls - 1;   /* for mtreeO */
               // special object records
               gv.ptp->ctp->dptr = 0;   /* Not needed */

               // need this type sync in FR TO cases
               arc_out(1, hp);          /* POP  */   // exit the 1 level object push
               levx = mlev;             /* @ real method level */
               CheckHARR(levx);
            }

            // process FR TO records inserted by wi.awk
            // need to pop back to where we statred
            else if ( (tcnt == 4) && (tokarr[0][0] == '+'))
            {
               methflag = 1;
               mlev = levx = atoi(tokarr[1]);
               CheckHARR(levx);

               acct2();                 /* time accounting */

               /* metric field based on option */
               /* atof range ??? */
               calls  = atoUINT64(tokarr[2]);

               if (ftflag == 0)
               {
                  ddtime = 0;
               }
               else
               {
                  ddtime = (UNIT)((INT64)calls);
               }

               gv.dtime += ddtime;

               // removed if levx >= 1
               mnm = tokarr[tcnt - 1];

               hp  = hkdb_find(mnm, 1); /* method => hp */

               // this needed to reset to old level
               if (olevx >= levx)
               {
                  /* pop stack to new level - 1 */
                  for (i = olevx; i >= levx; i--)
                  {
                     arc_out(1, harr[i]);   /* EXIT */
                  }
               }

               arc_out(0, hp);          /* PUSH entry */
               harr[levx] = hp;

               calls  = atoUINT64(tokarr[2]);

               /* arc_out => calls++ for normal entry / exits */
               // because arc_out gives you free 1 ???
               gv.ptp->ctp->calls += calls - 1;   /* for mtreeN */

               // Old design. awk adding records to log-rt
               gv.ptp->ctp->dptr = strtol(tokarr[tcnt - 2], NULL, 16);
            }
            olevx = levx;
         }
      }                                 /* while neof */
      acct2();                          /* last time accounting */

      // Process WHERE file here
      if (gv.where != 0)
      {
         int cnt;
         int s1, s2;
         char * pid1, * pid2, * tid1, * tid2;
         char * pnm1, * pnm2;
         static char ptrec[256];

         jsp_t * js1;
         jsp_t * js2;

         struct TREE * t1;
         struct TREE * t2;
         struct TREE * tt;
         struct TREE * tarr[256];
         struct TREE * tx;
         struct TREE * tb;              // base tree
         struct TREE * ta;              // add on tree

         char xnm[512];

         // read records
         // create additional stacks per recs
         // use FT option to determine metric
         while ((fgets(buf, 2048, gv.where) != NULL) )
         {
            tokarr = tokenize(buf, &tcnt);

            s1  =  s2 = 0;
            js1 = js2 = 0;
            t1  =  t2 = 0;

            cnt  = atoi(tokarr[0]);
            pid1 = tokarr[1];
            tid1 = tokarr[2];
            s1   = strtol(tokarr[3], NULL, 16);
            pnm1 = tokarr[4];
            pid2 = tokarr[5];
            tid2 = tokarr[6];
            s2   = strtol(tokarr[7], NULL, 16);
            pnm2 = tokarr[8];

            if (s1)
            {
               js1 = lookup_jstack(s1);
               if (js1)
               {
                  t1 = js1->tp;
               }
            }

            if (s2)
            {
               js2 = lookup_jstack(s2);
               if (js2)
               {
                  t2 = js2->tp;
               }
            }

            if (t1 && t2)
            {
               int i, j, tmax;
               char aft[2][4] = { "T_", "F_"};

               for (j = 0; j <= 1; j++)
               {                        // t1 (TO) t2 (FROM)
                  if (j == 0)
                  {
                     // base t1 addon t2
                     tb = t1;
                     ta = t2;
                     hp = hkdb_find("TO", 1);   /* method => hp */
                  }
                  else
                  {
                     // base t2 addon t1
                     tb = t2;
                     ta = t1;
                     hp = hkdb_find("FROM", 1);   /* method => hp */
                  }

                  tt = pushName(tb, hp);
                  tt->calls += cnt;

                  i = 0;
                  tarr[0] = ta;
                  tx = ta->par;
                  while (tx != 0)
                  {
                     i++;
                     tarr[i] = tx;
                     tx = tx->par;
                  }
                  tmax = i;

                  for (i = tmax; i >= 0; i--)
                  {
                     sprintf(xnm, "%s%s", aft[j], tarr[i]->hp->hstr2);
                     hp = hkdb_find(xnm, 1);   /* method => hp */
                     tt = pushName(tt, hp);
                     tt->calls += cnt;
                  }
                  tt->btm  += cnt;      // top of stack get base

                  gv.dtime += (UNIT)cnt;
                  // fix
                  acct2ft();            // keep gv.tottime updated
               }
            }

            if (t1 && !t2)
            {
               // simple TO + pid-tid-pnm @ t1
               sprintf(ptrec, "%s_%s_%s", pid2, tid2, pnm2);

               hp = hkdb_find("TO", 1); /* method => hp */
               tt = pushName(t1, hp);
               tt->calls += cnt;

               hp = hkdb_find(ptrec, 1);   /* method => hp */
               tt = pushName(tt, hp);
               tt->calls += cnt;
               tt->btm   += cnt;        // top of stack get base

               gv.dtime  += (UNIT)cnt;
               acct2ft();               // keep gv.tottime updated
            }

            if (t2 && !t1)
            {
               // simple FROM + pid-tid-pnm @ t1
               sprintf(ptrec, "%s_%s_%s", pid1, tid1, pnm1);

               hp = hkdb_find("FROM", 1);   /* method => hp */
               tt = pushName(t2, hp);
               tt->calls += cnt;

               hp = hkdb_find(ptrec, 1);   /* method => hp */
               tt = pushName(tt, hp);
               tt->calls += cnt;
               tt->btm   += cnt;        // top of stack get base

               gv.dtime  += (UNIT)cnt;
               acct2ft();               // keep gv.tottime updated
            }
         }
      }
   }                                    /* rtarcf log-rt */
   if ( harr )
   {
      free(harr);
      harrsz = 0;
   }
}                                       /* end of proc_inp */


// how to update a node for a flow.
// overkill for tree input
#ifdef xxx
void pushft(UNIT ddtime, UINT64 calls, char * mnm)
{
   // just keep time correct at Node. no globals
   acct2();                             /* time accounting */
   // not reqd  ? side-effects on min sub add ??
   gv.dtime += (double)ddtime;          // lagging update
   // yes
   hp  = hkdb_find(mnm, 1);             /* method => hp */
   // no need to involve arc_out. just fix node
   arc_out(0, hp);                      /* PUSH entry */
   // fix node
   gv.ptp->ctp->calls += calls - 1;     /* for mtreeN */
   // fix node
   gv.ptp->ctp->dptr = 0;               // not valid for F T recs
}
#endif
