/*   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2008
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "post.h"
#include "bputil.h"
#include "perfutil.h"

extern void panic(char * why);
void        read_next_rec(filesec_t * f, int type);

FILE      * fnrm2 = NULL;                  // The nrm2 file (very unassuming name)
filesec_t * afsp[MAX_CPUS]     = {NULL};   // array of file section ptrs
filesec_t * afsp_mte[MAX_CPUS] = {NULL};   // array of file section ptrs
filesec_t * afsp_final_mte      = NULL;    // Final mte data for Linux normal mode

int  big_endian;                        // file Endianness 0:little 1:big
int  nrm2 = 0;                          // nrm2 = 1 normal2 = 0

int  post_num_cpus = 0;                 // number of cpus
int  post_slow_tsc = 0;	                // number of "slow" timestamps
int  post_variable_timer_ticks;         // Whether or not CONFIG_NOT_HZ is defined (linux-only)

uchar  tbuf[8192];                      // The rest of the trace record
int64  read_bytes = 0;                  // total bytes read from a file
int    g_mcnt     = 1;

struct offset_node * first_trace_section[MAX_CPUS]   = {NULL};   // pointers to first trace section info
struct offset_node * first_mte_section[MAX_CPUS]     = {NULL};   // pointers to first mte section info
struct offset_node * current_trace_section[MAX_CPUS] = {NULL};   // pointers to current section info
struct offset_node * current_mte_section[MAX_CPUS]   = {NULL};   // pointers to current section info
struct offset_node * final_mte_section               = NULL;     // Pointer to the final MTE data section

static uint64 nonsection_size = 0;      // size of non-section data, such as kallsyms for Linux

/**********************************/
/****** CODE START ******/
/**********************************/

/**********************************/
double getTime(int cpu)
{
   double tm;
   // double two32th = 4.0 * 1024.0 * 1024.0 * (double)1024.0;
   filesec_t * fsp;                     // array of file section ptrs

   fsp = afsp[cpu];
   tm  = (double)(INT64)fsp->ts;
   //tm = ((double)tr->thi) * two32th;
   //tm += (double)tr->tlo;

   return(tm);
}

/* ************************************************************ */
int64 getReadBytes(void)
{
   return(read_bytes);
}

/* ************************************************************ */
char ** tokenize(char * str, int * pcnt)
{
   char c;
   int  ind  = 0;
   int  owsp = 1;
   static int carr[256] = {0};
   static int first     = 0;

   /* NULL terminate tokens in input string */
   static char * tkarr[128];            /* array of STRING ptrs */

   if (first == 0)
   {
      /* 1 for white space */
      first = 1;
      carr[(int)'\t'] = 1;
      carr[(int)' ']  = 1;
      carr[(int)'\n'] = 1;
      carr[(int)'\r'] = 1;
      carr[(int)'\0'] = 1;
   }

   while ( (c = *str) != '\0')
   {
      if (carr[(int)c] != owsp)
      {                                 /* new != old */
         if (owsp == 1)
         {                              /* old == white */
            if (ind < 16 * 1024)
            {
               tkarr[ind] = str;        /* start of ind-th string(0-based) */
            }
            ind++;
         }
         else *str = '\0';              /* white => NULL */
         owsp = !owsp;
      }
      str++;
   }

   *pcnt = ind;                         /* token count */

   return(tkarr);                       /* return char * array */
}

/*******************************************/
void endianChk(unsigned char c)
{

   if (c == 0x01)
   {
      fprintf(gc.msg, " Log written in LITTLE Endian\n");
      big_endian = 0;
   }
   else if (c == 0x04)
   {
      fprintf(gc.msg, " Log written in BIG Endian\n");
      big_endian = 1;
   }
   else
   {
      fprintf(stderr, "ERROR : Invalid NRM2 hdr\n");
      exit(1);
   }
}

/*******************************************/
unsigned short get_ushort(int off)
{
   unsigned short t;
   unsigned char  c1, c2;

   // read byte @ time => dont need to know own Endian-ness
   c1 = (unsigned char)tbuf[off];
   c2 = (unsigned char)tbuf[off + 1];

   if (big_endian)
   {
      t = (c1 << 8) + c2;
   }
   else
   {
      t = (c2 << 8) + c1;
   }
   return(t);
}

/*******************************************/
int read_eint(int * ip)
{
   int t, t1, t2, t3, t4;
   unsigned char * cp;

   cp = (unsigned char *)ip;

   t1 = (int)cp[0];
   t2 = (int)cp[1];
   t3 = (int)cp[2];
   t4 = (int)cp[3];

   if (big_endian)
   {
      t = ((  t1 << 24) & 0xff000000)
          | ((t2 << 16) & 0xff0000)
          | ((t3 << 8)  & 0xff00)
          | ( t4        & 0xff);
   }
   else
   {
      t = ((  t4 << 24) & 0xff000000)
          | ((t3 << 16) & 0xff0000)
          | ((t2 << 8)  & 0xff00)
          | ( t1        & 0xff);
   }
   return(t);
}

/*******************************************/
int64 read_eint64(unsigned char * cp)
{
   int64 t = 0;
   int   i;

   if (big_endian)
   {
      for (i = 0; i < 8; ++i)
      {
         t = t << 8;
         t = t | (cp[i] & 0xff);
      }
   }
   else
   {
      for (i = 0; i < 8; ++i)
      {
         t = t << 8;
         t = t | (cp[7-i] & 0xff);
      }
   }
   return(t);
}

/*******************************************/
unsigned int get_ulong(int off)
{
   // Speedup if LOG & Machine have same Endianness
   // Log     Endianness in Header
   // Machine Endianness via WriteBytes/ReadInt code
   // 1 => b[0] => Little -> 1, Big -> 2**24
   unsigned int t, t1, t2, t3, t4;

   t1 = (unsigned int)tbuf[off];
   t2 = (unsigned int)tbuf[off + 1];
   t3 = (unsigned int)tbuf[off + 2];
   t4 = (unsigned int)tbuf[off + 3];

   if (big_endian)
   {
      t = ((  t1 << 24) & 0xff000000)
          | ((t2 << 16) & 0xff0000)
          | ((t3 << 8)  & 0xff00)
          | ( t4        & 0xff);
   }
   else
   {
      t = ((  t4 << 24) & 0xff000000)
          | ((t3 << 16) & 0xff0000)
          | ((t2 << 8)  & 0xff00)
          | ( t1        & 0xff);
   }
   return(t);
}


/*******************************************/
int init_list_ptr_trace(void)
{
   int i;

   for (i = 0; i < post_num_cpus; i++)
   {
      current_trace_section[i] = first_trace_section[i];
      afsp[i]->curr = current_trace_section[i]->file_offset + PERFHEADER_BUFFER_SIZE;
      afsp[i]->ra   = 0;
      if ( first_trace_section[i]->section_size != 0 )
      {
         afsp[i]->eos = 0;
      }
      else
      {
         afsp[i]->eos = 1;  // No records for this cpu - mark it as end
      }
      afsp[i]->beg  = current_trace_section[i]->file_offset;
      afsp[i]->end  = current_trace_section[i]->section_size + afsp[i]->curr;
   }
   return(0);
}

/*******************************************/
int init_list_ptr_mte(void)
{
   int i;

   for (i = 0; i < post_num_cpus; i++)
   {
      current_mte_section[i] = first_mte_section[i];
      afsp_mte[i]->curr = current_mte_section[i]->file_offset + PERFHEADER_BUFFER_SIZE;
      afsp_mte[i]->ra   = 0;
      if ( first_mte_section[i]->section_size != 0 )
      {
         afsp_mte[i]->eos = 0;
      }
      else
      {
         afsp_mte[i]->eos = 1;  // No records for this cpu - mark it as end
      }
      afsp_mte[i]->beg  = current_mte_section[i]->file_offset;
      afsp_mte[i]->end  = current_mte_section[i]->section_size + afsp_mte[i]->curr;
   }

   if (COLLECTION_MODE_NORMAL == gv.trace_type)
   {
      init_last_mte();
   }

   return(0);
}


/*******************************************/
void init_last_mte(void)
{
   afsp_final_mte->curr = final_mte_section->file_offset + PERFHEADER_BUFFER_SIZE;
   afsp_final_mte->ra   = 0;
   if ( final_mte_section->section_size != 0 )
   {
      afsp_final_mte->eos = 0;
   }
   else
   {
      afsp_final_mte->eos = 1;  // No records - mark it as end
   }
   afsp_final_mte->beg  = final_mte_section->file_offset;
   afsp_final_mte->end  = final_mte_section->section_size + afsp_final_mte->curr;
}


//
// get_next_rec_mte()
// ******************
// Returns the cronologically next record in the mte trace
//
trace_t * get_next_rec_mte(void)
{
   int         i, xmin;
   UINT64      tsmin;
   filesec_t * fp;
   trace_t   * tp = NULL;

   // read-ahead rec per CPU
   // select cpu w min time

   tsmin = ((UINT64)0x7fffffff) << 32;
   tsmin = tsmin | 0xffffffff;
   xmin  = -1;

#ifdef PI_DEBUG
   fprintf(gc.msg, "\n get_next_rec_mte\n");
#endif

   for (i = 0; i < post_num_cpus; i++)
   {
#ifdef PI_DEBUG
      fprintf(gc.msg, " cpu %d\n", i);
#endif
      fp = afsp_mte[i];

      if (fp->eos == 0)
      {                                 // not @ EndOfFile
         if (fp->ra == 0)
         {                              // need read-ahead on this CPU
#ifdef PI_DEBUG
            fprintf(gc.msg, "\n readAhead on cpu %d\n", i);
#endif
            read_next_rec(fp, MTE_TYPE);
         }

#ifdef PI_DEBUG
         fprintf(gc.msg, " cpu %d  cputs %16" _P64 "x\n", i, fp->ts);
         fprintf(gc.msg, "  currmin ts %16" _P64 "x\n", tsmin);
#endif

         if (fp->ra == 1)
         {
            if ( (fp->ts < tsmin) || (xmin == -1) )
            {
               tsmin = fp->ts;
               xmin  = i;               // earliest cpu
#ifdef PI_DEBUG
               fprintf(gc.msg, " cpu %d newMin %16" _P64 "x\n", xmin, tsmin);
#endif
            }
         }
      }
   }

   if (xmin >= 0)
   {
      fp         = afsp_mte[xmin];
      tp         = fp->tp;
      fp->ra     = 0;                   // reset read-ahead on used/earliest cpu
      tp->cpu_no = xmin;
   }

   if ( (tp == NULL) && (gv.trace_type == COLLECTION_MODE_NORMAL) )
   {
      // Still need to process the final mte data
      tp = get_next_rec_final_mte();
   }

   return(tp);
}


//
// get_next_rec_final_mte()
// ************************
// Returns the cronologically next record in the final mte trace
//
trace_t * get_next_rec_final_mte(void)
{
   filesec_t * fp;
   trace_t   * tp = NULL;

   fp = afsp_final_mte;

   if (fp->eos == 0)
   {                                 // not @ EndOfFile
      read_next_rec(fp, MTE_FINAL_TYPE);
      tp         = fp->tp;
      tp->cpu_no = 0;
      tp->time    = 0xFFFFFFFF; // FIXME
   }
   return(tp);
}

//
// get_next_rec()
// *******************
// Returns the cronologically next record in the trace
// On Linux, we need to check both MTE and non-MTE records,
// since they are stored in different buffers/trace sections
//
trace_t * get_next_rec(void)
{
   int         i, xmin;
   int         use_mte_min;
   UINT64      tsmin;
   filesec_t * fp;
   trace_t   * tp = NULL;

   // read-ahead rec per CPU
   // select cpu w min time

   tsmin       = ((UINT64)0x7fffffff) << 32;
   tsmin       = tsmin | 0xffffffff;
   xmin        = -1;
   use_mte_min =  0;

#ifdef PI_DEBUG
   fprintf(gc.msg, "\n get_next_rec\n");
#endif

   for (i = 0; i < post_num_cpus; i++)
   {
#ifdef PI_DEBUG
      fprintf(gc.msg, " cpu %d\n", i);
#endif
      fp = afsp[i];

      if (fp->eos == 0)
      {                                 // not @ EndOfFile
         if (fp->ra == 0)
         {                              // need read-ahead on this CPU
#ifdef PI_DEBUG
            fprintf(gc.msg, "\n readAhead TRACE_TYPE on cpu %d\n", i);
#endif
            read_next_rec(fp, TRACE_TYPE);
         }

#ifdef PI_DEBUG
         fprintf(gc.msg, " cpu %d  cputs %16" _P64 "x\n", i, fp->ts);
         fprintf(gc.msg, "  currmin ts %16" _P64 "x\n", tsmin);
#endif

         if (fp->ra == 1)
         {
            if ( (fp->ts < tsmin) || (xmin == -1) )
            {
               tsmin = fp->ts;
               xmin  = i;               // earliest cpu
#ifdef PI_DEBUG
               fprintf(gc.msg, " cpu %d newMin %16" _P64 "x\n", xmin, tsmin);
#endif
            }
         }
      }
   }

   if (gv.oprof)                        // Linux - we need to check mte records too
   {
       // For Normal, we need only final mte
      if (COLLECTION_MODE_NORMAL != gv.trace_type)
      {
         for (i = 0; i < post_num_cpus; i++)
         {
#ifdef PI_DEBUG
            fprintf(gc.msg, " cpu %d\n", i);
#endif
            fp = afsp_mte[i];

            if (fp->eos == 0)
            {                              // not @ EndOfFile
               if (fp->ra == 0)
               {                           // need read-ahead on this CPU
#ifdef PI_DEBUG
                  fprintf(gc.msg, "\n readAhead MTE_TYPE on cpu %d\n", i);
#endif
                  read_next_rec(fp, MTE_TYPE);
               }

#ifdef PI_DEBUG
               fprintf(gc.msg, " cpu %d  cputs %16" _P64 "x\n", i, fp->ts);
               fprintf(gc.msg, "  currmin ts %16" _P64 "x\n", tsmin);
#endif

               if (fp->ra == 1)
               {
                  if ( (fp->ts < tsmin) || (xmin == -1) )
                  {
                     tsmin       = fp->ts;
                     xmin        = i;      // earliest cpu
                     use_mte_min = 1;      // we found an mte record with lower ts
#ifdef PI_DEBUG
                     fprintf(gc.msg, " cpu %d newMin %16" _P64 "x\n", xmin, tsmin);
#endif
                  }
               }
            }                           // end if (fp->eos == 0)
         }                              // end for (i = 0; i < post_num_cpus; i++)
      }                                 // end if not Normal mode
   }                                    // end if (gv.oprof)


   if (xmin >= 0)
   {
      if (use_mte_min)
         fp = afsp_mte[xmin];
      else
         fp = afsp[xmin];

      tp         = fp->tp;
      fp->ra     = 0;                   // reset read-ahead on used/earliest cpu
      tp->cpu_no = xmin;

   }
   else
   {
      if (gv.oprof && (COLLECTION_MODE_NORMAL == gv.trace_type))
      {
         tp = get_next_rec_final_mte();
      }
   }

   return(tp);
}



//
// safe_fread()
// ************
//
int safe_fread(filesec_t * fp, int len, int type)
{

   struct offset_node * trace_section;
   int bytes_in_section;
   int rc  = 0;
   int cpu = fp->cpu;

   bytes_in_section = (int)(fp->end - fp->curr);
   safe_fseeko(fnrm2, fp->curr, SEEK_SET);

   if (bytes_in_section >= len)
   {
      rc = (int)fread(tbuf, 1, len, fnrm2);
      fp->curr += rc;
      return(rc);
   }

   // else some bytes are in the next section, read only bytes_in_section

   rc = (int)fread(tbuf, 1, bytes_in_section, fnrm2);

   if (rc != bytes_in_section)
   {
      return(rc);
   }

   // Move to the next section if it exists

   OptVMsg("cpu[%d]: End of a section %"_L64u" - %"_L64u"\n",
           cpu, fp->beg, fp->end);

   if (type == TRACE_TYPE)
   {
      current_trace_section[cpu] = current_trace_section[cpu]->next_ptr;
      trace_section              = current_trace_section[cpu];
   }
   else if (type == MTE_TYPE)
   {
      current_mte_section[cpu] = current_mte_section[cpu]->next_ptr;
      trace_section            = current_mte_section[cpu];
   }
   else
   {   // MTE_FINAL_TYPE - only one section
       trace_section = NULL;
   }

   if (trace_section == NULL)           // no more sections for this cpu
   {
      fp->eos = 1;
      return(rc);
   }

   // There is a next section

   fp->curr = trace_section->file_offset + PERFHEADER_BUFFER_SIZE;
   fp->beg  = trace_section->file_offset;
   fp->end  = trace_section->section_size + fp->curr;

   safe_fseeko(fnrm2, fp->curr, SEEK_SET);

   rc = (int)fread(tbuf + bytes_in_section, 1, len - bytes_in_section, fnrm2);
   fp->curr += rc;

   return(rc + bytes_in_section);

}

/****************************************************/
void read_next_rec(filesec_t * fp, int type)
{
   int       tmp, tpos, tlen, len, size;
   int       ints, strings;
   int       hook_type, hook_len;
   trace_t * tp;
   int       mets = 0;

   ints = strings = 0;
   tp   = fp->tp;

   // Try to read 16-byte header
   safe_fseeko(fnrm2, fp->curr, SEEK_SET);
   tp->file_offset = fp->curr;
   size = safe_fread(fp, TRACE_RECORD_HEADER, type);   // 12 bytes: TY-LEN-MAJ:MIN:TS

   if (size != TRACE_RECORD_HEADER)
   {
      fp->eos = 1;
      fprintf(gc.msg, " EOS: cpu %2d curr %16"_P64x" end %16"_P64x"\n",
              fp->cpu, fp->curr, fp->end);
      return;

   }

   if (gv.showx == 2)
   {
      memcpy(tp->raw, tbuf, HOOK_HEADER_SIZE);
   }
   PI_HOOK_HEADER *phh = HOOK_HEAD_PTR(tbuf);

   tmp         = phh->typeLength;
   hook_type   = tmp & RAWFMT_HOOK_MASK;
   hook_len    = tmp & RAWFMT_HOOK_LEN_MASK;
   read_bytes += hook_len;

   /* parse common fields */
   tp->hook_type = hook_type;
   tp->hook_len  = hook_len;
   tp->major     = phh->majorCode;       // off in tbuf
   tp->minor     = phh->minorCode;
   tp->time       = phh->timeStamp;

   // abort on len < HOOK_HEADER_SIZE
   if (tp->hook_len < HOOK_HEADER_SIZE)
   {
      fprintf(stderr, " Hook Length Error %d EXITING\n",
              tp->hook_len);
      fprintf(stderr, "   cpu %2d curr %16"_P64x" end %16"_P64x"\n",
              fp->cpu, fp->curr, fp->end);
      exit(1);
   }
   if (tp->major == 0)
   {
      fprintf(stderr, " Major = ZERO: EXITING\n");
      fprintf(stderr, "   cpu %2d curr %16"_P64x" end %16"_P64x"\n",
              fp->cpu, fp->curr, fp->end);
      exit(1);
   }

   len  = tp->hook_len - HOOK_HEADER_SIZE;            // read rest of hook
   size = (int) safe_fread(fp, len, type);

   if (size != len)
   {
      fprintf(stderr, " Not enough data in the trace: EXITING\n");
      fprintf(stderr, " EOS: cpu %2d curr %16"_P64x" end %16"_P64x", required %d, got %d \n",
              fp->cpu, fp->curr, fp->end, len, size);
      fprintf(gc.msg, " Not enough data in the trace: EXITING\n");
      fprintf(gc.msg, " EOS: cpu %2d curr %16"_P64x" end %16"_P64x", required %d, got %d \n",
              fp->cpu, fp->curr, fp->end, len, size);
      return;

   }
   tp->file_offset = fp->curr;

   if (gv.showx == 2)
   {
      int len1;

      len1 = len;
      if (len > MAX_SHOWX_LEN) len1 = MAX_SHOWX_LEN;
      memcpy(&(tp->raw[TRACE_RECORD_HEADER]), tbuf, len1);
   }

   uint64 *ptr;
   tpos = 0;
   switch (tp->hook_type)
   {
   case PERF_64BIT:
      while (tpos < len)
      {
         tp->ival[ints] = READ_64_RAWBUF(tbuf, tpos);
         ints++;
      }
      break;

   case PERF_VARDATA:
      while (tpos < len)
      {
         tlen = READ_64_RAWBUF(tbuf, tpos);        // string length

         if ( (tpos + tlen) > len)
         {
            fprintf(stderr, " NRM2 Format Error\n");
            fprintf(stderr, " str exceeds Hook Length \n");
            fprintf(stderr, " cpu %2d curr %16"_P64x"\n",
                    fp->cpu, fp->curr);
            exit(1);
         }

         tp->slen[strings] = tlen;
         memcpy(&tp->sval[strings], &tbuf[tpos], tlen);
         strings++;

         tpos += tlen;
         tpos += 7;                     // 8 byte align, these should already be aligned
         tpos &= 0xfffffff8;
      }
      break;

   case PERF_MIXED_DATA:
	   ptr = (uint64 *)(tbuf+tpos);
	   tlen = READ_64_RAWBUF(tbuf, tpos);            // no of ints

      while (tlen > 0)
      {                                 // ints
    	  ptr = (uint64 *)(tbuf+tpos);
         tp->ival[ints] = READ_64_RAWBUF(tbuf, tpos);
         ints++;
         tlen--;
      }

      // strings
      while (tpos < len)
      {
         tlen = READ_64_RAWBUF(tbuf, tpos);        // str length

         if ( (tpos + tlen) > len)
         {
            fprintf(stderr, " NRM2 Format Error\n");
            fprintf(stderr, "  str exceeds Hook Length \n");
            fprintf(stderr, "  cpu %2d curr %016" _P64 "X\n",
                    fp->cpu, fp->curr);
            exit(1);
         }

         tp->slen[strings] = tlen;
         memcpy(&tp->sval[strings], &tbuf[tpos], tlen);
         strings++;

         tpos += tlen;
         tpos +=7;                      // 8 byte align
         tpos &= 0xfffffff8;
      }
      break;

   default:
      fprintf(gc.msg, " Extended rec type at %16"_P64x"\n", fp->curr);
   }

   // 0x08 MM hi fields from 08
   // 0x09 lo metrics in main.c

   fp->ra = 1;

   tp->mets    = mets;
   tp->ints    = ints;
   tp->strings = strings;

   // set global thi ???
   if (((tp->major == SPECIAL_MAJOR) || (tp->major == TSCHANGE_MAJOR)) && (tp->minor == TSCHANGE_TIMEHI_MIN))
   {
      tp->time = tp->ival[0];
   }

   // fp time (adjusted for cpu sync)
   fp->ts = tp->time;
   //fp->ts -= fp->ts_del;                // sync up cpus

   return;
}

/*******************************************/
void noskew(void)
{
   int i;
   filesec_t * fp;

   fprintf(gc.msg, " Removing Clock Skew Compensation\n");

   for (i = 0; i < post_num_cpus; i++)
   {
      fp         = afsp[i];
      fp->ts_del = 0;
      if (gv.oprof) afsp_mte[i]->ts_del = 0;   // Linux only
   }
}

/*******************************************/
int fsec_init(FILE * fd, int hsize, int new)
{
   int          i;
   int          dsize, size;
   uint64_t     stt    = 0;
   unsigned int tmp;
   PERFHEADER * ph     = (PERFHEADER *)tbuf;
   UINT64       ts_min;
   UINT64       ts_stt = 0;
   filesec_t  * fp;
   trace_t    * tp;

   populate_list_ptr(fd);

   ts_min = ((UINT64)0x7fffffff) << 32;
   ts_min = ts_min | 0xffffffff;

   fprintf(gc.msg, " perfhdr size : %d\n", PERFHEADER_BUFFER_SIZE);

   for (i = 0; i < post_num_cpus; i++)
   {
      // calloc for each CPU

      fp = afsp[i] = (filesec_t *)zMalloc("filesec", sizeof(filesec_t));   // filesec
      tp = (trace_t *)zMalloc("trace buffer",sizeof(trace_t));   // trace buffer

      if ( NULL == fp || NULL == tp )
      {
         fprintf(stderr, " ** Out of memory in fsec_init(FILE *,int,int) ***\n");
         exit(1);
      }

      tp->time = 0;
      fp->ts  = 0;
      fp->tp  = tp;
      fp->cpu = i;

      stt = first_trace_section[i]->file_offset;

      safe_fseeko(fd, stt, SEEK_SET);   // => header of the first section for that cpu
      size = (int)fread(tbuf, 1, hsize, fd);
      if (size < hsize) return(BP_ERROR);

      if (new == 1)
      {
         dsize      = read_eint((int *)&(ph->data_size));
         ts_stt    += ph->start_time;
         fp->ts_stt = ts_stt;

         g_mcnt     = read_eint((int *)&ph->Metric_cnt);
      }
      else
      {                                 // deprecated format, so no large files
         fseek(fd, 0, SEEK_END);        // move to end of file
         size  = ftell(fd);
         dsize = size - PERFHEADER_BUFFER_SIZE;

         fp->ts_stt = ts_stt;
      }

      if (new == 1)
      {
         fprintf(gc.msg, " NRM2 Format\n");
      }
      else
      {
         fprintf(gc.msg, " Normal2 Format(DEPRECATED)\n");
      }

      fprintf(gc.msg, "   CPU       %8d\n", i);
      fprintf(gc.msg, "   First section starting  0x%016"_P64x" %16"_P64d"\n", stt,   stt);
      fprintf(gc.msg, "   Header    0x%08x %8d\n", hsize, hsize);
      fprintf(gc.msg, "   First section data      0x%08x %8d\n", dsize, dsize);
      fprintf(gc.msg, "   First section end       0x%016"_P64x" %16"_P64d"\n",
              stt + hsize + dsize, stt + hsize + dsize);

      fprintf(gc.msg, "   ts-stt %16" _P64 "x\n", ts_stt);

      // sync buffers
      if (ts_stt < ts_min)
      {
         ts_min = ts_stt;
         fprintf(gc.msg,
                 " NewMinForSync  cpu %d ts_min %16" _P64 "x\n",
                 i, ts_min);
      }
      fp->beg  = stt + hsize;           // file offset @ stt of data
      fp->end  = stt + hsize + dsize;   // file offset @ end of section
      fp->curr = stt + hsize;           // current offset

      if (gv.oprof)
      {
         afsp_mte[i] = (filesec_t *)zMalloc("filesec",sizeof(filesec_t));   // filesec for mte
         tp          = (trace_t *)zMalloc("trace buffer",sizeof(trace_t));
         if ( NULL == afsp_mte[i] || NULL == tp )
         {
            fprintf(stderr, " ** Out of memory in fsec_init(FILE *,int,int) ***\n");
            exit(1);
         }
         tp->time   = 0;
         afsp_mte[i]->ts     = 0;
         afsp_mte[i]->tp     = tp;
         afsp_mte[i]->cpu    = i;
         afsp_mte[i]->ts_stt = ts_stt;
         afsp_mte[i]->beg    = first_mte_section[i]->file_offset;
         afsp_mte[i]->curr   = afsp_mte[i]->beg + hsize;
         afsp_mte[i]->end    = afsp_mte[i]->curr + first_mte_section[i]->section_size;
      }

   }


   if (gv.oprof && (COLLECTION_MODE_NORMAL == gv.trace_type ))
   {
      afsp_final_mte = (filesec_t *)zMalloc("filesec", sizeof(filesec_t));   // filesec for mte
      tp            = (trace_t *)zMalloc("trace buffer", sizeof(trace_t));
      if ( NULL == afsp_final_mte || NULL == tp )
      {
         fprintf(stderr, " ** Out of memory in fsec_init(FILE *,int,int) ***\n");
         exit(1);
      }
      tp->time			     = 0;
      afsp_final_mte->ts     = 0;
      afsp_final_mte->tp     = tp;
      afsp_final_mte->cpu    = 0;
      afsp_final_mte->ts_stt = ts_stt;
      afsp_final_mte->beg    = final_mte_section->file_offset;
      afsp_final_mte->curr   = afsp_final_mte->beg + hsize;
      afsp_final_mte->end    = afsp_final_mte->curr + final_mte_section->section_size;
   }

   for (i = 0; i < post_num_cpus; i++)
   {
      fp         = afsp[i];
      fp->ts_del = fp->ts_stt - ts_min;
      if (gv.oprof) afsp_mte[i]->ts_del = fp->ts_del;   // same skew for the same CPU

      fprintf(gc.msg, " Skew : cpu(%d) del %8" _P64 "x\n",
              i, fp->ts_del);
   }

   OptVMsg("End of fsec_init\n");
   return(BP_OK);
}

/*******************************************/
int64 nrm2_size(void)
{
   int64 filesize;

   safe_fseeko(fnrm2, 0, SEEK_END);
   filesize = (int64)ftello(fnrm2);
   return(filesize);
}

/*******************************************/
int populate_list_ptr(FILE * fd)
{
   int           bytes, i;
   uint64        filesize    = 0;
   uint64        nrm2_offset = 0;
   int           sec_size    = 0;
   int           current_cpu;
   char          first_trace_section_flag[MAX_CPUS] = {0};
   char          first_mte_section_flag[MAX_CPUS]   = {0};
   PERFHEADER  * ph;
   uint32        section_type;
   int         * lost_trace_data = NULL;   // lost trace records per cpu
   int         * lost_mte_data   = NULL;   // lost mte records per cpu

   // Allocate first nodes

   for (i = 0; i < post_num_cpus; i++)
   {
      first_trace_section[i] = xMalloc("first_trace_section",sizeof(struct offset_node));

      first_trace_section[i]->next_ptr     = 0;
      first_trace_section[i]->file_offset  = 0;
      first_trace_section[i]->section_size = 0;
      current_trace_section[i]             = first_trace_section[i];

      if (gv.oprof)                     // Linux
      {
         if (gv.trace_type == COLLECTION_MODE_NORMAL)
         {
            // Normal mode means we have only 1 final MTE buffer,
            // other MTE data is in trace buffers
            first_mte_section[i]   = first_trace_section[i];
         }
         else
         {
            // Separate MTE buffers
            first_mte_section[i] = xMalloc("first_mte_section",sizeof(struct offset_node));
         }

         first_mte_section[i]->next_ptr     = 0;
         first_mte_section[i]->file_offset  = 0;
         first_mte_section[i]->section_size = 0;
         current_mte_section[i]             = first_mte_section[i];
      }
   }

   if (gv.oprof)
   {
      lost_trace_data = zMalloc("lost_trace_data", post_num_cpus * sizeof(int));
      lost_mte_data   = zMalloc("lost_mte_data",   post_num_cpus * sizeof(int));

      // If Normal mode, allocate final MTE buffer node
      if (gv.trace_type == COLLECTION_MODE_NORMAL)
      {
         final_mte_section = xMalloc("final_mte_section",sizeof(struct offset_node));

         final_mte_section->next_ptr     = 0;
         final_mte_section->file_offset  = 0;
         final_mte_section->section_size = 0;
      }
   }

   // Read all section headers and link nodes with offset and size info

   filesize = nrm2_size();

   if (gv.oprof)
   {
      OptVMsg("filesize = 0x%016"_P64x", nonsection_size = 0x%016"_P64x"\n",
              filesize, nonsection_size);
      filesize = filesize - nonsection_size;
   }

   while (nrm2_offset != filesize)
   {
      OptVMsg("--------------------------------\n");
      OptVMsg("current nrm2_offset: %"_L64u"\n", nrm2_offset);

      safe_fseeko(fd, nrm2_offset, SEEK_SET);
      bytes = (int) fread((void *)&tbuf, sizeof(char), PERFHEADER_BUFFER_SIZE, fd);
      if (bytes < PERFHEADER_BUFFER_SIZE)
      {
         ErrVMsgLog(" ERROR reading nrm2\n");
         exit(1);
      }

      ph = (PERFHEADER *)tbuf;
      OptVMsg("NRM2? %d\n", strncmp((char *)&tbuf, "NRM2", 4));

      sec_size      = read_eint((int *)&ph->data_size);
      current_cpu   = read_eint((int *)&ph->cpu_no);

      OptVMsg("sec_size[%d] = %d \n", current_cpu, sec_size);

      if ( 0x01 & read_eint((int *)&(ph->flag)) )
      {
         ErrVMsgLog(" BufferOverflow CPU %8d\n", current_cpu);
      }

      if (gv.oprof)                     // Linux
      {

         section_type = read_eint((int *)&ph->section_type);

         // Is this a final MTE section?
         if (section_type & MTE_FINAL_TYPE)
         {
            OptVMsg("Final MTE section\n");

            lost_mte_data[0] = read_eint((int *)&ph->datalosscount);

            // the current_mte_section is already allocated

            final_mte_section->section_size = sec_size;
            final_mte_section->file_offset  = nrm2_offset;
            final_mte_section->next_ptr     = 0;

            nrm2_offset += (uint64)sec_size + PERFHEADER_BUFFER_SIZE;
            read_bytes  += PERFHEADER_BUFFER_SIZE;
            continue;                   // to the next loop iteration
         }

         // Is this a regular trace section or MTE-only section?
         if (section_type & MTE_TYPE)
         {
            OptVMsg("MTE section\n");

            lost_mte_data[current_cpu] = read_eint((int *)&ph->datalosscount);

            // If size = 0, just do not link this section - can happen at the end
            if (0 != sec_size) {
               if (first_mte_section_flag[current_cpu] != 1)
               {
                  first_mte_section_flag[current_cpu] = 1;

                  // the current_mte_section is already allocated
               }
               else                        // we need to allocate another node
               {
                  current_mte_section[current_cpu]->next_ptr = xMalloc("current_mte_section",sizeof(struct offset_node));
                  current_mte_section[current_cpu]           = current_mte_section[current_cpu]->next_ptr;
               }
               current_mte_section[current_cpu]->section_size = sec_size;
               current_mte_section[current_cpu]->file_offset  = nrm2_offset;
               current_mte_section[current_cpu]->next_ptr     = 0;
            }

            nrm2_offset += (uint64)sec_size + PERFHEADER_BUFFER_SIZE;
            read_bytes  += PERFHEADER_BUFFER_SIZE;
            continue;                   // to the next loop iteration
         }

         // else just update lost data

         lost_trace_data[current_cpu] = read_eint((int *)&ph->datalosscount);
      }

      // Regular trace section

      // If size = 0, just do not link this section - can happen at the end
      if (0 != sec_size) {
         if (first_trace_section_flag[current_cpu] != 1)
         {
            first_trace_section_flag[current_cpu] = 1;

            // the present_node is already allocated
         }
         else                              // we need to allocate another node
         {
            current_trace_section[current_cpu]->next_ptr = xMalloc("current_trace_section",sizeof(struct offset_node));
            current_trace_section[current_cpu]           = current_trace_section[current_cpu]->next_ptr;
         }
         current_trace_section[current_cpu]->section_size = sec_size;
         current_trace_section[current_cpu]->file_offset  = nrm2_offset;
         current_trace_section[current_cpu]->next_ptr     = 0;
      }

      nrm2_offset += (uint64)sec_size + PERFHEADER_BUFFER_SIZE;
      read_bytes  += PERFHEADER_BUFFER_SIZE;
   }

   if (gv.oprof)
   {
      for (i = 0; i < post_num_cpus; ++i)
      {
         if (lost_trace_data[i])
         {
            ErrVMsgLog("\n *WARNING*: cpu %d: trace records LOST = %d", i, lost_trace_data[i]);
         }
         if (lost_mte_data[i])
         {
            ErrVMsgLog("\n *INFO*: cpu %d: mte records had to wait = %d", i, lost_mte_data[i]);
         }
      }
      ErrVMsgLog("\n\n");
      OptVMsg("End populate_list_ptr\n");
   }

   return(0);
}


/*******************************************/
int init_raw_file(char * fnm)
{                                       // nrm2
   int          bytes;
   int          hsize;
   int          anrm2 = 0;
   int          enrm2 = 0;
   PERFHEADER * ph;
   uchar      * cp;
   uchar        c;

   fnrm2 = xopen(fnm, "rb");

   bytes = (int) fread((void *)&tbuf, 1, PERFHEADER_BUFFER_SIZE, fnrm2);

   if (bytes < PERFHEADER_BUFFER_SIZE)
   {
      fprintf(stderr, " ERROR reading %s\n", fnm);
      return(BP_ERROR);
   }

   anrm2 = strncmp((char *)&tbuf, "NRM2", 4) == 0;

   enrm2 = (0xd5 == tbuf[0]) &&         // EBCDIC NRM2
           (0xd9 == tbuf[1]) &&
           (0xd4 == tbuf[2]) &&
           (0xf2 == tbuf[3]);

   if (enrm2 || anrm2)
   {
      OptVMsgLog(" NRM2 Format\n");

      nrm2  = 1;

      ph = (PERFHEADER *)tbuf;
      cp = (unsigned char *)ph;

      // check buffer type
      // value for oprofile 2-pass (late mte data)
      if ((0x1 & ph->collection_mode) == 1)
      {
         gv.oprof = 1;

         // Also set the trace type
         gv.trace_type = ph->collection_mode & 0xfffffffe;
         OptVMsgLog("gv.trace_type %d\n", gv.trace_type);
      }

#ifdef DONT_COMPILE
      {
         int i;
         unsigned char * p;

         p = cp;
         OptVMsgLog(" NRM2 Header\n");

         for (i = 0; i < 255; i++)
         {
            if (0 == i % 4)
            {
               fprintf(stderr, "\n");
            }
            fprintf(stderr, "%02x", (unsigned char)(*p));
            p++;
         }
         fprintf(stderr, "\n");
      }
#endif

      endianChk(cp[4]);

      c = cp[8];
      if (c != 0xa)
      {                                 // binary xfer error ??
         fprintf(stderr, " %s\n",
                 " Exiting: CR missing in Header. BinXferError???\n");
         exit(0);
      }
      else
      {
         OptVMsgLog(" CR in Header\n");
      }

      hsize          = read_eint((int *)&ph->header_size);
      post_num_cpus  = read_eint((int *)&ph->num_cpus);
      post_slow_tsc  = read_eint((int *)&ph->slow_tsc_cnt);
      fprintf(gc.msg, " NoCPUs(fromHdr) %d\n", post_num_cpus);
#if defined(_LINUX)
      post_variable_timer_ticks = read_eint((int *)&ph->variable_timer_tick);
      if (post_variable_timer_ticks) {
         fprintf(gc.msg, " !!!!! Variable timer ticks (CONFIG_NO_HZ defined)\n");
      }
#else
      post_variable_timer_ticks = 0;
#endif

      if (gv.oprof)
      {
         // For Linux, read kallsyms size and module size
         nonsection_size  = read_eint64((uchar *)&tbuf[KALLSYMS_SIZE_OFFSET]);
         nonsection_size += read_eint64((uchar *)&tbuf[MODULES_SIZE_OFFSET]);
      }

      fsec_init(fnrm2, hsize, 1);
      return(BP_OK);
   }

   // DEPRECATED - long long time ago, ...
   else
   {                                    // old Normal2
      nrm2 = 0;
      post_num_cpus = 1;

      fsec_init(fnrm2, PERFHEADER_BUFFER_SIZE, 0);

      if ( (unsigned char)tbuf[79] == 0xCA)
      {
         fprintf(gc.msg, " %s => LITTLE Endian\n", fnm);

         big_endian = 0;
         return(BP_OK);
      }
      else if ( (unsigned char)tbuf[76] == 0xCA)
      {
         fprintf(gc.msg, " %s => BIG Endian\n", fnm);
         big_endian = 1;
         return(BP_OK);
      }
      else
      {
         ErrVMsgLog("ERROR : Invalid Normal2 hdr : %s\n", fnm);
         return(BP_ERROR);
      }
   }
}

/*******************************************/
void close_raw_file()
{
   fclose(fnrm2);
}

/*******************************************/
void reset_raw_file()
{
   safe_fseeko(fnrm2, 0, SEEK_SET);
}

/*******************************************/
int safe_fseeko(FILE *stream, uint64 offset, int whence)
{
   int rc;

#ifdef _LINUX
   rc = fseeko(stream, offset, whence);
#else
   rc = fseek(stream, (long)offset, whence);
#endif

   return(rc);
}
