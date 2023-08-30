
#include <math.h>

#include "encore.h"
#include "a2n.h"
#include "perfutil.h"
#include "utils/common.h"

#include "post.h"
#include "tree.h"
#include "bputil.h"
#include "hash.h"

typedef char * STRING;
typedef unsigned long Address;

// return from inst_type call
#define INTRPT 0
#define CALL   1
#define RETURN 2
#define JUMP   3
#define IRET   4
#define OTHER  5
#define UNDEFD 6
#define ALLOC  7

char oclab[8][8] =
{ "INTRPT", "CALL", "RETURN", "JUMP", "IRET", "OTHER", "ALLOC"};

char *ku[2] = { "K", "U"};

#if 0 // defined(CONFIG_PPC64) || defined(_AIX)
int decode_inst_type( char * iptr );    // return CALL RETURN JUMP IRET or OTHER
#else
// in i386dis[64].c
int inst_type(char * opcode);           // return CALL RETURN JUMP IRET or OTHER
#endif

char * i386_instruction(
                       char * data,     /* binary instruction (up to 16 bytes) */
                       uint64 addr,     /* address */   //STJ64
                       int  * size,     /* inst length */
                       char * buf,
                       int    mode);    /* 16(0), 64(1), 32(2) bit segment, see a2n.h */

void handle_generic_mte_hook(trace_t * tr);
void handle_pidmap(trace_t * tr);
void read_jita2n(process_t * ppt);
void itrace_footer(FILE * fd);

uint64 str2uint64(unsigned char * cp);

void panic(char * why);

GV gv = {0};                            // global variables
GD gd = {NULL};                         // global FILE descriptors

trace_t  * tr = NULL;                   // set by get_next_rec
UINT64 currtt = 0;                      // curr trace time

// output arc : int2f || (int3c == 1)
// int3 is deprecated ???
int int3c = 0;                          // no of int3 hooks
int int3f = 1;                          // output arc
int aout  = 1;                          // arc out: int3f || int3c == 1
int int3x = 0;                          // use to init arc after int3 stt

pTNode root;                            // tprof tick database
int   dlev[5] = {0};

int     hmaj[64 * 1024];
char  * hmajlab[64 * 1024];
int     ssring[4]          = {0};
void    output(void);

char ban[40] = {"================================"};

// default reports
char rlev[16][6] = {
   "1", "13","134","12","123","1234","3", "34", "345"};
int  ropt[16] = {1, 0, 1, 0, 0, 1, 1, 1, 0};   // nt report vector

char tlabx[8][10] = { "", "Process", "Thread", "Module", "Symbol", "Offset"};
char * tlab[8] = { "", "PID", "TID", "MOD", "SYM", "OFF"};

// global double time
double trstt, trstp;                    // trace time: sec = (stp - stt) / cycles/sec

char * kmap   = {""};                   // kernel map file def
char * knmptr = {"/usr/src/linux/vmlinux"};   // kernel file def
char * kas    = "/proc/kallsyms";
char * kmd    = "/proc/modules";
int knew = 0;                           // k => 0x1 kmap => 0x2   kas/kmod => 0x4

int64 kas_off = 0, kas_size = 0;
int64 kmd_off = 0, kmd_size = 0;

int a2n_nsf = 0;                        // Section name instead of NoSymbols/NoSymbolFound */
char * default_jita2n_file_prefix = "log";
char * jitpre = NULL;
char * mrgfnm = NULL;                   // merge  file def

// 0(first look) 1(finished) 2(ptr to unused JREC)
// replace with entry in process struct
//JTNM * pidtnm[64 * 1024];

// a2n rc histogram
int a2nrc[8] = {0};

// call flow stats
int cfbytes  = 0;
int cfbbs    = 0;

unsigned int cfinsts  = 0;
int hbys[66] = {0};                     // log-histogram of cf bytes executed
int hto[66]  = {0};                     // log-histogram of ct "to" distance(signed)


POST_CPU_DATA post_cpu_data[MAX_CPUS] = {0};

int InterNo = -1;                       // interrupt vector ( different for IA64 vs other ??)

int a2ncalls = 0;

// cpu# ring lastopc instcnt @ offset method:module pid_tid_pidname_thrdname lastopcStr linenum
char * farcd = " %2d %s %d  %5d %c %4x %s:%s   %x_%x_%s_%s %s %d\n";

int necnt1 = 0;                         // prev cf hk
int necnt2 = 0;                         //

// aix temp hack
char * gv_tppnm;

#ifdef _AIX
int aix = 1;
#else
int aix = 0;
#endif

int a2ntestflag = 0;
int a2nanysym = 0;
int jita2nmsg = 0;

double getTime(int cpu);

char * gf_nrm2;


#if 0 // defined(CONFIG_PPC64) || defined(_AIX)
/************************/
int decode_inst_type( char * iptr )
{
   int oc = OTHER;                      // Other

   unsigned long opcode = *( (unsigned long *)iptr );

   if ( 0x4C000020 == ( opcode & 0xFC0007FE ) )   // Includes bclrl (RETURN-CALL)
   {
      oc = RETURN;                      // RETURN instruction
   }
   else if ( 0x40000000 == ( opcode & 0xF400000 ) )
   {
      oc = ( opcode & 1 ) ? CALL : JUMP;   // LK selects CALL or JUMP instruction
   }
   else if ( 0x4C000420 == ( opcode & 0xFC0007FE ) )
   {
      oc = ( opcode & 1 ) ? CALL : JUMP;   // LK selects CALL or JUMP instruction
   }

   if ( gc.verbose )
   {
      fprintf( gc.msg, "DECODE_INST_TYPE: %08X = %d\n",
               *( (unsigned long *)iptr ),
               oc );
      fflush( gc.msg );
   }
   return(oc);
}
#endif

// ********************
// **** CODE START ****
// ********************

/******************************/
void err_exit(char * why)
{
   if ( why )
   {
      ErrVMsgLog("\nPOST EXITING <%s>\n", why);
   }
   else
   {
      ErrVMsgLog("\nPOST EXITING\n");
   }
   exit(0);
}

void writeStderr(char * msg)            // Allow hooking of stderr
{
   fprintf(stderr, "%s", msg);
}

void writeStdout(char * msg)            // Allow hooking of stdout
{
   fprintf(stdout, "%s", msg);
}

/**********************************/
FILE * wopen(char * fnm, char * mode)   // Weak Open of files, failure allowed
{
   FILE * fd = NULL;

   if ( 0 != ( fd = fopen(fnm, mode) ) )
   {
      if ( 'w' == *mode )
      {
         ErrVMsgLog( "Writing %s\n", fnm );
      }
      else
      {
         ErrVMsgLog( "Reading %s\n", fnm );
      }
   }
   else
   {
      MVMsg( gc_mask_Show_Files, ( "Unable to open optional file: %s\n", fnm ) );
   }
   return(fd);
}

/**********************************/
FILE * xopen(char * fnm, char * mode)   // Open with eXit on failure
{
   FILE * fd = NULL;

   if ( 'w' == *mode )
   {
      ErrVMsgLog( "Writing %s\n", fnm );
   }
   else
   {
      ErrVMsgLog( "Reading %s\n", fnm );
   }

   if ((fd = fopen(fnm, mode)) == NULL)
   {
      fprintf(stderr, "\nError opening required file: %s\n", fnm);
      exit(1);
   }
   return(fd);
}

#if defined(USE_LIBOPCODES)

   #include "dis-asm.h"
   #include <stdarg.h>

/* Pseudo FILE object for strings.  */
/**********************************/
typedef struct
{
   char * buffer;
   size_t size;
   char * current;
} SFILE;

/* sprintf to a "stream" */

/**********************************/
static int
stream_sprintf( SFILE *f, const char *format, ... )
{
   int     n;
   va_list args;

   va_start( args, format );

   n = vsprintf( f->current, format, args );

   f->current += n;

   va_end( args );
   return( n );
}

/**********************************/
// Disassemble an instruction
//   target = buffer to receive the output (512+ bytes)
//   insbuf = address of the instruction buffer
//   ip     = current instruction pointer

unsigned long disassemble( char *target, char *insbuf, bfd_vma ip, disassembler_ftype func )
{
   static struct disassemble_info info;
   static disassembler_ftype      disassemble_fn = 0;   // Function pointer
   static SFILE                   sf;

   sf.buffer  =
   sf.current = target;
   sf.size    = 512;

   if ( 0 == disassemble_fn )
   {
      disassemble_fn = func;

      INIT_DISASSEMBLE_INFO( info, &sf, stream_sprintf );

      info.buffer_length = 8;
   }

   info.mach          = bfd_mach_x86_64;
   info.buffer        = (unsigned char *)insbuf;
   info.buffer_vma    = (bfd_vma)ip;    // Must match first parm of call

   return( (*disassemble_fn)( ip, &info ) );
}

#endif

/**********/
/*
   0x20/0x21  call_flow(ker/app) : ft[c] fftt[c]
   0x22/0x23  call_flow(ker/app) : fft[c]
   0x24/0x25  call_flow(ker/app) : fb ffb (b=BBCC)
   0x28/0x29  Interrupt / Exception : vcff
   0x2c       cf_pidtid : LX  : tid c
   0x2c       cf_pidtid : Win : pid tid c
   0x2d       cf_pidtid : Win : pid tid c ts*
   0x2e       itracestt cpid ctid sttpid stttid mode
   0x2f       itracestp cpu cfhkcnt    (per processor)

   // other
   0x04  void handle_interrupt(trace_t * tr) {
   0x10  void handle_tprof_hook(trace_t * tr) {
   0x11  void handle_startup_hook(trace_t * tr) {
   0x12  void handle_dispatch(trace_t * tr) {
   0x19  0x01 void handle_19_1_hook(trace_t * tr) {
         0x34 void handle_19_34_hook(trace_t * tr) {
         0x63 void handle_pidmap(trace_t * tr) {   // 19/63
   0xf9  void handle_f9(trace_t * tr) {
 */

/**********************************/
UINT64 getTraceTime(void)
{
   UINT64 tmtr = 0;                     // curr trace time

   if (tr)
   {
      tmtr = tr->time;
   }
   else
   {                                    // @ trace end
      return(currtt);
   }

   return(tmtr);
}

/**********************************/
// A2nGetSymbol wrapper
int getSymbol(unsigned int pid, uint64 addr, SYMDATA * sdp)
{                                       //STJ64
   int         rc = 0;
   char      * cp;
   process_t * ppt;

   // handle pid = -1
   if (pid != 0xffffffff)
   {
      rc = A2nGetSymbol(pid, addr, sdp);

      if (sdp->code != NULL)
      {
         cp = (char *)sdp->code;        // code at addr given
         // move from addr to sym_addr
         sdp->code = cp + (int)(sdp->sym_addr - addr);

         if ( sdp->flags & SDFLAG_SYMBOL_JITTED )
         {
            ppt = insert_process( pid );

            sdp->flags |= ppt->flags;   // Set the flags 32/64-bit flag for JITTED methods for this PID
         }
      }
   }
   else
   {
      // fill in struct
      rc = 3;                           // NoModule
      sdp->owning_pid_name = "MissingPID";
      sdp->owning_pid = pid;
      sdp->mod_name   = "NoModule";
      sdp->mod_addr   = 0;
      sdp->mod_length = 0;
      sdp->sym_name   = "NoSymbol";
      sdp->code       = NULL;
      sdp->src_file   = "MissingSrcFile";
      sdp->src_lineno = -1;
      sdp->process    = NULL;
      sdp->module     = NULL;
      sdp->symbol     = NULL;
      sdp->flags      = 0;
   }
   return(rc);
}

/**********************************/
int getlineno(SYMDATA * s)
{
#ifndef NOLINENOS
   return(s->src_lineno);
#else
   return(-2);
#endif
}

/**********************************/
// ia64 (ring) ia32 (t->ring) ???
void arcflush(thread_t * tp)
{
   SYMDATA * sdp = &(tp->sd);

   cfinsts   += tp->ci;
   tp->cfcnt += tp->ci;

   if ( gc.verbose )
   {
      fprintf( gc.msg,
               farcd,                   // Format string
               tp->cpu,                 // CPU
               ku[tp->ring],            // Ring ( Kernel or User )
               tp->oc,                  // Opcode Type (call/ret/jump/iret/other)
               tp->ci,
               '@',
               tp->offset,
               sdp->sym_name,
               sdp->mod_name,
               tp->pid,
               tp->tid,
               sdp->owning_pid_name,
               tp->thread_name,
               oclab[tp->oc],
               getlineno(sdp) );
   }

   fprintf( gd.arc,
            farcd,                      // Format string
            tp->cpu,                    // CPU
            ku[tp->ring],               // Ring ( Kernel or User )
            tp->oc,                     // Opcode Type (call/ret/jump/iret/other)
            tp->ci,
            '@',
            tp->offset,
            sdp->sym_name,
            sdp->mod_name,
            tp->pid,
            tp->tid,
            sdp->owning_pid_name,
            tp->thread_name,
            oclab[tp->oc],
            getlineno(sdp) );

   fflush(gd.arc);

   tp->ci = 0;
}

/**********************************/
void arcbuf( thread_t * tp,
             SYMDATA * s,
             int tcnt,
             int offset,
             int ring,
             int oc,
             int cpu,
             int oc1)
{
   int k_flush_cond = 0;
   int p_flush_cond = 0;
   int f_flush_cond = 0;

   // We can have:
   // - No compaction
   // - Any combination of the following compactions:
   //    - function compaction (successive branches compacted to 1)
   //    - <plt> compaction (roll up with the previous branch, if it is user)
   //    - kernel compaction - all kernel branches between user branches
   //      compacted to 1


   if (0 == gv.compact)
   {
      // No compaction, just flush current
      tp->ci          = tcnt;
      tp->cpu         = cpu;
      tp->oc          = oc;
      tp->ring        = ring;
      tp->offset      = offset;
      tp->sd = *s;

      arcflush(tp);
      return;
   }

   // Else some compaction, calculate flush conditions

   if (gv.compact & POST_COMPACT_FUNCTION)
   {
      // For function compaction,
      // flush if a different function/module/too many
      if ( (tp->sd.sym_name != s->sym_name) ||
           (tp->sd.mod_name != s->mod_name) ||
           (tp->ci >= 1000) )
         f_flush_cond = 1;
   }

   if (gv.compact & POST_COMPACT_PLT)
   {
      // For <plt> compaction, flush if not <plt>
      // or a previous branch was a kernel one
      if ( (0 != strncmp(s->sym_name, "<plt>", 5)) ||
           (tp->ring == 0) )
         p_flush_cond = 1;
   }

   if (gv.compact & POST_COMPACT_KERNEL)
   {
      // For kernel compaction,
      // flush if now comes a user branch after a kernel branch
      if ( (1 == ring) &&
           (0 == tp->ring) )
         k_flush_cond = 1;
   }

   if (tp->ci != 0)
   {
      // We have something buffered,
      // Do we need to flush it?

      if (k_flush_cond)
      {
         arcflush(tp);
      }
      else
      {
         if ( p_flush_cond &&
              (!(gv.compact & POST_COMPACT_FUNCTION) || f_flush_cond) &&
              (!(gv.compact & POST_COMPACT_KERNEL) || (ring == 1) || (tp->ring == 1) ) )
         {
            arcflush(tp);
         }
         else
         {
            if ( f_flush_cond &&
                 ( !(gv.compact & POST_COMPACT_KERNEL) || (ring == 1) || (tp->ring == 1) )  &&
                 ( !(gv.compact & POST_COMPACT_PLT) || p_flush_cond  ) )
            {
               arcflush(tp);
            }
         }
      }
   }


   // Do we need to buffer?
   if ( ((gv.compact & POST_COMPACT_KERNEL) && (0 == ring)) ||
        (gv.compact & POST_COMPACT_PLT) ||
        (gv.compact & POST_COMPACT_FUNCTION) )
   {
      if ( gv.compact & POST_COMPACT_FUNCTION && (tp->sd.sym_name == s->sym_name) )
      {
         tp->oc = oc;                   // Last bb, so we keep call/return
      }

      if (tp->ci == 0)
      {                                 // first bb
         tp->offset = offset;           // offset from 1st bb
         tp->sd     = *s;               // name from 1st bb
         tp->oc     = oc;               // type from first bb
      }

      // For all compaction options, set cpu/ring to the last bb values
      tp->ci  += tcnt;
      tp->cpu  = cpu;
      tp->ring = ring;

   }
   else                                 // No need to buffer
   {
      tp->ci          = tcnt;
      tp->cpu         = cpu;
      tp->oc          = oc;
      tp->ring        = ring;
      tp->offset      = offset;
      tp->sd = *s;

      arcflush(tp);
   }
}


/**********************************/
// for writing breaks, vectors, etc.
// ia64 (ring) ia32 (t->ring) ???
void arcout( thread_t * tp,
             int        tcnt,
             char       dir,
             int        offset,
             char     * mnm,
             char     * modnm,
             int        ring,
             int        oc,
             char     * lab,
             int        cpu)
{
   SYMDATA * sdp = &(tp->sd);

   if (tp->ci >= 1)
   {
      arcflush(tp);
   }

   cfinsts += tcnt;

   if (sdp->owning_pid_name == NULL)
   {
      sdp->owning_pid_name = A2nGetProcessName(tp->pid);
   }

   fprintf( gd.arc,
            farcd,
            cpu,
            ku[ring],
            oc,
            tcnt,
            dir,
            offset,
            mnm,
            modnm,
            tp->pid,
            tp->tid,
            sdp->owning_pid_name,
            tp->thread_name,
            "-",
            getlineno(sdp) );
}

/**********************************/
void pt_flush(int pid, int tid)
{
   thread_t * tp;

   tp = insert_thread(tid, pid);

   if ( (aout) && (tp->ci >= 1) )
   {
      fprintf(gd.arc, " ## arcflush %x %x\n", pid, tid);
      fflush(gd.arc);

      arcflush(tp);

      fprintf(gd.arc, " ## arcflush %x %x\n", pid, tid);
      fflush(gd.arc);
   }
}

/**********************************/
void allpt_flush(void)
{
   thread_t * tp;
   int cpu, pid, tid;

   fprintf(gd.arc, " ## allpt_flush\n");
   fflush(gd.arc);

   for (cpu = 0; cpu < MAX_CPUS; cpu++)
   {
      if (post_cpu_data[cpu].stt == 1)
      {
         pid = post_cpu_data[cpu].pid;
         tid = post_cpu_data[cpu].tid;
         tp  = post_cpu_data[cpu].tp;

         fprintf(gd.arc, " ## pt_flush @ %x %x\n", pid, tid);
         fflush(gd.arc);

         pt_flush(pid, tid);

         fprintf(gd.arc, " ## retn pt_flush @ %x %x\n", pid, tid);
         fflush(gd.arc);
      }
   }
}

/**********************************/
void cf_pidtid(trace_t * tr)
{
   int cpu, pid, tid, opid, otid;
   thread_t * tp;
   int tcnt = 0;

   //  0x2c  pid tid c       // lx (1 int)
   //  0x2c  pid tid c       // nt
   //  0x2d  pid tid c [ts]* // nt64

   cpu = tr->cpu_no;

   pid  = tr->minor;
   tid  = tr->ival[0];
   tcnt = tr->ival[1];

#if defined(_LINUX)
   fprintf(gd.arc, " tid %x pid %x\n", tid, pid);
   fflush(gd.arc);
#endif

   if ( (pid == 0) && (tid == 0) )
   {
      tid = cpu;                        // SMP - Idle separation
   }

   if (post_cpu_data[cpu].stt)
   {
      opid = post_cpu_data[cpu].pid;
      otid = post_cpu_data[cpu].tid;
      pt_flush(opid, otid);             // arcflush
   }

   tp = insert_thread(tid, pid);

   post_cpu_data[cpu].stt = 1;
   post_cpu_data[cpu].pid = pid;
   post_cpu_data[cpu].tid = tid;
   post_cpu_data[cpu].tp  = tp;

   arcout(tp, tcnt, '?', 1, "callflow", "disp", 0, 0, "-disp-", cpu);
}

/**********************************/
void handle_dispatch(trace_t * tr)
{
   unsigned int pid;
   unsigned int tid;
   int cpu, tcnt;

   //      minor
   // win: pid   ts tid tcnt
   // lx:  tid   ts tcnt

   cpu  = tr->cpu_no;
   pid  = tr->minor;
   tid  = tr->ival[0];
   tcnt = tr->ival[1];                  //

   if ( (pid == 0) && (tid == 0) )
   {
      tid = cpu;
   }
}

/**********************************/
// not a callflow hook
void handle_interrupt(trace_t * tr)
{
   int cpu, mnr, tcnt;

   // pid mnr ts tid tcnt
   cpu  = tr->cpu_no;
   mnr  = tr->minor;

   if (post_cpu_data[cpu].stt == 0) return;

   if (mnr == 0x80000001)
   {
      if (tr->ints == 1)
      {                                 // IA64 only
         tcnt = tr->ival[0];
      }
   }
}

/**********************************/
void handle_19_1_hook(trace_t * tr)
{
   char modnm   [MAX_STR_LEN];
   unsigned int pid;
   int size, rc, csum, tstp;
   uint64 addr;                         //STJ64
   static int a2nerr = 0;

   // int : pid size tstp csum alo [ahi]
   // str : modnm

   pid  = tr->ival[0];
   size = tr->ival[1];
   tstp = tr->ival[2];
   csum = tr->ival[3];

   addr = (uint64)tr->ival[4];

   memcpy(modnm, &tr->sval[0], tr->slen[0]);
   modnm[tr->slen[0]] = 0;              /* null terminate it*/

#if defined(_LINUX)
   if (strcmp(modnm, PI_VDSO_SEGMENT_NAME) == 0) {
      tstp = TS_VDSO;
   }
#endif

   rc = A2nAddModule( pid,
                      addr,             // mod stt address //STJ64
                      size,
                      modnm,
                      tstp,             // timestamp(or 0)
                      csum,             // checksum(or 0)
                      NULL);

   if (rc != 0)
   {
      a2nerr++;
      if (a2nerr <= 5)
      {
         fprintf(stderr, " A2nAddModule() ERROR rc %d\n", rc);
      }
      else
      {
         if (0 == a2nerr % 100)
         {
            fprintf(stderr, " %d A2nAddModule() ERRORs rc %d\n", a2nerr,rc);
         }
      }
   }

   return;
}

/**********************************/
void a2ntest(void)
{
   int     pid;
   char    mnm[100];
   uint64  addr;                        //STJ64
   int     size, rc;
   SYMDATA sd;
   SYMDATA * sdp = &sd;
   // char          *owning_pid_name;  // nm "owning" process
   // unsigned int  owning_pid;        // pid of "owning" process
   // char          *mod_name;         // Nm of module
   // char          *mod_addr;         // Mod starting (load) virtual address
   // unsigned int  mod_length;        // Mod length (in bytes)
   // char          *sym_name;         // Sym name
   // char          *sym_addr;         // Sym starting virtual address
   // unsigned int  sym_length;        // Sym length (in bytes)

   pid  = 17;
   addr = 0x40000;                      //STJ64
   size = 0x1000;

   rc = A2nAddModule(pid, 0x30000, 0x1000,   //STJ64
                     "/home/bob/cust/aixtools/post.exe",
                     0, 0, NULL);
   OptVMsg("\n *** A2nAddModule rc %d\n", rc);

   pid  = 17;
   addr = 0x5c000;                      //STJ64
   size = 0x1000;

   OptVMsg("\n *** JIT load Data\n");
   OptVMsg(" JIT load Data\n");
   OptVMsg(" pid      %x\n", pid);
   OptVMsg(" stt_addr %016" _P64 "X\n", addr);   //STJ64
   OptVMsg(" size     %x\n", size);

   strcpy(mnm, "java/sun/io/writeByte(int c)Z;");
   rc = A2nAddJittedMethod(pid, mnm, addr, (int)size, NULL);   //STJ64
   OptVMsg(" A2nAddJittedMethod rc %d\n", rc);

   addr = 0x5c150;                      //STJ64
   rc = getSymbol(pid, addr, sdp);
   OptVMsg("\n *** getSymbol rc %d\n", rc);

   OptVMsg("\n JIT lookup input Data\n");
   OptVMsg(" pid      %x\n", pid);
   OptVMsg(" addr     %016" _P64 "X\n", addr);   //STJ64

   OptVMsg("\n JIT lookup Results\n");
   OptVMsg("     tgid %8x\n",           sdp->owning_pid);
   OptVMsg("    modnm <%s>\n",          sdp->mod_name);
   OptVMsg("      mnm <%s>\n",          sdp->sym_name);
   OptVMsg("  symaddr %016" _P64 "X\n", sdp->sym_addr);   //STJ64
   OptVMsg("   offset %016" _P64 "X\n", addr - sdp->sym_addr);   //STJ64

   exit(-1);
}


#if defined(_LINUX)
   #if defined(_AMD64)
      #include <sys/user.h>
      #include <asm/vsyscall.h>
const uint64_t vsyscall32_base = (uint64_t)0xFFFFE000;
const uint64_t vsyscall32_end  = (uint64_t)0xFFFFF000;
     #include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
const uint64_t vsyscall64_base = (uint64_t)VSYSCALL_START;
const uint64_t vsyscall64_end  = (uint64_t)VSYSCALL_END;
#else
const uint64_t vsyscall64_base = (uint64_t)VSYSCALL_ADDR;
const uint64_t vsyscall64_end = (uint64_t)VSYSCALL_ADDR;
#endif
   #elif defined(_X86)
const uint64_t vsyscall32_base = (uint64_t)0xFFFFE000;
const uint64_t vsyscall32_end  = (uint64_t)0xFFFFF000;
const uint64_t vsyscall64_base = 0;
const uint64_t vsyscall64_end  = 0;
   #else
const uint64_t vsyscall32_base = 0;
const uint64_t vsyscall32_end  = 0;
const uint64_t vsyscall64_base = 0;
const uint64_t vsyscall64_end  = 0;
   #endif
#endif


/**********************************/
int initSymUtil()
{
   unsigned long v;
   int rc;

#if defined(_LINUX)
   int64  * pzap;
   FILE   * nrmfh;
   char     trace_header[512];
#endif

   // options on NoSymbolFound
   // def           : NoSymbolFound
   // section name  : e.g. .text
   // null          : NULL
   // SymbolQuality
   // any symbol
   // (def)debug symbols (all)
   // SearchPath (list of dirs)
   // LX : def
   // in image in addMod, *.map, map in other dirs
   // NT :
   // def(env vars), add some


   A2nInit();                           // REQUIRED for _ZOS, harmless for all others

   v = A2nGetVersion();
   fprintf(gc.msg, " A2nVersion: %d.%d.%d\n",
           (int)A2N_VERSION_MAJOR(v), (int)A2N_VERSION_MINOR(v), (int)A2N_VERSION_REV(v));

   // a2n debug
   if (gv.a2ndbg == 1)
   {
      A2nSetDebugMode(DEBUG_MODE_ALL);
   }

   if (a2nanysym == 1)
   {
      A2nSetSymbolQualityMode(MODE_ANY_SYMBOL);
   }

   // a2n rename duplicate symbols
   //  BADDOG. add/stub to LX & AIX
#ifdef _WINDOWS
   if (gv.a2nrdup == 1)
   {
      A2nRenameDuplicateSymbols();
   }
#endif

   // standalone a2n test
   if (a2ntestflag == 1)
   {
      // test a2n w/o nrm2
      a2ntest();
   }

   if (gv.sympath != NULL)
   {                                    // List of paths
      A2nSetSymbolSearchPath(gv.sympath);
   }

   // Tell A2N what to return if NoSymbols or NoSymbolFound:
   //   a2n_nsf = 1: Return NoSymbols or NoSymbolFound
   //   a2n_nsf = 0: Return <<section_name>> or NoSymbols
   if (a2n_nsf)
   {
      A2nReturnNoSymbolFoundOnNSF();
   }
   else
   {
      A2nReturnSectionNameOnNSF();
   }

   // add post option to error control
   A2nSetErrorMessageMode(4);           // E:1 W:2 I:3 A:4
   A2nLazySymbolGather();
   // only if -off option
   A2nGatherCode();                     // A2nDontGatherCode();
   A2nSetSystemPid(0);                  // System Pid

#if defined(_LINUX)
   // Add the VSYSCALL virtual address range
   // As of kernel versions >= 2.6.18 the syscall stub (which used to be
   // mapped at 0xFFFFE000) was replaced with the VDSO segment ([vdso] in
   // the /proc/<pid>/maps file). The VDSO segment may or may not be
   // mapped at a fixed address for all processes. MTE data should contain
   // a 19/01 hook for the VDSO segment for each PID.
   // Adding the VSYSCALL page should not make a difference even if the
   // VDSO segment happens to be mapped at the same address because it
   // comes later and will be seen firts when A2N searches for a loaded
   // module.
   if (vsyscall32_base)
   {
      int vsyscall32_size = (int)(vsyscall32_end - vsyscall32_base);
      A2nAddModule(0, vsyscall32_base, vsyscall32_size, A2N_VSYSCALL32_STR, TS_VSYSCALL32, 0, NULL);
   }
   if (vsyscall64_base)
   {
      int vsyscall64_size = (int)(vsyscall64_end - vsyscall64_base);
      A2nAddModule(0, vsyscall64_base, vsyscall64_size, A2N_VSYSCALL64_STR, TS_VSYSCALL64, 0, NULL);
   }
#endif


#ifndef NOLINENOS
   #if defined(WIN32) || defined(WINDOWS) || defined(_LINUX)
   if ( (gv.off == 1) || (gv.arc) )
   {
      A2nReturnLineNumbers();
   }
   #endif
#endif

#if defined(_LINUX)
   if (2 == (knew & 2) )
   {
      rc = A2nSetKernelMapName(kmap);
      if (rc != 0)
      {
         fprintf(gc.msg, " post: A2nSetKernelMapName() rc %d\n", rc);
         fprintf(stderr, " ================================\n");
         fprintf(stderr, "\n FATAL ERROR : %s Not Found\n\n", kmap);
         fprintf(stderr, " ================================\n");
         exit(-1);
      }
   }

   if (1 == (knew & 1) )
   {
      rc = A2nSetKernelImageName(knmptr);
      if (rc != 0)
      {
         fprintf(gc.msg, " post: A2nSetKernelImageName() rc %d\n", rc);
         fprintf(stderr, " ================================\n");
         fprintf(stderr, "\n FATAL ERROR : %s Not Found\n\n", knmptr);
         fprintf(stderr, " ================================\n");
         exit(-1);
      }
   }

   if (4 == (knew & 4))
   {
      rc = A2nSetKallsymsFileLocation(kas, 0, 0);
      if (rc != 0)
      {
         fprintf(gc.msg, " post: A2nSetKallsymsFileLocation() rc %d (%s)\n", rc, A2nRcToString(rc));
         fprintf(stderr, " ================================\n");
         fprintf(stderr, "\n FATAL ERROR : %s Not Found\n\n", kas);
         fprintf(stderr, " ================================\n");
         exit(-1);
      }
      rc = A2nSetModulesFileLocation(kmd, 0, 0);
      if (rc != 0)
      {
         fprintf(gc.msg, " post: A2nSetModulesFileLocation() rc %d (%s)\n", rc, A2nRcToString(rc));
         fprintf(stderr, " ================================\n");
         fprintf(stderr, "\n FATAL ERROR : %s Not Found\n\n", kmd);
         fprintf(stderr, " ================================\n");
         exit(-1);
      }
   }
   else
   {
      nrmfh = xopen(gf_nrm2, "r");
      if (nrmfh)
      {
         fseek(nrmfh, 0, SEEK_SET);
         if (fread(trace_header, sizeof(char), 512, nrmfh) != 512)
         {
            ErrVMsgLog("Post cannot read trace header\n");
            fclose(nrmfh);
            exit(-1);
         }
         pzap = (int64 *)&trace_header[offsetof(PERFHEADER, kallsyms_offset)];
         kas_off  = *pzap;              // offset to copy of kallsyms
         pzap++;
         kas_size = *pzap;              // size of kallsyms
         pzap++;
         kmd_off  = *pzap;              // offset to copy of modules
         pzap++;
         kmd_size = *pzap;              // size of modules
         OptVMsg("kas_off: 0x%016"_P64x", kas_size: 0x%016"_P64x", kmd_off: 0x%016"_P64x", kmd_size: 0x%016"_P64x"\n",
                 kas_off, kas_size, kmd_off, kmd_size);
         fclose(nrmfh);
      }
      if (kas_size != 0 && kmd_size != 0)
      {
         rc = A2nSetKallsymsFileLocation(gf_nrm2, kas_off, kas_size);
         if (rc != 0)
         {
            fprintf(gc.msg, " post: A2nSetKallsymsFileLocation() rc %d (%s)\n", rc, A2nRcToString(rc));
            fprintf(stderr, " ================================\n");
            fprintf(stderr, "\n FATAL ERROR : %s Not Found\n\n", gf_nrm2);
            fprintf(stderr, " ================================\n");
            exit(-1);
         }
         rc = A2nSetModulesFileLocation(gf_nrm2, kmd_off, kmd_size);
         if (rc != 0)
         {
            fprintf(gc.msg, " post: A2nSetModulesFileLocation() rc %d (%s)\n", rc, A2nRcToString(rc));
            fprintf(stderr, " ================================\n");
            fprintf(stderr, "\n FATAL ERROR : %s Not Found\n\n", gf_nrm2);
            fprintf(stderr, " ================================\n");
            exit(-1);
         }
      }
   }
#endif

   if (mrgfnm != NULL)
   {
      rc = A2nLoadMsiFile(mrgfnm);
      if (rc != 0)
      {
         fprintf(gc.msg, " post: A2nLoadMsiFile() rc %d\n", rc);
      }
   }

   if (gv.nv == 1)
   {
      A2nDontValidateKernel();
   }

   OptVMsg(" Leaving initSym\n");

   return 0;
}

/**********************************/
uint64 str2uint64(unsigned char * cp)
{
   uint64       d = 0;
   unsigned int r;

#ifdef _ZOS
   sscanf(cp, "%lx", &d);
   return(d);
#endif

   while ((r = *cp) != '\0')
   {
      if (     (r & 0xf0) == 0x30)  r = r & 0xf;   /* 0-9 */
      else if ((r & 0xf0) == 0x60)  r = (r & 0x7) + 9;   /* a-f */
      else if ((r & 0xf0) == 0x40)  r = (r & 0x7) + 9;   /* A-F */
      else                         r = 0;   /* 0xX */
      d = (d << 4) + r;
      cp++;
   }
   return(d);
}


/**********************************/
char * hex2bin(char * cp, char * rec)
{
   int i, d, d1 = 0, d2 = 0, len;
   char h1, h2;

   int mcnt = 0;
   static int cnt = 0;

   cnt++;
   len = (int)strlen(rec) - 1;

   if (cnt <= mcnt)
   {
      OptVMsg(" len %d\n", len);        // FIXME!!!!!  Add check for oversize line
   }

   for (i = 0; i < len; i += 2)
   {
      h1 = *rec;
      rec++;
      h2 = *rec;
      rec++;

      if ( (h1 >= '0') && (h1 <= '9') ) d1 = h1 - '0';
      if ( (h1 >= 'a') && (h1 <= 'f') ) d1 = h1 - 'a' + 10;
      if ( (h2 >= '0') && (h2 <= '9') ) d2 = h2 - '0';
      if ( (h2 >= 'a') && (h2 <= 'f') ) d2 = h2 - 'a' + 10;

      d = 16 * d1 + d2;

      *cp = (unsigned char)d;
      cp++;

      if (mcnt >= cnt)
      {
         OptVMsg(" i %d h1 %c h2 %c\n", i, h1, h2);
         OptVMsg("   - d1 %x d2 %x d %x\n", d1, d2, d);
      }
   }
   return(cp);
}

/**********************************/
void read_jita2n( process_t * ppt )
{
   // Called by jitrec, & 19/41 & 19/42
   // use by time | id

   char fnm[ 128];
   char ftnm[128];
   char hrec[128];                      // hex code records
   char rec[2048];                      // jit or jtnm records
   FILE * fd;
   unsigned long long int ttime		= 0;	// trace time
   unsigned long long int ftime		= 0;	// file time
   unsigned long long int mintime	= 0;	// min file time
   char ** tokarr;
   char * mnm;
   char   mnmbuf[2048];

   uint64 addr;
   int size;

   int saw_printable_hex = 0;
   int gave_msg = 0;

   int  tcnt  = 0;
   int  rcnt  = 0;                      // jitted method records
   int  dcnt  = 0;                      // delayed records

   JREC      * pJREC    = NULL;
   JREC      * prevJREC = NULL;
   JTNM      * pJTNM    = NULL;
   JTNM      * prevJTNM = NULL;

   // Jitted Code
   char * cp;
   int    cflag;

   // thread names
   int tid;
   char * tnm;
   int usenow = 0;

   int bytime;
   int byid;
   int tnmcnt = 0;

   int pid = PtrToUint32(ppt->pid);

#if defined(_PPC64) && !defined(_LINUX) // jit & trace time problems
   // jit  : Read and use all. breaks if jit space reused
   // jtnm : Skip. Nothing better than wrong.
   usenow = 1;
#endif

   ttime = tr->time;                       // Current trace time

   ppt->read   = 1;
   ppt->trcid  = gv.trcid;              // for trace On/Off
   // ##ENIO## THIS DOESN'T LOOK RIGHT. WHY SET ppt->trcid ON EVERY CALL?
   if (jita2nmsg) {
      WriteFlush(gc.msg, "read_jita2n: pid=%d trcid=%d\n", pid, ppt->trcid);
   }

   // create file names (chk if files exit)
   sprintf(ftnm, "%s-jtnm_%u",   jitpre, pid);
   sprintf(fnm,  "%s-jita2n_%u", jitpre, pid);

   // THREAD NAME file
   if ( !usenow )
   {
      if ((fd = wopen(ftnm, "rb")) != NULL)
      {
         int tnmid = 0;
         thread_t * tp;

         prevJTNM = NULL;

         while (fgets(rec, 2048, fd) != NULL)
         {
        	 int toknum = 0;
            tokarr = (char **)tokenize(rec, &tcnt);

            if (tcnt == 5 && strcmp(tokarr[0], "TNM:") == 0) // thread name file
            {                           // normal rec
               tnmcnt++;
               toknum++;
               ftime = strtouq(tokarr[toknum++], NULL, 16);   // file time
               tid = strtoul(tokarr[toknum++], NULL, 16);
               tnm = tokarr[toknum++];

               OptVMsg(" tnmFile %16x %4x %s %s\n",
                       ftime, tid, tnm, tokarr[toknum]);

               // early jtnm's -> thread hash table
               bytime = ( ftime < ttime );
               byid   = tnmid < gv.trcid;

               if ( byid || ( (gv.trcid == 0) && bytime ) )
               {
                  ppt->jtnmUsed++;

                  tp  = insert_thread(tid, pid);

                  if (strcmp(tokarr[toknum], "START") == 0)
                  {
                     tp->thread_name = xStrdup("thread_name",tnm);
                  }
                  else
                  {
                     tp->thread_name = "";
                  }
               }

               // save for time matching
               else
               {
                  ppt->jtnmSaved++;

                  pJTNM = (JTNM *)zMalloc("read_jita2n", sizeof(JTNM));

                  if ( prevJTNM  )
                  {
                     prevJTNM->next = pJTNM;   // add to end
                  }
                  else
                  {
                     ppt->tnm_ll = pJTNM;   // 1st tnm entry on pid
                  }
                  prevJTNM = pJTNM;

                  pJTNM->next  = NULL;
                  pJTNM->time  = ftime;
                  pJTNM->tid   = tid;

                  if (strcmp(tokarr[toknum], "END") == 0)
                  {
                     pJTNM->tnm = "";
                  }
                  else
                  {
                     pJTNM->tnm = xStrdup("tnm",tnm);
                  }
               }
               toknum++;
            }
            else if (tcnt == 2)
            {                           // SeqID
               if (strcmp(tokarr[0], "SeqNo") == 0)
               {
                  tnmid = atoi(tokarr[1]);
               }
            }
         }
         OptVMsgLog(" %4d log-jtnm records for PID %d (0x%x). Used %d, Saved %d.\n",
                    tnmcnt, pid, pid, ppt->jtnmUsed, ppt->jtnmSaved);

         fflush(gc.msg);
         fclose(fd);
      }
   }

   // JITA2N File

   if ((fd = wopen(fnm, "rb")) != NULL)
   {
      int seqNo;                        // jit record SeqID

      OptVMsg(" pid %x TraceTime %16x\n", pid, ttime);

      seqNo    = 0;
      prevJREC = NULL;

      while (fgets(rec, 2048, fd) != NULL)
      {
         int use = 0;
         int toknum = 0;

         tokarr = (char **)tokenize(rec, &tcnt);

         if ( tcnt > 0 && '(' == tokarr[0][0] )   // Part of header detected
         {
            if ( 0 == strcmp( tokarr[0], "(Platform" ) )
            {
               ppt->flags |= ( strstr( tokarr[2], "64" ) )
                             ? SDFLAG_MODULE_64BIT
                             : SDFLAG_MODULE_32BIT;
            }
         }

         else if (tcnt == 6)
         {                              // normal rec
            int len1, len2;

            // check format : 8 characters
            len1 = (int)strlen(tokarr[0]);

            if ( len1 == 8 )
               {
               int thi = strtouq(tokarr[toknum++], NULL, 16);
               int tlo = strtouq(tokarr[toknum++], NULL, 16);
               ftime = ((unsigned long long)thi << 32) | ((unsigned long long)tlo);

               rcnt++;
               if (rcnt == 1)
               {
                  mintime = ftime;
               }

               addr = str2uint64((uchar *)tokarr[toknum++]);
               size = (int)str2uint64((uchar *)tokarr[toknum++]);

               cflag = atoi(tokarr[toknum++]);

               if ( cflag & jrec_cflag_Code )
               {                        // HEX code in file
                  if (!saw_printable_hex)
                  {
                     cflag = 0;         // Make it look like there's no code
                     if (!gave_msg)
                     {
                        ErrVMsgLog(" %s missing \"BINARY DATA IN PRINTABLE HEX\" line. Ignoring jitted code.\n", fnm);
                        gave_msg = 1;
                     }
                  }
               }

               // mnm + hexAddr (for rejitted resolution)
               mnm = tokarr[toknum++];

               if ( (gv.off == 1) && (gv.undup == 0) )
               {                        // gv.off never set to 2 now
                  sprintf(mnmbuf, "%s_%"_P64"x", mnm, addr);

                  mnm = mnmbuf;
               }

               if ( cflag & jrec_cflag_Code )   // HEX code in file
               {
                  int ii;
                  char * cpStart;

                  cpStart = cp = zMalloc("code space", size + 32);   // code space

                  for ( ii = 0; ii < size; ii += 32 )   // Up to 32 bytes (64 Hex digits) per line
                  {
                     if ( fgets( hrec, 128, fd ) )
                     {
                        cp = hex2bin(cp, hrec);
                     }
                     else
                     {
                        break;
                     }
                  }
                  if ( size != (int)(cp - cpStart) )
                  {
                     ErrVMsgLog(" Incomplete record from jita2n ignored: size = %d, expected size = %d\n",
                                (int)(cp - cpStart), size);

                     xFree( cpStart, size + 32 );

                     cpStart = NULL;    // Error reading binary data, ignore all of it

                     cflag &= ~jrec_cflag_Code;
                  }
                  cp = cpStart;         // restore cp to code ptr
               }
               else
               {
                  cp = NULL;
               }

               // time or id based
               bytime = ftime < ttime;
               byid   = seqNo < gv.trcid;

               if ( gv.nv || usenow || byid || ( (gv.trcid == 0) && bytime ) )
               {
                  use = 1;
                  ppt->jitUsed++;
                  if (jita2nmsg) {
                     WriteFlush(gc.msg, " A2nAddJitted (bytime=%d, byid=%d) %16llx %s\n",
                                        bytime, byid, ftime, mnm);
                  }

                  if (gd.show)
                  {
                     fprintf( gd.show, " A2nAddJitted %16llx %s\n",
                              ftime, mnm);
                  }
                  switch (cflag)        // Use jrec_cflag_????
                  {
                  case 0:               // JIT wo Code
                     A2nAddJittedMethod(pid, mnm, addr, size, NULL);
                     break;

                  case 1:               // JIT w Code
                     A2nAddJittedMethod(pid, mnm, addr, size, cp);
                     break;

                  case 2:               // MMI wo Code
                     A2nAddMMIMethod(pid, mnm, addr, size, NULL);
                     break;

                  case 3:               // MMI w Code
                     A2nAddMMIMethod(pid, mnm, addr, size, cp);
                     break;
                  }
               }

               else
               {                        // save
                  ppt->jitSaved++;
                  dcnt++;

                  pJREC = (JREC *)zMalloc("JREC", sizeof(JREC));

                  if (prevJREC == NULL)
                  {
                     ppt->jit_ll    = pJREC;   // Start the new linked list
                  }
                  else
                  {
                     prevJREC->next = pJREC;   // add at end to keep in time order
                  }

                  if (jita2nmsg) {
                     WriteFlush(gc.msg, " Saved (cnt=%d, seqno=%d) %16llx %s\n", dcnt, seqNo, ftime, mnm);
                  }

                  // fillin struct
                  pJREC->next  = NULL;
                  pJREC->time   = ftime;
                  pJREC->addr  = addr;
                  pJREC->size  = size;
                  pJREC->mnm   = xStrdup("mnm",mnm);
                  pJREC->cflag = cflag;
                  pJREC->code  = cp;
                  pJREC->seqNo = seqNo;
                  pJREC->already_sent = 0;

                  prevJREC = pJREC;     // keep last ptr to link next @ end
               }
            }
         }

         // default SeqNo = 0, only > 0 for IA32-IA64 machines

         else if ( tcnt == 2 )
         {                              // jitfile SeqNo record
            if (strcmp(tokarr[0], "SeqNo") == 0)
            {
               seqNo = atoi(tokarr[1]);
               OptVMsg( " SeqNo changed to %d\n", seqNo);
            }
         }

         // Look for marker. If we don't see it then we'll
         // skip reading jitted code. Most likely the file was
         // generated using and old JProf and the code is in binary.

         else if ( tcnt == 5 )
         {
            if ( (strcmp(tokarr[0], "BINARY"   ) == 0) &&
                 (strcmp(tokarr[1], "DATA"     ) == 0) &&
                 (strcmp(tokarr[2], "IN"       ) == 0) &&
                 (strcmp(tokarr[3], "PRINTABLE") == 0) &&
                 (strcmp(tokarr[4], "HEX"      ) == 0) )
            {
               saw_printable_hex = 1;
            }
         }
      }
      OptVMsgLog(" %4d log-jita2n records for PID %d (0x%x). Used %d, Saved %d.\n",
                 rcnt, pid, pid, ppt->jitUsed, ppt->jitSaved);
      OptVMsg("  mintm %16llx\n", mintime);
      OptVMsg(" lasttm %16llx\n", ftime);

      fflush(gc.msg);
      fclose(fd);
   }
}

/**********************************/
void panic(char * why)
{
   fprintf(stderr, "%s\n", why);
   fprintf(gc.msg, "%s\n", why);
   exit(-1);
}

/**********************************/
void cfstats()
{
   int      i;
   unsigned int j;
   double   rat, totb, tote, tot;
   int64    readbytes = getReadBytes();
   int64    fsize;

   if (cfbbs >= 1)
   {
      fprintf(gc.msg, "\n Call Flow Stats\n");
      fprintf(gc.msg, " %10d  BBs\n",           cfbbs);
      fprintf(gc.msg, " %10d  ExecutedBytes\n", cfbytes);
      fprintf(gc.msg, " %10d  Inst\n",          cfinsts);
      rat = cfbytes / (double)cfbbs;
      fprintf(gc.msg, " %10.1f  ExecutedBytes / BB\n",   rat);
      rat = cfinsts / (double)cfbbs;
      fprintf(gc.msg, " %10.1f  Insts / BB\n",           rat);
      rat = cfbytes / (double)cfinsts;
      fprintf(gc.msg, " %10.1f  ExecutedBytes / Inst\n", rat);

      fsize = nrm2_size();
      fprintf(gc.msg, "\n %016" _P64 "X  FileSize\n", fsize);
      fprintf(gc.msg, "\n %016" _P64 "X  BytesRead\n", readbytes);
      rat   = fsize / (double)cfinsts;
      fprintf(gc.msg, " %10.1f  FileBytes / Inst\n", rat);
      rat   = readbytes / (double)cfinsts;
      fprintf(gc.msg, " %10.1f  BytesRead / Inst\n", rat);

      fprintf(gc.msg, "\n BytesExecuted Histogram\n");
      fprintf(gc.msg, "\n       Freq   Bits      Range   %s", "Cum%");
      fprintf(gc.msg, "\n       ====   ====      =====   ====\n" );
      tote = totb = tot = 0;

      for (i = 0; i <= 32; i++)
      {
         tot += hbys[i];
      }
      for (i = 0; i <= 32; i++)
      {
         if (hbys[i] >= 1)
         {
            j = 1 << i;
            totb += hbys[i] * i;
            tote += hbys[i];
            rat = 100.0 * tote / tot;

            fprintf(gc.msg, " %10d %6d %10d %6.1f\n",
                    hbys[i], i, j, rat);
         }
      }
      fprintf(gc.msg, "       ====   ====      =====   ====\n" );
      fprintf(gc.msg, " %10.0f\n", tote);

      tote = totb = tot = 0;

      fprintf(gc.msg, "\n BytesToNextBB Histogram\n");
      fprintf(gc.msg, "\n       Freq   Bits      Range   %s", "Cum%");
      fprintf(gc.msg, "\n       ====   ====      =====   ====\n" );

      for (i = 0; i <= 32; i++)
      {
         tot += hto[i];
      }
      for (i = 0; i <= 32; i++)
      {
         if (hto[i] >= 1)
         {
            j = 1 << i;
            totb += hto[i] * i;
            tote += hto[i];
            rat = 100.0 * tote / tot;
            fprintf(gc.msg, " %10d %6d %10u %6.1f\n",
                    hto[i], i, j, rat);
         }
      }
      fprintf(gc.msg, "       ====   ====      =====   ====\n" );
      fprintf(gc.msg, " %10.0f\n", tote);
   }
}

/**********************************/
void thisto( uint64 fr, uint64 to)
{                                       //STJ64
   int i = 1;
   size_t del;

   // absolute
   if (to > fr) del = (size_t)( to - fr );   //STJ64
   else         del = (size_t)( fr - to );   //STJ64

   if (del >= 1)
   {
      while (del > 1)
      {
         del = del >> 1;
         i++;
      }
      hto[i]++;
   }
}

/**********************************/
void bhisto(int bytes)
{
   int i = 1;
   int tmp = bytes;

   if (tmp >= 1)
   {
      while (tmp > 1)
      {
         tmp = tmp >> 1;
         i++;
      }
      hbys[i]++;
   }
}

/**********************************/
int count_inst(uint64 prevTo, int bytes, SYMDATA * sdp)
{
   int  b       = 0;
   int  size    = 0;
   int  icnt    = 0;
   int  inval   = 0;
   int  istrlen = 0;

   char * cp;
   char * iptr;
   char   istr[10] = {""};
   char * opc = "-";
   char   buf[2048];

   uint64 addr;                         //STJ64

   if (bytes >= 0)
   {
      opc  = "????";
      strcpy(buf, "????");

      // getSym for TO address of last callflow hook

      if (sdp->code != NULL)
      {
         iptr = sdp->code;              // code at beginning of symbol
         iptr += (int)(prevTo - sdp->sym_addr);   // adjust code ptr to "prevTo"
         addr = prevTo;                 // virtual addr of this code

         while (b <= bytes)
         {
            icnt++;

            opc = (char *)i386_instruction((char *)iptr,
                                           (uint64)addr,
                                           &size,
                                           buf,
                                           sdp->flags & ( SDFLAG_MODULE_32BIT | SDFLAG_MODULE_64BIT ));   //STJ64

            inval = (int)(strcmp(opc, "INVALID") == 0);

            if ( inval )
            {
               OptVMsgLog("\n INVALID_INSTRUCTION\n");
               OptVMsgLog("   prevTo %016" _P64 "X  err@addr %016" _P64 "X ", prevTo, addr);   //STJ64
               OptVMsgLog(" %s:%s\n", sdp->sym_name, sdp->mod_name);

               cp = sdp->code;
               cp += (int)(prevTo - sdp->sym_addr);   // prevTo

               while (cp < iptr + 8)
               {
                  unsigned char c;

                  if (cp == iptr)
                  {
                     OptVMsgLog("-");
                  }

                  c = (*cp) & 0xff;
                  cp++;
                  OptVMsgLog("%02x", c);
               }
               OptVMsgLog("\n");

               if (gv.showss >= 1)
               {
                  fprintf(gd.arc,
                          " ss: %016" _P64 "X %2d %s\n",   //STJ64
                          addr, size, "INVALID");
               }
            }

            if (gv.showss >= 1)
            {
               if (gv.showss == 2)
               {                        // hex instrs ??
                  int i, ilen;
                  char * ip;
                  unsigned char c;

                  istrlen = 8;
                  cp = (char *)iptr;    // start at to
                  ip = &istr[0];

                  ilen = size;
                  if (ilen > 4) ilen = 4;

                  for (i = 0; i < ilen; i++)
                  {
                     c = (*cp) & 0xff;
                     cp++;
                     sprintf(ip, "%02x", c);
                     ip += 2;
                  }
                  istr[2 * ilen] = 0;
               }

               if ( gc.verbose )
               {
                  fprintf(gc.msg,
                          " ss: %016" _P64 "X %2d %*s %s\n",   //STJ64
                          addr, size, istrlen, istr, buf);
                  fflush(gc.msg);
               }

               fprintf(gd.arc,
                       " ss: %016" _P64 "X %2d %*s %s\n",   //STJ64
                       addr, size, istrlen, istr, buf);
            }

            b    += size;
            iptr += size;
            addr += size;

            if ( (size == 0) || inval )
            {
               break;
            }
         }
      }
   }

   if (icnt == 0)
   {
      if (gv.showss == 1)
      {
         fprintf(gd.arc,
                 " ss: %016" _P64 "X  :: 0 icnt\n",
                 prevTo);
      }
   }
   fflush(gd.arc);

   return(icnt);
}

/**********************************/
char gettype(int oc, int diff, int offset, int rc)
{
   char gtype = '@';

   if (diff != 0) diff = 1;

   if (oc == CALL)
   {
      if (diff == 1)
      {                                 // direct recur NoSym NoMod
         gtype  = '>';
      }
      // call & same => @
   }

   else if ( (oc == RETURN) || (oc == JUMP) )
   {
      if (offset == 0)
      {
         if (diff == 1)
         {
            gtype  = '>';               // runtime_resolve & others
         }
      }
   }

   return(gtype);
}

/**********************************/
void show(char * s)
{
   if (gd.show)
   {
      fprintf(gd.show, " --- %s\n", s);
      fflush(gd.show);
   }
}

/**********************************/
void handle_13(trace_t * tr)
{                                       // jstk hook
   int  pid, tid;
   // int  ty;
   int  stk, min, cpu;
   int del;
   char * fpnm, * pnm;

   static int fpid[32] = {0};           // by cpu. not reqd for
   static int ftid[32] = {0};
   static int fstk[32] = {0};

   // good-FROM => save by CPU
   // good-TO   => post.where

   // Both 13/1 p t s p t s // now both are Good
   // From 13/2 p t s
   // To   13/3 p t s

   min = tr->minor;
   cpu = tr->cpu_no;

   if ( (min == 1) || (min == 2) )
   {
      fpid[cpu] = tr->ival[0];
      ftid[cpu] = tr->ival[1];
      fstk[cpu] = tr->ival[2];
   }

   if ( (min == 1) || (min == 3) )
   {                                    // TO
      if (min == 1) del = 3;
      else         del = 0;

      pid  = tr->ival[0 + del];
      tid  = tr->ival[1 + del];
      stk  = tr->ival[2 + del];

      // diff thrd & TO => out
      // need pnm on F or T ??
      if ( (pid != fpid[cpu]) || (tid != ftid[cpu]) )
      {
         fpnm = A2nGetProcessName(fpid[cpu]);
         pnm  = A2nGetProcessName(pid);

         if ( 0 == gd.wi )
         {
            gd.wi = xopen("post.where", "w");
         }

         fprintf( gd.wi, " %4x %4x %x %s %4x %4x %x %s\n",
                  fpid[cpu], ftid[cpu], fstk[cpu], fpnm,
                  pid, tid, stk, pnm);

         fpid[cpu] = pid;
         ftid[cpu] = tid;
         fstk[cpu] = stk;
      }
   }
}

/**********************************/
//
// Here's an excerpt from a trace.
//
//   CPU MM mm  timestamp  ---------- data -----------
// 1) 1  10 20 41:3F4A7154 1058 1044 FFFFF880 05551FFD
// 2) 0  10 20 41:3F6EFCF8 1058 AF4 FFFFF800 02AD80ED
// 3) 1  10 23 41:3F6EFD58 1058 1044 7FF FEF4805F
// 4) 0  19 41 41:3F6F0E20 1 1058 3F6ED238 41 FF7B35C4 *JProf Jitted Method
// 5) 0  19 41 41:3F710800 1 1058 3F70F8E8 41 FF7B35A8 *JProf Jitted Method
//
// When we get to hook 3, we go thru catch-up logic in jitrec() to see if any
// of the saved jitted methods has a timestamp less than or equal to the
// timestamp in hook 3. Above we see that the jitted method described
// by hook 4 has a timestamp that is earlier than the tprof hook (3) we're
// standing on. As such, jitrec() pulls the corresponding jita2n record off
// the list, sends it to A2N, and marks it as already_sent.
// When we get to hook 4, handle_19_41() will find the record in the
// saved list, but will see that it has already been sent. In that case
// the record is deleted off the list and life goes merrily on.
//
// There is always one 19/41 hook for each record in the log-jita2n file
// while tracing was on.
//
void handle_19_41(trace_t * tr)
{                                       // jita2n method load
   int  trcid, pid;
   uint64_t  addr;
   int  rc;

   JREC      * pJREC    = NULL;
   JREC      * prevJREC = NULL;
   process_t * ppt;
   unsigned long long int jtime, ttime;

   int cnt;
   static int no_jit_record_cnt = 0;

   // debug. hooks written but pretend not
   // -oldjit option
   if ( (gv.oldjit == 1) || (gv.nv == 1) ) return;

   trcid = tr->ival[0];
   pid   = tr->ival[1];                 // pid
   ttime   = tr->time;                 // trace time
   addr  = tr->ival[3];                 // jit starting addr (low 32bits)

   ppt = insert_process(pid);

   if (ppt->read == 0)
   {
      WriteFlush(gc.msg, " Reading jita2n %x from 19/41 hook\n", pid);
      read_jita2n(ppt);
   }

   pJREC = (JREC *)ppt->jit_ll;

   //WriteFlush(gc.msg, "\n 19/41 p %p\n", p);
   //WriteFlush(gc.msg, " trcid %4d pid %4x t %8x %8x addr %8x\n",
   //   trcid, pid, thi, tlo, addr);

   // Need to locate record for HexCode & 64 bit address
   // Match on trcid & time. Remove Matched rec and rewire LL
   // Dont issue skipped records, but keep

   // breaks if hook not found.
   cnt = 0;

   if (jita2nmsg) {
      WriteFlush(gc.msg, " handle_19_41: time=0x%16llX\n", ttime);
   }

   while ( pJREC )
   {
      jtime = pJREC->time;

      if ( cnt == 2 )
      {
         WriteFlush( gc.msg, "Possible problem: Symbol not found in first two JITA2N records\n" );
         WriteFlush( gc.msg, "19/41 = seq:%d time:%llx addr:%llx\n", trcid, ttime, addr );
         WriteFlush( gc.msg, "JREC  = seq:%d time:%llx addr:%llx\n", pJREC->seqNo, jtime, pJREC->addr );
      }

      cnt++;

      if ( ( pJREC->seqNo == trcid) && ( ttime == jtime ) )
      {
         if (pJREC->already_sent) {
            // This record was already sent to A2N by the catch-up logic
            // in jitrec(). All we have to do is delete it off the list.
            if (jita2nmsg || (gc_verbose_logmsg & gc.verbose)) {
               WriteFlush(gc.msg, " ALREADY-SENT: A2nAddJitted %16llx %s cflag %d\n",
                                  jtime, pJREC->mnm, pJREC->cflag);
            }
         }
         else {
            // post.msg grows very rapidly if there are a lot of jitted methods.
            // print these messages only if -verbmsg option given.

            if (jita2nmsg || (gc_verbose_logmsg & gc.verbose)) {
               WriteFlush(gc.msg, " MATCHed: A2nAddJitted %16llx %s cflag %d\n",
                                  jtime, pJREC->mnm, pJREC->cflag);
            }

            if ( pJREC->cflag & jrec_cflag_MMI )   // MMI
            {
               rc = A2nAddMMIMethod(pid, pJREC->mnm, pJREC->addr, (int)pJREC->size, pJREC->code);
            }
            else                           // JIT
            {
               rc = A2nAddJittedMethod(pid, pJREC->mnm, pJREC->addr, (int)pJREC->size, pJREC->code);
            }

            if ( rc  )
            {
               WriteFlush(gc.msg, " ERROR: A2nAddJittedMethod() rc %d method %s\n", rc, pJREC->mnm);
               WriteFlush(gc.msg, "  tm %16llx jaddr %016"_P64"x sz %x\n",
                          ttime, pJREC->addr, pJREC->size);
            }
         }

         if ( prevJREC )
         {
            if ( pJREC->next )
            {
               prevJREC->next = pJREC->next;   // Remove JREC from middle of list
            }
            else
            {
               prevJREC->next = NULL;   // Remove last JREC from list
            }
         }
         else
         {
            if ( pJREC->next )
            {
               ppt->jit_ll = pJREC->next;   // Remove JREC from head of list
            }
            else
            {
               ppt->jit_ll = NULL;      // Remove only JREC from list
            }
         }

         if ( pJREC->code )
         {
            xFree( pJREC->code, 0 );
         }
         xFree( pJREC->mnm, 0 );
         xFree( pJREC, 0 );             // Free the JREC we just removed

         return;                        // Return when match found
      }

      prevJREC = pJREC;
      pJREC    = pJREC->next;
   }
   // No match was found

   no_jit_record_cnt++;                 // Don't print the "No JIT record" message forever unless -verbmsg

   if ( gc_verbose_logmsg & gc.verbose )
   {
      WriteFlush(gc.msg, " 19/41 Hook. No JIT record? ");
      WriteFlush(gc.msg, " trcid %4d pid %4d t %16llx addr %8x\n",
                 trcid, pid, ttime, addr);
   }
   else if ( no_jit_record_cnt <= 10 )
   {
      WriteFlush(gc.msg, " 19/41 Hook. No JIT record?  pid %d\n", pid);
   }

   if ( no_jit_record_cnt == 1 )
   {
      ErrVMsgLog(" *****\n");
      ErrVMsgLog(" ***** POST is having trouble resolving jitted method(s).\n");
      ErrVMsgLog(" ***** - It is looking for '%s-jita2n_%u'\n", jitpre, pid);
      ErrVMsgLog(" ***** - Does that look right?\n");
      ErrVMsgLog(" ***** - Are there any log-jita2n files?\n");
      ErrVMsgLog(" ***** - Is the -jdir option set to point to them?\n");
#ifdef _WINDOWS
      ErrVMsgLog(" ***** - Are environment variables IBMPERF_JPROF_LOG_FILES_PATH and\n");
      ErrVMsgLog(" *****   IBMPERF_JPROF_LOG_FILES_NAME_PREFIX (in toolsenv.cmd) set correctly?\n");
#endif
      ErrVMsgLog(" *****\n");
   }
}

/**********************************/
void jitrec(int pid, trace_t * tr, int all)
{
   // all = 1 => read all jit records, forget time
   // Before all getSymbol Calls

   JREC       * pJREC;
   JREC       * list;
   JTNM       * pJTNM;
   int          rc;
   int          db = 0;                 // jita2n extra data
   unsigned long long int jtime, ttime;
   process_t  * ppt;
   thread_t   * tp;
   int          cut = 0;                // catchup mode

   // by pid:  jit & jtnm catchup
   ppt = insert_process(pid);

   // jita2n & jtnm files read ?
   if (ppt->read == 0)
   {
      read_jita2n(ppt);
   }

   // catchup mode
   if (gv.trcid > ppt->trcid)
   {
      cut = 2;                          // trcid
      ppt->trcid = gv.trcid;
   }
   else if (gv.trcid == 0 || gv.trcid == ppt->trcid)
   {
      cut = 1;                          // time
   }
   else
   {
      return;                           // no catchup needed
   }

   ttime = tr->time;
   // JITA2N catchup
   list = ppt->jit_ll;
   while ( list )                       // List of delayed JITA2N records
   {
      pJREC = list;

      if (pJREC->already_sent) {
         // Sent this one already and handle_19_41() hasn't removed it from
         // the list yet. Keep moving but don't remove records off the list.
         list = pJREC->next;
         continue;
      }

      jtime   = pJREC->time;               // jit time

      // jittime < tracetime
      if ( ( (cut == 1) &&  (pJREC->time < tr->time ) ) ||
           ( (cut == 2) && (pJREC->seqNo < gv.trcid) ) )
      {
         if (jita2nmsg) {
            WriteFlush(gc.msg, " A2nAddJittedMethod(cut=%d) %16llx %s cflag %d\n",
                               cut, jtime, pJREC->mnm, pJREC->cflag);
         }

         if (gd.show)
         {
            fprintf(gd.show, " A2nAddJittedMethod() %16llx %s cflag %d\n",
                    jtime, pJREC->mnm, pJREC->cflag);
         }

         if ( pJREC->cflag & jrec_cflag_MMI )   // MMI
         {
            rc = A2nAddMMIMethod(pid, pJREC->mnm, pJREC->addr, (int)pJREC->size, pJREC->code);

            if ( rc )
            {
               WriteFlush(gc.msg, "A2nAddMMIMethod(): rc %d tm %16llx sz %x jaddr %16" _P64 "x\n",
                          rc, ttime, pJREC->size, pJREC->addr);
            }
         }
         else                           // JIT
         {
            rc = A2nAddJittedMethod(pid, pJREC->mnm, pJREC->addr, (int)pJREC->size, pJREC->code);

            if ( rc )
            {
               WriteFlush(gc.msg, "A2nAddJittedMethod(): rc %d tm %16llx sz %x jaddr %16" _P64 "x\n",
                          rc, ttime, pJREC->size, pJREC->addr);
            }
         }
         ppt->jitSaved++;

         if (db == 1)
         {
            fprintf(gc.msg, " pid %x jit2 %d A2nAddJittedMethod()\n",
                    pid, ppt->jitSaved);
            fprintf(gc.msg,
                    " hktm %16llx jtm %16llx jaddr %016" _P64 "X sz %x\n",
                    ttime, jtime, pJREC->addr, pJREC->size);
            fflush(gc.msg);
         }

         pJREC->already_sent = 1;         // Let handle_19_41() remove the records from the saved list
         list = pJREC->next;            // Keep moving but don't remove records off the list
      }
      else
      {                                 // catchup completed
         break;
      }
   }
   // End JITA2N CatchUp

   // Beginning JTNM Catchup

   while ( ppt->tnm_ll )
   {
      pJTNM = ppt->tnm_ll;

      jtime = pJTNM->time;

      if (db)
      {
         OptVMsg(" JTNM %16llx tid %4x pid %4x %s\n",
                 jtime, pJTNM->tid, pid, pJTNM->tnm);
      }

      if ( ( (cut == 1) && ( jtime < ttime ) ) ||
           ( (cut == 2) && (pJTNM->tnmid < gv.trcid) ) )
      {
         tp  = insert_thread( pJTNM->tid, pid);

         if ( *pJTNM->tnm )
         {
            tp->thread_name = pJTNM->tnm;   // Use name string allocated for JTNM record
         }
         else
         {
            tp->thread_name = "";
         }
         ppt->tnm_ll = pJTNM->next;     // Eliminate JTNM from head of list

         xFree(pJTNM,0);
      }
      else
      {                                 // catchup completed
         break;
      }
   }
   // End JTNM Catchup
}

/**********************************/
int oneoc( uint64 addr, SYMDATA * sdp ) //STJ64
{
   char   buf[2048];
   char * iptr;
   char * opc;
   int    size;
   int    oc = OTHER;

   if ( sdp && sdp->code )
   {
      iptr = sdp->code + (int)(addr - sdp->sym_addr);

#if 0 // defined(CONFIG_PPC64) || defined(_AIX)

      oc  = decode_inst_type( iptr );   // 1-5 call return jmp iret other

      if ( gc.verbose )
      {
         opc  = (char *)i386_instruction(iptr,
                                         (uint64)addr,
                                         &size,
                                         buf,
                                         sdp->flags & ( SDFLAG_MODULE_32BIT | SDFLAG_MODULE_64BIT ));   //STJ64

         fprintf( gc.msg, "ONEOC: %016" _P64 "X: %08X = %d, %s\n",
                  addr,
                  *( (unsigned long *)iptr ),
                  oc,
                  opc );
         fflush( gc.msg );
      }
#else
      opc = (char *)i386_instruction(iptr,
                                     (uint64)addr,
                                     &size,
                                     buf,
                                     sdp->flags & ( SDFLAG_MODULE_32BIT | SDFLAG_MODULE_64BIT ));   //STJ64

      oc  = inst_type(opc);             // 1-5 call return jmp iret other
#endif
   }
   return(oc);
}

/**********************************/
void hstats(int bytes, uint64 fr, uint64 to)
{                                       //STJ64
   bhisto(bytes);                       // bytes   histo
   thisto(fr, to);                      // from/to histo
   cfbytes += bytes;
   cfbbs++;
}

/**********************************/
//STJ64 - Begin
uint64 makeptr( uint64 lo, uint64 hi )
{
   uint64 p = ( hi << 32 ) + ( lo & 0x0FFFFFFFF );   //ull suffix needed???
   return(p);
}
//STJ64 - End

/**********************************/
/// Process one CallFlow hook
///
/// \verbatim
///
/// PERCS (MM only on 64 cases(20/21)
/// if first c always instructions
/// 20/21                    ffttc    64 // MM 1
/// 20/21                    ffttcc   64 // MM 2
/// 20/21                    ffttccc  64 // MM 3
///
/// 20/21  ft[c] 32 ft[c] 64 fftt[c] 64  // cf
/// 22/23  fft[c] 64 & fh = th           // cf
/// 24/25  fb 32 ffb 64 (b=BBCC)         // cf
/// 28/29  vcf vcff (ia64)               // vec
///
/// 2c     dispatch xxxccccc
/// \endverbatim
void call_flow(trace_t * tr)
{
   int  offset = -1;
   int  oc  = 0;                        // Opcode type of last instruction in block
   int  oc1 = 0;                        // Opcode type of first instruction in block
//                                      // ia64/lx to fix lack of calls in places??
   int  maj,   pid,  tid,  cpu, ring, rc;
   int  loc;
   int  dcnt,  tcnt,  ucnt, rcnt, vec;
   int  i64,   cx, tx;
   int  bytes;
   int  ints;
   char vbuf[16];

   uint64 fr     = 0;                   // FROM address of callflow hook
   uint64 to     = 0;                   //  TO  address of callflow hook
   uint64 prevTo = 0;                   //  TO  address of PREVIOUS callflow hook

   thread_t  * tp;

   SYMDATA sd    = {0};

   // debug vars
   int mx = 0;

   /*** code ***/

   pid  = tid  = ring = bytes = loc = 0;
   tcnt = dcnt = ucnt = rcnt  = vec  = 0;
   rc   = tx   = cx   = 0;

   cpu  = tr->cpu_no;
   ints = tr->ints;

   // also return if -int3 & no f9 yet
   // actually dont get here on above cases
   if (post_cpu_data[cpu].stt == 0) return;

   i64 = (int)(sizeof(char *) == 8);

   pid  = post_cpu_data[cpu].pid;
   tid  = post_cpu_data[cpu].tid;
   tp   = post_cpu_data[cpu].tp;        // NO INSERT_THREAD !!!!

   maj  = tr->major;
   if (maj == 0x20) mx = 1;             // add MM itrace

   ring = maj & 0x1;
   maj  = maj & 0xfe;                   // Separate ring from maj

   trstp = getTime(cpu);

   if ( (maj == 0x20) || (maj == 0x22) )
   {
      if (maj == 0x20)                  // 0x20/0x21
      {
         if (ints <= 2)
         {
            fr = makeptr( tr->minor,   0 );
            to = makeptr( tr->ival[0], 0 );

            if (ints == 2)
            {
               tcnt = tr->ival[1];
            }
         }
         else
         {                              // 64
            fr = makeptr( tr->minor,   tr->ival[0] );
            to = makeptr( tr->ival[1], tr->ival[2] );

            if (ints == 4)
            {
               tcnt = tr->ival[3];
            }
            else if (ints > 4)
            {
               tcnt = tr->ival[3 + gv.itsel];
            }
         }
      }
      if (maj == 0x22)                  // 0x22/0x23
      {
         fr = makeptr(tr->minor,   tr->ival[0]);
         to = makeptr(tr->ival[1], tr->ival[0]);

         tcnt = tr->ival[2];
      }

      if ((fr == 0) || (to == 0))
      {
         OptVMsg(" fr or to = 0. skip\n");
         return;
      }

      // callflow hook path
      jitrec(pid, tr, 0);               // jita2n catchup
      rc = getSymbol(pid, fr, &sd);     // stack sd

      if (rc <= 2)
      {                                 // sym nsf ns BADDOG-SYMBOL
         int bad = 0;

         prevTo = tp->to;               // Starting address is target from last hook

         if ( (fr >= prevTo) && (prevTo >= sd.sym_addr)
              && (fr < prevTo + 2048) )
         {

            bytes = (int)(fr - prevTo);
            hstats(bytes, fr, to);

            // below for
            // 1) !tcnt  & !ss
            // 2) !tcnt  &  ss
            // 3)  tcnt  & !ss
            // 4)  tcnt  &  ss

            // NB: Below are several special cases
            // => needs revisiting for each port & nuance
            // instCnt in/notin trace
            // platform anomalies (LX below)
            // disasm availability
            // MM PERCS (could provide CALL/RETURN directly)
            // left over debug ??
            // other
            // NB: END

            // tcnt == 0 => calc tcnt from disasm (dcnt)
            // tcnt != 0, no disasm => pick tcnt
            // tcnt != 0, tcnt != dcnt => pick tcnt unless gv.use_disasm_count set

            // tcnt == 0 or use_disasm_count => calc tcnt from disasm
            if ( (gv.showss >= 1) || (tcnt == 0) || gv.use_disasm_count )
            {
               if (sd.code)
               {
                  // decode 1st inst : IA64/LX/Java131- issue
                  // convert "alloc" to a previous CALL
                  oc1 = oneoc(prevTo, &sd);
               }
               // disasm the whole block of code
               dcnt = count_inst(prevTo, bytes, &sd);

               // trace count in trace AND trace count != disasm count
               if ( (tcnt > 0) && (tcnt != dcnt) )
               {
                  if ( gv.showss )
                  {
                     if ( (post_cpu_data[cpu].hk >= 0x20) && (post_cpu_data[cpu].hk <= 0x25) )
                     {
                        necnt1++;       // prev a call_flow
                        fprintf(gd.arc,
                                " # UNEX trace count != disasm count # tcnt %4d dcnt %4d\n",
                                tcnt, dcnt);
                     }
                     else
                     {
                        necnt2++;
                        fprintf(gd.arc,
                                " # EX   trace count != disasm count # tcnt %4d dcnt %4d\n",
                                tcnt, dcnt);
                     }
                  }

                  if (dcnt <= 1)
                  {                     // 0 or 1
                     // show rc fr prevTo sym_addr
                     if (gv.db == 4)
                     {
                        fprintf(gd.arc,
                                " ## code %p rc %d symaddr %016" _P64 "X fr %016" _P64 "X prevTo %016" _P64 "X\n",
                                sd.code,
                                rc,
                                sd.sym_addr,
                                fr,
                                prevTo);
                     }
                  }

                  if ( gv.use_disasm_count && dcnt != 0)
                  {                     // pick disasm cnt instead of trace cnt, if we can dissasemble it
                     tcnt = dcnt;
                  }
               }
               else
               {                        // no cnt in trace or same
                  tcnt = dcnt;
               }
            }
            else
            {                           // cnt in trace & no ss => disasm fr only
                                        // to extract CALL RETURN etc. to improve tree quality
               if (sd.code)
               {
                  // decode 1st inst : IA64/LX/Java131- issue
                  // convert "alloc" to a previous CALL
                  oc1 = oneoc(prevTo, &sd);
               }
            }

            gv.cfcnt++;
            cfinsts += tcnt;
         }
         else
         {                              // fr, prevTo break
            tcnt   = 1;
            bad    = 1;
            offset = (int)(fr - sd.sym_addr);

            if (gv.db == 4)
            {
               fprintf(gd.arc, " ### brk_%d: fr %016" _P64 "X prevTo %016" _P64 "X ofrom %016" _P64 "X\n",   //STJ64
                       cpu, fr, prevTo, tp->from);
            }
         }

         if (bad == 0)
         {
            if ( (rc == 0) && (prevTo != 0) )
            {                           // Sym
               // the really really good case
               offset = (int)(prevTo - sd.sym_addr);
            }
            else
            {
               // the not so good cases
               // offset=1 is flag for less than perfect
               offset = 1;              // noS & noSF
            }
         }
      }

      else
      {                                 // NoMod
         if (tcnt == 0)
         {
            tcnt = 1;                   // non-zero
         }
         offset = 1;
      }

      // add the LX alloc fixup here ????

      oc = oneoc(fr, &sd);              // decode last opcode

      arcbuf(tp, &sd, tcnt, offset, ring, oc, cpu, oc1);

      tp->from  = fr;                   // from
      tp->to    = to;                   // to
//      tp->ring  = ring;               // set in arcbuf
//      tp->sd    = sd;                 // set in arcbuf (symbol by thread)
      tp->sd_rc = rc;
   }

   if (maj == 0x28)
   {                                    // ia64 stuff
      int mnr;

      mnr  = tr->minor;
      vec  = 0xff & mnr;
      tcnt = tr->ival[0];

      // 28/29  ( enter : vcf vcff ) ( exit  : vc )
      if ((0x80000000 & mnr) == 0)
      {                                 // entry
         fr = makeptr(tr->ival[1], tr->ival[2]);   //STJ64
      }

      if (gv.arc)
      {
         sprintf(vbuf, "Int_0x%02X", vec);

         arcout(tp, tcnt, '?', 1, vbuf, "VECTOR",
                ring, 0, "-vec-", cpu);
      }
   }
}

/**********************************/
void shownrm2(int64 rec, trace_t * tr, FILE * fd)
{
   int        x;
   char       temp_str[MAX_STR_LEN];
   static int first = 0;
   //int mx;

   // enhance with pnm
   // 19.1 for pid => pnm
   // 12 (dispatch) => add pnm (from pid)

   if (first == 0)
   {
      first = 1;
      fprintf( fd, "        OFFSET CPU MAJOR MINOR TimeHigh:TimeLow    [INT]*  [STR]*\n" );
   }

   // integrated show (MM ??)
   //for(mx = 0; mx < tr->mets; mx++) {
   //   fprintf(fd, " %8x", tr->met_lo[mx]);
   //}

   if (gv.showx == 2)
   {
      // show raw binary data
      int i, len;

      len = tr->hook_len;
      if (len > 2048) len = 2048;
      for (i = 0; i < len; i += 4)
      {
         fprintf(fd, " %02x", tr->raw[i]);
         fprintf(fd, "%02x", tr->raw[i + 1]);
         fprintf(fd, "%02x", tr->raw[i + 2]);
         fprintf(fd, "%02x", tr->raw[i + 3]);
      }
      fprintf(fd, "\n");
   }

   fprintf(fd, " **** %016"_P64"x %3d %5x %5x %16llx",
           tr->file_offset,
           tr->cpu_no, tr->major, tr->minor, tr->time);

   for (x = 0; x < tr->ints; x++)
   {
      fprintf(fd, " %16llx", tr->ival[x]);
   }

   for (x = 0; x < tr->strings; x++)
   {
      memset(temp_str, 0, MAX_STR_LEN);
      strncpy(temp_str, (char *)&tr->sval[x][0], tr->slen[x]);
      fprintf(fd, " [%d]%s", tr->slen[x], temp_str);
   }

   // add pnm to dispatch(0x12)
   // Win Only Debug Hack for WhereIdle.
#ifdef _WINDOWS
   if (!gv.showx)
   {
      if (tr->major == 0x12)
      {
         char * pnm;

         pnm = A2nGetProcessName(tr->minor);
         fprintf(fd, " <%s>", pnm);
      }
   }
#endif

   fprintf(fd, "\n");
   fflush(fd);
}


/**********************************/
void handle_f9(trace_t * tr)
{
   // ints(x x cpu aacnt)
   // reduce between 1st & 2nd int3
   // also AA cnt per cpu is in int[3]

   if (aout)
   {
      if (gv.arc) fprintf(gd.arc, " ## int3\n");
   }
   int3c++;

   if ((aout == 0) && (int3c == 1))
   {
      // int3 On
      int3x = 1;
   }
   aout = int3f || (int3c == 1);

   gv.aatot += tr->ival[3];
}

/**********************************/
// common between NT LX & AIX
void proc_tprof(int pid, int tid, uint64 addr, int cpu)
{
   int rcq;
   SYMDATA sd = {0};
   thread_t * tp;

   char    * mnm;
   char    * mod;
   char    * pnm;
   static char modx[256];               // mod 32/64 decoration

   char     pstr[MAX_STR_LEN];
   char     tstr[MAX_STR_LEN];
   char     symstr[MAX_STR_LEN];
   char     cpustr[10];

   size_t   offset;
   pTNode   p;

   static int bdcnt = 0;                // baddog quality count

   char obuf[16];

   static int acnt       = 0;           // show sym_stt_addr for first 100
   int    acntlim        = 0;
   uint64 xaddr;                        // mod or sym addr
   uint64 zero64         = 0;           // 64 bit 0
   static int64 nosymcnt = 0;

   ///////////////////////
   /// proc_tprof CODE ///
   ///////////////////////

   jitrec(pid, tr, 0);                  // jita2n catchup

   // A2N return codes:
   //  (0) A2N_SUCCESS:           Everything worked. Have symbols.
   //  (1) A2N_NO_SYMBOL_FOUND:   Have symbols but no symbol found for addr
   //  (2) A2N_NO_SYMBOLS:        Module has no symbols
   //  (3) A2N_NO_MODULE:         No loaded module found
   rcq = getSymbol(pid, addr, &sd);

   mod  = (char *)sd.mod_name;
   mnm  = (char *)sd.sym_name;

   if (rcq == A2N_SUCCESS)
   {
      // Have symbols
      xaddr = sd.sym_addr;
   }
   else if (rcq == A2N_NO_SYMBOL_FOUND || rcq == A2N_NO_SYMBOLS)
   {
      // Can't find symbol for addr or there are no symbols.
      // A2N returns either "NoSymbols", "NoSymbolFound" or a section name.
      if (mnm[0] == '<' && mnm[1] == '<')
      {
         // Section name. Consider that a valid symbol
         xaddr = sd.sym_addr;
      }
      else
      {
         // "NoSymbols" or "NoSymbolFound". Not much we can do
         xaddr = sd.mod_addr;
      }
      nosymcnt++;

      if (gc_verbose_logmsg & gc.verbose)
      {
         WriteFlush(gc.msg, " *NOSYM* cnt=%"_L64"d rcq=%d symaddr 0x%8"_P64"X xaddr 0x%8"_P64"X mod %s sym %s\n",
                    nosymcnt, rcq, addr, xaddr, mod, mnm);
      }
   }
   else
   {
      // No module or something else
      xaddr = sd.mod_addr;
   }
   offset = (size_t)(addr - xaddr);

   // for Kean (raw data for debug)
   if (gv.db == 6)
   {
      static int once = 0;

      if (once == 0)
      {
         once = 1;
         OptVMsg(" ADDR: %3s %6s %6s %16s %16s %8s %s %s\n",
                 "rcq", "pid", "tid", "addr", "xaddr", "offset", "mod", "mnm");
      }
      OptVMsg(" ADDR %16llx %3d %6x %6x %016" _P64 "X %016" _P64 "X %8x %s %s\n",
              tr->time, rcq, pid, tid, addr, xaddr, offset, mod, mnm);
   }

   // error report for low quality A2N
   if (rcq >= 1)
   {
      bdcnt++;
      if (bdcnt <= 5)
      {
         ErrVMsgLog(" Low Quality Symbol %d: a2n_rc=%d  addr=0x%"_PZP"  module=%s\n",
                    bdcnt, rcq, Uint64ToPtr(addr), mod);
      }
      if (bdcnt == 5)
      {
         ErrVMsgLog("\n *WARNING* At least 5 Low Quality Symbols (a2n_rc >= 1).\n");
         WriteFlush(gc.msg, " - a2n_rc = 1: Symbol Not Found\n");
         WriteFlush(gc.msg, " - a2n_rc = 2: No Symbols for Module\n");
         WriteFlush(gc.msg, " - a2n_rc = 3: No Module\n");
      }
   }

   // === BEGIN PNM ===
   if (aix == 0)
   {                                    // Normal A2N
      pnm = (char *)sd.owning_pid_name;
   }
   else
   {                                    // AIX : pnm on hook
      pnm = gv_tppnm;
   }

   if (pnm != NULL)
   {
      if (gv.pnmonly == 0)
      {
         sprintf(pstr, "%s_%04x", pnm, pid);
      }
      else
      {
         sprintf(pstr, "%s", pnm);
      }
   }
   else
   {
      sprintf(pstr, "pid_%04x", pid);
   }
   // === END PNM ===

   if (gv.cpu == 1)
   {
      sprintf(cpustr, "_%d", cpu);
      strcat(pstr, cpustr);
   }

   // === BEGIN TNM ===
   // thread name creation
   tp = insert_thread(tid, pid);

   // 3 formats
   // tid_%04x      - noName
   // tid_tnm_%04x  - Name
   // tid_tnm       _ Name & tnmonly option
   if ( !tp->thread_name || !*tp->thread_name )
	   tp->thread_name = A2nGetProcessName(tid);

   if ( tp->thread_name && *tp->thread_name )              // Thread name is not ""
   {
      if (gv.tnmonly == 0)
      {
         sprintf(tstr, "tid_%s_%04x", tp->thread_name, tid);
      }
      else
      {
         sprintf(tstr, "tid_%s", tp->thread_name);
      }
   }
   else
   {
      sprintf(tstr, "tid_%04x", tid);
   }
   // === END TNM ===

   // tag 64 bit modules with _64
   if (gv.tmod)
   {
      if ( sd.flags & SDFLAG_MODULE_64BIT )
      {
         sprintf(modx, "%s_64", mod);
         mod = modx;
      }
   }

   // === BEGIN Sym Addr ===
   // append addr to symbols
   if (gv.undup)
   {
      if (rcq == 0)
      {
         int addr = (int)(xaddr & 0xffffffff);

         sprintf(symstr, "%s_%x", mnm, addr);
         mnm = &symstr[0];
      }
   }
   // === END Sym Addr ===


   // push tick into tree (4 or 5 levels)
   // - PID
   // -  TID
   // -   Module
   // -    Symbol
   // -     Offset
   p = pusha(root, pstr,        zero64);   // pid node
   p = pusha(p,    tstr,        zero64);   // tid node
   p = pusha(p,    (char *)mod, zero64);   // mod node
   p = pusha(p,    (char *)mnm, xaddr); // sym node

   // debug peek at sttaddr
   if (acnt < acntlim)
   {
      if (p->rc == 0)
      {
         acnt++;
         OptVMsg(" %3d %16" _P64 "x\n p->nm <%s>\n",
                 acnt, p->sttaddr, p->nm);
      }
   }

   // decorating the SYMBOL Node Only
   p->rc      = rcq;                    // symbols Quality
   p->clen    = sd.sym_length;
   p->cptr    = sd.code;                // code ptr ( or NULL )
   p->module  = sd.module;              // late a2n resolution
   p->symbol  = sd.symbol;              // late a2n resolution
   p->sttaddr = sd.sym_addr;            // ??
   p->flags   = sd.flags;               // Save the symbol flags //STJ64

   // offset for microprofiling
   if (gv.off)
   {
      sprintf(obuf, "0x%x", (unsigned int)offset);

      if (rcq == 0)
      {                                 // good symbol found
         p->cptr = sd.code;             // code ptr ( or NULL )
      }
      else p->cptr = NULL;

      p->snm = "-";                     // default bef linenos used

      // if LINENOS & (WIN32 or LINUX)
#ifndef NOLINENOS
   #if defined(WIN32) || defined(WINDOWS) || defined(_LINUX)
      p->snm = sd.src_file;
   #endif
#endif

      // Pushing the OFFSET node
      p = pusha(p, obuf, zero64);

      p->rc = -2;                       // default bef linenos used

#ifndef NOLINENOS
   #if defined(WIN32) || defined(WINDOWS) || defined(_LINUX)
      p->rc  = sd.src_lineno;
   #else
      p->rc = -3;                       // if linenos but not on this platform
   #endif
#endif
   }
   p->base++;                           // tick on lowest level (symbol or offset)

   if ( (gv.db == 4) && gd.arc)
   {
      fprintf(gd.arc, " tprof: addr %016" _P64 "X pid %x < %s %s %s %s >\n",   //STJ64
              addr, pid, mnm, mod, tstr, pstr);
      fflush(gd.arc);
   }
}

/**********************************/
void handle_tprof_hook(trace_t * tr)
{
   // NT & LX : nrm2 format

   uint64    addr = 0;                  //STJ64
   //size_t    addrhi;
   uint64    addrhi;
   int       pid = 0;
   int       tid = 0;

   int mnr, cpu, ring, flg;
   int nx = 0;
   int maj;
   int sub;

   static int64 tpcnt = 0;              // filter tprof range
   static int   first = 0;              // first tprof hook time

   // 0x10 : tprof for LX & NT
   //   mnr 0x10 | 0x11 | 0x13    tid f[f]      // lx
   //       0x20 | 0x21 | 0x23    pid tid f[f]  // nt
   //   mm tprof => use 2nd byte of minor (i.e 0423 => 4-th metric
   //       0xbbxx => bb for selection of which metric to use
   //                 bb = 0 => use all tprof together
   //                 bb != 0 => use selected via tprofmm n

   // tprof windowing
   tpcnt++;
   if (gv.tstt > tpcnt)      return;
   else if (gv.tstp < tpcnt) return;

   cpu = tr->cpu_no;

   // get 1st & last tprof time
   if (first == 0)
   {
      first = 1;
      trstt = getTime(cpu);             // 1st tprof hook time
   }
   long double time = getTime(cpu);
   if (time > trstp)
	   trstp = time;           // last tprof hook

   maj = tr->major;
   mnr = tr->minor;
   sub = (0xff00 & mnr) >> 8;           // which tprof metric
   mnr = 0xff & mnr;

   if (gv.tpsel == sub)
   {                                    // normally both 0
      // using this tprof hook

      if ( (mnr & 0x30) != 0)
      {
         flg  = mnr & 0x30;             // NT 0x20 vs LX 0x10
         ring = mnr & 0x1;              // 0 1 3

         if (flg == 0x20)
         {                              // nt
            pid = tr->ival[0];
            tid = tr->ival[1];
            nx = 2;
         }
         else if (flg == 0x10)
         {                              // lx
            tid = tr->ival[0];
            pid = A2nGetOwningPid(tid);
            if (pid == -1)
            {
               fprintf(stderr, " A2nGetOwningPid Err. tid %x\n", tid);
               //return;
               // continue with pid = -1
            }
            nx = 1;
         }

         addr = (uint64)tr->ival[nx];   //STJ64
         nx++;
      }
      else
      {
         fprintf(stderr, " Invalid TPROF minor : 0x%x\n", mnr);
         exit(-1);
      }

      cpu = tr->cpu_no;

      proc_tprof(pid, tid, addr, cpu);
   }
   else
   {
      // OptVMsg(" not reducing this guy sub = %d\n", sub);
   }
}

/**********************************/
void handle_19_34_hook(trace_t * tr)
{
   char pnmbuf[MAX_STR_LEN];
   int  pid;

   // ints(pid) [32/64] strs(pnm)
   // ints = 1 strs = 1
   pid = tr->ival[0];

   // AMD & LX 32(x86)/64(x64) problem.
   // running 32b emulation on a x64
   //if(ints == 2) {
   // 0x32 => 1 => pnm_pid(x86)
   // 0x64 => 0
   // decorate pnm for tprof
   //gv.p32 = tr->ival[1]; // 0x32 0x64
   //}

   memcpy(pnmbuf, &tr->sval[0], tr->slen[0]);   // mnm
   pnmbuf[tr->slen[0]] = 0;

   A2nSetProcessName(pid, pnmbuf);      // nop for WIN64
}

/**********************************/
void handle_newpid(trace_t * tr)
{                                       // 19/64
   int rc = 0;

   rc = A2nCreateProcess(tr->ival[0]);
   // err cases ??
}

/**********************************/
void handle_19_42(trace_t * tr)
{                                       // Thread Start / Stop
   int        pid;
   int        tid;
   thread_t * tp;

   if (gv.oldjit == 1) return;

   // All needed data in hook
   // => dont need to Match w saved records.

// trcid = tr->ival[0];
   pid   = tr->ival[1];                 // pid when hook written
// tlo   = tr->ival[2];                 // trace time low
// thi   = tr->ival[3];                 // trace time high
   tid   = tr->ival[4];                 // java thread ID
// ss    = tr->ival[5];                 // 0:Start, 1:Stop

   tp  = insert_thread(tid, pid);

   if ( tr->ival[5] )                   // 0:Start, 1:Stop
   {
      tp->thread_name = "";             // Stop
   }
   else
   {
      tp->thread_name = xMalloc( "handle_19_42", 1 + tr->slen[0] );   // Start

      memcpy( tp->thread_name, &tr->sval[0], tr->slen[0]);   // thread_name
      tp->thread_name[ tr->slen[0] ] = 0;   // NULL terminate
   }

   //OptVMsg(" ADDR %08x %08x 19/42 JTNM <%s>\n", thi, tlo, jtnm);
}

/**********************************/
// pid parpid ownpid
// action  pid ppid   opid
// ====== ==== ==== ======
//   fork    A    B      A
//   exec    A    B own(B)
void handle_pidmap(trace_t * tr)
{                                       // 19/63
   int  pid, ppid, clone;
   int  rc;
   char fpid[16];
   char tpid[16];
   static int hcnt = 0;

   pid   = tr->ival[0];                 // new  pid
   ppid  = tr->ival[1];                 // prev pid
   clone = tr->ival[2];                 // 0-fork, 0x100-clone

   sprintf(fpid, "%04x", ppid);
   sprintf(tpid, "%04x", pid);

   hcnt++;

   if (clone == 0)
   {                                    // fork
      rc = A2nForkProcess(ppid, pid);   // pid owns itself

      if (rc != 0)
      {
         fprintf(stderr, " a2nERR: A2nForkProcess(%x %x) rc %d\n",
                 ppid, pid, rc);
      }
   }
   else
   {                                    // clone  0x100
      rc = A2nCloneProcess(ppid, pid);

      if (rc != 0)
      {
         fprintf(stderr, " a2nERR: A2nCloneProcess(%x %x) rc %d\n",
                 ppid, pid, rc);
      }
   }

   return;
}

/*****************************/
void handle_startup_hook(trace_t * tr)
{
   int speed;

   switch (tr->minor)
   {
   case SYSINFO_INTERVAL_ID:                           // trace Sequence No.
      if (gv.oldjit == 0)
      {
         gv.trcid = tr->ival[0];        // traceID for NUMA
      }
      OptVMsg(" 11/b2 trcid %d\n", gv.trcid);
      break;

   case SYSINFO_END_MINOR:                           // end of start up
      break;

   case SYSINFO_CPUSPEED_MINOR:
      speed = tr->ival[0];              // MHz
      gv.clock_rate = (double)(speed * 1000000.0);
      fprintf(gc.msg, " ClockRate %8.0f\n", gv.clock_rate);
      break;

   case SYSINFO_TIME_BASE_MINOR:
      gv.timebase = tr->ival[0];
      fprintf(gc.msg, " Timebase %d 0x%x\n", gv.timebase, gv.timebase);
      break;

   case SYSINFO_TPROF_MODE:
      gv.tprof_mode  = tr->ival[0];
      gv.tprof_count = tr->ival[1];
#if defined(_WINDOWS)
      if (gv.tprof_mode)  gv.tprof_count = -gv.tprof_count;
#endif
      fprintf(gc.msg, " Tprof mode %d, count = %d\n", gv.tprof_mode, gv.tprof_count);
      break;

   case SYSINFO_TPROF_EVENT_INFO:
      gv.tprof_event = tr->ival[1];
      fprintf(gc.msg, " Tprof event %d\n", gv.tprof_event);
      break;


   case 0xbc:                           // NRM2 Architectural Meeting REQUIRED
      // ind = int[0] - 1
      // metric[int[ind] = int[1]
      // mcnt = int[0]
      //gv.timebase = tr->ival[0];

      //fprintf(gc.msg, " \n");

      break;
   }
}

/**********************************/
void major_histo(void)
{
   int i;

   ErrVMsgLog("\n    Major     Freq  Function\n    =====     ====  ========\n");

   for (i = 0; i < 64 * 1024; i++)
   {
      if (hmaj[i] >= 1)
      {
         ErrVMsgLog(" %8x %8d  %s\n", i, hmaj[i], hmajlab[i]);
      }
   }
   ErrVMsgLog("\n");

   fprintf(gc.msg, "\n %8d  CallFlow Hooks from int3\n", gv.aatot );

   if (ssring[3] >= 1)
   {
      fprintf(gc.msg, "\n  CallFlowInstructions by Ring\n\n     Ring     Freq\n     ====     ====\n");

      for (i = 0; i < 4; i++)
      {
         if (ssring[i] >= 1)
         {
            fprintf(gc.msg, " %8d %8d\n", i, ssring[i]);
         }
      }
   }
}

/**********************************/
void readnrm2(int64 nstt, int64 nstp)
{
   int64  rec2        = 0;
   int64  rec         = 0;

   OptVMsg(" A2N Version%x\n", A2nGetVersion());

   if (gv.oprof == 1)                   // i.e., Linux
   {
      // pass:1 - mte hooks only, no need if showx

      if (gv.showx == 0)
      {
         init_list_ptr_mte();           // init list of pointers to file sections

         tr = get_next_rec_mte();
         while (tr != NULL)
         {
            switch (tr->major)
            {
            case MTE_MAJOR:             // mte & jitted methods
               switch (tr->minor)
               {
               case MTE_MINOR:          // mte mod
                  handle_19_1_hook(tr);
                  break;

               case MTE_PIDNAME_MINOR:     // pnm from fork/exec
                  handle_19_34_hook(tr);   // pid pnm
                  break;

               case MTE_CLONE_FORK_MINOR:
                  handle_pidmap(tr);    // pid ppid cflg
                  break;

               case MTE_CREATE_PID_MINOR:
                  handle_newpid(tr);
                  break;
               }
               break;

            case SYS_INFO_MAJOR:        // end of init
               handle_startup_hook(tr);
               break;

            default:
               break;
            }                           // switch

            tr = get_next_rec_mte();
         }                              // while

         // reset trace to beginning (for 2nd pass)
         reset_raw_file();
      }                                 // showx
      // If normal tracing mode, we need to reset values
      // only for the final MTE section
      if (COLLECTION_MODE_NORMAL == gv.trace_type)
      {
         init_last_mte();
      }
      else
      {
         init_list_ptr_mte();
      }
   }                                    // oprof

   // 1st pass OR oprof 2nd pass
   init_list_ptr_trace();
   tr = get_next_rec();

   while ( (rec <= nstp) && (tr != NULL))
   {
      int  miss = 0;

      hmaj[tr->major]++;
      currtt = getTraceTime();

      if (rec >= nstt)
      {
         if (gd.show)
         {
            shownrm2(rec, tr, gd.show);
         }
         if ( (gv.db == 4) && gd.arc )
         {
            shownrm2(rec, tr, gd.arc);
         }

         if (gv.showx == 0)
         {
            switch (tr->major)
            {
            case TASK_SWITCH_MAJOR:
               handle_13(tr);           // pid-tid-jstk
               break;

               // MM 64 cf : fftt[c]+ (4+)
            case BRANCH_MAJOR:                  // cf: ft[c] (1|2) fftt[c] (3|4)
            case BRANCH_PRIV_MAJOR:
            case 0x22:                  // cf: fft[c] : ia64 & fh = th
            case 0x23:
            case 0x24:                  // cf: fb ffb (b=BBCC)
            case 0x25:
            case 0x28:                  // Interrupt / Exception : vc  vcl
            case 0x29:
               if (gd.arc)
               {
                  call_flow(tr);
               }
               break;
               // added for MM ITrace
            case 0x1e:                  // MMdisp LX  : tid [c]+
            case 0x1f:                  // MMdisp Win : pid tid [c]+

            case PROC_SWITCH_MAJOR:                  // disp LX : tid c
               //      //LX : tid [c]+
               //      Win : pid tid c
            case 0x2d:                  //      //Win : pid tid c ts*
               if (gd.arc)
               {
                  cf_pidtid(tr);
               }
               break;

            case TRACE_ON_MAJOR:                  // itracestt cpid ctid sttpid stttid mode
               // equivalent to 0x2c & start now
               trstt = getTime(tr->cpu_no);
               break;

            case TRACE_OFF_MAJOR:                  // itracestp cpu cfhkcnt (per processor)
               trstp = getTime(tr->cpu_no);
               break;

               // old, retain
            case INTERRUPT_MAJOR:                  // interrupt entry/exit
               handle_interrupt(tr);
               break;

            case TPROF_MAJOR:
               // -arc option NOT used
               if (gv.mtrace == 0)
               {
                  handle_tprof_hook(tr);
               }
               break;

            case SYS_INFO_MAJOR:                  // end of init
               handle_startup_hook(tr);
               break;

            case DISPATCH_MAJOR:                  // dispatch . tprof only
               handle_dispatch(tr);
               break;

            case MTE_MAJOR:                  // mte & jitted methods
               if (gv.oprof == 0)
               {
                  switch (tr->minor)
                  {
                  case MTE_MINOR:            // mte mod
                     handle_19_1_hook(tr);
                     break;

                  case MTE_PIDNAME_MINOR:            // pnm from fork/exec
                     handle_19_34_hook(tr);   // pid pnm
                     break;

                  case MTE_CLONE_FORK_MINOR:
                     handle_pidmap(tr); // pid ppid cflg
                     break;

                  case MTE_CREATE_PID_MINOR:
                     handle_newpid(tr);
                     break;

                  }
               }
               switch (tr->minor)
               {
               case MTE_JITSYM_MINOR:
                  handle_19_41(tr);     // jitload
                  break;

               case MTE_JTHR_NAME_MINOR:
                  handle_19_42(tr);     // jtnm
                  break;
               }
               break;

               // DEPRECATED
            case 0xf9:                  // int3 ( logical start stop )
               //handle_f9(tr);
               break;


            case TSCHANGE_MAJOR:                // low timer overflow
            case SPECIAL_MAJOR:                  // low timer overflow
               // handled in bputil
               break;

            case METRICS_MAJOR:                   // MM hook [08]*[09][xx]
               // should be handled in bputil to update hi metric
               break;

            default:
               miss = 1;
               break;
            }

            if (miss & aout)
            {
               // EnEx
               if (gv.arc)
               {
                  //fprintf(gd.arc, " %6d ? hook_%x_%x\n",
                  //      0, tr->major, tr->minor);
                  // ptflush & arcout() ??
               }
            }
         }
      }

      post_cpu_data[tr->cpu_no].hk = tr->major;

      tr = get_next_rec();

      if (rec2 == 10000)
      {
         fprintf(stdout, " Records  %10" _P64 "d\n", rec);
         rec2 = 0;
      }
      rec2++;
      rec++;
   }

   // fflush active ITrace threads
   if (gd.arc) allpt_flush();

   if (cfinsts >= 1)
   {
      fprintf(gc.msg, " Total CallFlow Insts %d\n", cfinsts);
   }
   fprintf(gc.msg, " Total Records %10" _P64 "d\n", rec);

   major_histo();
   cfstats();
   if (gd.arc)
   {
      itrace_footer(gc.msg);
      itrace_footer(gd.arc);
   }
   close_raw_file();
}

/**********************************/
void initGV(void)
{
   int i;

   struct _maj
   {
      int    ind;
      char * lab;
   };

   // labels for major histogram
   static struct _maj xmaj[64] =
   {
      { 0x03, "EXCEPTION"},
      { 0x04, "INTERRUPT"},
      { 0x05, "SYSCALL"},
      { 0x06, "Vec-WinIA64"},
      { 0x07, "Inter-Ent-WinIA64"},
      { 0x08, "TIMER_WRAP"},
      { 0x10, "TPROF"},
      { 0x11, "STARTUP"},
      { 0x12, "DISPATCH"},
      { 0x19, "MTE JMNM"},
      { 0x1e, "MMIT-Disp-LX"},
      { 0x1f, "MMIT-Disp-Win"},

      { 0x20, "CALL_FLOW"},
      { 0x21, "CALL_FLOW"},
      { 0x22, "CALL_FLOW"},
      { 0x23, "CALL_FLOW"},
      { 0x24, "CALL_FLOW"},
      { 0x25, "CALL_FLOW"},
      { 0x28, "Inter/Exc"},
      { 0x29, "Inter/Exc"},
      { 0x2c, "CF_PIDTID"},
      { 0x2d, "CF_PIDTID"},
      { 0x2e, "Itrace_stt"},
      { 0x2f, "Itrace_stp"},
      { 0x30, "MEMORY"},

      { 0xa8, "TIMER_WRAP"},
      { 0xaa, "CALL_FLOW"},
      { 0xab, "CALL_FLOW"},
      { 0xef, "SYSCALLS"},
      { 0xf0, "EXCEPTION"},
      { 0xf1, "DISPATCH"},
      { 0xf2, "FORK"},
      { 0xf3, "TASKLET"},
      { 0xf5, "BOTHALF"},
      { 0xf7, "INTERRUPT"},
      { 0xf9, "INT3"},
      { 0xfb, "SINGLESTEP"},
      { 0x00, ""},                      // sentinel (stops linear search)
   };

   // init 64k structs here (avoid exe space)
   for (i = 0; i < 64 * 1024; i++)
   {
      hmaj[i] = 0;
      hmajlab[i] = NULL;
   }

   // dont stop on disasm error
   noStopOnInvalid();

   // linux 64 bit ( initializer on GV didnt work ??? )
   memset(&gv, 0, sizeof(GV));

   gv.sympath  = NULL;
// gv.ptree    = 1;                     // default ITrace output is ptree
   gv.tottks   = 0;                     // redundant
   gv.tstt     = 0;                     // redundant
   gv.tstp = 10000000;

   for (i = 0; i < 64 * 1024; i++)
   {
      hmaj[i] = 0;
      hmajlab[i] = "";
   }

   for (i = 0; i < 64; i++)
   {
      int j;

      j = xmaj[i].ind;
      if (j == 0) break;
      hmajlab[j] = xmaj[i].lab;
   }
}

/**********************************/
void itrace_footer(FILE * fd)
{
   double tcycs, tt;

   tcycs = trstp - trstt;

   if (gv.timebase > 0)
   {
      tt = tcycs / gv.timebase;
   }
   else
   {
      tt = tcycs / gv.clock_rate;
   }

   fprintf(fd, "\n");
   fprintf(fd, "# %20s %15d\n",        "Processors",     post_num_cpus);
   fprintf(fd, "# %20s %15.0f\n",      "ProcessorSpeed", gv.clock_rate);
   fprintf(fd, "# %20s %15.0f\n",      "TraceCycles",    tcycs);
   fprintf(fd, "# %20s %15.3f(sec)\n", "TraceTime", tt);
}

/**********************************/
void setrpt(char * p)
{
   int k, i = 0;

   //fprintf(gc.msg, " XX Report: %s\n", p);

   // parse a report definition (ie 134 into global vars)
   while (*p != '\0')
   {
      i++;
      k = (int)(*p) - (int)'0';

      //fprintf(gc.msg, " YY k %d tlab[k]  %s\n", k, tlab[k]);
      //fprintf(gc.msg, " ZZ k %d tlabx[k] %s\n", k, tlabx[k]);

      gv.xind[i] = k;                   // k-lev in orig => i-th lev in new
      gv.flab[i] = tlab[k];             // assoc level label (i.e. 3 => MOD)
      gv.rlab[i] = tlabx[k];            // assoc title label (i.e _MODULE)
      gv.xlevs = i;                     // total levels in new tree
      //fprintf(gc.msg,
      //      " XX i=%d xind %2d flab %8s rlab %8s\n",
      //      i, k, tlab[k], tlabx[k]);
      p++;
   }
}

/**********************************/
void html_link(int i)
{
   if (0 == 1)
   {
      fprintf(gd.tp, " <A HREF=\"#R_%d\">))</A>\n", i);
   }
}

/**********************************/
void html_tag(int i)
{
   if (0 == 1)
   {
      fprintf(gd.tp, "<A NAME=\"R_%d\"></A>\n", i);
   }
}

/**********************************/
void rtitle(int link)
{
   int i;

   if ( (0 == 1) && (link >= 0))
   {                                    // html link
      fprintf(gd.tp, "   <A HREF=\"#R_%d\">)) %s",
              link, gv.rlab[1]);
   }
   else
   {
      fprintf(gd.tp, "    )) %s", gv.rlab[1]);
   }

   //fprintf(gd.tp, "    )) %s", gv.rlab[1]);

   for (i = 2; i <= gv.xlevs; i++)
   {
      fprintf(gd.tp, "_%s", gv.rlab[i]);
   }

   if ((0 == 1) && link >= 0)
   {                                    // html link
      fprintf(gd.tp, "</A>\n");
   }
   else
   {
      fprintf(gd.tp, "\n");
   }
}

/**********************************/
void xfixuppid(pTNode root)
{
   // scan root children(pids)
   // fixup pnm if possible

   // walk pid only and print
}

/**********************************/
// root  - global tick database
// p     - the report type 4321 => SMTP
// tklev - ticks lev in database (4 or 5(w off))
void xreport(pTNode root, char * p, int tklev)
{
   pTNode xroot;

   setrpt(p);                           // set global vars (select data @ level)

   fprintf(gd.tp, "\n   %s\n", ban);    // report banner
   rtitle(-1);
   fprintf(gd.tp, "   %s\n\n", ban);

   xroot = initTree();                  // new root
   tree_new(root, 0, xroot, tklev);     // tree orig(root) => new(xroot);
   tree_cum(xroot, 0);                  // cum
   tree_sort_out_free(xroot, 0, gd.tp); // sort.out.free

   fflush(gd.tp);
}

/**********************************/
void xinput(pTNode root)
{
   char buf[128];
   int rc = 1;
   char * cp;

   while (rc == 1)
   {
      if (fgets(buf, 128, gd.xfd) != NULL)
      {
         gv.xlevs = 0;
         cp = buf;
         while (*cp == ' ') cp++;       // skip leading blanks
         while ( (*cp != ' ') && (*cp != '\0')  && (*cp != '\n') )
         {
            cp++;
         }
         *cp = '\0';
         xreport(root, buf, 4 + gv.off);   // 4: sym 5: offset
      }
      else rc = 0;
   }
}

/**********************************/
void output(void)
{
   int    nreps = 8;
   int    i;
   int    tmp;
   double tcycs, tt, pc;
   int    tlim;
#ifdef _S390
   double k390cycs2sec = 4096.0 * 1000000.0;
#endif
   //int timebase;

   fprintf(gd.tp, "<pre>\n\n Tprof Reports\n\n");
   fprintf(gd.tp, " Platform : %s\n\n", _PLATFORM_STR);

   // AIX zOS
   if (gv.clock_rate  != 0 || gv.timebase)
   {

      tcycs = trstp - trstt;

      OptVMsgLog(" trstt %f\n", trstt);
      OptVMsgLog(" trstp %f\n", trstp);
      OptVMsgLog(" tcycs %f\n", tcycs);

#ifdef _S390
      tt = tcycs / k390cycs2sec;
#else
      if (gv.timebase > 0)
      {
         tt = tcycs / gv.timebase;
      }
      else
      {
         tt = tcycs / gv.clock_rate;
      }
      fprintf(gd.tp, " %20s %15d\n", "Processors", post_num_cpus);
      fprintf(gd.tp, " %20s %15.0f\n", "ProcessorSpeed", gv.clock_rate);
#endif

      fprintf(gd.tp, " %20s %15.0f\n", "TraceTime_ns", tcycs);
      fprintf(gd.tp, " %20s %15.3f(sec)\n", "TraceTime", tt);

      if (post_slow_tsc != 0)
      {
         fprintf(gd.tp, " %20s %15d\n", "SlowTscCount", post_slow_tsc);
         fprintf(gd.tp, "\n");
         fprintf(gd.tp, "    ********************************************************************************\n");
         fprintf(gd.tp, "    *** WARNING **** WARNING **** WARNING **** WARNING **** WARNING **** WARNING ***\n");
         fprintf(gd.tp, "    *** WARNING **** WARNING **** WARNING **** WARNING **** WARNING **** WARNING ***\n");
         fprintf(gd.tp, "    ********************************************************************************\n");
         fprintf(gd.tp, "\n");
         fprintf(gd.tp, "      Processor Power Management is throttling down the processor, causing the Time\n");
         fprintf(gd.tp, "      Stamp Counter (TSC) and the processor bus to slow down and/or to stop\n");
         fprintf(gd.tp, "      completely during portions of the TProf run. TProf uses the bus clock to drive\n");
         fprintf(gd.tp, "      the TProf interrupts, and it uses the TSC to timestamp trace records so the\n");
         fprintf(gd.tp, "      trace records can be ordered by time across multiple processors.\n");
         fprintf(gd.tp, "      Because the TSC and the bus slowed down and/or stopped, the TProf tick rate\n");
         fprintf(gd.tp, "      is, most likely, incorrect, and the trace is not guaranteed to be ordered.\n");
         fprintf(gd.tp, "      The results of this run should be considered, at the very least, suspect, if\n");
         fprintf(gd.tp, "      not incorrect.\n");
         fprintf(gd.tp, "      You may want to go into the system BIOS and turn off/disable all Processor\n");
         fprintf(gd.tp, "      Power Management options and then retry the TPRof run.\n");
         fprintf(gd.tp, "\n");
         fprintf(gd.tp, "    ********************************************************************************\n");
         fprintf(gd.tp, "    *** WARNING **** WARNING **** WARNING **** WARNING **** WARNING **** WARNING ***\n");
         fprintf(gd.tp, "    *** WARNING **** WARNING **** WARNING **** WARNING **** WARNING **** WARNING ***\n");
         fprintf(gd.tp, "    ********************************************************************************\n");
      }
      fprintf(gd.tp, "\n");

#if defined (_WINDOWS) || (defined(_LINUX) && !defined(BUILD_STAP))
      if (!gv.tprof_compat)
      {
         if (gv.tprof_mode)
         {
            // event-based
            fprintf(gd.tp, " %20s, ticks every %d %s[%d]\n", "Event-based mode",
                    gv.tprof_count, GetPerfCounterEventNameFromId(gv.tprof_event), gv.tprof_event);
         }
         else
         {
            // time-based
            fprintf(gd.tp, " %20s, ticks every %d microseconds (~%d ticks/sec)\n", "Time-based mode", gv.tprof_count, gv.tprof_count);

            if (post_variable_timer_ticks) {
               fprintf(gd.tp, "\n");
               fprintf(gd.tp, "    ********************************************************************************\n");
               fprintf(gd.tp, "    *** WARNING **** WARNING **** WARNING **** WARNING **** WARNING **** WARNING ***\n");
               fprintf(gd.tp, "    *** WARNING **** WARNING **** WARNING **** WARNING **** WARNING **** WARNING ***\n");
               fprintf(gd.tp, "    ********************************************************************************\n");
               fprintf(gd.tp, "\n");
               fprintf(gd.tp, "      The kernel was built with CONFIG_NO_HZ set to YES. That causes timer ticks\n");
               fprintf(gd.tp, "      to be generated only when needed, instead of at regular intervals. There will\n");
               fprintf(gd.tp, "      be fewer timer ticks when processors are idle. As such, if there is any idle\n");
               fprintf(gd.tp, "      time during the profiling interval, the TProf rate will not be constant.\n");
               fprintf(gd.tp, "      The only way to correct the problem is to rebuild the kernel and not set\n");
               fprintf(gd.tp, "      CONFIG_NO_HZ (Don't set \"Tickless System (Dynamic Ticks)\" under Processor\n");
               fprintf(gd.tp, "      Type and Features when configuring the kernel build).\n");
               fprintf(gd.tp, "\n");
               fprintf(gd.tp, "    ********************************************************************************\n");
               fprintf(gd.tp, "    *** WARNING **** WARNING **** WARNING **** WARNING **** WARNING **** WARNING ***\n");
               fprintf(gd.tp, "    *** WARNING **** WARNING **** WARNING **** WARNING **** WARNING **** WARNING ***\n");
               fprintf(gd.tp, "    ********************************************************************************\n");
            }
         }
      }
#endif
   }

   // select reports (via the rsel option)
   if (gv.rsel == 1)
   {
      gv.ntonly = 1;                    // turn off defaults
      for (i = 0; i < 8; i++)
      {
         ropt[i] = 0;
         if (gv.rselvec[i] == '1')
         {
            ropt[i] = 1;
         }
      }
   }

   //mlev = 5;  // ???

   // DATA BASE tree from "root"
   tree_cum(root, 0);
   tree_sort(root, 0);
   gv.tottks = root->cum;

   // clumsy log(10) of tottks
   tmp = gv.tottks;
   for (i = 1; i < 10; i++)
   {
      if (tmp < 10)
      {
         gv.twidth = i;
         break;
      }
      tmp = tmp / 10;
   }

   fprintf(gd.tp, "\n\n TOTAL TICKS %14d\n", gv.tottks);

   tlim = (int)(gv.clip * gv.tottks);
   pc   = 100 * gv.clip;

   fprintf(gd.tp, " (Clipping Level : %5.1f %%  %3d Ticks)\n\n", pc, tlim);
   fprintf(gd.tp, "\n   %s\n", ban);
   fprintf(gd.tp, "      TPROF Report Summary\n\n");

   // TITLES for selected reports
   for (i = 0; i < nreps; i++)
   {
      if ( (gv.ntonly == 0) || (ropt[i] == 1) )
      {
         setrpt(rlev[i]);
         //html_link(i);
         rtitle(i);                     // generate HTML links here
      }
   }
   fprintf(gd.tp, "   %s\n\n", ban);

   for (i = 0; i < nreps; i++)
   {
      if ( (gv.ntonly == 0) || (ropt[i] == 1) )
      {
         // generate HTML tags here
         html_tag(i);
         xreport(root, rlev[i], 4 + gv.off);
      }
   }

   // extra reports
   if (gd.xfd != NULL)
   {
      fprintf(gd.tp, "\n\n   Extra Tree Reports\n");
      //fprintf(gd.tp, "    Module-Process\n");
      xinput(root);
   }

   // tprof.micro report
   if (gv.off)
   {                                    // generate Mod-Sym-Offset report
      fprintf(gd.tp, "\n Offset Report\n");
      xreport(root, "345", 4 + gv.off);
   }
   fprintf(gd.tp, "</pre>\n");
   fflush(gd.tp);
}
