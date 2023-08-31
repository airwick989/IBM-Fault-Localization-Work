/*
 *
 *  Copyright (c) IBM Corporation 1996.
 *  Property of IBM Corporation.
 *  All Rights Reserved.
 *
 *  Origin 30 (IBM)
 *
 */

#include "nrm.h"   /* includes global.h */
void acct2(void);
struct HKDB * hp;

/* need to leave this prototype out to fake types) */
/* int  arc_out(int ex, struct HKDB * hp); */

FILE * rawout;
FILE * nrm;

extern double gv_suptime;
extern int    gv_sup_comp;              /* start up complete flag */
FILE * xopen(STRING fnm, STRING type);

/* ************************************************************ */
void init_globalx()
{

   gv.ptp = ptptr("fffffffe");          /* interrupt pidtid */
   gv.ptp = ptptr("fffffffd");          /* idle pidtid */
   gv.ptp = ptptr("ffffffff");          /* unknown pidtid */

   gv.hkstt = 0;
   gv.hkstp = 0xffffffff;

   hkout = xopen("hkout", "wb");
   dsum  = xopen("dsum",  "wb");
}

/* ************************************************************ */
void Open_deko(char * filename)
{
   char   buf[1024];
   STRING s;
   STRING * tarr;
   int    i, tcnt;

   init_globalx();

   if ( (nrm = fopen(filename, "rb") ) == NULL)
   {
      fprintf(stderr, " nrmrd.c : can't open %s\n", filename);
      pexit("can't open nrm");
   }

   if ( (rawout = fopen("rawout", "wb") ) == NULL)
   {
      fprintf(stderr, " nrmrd.c : can't open rawout\n");
      pexit("can't open rawout");
   }

   /* fseek(nrm, 0, 2); */
   fseek(nrm, 0, SEEK_SET);
   dh1.fsize = ftell(nrm);              /* global size */

   /* parse for Timer Resolution */
   if (fread(buf, 1024, 1, nrm));
   buf[1023] = '\0';
   tarr = tokenize(buf, &tcnt);
   for (i = 0; i < tcnt; i++)
   {
      if (strcmp(tarr[i], "Resolution:") == 0)
      {
         s = tarr[i + 1];
         while (*s != ']')
         {
            if (*s == '\0') *s = '-';
            s++;
         }
         *s = '\0';
         gv.Units = Remember(tarr[i + 1]);
         s++;
         i = tcnt;
      }
   }
   fseek(nrm, 0, SEEK_SET);
   dh1.next = 1024;                     /* skip header */
   OptVMsg(" nrmsize %d\n", dh1.fsize);
}

/* *************************************** */
/* method name => radtable */
/* DEPRECATED */
void proc_meth_b001(void)
{
   pMENT q = NULL;
   int rc = 0;

   /* mblk => mnm */
   /*
   q = (pMENT)find_radtbl(gv.mnm_anc, dh1.p[0], &rc, sizeof(MENT));
    */
   if (rc == 1) gv.mnmcnt++;            /* new */

   if (q)
   {
      q->mb = dh1.p[0];                 /* mb */
      q->mnm = Remember(dh1.s[0]);      /* method name */
   }
}

/* *************************************** */
/* method call or return. uses cnm & mnm tables */
/* DEPRECATED */
void arc_out(int opc, void *);
void proc30x(void)
{
   pMENT q = NULL;
   int ex = 0;
   struct HKDB * hp;
   int rc = 0;
   char mnm_buf[16];
   aint mblk;

   /* WARNING */
   /* major codes 0x30 & 0x50 will share this code */

   if ( (dh1.idmin & 0x80) == 0x80) ex = 1;   /* entry/exit flag */
   mblk = dh1.p[0];

   /* mblk to mnm only */
   /*
   q = (pMENT)find_radtbl(gv.mnm_anc, mblk, &rc, sizeof(MENT));
    */

   if (rc == 1)
   {                                    /* error. no mnm for this mblk ??? */
      gv.mnmcnt++;
      sprintf(mnm_buf, "mblk_%08x", mblk);
      if (q) q->mnm = Remember(mnm_buf);
   }

   /* change api: arc_mnm(q->mnm, ex) */
   /* more obj-like. let arcf dll handle hkdb_find & hp stuff */
   hp = hkdb_find(q->mnm, 1);
   arc_out(ex, hp);
}


/* ***************************************
 *  Deko Hook Format
 *  Pos  Bytes Function
 *  0    1     major id
 *  1    1     minor id
 *  2    1     hook length
 *  3    1     -
 *  4    6     time stamp
 *  a    1     duration length(L)
 *  b    L     duration(variable: 1-6)
 *  b+L  x     hook data
 * *************************************** */
int proc_nrm(STRING fname, int oopt)
{
   int    neof  = 0;
   int    done;                         /* tailored hk processing for af */
   int    daft;                         /* dispatch after hook */
   STRING buf;                          /* local ptr to hook data */

   char   ptbuf[16];                    /* buffer to convert binary pidtid to ascii */
   aint   tmppt;
   int *  tmp;

   gv.cnm_anc = (char **)zMalloc("CNM_Anchor",68);
   tmp = (int *)gv.cnm_anc;
   tmp[16] =  0xdeadbeef;

   gv.mnm_anc = (char **)zMalloc("MNM_Anchor",17 * sizeof(char *));
   tmp = (int *)gv.mnm_anc;
   tmp[16] =  0xdeadbeef;
   OptVMsg(" mnm_anc %p\n", gv.mnm_anc);

   Open_deko(fname);

   /* Process NRM File */
   while ( (dh1.next < (dh1.fsize - 8)) && (gv.hkstp > gv.hkccnt) && !neof)
   {
      gv.hkccnt++;

      fseek(nrm, dh1.next, 0);
      if (fread(&dh1, 10, 1, nrm));     /* sets part of global dh1 structure */

      if (gv.db == 10)
      {
         OptVMsg(" next %08x len %2x Maj %02x Min %02x Res %2x\n",
                 dh1.next, dh1.hklen, dh1.idmaj, dh1.idmin, dh1.cres);
      }

      if (dh1.hklen <= 10)
      {
         fprintf(stderr, " HOOK LENGTH ERROR : Len %d\n", dh1.hklen);
         fprintf(stderr, "  @ %08x\n", dh1.next);
         exit(-1);
      }

      /* skip these strace hooks */
      if ( (dh1.idmaj == 0x7f) ||
           ( (dh1.idmaj == 0xa8) && (dh1.idmin == 0x1) ) ||
           ( (dh1.idmaj == 0xa8) && (dh1.idmin == 0x2) ) )
      {
         dh1.next += dh1.hklen;
         continue;
      }

      if (gv.hkccnt >= gv.hkstt)
      {
         /* sets dh1.hp : hkdb ptr for deko/strace hooks */
         buf = hkparse1(nrm, oopt);     /* parse rest of dekko hook */
         if (dh1.eof == 1) break;
      }
      else
      {                                 /* skip hook */
         dh1.next += dh1.hklen;
         continue;
      }

      done = daft = 0;

      /* use arcflow acct-ing ?? look at arcf/jraw */
      if (gv_sup_comp == 1)
      {
         acct2();
      }

      switch ((unsigned char)dh1.idmaj)
      {
      case 0x12:                        /* dispatching */
         done = 1;                      /* disp handled diff */

         if (dh1.idmin == 1)
         {                              /* disp */
            done = 1;                   /* disp handled diff */

            if (dh1.idmin == 1)
            {
               tmppt = (dh1.p[0] << 16) + dh1.p[1];
               sprintf(ptbuf, "%08x", tmppt);
               gv.ptp = ptptr(ptbuf);
               arc_out(3, dh1.hp);      /* pt rec */
            }
         }
         break;

      case 0x30:                        /* method hooks (put in arcf's hkdb) */
         /* start at 1st method(until sup hook provided) */

         if (gv_sup_comp == 0)
         {
            gv_sup_comp = 1;
            acct2();
         }
         proc30x();
         break;

      case 0xb0:                        /* File System External(types) */
         proc_meth_b001();
         break;

      case 0x55:                        /* end of dekko file marker */
         done = 1;
         neof = 1;
         break;

      default:
         break;

      }                                 /* major switch */

      /* debug: Output raw hex hooks */
      if (gv.db == 11)
      {
         static xfirst = 0;
         int i;

         fprintf(rawout, "R %10.0f %08x ", gv.dtime, dh1.hkkey);
         for (i = 0; i < dh1.hklen - 10; i++)
         {
            fprintf(rawout, "%02x", dh1.buf[i]);
         }
         fprintf(rawout, "\n");
      }

      /* arcflow processing(flags set in switch) */
      if (done == 0)
      {
         if (dh1.hp->mat == 0) dh1.ex = 2;   /* unmatched impulse */

         /* This call is arcflowing other hooks that aren't
            explicitly arcflowed above
          */
         /* arc_out(dh1.ex, dh1.hp); */
      }

      dh1.next += dh1.hklen;
   }                                    /* while neof */

   acct2();
   OptVMsg(" end of proc_nrm\n");
   return(0);
}



