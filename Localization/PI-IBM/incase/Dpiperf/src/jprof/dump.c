/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2009
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "jprof.h"
#include "tree.h"

/* Fields */
typedef struct
{
   char * name;                         /* filed name */
   jint type;                           /* type */
} field_t;

#define WRITE_BUF_SIZE 2048

static int    buf_index = 0;
static char * write_buf;

char arrty2str[16][16] =
{
   "OBJECT",
   "",
   "CLASS",
   "",
   "BOOLEAN",
   "CHAR",
   "FLOAT",
   "DOUBLE",
   "BYTE",
   "SHORT",
   "INT",
   "LONG"
};

char * dp;                              /* dump pointer */

typedef struct __jdata jdata_t;

struct __jdata
{
   double       miss;
   double       util;
   double       averht;
   int          gets;
   int          rec;
   int          nrec;
   int          fast;
   int          slow;
   int          tier2;
   int          tier3;
   char       * mnm;
   unsigned int htlo;
   unsigned int hthi;
   double       dhtm;
   jlong        tag;
};

// foot print (32 bit only)
int foot[1024] = { 0};                  // 4 Mbytes resolution

/* ************************************************************ */
int qcmp_jlm(const void * p1, const void * p2)
{
   jdata_t * r;
   jdata_t * s;

   r = *(jdata_t **)p1;
   s = *(jdata_t **)p2;

   // down
   if (r->miss > s->miss) return(-1);
   if (r->miss < s->miss) return(1);

   if (r->gets > s->gets) return(-1);
   if (r->gets < s->gets) return(1);

   return(0);
}

/******************************/
static void dump_read(void * dp, void * res, int size)
{
   memcpy(res, dp, size);
   return;
}

/******************************/
static short dump_read_u2(char * dp)
{
   short temp;

   dump_read((void *)dp, (void *)&temp, 2);
   temp = ntohs(temp);
   return(temp);
}

/******************************/
static unsigned int dump_read_u4(char * dp)
{
   unsigned int temp;

   dump_read((void *)dp, (void *)&temp, 4);
   temp = ntohl(temp);
   return(temp);
}

/******************************/
static jlong dump_read_jlong(char * dp)
{
   jlong temp;

   dump_read((void *)dp, (void *)&temp, 8);

   if ( 23 != ntohs(23) )
   {
      char * p = (char *)&temp;
      int    i;

      for ( i = 7; i >= 0; i-- )
      {
         *p++ = dp[i];
      }
   }
   return(temp);
}

/******************************/
static void * dump_read_p(void * p)
{
   void * q;

   dump_read(p, &q, sizeof(void *));
   return(q);
}

/******************************/
FILE * mopen(char * fnm, char * mode)
{
   FILE * fd;

   if ( (fd = fopen(fnm, mode)) == NULL)
   {
      fprintf(stderr, "JPROF Err: Opening %s as %s failed. errno = %d (%s)\n", fnm, mode, errno, strerror(errno));
      err_exit(" Can't open monitor-dump file in mopen ");
   }
   return(fd);
}

/******************************/
void monitor_dump_event(char *stt, char *end)
{
   FILE     * fdmd;
   int        jlm = 0;

   unsigned char ty;
   int           i;
   int           szPVoid = sizeof(void *);

   char mlab[2][10] = {"MON_JAVA", "MON_RAW"};
   int int1, int2;

   int rawcnt = 0;
   int infcnt = 0;

   UINT64 tm_delta;

   char * jlmhdr = " %s     GETS   NONREC     SLOW      REC    TIER2    TIER3 %s AVER_HTM  MON-NAME\n\n";
   char * jlmfmt = " %5.0f %8d %8d %8d %8d %8d %8d %5.0f %8.0f  ";

   jdata_t  * jp;

   int verJlmDump = 0;                  // Version
   int fmtJlmDump = 0;                  // Format

   int MONMAX;                          // Maximum number of monitors, calculated from dump size

   jdata_t ** arrjmon;                  // array of jdata_t ptrs
   jdata_t ** arrsmon;

   /*** code starts ***/

   ge.seqJLM++;                         // JLM Dump sequence number

   fprintf(gc.msg, "\n > monitor_jlm_dump\n");
   fdmd = file_open("jlm", ge.seqJLM);

   WriteFlush(fdmd, "\n Java Lock Monitor Report\n\n");

   gv->tm_curr = read_cycles_on_processor(0);

   tm_delta = gv->tm_curr - gv->tm_stt;

   WriteFlush(fdmd, " %20s %" _P64  "d\n\n",
              "JLM_Interval_Time", tm_delta);

   if ( gv->DistinguishLockObjects )    // Should have been ( ge.dumpJlmStats )
   {
      verJlmDump = dump_read_u4(stt); stt += 4;   // Version
      fmtJlmDump = dump_read_u4(stt); stt += 4;   // Format
   }
   OptVMsgLog( "JLM Dump: ver = %d, fmt = %d\n", verJlmDump, fmtJlmDump );

   WriteFlush( gc.msg, "stt=%p,end=%p\n", stt, end );

   MONMAX = (int)((end - stt) / 32); // Conservative estimate of size of one entry in the dump

   arrjmon = (jdata_t **)zMalloc( "mon_dump_event", MONMAX * sizeof(jdata_t *) );
   arrsmon = (jdata_t **)zMalloc( "mon_dump_event", MONMAX * sizeof(jdata_t *) );   // Can be combined with extra code

   WriteFlush( gc.msg, "MONMAX = %X\n", MONMAX );

   while (stt < end)
   {
      unsigned char held;
      unsigned int  htlo, hthi;
      size_t   len;
      double   dhtm;

      jp = (jdata_t *)zMalloc( "mon_dump_event", sizeof(jdata_t) );

      ty   = (unsigned char)*stt;
      stt++;
      held = (unsigned char)*stt;
      stt++;

      if (ty == MONITOR_JAVA)
      {
         arrjmon[infcnt] = jp;
         infcnt++;
         if (infcnt >= MONMAX)
         {
            WriteFlush(fdmd, "\nStopping JLM - Java Monitor Count %d\n", MONMAX);
            fclose(fdmd);
            return;
         }
      }
      else if (ty == MONITOR_RAW)
      {
         arrsmon[rawcnt] = jp;
         rawcnt++;
         if (rawcnt >= MONMAX)
         {
            WriteFlush(fdmd, "\nStopping JLM - Raw Monitor Count %d\n", MONMAX);
            fclose(fdmd);
            return;
         }
      }
      else
      {
         ErrVMsgLog("\nStopping Java Lock Monitor - Bad Data\n" );
         WriteFlush(fdmd, "\nStopping Java Lock Monitor - Bad Data\n");
         fclose(fdmd);
         return;
      }

      jp->gets   = dump_read_u4(stt); stt += 4;
      jp->slow   = dump_read_u4(stt); stt += 4;
      jp->rec    = dump_read_u4(stt); stt += 4;
      jp->tier2  = dump_read_u4(stt); stt += 4;
      jp->tier3  = dump_read_u4(stt); stt += 4;

      hthi     = (unsigned int)dump_read_u4(stt); stt += 4;
      htlo     = (unsigned int)dump_read_u4(stt); stt += 4;

      jp->hthi = hthi;
      jp->htlo = htlo;

      if ( 1 == fmtJlmDump && ty == MONITOR_JAVA )
      {
         jp->tag = dump_read_jlong(stt);
      }

      if ( 1 == fmtJlmDump )
      {
         stt += sizeof(jlong);
      }
      else
      {
         stt += szPVoid;             // 4 or 8
      }

      jp->mnm = dupJavaStr(stt);
      len     = strlen(jp->mnm);
      stt    += len + 1;

      // derived entries
      jp->nrec = jp->gets - jp->rec;
      jp->fast = jp->nrec - jp->slow;

      if (jp->gets > 0)
      {
         dhtm = (double)htlo;
         dhtm += 1024.0 * 1024.0 * 4096.0 * (double)hthi;
         jp->dhtm = dhtm;
         jp->util = 100.0 * dhtm / (double)(INT64)tm_delta;

         if (jp->nrec > 0)
         {
            jp->averht = dhtm / (double)(INT64)jp->nrec;
            jp->miss   = 100.0 * jp->slow / (double)jp->nrec;
         }
         else
         {
            jp->averht = 0.0;
            jp->miss   = 0.0;
         }
      }
      else
      {
         jp->util = jp->miss = jp->averht = 0.0;
      }
   }

   // qsort by 1) miss 2) gets
   WriteFlush(fdmd,
              "\nSystem (Registered) Monitors\n\n");

   qsort((void *)arrsmon, rawcnt, sizeof(jdata_t *), qcmp_jlm);

   WriteFlush(fdmd, jlmhdr, "%MISS", "%UTIL" );

   for (i = 0; i < rawcnt; i++)
   {
      jp = arrsmon[i];

      WriteFlush(fdmd, jlmfmt,
                 jp->miss, jp->gets, jp->nrec,
                 jp->slow, jp->rec, jp->tier2, jp->tier3,
                 jp->util, jp->averht);
      WriteFlush(fdmd, "%s\n", jp->mnm);

      xFreeStr( jp->mnm );
      xFree(jp, sizeof(jdata_t)+1 );
   }
   xFree( arrsmon, MONMAX * sizeof(jdata_t *) + 1 );

   WriteFlush(fdmd, "\n Java (Inflated) Monitors\n\n");

   WriteFlush(fdmd, jlmhdr, "%MISS", "%UTIL" );

   qsort((void *)arrjmon, infcnt, sizeof(jdata_t *), qcmp_jlm);

   for (i = 0; i < infcnt; i++)
   {
      jp = arrjmon[i];

      fprintf(fdmd, jlmfmt,
              jp->miss, jp->gets, jp->nrec,
              jp->slow, jp->rec, jp->tier2, jp->tier3,
              jp->util, jp->averht);

      if ( gv->DistinguishLockObjects && jp->tag )
      {
         // new form of MON-NAME including object hashcode
         // jp->mnm is in the form:
         //   "[<monitoraddress>] <class>@<object> (<type>)"
         // we will output the form:
         //   "<class>@<objecthashcode>-<monitoraddress> (<type>)"
         // Newest form is:
         //   "[<monitoraddress>] <class>@<object> (<type>) <tag>"

         fprintf(fdmd, "%s %016" _P64 "X\n", jp->mnm, jp->tag);
      }
      else
      {
         fprintf(fdmd, "%s\n", jp->mnm);
      }
      fflush(fdmd);

      xFreeStr( jp->mnm );
      xFree(jp, sizeof(jdata_t)+1 );
   }
   xFree( arrjmon, MONMAX * sizeof(jdata_t *) + 1 );

   // legend
   fprintf(fdmd, "\n LEGEND:\n");
   fprintf(fdmd, " %20s : %s\n", "%MISS", "100 * SLOW / NONREC");
   fprintf(fdmd, " %20s : %s\n", "GETS", "Lock Entries");
   fprintf(fdmd, " %20s : %s\n", "NONREC", "Non Recursive Gets");
   fprintf(fdmd, " %20s : %s\n", "SLOW", "Non Recursives that Block");
   fprintf(fdmd, " %20s : %s\n", "REC", "Recursive Gets");
   fprintf(fdmd, " %20s : %s\n", "TIER2",
           "SMP: Total try-enter spin loop cnt (middle for 3 tier) ");
   fprintf(fdmd, " %20s : %s\n", "TIER3",
           "SMP: Total yield spin loop cnt (outer for 3 tier) ");
   fprintf(fdmd, " %20s : %s\n", "%UTIL", "100 * Hold-Time / Total-Time");
   fprintf(fdmd, " %20s : %s\n", "AVER-HT", "Hold-Time / NONREC");
   fflush(fdmd);

   fclose(fdmd);
   WriteFlush(gc.msg, " < monitor_dump_event\n");
}
