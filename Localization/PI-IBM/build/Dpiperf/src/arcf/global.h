#ifndef _GLOBAL_H_
   #define _GLOBAL_H_

   #define  HKDBSZ  256 * 1024 /*STJ*/
   #define  CZERO   '\0'
   #define  ARCF_MAX_CPUS      64

/* ************************************************************ */
   #include "encore.h"

   #include <sys/types.h>
   #include <stdio.h>
   #include <stdarg.h>

   #include "utils/common.h"

   #ifdef OptMsgRoutine
      #define OptMsg(X) OptVMsg(X)
   #else
      #define OptMsg(X) if (gv.verbose) fprintf(stderr,X)
   #endif

#define dbgmsg(...)                       \
   do {                                   \
      if (gv.dbgmsg) {                    \
         fprintf(stderr, __VA_ARGS__);    \
         fflush(stderr);                  \
      }                                   \
   } while (0)

typedef unsigned int aint;              /* address int */
typedef char *   STRING;
typedef unsigned short int usint;

typedef double   UNIT;

   #ifdef _AIX
      #define LKCONV
      #define ENDIAN 1

      #define RD16REV(n) ( ( (n) & 0xff) << 8) | ( ( (n >> 8) & 0xff) )
      #define RD16REVB(n) (n)
      #define RD32REV(n) (((n)&0xff)<<24) | (((n>>8)&0xff)<<16) | \
      (((n>>16)&0xff)<<8) | (((n>>24)&0xff))
      #define RD32REVB(n) (n)
   #endif

   #ifdef _OS2
      #include    <os2.h>
      #define LKCONV  _Optlink
      #define ENDIAN 0

      #define RD16REV(n) (n)
      #define RD16REVB(n) ( ( (n) & 0xff) << 8) | ( ( (n >> 8) & 0xff) )

      #define RD32REV(n) (n)
      #define RD32REVB(n) (((n)&0xff)<<24) | (((n>>8)&0xff)<<16) | \
      (((n>>16)&0xff)<<8) | (((n>>24)&0xff))
   #endif

   #ifdef _WIN32
      #define LKCONV
      #define ENDIAN 0

      #define RD16REV(n) (n)
      #define RD16REVB(n) ( ( (n) & 0xff) << 8) | ( ( (n >> 8) & 0xff) )

      #define RD32REV(n) (n)
      #define RD32REVB(n) (((n)&0xff)<<24) | (((n>>8)&0xff)<<16) | \
      (((n>>16)&0xff)<<8) | (((n>>24)&0xff))
   #endif

   #ifdef _LINUX
      #define LKCONV
      #define ENDIAN 0

      #define RD16REV(n) (n)
      #define RD16REVB(n) ( ( (n) & 0xff) << 8) | ( ( (n >> 8) & 0xff) )

      #define RD32REV(n) (n)
      #define RD32REVB(n) (((n)&0xff)<<24) | (((n>>8)&0xff)<<16) | \
      (((n>>16)&0xff)<<8) | (((n>>24)&0xff))
   #endif

   #ifdef _ZOS
      #define LKCONV
      #define ENDIAN 1

      #define RD16REV(n) ( ( (n) & 0xff) << 8) | ( ( (n >> 8) & 0xff) )
      #define RD16REVB(n) (n)
      #define RD32REV(n) (((n)&0xff)<<24) | (((n>>8)&0xff)<<16) | \
      (((n>>16)&0xff)<<8) | (((n>>24)&0xff))
      #define RD32REVB(n) (n)
   #endif

/* ************************************************************ */
/* File Related */
extern STRING nrmfile;
extern STRING nrmarr[8];                /* array of input files */
extern STRING sumfile;

extern FILE * nrm;                      /* nrm file */

/* ************************************************************ */
/* Hook definition & usage stats array */
/* ARCFLOWS HKDB. UNIQUE ARCFLOW NAMES */
struct HKDB
{
   int    mat;                          /* unmatched(0) matched(1) */
   aint   hkkey;                        /* hk key for comparisons */
   STRING hstr;                         /* method name */
   STRING modnm;                        /* module name */

   int    first;                        /* set on first instance of hook in trace */
   int    cnt;                          /* freq in trace */

   /* arcflow */
   UINT64 calls;                        /* entries only  */
   UNIT   btm;                          /* base time */
   UNIT   ctm;                          /* cumulative time(correct w recursion) */
   UNIT   ctma;                         /* cumulative time(absolute: delta tree) */
   UNIT   ctm2;                         /* cumulative time(overcounted w recursion) */
   UNIT   wtm;                          /* wall clock time */
   struct TREE * nxm;                   /* nextme anchor */
   struct TREE * tree;                  /* parent/child tree anchor */

   /* vector metric */
   UNIT   btmv;                         /* base time */
   UNIT   ctmv;                         /* cumulative time(correct w recursion) */

   int    rcnt;                         /* rec cnt in curr tree ??? */
   STRING hstr2;                        /* same as hstr(compatibility w parx) */
   STRING hstr3;                        /* same as hstr(compatibility w parx) */

   /* accum parent, child, given */
   /* total(recursive & non-rec) - sort on this */
   UINT64 tcalls;
   int    trtrns;
   UNIT   tbtm;
   UNIT   tctm;

   /* non-rec only */
   UINT64 nrcalls;
   UNIT   nrbtm;
   UNIT   nrctm;

   int    pind;                         /* index in xprof */
   int    ring;                         /* ring 0 | 3 */
   int    cpu;
};

/* RJU 9/26/02 */                       // => 128K. When next double reqd ??
struct HKDB * hkdb[HKDBSZ];

/* ************************************************************ */
/* PIDTID */
struct pidtid_entry
{
   aint    pt;                          /* decimal pidtid */
   STRING  apt;                         /* ascii   pidtid */

   UNIT    atime;                       /* accumulated time in pidtid */
   UNIT    stttime;                     /* 1st time in pidtid */
   UNIT    stptime;                     /* last time in pidtid */

   /* added for arcflow */
   struct TREE * ctp;                   /* pt curr node index */
   struct TREE * rtp;                   /* pt root node index */

   int     flg30;                       /* last hook a 0x30 major entry hook */
   int     first;                       /* used to init "time" for some units */
   int     lflg;                        /* lock flag */
   int     kflag;                       /* k state - 0:app 1:kernel */
   char    svcnm[ARCF_MAX_CPUS];        /* remembered from glink */

   int     loc0;                        /* last opc(prev bb) - ring0 */
   int     loc3;                        /* last opc(prev bb) - ring3 */
   int     ring;                        /* last ring ?? */
};

typedef struct pidtid_entry    PIDTID;
typedef struct pidtid_entry * pPIDTID;

   #define MAX_PTARR 24*1024
pPIDTID PTarr[MAX_PTARR];               /* max number of pidtid's */
/* NB. change ptptr to binary search */

typedef struct _chist CHIST;
struct _chist
{
   UNIT ctm;
   UNIT btm;
   int nodes;
};

// Input file types, gv_ftype_XXXX
// MJD added gv_ftype_genericWAS .

   #define  gv_ftype_aixtrc        -2
   #define  gv_ftype_nrm           -1
   #define  gv_ftype_javaosbin      0
   #define  gv_ftype_java           1
   #define  gv_memflag_jbytecodes     0
   #define  gv_memflag_jheapcalls     1
   #define  gv_memflag_jheapbytes     2
   #define  gv_memflag_jra_heapcalls  3
   #define  gv_memflag_jra_heapbytes  4
   #define  gv_memflag_jrf_heapcalls  5
   #define  gv_memflag_jrf_heapbytes  6
   #define  gv_ftype_javascript     2
   #define  gv_ftype_plumber        3
   #define  gv_ftype_jctree         5
   #define  gv_ftype_generic        6
   #define  gv_ftype_javaos         7
   #define  gv_ftype_jvar_stks      8
   #define  gv_ftype_jvar_stks2     9
   #define  gv_ftype_jaix           10
   #define  gv_ftype_jaixjit        11
   #define  gv_ftype_javaos_tpstks  12
   #define  gv_ftype_xtree_sub      13
   #define  gv_ftype_xtree_add      14
   #define  gv_ftype_mtree          15
   #define  gv_ftype_mtree_cycs     16
   #define  gv_ftype_genericIJ      17
   #define  gv_ftype_genericMP      18
   #define  gv_ftype_genericDel     19
   #define  gv_ftype_genericL       20
   #define  gv_ftype_mtreeN         21
   #define  gv_ftype_allocStacks    22
   #define  gv_ftype_generic2       23
   #define  gv_ftype_mtreeO         24
   #define  gv_ftype_genericN       25
   #define  gv_ftype_genericMod     26
   #define  gv_ftype_xtree_min      27
   #define  gv_ftype_genericWAS     28
   #define  gv_ftype_ctrace         100
   #define  gv_ftype_itrace         101

/* ************************************************************ */
/* Global Variables */
struct Global
{
   pPIDTID ptp;                         /* ptr to current PIDTID entry */
   pPIDTID aptp[ARCF_MAX_CPUS];         /* ptr to current PIDTID entry per cpu */
   STRING  apt;                         /* present string pidtid  */
   int PTcnt;                           /* unique pidtid's */

   hash_t  jstack_table;                // jstack hash table
   hash_t  method_table;                // method hash table

   UNIT dtime;                          /* curr time */
   UNIT odtime;                         /* prev hook time */
   UNIT deltime;                        /* delta time */
   UNIT tottime;                        /* tot hook time */

   aint hkstt;                          /* starting hook cnt    */
   aint hkstp;                          /* stopping hook cnt    */
   aint hkccnt;                         /* current hook count   */

   int  hkcnt;                          /* hkdb size(0 based)   */
   int  trcnt;                          /* total tree nodes */

   char indent[128];                    /* ----+----+ ...  */
   char nstr[1];                        /* null str */
   int  db;                             /* debug var */
   int  flow;                           /* 1 => output ascii flow file */
   int  curcpu;                         /* CPU for arc record we're standing on */
   int  append_cpu;                     /* 1 => append CPU to *ALL* x-file records */
   int  append_cpu_pidtid;              /* 1 => append CPU *ONLY* to "pidtid" x-file records */
   int  filter_cpu;                     /* CPU number to filter on. -1 => all */
   char * filter_pidtid;                /* pid_tid to filter on. NULL => all */
   int  group_disp;                     /* group flow by dispatches on processor */
   FILE * ftempcpu[ARCF_MAX_CPUS];      /* per-cpu output buffer */
   char * curthr[ARCF_MAX_CPUS];        /* current pid/tid */
   int  ftemp_cnt[ARCF_MAX_CPUS];       /* number of records in buffer. 0 => empty */
   int  dbgmsg;                         /* display debug messages */
   int  cond;                           /* 1 => output ascii condensed flow file */
   int  tlines;                         /* tree output lines since last stack dump */
   char hdstr[256];                     /* hook data string(resolved from hk parms) */

   int  ftype;                          // log file type & varient, uses gv_ftype_XXXX

   int    wflag;                        /* wall clock flag: 0=no, 1=yes */
   STRING Units;                        /* base & cum Units */
   int    raw;                          /* if >= 1 divide into raw units */
   /* else dont print raw fields */
   int    pwid;                         /* print width of raw/scaled fields */
   UNIT   tunits;                       /* total units */

   /* bin javaos log */
   UNIT hightm;                         /* high 32 bits of time(a8:01 hook) */
   FILE * fdb;                          /* debug output file */
   int  cnmcnt;                         /* class cnt */
   char ** cnm_anc;                     /* class name radix tree anchor */
   int  mnmcnt;                         /* method cnt */
   char ** mnm_anc;                     /* class name radix tree anchor */

   /* units, where not explicit in input file */
   int    uflag;
   char * ustr;

   int tags;                            /* tags */
   int dptr;                            /* include dptr in xtree & xprof & xarc */
   int jhdf;                            /* jstem hdf files */
   int ind;                             /* 1: indent str in xtree. 0: no ind str */
   int ptype;                           /* plumber direction 1: top->down 2: bottom up */

   int rev;                             /* reverse child/sib order */
   int norecur;                         /* non-recursive filter */
   int allrecs;                         /* (-a)adorn xflow with unused records */
   int emode;                           /* 0: old way. 1: treat all err like BAD err */
   int mod;                             /* 1: include modnm in err handling */
   int nobtree;                         /* 1: no xbtree */
   int deltree;                         /* del tree mode */

   char * infiles[8];
   int    ninfiles;
   int    xmin;                         /* xtree_min */
   int    ksum;                         /* summarize kernel activity */
   int    nicnt;                        /* dont count interrupts */

   /* collapse tree */
   int    ctree;
   char * ctstr[16];
   int    html;                         /* add html markup to xarc */
   int    tsort;                        /* sort child by cum for xtree output */
   int    nopre;                        /* remove I: J: C: prefixes */

   // user control of MPRUNE & MPROF
   UNIT   prune;                        // xtree & xarc pruning in % ( 0 => only keep != 0)
   UNIT   pcum;                         // xtree & xarc pruning level in UNITS
   int    mprof;

   int    sobjs;                        // separate objects by size

   int    snms;                         // short names in par/chd trees
   int    pdep;                         // depth of xarc Parent trees     d:0   -pdep n
   int    cdep;                         // depth of xarc Child trees      d:0   -cdep n
   int    sdep;                         // depth of xarc Selfs with trees d:100 -sdep n
   double ptclip;                       // parent tree clip level %.      d:1%  -ptclip n
   double ctclip;                       // child  tree clip level %.      d:1%  -ctclip n
   double pclip;                        // parent tree clip level UNITS.
   double cclip;                        // child  tree clip level UNITS.
   double cnode;                        // confounding node analysis
   CHIST  chist[30];

   // compact thread name (1st match from a file)
   int    tnms;
   char * tnmarr[32];

   // where idle
   char * wfnm;
   FILE * where;

   FILE * tfile;
   int    verbose;                      // Enable Optional Messages
   int    pruneSize;                    // Number in pruned files
   int    abs;                          // Input values are absolute

   int    oldheader;                    // Old header style

   #define MAX_BASES 32
   char * baseName[MAX_BASES];          // Array of Base Names
   char * baseDesc[MAX_BASES];          // Array of Base Descriptions
   int    numBases;                     // Number of Bases

   #define MAX_TOKENS 1024
   char * asTokens[MAX_TOKENS];         // Array of token pointers
   int    maxTokens;                    // Max tokens for next call to tokenize()
};
extern struct Global gv;                /* global variable struct */

/* db const & finds */
struct HKDB  * const_hkdb(int db);      /* hkdb constructor */
struct TREE  * const_tree(void);        /* returns tree ptr */

/* ************************************************************ */
/* Function Prototypes */
/* ************************************************************ */
/* util.c */
STRING printrep(unsigned char c);
STRING Remember(STRING s);
int    ptrace(STRING name);
void   panic(STRING why);
void   pexit (char *s);
STRING * tokenize(STRING input, int * tcnt);

/* parx.c */
pPIDTID ptptr(STRING pt);               /* retns ptr to curr pidtid struct */
void    pt_init(pPIDTID p);             /* arcf unique pidtid init(empty now) */

/* out.c */
void out(void);                         /* output reports */

/* ****************************
 * global.c
 *   Global Defines
 *   File Opens
 *   Qsort & Bsearch Routines
 */
void          init_global(void);        /* init all global vars & structs */

struct HKDB * hkdb_find(STRING meth, int flg);
struct HKDB * add_hook(STRING meth);
struct TREE * pushName(struct TREE * tp, struct HKDB * hp);

void hdr(FILE * fh);
void push(struct HKDB * hp);

/****************************************/
/* includes for reading javaos hooks */
/*  either binary format(dekko/javaos) */
/****************************************/
struct thelem
{                                       /* threads : tblk -> tnm */
   aint   tblk;
   STRING tnm;
   struct thelem * next;
};
typedef struct thelem TELEM;
typedef struct thelem * pTELEM;
pTELEM thrd_anc;

struct class_entry
{                                       /* classes : cb -> cnm */
   aint   cb;
   STRING cnm;
};
typedef struct class_entry CLENT;
typedef struct class_entry * pCLENT;

struct mnm_entry
{                                       /* methods : mb -> mnm */
   aint   mb;
   int    flag;                         /* 1 => had to use mblk ??? */
   STRING mnm;
};
typedef struct mnm_entry MENT;
typedef struct mnm_entry * pMENT;

/* ************************************************************ */
/* javaos raw Global Variables */
struct graw
{
   FILE * db;                           /* */
   int  cnmcnt;                         /* class cnt */
   char ** cnm_anc;                     /* class name radix tree anchor */
   int  mnmcnt;                         /* method cnt */
   char ** mnm_anc;                     /* class name radix tree anchor */
};
extern struct graw gvr;                 /* global variable struct */

void proc_ctrace(STRING fname);
void proc_itrace(STRING fname);
void acct2(void);

#endif /* #ifndef _GLOBAL_H_ */
