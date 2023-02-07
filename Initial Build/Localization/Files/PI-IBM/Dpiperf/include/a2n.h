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

#ifndef _A2N_H_
#define _A2N_H_

#if defined(_WINDOWS) || defined(WINDOWS) || defined(WIN32) || defined(_WIN32)
   #define ApiExport   __declspec(dllexport)
   #define ApiLinkage  __cdecl
#else
   #define ApiExport
   #define ApiLinkage
#endif


#if defined(_LINUX)
  #define NoExport __attribute__((visibility ("hidden")))
#else
  #define NoExport
#endif

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                              Data Structures                             //
//                              ***************                             //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

//
// SYMDATA: Symbol Data
// ********
//
// Filled in by A2N in response to an A2nGetSymbol() request.
// - All strings and code should be considered to be "read-only".
//   If you need to modify them, copy them and modify the copies.
// - Pointers to names (ex. mod_name) are never NULL (exception is
//   sym_name if requested). They always point to valid strings.
// - Code pointer (code) can be NULL if there is no code.
//
// How SYMDATA fields are set by A2nGetSymbol()
// ********************************************
//
//
//                                                    SYMDATA Structure Fields
//                     *************************************************************************************
//                     owning_
// rc/nsf_action       pid name(1)  mod_name mod_addr mod_len  sym_name      sym_addr    sym_len     code(2)
// -----------------   --- -------  -------- -------- -------  ------------- ----------- ----------  -------
// SUCCESS             Y   Y        Y        Y        Y        Y             Y           Y           Y
//
// NO_SYMBOL_FOUND
//   action: NSF       Y   Y        Y        Y        Y        NoSymbolFound mod_addr    mod_len     Y
//   action: SECTION   Y   Y        Y        Y        Y        sec_name(3)   sec_addr(4) sec_len(4)  Y
//   action: NOSYM     Y   Y        Y        Y        Y        NULL          mod_addr    mod_len     Y
//
// NO_SYMBOLS
//   action: NSF       Y   Y        Y        Y        Y        NoSymbols     mod_addr    mod_len     Y
//   action: SECTION   Y   Y        Y        Y        Y        sec_name(3)   sec_addr(4) sec_len(4)  Y
//   action: NOSYM     Y   Y        Y        Y        Y        NULL          mod_addr    mod_len     Y
//
// NO_MODULE           Y   Y        NoModule NULL     0        NoSymbols     NULL        0           NULL
//
// OTHER               (?) (?)      (?)      (?)      (?)      (?)           (?)         (?)         (?)
//
//
// (1) owing_pid_name can be: "Unknown", "name_of_1st_module_added" or name set via A2nSetProcessName()
// (2) Code stream at given address (if code harvested) or NULL otherwise
// (3) If section name not available then "NoSymbolFound" or "NoSymbols" is returned
// (4) Set if known, NULL/0 otherwise
// (?) May or may not be set and not guaranteed to be any good.
//
typedef struct symdata SYMDATA;
struct symdata {
   //
   // Process Information
   //
   char          * owning_pid_name;         // Name of "owning" process
   unsigned int  owning_pid;                // Process id of "owning" process

   //
   // Module Information
   //
   char          * mod_name;                // Name of module
   uint64        mod_addr;                  // Module starting (load) virtual address
   unsigned int  mod_length;                // Module length (in bytes)
   int           mod_req_cnt;

   //
   // Symbol Information
   //
   char          * sym_name;                // Symbol name
   uint64        sym_addr;                  // Symbol starting virtual address
   unsigned int  sym_length;                // Symbol length (in bytes)
   int           sym_req_cnt;

   //
   // Symbol Code Information
   //
   char          * code;                    // Code stream at given address
   char          * src_file;                // Source file
   int           src_lineno;                // Source line number

   //
   // Shortcuts (to be used someday)
   //
   void          * process;
   void          * module;
   void          * symbol;

   //
   // Flags
   //
   unsigned int  flags;

#define SDFLAG_MODULE_64BIT             0x00000001   // 64-bit ELF/PE module or jitted method
#define SDFLAG_MODULE_32BIT             0x00000002   // 32-bit ELF/PE module or jitted method
#define SDFLAG_SYMBOL_EXPORT            0x00000004   // Symbol came from exports
#define SDFLAG_SYMBOL_JITTED            0x00000008   // Symbol is jitted method
#define SDFLAG_SYMBOL_RANGE             0x00000010   // Symbol is a symbol range name or in a range
#define SDFLAG_SYMBOL_HAS_SUBSYMBOLS    0x00000020   // Symbol contains other symbols
#define SDFLAG_SYMBOL_HAS_ALIASES       0x00000040   // Symbol has other name(s)
#define SDFLAG_SYMBOL_MMI               0x00000080   // Symbol is dynamic code generated by the JVM
#define SDFLAG_SYMBOL_SYSCALLSTUB       0x00000100   // Symbol is SystemCallStub
#define SDFLAG_SYMBOL_IS_FAKE           0x00000200   // Symbol is a made-up one
#define SDFLAG_PROCESS_64BIT            0x00000400   // 64-bit process
#define SDFLAG_PROCESS_32BIT            0x00000800   // 32-bit process (can have 64-bit modules)

   //
   // Room to grow
   //
   int           spare[7];
};


//
// Special strings used for module and symbol names
//
#define A2N_JITTED_CODE_STR     "JITCODE"         // Returned as module name for jitted method
                                                  // symbols.
#define A2N_MMI_CODE_STR        "GENERATED_CODE"  // Returned as module name for MMI method
                                                  // symbols. These are generated by the JVM.
#define A2N_NO_MODULE_STR       "NoModule"        // Returned as module name when no loaded
                                                  // module is found to contain the requested
                                                  // address.
#define A2N_NO_SYMBOLS_STR      "NoSymbols"       // Returned as symbol name when a module exists
                                                  // that contains the requested address but the
                                                  // module contains no symbols.
#define A2N_NO_SYMBOL_FOUND_STR "NoSymbolFound"   // Returned as symbol name when a module exists,
                                                  // the module contains symbols, but no symbol
                                                  // was found to contain the requested address.
#define A2N_SYMBOL_PLT_STR      "<plt>"           // Returned as symbol name when a module exists
                                                  // and the requested address is in the ELF
                                                  // Procedure Linkage Table. The module may
                                                  // or may not contain any other symbols.
#define A2N_SYMBOL_ILT_STR      "<ilt>"           // Returned as symbol name when a module exists
                                                  // and the requested address is what we think
                                                  // is the ILT. The module may or may not
                                                  // contain any other symbols.  Windows IA64 only.
#define A2N_VSYSCALL32_STR      "<vsyscall-32>"   // Returned as module name for the vsyscall page
                                                  // on x86 and x86_64 Linux systems.
#define A2N_VSYSCALL64_STR      "<vsyscall-64>"   // Returned as module name for the vsyscall page
                                                  // on x86_64 Linux systems.


//
// LINEDATA: Line Number Data
// *********
//
// Filled in by A2N in response to an A2nGetLineNumbersForSymbol() request.
//
// - Caller *MUST* free the storage.
//
typedef struct linedata LINEDATA;
struct linedata {
   int              cnt;                    // Number of elements in line[]
   struct {
      int           lineno;                 // Line number
      unsigned int  offset_start;           // Starting offset (relative to symbol)
      unsigned int  offset_end;             // Ending offset (relative to symbol)
   } line[1];
};
#define SIZEOF_LINE_INFO   12



//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                               Exported APIs                              //
//                               *************                              //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

//
// A2nInit()
// *********
//
// Initialize A2N.
// Only required on platforms where the shared object's initialization
// entry point is not automatically invoked when the shared object is
// loaded. Doesn't hurt if invoked on any platform.
//
ApiExport void ApiLinkage A2nInit(
                             void
                          );

typedef   void (ApiLinkage * PFN_A2nInit) (
                             void
                          );


//
// A2nGetVersion()
// ***************
//
// Returns A2N version information.
// Useful if user program has a dependency on specific version(s) of A2N.
// Version information is returned as an unsigned 32-bit quantity interpreted
// as follows:
//
//           MSB bit                            LSB bit
//
//               3      2 2      1 1
//               1      4 3      6 5              0
//              +--------+--------+----------------+
//              | Major  |  Minor |    Revision    |
//              +--------+--------+----------------+
//               (0-255)  (0-255)     (0-65535)
//
// Returns
//    non-zero: Version information
//
ApiExport unsigned int ApiLinkage A2nGetVersion(void);

typedef   unsigned int (ApiLinkage * PFN_A2nGetVersion) (void);

#define A2N_VERSION_MAJOR(v)  ((v) >> 24)
#define A2N_VERSION_MINOR(v)  (((v) & 0x00ff0000) >> 16)
#define A2N_VERSION_REV(v)    ((v) & 0x0000ffff)


//
// A2nCreateProcess()
// ******************
//
// Add process which is neither a clone nor a fork.
// If a process with the same PID already exists all the information is
// deleted and a new process created.
//
// This API is supported only on Windows.
//
// Returns
//    0:        Process added successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nCreateProcess(
                            unsigned int pid          // Process id
                         );

typedef   int (ApiLinkage * PFN_A2nCreateProcess) (
                            unsigned int
                         );


//
// A2nForkProcess()
// ****************
//
// Add process which inherits but *DOES NOT* share the address space
// (and therefore the loaded module/symbol information) with the process
// that forked it.
// After creation, the forked/forker processes become independent: changes
// affecting one do not affect the other.
// Forked processes can themselves fork/clone other processes.
//
// This API is supported only on Linux.
//
// Returns
//    0:        Process added successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nForkProcess(
                            unsigned int forker_pid,  // Forker process id
                            unsigned int forked_pid   // Forked process id
                         );

typedef   int (ApiLinkage * PFN_A2nForkProcess) (
                            unsigned int,
                            unsigned int
                         );


//
// A2nCloneProcess()
// *****************
//
// Add process which inherits and shares the address space (and therefore the
// loaded module/symbol information) with the process that cloned it. A cloned
// process is similar to a thread.
// Cloned processes can themselves clone/fork other processes.
//
// This API is supported only on Linux.
//
// Returns
//    0:        Process cloned successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nCloneProcess(
                            unsigned int cloner_pid,  // Cloner process id
                            unsigned int cloned_pid   // Cloned process id
                         );

typedef   int (ApiLinkage * PFN_A2nCloneProcess) (
                            unsigned int,
                            unsigned int
                         );


//
// A2nAddModule()
// **************
//
// Associate module (MTE/Executable) information with a process and
// automatically add a process block for the process on the first module
// loaded/added to the process.  The resulting process is a "root" process:
// not a clone and not forked.
//
// If a module is associated with more than one process (such as Win32 DLLs
// or Linux/AIX shared objects) then it must be added to all processes to
// which it is mapped.
//
// If the processes to which the module is mapped all share the same virtual
// address space (cloned processes in Linux/threads in Win32) then the module
// can be "aliased" to another process and a mapping established between the
// processes via A2nCloneProcess().  That will cause all cloned processes to
// share the loaded module information with the "cloner".
//
// Returns
//    0:        Module added successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nAddModule(
                            unsigned int pid,         // Process id
                            uint64 base_addr,         // Module base/load address
                            unsigned int length,      // Module length (in bytes)
                            char * name,              // Fully qualified module name
                            unsigned int ts,          // Module timestamp (0 if not applicable)
                            unsigned int chksum,      // Module check sum (0 if not applicable)
                            void ** not_used          // Here for backward compatibility
                         );

typedef   int (ApiLinkage * PFN_A2nAddModule) (
                            unsigned int,
                            uint64,
                            unsigned int,
                            char *,
                            unsigned int,
                            unsigned int,
                            void **
                         );

#define TS_ANON          0xFFFFFFF1         // Anonymous segment/module
#define TS_VSYSCALL32    0xFFFFFFF2         // vsyscall-32 segment
#define TS_VSYSCALL64    0xFFFFFFF4         // vsyscall-64 segment
#define TS_VDSO          0xFFFFFFF8         // vdso


//
// A2nAddJittedMethod()
// ********************
//
// Add jitted method symbol information to a process.
// The process to which the jitted method symbol will be added *MUST* exist.
//
// Returns
//    0:        Jitted method symbol added successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nAddJittedMethod(
                            unsigned int pid,         // Process id
                            char * name,              // Jitted method name
                            uint64 addr,              // Jitted method address
                            unsigned int length,      // Jitted method length (in bytes)
                            char * code               // Code (or NULL if no code)
                         );

typedef   int (ApiLinkage * PFN_A2nAddJittedMethod) (
                            unsigned int,
                            char *,
                            uint64,
                            unsigned int,
                            char *
                         );


//
// JTLINENUMBER: JVMTI-style Java Method Line Number Data
// *************
//
// Filled in by caller from information obtained from the JVM.
// - See jvmtiLineNumberEntry for JVMTI
//
typedef struct jtlinenumber JTLINENUMBER;
struct jtlinenumber {
   uint64   offset;                         // Offset (from start of method)
   int      lineno;                         // Line number
};

//
// Values for jln_type parameter to A2nAddJittedMethodEx()
//
#define JLN_TYPE_JVMTI            2         // JVMTI-style line number info (JTLINENUMBER)

//
// A2nAddJittedMethodEx()
// **********************
//
// Add jitted method symbol information to a process.
// The process to which the jitted method symbol will be added *MUST* exist.
// The difference between A2nAddJittedMethod() and A2nAddJittedMethodEx()
// is that the latter allows passing method line number information.
//
// Returns
//    0:        Jitted method symbol added successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nAddJittedMethodEx(
                            unsigned int pid,         // Process id
                            char * name,              // Jitted method name
                            uint64 addr,              // Jitted method address
                            unsigned int length,      // Jitted method length (in bytes)
                            char * code,              // Code (or NULL if no code)
                            int jln_type,             // JVMTI-style line number information
                            int jln_cnt,              // Number of JTLINENUMBER/JPLINENUMBER structures
                            void * jln                // Array of JTLINENUMBER/JPLINENUMBER structures
                         );

typedef   int (ApiLinkage * PFN_A2nAddJittedMethodEx) (
                            unsigned int,
                            char *,
                            uint64,
                            unsigned int,
                            int,
                            char *,
                            int,
                            void *
                         );

//
// A2nAddMMIMethod()
// *****************
//
// Add dynamic code (by the MMI) information to a process.
// The process to which the MMI method symbol will be added *MUST* exist.
//
// Returns
//    0:        MMI method symbol added successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nAddMMIMethod(
                            unsigned int pid,         // Process id
                            char * name,              // MMI method name
                            uint64 addr,              // MMI method address
                            unsigned int length,      // MMI method length (in bytes)
                            char * code               // Code (or NULL if no code)
                         );

typedef   int (ApiLinkage * PFN_A2nAddMMIMethod) (
                            unsigned int,
                            char *,
                            uint64,
                            unsigned int,
                            char *
                         );


//
// A2nLoadMap()
// ************
//
// Loads the current mappings of shared objects for a specified process.
// pid = -1 loads modules for all processes
//
// Returns
//    0:        Mappings successfully loaded
//    non-zero: Error
//
ApiExport int ApiLinkage A2nLoadMap(
                            unsigned int pid, // pid to load modules
                            int loadAnon      // whether to load anon modules
                         );

typedef   int (ApiLinkage * PFN_A2nLoadMap) (
                            unsigned int pid,
                            int loadAnon
                         );


//
// A2nGetSymbol()
// **************
//
// Get symbol information for a given address in a given process.
//
// See description of SYMDATA structure above for information on what
// fields are set, and what values they contain, for each return code.
//
// Returns
//    A2N_SUCCESS:         Complete symbol information returned.
//    A2N_NO_SYMBOL_FOUND: Module has symbols but no symbol found for given addr.
//    A2N_NO_SYMBOLS:      Module does not have symbols.
//    A2N_NO_MODULE:       Given addr did not fall in address range of any loaded module.
//    Other non-zero:      Error or symbol not found.
//
ApiExport int ApiLinkage A2nGetSymbol(
                            unsigned int pid,         // Process id
                            uint64 addr,              // Address
                            SYMDATA * sd              // Pointer to SYMDATA structure
                         );

typedef   int (ApiLinkage * PFN_A2nGetSymbol) (
                            unsigned int,
                            uint64,
                            SYMDATA *
                         );


//
// A2nGetSymbolEx()
// ****************
//
// Get symbol information for a given address in a pid set in advance.
//
// * PID must have been set via A2nSetGetSymbolExPid()
// * sd is assumed not to be NULL. No checking.
// * Sets only the following fields in SD:
//   - mod_name, mod_addr, mod_length
//   - sym_name, sym_addr, sym_length
//   - flags
// * Returns same set of return codes as A2nGetSymbol().
//
ApiExport int ApiLinkage A2nGetSymbolEx(
                            uint64 addr,              // Address
                            SYMDATA * sd              // Pointer to SYMDATA structure
                         );

typedef   int (ApiLinkage * PFN_A2nGetSymbolEx) (
                            uint64,                   // Address
                            SYMDATA *                 // Pointer to SYMDATA structure
                         );


//
// A2nSetGetSymbolExPid()
// **********************
//
ApiExport int ApiLinkage A2nSetGetSymbolExPid(
                             unsigned int pid
			  );

typedef   int (ApiLinkage * PFN_A2nSetGetSymbolExPid) (
                             unsigned int
			  );


//
// A2nGetLineNumber()
// ******************
//
// Get source line number and source file, if available, for a given address
// in a given symbol.
//
// Should only be used after A2nGetSymbol() by using the module and symbol
// pointers returned in the SYMDATA structure.
//
// Returns
//    0          no line numbers or errors
//    non-zero:  valid line number
//
ApiExport int ApiLinkage A2nGetLineNumber(
                            void * module,            // Value returned in SYMDATA
                            void * symbol,            // Value returned in SYMDATA
                            uint64 addr,              // Address
                            char ** src_file          // Buffer to return source filename
                         );

typedef   int (ApiLinkage * PFN_A2nGetLineNumber) (
                            void *,
                            void *,
                            uint64,
                            char **
                         );


//
// A2nGetLineNumbersForSymbol()
// ****************************
//
// Returns all the line numbers, if available, for a given symbol.
//
// Should only be used after A2nGetSymbol() by using the module and symbol
// pointers returned in the SYMDATA structure.
//
// Returns
//    Non-NULL   pointer to LINEDATA structure containing the line[] array. The
//               structure is allocated using A2nAlloc() and must be freed by
//               the caller using A2nFree().
//    NULL       no line numbers or errors
//
ApiExport LINEDATA * ApiLinkage A2nGetLineNumbersForSymbol(
                                   void * module,     // Value returned in SYMDATA
                                   void * symbol      // Value returned in SYMDATA
                                );

typedef   LINEDATA * (ApiLinkage * PFN_A2nGetLineNumbersForSymbol) (
                                   void *,
                                   void *
                                );


//
// A2nGetAllFunctionsByName()
// **************************
//
// Searches for functions of a given name in all loaded modules for the given
// process. For example: A2nGetAllFunctionsByName(pid, "foo"); will return
// all ocurrences of function "foo" in all loaded modules in the process.
//
// *** The list returned must be freed by the caller via A2nFreeSymbols().
//
// Returns
//    Non-NULL   pointer to an (null-terminated) array of SYMDATA pointers
//    NULL       Error
//
ApiExport SYMDATA ** ApiLinkage A2nGetAllFunctionsByName(
                                   unsigned int pid,
                                   const char * name
                                );

typedef   SYMDATA ** (ApiLinkage * PFN_A2nGetAllFunctionsByName) (
                                   unsigned int pid,
                                   const char * name
                                );


//
// A2nGetAllFunctions()
// ********************
//
// Returns a list of all functions in all loaded modules for the given process.
//
// *** The list returned must be freed by the caller via A2nFreeSymbols().
//
// Returns
//    Non-NULL   pointer to an (null-terminated) array of SYMDATA pointers
//    NULL       Error
//
ApiExport SYMDATA ** ApiLinkage A2nGetAllFunctions(
                                   unsigned int pid
                                );

typedef   SYMDATA ** (ApiLinkage * PFN_A2nGetAllFunctions) (
                                   unsigned int pid
                                );


//
// A2nFreeSymbols()
// ****************
//
// Free array of symbol returned by A2nGetAllFunctionsByName() and
// A2nGetAllFunction().
//
ApiExport void ApiLinkage A2nFreeSymbols(
                             SYMDATA ** sym_array
                          );

typedef   void (ApiLinkage * PFN_A2nFreeSymbols) (
                             SYMDATA ** sym_array
                          );


//
// A2nGetAddress()
// ***************
//
// Get address of a given symbol within a module in a given process.
// Assumes symbols have already been harvested (run in immediate_gather mode).
//
// Returns
//    0:        Address, and optionally code pointer, returned.
//    non-zero: Error
//
ApiExport int ApiLinkage A2nGetAddress(
                            unsigned int pid,         // Process id
                            char * symbol_name,       // Symbol name
                            char * module_name,       // Module name
                            uint64 * addr,            // Pointer to where address returned (or NULL)
                            void ** code              // Pointer to where code pointer is returned (or NULL)
                         );

typedef   int (ApiLinkage * PFN_A2nGetAddress) (
                            unsigned int,
                            char *,
                            char *,
                            uint64 *,
                            void **
                         );


//
// A2nCollapseSymbolRange()
// ************************
//
// Attempts to collapse all symbols between symbol_start and symbol_end,
// inclusive, into a single symbol name. The new symbol name can be given
// or, if missing, it is assumed to be symbol_start.
// Symbols are collapsed in all modules currently loaded and the information
// is queued and applied if and when a module with the requested name is
// loaded at a later time.
//
// 'module_filename' is the filename (just the name, no path) of the desired
// module. The symbols will be collapsed in all instances of the named module.
// Symbols will be harvested if needed.
//
ApiExport void ApiLinkage A2nCollapseSymbolRange(
                             char * module_filename,  // Module filename
                             char * symbol_start,     // Starting symbol name
                             char * symbol_end,       // Ending symbol name
                             char * symbol_new        // New name for resulting symbol (or NULL)
                          );

typedef   void (ApiLinkage * PFN_A2nCollapseSymbolRange) (
                             char *,
                             char *,
                             char *,
                             char *
                          );


//
// A2nSetCollapseMMIRange()
// ************************
//
// Indicate that the caller wants to automatically collapse MMI symbols,
// in IBM JVMs, into a single symbol name. Actual collapsing takes place
// when (and if) symbols for the JVM are harvested.
// If range_name is NULL then a default name is assigned to the range.
//
// ##### Only works for x86 (Windows/Linux) IBM JVMs #####
//
ApiExport void ApiLinkage A2nSetCollapseMMIRange(
                             char * range_name        // Name of range or NULL
                          );

typedef   void (ApiLinkage * PFN_A2nSetCollapseMMIRange) (
                             char *
                          );



// For use with A2nSet/GetProcessType()
#define PROCESS_TYPE_32BIT     0x32         // 32-bit process
#define PROCESS_TYPE_64BIT     0x64         // 64-bit process

//
// A2nSetProcessType()
// *******************
//
// Set whether a process is 32-bit or 64-bit.
//
// Notes:
// * There is no default process type. It must be set using this API.
// * If set, the SYMDATA.flags field will have the SDFLAG_PROCESS_64BIT
//   or SDFLAG_PROCESS_32BIT set accordingly.
//   Also, the SDFLAG_MODULE_64BIT or SDFLAG_MODULE_32BIT will be set
//   for jitted/MMI methods from the process.
// * The type argument can be either PROCESS_TYPE_32BIT or PROCESS_TYPE_64BIT.
//
// Returns
//    0:        Process type set successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nSetProcessType(
                            unsigned int pid,         // Process id
                            int type                  // Process type
                         );

typedef   int (ApiLinkage * PFN_A2nSetProcessType) (
                            unsigned int,
                            int
                         );


//
// A2nGetProcessType()
// *******************
//
// Returns whether a process is 32-bit or 64-bit.
//
// Notes:
// * There is no default process type. It must be set using the
//   A2nSetProcessType() API.
//
// Returns
//    PROCESS_TYPE_32BIT:  32-bit process
//    PROCESS_TYPE_64BIT:  64-bit process
//    Zero:                Process type not set/unknown
//    Other non-zero:      Error(s)
//
ApiExport int ApiLinkage A2nGetProcessType(
                            unsigned int pid          // Process id
                         );

typedef   int (ApiLinkage * PFN_A2nGetProcessType) (
                            unsigned int
                         );


//
// A2nSetProcessName()
// *******************
//
// Associate a name with a process.
//
// Default process names are as follows:
// * When a process is first created as a result of A2nAddModule(),
//   the process name defaults to the name of the first module added.
// * When a process is first created as a result of A2nSetProcessName(),
//   the process name is set as requested.
// * When a process is created as a result of A2nCloneProcess()
//   the process name defaults to the name of the cloner process.
// * When a process is created as a result of A2nForkProcess()
//   the process name defaults to the name of the forker process.
//
// Returns
//    0:        Process name set successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nSetProcessName(
                            unsigned int pid,         // Process id
                            char * name               // Process name
                         );

typedef   int (ApiLinkage * PFN_A2nSetProcessName) (
                            unsigned int,
                            char *
                         );


//
// A2nGetProcessName()
// *******************
//
// Return name (if known) of given process.
//
// Note:
// * Space is allocated via malloc() for the name. As such, the caller
//   must free() the string.
//
// Returns
//    NULL:     Invalid pid or process name not set/known
//    non-NULL: Process name
//
ApiExport char * ApiLinkage A2nGetProcessName(
                               unsigned int pid       // Process id whose name we want
                            );

typedef   char * (ApiLinkage * PFN_A2nGetProcessName) (
                               unsigned int
                            );


//
// A2nGetParentPid()
// *****************
//
// Return pid of parent process.
//
// Windows:
// * There is no notion of parent pid. pid and parent pid are the same.
//
// Linux:
// * The "parent process" is the process that forked or cloned the
//   child process.
// * For processes that were not cloned nor forked the returned parent pid
//   is the system pid.
//
// Returns
//    -1:       Error or no parent
//    Other:    Parent pid
//
ApiExport unsigned int ApiLinkage A2nGetParentPid(
                                     unsigned int child_pid  // Child pid
                                  );

typedef   unsigned int (ApiLinkage * PFN_A2nGetParentPid) (
                                     unsigned int
                                  );


//
// A2nGetOwningPid()
// *****************
//
// Return pid of owning process.
//
// Windows:
// * There is no notion of owning pid. pid and owning pid are the same.
//
// Linux:
// * The "owning process" is the process that owns the address space.
// * A forked process is, by definition, an owning process.
// * For cloned processes the owning process is the forked process
//   that started the clone tree.
//
// Returns
//    -1:       Error or no owning pid
//    Other:    Parent pid
//
ApiExport unsigned int ApiLinkage A2nGetOwningPid(
                                     unsigned int child_pid  // Child pid
                                  );

typedef   unsigned int (ApiLinkage * PFN_A2nGetOwningPid) (
                                     unsigned int
                                  );


//
// A2nSetSystemPid()
// *****************
//
// Set the system's (kernel) process id.
// This information is necessary in order to know which modules are "owned"
// by the system since the system can run in the context of any other process.
// Thus symbols that are not found in a process' loaded modules could be
// found in modules loaded by the system.
//
// *** The default system PID is zero (0) on both Windows and Linux.
//
ApiExport void ApiLinkage A2nSetSystemPid(
                             unsigned int system_pid  // System pid
                          );

typedef   void (ApiLinkage * PFN_A2nSetSystemPid) (
                             unsigned int
                          );


//
// A2nCreateMsiFile()
// ******************
//
// Save internal representation of module/symbol information to disk for
// later use. Information about loaded modules and any symbols gathered
// so far is saved so it can be re-loaded at a later time.
//
// Returns
//    0:        Current state saved successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nCreateMsiFile(
                            char * filename           // File to which state is saved
                         );

typedef   int (ApiLinkage * PFN_A2nCreateMsiFile) (
                            char *
                         );


//
// A2nLoadMsiFile()
// ****************
//
// Read a previously saved module/symbol information file and rebuild internal
// symbol representation.  Invoking this API implies that the caller is working
// with static data and as such:
//
// * Symbol gathering is turned off. Only symbols contained in the restored
//   MSI file are used.
//
// * MSI files can't be restored on machines with different architecture
//   than the machine on which the files were created  Results will be
//   unpredictable (should the restore even succeed).
//
// * A2nLoadMsiFile() *WILL BE REJECTED* if any of the following requests
//   have already been made:
//   - A2nAddModule()
//   - A2nAddSymbol()
//
// * The following requests *ARE ALLOWED* after A2nLoadMsiFile() has been
//   invoked:
//   - A2nAddModule()
//   - A2nAddSymbol()
//   - A2nAddJittedMethod()
//   - A2nSetProcessName()
//   - A2nCloneProcess()
//   - A2nForkProcess()
//   - A2nGetSymbol()
//   - A2nGetVersion()
//   - A2nGetAddress()
//   - A2nCollapseSymbolRange()
//   - A2nGetParentPid()
//   - A2nGetOwningPid()
//   - A2nSetSystemPid()
//   - A2nSetErrorMessageMode()
//   - A2nSetDebugMode()
//   - A2nDumpProcesses()
//   - A2nDumpModules()
//   - A2nSetNoModuleFoundDumpThreshold()
//   - A2nSetNoSymbolFoundDumpThreshold()
//   - A2nGetMsiFileInformation()
//
// * The following requests *ARE IGNORED* after A2nLoadMsiFile():
//   - A2nSetKernelImageName()
//   - A2nSetKernelMapName()
//   - A2nSetKallsymsFileLocation()
//   - A2nSetModulesFileLocation()
//   - A2nSetSymbolSearchPath()
//   - A2nSetSymbolGatherMode()
//   - A2nSetCodeGatherMode()
//   - A2nSetValidateKernelMode()
//
// Returns
//    0:        State restored successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nLoadMsiFile(
                            char * filename           // File from which state is restored
                         );

typedef   int (ApiLinkage * PFN_A2nLoadMsiFile) (
                            char *
                         );


//
// A2nGetMsiFileInformation()
// **************************
//
// Returns information about the last MSI file loaded.
//
// Returns
//    0:        Current state saved successfully
//    non-zero: Error
//
ApiExport int ApiLinkage A2nGetMsiFileInformation(
                            int * wordsize,           // Word size of machine where MSI file written
                            int * bigendian,          // 1 if machine where MSI file written was big endian
                            int * os                  // OS running when MSI file written
                         );

typedef   int (ApiLinkage * PFN_A2nGetMsiFileInformation) (
                            int *,
                            int *,
                            int *
                         );

#define MSI_FILE_WINDOWS       1            // MSI file written by Windows machine
#define MSI_FILE_LINUX         2            // MSI file written by Linux machine


//
// A2nSetKernelImageName()
// ***********************
//
// Gives the fully qualified name of the non-stripped kernel image file.
// Linux kernel symbol search rules are:
// 1- If A2nSetKernelImageName() issued then try that image name
// 2- If A2nSetKernelMapName() issued then try that map name
// 3- Try module as given my A2nAddModule() - (usually "./vmlinux")
// 4- Give up
//
// Note:
// * Calls to this API are ignored on Windows - the kernel is treated
//   just like any other image.
//
ApiExport int ApiLinkage A2nSetKernelImageName(
                            char * fqname             // Fully qualified name
                         );

typedef   int (ApiLinkage * PFN_A2nSetKernelImageName) (
                            char *
                         );


//
// A2nSetKernelMapName()
// *********************
//
// Gives the fully qualified name of the kernel map file.
// Linux kernel symbol search rules are:
// 1- If A2nSetKernelImageName() issued then try that image name
// 2- If A2nSetKernelMapName() issued then try that map name
// 3- Try module as given my A2nAddModule() - (usually "./vmlinux")
// 4- Give up
//
// Note:
// * Calls to this API are ignored on Windows - the kernel is treated
//   just like any other image.
//
ApiExport int ApiLinkage A2nSetKernelMapName(
                            char * fqname             // Fully qualified name
                         );

typedef   int (ApiLinkage * PFN_A2nSetKernelMapName) (
                            char *
                         );


//
// A2nSetKallsymsFileLocation()
// ****************************
//
// Gives the fully qualified name of either the kallsyms file (usually
// /proc/kallsyms) or another file that contains a copy of kallsyms.
// PI sometimes copies /proc/kallsyms to some other location and wants
// to use that copy for symbol resolution.
//
// size = 0 implies entire file, or to EOF from offset.
//
// Note:
// * Calls to this API are ignored on Windows.
//
ApiExport int ApiLinkage A2nSetKallsymsFileLocation(
                            char * fqname,            // Fully qualified name
                            int64_t offset,           // File offset or 0
                            int64_t size              // File size or 0
                         );

typedef   int (ApiLinkage * PFN_A2nSetKallsymsFileLocation) (
                            char *,
                            int64_t,
                            int64_t
                         );


//
// A2nSetModulesFileLocation()
// ***************************
//
// Gives the fully qualified name of either the modules file (usually
// /proc/modules) or another file that contains a copy of modules.
// PI sometimes copies /proc/modules to some other location and wants
// to use that copy for symbol resolution.
//
// size = 0 implies entire file, or to EOF from offset.
//
// Note:
// * Calls to this API are ignored on Windows.
//
ApiExport int ApiLinkage A2nSetModulesFileLocation(
                            char * fqname,            // Fully qualified name
                            int64_t offset,           // File offset or 0
                            int64_t size              // File size or 0
                         );

typedef   int (ApiLinkage * PFN_A2nSetModulesFileLocation) (
                            char *,
                            int64_t,
                            int64_t
                         );


//
// A2nSetSymbolSearchPath()
// ************************
//
// Provide additional paths from which to attempt to harvest symbols
// should the actual executable/module not contain symbols.
// "path" is a string containing paths/directories.
//
// Rules for using the search path:
// 1) Not used if symbols are obtained from the on-disk executable.
// 2) For each path:
//    a) Look for a "symbol" file associated with the module.
//    b) If (a) does not yield symbols continue repeating a-b for
//       each path until symbols are found or until all paths
//       have been searched.
//
// Paths, if more than one, are separated by a platform-specific separator
// character as follows:
// * AIX and Linux:    ':'  (colon)      (ex: "~:/work:/usr/local/symbols")
// * Windows and OS/2: ';'  (semicolon)  (ex: "c:\winnt\system32\symbols;f:\symbols")
//
// Default paths for each supported platform are:
// * Linux: "/symbols:/usr/symbols:~/symbols"
// * Win32: "%SystemRoot%\symbols\<dll | exe | sys | ...>"
// * AIX:   ????
// * OS/2:  ????
//
// Returns
//    0:        Symbol search path set
//    non-zero: Error (ex. invalid usage)
//
ApiExport int ApiLinkage A2nSetSymbolSearchPath(
                            const char * searchpath   // List of "paths"
                         );

typedef   int (ApiLinkage * PFN_A2nSetSymbolSearchPath) (
                            const char *
                         );


//
// A2nSetRangeDescFilename()
// *************************
//
// Sets the name of a file, containing symbol range descriptions, to be
// processed by A2N.  A2N reads the file at initialization time and
// remembers the ranges.  As symbols are harvested, ranges are collapsed
// as required.
// The file contains one line per desired range.  Each line contains
// either 3 or 4 blank-delimited fields in the same for as the inputs
// to A2nCollapseSymbolRange() API:
//
//   module_filename  first_symbol_in_range  last_symbol_in_range  range_name
//
// * module_filename, first_symbol_in_range and last_symbol_in_range
//   are required.
// * range_name is optional.  If not present the rage name will be
//   named first_symbol_in_range.
//
// Returns
//    0:        Success.
//    non-zero: Error
//
ApiExport int ApiLinkage A2nSetRangeDescFilename(
                            char * fn                 // Filename
                         );

typedef   int (ApiLinkage * PFN_A2nSetRangeDescFilename) (
                            char *
                         );


//
// A2nSetJvmmapFilename()
// **********************
//
// Sets the name, just the filename, of the jvmmap file to use.
// The file must reside (in search order) in either:
// * The current directory
// * Along the PATH
// * Along the symbol search path
//
// Returns
//    0:        Success.
//    non-zero: Error
//
ApiExport int ApiLinkage A2nSetJvmmapFilename(
                            char * fn                 // Filename
                         );

typedef   int (ApiLinkage * PFN_A2nSetJvmmapFilename) (
                            char *
                         );


//
// A2nAlloc()
// **********
//
// Allocates memory. Use A2nFree() to deallocate.
//
ApiExport void * ApiLinkage A2nAlloc(
                               int size
                            );

typedef   void * (ApiLinkage * PFN_A2nAlloc) (
                               int size
                            );


//
// A2nFree()
// *********
//
// Deallocates memory allocated via A2nAlloc().
//
ApiExport void ApiLinkage A2nFree(
                             void * mem
                          );

typedef   void (ApiLinkage * PFN_A2nFree) (
                             void *
                          );


//
// A2nGenerateHashValue()
// **********************
//
// Generate a hash value given a string.
//
ApiExport unsigned int ApiLinkage A2nGenerateHashValue(
                                     char * str
                                  );

typedef   unsigned int (ApiLinkage * PFN_A2nGenerateHashValue) (
                                     char *
                                  );


//
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// Flags/values used with Set*Mode() APIs                                   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
//
#define MODE_OFF               0            // Turn off requested mode
#define MODE_ON                1            // Turn on requested mode

#define MODE_LAZY              0            // Set Lazy symbol gather mode
#define MODE_IMMEDIATE         1            // Set Immediate symbol gather mode

#define MSG_NONE               0            // Suppress all messages
#define MSG_ERROR              1            // Show error messages and higher
#define MSG_WARN               2            // Show informational messages and higher
#define MSG_INFO               3            // Show warning messages and higher
#define MSG_ALL                4            // Show all messages

#define MODE_ANY_SYMBOL        1            // Return any symbol harvested
#define MODE_GOOD_SYMBOLS      2            // Return good (debug) symbols only

#define MODE_RANGE             0            // Return only range name for ranges
#define MODE_RANGE_SYMBOL      1            // Return range.symbol for ranges

#define NSF_ACTION_NOSYM       1            // Return NULL symbol name on NSF
#define NSF_ACTION_NSF         2            // Return "NoSymbolFound" symbol name on NSF
#define NSF_ACTION_SECTION     3            // Return section name on NSF

#define DEBUG_MODE_NONE        0x00000000   // No debug
#define DEBUG_MODE_INTERNAL    0x00000001   // Entry/exit of all all functions
#define DEBUG_MODE_HARVESTER   0x00000002   // Harvester
#define DEBUG_MODE_API         0x00000004   // Entry/exit of exported APIs only
#define DEBUG_MODE_ALL         0x7FFFFFFF   // Every possible debug output


//
// A2nSetSymbolQualityMode()
// *************************
//
// Tells A2N the desired symbol quality the caller is willing to accept.
// The caller may be willing to accept any symbol or only debug symbols
// contained in the executable.
//
// *** A2N default is to return debug symbols.
//
ApiExport void ApiLinkage A2nSetSymbolQualityMode(
                             int mode                 // Quality mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetSymbolQualityMode) (
                             int
                          );

#define A2nReturnAnySymbol()           A2nSetSymbolQualityMode(MODE_ANY_SYMBOL)
#define A2nReturnGoodSymbolsOnly()     A2nSetSymbolQualityMode(MODE_GOOD_SYMBOLS)


//
// A2nSetValidateKernelMode()
// **************************
//
// Tells A2N whether or not to perform kernel symbol validatation (ie. making
// sure kernel from which symbols are harvested is the kernel we're running
// with).
// It goes without saying that this call must be made prior to making any
// A2nGetSymbol() request which results in kernel symbols being harvested.
//
// This API is supported only on Linux.
//
// *** A2N default is to always validate kernel symbols.
//
ApiExport void ApiLinkage A2nSetValidateKernelMode(
                             int mode                  // Validation mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetValidateKernelMode) (
                             int
                          );

#define A2nValidateKernel()            A2nSetValidateKernelMode(MODE_ON)
#define A2nDontValidateKernel()        A2nSetValidateKernelMode(MODE_OFF)


//
// A2nStopOnKernelValidationError()
// ********************************
//
// Tells A2N to stop processing, and quit, if kernel symbol validation fails.
// This API is supported only on Linux.
//
// *** A2N default is to continue processing if validation fails.
//
ApiExport void ApiLinkage A2nStopOnKernelValidationError(
                             void
                          );

typedef   void (ApiLinkage * PFN_A2nStopOnKernelValidationError) (
                             void
                          );


//
// A2nSetSymbolValidationMode()
// ****************************
//
// Tells A2N whether or not to perform symbol validatation (ie. making symbols
// are in fact correct for the module/image).
//
// Symbol validation varies by plaform:
// Linux:
//   Since ELF modules have no means of uniquely identifying a module
//   A2N has no way to validate symbols *IF* they *DON'T* come from the module
//   itself (ie. a map file, for example).
//   A2N *DOES* validate kernel symbols to make sure they match the currently
//   running kernel. Kernel validation is accomplished by making sure every
//   KSYM in present (with the same name and at the same address) in the harvested
//   symbols.
//
// Windows:
//   PE images, DBG, PDB and MAP files all contain a 32-bit timestamp, and
//   optionally a 32-bit checksum, generated by the linker when the image
//   is linked. Symbol files for the image also contain the same timestamp,
//   regardless of when they symbol files are generated.
//   A2N only gathers symbols if:
//   * The loaded module and symbol file checksums match.
//   * The disk image and symbol file checksums match.
//   * The loaded module and symbol file timestamps match.
//   * The disk image and symbol file timestamps match.
//
// *** A2N default is to always validate symbols.
//
ApiExport void ApiLinkage A2nSetSymbolValidationMode(
                             int mode                 // Validation mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetSymbolValidationMode) (
                             int mode
                          );

#define A2nValidateSymbols()           A2nSetSymbolValidationMode(MODE_ON)
#define A2nDontValidateSymbols()       A2nSetSymbolValidationMode(MODE_OFF)


//
// A2nSetErrorMessageMode()
// ************************
//
// Tells A2N whether or not to write messages (to a2n.err) on errors.
//
// *** A2N default is to always write messages on errors.
//
ApiExport void ApiLinkage A2nSetErrorMessageMode(
                             int mode                 // Error message mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetErrorMessageMode) (
                             int mode
                          );

#define A2nDontShowAnyMessages()       A2nSetErrorMessageMode(MSG_NONE)
#define A2nShowErrorMessages()         A2nSetErrorMessageMode(MSG_ERROR)
#define A2nShowWarningMessages()       A2nSetErrorMessageMode(MSG_WARN)
#define A2nShowInfoMessages()          A2nSetErrorMessageMode(MSG_INFO)
#define A2nShowAllMessages()           A2nSetErrorMessageMode(MSG_ALL)


//
// A2nSetNoSymbolFoundAction()
// ***************************
//
// Tells A2N what symbol name to return whenever the A2N_NO_SYMBOL_FOUND
// or A2N_NO_SYMBOLS return codes are returned by A2nGetSymbol().
// These actions *ONLY* affect the values returned in the sym_name,
// sym_addr and sym_length fields in the SYMDATA structure.
//
// Possible actions are:
// * NSF_ACTION_NSF:     Return "NoSymbolFound" string for symbol name (DEFAULT)
// * NSF_ACTION_SECTION: Return section name string for symbol name
// * NSF_ACTION_NOSYM:   Return NULL string for symbol name
//
// How sym_* fields in SYMDATA are set:
//
//      rc/nsf_action         sym_name       sym_addr     sym_len
//      -----------------     -------------  -----------  ----------
//      SUCCESS               Y              Y            Y
//
//      NO_SYMBOL_FOUND
//        action: NSF         NoSymbolFound  mod_addr     mod_len
//        action: SECTION     sec_name(3)    sec_addr(4)  sec_len(4)
//        action: NOSYM       NULL           mod_addr     mod_len
//
//      NO_SYMBOLS
//        action: NSF         NoSymbols      mod_addr     mod_len
//        action: SECTION     sec_name(3)    sec_addr(4)  sec_len(4)
//        action: NOSYM       NULL           mod_addr     mod_len
//
//      NO_MODULE             NoSymbols      NULL         0
//
//      OTHER                 (?)            (?)          (?)
//
//
// (3) If section name not available then "NoSymbolFound" or "NoSymbols" is returned
// (4) Set if known, NULL/0 otherwise
// (?) May or may not be set and not guaranteed to be any good.
//
// *** A2N default is to return "NoSymbolFound" or "NoSymbols" string (NSF_ACTION_NSF)
//
ApiExport void ApiLinkage A2nSetNoSymbolFoundAction(
                             int action               // No symbol found action
                          );

typedef   void (ApiLinkage * PFN_A2nSetNoSymbolFoundAction) (
                             int
                          );

#define A2nReturnNullSymbolNameOnNSF() A2nSetNoSymbolFoundAction(NSF_ACTION_NOSYM)
#define A2nReturnNoSymbolFoundOnNSF()  A2nSetNoSymbolFoundAction(NSF_ACTION_NSF)
#define A2nReturnSectionNameOnNSF()    A2nSetNoSymbolFoundAction(NSF_ACTION_SECTION)


//
// A2nSetSymbolGatherMode()
// ************************
//
// Tells A2N whether to gather symbols for a module the first time a
// symbol from the module is requested (LAZY mode) or to gather symbols
// immediately after a module is added (IMMEDIATE mode).
//
// IMMEDIATE mode is not recommended because it will result in symbols
// being gathered for all modules, whether or not a symbol from the
// module is ever needed.
//
// *** A2N default is LAZY symbol gather mode.
//
ApiExport void ApiLinkage A2nSetSymbolGatherMode(
                             int mode                 // Symbol gather mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetSymbolGatherMode) (
                             int
                          );

#define A2nLazySymbolGather()          A2nSetSymbolGatherMode(MODE_LAZY)
#define A2nImmediateSymbolGather()     A2nSetSymbolGatherMode(MODE_IMMEDIATE)


//
// A2nSetCodeGatherMode()
// **********************
//
// Tells A2N whether or not to harvest actual code from the on-disk
// executables/images along with symbols (if present).
// Option ignored if symbols are not gathered from an executable.
//
// *** A2N default is not to gather code.
//
ApiExport void ApiLinkage A2nSetCodeGatherMode(
                             int mode                 // Code gather mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetCodeGatherMode) (
                             int mode
                          );

#define A2nGatherCode()                A2nSetCodeGatherMode(MODE_ON)
#define A2nDontGatherCode()            A2nSetCodeGatherMode(MODE_OFF)


//
// A2nSetDemangleCppNamesMode()
// ****************************
//
// Tells A2N to demangle C++ names (if possible) when gathering symbols.
//
// *** A2N default is not to demangle names.
//
ApiExport void ApiLinkage A2nSetDemangleCppNamesMode(
                             int mode                 // Demangle mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetDemangleCppNamesMode) (
                             int
                          );

#define A2nDemangleCppNames()          A2nSetDemangleCppNamesMode(MODE_ON)
#define A2nDontDemangleCppNames()      A2nSetDemangleCppNamesMode(MODE_OFF)


//
// A2nSetReturnRangeSymbolsMode()
// ******************************
//
// Tells A2N what symbol name to return (A2nGetSymbol) when the symbol
// is contained in a user-defined range.
// Choices are:
// * Return just the range name
// * Return the range name concatenated with the symbol name (range.symbol)
//
// *** A2N default is to only return the range name.
//
ApiExport void ApiLinkage A2nSetReturnRangeSymbolsMode(
                             int mode                 // Symbol return mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetReturnRangeSymbolsMode) (
                             int
                          );

#define A2nReturnRangeNameOnly()       A2nSetReturnRangeSymbolsMode(MODE_RANGE)
#define A2nReturnRangeAndSymbolName()  A2nSetReturnRangeSymbolsMode(MODE_RANGE_SYMBOL)


//
// A2nSetReturnLineNumbersMode()
// *****************************
//
// Tells A2N to return source line numbers, if available, along with symbol info.
//
// *** A2N default is to not return line numbers.
//
ApiExport void ApiLinkage A2nSetReturnLineNumbersMode(
                             int mode                 // Line number return mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetReturnLineNumbersMode) (
                             int
                          );

#define A2nReturnLineNumbers()         A2nSetReturnLineNumbersMode(MODE_ON)
#define A2nDontReturnLineNumbers()     A2nSetReturnLineNumbersMode(MODE_OFF)


//
// A2nSetRenameDuplicateSymbolsMode()
// **********************************
//
// Tells A2N whether or not to give unique names to duplicate symbols.
//
// *** A2N default is to not rename dupliate symbols.
//
ApiExport void ApiLinkage A2nSetRenameDuplicateSymbolsMode(
                             int mode                 // Rename duplicate symbol mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetRenameDuplicateSymbolsMode) (
                             int
                          );

#define A2nRenameDuplicateSymbols()         A2nSetRenameDuplicateSymbolsMode(MODE_ON)
#define A2nDontRenameDuplicateSymbols()     A2nSetRenameDuplicateSymbolsMode(MODE_OFF)


//
// A2nSetMultiThreadedMode()
// *************************
//
// Causes A2N to be as thread-safe as it can.
//
ApiExport void ApiLinkage A2nSetMultiThreadedMode(
                             int mode
			  );

typedef   void (ApiLinkage * PFN_A2nSetMultiThreadedMode) (
                             int
			  );

#define A2nMultiThreadedMode()              A2nSetMultiThreadedMode(MODE_ON)
#define A2nSigleThreadedMode()              A2nSetMultiThreadedMode(MODE_OFF)




//////////////////////////////////////////////////////////////////////////////
//                             Official Use Only                            //
//                             *****************                            //
//////////////////////////////////////////////////////////////////////////////

//
// A2nSetDebugMode()
// *****************
//
// Sets the A2N debugging mode.
// Setting the mode to any of the "on" modes forces ErrorMessageMode on.
//
// *** A2N default is to turn off debugging mode.
//
ApiExport void ApiLinkage A2nSetDebugMode(
                             int mode                 // Debug mode
                          );

typedef   void (ApiLinkage * PFN_A2nSetDebugMode) (
                             int
                          );

#define A2nDebugNone()                 A2nSetDebugMode(DEBUG_MODE_NONE)
#define A2nDebugApi()                  A2nSetDebugMode(DEBUG_MODE_API)
#define A2nDebugHarvester()            A2nSetDebugMode(DEBUG_MODE_HARVESTER)
#define A2nDebugAll()                  A2nSetDebugMode(DEBUG_MODE_ALL)


//
// A2nDumpProcesses()
// ******************
//
// Force dump of the process data structures to disk for diagnostics purpose.
// Data dumped in ascii.
//
// *** A2N default is to NOT ever dump processes.
//
ApiExport void ApiLinkage A2nDumpProcesses(
                             char * filename
                          );

typedef   void (ApiLinkage * PFN_A2nDumpProcesses) (
                             char *
                          );


//
// A2nDumpModules()
// ****************
//
// Force dump of the module data structures to disk for diagnostics purpose.
// Data dumped in ascii.
//
// *** A2N default is to NOT ever dump modules.
//
ApiExport void ApiLinkage A2nDumpModules(
                             char * filename
                          );

typedef   void (ApiLinkage * PFN_A2nDumpModules) (
                             char *
                          );


//
// A2nSetNoModuleFoundDumpThreshold()
// **********************************
//
// Sets maximum number of times A2nGetSymbol() returns A2N_NO_MODULE
// (ie. no module found) before forcing a Process/Module information dump.
// If the threshold is reached, a Process/Module dump is forced when
// a2n terminates.
// Specifying -1 turns off the feature.
//
// *** A2N default is to set threshold to -1.
//
ApiExport void ApiLinkage A2nSetNoModuleFoundDumpThreshold(
                             int threshold            // Number of NMF before forcing dump
                          );

typedef   void (ApiLinkage * PFN_A2nSetNoModuleFoundDumpThreshold) (
                             int
                          );


//
// A2nSetNoSymbolFoundDumpThreshold()
// **********************************
//
// Sets maximum number of times A2nGetSymbol() returns A2N_NO_SYMBOL FOUND
// before forcing a Process/Module information dump.
// If the threshold is reached, a Process/Module dump is forced when
// a2n terminates.
// Specifying -1 turns off the feature.
//
// *** A2N default is to set threshold to -1.
//
ApiExport void ApiLinkage A2nSetNoSymbolFoundDumpThreshold(
                             int threshold            // Number of NSF before forcing dump
                          );

typedef   void (ApiLinkage * PFN_A2nSetNoSymbolFoundDumpThreshold) (
                             int
                          );


//
// A2nListControls()
// *****************
//
// List environment variables (to stdout) and how to use them.
//
ApiExport void ApiLinkage A2nListControls(void);

typedef   void (ApiLinkage * PFN_A2nListControls) (void);


//
// A2nRcToString()
// ***************
//
ApiExport char * ApiLinkage A2nRcToString(int rc);

typedef   char * (ApiLinkage * PFN_A2nRcToString)(int);




//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                               Return Codes                               //
//                               ************                               //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
#define  A2N_SUCCESS                       0     // Everything worked
#define  A2N_NO_SYMBOL_FOUND               1     // A2nGetSymbol/Ex: Have symbols but no symbol found
#define  A2N_NO_SYMBOLS                    2     // A2nGetSymbol/Ex: Module has no symbols
#define  A2N_NO_MODULE                     3     // A2nGetSymbol/Ex: No loaded module found
#define  A2N_FLAKY_SYMBOL                  4     // A2nGetSymbol/Ex: Not sure if symbols match image
#define  A2N_CREATEPROCESS_ERROR          10     // Unable to create process
#define  A2N_ADDMODULE_ERROR              11     // Unable to add module
#define  A2N_ADDSECTION_ERROR             12     // Unable to add section
#define  A2N_GETPROCESS_ERROR             13     // Unable to find process
#define  A2N_GETMODULE_ERROR              14     // Unable to find module
#define  A2N_GETSYMBOL_ERROR              15     // Unable to find symbol
#define  A2N_MALLOC_ERROR                 16     // Unable to malloc memory
#define  A2N_NULL_SYMDATA_ERROR           17     // NULL SYMDATA pointer
#define  A2N_NOT_SUPPORTED                18     // Function not supported
#define  A2N_INVALID_TYPES                19     // "types" string is NULL
#define  A2N_INVALID_PATH                 20     // "path" string is NULL
#define  A2N_INVALID_USAGE                21     // Invalid usage value
#define  A2N_INVALID_HANDLE               22     // Invalid module/A2N instance handle
#define  A2N_INVALID_FILENAME             23     // Invalid filename
#define  A2N_INVALID_PARENT               24     // Parent process not found
#define  A2N_NO_MODULE_LOADED             25     // No loaded module contains symbol
#define  A2N_ADDSYMBOL_ERROR              26     // Error adding symbol
#define  A2N_ADD_JITTEDMETHOD_ERROR       27     // Error adding LM node for jitted method
#define  A2N_INVALID_NAME                 28     // Invalid process name
#define  A2N_TRYING_TO_NAME_CLONE         29     // Trying to name a clone process
#define  A2N_CLONEPROCESS_ERROR           30     // Error trying to clone process
#define  A2N_FORKPROCESS_ERROR            31     // Error trying to fork process
#define  A2N_FILE_NOT_FOUND_OR_CANT_READ  32     // File not found or no permission to read it
#define  A2N_REQUEST_REJECTED             33     // Request rejected :-)
#define  A2N_FILEWRITE_ERROR              34     // File write error
#define  A2N_CREATEMSIFILE_ERROR          35     // Error creating MSI file
#define  A2N_READMSIFILE_ERROR            36     // Error reading MSI file
#define  A2N_INVALID_MODULE_TYPE          37     // Unknown/invalid module type
#define  A2N_INVALID_MODULE_LENGTH        38     // Invalid module length
#define  A2N_NULL_MODULE_NAME             39     // Module name is NULL
#define  A2N_INVALID_ARGUMENT             40     // Invalid input argument
#define  A2N_COLLAPSE_RANGE_ERROR         41     // Unable to collapse symbol range
#define  A2N_FILEOPEN_ERROR               42     // File open error
#define  A2N_FILEREAD_ERROR               43     // File read error
#define  A2N_INVALID_RANGE_FILE           44     // Range file has no valid descriptors
#define  A2N_ADD_MMIMETHOD_ERROR          45     // Error adding an MMI (dynamic code) method
#define  A2N_INVALID_METHOD_LENGTH        46     // Invalid jitted method length
#define  A2N_NULL_METHOD_NAME             47     // Method name is NULL
#define  A2N_FILE_NOT_RIGHT               48     // Something isn't right with the file
#define  A2N_INTERNAL_NON_FATAL_ERROR     49     // Something's wrong but we can survive
#define  A2N_RESERVED_DO_NOT_USE_1        50     // ***** (0x32) DO NOT USE *****
#define  A2N_INTERNAL_FATAL_ERROR         51     // Something's wrong but we can't survive
#define  A2N_INVALID_PROCESS_TYPE         52     // Invalid process type
#define  A2N_ACCESS_DENIED                53     // Missing pre-reqs. Can't do what you want
#define  A2N_LARGEST_RC                   53

#define  A2N_RESERVED_DO_NOT_USE_2       100     // ***** (0x64) DO NOT USE *****

#endif  // _A2N_H_
