/*
 *
 *  Copyright (c) IBM Corporation 1996.
 *  Property of IBM Corporation.
 *  All Rights Reserved.
 *
 *  Origin 30 (IBM)
 */

/* 041198 - ignore 7fxx - cpu switch
     - ignore 8axx - javaos2 stuff
 */

#include "global.h"
#include "arc.h"
#include "jhash.h"

struct dekhk
{
   int  type;
   int  maj;
   aint min;
   int  hklen;
   int  time;

   char buf[16 * 1024];

   UNIT ntime;
   UNIT ts;                             /* first hook time */
   aint ex;                             /* entry/exit flag */
   struct HKDB * hp;                    /* ptr to hkdb entry for this hook */

   aint   wd[32];
   STRING str[32];

   /* nrm file variables */
   aint next;
   aint fsize;
};
struct dekhk dh;

aint  pidtid = 0;
int   amaj[257] = {0};
void  proc11(void);

/* ***************************************************** */
/* struct graw  gvr = {0}; */           /* clean up in init_global */
struct dekhk dh = {0};
double gv_suptime = 0;
int gv_sup_comp = 0;                    /* start up complete flag */

void acct2(void);

pTELEM gv_tb2tnm[256] = {0};
int    gv_tnmcnt = 0;

FILE * dbfd;

/* atas prototype */
char * geta2n(int pid, unsigned int address);

/************************/
/* CODE START */
/************************/

/************************/
/* jstem hdf files */
void proc_jhdf(char * fname)
{
   char  buf[512];
   char * mnm;
   FILE * hdf;
   char ** tarr;
   int tcnt;
   int maj, min;
   int len = 0;
   static int cnt = 0;
   /* jsmethod * mp; */

   /* jsmethod_table_init(); */

   if ( (hdf = fopen(fname, "rb") ) == NULL)
   {
      printf(" hdf.c : can't open %s\n", fname);
      pexit(" can't open hdf file");
   }

   while (fgets(buf, 256, hdf) != NULL)
   {
      cnt++;
      tarr = tokenize(buf, &tcnt);
      if (strcmp(tarr[0], "(") == 0)
      {
         if (strncmp(tarr[1], "0x", 2) == 0)
         {
            maj = strtol(tarr[1], NULL, 0);
            min = strtol(tarr[3], NULL, 0);
            mnm = &tarr[8][1];
            len = (int)strlen(mnm);
            mnm[len - 2] = 0;
         }
      }
   }
}

/* ************************************************************ */
void showthreads(void)
{
   pTELEM p;
   int i;

   for (i = 0; i < gv_tnmcnt; i++)
   {
      p = gv_tb2tnm[i];
      if (p != 0)
      {
         fprintf(dbfd, " THRD_IND %d %x %s\n", i, p->tblk, p->tnm);
      }
   }
}

/* ************************************************************ */
/*
   tnm != NULL => insert
   tnm == NULL => lookup
   insert sorted by tb
 */
/* ************************************************************ */
STRING find_thread(aint tb, STRING tnm)
{
   pTELEM p;
   static char rbuf[64];
   int i, j;

   fprintf(dbfd, " find_thread %x <%s>\n", tb, tnm);

   for (i = 0; i < gv_tnmcnt; i++)
   {
      p = gv_tb2tnm[i];

      if (tb >= p->tblk)
      {
         if (tb == p->tblk)
         {                              /* found thread block */
            /* if tnm specified => change tnm */
            if (tnm[0] != '\0')
            {
               sprintf(rbuf, "%s", tnm);
               p->tnm  = Remember(tnm);
            }
            else
            {                           /* create tnm */
               sprintf(rbuf, "_tnm_0x%08x", tb);
            }
            return(p->tnm);
         }
      }
      else
      {                                 /* tb < p->tblk */
         /* move rest down 1 slot */
         for (j = gv_tnmcnt; j > i; j--)
         {
            gv_tb2tnm[j] = gv_tb2tnm[j - 1];
         }
         break;
      }
   }

   /* new entry @ i */
   p = (pTELEM)zMalloc("thrd elem",sizeof(TELEM));
   gv_tb2tnm[i] = p;

   if (tnm[0] != '\0')
   {                                    /* tnm given */
      sprintf(rbuf, "%s", tnm);
   }
   else
   {                                    /* create tnm */
      sprintf(rbuf, "_tnm_0x%08x", tb);
   }

   p->tblk = tb;
   p->tnm  = Remember(rbuf);

   gv_tnmcnt++;

   /* showthreads(); */
   return(p->tnm);
}

/* ************************************************************ */
int dispthrd(aint tb)
{
   STRING tnm;
   struct HKDB * hp;

   tnm = find_thread(tb, "");           /* tb => tnm */
   gv.ptp = ptptr(tnm);                 /* find/add pidtid */
   gv.apt = gv.ptp->apt;
   hp = hkdb_find(tnm, 1);
   arc_out(3, hp);

   return(0);
}

/* ************************************************************ */
int hkparse(void)
{
   static first = 0;
   int i, fcnt = 0, done;
   int cnt4   = 0;                      /* cnt for fixed length(4 byte) items */
   int cnts   = 0;                      /* cnt for str parms */
   aint hightm, lowtm;
   static UNIT twopower32 = 4096.0 * 1024.0 * 1024.0;

   STRING buf;
   int slen, len;
   int rc2;

   fseek(nrm, dh.next, 0);

   if (gv.hkccnt % 50000 == 0)
   {
      fprintf(stderr, "\n Hooks Processed  %8d @ %.0f\n",
              gv.hkccnt, gv.dtime);
   }

   /* type/len(2) maj(2) min(4) */
   rc2 = (int)fread(&dh.buf[0], 12, 1, nrm);
   buf = &(dh.buf[0]);

   dh.hklen = (int)RD16REV(*(usint *)buf);
   buf += 2;
   dh.type = dh.hklen & 0x3;
   dh.hklen = 0xfffffffc & dh.hklen;
   dh.maj = RD16REV(*(usint *)buf);
   buf += 2;

   dh.min = RD32REV(*(int *)buf);
   buf += 4;

   /* histogram of majors */
   if (dh.maj < 256)
   {
      amaj[dh.maj]++;
   }
   else
   {
      amaj[256]++;
   }

   /* low time stamp */
   lowtm = RD32REV(*(aint *)buf);
   buf += 4;                            /* buf empty */

   if (fread(&dh.buf[12], dh.hklen - 12, 1, nrm));   /* rest of hook */

   if ( (dh.maj == 0x7f) || (dh.maj == 0xa8) )
   {
      /* 7f/xx - ???  a8/02 - ??? */
      if (dh.maj == 0xa8)
      {
         if (dh.min == 0x1)
         {
            hightm = RD32REV(*(aint *)buf);
            gv.hightm = (UNIT)hightm;
            OptVMsg(" timeRollOver %d %0x8\n",
                    hightm, hightm);
         }
      }
      OptVMsg(" maj = %04x min = %04x\n", dh.maj, dh.min);
   }

   /* high time changes above */
   dh.ntime = gv.hightm * twopower32 + (UNIT)lowtm ;

   /* this establishes the time reference */
   if (first == 1)
   {
      gv.odtime = gv.dtime;
      gv.dtime = dh.ntime - dh.ts;
   }
   else
   {
      OptVMsg(" init time on first hook %8.0f\n", dh.ntime);
      first = 1;
      dh.ts = dh.ntime;                 /* time of first hook */
      gv.dtime = 0;
   }

   dh.ex = (dh.min >> 31) & 0x1;        /* entry/exit flag */

   if (gv.hkccnt < gv.hkstt)
   {
      dh.next += dh.hklen;
      return(1);                        /* skip til @ gv.hkstt-th record */
   }

   /* change to time since arcflow start(supcomp hook) */
   if (gv.db == 22)
   {
      fprintf(gv.fdb, " ### time %8.0f hkno %8d nx %6x len %x %2x:%02x",
              gv.dtime - gv_suptime, gv.hkccnt, dh.next, dh.hklen,
              dh.maj, dh.min);
   }

   /* NO LOCAL HKDB for STRACE2 in ARCF !! */
   /* Strace/Dekko: see nrmrd.c & nrmhdf.c for hdf & HKDBx stuff */

   /* if not in hkdb => create & add(null description) */
   /* dh.hp = hp = hkdb_ptr(dh.hkkey, 1); */
   /* hp->cnt++; */                     /* hook count */

   /* parm parsing */
   len = 12;
   slen = 0;
   gv.hdstr[0] = CZERO;

   if (dh.type == 0)
   {
      fcnt = (dh.hklen - len) / 4;
   }
   else if (dh.type == 1)
   {
      fcnt = 0;
   }
   else if (dh.type == 2)
   {
      fcnt = RD32REV(*(aint *)buf);
      buf += 4;
      len += 4;
   }

   done = 0;
   while (dh.hklen > len)
   {
      int res;

      if (fcnt >= 1)
      {                                 /* fixed 4 byte fields */
         fcnt--;
         dh.wd[cnt4] = RD32REV(*(aint *)buf);
         if (gv.db == 22)
            fprintf(gv.fdb, " %8x", dh.wd[cnt4]);
         buf += 4;
         len += 4;
         cnt4++;
      }
      else
      {
         char c;

         slen = RD32REV(*(aint *)buf);
         buf += 4;
         len += 4 + slen;
         dh.str[cnts] = zMalloc("var-hk-field",slen + 1);
         for (i = 0; i < slen; i++)
         {
            c = *buf++;
            if ( (c == '"') || (c == ' ') ) c = '_';
            dh.str[cnts][i] = c;
         }
         dh.str[cnts][slen] = CZERO;    /* stop str parms */
         res = slen % 4;
         if (res != 0)
         {                              /* next 4 byte boudary */
            buf += 4 - res;
            len += 4 - res;
         }
         if (gv.db == 22)
            fprintf(gv.fdb, " <%s>", dh.str[cnts]);
         cnts++;
      }
   }

   if (gv.db == 22)
      fprintf(gv.fdb, "\n");

   /* raw hex hook output */
   if (gv.db == 22)
   {
      int i;
      for (i = 0; i < dh.hklen; i++)
      {
         if (gv.db == 22)
         {
            if (0 == i % 16) fprintf(gv.fdb, "\n");
            fprintf(gv.fdb, " %02x", dh.buf[i]);
         }
      }
   }
   return(0);
}

/* ************************************************************ */
void Open_jraw(char * filename)
{
   char buf[512];
   int  len;
   int  type = 0;

   if (xflow != NULL)
   {
      dbfd = xflow;
   }
   else
   {
      dbfd = stdout;
   }

   if ( (nrm = fopen(filename, "rb") ) == NULL)
   {
      fprintf(stderr, " jraw.c : can't open %s\n", filename);
      pexit("can't open nrm");
   }

   if ( (gv.fdb = fopen("debug", "w") ) == NULL)
   {
      fprintf(stderr, " jraw.c : can't open %s\n", "debug");
      pexit("can't open debug");
   }

   fseek(nrm, 0, SEEK_END);
   dh.fsize = ftell(nrm);               /* global size */

   fseek(nrm, 0, SEEK_SET);

   /* read 8 byte signature */
   if (fread(buf, 128, 1, nrm));        /* type/len(2) maj(2) min(4) */

   if (strncmp(buf, "JAVAOSTR", 8) == 0)
   {
      ;
      dh.next = 16;

      OptVMsg(" NEW format\n");
      while (type != 3)
      {
         fseek(nrm, dh.next, SEEK_SET);
         if (fread(buf, 8, 1, nrm));    /* length(4) type(4) 1-trailer 2-header*/
         /* 3-trace */
         len = RD32REV(*(int *)buf);
         type = RD32REV(*(int *)(buf + 4));

         OptVMsg(" len %d type %d\n", len, type);

         if (type != 3)
         {
            dh.next += len;
         }
         else
         {
            dh.next += 8;
            dh.fsize = dh.next + len - 8;
            return;
         }
      }
   }
   else
   {
      int units;

      units = RD32REV(*(int *)(buf + 84));

      if (units == 0)
      {
         gv.Units = Remember("Cycles");
      }
      else if (units == 1)
      {
         gv.Units = Remember("Instructions");
      }
      else
      {
         fprintf(stderr, " Units NOT set by LOG. units %d\n", units);
      }

      OptVMsg(" javaosbin units %s\n", gv.Units);

      fseek(nrm, 0, SEEK_SET);
      dh.next = 516;                    /* skip header */
      OptVMsg(" nrmsize %d\n", dh.fsize);
   }
}

/* *************************************** */
/* add a class name entry in radtable */
void proc11(void)
{
   pCLENT p = NULL;
   int rc = 0;

   switch (dh.min)
   {
   case 0xb0:                           /* startup class */
   case 0xc0:                           /* load class */

      /*
      p = (pCLENT)find_radtbl(gv.cnm_anc, dh.wd[0], &rc, sizeof(CLENT));
       */
      if (rc == 1)
      {
         gv.cnmcnt++;
      }
      else
      {                                 /* replace method name at previously used addr */
         /* collision ?? */
         OptVMsg(" tm %8.0f Method Addr Reuse : %s\n",
                 gv.dtime, dh.str[0]);
      }
      p->cb = dh.wd[0];
      p->cnm = Remember(dh.str[0]);

      if (gv.db == 22)
         fprintf(gv.fdb, " cb %x cnm %s\n", p->cb, p->cnm);
      break;

   case 0xb3:                           /* startup complete - jit begin */
      gv_sup_comp = 1;
      gv_suptime = gv.dtime;

      /* disp init thrd */
      gv.ptp = ptptr(gv.apt);           /* find/add pidtid */
      arc_out(3, NULL);

      fprintf(dbfd, " STARTUP COMPLETE : hkcnt %d %s\n",
              gv.hkccnt, gv.ptp);

      acct2();
      break;

   default:
      break;
   }
}

/* *************************************** */
/* add a method name entry in radtable */
void proc_method(void)
{
   pCLENT p = NULL;
   pMENT  q = NULL;
   char   mbuf[512];
   int    rc = 0;

   /* cblk => cnm */
   /*
   p = (pCLENT)find_radtbl(gv.cnm_anc, dh.wd[1], &rc, sizeof(CLENT));
    */

   if (rc == 1)
   {                                    /* new cnm */
      strcpy(mbuf, "null_cnm_");        /* error */
   }
   else
   {
      if (gv.db == 23)
      {
         OptVMsg(" %d : p->cnm %p <%s>\n",
                 gv.hkccnt, p->cnm, p->cnm);
      }
      strcpy(mbuf, p->cnm);
   }

   /* mblk => mnm */
   /*
   q = (pMENT)find_radtbl(gv.mnm_anc, dh.wd[0], &rc, sizeof(MENT));
    */
   if (rc == 1)
   {                                    /* new */
      gv.mnmcnt++;
   }

   /* this code will step on the previous mblk_address name */

   q->mb = dh.wd[0];                    /* mb */
   strcat(mbuf, ".");
   strcat(mbuf, dh.str[0]);             /* method */
   strcat(mbuf, dh.str[1]);             /* signature */
   q->mnm = Remember(mbuf);             /* method name */
}

/* *************************************** */
/* process interrupts */
void proc4(void)
{
   struct HKDB * hp;
   static STRING ptsav = "UnkThrd";     /* pidtid entering interrupts */

   STRING ithread = {"_interrupt_"};    /* interrupt thread */
   int idepth = dh.wd[1];

   if ( (dh.min & 0x80000000) == 0)
   {                                    /* entry */
      if (idepth == 1)
      {
         ptsav = gv.apt;                /* save for interrupt exit */
         fprintf(dbfd, " interENTRY %s => %s\n", ptsav, ithread);

         gv.apt = ithread;
         gv.ptp = ptptr(ithread);       /* find/add pidtid */
         hp = hkdb_find(ithread, 1);

         arc_out(3, hp);
      }
   }

   else
   {                                    /* exit */
      /* need to see nested interrupt example */
      if (idepth == 1)
      {                                 /* return to prev thread */
         gv.apt = ptsav;
         gv.ptp = ptptr(gv.apt);        /* find/add pidtid */
         hp = hkdb_find(gv.apt, 1);

         fprintf(dbfd, " interEXIT %s => %s\n", ithread, ptsav);
         arc_out(3, hp);
      }
   }
}

/* *************************************** */
/* process MTE hook */
void proc19(void)
{

   int pid      = dh.wd[0];
   int seg_addr = dh.wd[1];
   int seg_len  = dh.wd[2];
   int seg_flgs = dh.wd[3];
   int chksum   = dh.wd[4];
   int modtstp  = dh.wd[5];

   OptVMsg(" MTE: maj %x min %x\n", dh.maj, dh.min);

   OptVMsg(" pid      %8x\n", pid     );
   OptVMsg(" seg_addr %8x\n", seg_addr);
   OptVMsg(" seg_len  %8x\n", seg_len );
   OptVMsg(" seg_flg  %8x\n", seg_flgs);
   OptVMsg(" chksum   %8x\n", chksum  );
   OptVMsg(" modts    %8x\n", modtstp );

   OptVMsg(" segnm   %s\n", dh.str[0]);
   OptVMsg(" modnm   %s\n", dh.str[1]);
}

/* *************************************** */
/* jstem call or return */
void proc_jstem(int dir, char * mnm)
{
   struct HKDB * hp;

   hp = hkdb_find(mnm, 1);              /* this builds HKDB */
   arc_out(dir, hp);                    /* push or pop mnm */
}

/* dummy proc_jraw */
void proc_jraw(STRING fname)
{
   fname = NULL;
}

