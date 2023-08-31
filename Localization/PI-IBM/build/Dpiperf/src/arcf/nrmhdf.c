/*
 *
 *  Copyright (c) IBM Corporation 1996.
 *  Property of IBM Corporation.
 *  All Rights Reserved.
 *
 *  Origin 30 (IBM)
 *
 */

#include "nrm.h"

#define  HKDBxSZ  8 * 1024
struct HKDBx * hkdbx[HKDBxSZ];

FILE * xopen(STRING fn, STRING opt);
struct HKDBx * hkdb_ptr(aint hkkey, int flg);
struct HKDBx * const_hkdbx(int db);
struct HKDBx * add_hookx(aint key);

void   calc_hstr2(struct HKDBx * hp);
void   init_globalx(void);

int LKCONV qcmp_hkdb(const void * p1, const void * p2);   /* * _Optlink */
int LKCONV bs_hkx(const void * a, const void * p);

FILE * dsum;                            /* dekko summary output */
FILE * hkout;                           /* dekko summary output */

/* (comm.c instantiates) struct Global gv = {0}; */

/* file global for hdf.c */
int g_hkcnt = 0;

/* *************************************
 *  Read Input File & HDF Files
 *  Process Hooks
 * ************************************* */

/* ************************************************************ */
int sorthooks(void)
{
   qsort((void *)hkdbx, g_hkcnt, sizeof(struct HKDBx *), qcmp_hkdb);
   return(0);
}

/* ************************************************************ */
int LKCONV qcmp_hkdb(const void * p1, const void * p2)
{
   struct HKDBx * r;
   struct HKDBx * s;

   r = *(struct HKDBx **)p1;
   s = *(struct HKDBx **)p2;

   if (r->hkkey >= s->hkkey)
   {
      if (r->hkkey > s->hkkey)
      {
         return(1);
      }
      else return(0);
   }
   else return(-1);
}

/* ************************************************************ */
int LKCONV bs_hkx(const void * pkey, const void * ptr)
{
   struct HKDBx * hp;
   aint key;

   key = *(aint *)pkey;
   hp = *(struct HKDBx **)ptr;

   if (hp == NULL)
   {
      OptVMsg(" gv.apt    %s\n", gv.ptp->apt);
      OptVMsg(" gv.dtime  %10.0f\n", gv.dtime);
      OptVMsg(" gv.hkccnt %10d\n", gv.hkccnt);
      exit(0);
   }

   if (key > hp->hkkey) return(1);
   if (key < hp->hkkey) return(-1);
   return(0);
}

/* ************************************************************ */
struct HKDBx * hkdb_ptr(aint hkkey, int flg)
/* flag(0) => exit , flag(1) => add hook if missing */
{
   struct HKDBx * hp;
   struct HKDBx * hp2;
   struct HKDBx ** php;

   php = (struct HKDBx **)bsearch((void *)&hkkey, (void *)hkdbx,
                                  g_hkcnt, sizeof(struct HKDBx *), bs_hkx);

   if (php != NULL)
   {
      hp = *php;
   }
   else
   {
      hp = 0;
   }

   if (flg == 1)
   {                                    /* 0 => exit , 1 => add hook if missing */
      if (hp == 0)
      {
         fprintf(hkout, "hkdb_ptr: NO HDF ENTRY FOR %08x g_hkcnt %d\n",
                 hkkey, g_hkcnt);
         fflush(hkout);

         if ( (hkkey & 0x80) == 0)
         {                              /* entry hook */
            hp = add_hookx(hkkey);
         }

         else
         {                              /* exit hook */
            int key;

            key = hkkey & 0xffffff7f;   /* matching entry key */

            php = (struct HKDBx **)bsearch((void *)&key, (void *)hkdbx,
                                           g_hkcnt, sizeof(struct HKDBx *), bs_hkx);

            if (php != NULL)
            {                           /* found matching entry hook */
               hp2 = *php;
               hp = add_hookx(hkkey);
               hp->mat = hp2->mat;      /* set exit mat to entry mat */

               hp->hstr = hp2->hstr;    /* use entry name */
            }
            else
            {                           /* no matching entry hook found */
               hp = add_hookx(hkkey);
               /* Mystery. Why did I set mat = 1 matched ??? */
               /* hp->mat  = 1; */      /* matched */
               hp->mat  = 0;            /* not matched */
            }
         }
      }
   }

   return(hp);
}

/* ************************************************************ */
void calc_hstr2(struct HKDBx * hp)
{
   int len;

   len = (int)strlen(hp->hstr);
   hp->hstr2 = xMalloc( "hstr2", len + 10 );

   if (hp->hkkey != 0)
   {
      if ((hp->hkkey & 0x7f) == 0) len = 8;
      else len = 4;
      sprintf(hp->hstr2, "%0*x_%s", len, hp->hkkey, hp->hstr);
   }
   else
   {                                    /* pidtid */
      hp->hstr2 = hp->hstr;
   }
}

/* ************************************** */
/* HKDBx CONSTRUCTOR */
/* db: 1 => add to hkdbx, 0 =>  return ptr only */
struct HKDBx * const_hkdbx(int dbase)
{
   struct HKDBx * hp;

   if (g_hkcnt >= HKDBxSZ)
   {
      panic("HKDBx Over Size Limit");
   }

   hp = (struct HKDBx *) zMalloc("HKDBx entry",sizeof(struct HKDBx));

   if (dbase == 1)
   {                                    /* add to hkdbx */
      hkdbx[g_hkcnt] = hp;
      g_hkcnt++;                        /* entry in hkdbx */
      if ( (g_hkcnt % 1000) == 0)
      {
         OptVMsg(" strace g_hkcnt %d\n", g_hkcnt);
      }
   }
   hp->hstr = gv.nstr;

   return(hp);
}

/* ************************************************************ */
int getdur(char * buf)
{
   int leng;

   leng = buf[0];
   return(leng + 1);                    /* buf positions to skip duration */
}

/* ************************************************************ */
/* Parsing Hooks From NRM File in Real Time */
/* ************************************************************ */
STRING hkparse1(FILE * nrm, int oopt)
{
   static first = 0;
   int i, j, k;
   int pos;
   int cnt4   = 0;                      /* cnt for byte, short & int parms */
   int cnts   = 0;                      /* cnt for str parms */
   int np     = 0;                      /* 1 => not parsed */
   int repcnt = 0;

   char c;
   STRING buf;                          /* local dh.buf ptr */
   struct HKDBx * hp;
   int byrd;
   int slen;

   if (gv.hkccnt % 50000 == 0)
   {
      fprintf(stderr, " Hooks Processed  %8d @ %.0f\n",
              gv.hkccnt, gv.dtime);
   }

   /* 6 byte time stamp */
   /* dh1.ntime = RD32REVB(*(int *)&dh1.ts36);*/   /* big endian */
   dh1.ntime = (UNIT)dh1.ts16[0];

   for (i = 1; i < 6; i++)
   {
      dh1.ntime = 256 * dh1.ntime + (UNIT)dh1.ts16[i];
   }

   if (first == 1)
   {
      gv.odtime = gv.dtime;
      gv.dtime = dh1.ntime - dh1.ts;
   }
   else
   {
      first = 1;
      dh1.ts = dh1.ntime;               /* time offset for first hook */
      gv.dtime = 0;
   }

   /* Rest of Hook */

   if ((byrd = (int)fread(dh1.buf, dh1.hklen - 10, 1, nrm) ) != 1)
   {
      /* EOF */
      gv.dtime = gv.odtime;             /* EOF stepped on gv.dtime */
      OptVMsg(" EOF: SIZE %x NEXT %x\n", dh1.fsize, dh1.next);

      dh1.eof = 1;
      return(dh1.bptr);
   }
   dh1.buf[dh1.hklen - 10] = 0;         /* to prevent runaway %p */

   pos = getdur(dh1.buf);               /* 1 + duration bytes */
   buf = dh1.bptr = dh1.buf + pos;      /* Set Global dh1.bptr */
   /* so nrmrd.c can parse from here */

   dh1.ex   = (dh1.idmin >> 7) & 0x1;   /* entry/exit flag */
   dh1.hkx  = 256 * dh1.idmaj + dh1.idmin;   /* hk table index */

   /* double byte minors */
   if ((dh1.idmin == 0) || ((unsigned char)dh1.idmin == 0x80) )
   {
      dh1.dbm = RD32REVB(*(int *)buf);  /* big endian */
      buf += 4;
   }
   else dh1.dbm = 0;

   dh1.hkkey = (dh1.dbm << 16) + dh1.hkx;   /* 32 bit hkdbx key */

   /* if not in hkdbx => create & add(null description) */
   dh1.hp = hp = hkdb_ptr(dh1.hkkey, 1);
   hp->cnt++;                           /* hook count */

   /* parm parsing. nrmrd.c may also parse */
   slen = 0;
   gv.hdstr[0] = '\0';

   /* get hdf description */
   for (i = 0; i < hp->pcnt; i++)
   {
      repcnt = 1;
      c = hp->parms[i];

      switch (c)
      {
      /* case '1': */
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
         repcnt = (int)c - (int)'0';
         i++;
         c = hp->parms[i];
         break;

      default:
         break;
      }

      for (j = 1; j <= repcnt; j++)
      {
         switch (c)
         {
         case 'a':
         case 'd':
         case 'f':
            dh1.p[cnt4++] = RD32REV(*(int *)buf);
            if (oopt)
            {
               slen += sprintf(&gv.hdstr[slen], " %s %x",
                               hp->pstr[i], dh1.p[cnt4-1]);
            }
            buf += 4;
            break;

         case 'b':
            dh1.p[cnt4++] = (aint)*buf;
            if (oopt)
            {
               slen += sprintf(&gv.hdstr[slen], " %s %x",
                               hp->pstr[i], dh1.p[cnt4-1]);
            }
            buf++;
            break;

            /* pascal string. 1st byte is length */
            /* ??? Apparently "p" is being used as null term string ?? */
            /* case 'l': */
            /* case 'p': */
         case 'Z':                      /* fake case to nop below code */
            strncpy(dh1.s[cnts], buf + 1, j);
            if (oopt)
            {
               slen += sprintf(&gv.hdstr[slen], " %s %s",
                               hp->pstr[i], dh1.s[cnts]);
            }

            cnts++;

            buf += j + 1;
            break;

         case 'l':
         case 'p':
         case 's':
            strcpy(dh1.s[cnts], buf);   /* Q */
            buf += strlen(dh1.s[cnts]) + 1;
            if (oopt)
            {
               slen += sprintf(&gv.hdstr[slen], " %s %s",
                               hp->pstr[i], dh1.s[cnts]);
            }
            cnts++;
            break;

            /* hex string. 1st byte is length */
         case 'u':
            j = (int)*buf;              /* length of hex string */
            if (oopt)
            {
               slen += sprintf(&gv.hdstr[slen], " %s ", hp->pstr[i]);
               for (k = 1; k <= j; k++)
               {
                  slen += sprintf(&gv.hdstr[slen], "%02x", *(buf + k));
               }
            }
            cnts++;

            buf += j + 1;
            break;

         case 'w':
            dh1.p[cnt4++] = RD16REV(*(usint *)buf);
            if (oopt)
            {
               slen += sprintf(&gv.hdstr[slen], " %s %x",
                               hp->pstr[i], dh1.p[cnt4-1]);
            }
            buf += 2;
            break;

         case 'x':
            buf++;
            break;

         default:
            np = 1;
            break;
         }
      }
   }

   if (oopt)
   {
      sprintf(&gv.hdstr[slen], "\0");
      OptVMsg(" %10.0f %08x %s\n", gv.dtime, dh1.hkx, gv.hdstr);
   }

   /* first occurance of each unique hook */
   if (hp->first == 0)
   {
      hp->first = 1;
      if (np == 1)
      {
         fprintf(dsum, " NOT PARSED hkx: %04x hstr: %20s ",
                 dh1.hkx, hp->hstr);

         for (i = 0; i < hp->pcnt; i++)
         {
            fprintf(dsum, "%c", hp->parms[i]);
         }
         fprintf(dsum, "\n");
      }
      fprintf(dsum, " 1st: %8.0f %8d %08x\n", gv.dtime, gv.hkccnt, hp->hkkey);
   }

   return(dh1.bptr);
}

/* ************************************************************ */
void hkshow(void)
{
   int i;
   struct HKDBx * hp;
   int lev = 1;

   fprintf(hkout, "\n SHOW HKDBx\n");
   if (lev == 1)
   {
      fprintf(hkout, " %4s %8s %4s %4s %s\n",
              "i", "key", "mat", "pcnt", "hname");

      for (i = 0; i < g_hkcnt; i++)
      {
         hp = hkdbx[i];
         fprintf(hkout, " %4d", i);
         fprintf(hkout, " %08x", hp->hkkey);
         fprintf(hkout, " %4x", hp->mat);
         fprintf(hkout, " %4d", hp->pcnt);
         fprintf(hkout, " %s", hp->hstr);
         fprintf(hkout, "\n");
      }
   }
}

/************************************************************* */
/* Process HDF Files */
/************************************************************* */
void proc_hdf(STRING fname)
{
   char  buf[256];
   char  * ip;
   char  * nmp;
   char  c;
   char  strhk[8];                      /* convert hex hook data to int */

   int   i;
   int   intc;                          /* c as an int */
   aint  hkid;
   int   state;                         /* 0 looking for hk, 1 for * or ?, 2 looking for data */

   FILE * hdf;
   struct HKDBx * hp = NULL;

   if ( (hdf = fopen(fname, "rb") ) == NULL)
   {
      printf(" hdf.c : can't open %s\n", fname);
      pexit(" can't open hdf file");
   }

   strhk[2] = '\0';
   state = 0;

   while (fgets(buf, 256, hdf) != NULL)
   {
      int    eq = 0;                    /* = in % string */
      char * tptr = NULL;               /* token desc ptr(start of !white before = ) */
      int    wsp = 0;                   /* white space */

      ip = buf;
      c = ip[0];

      /* PARSE DATA OF HOOK RECORD */
      if (ip[0] == ' ')
      {
         if (state == 1)
         {
            /* parse for % parms */
            while ((c = *ip) != '\0')
            {
               if (hp->pcnt < 20)
               {
                  switch (c)
                  {
                  case ' ':
                  case '\t':
                     wsp = 1;
                     break;

                  case '=':
                     eq = 1;
                     *ip = '\0';        /* stop the token string */
                     break;

                  case '%':
                     intc = (aint)(*(ip + 1)) - (aint)'0';
                     /* Note: %1x => 1 disappears, x => parm stack */
                     if ( (intc >= 1) && (intc <= 9) )
                     {
                        if (intc >= 2)
                        {               /* keep N(2-9) in parm stream */
                           hp->parms[hp->pcnt] = *(ip + 1);
                           hp->pstr[hp->pcnt] = gv.nstr;
                           hp->pcnt++;
                        }
                        ip++;
                     }

                     hp->parms[hp->pcnt] = *(ip + 1);
                     if (eq == 1)
                     {
                        hp->pstr[hp->pcnt] = Remember(tptr);
                     }
                     hp->pcnt++;
                     eq = 0;
                     break;

                  default:
                     if (wsp == 1)
                     {
                        tptr = ip;      /* possible beginning of token str */
                        wsp = 0;
                     }
                     break;
                  }                     /* end switch */

                  if (c == '\n') *ip = '\0';   /* ??? */
                  if (c == '\r') *ip = '\0';   /* ??? */
                  ip++;
               }
               else
               {
                  assert(hp->pcnt < 20);
                  break;
               }
            }
         }
      }

      /* RECOGNIZE START OF HOOK RECORD */
      if ( (ip[0] == '*') || (ip[0] == '?') )
      {
         if ( !((ip[2] == 'C') && (ip[3] == 'H')) )
         {                              /* CHANGE recs */
            state = 1;                  /* look for data */

            hp = const_hkdbx(1);        /* create new hkdbx entry */
            if (*ip == '*') hp->mat = 1;

            /* Convert ascii hkid with strtoi */
            strhk[0] = ip[2];
            strhk[1] = ip[3];
            strhk[2] = '\0';
            hp->mj = strtoul(strhk, NULL, 16);
            strhk[0] = ip[5];
            strhk[1] = ip[6];
            strhk[2] = '\0';
            hp->mn = strtoul(strhk, NULL, 16);
            hkid = (hp->mj << 8) + hp->mn;

            /* Double Byte Minors */
            hp->dbm = 0;
            ip = buf + 8;
            if ( (hp->mn == 0) || (hp->mn == 0x80) )
            {
               for (i = 0; i < 4; i++)
               {
                  strhk[i] = ip[i];
               }
               strhk[4] = '\0';
               hp->dbm = strtoul(strhk, NULL, 16);
               ip = buf + 13;
            }

            hp->hkkey = (hp->dbm << 16) + (hp->mj << 8) + hp->mn;

            /* Hook Name: " " => "_". Terminate @ \n or \0 */
            nmp = ip;                   /* start of name */
            while ((c = *ip) != '\0')
            {
               if (c == ' ')  *ip = '_';
               if (c == '\n')
               {
                  *ip = '\0'; break;
               }
               if (c == '\r')
               {
                  *ip = '\0'; break;
               }
               ip++;
            }
            hp->hstr = Remember(nmp);
            /* HSTR2 Here */
            calc_hstr2(hp);
         }
         else state = 0;
      }
   }
   if (gv.db == -1) hkshow();
}

/* ************************************************************ */
/* add a hook that has no HDF entry */
struct HKDBx * add_hookx(aint key)
{
   struct HKDBx * hp;
   struct HKDBx * hpt;
   aint   hkkey;
   char   str[16];
   int i;

   hp = const_hkdbx(1);                 /* create new hkdbx entry */
   /* g_hkcnt++ */
   hp->mj    = (key >> 8) & 0xff;
   hp->mn    = key & 0xff;
   hp->dbm   = (key >> 16) & 0xffff;
   hp->hkkey = key;
   hp->mat   = 0;                       /* unmatched */

   /* key(wo exit bit) => ascii name */
   sprintf(str, "%08x", key & 0xffffff7f);
   hp->hstr = Remember(str);
   /* HSTR2 Here */
   calc_hstr2(hp);
   hp->pcnt = 0;

   /* insert sort */
   hkkey = hp->hkkey;
   for (i = g_hkcnt - 1; i > 0; i--)
   {
      hpt = hkdbx[i - 1];
      if (hkkey < hpt->hkkey)
      {
         hkdbx[i] = hpt;
      }
      else
      {
         hkdbx[i] = hp;
         break;
      }
   }
   return(hp);
}

/* **************************************
 * Clean up hdf input
 *   - dups
 *   - entry/exit name inconsistancy
 * ************************************** */
void hkclean()
{
   struct HKDBx * hp;
   struct HKDBx * hpn;
   int dcnt = 0;
   int i, j;

   hkshow();

   /* sort */
   qsort((void *)hkdbx, g_hkcnt, sizeof(struct HKDBx *), qcmp_hkdb);

   /* mark dups: hkkey = 0 */
   for (i = 1; i < g_hkcnt; i++)
   {
      hp = hkdbx[i];
      if (i < (g_hkcnt - 1))
      {
         hpn = hkdbx[i + 1];
         if (hp->hkkey == hpn->hkkey)
         {
            hp->hkkey = 0;
            dcnt++;
         }
      }
   }

   /* eliminate dups */
   for (i = 1, j = 1; i < g_hkcnt; i++)
   {
      hp = hkdbx[i];
      if (hp->hkkey != 0)
      {
         hkdbx[j] = hp;
         j++;
      }
   }
   g_hkcnt = g_hkcnt - dcnt;

   /* Find/Fix Entry/Exit Name Errors */
   for (i = 0; i < g_hkcnt; i++)
   {
      aint hkkey, hkkey2;

      hp    = hkdbx[i];
      hkkey = hp->hkkey;

      if ( (hkkey & 0x80) == 0)
      {                                 /* entry hook */
         hkkey2 = hkkey + 0x80;
         hpn    = hkdb_ptr(hkkey2, 0);  /* ignore if not found */
         if (hpn != 0)
         {
            /* check exit name consistancy */
            if (strcmp(hp->hstr, hpn->hstr) != 0)
            {
               /* show to HDF CZAR */
               fprintf(stderr, "\n HDF ERROR: Entry/Exit Names Different\n");
               fprintf(stderr, "   %08x <%s>\n   %08x <%s> \n",
                       hp->hkkey,  hp->hstr, hpn->hkkey, hpn->hstr);
               /* Fix It for output */
               hpn->hstr = hp->hstr;
            }
         }
      }
   }
}
