/*   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2007
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
#ifndef __BPUTIL_H
   #define __BPUTIL_H

#define READ_64_RAWBUF(tbuf, tpos) *((uint64 *)(tbuf+tpos)); \
									tpos += sizeof(uint64_t)

   #include <encore.h>
   #include <bputil_common.h>

   #include <stddef.h>

   /* limits */
   #define TBUF_SIZE     4096  /* read temp buffer size */
   #define MAX_INTS       128
   #define MAX_STRS         8
   #define MAX_STR_LEN   2048
   #define MAX_SHOWX_LEN 2036 // 2048 - 12? then should be 2048 - 16


   #define KALLSYMS_SIZE_OFFSET offsetof(PERFHEADER, kallsyms_size)
   #define MODULES_SIZE_OFFSET  offsetof(PERFHEADER, modules_size)

   typedef struct _trace_rec trace_t;
   struct _trace_rec {
      int64 file_offset;
      int   hook_type;
      int   hook_len;
      int   cpu_no;
      int   major;
      int   minor;
      int64 time;
      int   ints;
      int64 ival[MAX_INTS];              // 128
      int   strings;
      int   slen[MAX_STRS];              // 8
      uchar sval[MAX_STRS][MAX_STR_LEN]; // 8 x 2K
      int   mets;       // MM cnt
      int   pad;
      int64 met[8];  // MM hooks
      char  raw[MAX_STR_LEN];  // raw file data
   };

   // filesec_t / cpu
   typedef struct _filesec filesec_t;
   struct _filesec {
      UINT64       ts_stt; // ts stt of cpu section
      UINT64       ts_del; // del(+ from earliest)
      UINT64       ts;     // ts of read-ahead
      UINT64       beg;    // file offset : begin sec
      UINT64       end;    // file offset : end   sec
      UINT64       curr;   // file offset : curr  sec
      trace_t    * tp;     // next trace record
      int          eos;    // end of section(1)
      int          ra;     // read ahead(1)
      int          cpu;    // cpu
   };

   struct offset_node{
     UINT64               file_offset;
     struct offset_node * next_ptr;
     UINT32               section_size;
   };

extern trace_t  * tr;
extern filesec_t * afsp[];      // array of file section ptrs
extern filesec_t * afsp_mte[];  // array of file section ptrs

extern int post_num_cpus;	// number of cpus
extern int post_slow_tsc;	// number of "slow" timestamps
extern int post_variable_timer_ticks;  // Linux-only: Whether or not CONFIG_NOT_HZ is defined

   /* prototypes  */
   int       init_raw_file(char * fnm);
   void      close_raw_file(void);
   void      reset_raw_file(void);
   trace_t * get_next_rec(void);
   trace_t * get_next_rec_mte(void);
   trace_t * get_next_rec_final_mte(void);
   int       init_list_ptr_trace(void);
   int       init_list_ptr_mte(void);
   void      init_last_mte(void);
   int       populate_list_ptr(FILE * fd);
   int       safe_fseeko(FILE *stream, uint64 offset, int whence);
   void noskew(void);

#endif
