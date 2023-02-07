/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* perf53A src/bos/usr/sbin/perf/tools/lib/libptools/Symlib/symlib.h 1.15 */
/*                                                                        */
/* Licensed Materials - Property of IBM                                   */
/*                                                                        */
/* Restricted Materials of IBM                                            */
/*                                                                        */
/* (C) COPYRIGHT International Business Machines Corp. 2002,2004          */
/* All Rights Reserved                                                    */
/*                                                                        */
/* US Government Users Restricted Rights - Use, duplication or            */
/* disclosure restricted by GSA ADP Schedule Contract with IBM Corp.      */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */
/* @(#)66 1.15 src/bos/usr/sbin/perf/tools/lib/libptools/Symlib/symlib.h, cmdperft, perf53A, a2004_33C1 8/6/04 03:08:31 */
#ifndef _SYMLIB_H
#define _SYMLIB_H

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
/* Print format */
#ifdef  __64BIT__
/* uint64 is long */
#define LLD "ld"
#else   /* _ILP32 */
/* uint64 is long long */
#define LLD "lld"
#endif

#define MAXSYMLEN  4096

/* lookup commands */
#define  LKUP_FUNCTION	0x0001	 /* find function containing address   */
#define  LKUP_MARK_ONLY	0x0100	 /* simply mark the binary to indicate 
				  *                  symbols are needed */

/* flags to table initialization routines  */
#define  INIT_SHOBJ	0x001	 /* Do all kex and libs on ksym_init  */
#define	 INIT_SYMS	0x002	 /* Fill in symbols on init */
#define	 INIT_SORT	0x004	 /* Sort symbols. INIT_SYMS must also be set */
#define  INIT_STRIP     0x008	 /* Force use of traceback tables in syml_nm */
#define  INIT_PARAMS    0x010	 /* Fill in params to functions  */
#define  INIT_ARCHIVE   0x020	 /* For archives, create bintabs for members */
#define  INIT_KERNEL    0x100	 /* Indicates this is the kernel  */
#define  INIT_KPROC     0x200	 /* This is a kproc, no bintab  */
#define  INIT_INSTR     0x400	 /* Collect instructions */

/* commands for printing to gensyms file */
#define  GS_KERNEL	0x0001	 /* print kernel symbols */
#define  GS_KEX		0x0002	 /* print kernel extension symbols */
#define  GS_LIB32	0x0004	 /* print 32 bit shared library symbols */
#define  GS_LIB64	0x0008	 /* print 64 bit shared library symbols */
#define  GS_PIDS        0x0010	 /* print from list of pids */
#define  GS_BINARIES	0x0020	 /* print symbols from binary list */
#define  GS_ALLPIDS	0x0040	 /* print symbols for all installed pids */
#define  GS_ALLBINS	0x0080	 /* print symbols for all installed binaries */
#define  GS_MARKED	0x0100	 /* print only symbols from marked binaries */
#define  GS_APPEND	0x0200	 /* append to existing file */
#define  GS_NOSRC	0x0400	 /* do not print source file names */
#define  GS_OFFSET	0x0800	 /* print symbol's offset instead of load address */
#define  GS_KEEP	0x1000	 /* do not free syms after printing */
#define  GS_TEXTONLY	0x2000	 /* print only the text section symbols  */
#define  GS_LOCKNAMES	0x04000	 /* print lock clas names  */

/*  java stuff */
#define  GS_JPID	0x08000	 /* print java pid line */
#define  GS_JCLASS	0x10000	 /* print java class load line */
#define  GS_JUNLOAD	0x20000	 /* print java class unload line */
#define  GS_JMETHID	0x40000	 /* print java method id line */
#define  GS_JMETHADDR	0x80000	 /* print java method load address line */

#define  GS_INSTR	0x100000 /* print instructions */

/*  Useful combinations  */
#define  GS_ALLLIBS	(GS_LIB32 | GS_LIB64)
#define  GS_ALLSYMS	(GS_KERNEL | GS_KEX | GS_ALLLIBS | GS_ALLPIDS)
#define  GS_JMASK	(GS_JPID | GS_JCLASS | GS_JUNLOAD | \
				GS_JMETHID | GS_JMETHADDR)

/* gensyms format */
#define HEXPRT    "%08llx %c %s\n"
#define HEXPRT2   "%08llx %c %s %llx\n"
#define HEXPID    "Pid: %06lld %08llx %08llx %08llx %s\n"
#define HEXLIB32  "Lib32: %08llx %08llx %08llx %s\n"
#define HEXLIB32D "Lib32_data: %s %08llx %08llx %08llx\n"
#define HEXLIB64  "Lib64: %016llx %016llx %016llx %s\n"
#define HEXLIB64D "Lib64_data: %s %016llx %016llx %016llx\n"
#define HEXFILE32 "File-32: %08llx %08llx %08llx %s\n"
#define HEXFILE64 "File-64: %016llx %016llx %016llx %s\n"
#define HEXFILED  "File_data: %s %016llx %016llx %016llx\n"
#define HEXKERN   "Kernel: %08llx %08llx %08llx %s\n"
#define HEXKERND  "Kernel_data: %08llx %08llx %08llx\n"
#define KERN_VALID "Kernel_validated: %8d\n"
#define HEXKEX    "Kex: %08llx %08llx %08llx %s\n"
#define HEXKEXD   "Kex_data: %s %08llx %08llx %08llx\n"
#define HEXMOD    "Kex%-7s    %08llx %08llx %s\n"
#define HEXSRC    "Src: %s\n"
#define HEXHDR    "Hdr: %s\n"
#define HEXLCK    "%d l %s\n"
#define HEXINSTR  "Instr: %llx"
#define HEXINSTR2 "%06x %08x"
#define HEXINSTR3 " %08x"

/*  java stuff  */
#define HEXJPID   "Java: %s %6lld %s\n"
#define HEXJCL    "Class_load:  %19.9f %08llx %08llx %ld %s %s\n"
#define HEXJUNLD  "Class_unload:  %19.9f %08llx %08llx\n"
#define HEXMINFO  "%08llx m %08llx %08llx %s %s\n"
#define HEXMLOAD  "%08llx j %08llx %08llx %08x %s\n"
#define GEN_USING_OFFSETS       "Using offsets for all symbols\n"
#define GEN_USING_VADDRS        "Using absolute addresses for all symbols\n"

/* gennames format */
#define GENFROM   "Symbols from %s\n"
#define GEN64PID  "Pid: %06lld %016llx %016llx %016llx %s\n"
#define GENPID    "Pid: %06lld %08llx %08llx %08llx %s\n"
#define GENSRC    "%-27s|%10s|%6s|%10s|%6s|%5s|%-s\n"
#define GENPRT    "%-27s|%10llu|%6s|%10s|%6s|%5s|%-s\n"
#define GENINIT   " %s | %8.8llx| %8.8llx| %8.8llx|   |   |initialize \n"

typedef enum
{
  IS_ANYTHING,
  IS_KERNEL,
  IS_EXEC32,
  IS_EXEC64,
  IS_KEX,
  IS_LIB32,
  IS_LIB64,
  IS_JAVA,  /* pseudo file type used for storing method load info */
  IS_ARCHIVE
} bin_type;

typedef enum
{
  IS_NONE,
  IS_AIX,
  IS_LINUX
} sys_type;

#define SYML_METHOD_NAME_FORMAT_1  "%s~%s~%s"
#define SYML_METHOD_NAME_FORMAT_2  "%s~%s"

#define GETLOCK(__lock, __tid, __retry)				\
{								\
	if (__lock != __tid)					\
		while (cs(&__lock, 0, __tid) && (__retry--))	\
			usleep(5000);				\
}

#define UNLOCK(__lock) {__lock = 0;}

/*
 * Generic Hash Tables
 */

typedef struct syml_bucket_t
{
    struct syml_bucket_t *next;
    void *content;
} syml_bucket_t;

typedef struct
{
    uint n_entries;            /* number of buckets in the hash table */
    uint size;                 /* size of the hash table */
    syml_bucket_t **entries;
    uint64_t (*hash_f)(void *);    /* hashing function */
    uint (*size_f)(void *);    /* struct size */
    int (*compare_f)(void *, void *);  /* comparison function */
} syml_hash_t, *syml_hash_p;

#ifndef _AIX
/*
 * Java method
 */
struct _jmethodID;
typedef struct _jmethodID *jmethodID;
#endif

typedef struct symentry
{
    char 	    *name;          /* symbol, Java method, or file name */
    char            *source;        /* source file name */
    char            *binary;        /* name of binary (from binsym_lookup) */
    uint64_t         addr;          /* symbol offset from section start */
    uint64_t         size;          /* method size */
    uint_t          *instructions;  /* array of binary instructions */
    char	     scn_name[8];   /* section where symbol lives */
    bin_type	     file_type;     /* binary type, same as bintable below */
    char             storclass;     /* symbol storage class: C_EXT, ...  */
    u_char           fixedparms;    /* Number of fixed point parameters */
    u_char           floatparms;    /* Number of floating point parameters */
    u_char           parmsonstk;    /* Set if all parms are placed on stack */
    u_int            parminfo;      /* Order and type encoding of paramters:
			             * Left-justified bit-encoding as follows:
			             * '0'  ==> fixed parameter
			             * '10' ==> single-precision float parameter
			             * '11' ==> double-precision float parameter
			             */
    void            *s_private;     /* for programmer use */
    struct symentry *alias;         /* when adress have multiple entries name */
} symentry;
#define SYMENTRYSIZE sizeof(symentry)

/*  Each bintab will hold two symbol tables, one each for text and data */

#define SYM_TEXT  0
#define SYM_DATA  1

/*
 * Structure for a single entry in hash table of
 * binaries and shared libraries
 */

typedef struct bintable
{
    char            *name;         /* pathname of binary */
    symentry        *sym_base[2];  /* symbol tables */
    uint             sym_size[2];  /* currently allocated size */
    uint             sym_count[2]; /* number of symbols */
    uint64_t         tx_size;      /* size of text section */
    uint64_t         tx_offset;    /* offset of text section */
    uint64_t         dt_size;      /* size of data section (+ bss) */
    uint64_t         dt_offset;    /* offset of data section */
    pthread_t        bt_lock;      /* for messing with the symbol tables */
    bin_type	     bt_type;	   /* binary type */
    unsigned         is_64:1;      /* binary is 64 bit */
    unsigned	     no_dynsyms:1; /* symbols cannot be installed dynamically */
    unsigned         no_adjust:1;  /* Addresses are absolute if set */
    unsigned	     needsyms:1;   /* symbols need to be installed */
    unsigned         tx_needsort:1;/* text table needs to be sorted */
    unsigned         dt_needsort:1;/* data table needs to be sorted */
    unsigned	     bt_maxdata:1; /* data loaded at higher address */
    unsigned         printed:1;    /* table has been printed */
} bintable;

/*
 * Structure for linked list of entries describing the
 * binary and shared libraries loaded for the PID
 */

typedef struct pidentry
{
    char            *name;          /* pathname of binary */
    struct pidentry *next;          /* next entry or NULL */
    uint64_t         bstart[2];      /* load address of section */
    uint64_t         bend[2];        /* end address of section */
    uint64_t         pid;           /* PID */
    bintable        *bintab;        /* pointer to bintable */
    int              count;         /* references to this module */
    void            *p_private;     /* pointer for caller private data */
} pidentry;
#define PIDENTRYSIZE sizeof(pidentry)

/*
 * Structure for a single entry in the hash table of
 * active processes
 */

typedef struct pidtable
{
    uint64_t         pid;           /* PID */
    pthread_t        pt_lock;       /* for messing with the pidentries, etc. */
    pidentry        *b_entry;       /* pointer to table of loaded binaries */
    syml_hash_p      classtab;      /* class hash table */
    syml_hash_p      methodtab;     /* method hash table */
} pidtable;

/*  thread table */
typedef struct tidtable
{
    tid_t        tid;                   /* Thread ID */
    pid_t        pid;                   /* Process ID */
    pidtable    *pt;                    /* Pointer to pidtable */
    void        *t_private;             /* Hook for caller's private data */
} tidtab_t, *tidtab_p;

typedef struct sotable
{
    uint64_t         so_lowtx;     /* lowest virtual address for text sect. */
    uint64_t         so_hightx;    /* highest virtual address for text sect. */
    uint64_t         so_lowdt;     /* lowest virtual address for data sect. */
    uint64_t         so_highdt;    /* highest virtual address for data sect. */
    pidentry        *so_entry;     /* pointer to table of loaded objects */
    pthread_t        so_lock;      /* for messing with the entries */
} sotable_t, *sotable_p;

#define PIDTABLESIZE sizeof(pidtable)

/*
 * Structure for building Linux loaded module entries.  Not used in AIX
 */

typedef struct modtab
{
    char            *moduleid;
    char            *module;
    int64_t          start;
    int64_t          end;
    int64_t          offset;
    int              status;
    struct modtab   *next;
} modtab;
#define MODTABSIZE sizeof(modtab)

/* Java pid info  */
typedef struct syml_jpid_info
{
    uint64_t	jp_pid;
    char 	*jp_machine;
    int		jp_is64;
} *syml_jpid_info_p;

/* Info from java class load event  */
typedef struct syml_class_load_info
{
    double	cl_time;
    uint64_t	cl_envid;
    uint64_t	cl_id;
    char 	*cl_name;
    char	*cl_source;
    int		cl_methods;
} *syml_class_load_info_p;

/* Info from java class unload event  */
typedef struct syml_class_unload_info
{
    double	cu_time;
    uint64_t	cu_envid;
    uint64_t	cu_id;
} *syml_class_unload_info_p;

/* Info on methods from java class load event  */
typedef struct syml_method_info
{
    uint64_t	mi_envid;
    uint64_t	mi_id;
    uint64_t	mi_classid;
    char 	*mi_name;
    char	*mi_signature;
} *syml_method_info_p;

/* Info on method load addresses from method load event */
typedef struct syml_method_load_info
{
    uint64_t	ml_envid;
    uint64_t	ml_id;
    uint64_t	ml_loadaddr;
    uint	ml_size;
    char        *ml_name;
} *syml_method_load_info_p;

typedef struct locktable
{
    uint64_t         class_id;           /* Lock class ID */
    char             *name;          /* Lock class name */
} locktable;

extern char     *SymVersion;
extern char     *syml_system_id;
extern char      no_source[];
extern char     *syml_kernel_name;
extern char     *Symfile;
extern sys_type  syml_sysmode;

/*
 * Function prototypes
 */

#ifdef _AIX
int syml_install_all_libs(int);
int syml_install_aix_pid(uint64_t, int);
#endif /* _AIX */

symentry *syml_binsym_lookup(uint64_t, uint64_t, uint64_t *, int);
int syml_install_bin(char *, uint64_t, uint64_t, uint64_t, uint64_t, int);
bintable *syml_install_binary(char *, int);
int syml_install_one_pid(uint64_t, uint64_t, uint64_t, uint64_t, char *, int);
int syml_uninstall_all(void);
int syml_uninstall_one_pid(uint64_t);
int syml_install_all_pids(int);
int syml_install_all_kex(int);
int syml_install_all_symbols(char *, uint_t);
int syml_dupapid(uint64_t, uint64_t);
symentry *syml_get_ksyms(int index, uint *count); 
bintable *syml_lookup_bintab(char *);
bintable *syml_insert_bintab(char *);
void syml_remove_bintab(char *);
void syml_remove_bintab_all();
pidtable *syml_lookup_pidtab(uint64_t);
pidtable *syml_walk_all_pids(pidtable *);
tidtab_p syml_lookup_tidtab(uint64_t);
pidtable *syml_insert_pidtab(uint64_t);
tidtab_p syml_insert_tidtab(uint64_t);
void syml_remove_pidtab(uint64_t);
void syml_remove_tidtab(uint64_t);
void syml_remove_pidtab_all(void);
void syml_remove_tidtab_all(void);
symentry *syml_kernel_lookup(uint64_t, int, int);
int syml_ksym_init(char *, int);
int syml_symbols_from_file(char *);
symentry *syml_ksym_install(char *, uint64_t, char *, int);
symentry *syml_binsym_install(char *, uint64_t, char, char *, bintable *);
void syml_ksym_uninstall(void);
void syml_remove_class(uint64_t, pidtable *);
void syml_remove_class_all(pidtable *);
char *syml_method_name(uint64_t method_id, pidtable *, char **, char **, char **, char **);
uint64_t syml_method_class_id(uint64_t, pidtable *);
void syml_remove_method(uint64_t, pidtable *);
void syml_remove_method_all(pidtable *);
int syml_get_kaddrs(uint64_t *, uint64_t *, uint64_t *);
int syml_remove_symbols(char *, int);
int syml_print_to_file(char *, uint, int, void *);
int syml_print_java(FILE *, uint, void *);
bintable *syml_nm(char *name, char *member, uint flag);
void sym_sort(bintable *bt, int index);
bintable *syml_next_bintab(bintable *);
pidtable *syml_next_pidtab(pidtable *);
tidtab_p syml_next_tidtab(tidtab_p);
int syml_get_lib32addrs(int ndx, uint64_t *low, uint64_t *high);
int syml_get_lib64addrs(int ndx, uint64_t *low, uint64_t *high);
int syml_get_kexaddrs(int ndx, uint64_t *low, uint64_t *high);
locktable *syml_lookup_locktab(uint64_t class_id);
symentry * syml_kernext_lookup(uint64_t value);
#endif /* _SYMLIB_H */
