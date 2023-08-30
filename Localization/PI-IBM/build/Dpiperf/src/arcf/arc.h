#ifndef _ARC_H_
   #define _ARC_H_

   #define MPRUNE 60  /* clipping xtree */
   #define MPROF  25  /* clipping xprof */

/* ************************************************************ */
/* arc.c */
extern FILE * xtree;                    /* tree */
extern FILE * xtreep;                   /* tree.p */
extern FILE * xbtree;                   /* big tree */
extern FILE * xprof;                    /* flat profile */
extern FILE * xprofp;                   /* summary flat profile */
extern FILE * xarc;                     /* flat profile */
extern FILE * xarcp;                    /* flat profile */
extern FILE * xgiven;                   /* all the a given output */
extern FILE * xbecof;                   /* all x Because of y */
extern FILE * xflow;                    /* Indent flow output */
extern FILE * xcondu;                   /* Unsorted condensed flow output */
extern FILE * plist;                    /* list of parents. child given parent */

FILE * xopen(STRING fn, STRING opt);

STRING get_indent(int lev);

/* ************************************************************ */
/* Tree */
struct TREE
{
   struct HKDB * hp;                    /* pointer to HKDB */
   pPIDTID ptp;                         /* pidtid ptr(added for xarc by pidtid) */

   struct TREE *   par;
   struct TREE *   chd;
   struct TREE *   sib;
   struct TREE *   nxm;                 /* next me */

   UINT64 calls;                        /* calls */
   UNIT   btm;                          /* base time */
   UNIT   ctm;                          /* cum  time */
   UNIT   ctma;                         /* absolute cum  time */
   UNIT   btmv;                         /* base time 2 */
   UNIT   ctmv;                         /* cum  time 2 */

   /* condensed flow data */
   UNIT   crttm;                        /* node creation time */
   UNIT   stt;                          /* start time for curr invocation */
   UNIT   btmx;                         /* base time for curr invocation */
   UNIT   wtm;                          /* wall clock time */

   char * hdstr;                        /* null for arcf */

   int    dptr;                         /* rtarcf data dptr */
   int    chc;                          /* hk cnt at start of curr invocation */
   /* PARX/xcond. hook parms at entry */

   /* tree structure */
   int    lev;                          /* level/depth in tree */
   int    rlev;                         /* recursive lev/depth in this tree */
   int    pass;                         /* xtree_min. pass1 | 0x1, pass2 | 0x2 */
};

/* cpu to thread name */
extern aint cpu2tnm[64];

/* top ctm's for xtree.p xarc.p */
void top_ctm(UNIT ctm, UNIT btm);
// UNIT hctm[MPRUNE];
UNIT * hctm;

/* top 25 btm's for xprof.p */
void top_btm(UNIT n);
UNIT hbtm[MPROF];

void push(struct HKDB * p);

extern struct HKDB * * php;             /* used hooks & pidtids */
extern struct HKDB * * pha;             /* arcflow tmp usage */
extern int nhu;                         /* number of non zero(hooks or pidtids) */
extern int ga_err;                      /* arcflow err. add warning on all reports */
extern char ga_errMess[1000];

extern int    memflag;
extern int    g_oopt;
extern int    g_lopt;
extern char   nullstr[2];
extern char * g_ostr;

void arc_out(int opc, struct HKDB * h);
void arcMP_out(int cpu, int opc, struct HKDB * h);
void arc_out2(int opc, struct HKDB * hp);
/* void acct(void); */                  /* collision with linux inc */
void acct_x(void);
void units(FILE * fd);
void pxflush(void);
void xflush(void);
void out_pt(void);
void out_plist(void);

/* arcflow prototypes */
void arc_parent(struct HKDB * hp, FILE * fh, UNIT clip);
void arc_self  (struct HKDB * hp, FILE * fh, UNIT clip);
void arc_child (struct HKDB * hp, FILE * fh, UNIT clip);
void profile(struct HKDB * hpp, int flg);
void out_prof(void);

UNIT walk1(struct TREE * p, int lev, FILE * fn);
void walk2(struct TREE * p, int lev, FILE * fn);
void walk3(struct TREE * p, struct HKDB * hpar);
void walk4(struct TREE * p, struct HKDB * hpar);
void walk5(struct TREE *);
UNIT walk1p(struct TREE * p, int lev);
void walk2p(struct TREE * p, int lev, FILE * fn);
void init_arc(void);
void walk1min(struct TREE * p, int lev);
UNIT walk1abs(struct TREE * p, int lev, FILE * fn);
UNIT walk1pabs(struct TREE * p, int lev);

void  proc_jhdf(char * fname);
int   proc_nrm(STRING fname, int oopt);
void  proc_inp(STRING fname, int memflag, int lopt, char * ostr);
void  proc_jraw(STRING nrmfile);
void  proc_hdf(STRING fname);

void  Scan_Args(int argc, STRING * argv);
void  Usage(void);

/* parx specific */
int hkcomp(struct HKDB * h1, struct HKDB * h2, int push);
int dump_rad(int dcnt);
struct HKDB * hp_x2e(struct HKDB * hpp);

   #ifdef STRICMP
      #define strcasecmp stricmp
      #define strncasecmp strnicmp
   #endif

#endif /* #ifndef _ARC_H_ */

