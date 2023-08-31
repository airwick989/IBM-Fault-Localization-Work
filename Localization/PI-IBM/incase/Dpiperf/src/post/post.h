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

#ifndef POST_H
#define POST_H

#include <stdio.h>
#include <stdarg.h>

#include "utils/common.h"
#include "tree.h"

// atoll() reads int64 value on Linux and AIX, _atoi64() on Windows
// fseeko() moves the file pointer on 64-bit position on Linux,
// _fseeki64() on Windows

   #if defined(_WINDOWS)
      #define atoll  _atoi64
//     #define fseeko _fseeki64  // requires Visual Studio 2005
//     #define ftello _ftelli64  // requires Visual Studio 2005
      #define ftello ftell
   #endif

   #define MAX_BUFFER_SIZE  1024

typedef struct _gd GD;
struct _gd
{
   FILE * tp;
   FILE * tpaddr;
   FILE * arc;
   FILE * xfd;
// FILE * ptree;
   FILE * rw;                           // rewrite wo ts (compression)
   FILE * show;                         // show nrm in ascii
   FILE * micro;                        // show nrm in ascii
   FILE * wi;                           // post.where
   FILE * ls;                           // load/store trace
};
extern GD gd;

typedef struct _gv GV;
struct _gv
{
   double   clip;                       // min tks % to show(def 1)
   int64    tstt;
   int64    tstp;

   double   clock_rate;                 // Cycles per second
   double   catz;                       // Cycles At Time Zero (AIX Only)

// Debug output mask definitions

   #define gc_mask_Show_Disasm         0x00000100
   #define gc_mask_Show_Files          0x00000200

   int      aixtrc;                     // aix trace
   int      trcid;                      // Sequence number for JITA2N
   int      oldjit;                     // debug to force old jit algorithm

   // common
   int      readbytes;
   char   * sympath;                    // alternate search path for symbols
   int      undup;                      // separate sym by adding stt_addr suffix
   int      oprof;                      // oprofile (i.e., Linux), two trace passes (1 to get mte)
   int      trace_type;                 // Normal, continuous, or wrap-around (Linux only)

   // tprof
   int      tpsel;                      // sel 1 of 255 trof hooks
   int      itsel;                      // sel 1 of 255 metric fields from a itrace hook
   int      twidth;                     // tprof tk field width
   int      micro;                      // micro profiling output
   int      tprof;                      // any tprof hooks => 1
   int      tottks;
   int      ntonly;
   int      nospace;
   char   * flab[8];                    // func labels in compressed tree
   char   * rlab[8];                    // title labels
   int      llev;                       // last tree out level
   int      rsel;                       // report selection vector
   char   * rselvec;                    // report selcection vector
   int      xlevs;                      // levels in xtra tree
   int      xind[5];                    //
   char     xlabs[5][4];                // x lev labs
   pTNode   stk[5];                     // stack of data
   FILE   * xfd;                        // eXtra reports
   int      pnmonly;
   int      tnmonly;
   int      cpu;                        // include cpu into pid for reports
   int      off;                        // offset report

   int      tprof_mode;                 // Tprof mode (0: time-based, 1: event-based )
   int      tprof_compat;               // Compatibility mode - do not print the mode line
   int      tprof_event;                // Event number for event-based
   int      tprof_count;                // Tprof period in microseconds for time-based,
                                        // event count for event-based

   // call flow

   #define POST_COMPACT_FUNCTION 1
   #define POST_COMPACT_KERNEL   2
   #define POST_COMPACT_PLT      4

// int      ptree;                      // callflow ptree file (ala log-rt)
   int      arc;                        // callflow arc file   (ala log-gen)
   int      cfcnt;                      // callflow hooks used
   int      aatot;                      // total callflow hooks
   int      mtrace;                     // some ss/cf hooks
   int      showss;                     // ss:1 other:0
   int      compact;                    // compact mode: 0, POST_COMPACT_FUNCTION, POST_COMPACT_KERNEL
   int      v;                          // verbose flag
   int      stap;                       // convert stap data file to nrm2
   int      use_disasm_count;           // Use disassembly instruction count instead of count from trace

   int      db;
   int      nv;                         // dont validate symbols
   int      c2pid[64];
   int      maxcpu;
   int      nmcnt;                      // NoMod Count
   int      nmstp;                      // stop after nmstp NoMod events
   int      instrs;                     // instrs in ss format
   int      showx;                      // Only show nrm2, no processing
   int      noskew;                     // no clock skew compensation
   int      a2nopt;                     // reuse a2n calls within same Sym(rc = 0) only
   int      timebase;                   // ppc. trace time base different from MHz
   int      a2ndbg;                     // full a2n debug
   int      tmod;                       // tag Module with _32bit or _64bit
   int      a2nrdup;                    // rename duplicate symbols
   int      disasm;                     // Disassembly in tprof.micro, 0 = ascii only
   int      qtrace;                     // qtrace output
   int      mqtrace;                    // multiple qtrace output (per_thread)
   int      thread_cnt;                 // Number of traced threads
   int      insert_ls;                  // If set, insert dummy l/s instead of nops in qtrace when a l/s in the trace

   hash_t   thread_table;
   hash_t   process_table;
};
extern GV gv;


char ** tokenize(char * str, int * pcnt);

INT64   nrm2_size(void);
INT64   getReadBytes(void);
int     noStopOnInvalid(void);
UINT64  getTraceTime(void);

FILE  * wopen( char * fnm, char * mode );
FILE  * xopen( char * fnm, char * mode );

extern char * i386_instruction( char *data, uint64 addr, int *, char *, int disasm_mode);

// moved to post.c
extern int int3c;                          // no of int3 hooks
extern int int3f;                          // output arc
extern int aout;                          // arc out: int3f || int3c == 1

extern pTNode root;
extern int hmaj[64 * 1024];

extern char * kmap;
extern char * knmptr;
extern int knew;

extern int64 kas_off, kas_size;
extern int64 kmd_off, kmd_size;

extern int a2n_nsf;
extern char * default_jita2n_file_prefix;
extern char * jitpre;
extern char * mrgfnm;

extern int necnt1;
extern int necnt2;

extern int a2ntestflag;
extern int a2nanysym;
extern int jita2nmsg;

extern int a2nrc[8];

extern char * gf_nrm2;

void initGV(void);
int initSymUtil();
void readnrm2(int64 nstt, int64 nstp);
void output(void);

#endif
