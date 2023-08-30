/*
 * IBM Performance Inspector
 * Copyright (c) International Business Machines Corp., 1996 - 2009
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _A2NINT_H_
#define _A2NINT_H_


//
// Version number
// **************
//
#define V_MAJOR      3
#define V_MINOR      1
#define V_REV        51

#define VERSION_NUMBER   (V_MAJOR << 24) | (V_MINOR << 16) | V_REV




//
// Macros
// ******
//
#define PROCESS_IS_CLONE(pn) ((pn)->cloner != NULL)
#define PROCESS_HAS_MODULES(pn) ((pn)->modroot != NULL)
#define MODULE_HAS_SYMBOLS(mn) ((mn)->symcnt != 0)


//
// Common stuff
// ************
//
#define UNKNOWN_NAME                  "Unknown"
#define DEFAULT_MMI_RANGE_NAME        "MMI"
#define MMI_RANGE_SYMSTART            "__JVM_CODE_LOWEST"
#define MMI_RANGE_SYMSTART_ALT        "_mmiInitializeExecuteJavaHandlerTable"
#define MMI_RANGE_SYMEND              "__JVM_CODE_HIGHEST"

#define DEFAULT_JVMMAP_NAME           "jvmmap"
#define DEFAULT_UC_JVMMAP_NAME        "jvmmap.X"
#define DEFAULT_BLANK_SUB             '-'

#define A2N_MESSAGE_FILE              "a2n.err"   // All messages go here
#define A2N_MODULE_DUMP_FILE          "a2n.mod"   // Filename for module dump
#define A2N_PROCESS_DUMP_FILE         "a2n.proc"  // Filename for process dump
#define A2N_MSI_DUMP_FILE             "a2n.msi"   // Filename for MSI dump

#define PID_HASH_TABLE_SIZE   127           // Default pid hash table size
#define SYMCACHE_SIZE          3            // Symbol cache array size

#define SNP_ELEMENTS       100000           // SN pointer array elements
#define SNP_SIZE           (SNP_ELEMENTS * sizeof(SYM_NODE *))

#define CR     '\r'                         // Carriage return
#define LF     '\n'                         // Linefeed/Newline
#define TAB    0x09                         // Tab
#define SP     ' '                          // Space/Blank


//
// Linux operating system and machine architecture-specific stuff
// **************************************************************
//
#if defined(_LINUX) || defined(_ZOS) || defined(_AIX)
  #define SYSTEM_PID   0

  #define COMPARE_NAME    strcmp            // Case sensitive compare
  #define GET_PID()       getpid()
  #define GET_TID()       getpid()

  #define DEFAULT_PATH_SEP_CHAR         '/'
  #define DEFAULT_PATH_SEP_STR          "/"
  #define DEFAULT_SRCH_PATH_SEP_CHAR    ':'
  #define DEFAULT_SRCH_PATH_SEP_STR     ":"
  #define DEFAULT_KERNEL_FILENAME       "vmlinux"
  #define DEFAULT_KERNEL_IMAGENAME      "/usr/src/linux/vmlinux"
  #define DEFAULT_KERNEL_MAPNAME        "/boot/System.map"
  #define DEFAULT_KSYMS_FILENAME        "/proc/ksyms"
  #define DEFAULT_KALLSYMS_FILENAME     "/proc/kallsyms"
  #define DEFAULT_MODULES_FILENAME      "/proc/modules"

  #define MAX_LOADABLE_SEGS             8
  #define LOADABLE_SEGS_ELEMENTS        MAX_LOADABLE_SEGS

  #define VDSO_SEGMENT_NAME             "[vdso]"
  #define FAKE_VDSO_MODULE_NAME         "vdso.so"

  #if defined (_X86)
    #define EXEC_IMAGE_BASE   0x08048000
    #define A2N_BIG_ENDIAN    0
    #define A2N_WORD_SIZE     32
    // define interpreter symbol range
    #define MMI_RANGE_MODNAME      "libjvm.so"
    #define MMI_RANGE_SYM_TRIES    1
    #define MMI_RANGE_SYM_POS      1        // only 1 "_" . start at +1
    // -------------------------------

  #elif defined (_AMD64)
    #define EXEC_IMAGE_BASE   0x0000000040000000
    #define A2N_BIG_ENDIAN    0
    #define A2N_WORD_SIZE     64

  #elif defined (_IA64)
    #define EXEC_IMAGE_BASE   0x4000000000000000
    #define A2N_BIG_ENDIAN    0
    #define A2N_WORD_SIZE     64

  #elif defined (_PPC) && defined (_32BIT)
    #define EXEC_IMAGE_BASE   0x01800000
    #define A2N_BIG_ENDIAN    1
    #define A2N_WORD_SIZE     32

  #elif defined (_PPC) && defined (_64BIT)
    #define EXEC_IMAGE_BASE   0x0000000010000000
    #define A2N_BIG_ENDIAN    1
    #define A2N_WORD_SIZE     64

  #elif defined (_S390) && defined (_32BIT)
    #define EXEC_IMAGE_BASE   0x00400000
    #define A2N_BIG_ENDIAN    1
    #define A2N_WORD_SIZE     32

  #elif defined (_S390) && defined (_64BIT)
    #define EXEC_IMAGE_BASE   0x0000000080000000
    #define A2N_BIG_ENDIAN    1
    #define A2N_WORD_SIZE     64

  #else
    #define EXEC_IMAGE_BASE   0xFFFFFFFF
    #define A2N_BIG_ENDIAN    0
    #define A2N_WORD_SIZE     32
  #endif

#endif // _LINUX


//
// Windows operating system and machine architecture-specific stuff
// ****************************************************************
//
#if defined (_WINDOWS)
  #define SYSTEM_PID   0
  #define MAX_DEVICES  26

  #define COMPARE_NAME    stricmp           // Case insensitive compare
  #define GET_PID()       GetCurrentProcessId()
  #define GET_TID()       GetCurrentThreadId()

  #define DEFAULT_PATH_SEP_CHAR         '\\'
  #define DEFAULT_PATH_SEP_STR          "\\"
  #define DEFAULT_SRCH_PATH_SEP_CHAR    ';'
  #define DEFAULT_SRCH_PATH_SEP_STR     ";"
  #define DEFAULT_KERNEL_FILENAME       "ntoskrnl.exe"
  #define DEFAULT_KERNEL_IMAGENAME      NULL
  #define DEFAULT_KERNEL_MAPNAME        NULL
  #define DEFAULT_KSYMS_FILENAME        NULL
  #define DEFAULT_KALLSYMS_FILENAME     NULL
  #define DEFAULT_MODULES_FILENAME      NULL

  #define MAX_LOADABLE_SEGS             2
  #define LOADABLE_SEGS_ELEMENTS        1

  #define A2N_BIG_ENDIAN      0

  #define DEFAULT_SPECIAL_DLLS1   "ADVAPI32.DLL;COMCTL32.DLL;GDI32.DLL;IMAGEHLP.DLL;"
  #define DEFAULT_SPECIAL_DLLS2   "KERNEL32.DLL;MSVCRT.DLL;NTDLL.DLL;OLE32.DLL;OLEAUT32.DLL;"
  #define DEFAULT_SPECIAL_DLLS3   "PSAPI.DLL;RPCRT4.DLL;SHELL32.DLL;USER32.DLL;"

  #define DEVICE_NETWORK_DRIVE   1
  #define DEVICE_LOCAL_DRIVE     2
  #define DEVICE_FLOPPY_DRIVE    3
  #define DEVICE_CD_DVD_DRIVE    4
  #define DEVICE_SUBST_DRIVE     5
  #define DEVICE_UNKNOWN_DRIVE   6

  #if defined (_X86)
    // SharedUserData page
    #define SHARED_USER_DATA_START         0x7FFE0000
    #define SHARED_USER_DATA_END           0x7FFE0FFF
    #define SHARED_USER_DATA_LENGTH        0x1000
    #define SHARED_USER_DATA_SYMBOL        "SharedUserData"
    #define SYSTEMCALL_STUB_START          0x7FFE0300
    #define SYSTEMCALL_STUB_END            0x7FFE031F
    #define SYSTEMCALL_STUB_LENGTH         0x20
    #define SYSTEMCALL_STUB_SYMBOL         "SystemCallStub"
    #define SHARED_USER_DATA_NO_SYMBOL     "SharedUserData_Unknown_Symbol"

    #define EXEC_IMAGE_BASE   0x00400000
    #define A2N_WORD_SIZE     32
    // define interpreter symbol range
    #define MMI_RANGE_MODNAME      "jvm.dll"
    #define MMI_RANGE_SYM_TRIES    2        // 1 or 2 "_"
    #define MMI_RANGE_SYM_POS      0        // 1 or 2 "_" . start at +0
    // -------------------------------

    #define SYSTEM_ROOT_DIR   "\\WINNT"
    #define WIN_SYS_DIR       "\\winnt\\"

    //
    // Leading character added to function names by the compiler
    // depending on the linkage convention used:
    // _ (underscode): for __cdecl and __stdcall linkage
    // @ (at sign):    for __fastcall linkage
    //
    #define LEAD_CHAR_US   '_'                // Leading char added by compiler
    #define LEAD_CHAR_AT   '@'                // Leading char added by compiler

  #elif defined (_IA64) || defined (_AMD64)
    #define EXEC_IMAGE_BASE   0x0000000000400000
    #define A2N_WORD_SIZE     64

    #define SYSTEM_ROOT_DIR   "\\WINDOWS"
    #define WIN_SYS_DIR       "\\windows\\"

    //
    // Leading character added to function names by the compiler.
    // The IA64 compiler always seems to add a '.' (period).
    //
    #define LEAD_CHAR_DOT  '.'                // Leading char added by compiler

    // _ (underscode): for __cdecl and __stdcall linkage
    // @ (at sign):    for __fastcall linkage
    #define LEAD_CHAR_US   '_'                // Leading char added by compiler
    #define LEAD_CHAR_AT   '@'                // Leading char added by compiler

  #else
    #define EXEC_IMAGE_BASE   0xFFFFFFFF
  #endif

#endif  // _WINDOWS



//
// Lockout for values set via environment variables.
// 0 means unlocked (API allowed), 1 means locked (API not allowed).
//
typedef struct _apilocks API_LOCKS;
struct _apilocks {
   int DebugMode;                      // A2nSetDebugMode()
   int ErrorMessageMode;               // A2nSetErrorMessageMode()
   int SymbolGatherMode;               // A2nSetSymbolGatherMode()
   int GatherCodeMode;                 // A2nSetCodeGatherMode()
   int ValidateSymbolsMode;            // A2nSetValidateKernelMode() and A2nSetSymbolValidationMode()
   int DemangleMode;                   // A2nSetDemangleCppNamesMode()
   int NoSymbolFoundAction;            // A2nSetNoSymbolFoundAction()
   int NoModuleFountThreshold;         // A2nSetNoModuleFoundDumpThreshold()
   int NoSymbolFountThreshold;         // A2nSetNoSymbolFoundDumpThreshold()
   int SymbolQuality;                  // A2nSetSymbolQualityMode()
   int ReturnRangeSymbolsMode;         // A2nSetReturnRangeSymbolsMode()
   int SetKernelImageName;             // A2nSetKernelImageName()
   int SetKernelMapName;               // A2nSetKernelMapName()
   int SetCollapseMMIRange;            // A2nSetCollapseMMIRange()
   int SetMMIRangeName;                // -- Name part of A2nSetCollapseMMIRange()
   int ReturnLineNumbersMode;          // A2nSetReturnLineNumbersMode()
   int RenameDuplicateSymbolsMode;     // A2nSetRenameDuplicateSymbolsMode()
   int SetKallsymsLocation;            // A2nSetKallsymsFileLocation()
   int SetModulesLocation;             // A2nSetModulesFileLocation()
};


//
// API counters
//
typedef struct _apicounts API_COUNTS;
struct _apicounts {
   int AddModule;                      // Total number of A2nAddModule() calls
   int AddModule_OK;                   // Number of successful A2nAddModule() calls
   int AddJittedMethod;                // Total number of A2nAddJittedMethod/Ex() calls
   int AddJittedMethod_OK;             // Number of successful A2nAddJittedMethod/Ex() calls
   int ForkProcess;                    // Total number of A2nForkProcess() calls
   int ForkProcess_OK;                 // Number of successful A2nForkProcess() calls
   int CloneProcess;                   // Total number of A2nCloneProcess() calls
   int CloneProcess_OK;                // Number of successful A2nCloneProcess() calls
   int CreateProcess;                  // Total number of A2nCreateProcess() calls
   int CreateProcess_OK;               // Number of successful A2nCreateProcess() calls
   int AddSymbol;                      // Total number of A2nAddSymbol() calls
   int AddSymbol_OK;                   // Number of successful A2nAddSymbol() calls
   int SetProcessName;                 // Total number of A2nSetProcessName() calls
   int SetProcessName_OK;              // Number of  successful A2nSetProcessName()
   int GetSymbol;                      // Total number of A2nGetSymbol() calls
   int GetSymbol_OKSymbol;             // Successful A2nGetSymbol() calls returning symbol
   int GetSymbol_OKJittedMethod;       // Successful A2nGetSymbol() calls returning jitted method
   int GetSymbol_NoModule;             // Number of A2nGetSymbol() calls that failed (No Module Found)
   int GetSymbol_NoSymbols;            // Number of A2nGetSymbol() calls that failed (No Symbols)
   int GetSymbol_NoSymbolsValFailed;   // Number of A2nGetSymbol() calls that failed (No Symbols - validation failed)
   int GetSymbol_NoSymbolsQuality;     // Number of A2nGetSymbol() calls that failed (No Symbols - low quality)
   int GetSymbol_NoSymbolFound;        // Number of A2nGetSymbol() calls that failed (No Symbol Found)
   int GetSymbol_FastpathOk;           // Number of A2nGetSymbol() resolved via fastpath
   int GetSymbol_FastpathMod;          // Number of A2nGetSymbol() where symbol in same module
   int GetSymbol_FastpathNone;         // Number of A2nGetSymbol() where symbol in different module
};




//
// All the different nodes
// ***********************
//
typedef struct pid_node PID_NODE;           // Process (PID) node
typedef struct lmod_node LMOD_NODE;         // Loaded Module node
typedef struct jm_node JM_NODE;             // Jitted Method node
typedef struct mod_node MOD_NODE;           // Module node
typedef struct sec_node SEC_NODE;           // Section node
typedef struct sym_node SYM_NODE;           // Symbol node
typedef struct range_node RANGE_NODE;       // Symbol Range node
typedef struct rangedesc_node RDESC_NODE;   // Range Descriptor node
typedef struct lastsym_node LASTSYM_NODE;   // Last Symbol node
typedef struct srcline_node SRCLINE_NODE;   // Source line descriptor node
typedef struct srcfile_node SRCFILE_NODE;   // Source filename node
typedef struct line_info LINE_INFO;



//
// Definition for node flags field in all nodes.
// ---------------------------------------------
// * High order byte is the node signature (or node type)
// * Low order 24 bits are used for flags
//
#define NODE_SIGNATURE_MASK                 0xFF000000
#define A2N_FLAGS_PID_NODE_SIGNATURE        0x50000000  // "P" PID node
#define A2N_FLAGS_LMOD_NODE_SIGNATURE       0x4C000000  // "L" LMOD node
#define A2N_FLAGS_JM_NODE_SIGNATURE         0x4A000000  // "J" JM node
#define A2N_FLAGS_MOD_NODE_SIGNATURE        0x4D000000  // "M" MOD node
#define A2N_FLAGS_SEC_NODE_SIGNATURE        0x43000000  // "C" SEC node
#define A2N_FLAGS_SYM_NODE_SIGNATURE        0x53000000  // "S" SYM node

#define NODE_FLAGS_MASK                     0x00FFFFFF
#define A2N_FLAGS_REQUESTED                 0x00000001  // Node (whatever type) was requested
#define A2N_FLAGS_INHERITED                 0x00000002  // Module was inherited
#define A2N_FLAGS_KERNEL                    0x00000004  // Module/Loaded Module is the kernel
#define A2N_SYMBOL_ARRAY_BUILT              0x00000008  // Symbol array built for module

#define A2N_FLAGS_SYMBOLS_HARVESTED         0x00000010  // Have tried to harvest symbols
#define A2N_FLAGS_VALIDATION_FAILED         0x00000020  // Module validation failed. Symbols kept.
#define A2N_FLAGS_VALIDATION_NOT_DONE       0x00000040  // Module validation not done - errors. Symbols kept.
#define A2N_FLAGS_SYMBOLS_NOT_FROM_IMAGE    0x00000080  // Symbols not obtained from on-disk image

#define A2N_FLAGS_SYMBOL_CONTAINED          0x00000100  // Symbol contained in another symbol
#define A2N_FLAGS_SYMBOL_ALIASED            0x00000200  // Symbol aliased to another symbol
#define A2N_FLAGS_SYMBOL_LOW_QUALITY        0x00000400  // Low quality symbol
#define A2N_FLAGS_COFF_SYMBOLS              0x00000800  // Symbols from image and COFF

#define A2N_FLAGS_CV_SYMBOLS                0x00001000  // Symbols from image and CV
#define A2N_FLAGS_EXPORT_SYMBOLS            0x00002000  // Symbols from image and EXPORTS
#define A2N_FLAGS_BLANKS_CHANGED            0x00004000  // Blanks in symbol/module name already substituted
#define A2N_FLAGS_SYMBOL_COLLAPSED          0x00008000  // Symbol name was collapsed

#define A2N_FLAGS_ILT_SYMBOLS_ONLY          0x00010000  // Only symbol(s) is(are) ILT(s) (WIN64 only)
#define A2N_FLAGS_PLT_SYMBOLS_ONLY          0x00020000  // Only symbol is PLT (so far) (Linux only)
#define A2N_FLAGS_PID_REUSED                0x00040000  // PID node being re-used
#define A2N_FLAGS_NO_LINENO                 0x00080000  // Module has no line numbers

#define A2N_FLAGS_32BIT_MODULE              0x00100000  // Module/Image is 32-bit
#define A2N_FLAGS_64BIT_MODULE              0x00200000  // Module/Image is 64-bit
#define A2N_FLAGS_MMI_METHOD                0x00400000  // JVM dynamically generated code (?? why MMI ??)
#define A2N_FLAGS_VDSO                      0x00800000  // [vdso] pseudo-module

#define A2N_FLAGS_32BIT_PROCESS             A2N_FLAGS_32BIT_MODULE   // 32-bit process
#define A2N_FLAGS_64BIT_PROCESS             A2N_FLAGS_64BIT_MODULE   // 64-bit process



//
// Process (PID) node
// ******************
//
// * One per process
// * Created when:
//   - First module node is added
//   - Clone/ForkProcess
//   - SetProcessName
// * List anchored off global variable PidRoot
// * Highest level node.  All other nodes hang off this one.
//
// A process can fork and clone other processes.
// * "forked" is set if the process has forked others
// * "cloned" is set if the process has cloned others
//
// A process can itself be a clone or have been forked by another process.
// * "forker" is set if this process was forked
// * "cloner" is set if this process was cloned
//
// A process can't be a clone and have been forked.
// * Either "forker" or "cloner" can be set (or neither if this
//   process was not forked nor cloned).
//
// Only processes that are not clones can have modules.
// * "modroot" can only be set if "cloner" is not set.
//
// * "lmcnt" is the total number of modules loaded on the pid,
//   including jitted methods.
//   - Not used on cloned pids.  For cloned pids all modules are
//     loaded on the "rootcloner" (or "owning") pid.
//
// * "imcnt" is the total number of modules inherited on a fork.
//   - It reflects *ALL* inherited modules: jitted or not.
//   - It is initially set to the forker's "lmcnt" and never updated.
//
// * "jmcnt" is the total number of jitted method modules loaded
//   on the pid. It is a subset of "lmcnt".
//
// * "forker" points to the process that forked this one.
//   If set it means this process was forked.  At the time of the
//   fork the module list of the forker was copied to this one.
//   From this point on modules added to the forked process are
//   visible only to it, not to the forker.
//
// * "cloner" points to the process that cloned this one.
//   If set it means this process was cloned.  A cloned process
//   can't have modules so if modules are added to this process
//   we must follow the cloner pointer all the way back to the
//   process that cloned us all.  It's not enough to go back to the
//   cloner because it could also be a clone.  All clones from the
//   the same original cloner share its modules.
//
// * "forked" points to the first process forked by this one.
// * "fnext" links to the next process with the same forker.
//   These two pointers are used to anchor and link the list of
//   processes forked by this one.  They are only used to maintain
//   the forker/forkee relationship.
//
// * "cloned" points to the first process cloned by this one
// * "cnext" links to the next process with the same cloner
//   These two pointers are used to anchor and link the list of
//   processes cloned by this one.  They are used to find the
//   original cloner in a cloned chain because it is the only
//   process that owns modules.
//
struct pid_node {
   PID_NODE * next;                         // Next node in pid list
   PID_NODE * hash_next;                    // Next hash table element node
   LMOD_NODE * lmroot;                      // Start of loaded module list
   LASTSYM_NODE * lastsym;                  // Info about last symbols returned
   char * name;                             // Process name (if known)

   PID_NODE * rootcloner;                   // Process that cloned this chain
   PID_NODE * cloner;                       // Process that cloned this process (parent)
   PID_NODE * cnext;                        // Next process cloned by "cloner" (sibling)
   PID_NODE * cloned;                       // 1st process cloned by this one (children)
   PID_NODE * cend;                         // Last process cloned by this one

   PID_NODE * forker;                       // Process that forked this process (parent)
   PID_NODE * fnext;                        // Next process forked by "forker" (sibling)
   PID_NODE * forked;                       // 1st process forked by this one (children)
   PID_NODE * fend;                         // Last process forked by this one

   uint pid;                                // Process id
   int cloned_cnt;                          // Number of processess cloned by this one
   int forked_cnt;                          // Number of processess forked by this one

   int lmcnt;                               // Total number of modules loaded (including jitted methods)
   int jmcnt;                               // Number of jitted method modules
   int imcnt;                               // Number of inherited modules (set only at inherit time)
   int ijmcnt;                              // Number of inherited jitted methods (set only at inherit time)
   uint flags;                              // Node signature and flags

   int lmerr_cnt;                           // Number of loaded modules requests not found on this pid
   int kernel_req;                          // Number of symbol requested that were in the kernel
   int lmreq_cnt;                           // Number of loaded modules requests on this pid
};


//
// Loaded Module node
// ******************
//
// Used to maintain module/jitted method load order for symbol resolution.
// A Loaded Module node is basically just a pointer to the real module
// node in the Module Cache (for modules) or to a Jitted Method node
// in the Jitted Method pool (for jitted methods).
//
// * One per loaded module/jitted method per process
// * A loaded module (at least in Linux) can have up to 2 loadable segments
// * Cloned processes can't have modules
// * List anchored off process node
// * Actual symbols are maintained in the module cache for modules
//   or in the jitted method node for jitted methods.
//
struct lmod_node {
   LMOD_NODE * next;                        // Next node in loaded module list
   MOD_NODE * mn;                           // Module node in cache
   JM_NODE * jmn;                           // Jitted method node
#if defined(_32BIT)
   uint qwalign;                            // Maintain QWORD alignment
#endif
   uint64 start_addr;                       // Starting (load) virtual address of first segment
   uint64 end_addr;                         // Ending virtual address of first segment
   uint64 seg_start_addr[LOADABLE_SEGS_ELEMENTS]; // Starting virtual for segment numbers >= 2 (if any)
   uint64 seg_end_addr[LOADABLE_SEGS_ELEMENTS];   // Ending virtual for segment numbers >= 2 (if any)
   uint seg_length[LOADABLE_SEGS_ELEMENTS]; // Length (in bytes) of segment numbers >= 2 (if any)
   uint length;                             // Length (in bytes) of entire module
   uint total_len;                          // Length (in bytes) start of seg1 to end of seg2
   int type;                                // Either a module or a jitted method
   uint pid;                                // Pid to which this module was added
   uint flags;                              // Node signature and flags
   int seg_cnt;                             // Number of loaded segments (0=only 1 segment)
   int req_cnt;                             // Number of times requested
};

#define LMOD_TYPE_INVALID             0     // Invalid
#define LMOD_TYPE_MODULE              1     // A real module
#define LMOD_TYPE_JITTED_CODE         2     // Java Jitted Method
#define LMOD_TYPE_ANON                3     // Anonymous segment


//
// Jitted Method node
// ******************
//
// There's one node per jitted method added.
// All the information needed for a jitted method is maintained here.
// Load order is maintained by the corresponding loaded module node.
//
struct jm_node {
   JM_NODE * next;                          // Next node in jitted method pool
   char * name;                             // Method name
   char * code;                             // Code associated with method (or NULL)
   JTLINENUMBER * jln;                      // Array of JTLINENUMBER structures
   uint length;                             // Method length (in bytes)
   int jln_cnt;                             // Number of JTLINENUMBER structures
   uint flags;                              // Node signature and flags
   int req_cnt;                             // Number of times requested
};


//
// Bookkeeping stuff for source line numbers
//
struct line_info {
#if defined(_LINUX)
   bfd     * abfd;                          // BFD for this module
   asymbol ** asym;                         // BFD-type symbols
   int     asymcnt;                         // BFD-type symbol count

#elif defined(_WINDOWS)
   SRCLINE_NODE * srclines_list;
   SRCLINE_NODE ** srclines;                // Start of LINE_NODE array
   SRCFILE_NODE * srcfiles;                 // Start of SRCFILE_NODE list
   int       linecnt;                       // Total LINE_NODE array elements
   int       srccnt;                        // Number of SRCFILE_NODE list elements

#else
   int       dummy;
#endif
};


#if defined(_ZOS)
typedef _Packed struct bbnl_h       BBNL_HEADER;
typedef _Packed struct besd_h       BESD_HEADER;
typedef _Packed struct txt_header   TXT_HEADER;
#endif


//
// Module node
// ***********
//
// There's one (and only one) node per unique module.
// A module is unique if its name, timestamp and checksum are unique.
// Module nodes are maintained in the module cache and are the nodes which
// own symbols and sections.  That way we only have one copy of the symbols
// for each loaded module, regardless of the number of processes the module
// is mapped to.
// "type" indicates the actual type of file from which symbols were
// harvested.  For example, if symbols for an executable are harvested
// from a .map file then the type will be set to MAP, *NOT* executable.
//
struct mod_node {
   MOD_NODE * next;                         // Next node in module cache
   LMOD_NODE * last_lmn;                    // LMN of last loaded module that referenced this MN
   SYM_NODE ** symroot;                     // Start of SYM_NODE list or SYMBOL ARRAY
   SEC_NODE * secroot;                      // Start of executable SEC_NODE list
   SEC_NODE * secroot_end;                  // End of executable SEC_NODE list
   SEC_NODE * ne_secroot;                   // Start of non-executable SEC_NODE list
   SEC_NODE * ne_secroot_end;               // End of non-executable SEC_NODE list
   SYM_NODE * symcache[SYMCACHE_SIZE];      // Symbol cache array

   char * name;                             // Fully qualified name (fixed by FixupModuleName)
   char * hvname;                           // Fully qualified name (from where symbols harvested)
   char * orgname;                          // Fully qualified name (from A2nAddModule)

   LINE_INFO li;                            // src line info

   uint ts;                                 // Timestamp (if available)
   uint chksum;                             // Checksum (if available)
   uint name_hash;                          // Hash value for name

   int type;                                // Module type (of file from where symbols are harvested)
   int symcnt;                              // Total number of symbols
   int base_symcnt;                         // Number of non-alised/non-contained symbols
   int seccnt;                              // Total number of sections
   int ne_seccnt;                           // Number of non-executable sections
   int codesize;                            // Total amount of code harvested
   int strsize;                             // Total size of all strings associated with this module
   int cachecnt;                            // Number cache array entries in use
   uint flags;                              // Node signature and flags
   int req_cnt;                             // Number of times requested
   int symcachehits;                        // Number of times symbol found in cache
   int nosym_rc;                            // Reason for no symbols

#if defined(_ZOS)
   BBNL_HEADER * PTR32 bnlHeader;           // binder name List header
   BESD_HEADER * PTR32 esdHeaderB;          // binder ESD Header B_TEXT
   BESD_HEADER * PTR32 esdHeaderC;          // binder ESD Header C_CODE
   TXT_HEADER  * PTR32 txtHeaderB;          // BTXT record Header B_TEXT
   TXT_HEADER  * PTR32 txtHeaderC;          // BTXT record Header C_CODE
#endif
};


//
// Module Types
// ------------
//                                          AIX  Linux  Win
//                                          ---  -----  ---
#define MODULE_TYPE_INVALID          -1  //        Y    Y   Invalid
#define MODULE_TYPE_UNKNOWN          -1  //        Y    Y   Unknown type
#define MODULE_TYPE_DOS               1  //             Y   (.COM) DOS executable
#define MODULE_TYPE_PE                2  //             Y   (.EXE/.DLL/.SYS) NT executable (PE)
#define MODULE_TYPE_LX                3  //             Y   (.EXE/.DLL/.SYS) OS/2 executable (LX)
#define MODULE_TYPE_VXD               4  //             Y   (.VXD) Win9x VXD
#define MODULE_TYPE_DBG               5  //             Y   (.DBG) DBG file
#define MODULE_TYPE_SYM               6  //             Y   (.SYM) SYM file
#define MODULE_TYPE_ELF_REL           7  //        Y        (.so) ELF32 relocatable
#define MODULE_TYPE_ELF_EXEC          8  //        Y        ELF32 executable
#define MODULE_TYPE_MAP_VC            9  //             Y   (.MAP) Visual C++ map file
#define MODULE_TYPE_MAP_VA            10 //             Y   (.MAP) VisualAge C++ map file
#define MODULE_TYPE_MAP_NM            11 //        Y        (.MAP) Linux map file generated via "nm -an"
#define MODULE_TYPE_MAP_OBJDUMP       12 //        Y        (.MAP) Linux map file generated via "objdump -t"
#define MODULE_TYPE_MAP_READELF       13 //        Y        (.MAP) Linux map file generated via "readelf -s"
#define MODULE_TYPE_JITTED_CODE       14 //        Y    Y   Java Jitted Method
#define MODULE_TYPE_MYS               15 //             Y   (.MYS) MYS file (Riaz's format)
#define MODULE_TYPE_PDB               16 //             Y   (.PDB) MS Program Data Base
#define MODULE_TYPE_LE                17 //             Y   (.EXE/.DLL/.SYS) OS/2 (LE) or Win VXD
#define MODULE_TYPE_NE                18 //             Y   (.EXE/.DLL/.SYS) OS/2 (NE)
#define MODULE_TYPE_ELF64_REL         19 //        Y        (.so) ELF64 relocatable
#define MODULE_TYPE_ELF64_EXEC        20 //        Y        ELF64 executable
#define MODULE_TYPE_DBGPDB            21 //             Y   (.DBG/.PDB) Combination
#define MODULE_TYPE_MAP_JVMX          22 //             Y   (.jmap) Map extracted from jvmmap.X
#define MODULE_TYPE_MAP_JVMC          23 //             Y   Compressed jvmmap file
#define MODULE_TYPE_XCOFF32           24 //  Y              XCOFF32
#define MODULE_TYPE_XCOFF64           25 //  Y              XCOFF64
#define MODULE_TYPE_KALLSYMS_MODULE   26 //        Y        Kernel Module (from /proc/kallsyms)
#define MODULE_TYPE_KALLSYMS_VMLINUX  27 //        Y        vmlinux (from /proc/kallsyms)
#define MODULE_TYPE_VSYSCALL32        28 //        Y        Fake vsyscall-32 module
#define MODULE_TYPE_VSYSCALL64        29 //        Y        Fake vsyscall-64 module
#define MODULE_TYPE_ANON              30 //        Y        Anonymous segment
#define MODULE_TYPE_VDSO              31 //        Y        VDSO

// Pseudo types
#define MODULE_TYPE_ELF_REL_FIXED     -1 //      Y        .so that looks like an image
#define MODULE_TYPE_ELF64_REL_FIXED   -1 //      Y        .so that looks like an image

// Reason for not having symbols
#define NOSYM_REASON_IMAGE_NO_ACCESS                              1
#define NOSYM_REASON_IMAGE_CANT_MAP                               2
#define NOSYM_REASON_DOS_HEADER_NOT_READABLE                      3
#define NOSYM_REASON_IMAGE_NOT_PE                                 4
#define NOSYM_REASON_NOT_OK_TO_USE                                5
#define NOSYM_REASON_IMAGE_HAS_NO_SECTIONS                        6
#define NOSYM_REASON_EXPORTS_WANTED_BUT_NO_EXPORT_DIRECTORY       7
#define NOSYM_REASON_EXPORTS_WANTED_BUT_NO_EXPORTS                8
#define NOSYM_REASON_NO_SYMBOLS_AND_EXPORTS_NOT_WANTED            9
#define NOSYM_REASON_NO_SYMBOLS_EXPORTS_WANTED_BUT_NO_EXPORTS     10
#define NOSYM_REASON_MAP_CANT_MAP                                 11
#define NOSYM_REASON_IMAGE_NAME_MAP_MISMATCH                      12
#define NOSYM_REASON_MAP_TS_ZERO                                  13
#define NOSYM_REASON_MAP_IMAGE_TS_MISMATCH                        14
#define NOSYM_REASON_MAP_NO_SECTION_TABLE                         15
#define NOSYM_REASON_MAP_SECTION_DATA_MISMATCH                    16
#define NOSYM_REASON_DBGHELP_SYMINITIALIZE_ERROR                  17
#define NOSYM_REASON_DBGHELP_SYMLOADMODULE64_ERROR                18
#define NOSYM_REASON_DBGHELP_SYMGETMODULEINFO64_ERROR             19
#define NOSYM_REASON_NO_SYMBOLS                                   20
#define NOSYM_REASON_ONLY_EXPORTS_BUT_NOT_WANTED                  21
#define NOSYM_REASON_MDD_TS_AND_PDBSIG_MISMATCH                   22
#define NOSYM_REASON_MISMATCH_BUT_NO_CVH_NOR_MDD                  23
#define NOSYM_REASON_NB10_CVH_PDB_SIGNATURE_MISMATCH              24
#define NOSYM_REASON_RSDS_CVH_PDB_SIGNATURE_MISMATCH              25
#define NOSYM_REASON_UNKNOWN_PDB_FORMAT                           26
#define NOSYM_REASON_UNKNOWN_SYMBOL_FORMAT                        27
#define NOSYM_REASON_DBGHELP_MISSING_SYMENUMSYMBOLS               28


//
// Section node
// ************
//
// * One per executable section/segment
// * List anchored off module node
//
// Notes on code:
// - If loc_addr is set then we have "size" bytes of code in memory.
//
struct sec_node {
   SEC_NODE * next;                         // Next node in section list
   char * name;                             // Section name
   char * alt_name;                         // Section name (when used as symbol name)
   void * code;                             // Local addr of code associated with section
   void * asec;                             // BFD asection pointer
#if defined(_32BIT)
   uint qwalign;                            // Maintain QWORD alignment
#endif
   uint64 start_addr;                       // Section starting virtual address/offset
   uint64 end_addr;                         // Section ending address/offset

   int  number;                             // Section number in module
   uint offset_start;                       // Starting offset
   uint offset_end;                         // Ending offset
   uint size;                               // Section size (in bytes)
   uint sec_flags;                          // Section flags
   uint flags;                              // Node signature and flags
};


//
// Symbol node
// ***********
//
// * One per symbol
// * List anchored off module node
//
// Notes on code:
// * If symbol is harvested the code is kept in the appropriate section.
// * If symbol is SYMBOL_TYPE_USER_SUPPLIED the code is kept with the symbol.
//
struct sym_node {
   SYM_NODE * next;                         // Next node in symbol/alias/contained lists
   SYM_NODE * aliases;                      // Aliases (same offset as "parent" symbol)
   SYM_NODE * contained;                    // Symbols contained within "parent" symbol
   SYM_NODE * alias_end;                    // Last alias symbol node
   SYM_NODE * contained_end;                // Last contained symbol node
   RANGE_NODE * rn;                         // Range node (or NULL if not in a range)
   SEC_NODE * cn;                           // Section node
   char * name;                             // Symbol name
   char * code;                             // Code associated with this symbol
#if defined(_WINDOWS)
   int  line_start;                         // First SRCLINE_NODE for this symbol
   int  line_end;                           // Last SRCLINE_NODE for this symbol
   int  line_cnt;
#endif
   uint offset_start;                       // Offset from beginning of module
   uint offset_end;                         // Offset of end of symbol
   uint length;                             // Length (in bytes)
   uint type;                               // Symbol type
   uint sym_flags;                          // Symbol flags
   int section;                             // Section number
   uint name_hash;                          // Hash value for name
   int index;                               // Index in symbol array
   int alias_cnt;                           // Number of aliases
   int sub_cnt;                             // Number of contained
   uint flags;                              // Node signature and flags
   int req_cnt;                             // Number of times requested
};

//
// Symbol Types
// ------------
//                                            Linux  Win
//                                            -----  ---
#define SYMBOL_TYPE_INVALID         -1      //  Y     Y   Invalid
#define SYMBOL_TYPE_UNKNOWN         -1      //  Y     Y   Unknown type
#define SYMBOL_TYPE_NONE             0      //  Y     Y   Nothing
#define SYMBOL_TYPE_USER_SUPPLIED    1      //  Y     Y   Added via A2nAddSymbol()
#define SYMBOL_TYPE_JITTED_METHOD    2      //  Y     Y   Jitted method - Added via A2nAddJittedMethod()
#define SYMBOL_TYPE_FUNCTION         3      //  Y     Y   Function
#define SYMBOL_TYPE_LABEL            4      //  Y     Y   Label
#define SYMBOL_TYPE_FUNCTION_WEAK    5      //  Y         Weak Function
#define SYMBOL_TYPE_ELF_PLT          6      //  Y         Elf "Procedure Linkage Table"
#define SYMBOL_TYPE_EXPORT           7      //        Y   Exported symbol
#define SYMBOL_TYPE_FORWARDER        8      //        Y   Forwarder (stub). Implies Exported.
#define SYMBOL_TYPE_OBJECT           9      //  Y         Data object (variable, array, etc)
#define SYMBOL_TYPE_SECTION          10     //  Y         Module section
#define SYMBOL_TYPE_GENERATED        15     //  Y     Y   Generated by A2N
#define SYMBOL_TYPE_MS_PDB           16     //        Y   From DghHelp - .PDB
#define SYMBOL_TYPE_MS_COFF          17     //        Y   From DghHelp - COFF
#define SYMBOL_TYPE_MS_CODEVIEW      18     //        Y   From DghHelp - CodeView
#define SYMBOL_TYPE_MS_SYM           19     //        Y   From DghHelp - .SYM
#define SYMBOL_TYPE_MS_EXPORT        20     //        Y   From DghHelp - Exports
#define SYMBOL_TYPE_MS_OTHER         21     //        Y   From DghHelp - Other
#define SYMBOL_TYPE_MS_DIA           22     //        Y   From DghHelp - .PDB/DIA (whatever that is)
#define SYMBOL_TYPE_PE_ILT           23     //        Y   Pseudo-symbol for ILT (IA64)

//
// Symbol Flags
// ------------
//
#define SYMBOL_FLAGS_NONE             0               // Nothing
#define SYMBOL_FLAGS_ELF              0x00000001      // Debug symbols from ELF executable
#define SYMBOL_FLAGS_ELF_EXPORT       0x00000002      // Exports from ELF executable
#define SYMBOL_FLAGS_PE_COFF          0x00000004      // COFF symbols from PE image
#define SYMBOL_FLAGS_PE_CODEVIEW      0x00000008      // CodeView symbols from PE image
#define SYMBOL_FLAGS_PE_EXPORT        0x00000010      // Exports from PE image
#define SYMBOL_FLAGS_FILE_DBG         0x00000020      // Symbols from DBG file
#define SYMBOL_FLAGS_FILE_MAP         0x00000040      // Symbols from MAP file
#define SYMBOL_FLAGS_FILE_SYM         0x00000080      // Symbols from SYM file
#define SYMBOL_FLAGS_FILE_MYS         0x00000100      // Symbols from MYS file
#define SYMBOL_FLAGS_MS_PDB           0x00000200      // Symbols from MS engine - .PDB
#define SYMBOL_FLAGS_MS_COFF          0x00000400      // Symbols from MS engine - COFF
#define SYMBOL_FLAGS_MS_CODEVIEW      0x00000800      // Symbols from MS engine - CodeView
#define SYMBOL_FLAGS_MS_SYM           0x00001000      // Symbols from MS engine - .SYM
#define SYMBOL_FLAGS_MS_EXPORT        0x00002000      // Symbols from MS engine - Exports
#define SYMBOL_FLAGS_MS_OTHER         0x00004000      // Symbols from MS engine - Other
#define SYMBOL_FLAGS_MS_DIA           0x00008000      // Symbols from MS engine - .PDB/DIA
#define SYMBOL_FLAGS_GUESS_LABEL      0x00010000      // A2N thinks this is a label


//
// Symbol Range node.
// ******************
//
// * One per collapsed symbol range
// * Each collapsed symbol in the range points to the same RANGE_NODE
// * Only allocated if the symbol is collapased
//
struct range_node {
   char * name;                             // Symbol name to be returned
   char * code;                             // Code associated collapsed symbol range
   uint offset_start;                       // Offset from beginning of module
   uint offset_end;                         // Offset of end of symbol
   uint length;                             // Length (in bytes)
   int section;                             // Section number
   uint name_hash;                          // Hash value for name
   int ref_cnt;                             // Number SYM_NODES linking to this node
   int req_cnt;                             // Number of times requested
};


//
// Last Symbol
// ***********
//
// Data for Last Symbol returned on the PID.
// It is basically an extended SYMDATA structure and if filled in
// A2nGetSymbol(). Same rules apply.
//
struct lastsym_node {
   PID_NODE  * opn;                         // Owning Pid node
   LMOD_NODE * lmn;                         // Loaded Module node
   SEC_NODE  * cn;                          // Section node
   SYM_NODE  * sn;                          // Symbol node
   void      * sym_code;                    // Code at start of symbol
   int       valid;                         // 1=valid, 0=invalid
   int       range;                         // 1=last symbol was inside a range
   int       rc;                            // Last rc returned
#if defined(_64BIT)
   uint qwalign;                            // Maintain QWORD alignment
#endif
   SYMDATA   sd;                            // Copy of last SYMDATA structure returned
};


//
// Range Descriptor
// ****************
//
struct rangedesc_node {
   char * filename;                         // Or "ENV" or "API"
   char * mod_name;                         // Module containing range
   char * sym_start;                        // First symbol in range
   char * sym_end;                          // Last symbol in range
   char * range_name;                       // Name of range
   RDESC_NODE * next;
};


//
// Source Filename Descriptor
// **************************
//
struct srcfile_node {
   char * fn;                               // Source file name
   SRCFILE_NODE * next;                     // Next node
};


//
// Source Line Descriptor
// **********************
//
struct srcline_node {
   SRCLINE_NODE * next;                     // Next alias
   SRCFILE_NODE * sfn;                      // Source file name
   SEC_NODE     * section;                  // Section node
   int  lineno;                             // Line number (in source file)
   uint offset_start;                       // Module offset corresponding to this line
   uint offset_end;
};



//
// Process-related function prototypes
// ***********************************
//
uint hash_pid(uint pid);

void pid_hash_add(PID_NODE * pn);

#if defined(_WINDOWS) || defined(_LINUX)
   void enter_critical_section(void);
   void exit_critical_section(void);
#else
   #define enter_critical_section()
   #define exit_critical_section()
#endif

PID_NODE * CreateProcessNode(uint pid);

PID_NODE * CreateNewProcess(uint pid);

PID_NODE * CloneProcessNode(uint cloner_pid, uint cloned_pid);

PID_NODE * ForkProcessNode(uint forker_pid, uint forked_pid);

PID_NODE * GetProcessNode(uint pid);

PID_NODE * GetOwningProcessNode(PID_NODE * pn);

PID_NODE * GetParentProcessNode(PID_NODE * pn);

PID_NODE * AllocProcessNode(void);

void FreeProcessNode(PID_NODE * pn);

LASTSYM_NODE * AllocLastSymbolNode(void);

void FreeLastSymbolNode(LASTSYM_NODE * lsn);

RANGE_NODE * AllocRangeNode(void);

void FreeRangeNode(RANGE_NODE * rn);



//
// Loaded Module-related function prototypes
// *****************************************
//
LMOD_NODE * AddLoadedModuleNode(PID_NODE * pn,
                                char * name,
                                uint64 base_addr,
                                uint length,
                                uint ts,
                                uint chksum,
                                int jitted_method);

LMOD_NODE * GetMatchingLoadedModuleNode(PID_NODE * pn,
                                        char * name,
                                        uint64 base_addr,
                                        uint length);

LMOD_NODE * GetLoadedModuleNode(PID_NODE * pn, uint64 addr);

LMOD_NODE * GetLoadedModuleNodeForModule(PID_NODE * pn, MOD_NODE * mn);

LMOD_NODE * CopyLoadedModuleList(LMOD_NODE * root);

void FreeLoadedModuleList(LMOD_NODE * root);

LMOD_NODE * AllocLoadedModuleNode(void);

void FreeLoadedModuleNode(LMOD_NODE * lmn);

void HarvestSymbols(MOD_NODE * mn, LMOD_NODE * lmn);

char * GetLoadedModuleName(LMOD_NODE * lmn);

int LoadMap(uint pid, int loadAnon);


//
// Jitted methods-related function prototypes
// ******************************************
//
JM_NODE * AddJittedMethodNode(LMOD_NODE * lmn,
                              char * name,
                              uint length,
                              char * code,
                              int jln_type,
                              int jln_cnt,
                              void * jln);

JM_NODE * AllocJittedMethodNode(void);

void FreeJittedMethodNode(JM_NODE * jmn);

char * CopyCode(char * from, uint length);



//
// Module cache-related function prototypes
// ****************************************
//
void AddModuleNodeToCache(LMOD_NODE * lmn,
                          char * name,
                          uint ts,
                          uint chksum);

MOD_NODE * GetModuleNodeFromCache(char * name,
                                  uint hv,
                                  uint ts,
                                  uint chksum);

MOD_NODE * GetModuleNodeFromModuleFilename(char * filename, MOD_NODE * start_mn);

int ModuleFilenameMatches(MOD_NODE * mn, char * filename);

#if defined(_WINDOWS)
void FixupModuleName(LMOD_NODE * lmn, char * name, char * fixed_name);
#else
#define FixupModuleName(lmn, nane, fixed_name)  strcpy(fixed_name, name)
#endif

int ModulesAreTheSame(MOD_NODE * mn,
                      char * name,
                      uint hv,
                      uint ts,
                      uint chksum);

MOD_NODE * AllocModuleNode(void);

void FreeModuleNode(MOD_NODE * mn);



//
// Symbol-related function prototypes
// **********************************
//
void ChangeBlanksToSomethingElse(char * s);

int SymbolCallBack(SYMBOL_REC * sr);

SYM_NODE * AddSymbolNode(MOD_NODE * mn,
                         char * name,
                         uint offset,
                         uint length,
                         int section,
                         int type,
                         char * code);

SYM_NODE * GetSymbolNode(LMOD_NODE * lmn, uint64 addr);

SYM_NODE * GetSymbolNodeFromName(MOD_NODE * mn, char * name);

SYM_NODE * GetSubsymbolNode(LMOD_NODE * lmn, SYM_NODE * sn, uint64 addr);

void GetLineNumber(LMOD_NODE * lmn, SYM_NODE * sn, uint64 addr, char ** srcfn, int * srcline);

LINEDATA * GetLineNumbersForSymbol(LMOD_NODE * lmn, SYM_NODE * sn);

void FixupSymbols(MOD_NODE * mn);

void RenameDuplicateSymbols(MOD_NODE * mn);

uint CalculateLabelLength(MOD_NODE * mn, SYM_NODE * ln, SYM_NODE * nn);

void AliasNextToCurrent(SYM_NODE * ns, SYM_NODE * cs, MOD_NODE * mn);

void AliasCurrentToNext(SYM_NODE * cs, SYM_NODE * ns, MOD_NODE * mn);

void MoveToContained(SYM_NODE * cs, SYM_NODE * ps, MOD_NODE * mn);

void MoveToContained(SYM_NODE * contained, SYM_NODE * parent, MOD_NODE * mn);

SYM_NODE  *AllocSymbolNode(void);

void FreeSymbolNode(SYM_NODE * sn);

SYM_NODE * FindInSymbolCache(MOD_NODE * mn, uint offset);

void AddToSymbolCache(MOD_NODE * mn, SYM_NODE * sn);

int ReadRangeDescriptorFile(char * fn);

int CollapseSymbolRange(char * module_fn, char * symbol_start, char * symbol_end, char * symbol_new);

void CollapseRangeIfApplicable(MOD_NODE * mn);

void CollapseMMIRange(MOD_NODE * mn);

RANGE_NODE * CollapseRangeInModule(MOD_NODE * mn, char * symbol_start, char * symbol_end, char * range_name);

void AddRangeDescNode(char * fn, char * module_fn, char * symbol_start, char * symbol_end, char * range_name);

RDESC_NODE * AllocRangeDescNode(void);

void FreeRangeDescNodeList(void);

SRCLINE_NODE * AllocSourceLineNode(void);

void FreeSourceLineNode(SRCLINE_NODE * sln);

SRCFILE_NODE * AllocSourceFileNode(void);

void FreeSourceFileNode(SRCFILE_NODE * sfn);


//
// Section-related function prototypes
// ***********************************
//
int SectionCallBack(SECTION_REC * sr);

SEC_NODE * AddSectionNode(MOD_NODE * mn,
                          int number,
                          void * asec,
                          int executable,
                          uint64 addr,
                          uint offset,
                          uint length,
                          uint flags,
                          char * name,
                          void * loc_addr);

SEC_NODE * GetSectionNode(LMOD_NODE * lmn, uint64 addr);

SEC_NODE * GetSectionNodeByNumber(MOD_NODE * mn, int number);

SEC_NODE * GetSectionNodeByName(MOD_NODE * mn, char * name);

SEC_NODE * GetSectionNodeByOffset(MOD_NODE * mn, uint offset);

SEC_NODE * AllocSectionNode(void);

void FreeSectionNode(SEC_NODE * sn);


//
// Miscellaneous other things
// **************************
//
void make_kernel_symbols_choice(void);

int check_if_good_filename(char * fn);

int verify_kernel_image_name(char * fn);

int verify_kernel_map_name(char * fn);

int verify_kallsyms_file_info(char * fn, int64_t offset, int64_t size);

int verify_modules_file_info(char * fn, int64_t offset, int64_t size);


#endif //_A2NINT_H_
