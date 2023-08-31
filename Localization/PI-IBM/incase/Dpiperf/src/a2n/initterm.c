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

#define _GNU_SOURCE
#include "a2nhdr.h"
#if defined(_AIX)
  #include "bdate.h"
#endif

extern gv_t a2n_gv;                         // A2N globals
extern SYM_NODE ** snp;                     // SN pointer array
extern SYM_NODE ** dsnp;                    // SN pointer array for checking dups
extern API_LOCKS  api_lock;                 // API lockout flags
extern API_COUNTS  api_cnt;                 // API counters
extern int initialized;                     // dll/so initialized

#if defined(_WINDOWS)
extern CRITICAL_SECTION harvester_cs;       // Serialize access to harvester

extern char * SystemRoot;                   // Windows System Directory
extern char * SystemDrive;                  // Windows System Drive
extern char * WinSysDir;                    // Windows System Directory (without drive letter)
extern char * OS_System32;                  // %SystemRoot%\System32
extern char * OS_SysWow64;                  // %SystemRoot%\SysWOW64
extern char * OS_WinSxS;                    // %SystemRoot%\WinSxS
extern int dev_type[MAX_DEVICES];           // Device type
extern char rdev_name[MAX_DEVICES][128];    // Valid sym links for DOS devices (as is)
extern char dev_name[MAX_DEVICES][128];     // Valid sym links for DOS devices (fixed up)
extern char * drv_name[MAX_DEVICES];

extern int dbghelp_version_major;
extern PFN_SymInitialize            pSymInitialize;          // >= 5.1: Required
extern PFN_SymRegisterCallback64    pSymRegisterCallback64;  // >= 5.1: Required
extern PFN_SymSetSearchPath         pSymSetSearchPath;       // >= 5.1: Required
extern PFN_SymCleanup               pSymCleanup;             // >= 5.1: Required
extern PFN_SymGetOptions            pSymGetOptions;          // >= 5.1: Required
extern PFN_SymSetOptions            pSymSetOptions;          // >= 5.1: Required
extern PFN_UnDecorateSymbolName     pUnDecorateSymbolName;   // >= 5.1: Required to demangle names
extern PFN_SymEnumLines             pSymEnumLines;           // >= 6.4: Optional. Use if available
extern PFN_SymEnumSourceLines       pSymEnumSourceLines;     // >= 6.1: Optional. Use if SymEnumSourceLines() not available
extern PFN_SymEnumSymbols           pSymEnumSymbols;         // >= 5.1: Required
extern PFN_SymLoadModuleEx          pSymLoadModuleEx;        // >= 6.0: Use instead of SymLoadModule64() if available
extern PFN_SymLoadModule64          pSymLoadModule64;        // >= 5.1: Required
extern PFN_SymUnloadModule64        pSymUnloadModule64;      // >= 5.1: Required
extern PFN_SymGetModuleInfo64       pSymGetModuleInfo64;     // >= 5.1: Required

HMODULE hDbgHelp = NULL;
void find_good_dbghelp(void);

// WOW64 on Win64
BOOL wow64_process = FALSE;
typedef BOOL (WINAPI * PFN_IsWow64Process) (HANDLE h_rocess, PBOOL wow64_process);
PFN_IsWow64Process  pfnIsWow64Process = NULL;

// File System Redirection on Win64
void * fs_redirection_value = NULL;
typedef BOOL (WINAPI * PFN_Wow64DisableWow64FsRedirection) (void * * old_value);
typedef BOOL (WINAPI * PFN_Wow64RevertWow64FsRedirection) (void * old_value);
PFN_Wow64DisableWow64FsRedirection  pfnWow64DisableWow64FsRedirection = NULL;
PFN_Wow64RevertWow64FsRedirection   pfnWow64RevertWow64FsRedirection = NULL;
#endif

#if defined(_LINUX)
extern pthread_mutex_t harvester_mutex;     // Serialize access to harvester
#endif

#if defined(_ZOS)
extern CB * pCB;                            // ptr to binder Control Block
#endif

char * AdditionalSpecialDlls = NULL;        // Additional special Windows DLLs.
char * AdditionalSearchPath = NULL;         // User-specified search path

char a2n_err_file[MAX_FILENAME_LEN];
char a2n_mod_file[MAX_FILENAME_LEN];
char a2n_proc_file[MAX_FILENAME_LEN];
char a2n_msi_file[MAX_FILENAME_LEN];

//
// Internal prototypes
//
static void GetEnvVariables(void);
static void InitOsSpecificStuff(void);
void Initialize(void);
static void Terminate(void);
static void DumpGlobals(FILE * fh);




//
// .a/.so/.dll initialization/termination
// **************************************
//
// * For this to work onr AIX, you must specify -binitfini:_init:_fini
//
#if defined(_LINUX) || defined(_AIX)
#if defined(__GNUC__) || defined(AUTOMAKE)
void __attribute__ ((constructor)) a2n_init(void)
#else
void _init(void)
#endif
{
   Initialize();
   return;
}

#if defined(__GNUC__) || defined(AUTOMAKE)
void __attribute__ ((destructor)) a2n_fini(void)
#else
void _fini(void)
#endif
{
   Terminate();
   return;
}
#endif

#if defined(_WINDOWS)
BOOL WINAPI DllMain (HINSTANCE handle, DWORD reason, LPVOID situation)
{
   switch (reason) {
      case DLL_PROCESS_ATTACH:
         Initialize();
         break;

      case DLL_PROCESS_DETACH:
         Terminate();
         break;

      case DLL_THREAD_ATTACH:
      case DLL_THREAD_DETACH:
         break;

      default:
         printf("a2n.dll: ***** failed to load *****\n"); fflush(stdout);
         return (FALSE);                    // Don't load - invalid reason
   }

   return (TRUE);
}


void DumpDriveMapping(FILE * fh)
{
   int i;

   for (i = 0; i < 26; i++) {
      if (dev_name[i][0] != 0) {
         if (dev_type[i] == DEVICE_NETWORK_DRIVE)
            FilePrint(fh, "Drive %s \\%s  (%s)\n", drv_name[i], dev_name[i], rdev_name[i]);
         else if (dev_type[i] == DEVICE_SUBST_DRIVE)
            FilePrint(fh, "Drive %s %s  (%s)\n", drv_name[i], dev_name[i], rdev_name[i]);
         else
            FilePrint(fh, "Drive %s %s\n", drv_name[i], dev_name[i]);
      }
   }
   return;
}


void get_file_version(char * fn, int * major, int * minor, int * rev, int * spin)
{
   DWORD vi_size;
   DWORD h;
   void * vi = NULL;
   VS_FIXEDFILEINFO * pffi;


   if (major) *major = -1;
   if (minor) *minor = -1;
   if (rev)   *rev = 0;
   if (spin)  *spin = 0;

   vi_size = GetFileVersionInfoSize(fn, &h);
   if (vi_size) {
      vi = (void *)zmalloc(vi_size);
      if (GetFileVersionInfo(fn, h, vi_size, vi)) {
         if (VerQueryValue(vi, "\\", (void *)&pffi, (UINT *)&vi_size)) {
            if (major) *major = (pffi->dwFileVersionMS & 0xFFFF0000) >> 16;
            if (minor) *minor = (pffi->dwFileVersionMS & 0x0000FFFF);
            if (rev)   *rev = (pffi->dwFileVersionLS & 0xFFFF0000) >> 16;
            if (spin)  *spin = (pffi->dwFileVersionLS & 0x0000FFFF);
         }
      }
      zfree(vi);
   }

   return;
}


BOOL is_process_wow64(void)
{
   BOOL is_wow64 = FALSE;

   if (pfnIsWow64Process) {
      if (!pfnIsWow64Process(GetCurrentProcess(), &is_wow64))
         is_wow64 = FALSE;                  // Error. Assume not Wow64
   }

   return (is_wow64);
}
#endif


//
// Initialize()
// ************
//
// Perform dll/so initialization.
//
void Initialize(void)
{
   int i;
   char * cd;
   char * p, * op;
   uint pid;
#if defined(_WINDOWS) && defined(_32BIT)
   HMODULE h_kernel32 = NULL;
#endif
#if defined(_LINUX)
   pthread_mutexattr_t harvester_mutex_attr;
#endif


   if (initialized)
      return;

#if defined(DEBUG) && defined(_AIX)
   printf("Initializing liba2n.a built on %s", BUILDDATE);
#endif

   initialized = 1;

   memset(&a2n_gv, 0, sizeof(a2n_gv));      // Clear gv.  Initializes all pointers to NULL.
   memset(&api_lock, 0, sizeof(api_lock));  // Clear API lockout flags
   memset(&api_cnt, 0, sizeof(api_cnt));    // Clear API counters
   pid = GET_PID();

   //<hack>
   // Check if output is going someplace other than the current directory
   op = getenv("A2N_OUTPUT_PATH");
   if (op == NULL)
      op = getenv("a2n_output_path");

   if (op)
      G(output_path) = zstrdup(op);
   else
      G(output_path) = NULL;

   // Check if we need to separate output files by process
   p = getenv("A2N_SEPARATE_OUTPUT");
   if (p == NULL)
      p = getenv("a2n_separate_output");

   if (p) {
      G(append_pid_to_fn) = 1;

      if (op) {
         sprintf(a2n_err_file,  "%s%s%s_%d", op, DEFAULT_PATH_SEP_STR, A2N_MESSAGE_FILE, pid);
         sprintf(a2n_mod_file,  "%s%s%s_%d", op, DEFAULT_PATH_SEP_STR, A2N_MODULE_DUMP_FILE, pid);
         sprintf(a2n_proc_file, "%s%s%s_%d", op, DEFAULT_PATH_SEP_STR, A2N_PROCESS_DUMP_FILE, pid);
         sprintf(a2n_msi_file,  "%s%s%s_%d", op, DEFAULT_PATH_SEP_STR, A2N_MSI_DUMP_FILE, pid);
      }
      else {
         sprintf(a2n_err_file,  "%s_%d", A2N_MESSAGE_FILE, pid);
         sprintf(a2n_mod_file,  "%s_%d", A2N_MODULE_DUMP_FILE, pid);
         sprintf(a2n_proc_file, "%s_%d", A2N_PROCESS_DUMP_FILE, pid);
         sprintf(a2n_msi_file,  "%s_%d", A2N_MSI_DUMP_FILE, pid);
      }
   }
   else {
      G(append_pid_to_fn) = 0;

      if (op) {
         sprintf(a2n_err_file,  "%s%s%s", op, DEFAULT_PATH_SEP_STR, A2N_MESSAGE_FILE);
         sprintf(a2n_mod_file,  "%s%s%s", op, DEFAULT_PATH_SEP_STR, A2N_MODULE_DUMP_FILE);
         sprintf(a2n_proc_file, "%s%s%s", op, DEFAULT_PATH_SEP_STR, A2N_PROCESS_DUMP_FILE);
         sprintf(a2n_msi_file,  "%s%s%s", op, DEFAULT_PATH_SEP_STR, A2N_MSI_DUMP_FILE);
      }
      else {
         strcpy(a2n_err_file,  A2N_MESSAGE_FILE);
         strcpy(a2n_mod_file,  A2N_MODULE_DUMP_FILE);
         strcpy(a2n_proc_file, A2N_PROCESS_DUMP_FILE);
         strcpy(a2n_msi_file,  A2N_MSI_DUMP_FILE);
      }
   }
   //</hack>

   // Open messages file right away ...
   remove(a2n_err_file);
   G(msgfh) = fopen(a2n_err_file, "w");
   if (G(msgfh) == NULL) {
      msg_se("*E* A2N: Unable to open \"%s\". Messages will go to stderr.\n", a2n_err_file);
      G(msgfh) = stderr;
   }
   msg_log("********** %s A2N Version %d.%d.%d **********\n", _PLATFORM_STR, V_MAJOR, V_MINOR, V_REV);

   cd = GetCurrentWorkingDirectory();
   if (cd) {
      msg_log("********** Current directory: %s\n", cd);
      zfree(cd);
   }

   msg_log("********** Process Id: %d (0x%0X)\n\n", pid, pid);

   // Initialize globals
   for (i = 0; i < PID_HASH_TABLE_SIZE; i++) {
      G(PidTable[i]) = NULL;
   }
   G(PidRoot)                  = NULL;
   G(PidCnt)                   = 0;
   G(ModRoot)                  = NULL;
   G(ModCnt)                   = 0;
   G(add_module_cache_head)    = 1;         // Add to head of cached module list
   G(JmPoolRoot)               = NULL;
   G(JmPoolCnt)                = 0;
   G(SystemPid)                = SYSTEM_PID;
   G(SystemPidNode)            = NULL;
   G(ReusedPidNode)            = NULL;
   G(ReusedPidCnt)             = 0;
   G(SystemPidSet)             = 0;
   G(kernel_not_seen)          = 1;
   G(big_endian)               = A2N_BIG_ENDIAN;
   G(word_size)                = A2N_WORD_SIZE;
   G(range_desc_root)          = NULL;

   G(PathSeparator)            = DEFAULT_PATH_SEP_CHAR;
   G(PathSeparator_str)        = DEFAULT_PATH_SEP_STR;
   G(SearchPathSeparator)      = DEFAULT_SRCH_PATH_SEP_CHAR;
   G(SearchPathSeparator_str)  = DEFAULT_SRCH_PATH_SEP_STR;
   G(KernelFilename)           = DEFAULT_KERNEL_FILENAME;
   G(UnknownName)              = UNKNOWN_NAME;
   G(using_static_module_info) = 0;
   G(changes_made)             = 0;

   G(msi_file_wordsize)        = -1;
   G(msi_file_bigendian)       = -1;
   G(msi_file_os)              = -1;

   // These can be modified via environment variables
   G(KernelImageName)          = zstrdup(DEFAULT_KERNEL_IMAGENAME);
   G(use_kernel_image)         = 0;  // Not until told to do so
   G(ok_to_use_kernel_image)   = 1;  // OK if told to use it
   G(KernelMapName)            = zstrdup(DEFAULT_KERNEL_MAPNAME);
   G(use_kernel_map)           = 0;  // Not until told to do so
   G(ok_to_use_kernel_map)     = 1;  // OK if told to use it
   G(KallsymsFilename)         = zstrdup(DEFAULT_KALLSYMS_FILENAME);
   G(use_kallsyms)             = 0;  // Not until told to do so
   G(ok_to_use_kallsyms)       = 1;  // OK if told to use it
   G(kallsyms_file_offset)     = 0;
   G(kallsyms_file_size)       = 0;  // entire file
   G(ModulesFilename)          = zstrdup(DEFAULT_MODULES_FILENAME);
   G(use_modules)              = 0;  // Not until told to do so
   G(ok_to_use_modules)        = 1;  // OK if told to use it
   G(modules_file_offset)      = 0;
   G(modules_file_size)        = 0;  // entire file
   G(kernel_modules_added)     = 0;
   G(kernel_modules_harvested) = 0;
   G(debug)                    = DEBUG_MODE_NONE;
   G(dump_sd)                  = 0;
   G(display_error_messages)   = MSG_ERROR;
   G(immediate_gather)         = MODE_LAZY;
   G(gather_code)              = MODE_OFF;
   G(validate_symbols)         = MODE_ON;
   G(quit_on_validation_error) = MODE_OFF;
   G(demangle_cpp_names)       = MODE_ON;
   G(demangle_complete)        = 0;  // Only method name
   G(rename_duplicate_symbols) = 0;
   G(do_fast_lookup)           = 1;
   G(st_mode)                  = 1;  // Default is single-threaded mode
   G(mt_mode)                  = 0;  // Default is *not* multi-threaded mode
   G(collapse_ibm_mmi)         = 0;
   G(return_range_symbols)     = 0;
   G(nsf_action)               = NSF_ACTION_NSF;
   G(nsf_threshold)            = INT_MAX;
   G(nmf_threshold)            = INT_MAX;
   G(symbol_quality)           = MODE_GOOD_SYMBOLS;
   G(harvest_exports_only)     = 0;
   G(always_harvest)           = 0;
   G(snp_elements)             = SNP_ELEMENTS;
   G(snp_size)                 = SNP_SIZE;
   G(module_dump_on_exit)      = MODE_OFF;
   G(process_dump_on_exit)     = MODE_OFF;
   G(msi_dump_on_exit)         = MODE_OFF;
   G(blank_sub)                = DEFAULT_BLANK_SUB;
   G(collapsed_mmi_range_name) = DEFAULT_MMI_RANGE_NAME;
   G(jvmmap_fn)                = NULL;
   G(lineno)                   = 0;
   G(ignore_ts_cs)             = 0;
   G(symopt_debug)             = 0;
   G(symopt_load_anything)     = 0;

   GetEnvVariables();
   InitOsSpecificStuff();

#if defined(_WINDOWS)
   // Initialize harvester critical section
   InitializeCriticalSection(&harvester_cs);

   // Dump the device mappings
   DumpDriveMapping(G(msgfh));

   // Load dbghelp.dll
   find_good_dbghelp();

#if defined(_32BIT)
   //
   // This is here to allow a 32-bit a2n.dll, running on a 64-bit OS, to harvest
   // all symbols. Win64 turns on File System Redirection for 32-bit processes.
   // All requests to the %System32% directory are redirected to %SysWOW64%.
   // If we don't disable redirection then we'll never find symbols for 64-bit
   // images in the "real" %System32% directory.
   //
   h_kernel32 = GetModuleHandle("kernel32");
   if (h_kernel32 != NULL) {
      pfnIsWow64Process = (PFN_IsWow64Process)GetProcAddress(h_kernel32, "IsWow64Process");
      pfnWow64DisableWow64FsRedirection = (PFN_Wow64DisableWow64FsRedirection)GetProcAddress(h_kernel32, "Wow64DisableWow64FsRedirection");
      pfnWow64RevertWow64FsRedirection   = (PFN_Wow64RevertWow64FsRedirection)GetProcAddress(h_kernel32,  "Wow64RevertWow64FsRedirection");
   }

   wow64_process = is_process_wow64();
   if (wow64_process) {
      msg_log("\n*** Wow64DisableWow64FsRedirection = 0x%"_PZP"\n", pfnWow64DisableWow64FsRedirection);
      msg_log("*** Wow64RevertWow64FsRedirection  = 0x%"_PZP"\n", pfnWow64RevertWow64FsRedirection);
      if (pfnWow64DisableWow64FsRedirection) {
         if (!pfnWow64DisableWow64FsRedirection(&fs_redirection_value)) {
            warnmsg(("*W* Unable to turn off file system redirectiom. Wow64DisableWow64FsRedirection() rc = %d\n", GetLastError()));
         }
      }
   }
#endif

   msg_log("\n*** SymbolSearchPath      = '%s'\n", G(SymbolSearchPath));
   msg_log("*** SymbolSearchPathWow64 = '%s'\n\n", G(SymbolSearchPathWow64));

#endif

#if defined(_LINUX)
   // Initialize harvester mutex
   pthread_mutexattr_init(&harvester_mutex_attr);
   pthread_mutexattr_settype(&harvester_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutexattr_setpshared(&harvester_mutex_attr, PTHREAD_PROCESS_PRIVATE);
   pthread_mutex_init(&harvester_mutex, &harvester_mutex_attr);
#endif

#if defined(_ZOS)
   pCB = (CB *) getMemory(sizeof(CB), "A2nInit() initCB");
   initCB(pCB);

   //
   // The following set of functions are invoked once per execution of post module.
   // - load the binder into memory.
   // - Allocate memory for binder API calls below the bar.
   // - start a binder dialog, create a workmod
   // API descriptionm found in manual:
   // MVS Program Management: Advanced Facilities  SA22-7644
   //
   loadBinder(pCB);
   allocCommonMemmory();
   startd(pCB);
   createw(pCB);
#endif

   // Allocate SN pointer array
   snp = (SYM_NODE **)zmalloc(G(snp_size));
   if (snp == NULL) {
      msg_se("*E* A2N: Unable to malloc %d bytes for SNP array. Quitting!\n", G(snp_size));
      exit (-1);
   }

   // Allocate DSN pointer array
   if (G(rename_duplicate_symbols)) {
      dsnp = (SYM_NODE **)zmalloc(G(snp_size));
      if (dsnp == NULL) {
         msg_se("*E* A2N: Unable to malloc %d bytes for DSNP array. Quitting!\n", G(snp_size));
         exit (-1);
      }
   }
   else {
      dsnp = NULL;
   }

   // Done
   return;
}


//
// Terminate()
// ***********
//
// Perform dll/so termination
//
static void Terminate(void)
{
   //
   // Force Module and Process dumps if:
   // * Threshold of A2nGetSymbol() failures with rc=A2N_NO_MODULE exceeded.
   // * Threshold of A2nGetSymbol() failures with rc=A2N_NO_SYMBOL_FOUND exceeded.
   //
   if ( (G(nmf_threshold) >= 0  &&  api_cnt.GetSymbol_NoModule >= G(nmf_threshold)) ||
        (G(nsf_threshold) >= 0  &&  api_cnt.GetSymbol_NoSymbolFound >= G(nsf_threshold)) ) {
      G(module_dump_on_exit) = 1;           // Force module/symbol dump
      G(process_dump_on_exit) = 1;          // Force process dump
   }

   // Dump modules/processes if requested
   if (G(module_dump_on_exit))
      A2nDumpModules(a2n_mod_file);

   if (G(process_dump_on_exit))
      A2nDumpProcesses(a2n_proc_file);

   if (G(msi_dump_on_exit))
      A2nCreateMsiFile(a2n_msi_file);

#if defined(_WINDOWS)
   // Free up harvester critical section
   DeleteCriticalSection(&harvester_cs);

   // Remove uncompressed jvmmap(s) if necessary
   RemoveJvmmaps();

#if defined(_32BIT)
   //
   // If we're a WOW64 process, and successfully disabled File System Redirection
   // then re-enable it. I believe it doesn't matter much whether we re-enable
   // or not since the process is going away.
   //
   if (wow64_process) {
      if (pfnWow64RevertWow64FsRedirection && fs_redirection_value != NULL) {
         if (!pfnWow64RevertWow64FsRedirection(fs_redirection_value)) {
            warnmsg(("*W* Unable to restore file system redirectiom. Wow64RevertWow64FsRedirection() rc = %d\n", GetLastError()));
         }
      }
   }
#endif

   if (hDbgHelp)
      FreeLibrary(hDbgHelp);
#endif

   DumpGlobals(G(msgfh));

#if defined(_LINUX)
   // Free up harvester mutex
   pthread_mutex_destroy(&harvester_mutex);
#endif

#if defined(_ZOS)
   deletew(pCB);                            // delete workmod
   endd(pCB);                               // end  binder dialog
   deleteBinder(pCB);                       // delete binder load module
   free(pCB);                               // delete binder control block
#endif

   // Free SN array
   if (snp != NULL)
      zfree(snp);

   // Free DSN array
   if (dsnp != NULL)
      zfree(dsnp);

   // Flush and close the message file
   fflush(G(msgfh));
   if (G(msgfh) != stderr)
      fclose(G(msgfh));

   return;
}


//
// GetEnvVariables()
// *****************
//
// Gets environment variable values and update some defaults.
// Make sure changes are documented in A2nListControls().
//
static void GetEnvVariables(void)
{
   char * p;
   int rc;


   //
   // See if they want their own default value for debug level.
   //
   p = getenv("A2N_DEBUG");                 // The official one
   if (p == NULL) {
      p = getenv("a2n_debug");
      if (p == NULL) {
         p = getenv("A2NDEBUG");            // This one's for Jimmy
         if (p == NULL) {
            p = getenv("a2ndebug");
         }
      }
   }
   if (p != NULL) {
      api_lock.ErrorMessageMode = 1;        // Don't let API change it
      G(display_error_messages) = MSG_ALL;  // Display all messages

      api_lock.DebugMode = 1;               // Don't let API change it
      if ( (stricmp(p, "off") == 0) ||
           (stricmp(p, "no") == 0) ||
           (stricmp(p, "stop") == 0) ) {    // Nothing
         G(debug) = DEBUG_MODE_NONE;
      }
      else                           {      // Everything
         G(debug) = DEBUG_MODE_ALL;
         G(module_dump_on_exit) = MODE_ON;
         G(process_dump_on_exit) = MODE_ON;
      }
   }

   p = getenv("A2N_DEBUG_API");
   if (p == NULL) {
      p = getenv("a2n_debug_api");
   }
   if (p != NULL) {
      api_lock.ErrorMessageMode = 1;        // Don't let API change it
      G(display_error_messages) = MSG_ALL;  // Display all messages

      api_lock.DebugMode = 1;               // Don't let API change it
      if ( (stricmp(p, "off") == 0) || (stricmp(p, "no") == 0) ) {  // Nothing
         G(debug) = DEBUG_MODE_NONE;
      }
      else {                                // API entry/exit or invalid - set to API
         G(debug) = (DEBUG_MODE_API | DEBUG_MODE_INTERNAL);
         G(module_dump_on_exit) = MODE_ON;
         G(process_dump_on_exit) = MODE_ON;
      }
   }

   p = getenv("A2N_DEBUG_HV");
   if (p == NULL) {
      p = getenv("a2n_debug_hv");
   }
   if (p != NULL) {
      api_lock.ErrorMessageMode = 1;        // Don't let API change it
      G(display_error_messages) = MSG_ALL;  // Display all messages

      api_lock.DebugMode = 1;               // Don't let API change it
      if ( (stricmp(p, "off") == 0) || (stricmp(p, "no") == 0) ) {  // Nothing
         G(debug) = DEBUG_MODE_NONE;
      }
      else {                                // Harvester
         G(debug) = DEBUG_MODE_HARVESTER;
         G(module_dump_on_exit) = MODE_ON;
         G(process_dump_on_exit) = MODE_ON;
      }
   }

   //
   // See if we want to dump SYMDATA before and after each GetSymbol()
   // Only valid if any kind of DEBUG option is set.
   //
   p = getenv("A2N_DUMP_SD");
   if (p == NULL) {
      p = getenv("a2n_dump_sd");
   }
   if (p != NULL  &&  G(debug) != DEBUG_MODE_NONE) {
      if (stricmp(p, "no") == 0)
         G(dump_sd) = 0;
      else
         G(dump_sd) = 1;
   }

   //
   // See if they want to set the types of error messages to display.
   // *** Only bother if A2N_DEBUG hasn't already locked the mode.
   //
   if (api_lock.ErrorMessageMode == 0) {
      p = getenv("A2N_ERROR_MESSAGES");
      if (p == NULL) {
         p = getenv("a2n_error_messages");
      }
      if (p != NULL) {
         api_lock.ErrorMessageMode = 1;        // Don't let API change it
         if (stricmp(p, "none") == 0)          // Nothing
            G(display_error_messages) = MSG_NONE;
         else if (stricmp(p, "info") == 0)     // Informational and above
            G(display_error_messages) = MSG_INFO;
         else if (stricmp(p, "warn") == 0)     // Warnings and above
            G(display_error_messages) = MSG_WARN;
         else if (stricmp(p, "error") == 0)    // Errors and above
            G(display_error_messages) = MSG_ERROR;
         else                                  // All or invalid - set to everything
            G(display_error_messages) = MSG_ALL;
      }
   }

   //
   // See if they want us to gather symbols when modules are added.
   //
   p = getenv("A2N_IMMEDIATE_GATHER");
   if (p == NULL) {
      p = getenv("a2n_immediate_gather");
   }
   if (p != NULL) {
      api_lock.SymbolGatherMode = 1;        // Don't let API change it
      if (stricmp(p, "no") == 0)
         G(immediate_gather) = MODE_LAZY;
      else
         G(immediate_gather) = MODE_IMMEDIATE;
   }

   //
   // See if they want us to attempt to gather code when gathering symbols.
   //
   p = getenv("A2N_GATHER_CODE");
   if (p == NULL) {
      p = getenv("a2n_gather_code");
   }
   if (p != NULL){
      api_lock.GatherCodeMode = 1;          // Don't let API change it
      if (stricmp(p, "no") == 0)
         G(gather_code) = MODE_OFF;
      else
         G(gather_code) = MODE_ON;
   }

   //
   // See if they want us to dump modules on exit.
   //
   p = getenv("A2N_MODULE_DUMP");
   if (p == NULL) {
      p = getenv("a2n_module_dump");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)
         G(module_dump_on_exit) = MODE_OFF;
      else
         G(module_dump_on_exit) = MODE_ON;
   }

   //
   // See if they want us to dump processes on exit.
   //
   p = getenv("A2N_PROCESS_DUMP");
   if (p == NULL) {
      p = getenv("a2n_process_dump");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)
         G(process_dump_on_exit) = MODE_OFF;
      else
         G(process_dump_on_exit) = MODE_ON;
   }

   //
   // See if they want us to create an msi file on exit.
   //
   p = getenv("A2N_MSI_DUMP");
   if (p == NULL) {
      p = getenv("a2n_msi_dump");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)
         G(msi_dump_on_exit) = MODE_OFF;
      else
         G(msi_dump_on_exit) = MODE_ON;
   }

   //
   // See if they want us to not validate symbols.
   //
   p = getenv("A2N_DONT_VALIDATE");
   if (p == NULL) {
      p = getenv("a2n_dont_validate");
   }
   if (p != NULL) {
      api_lock.ValidateSymbolsMode = 1;     // Don't let API change it
      if (stricmp(p, "no") == 0)
         G(validate_symbols) = MODE_ON;
      else
         G(validate_symbols) = MODE_OFF;
   }

#if defined (_LINUX)
   //
   // See if they want us to quit when validation fails.
   //
   p = getenv("A2N_QUIT_ON_VALIDATION_ERROR");
   if (p == NULL) {
      p = getenv("a2n_quit_on_validation_error");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)
         G(quit_on_validation_error) = MODE_OFF;
      else
         G(quit_on_validation_error) = MODE_ON;
   }
#endif

   //
   // See if they want to change how modules are added to the cached module list.
   // If they specify *both* ways then TAIL wins.
   //
   p = getenv("A2N_ADD_MODULE_CACHE_HEAD");
   if (p == NULL) {
      p = getenv("a2n_add_module_cache_head");
   }
   if (p != NULL) {
      G(add_module_cache_head) = MODE_ON;
   }

   p = getenv("A2N_ADD_MODULE_CACHE_TAIL");
   if (p == NULL) {
      p = getenv("a2n_add_module_cache_tail");
   }
   if (p != NULL) {
      G(add_module_cache_head) = MODE_OFF;
   }

   //
   // See if they don't want us to demangle c++ names.
   //
   p = getenv("A2N_DONT_DEMANGLE");
   if (p == NULL) {
      p = getenv("a2n_dont_demangle");
   }
   if (p != NULL) {
      api_lock.DemangleMode = 1;            // Don't let API change it
      if (stricmp(p, "no") == 0)
         G(demangle_cpp_names) = MODE_ON;
      else
         G(demangle_cpp_names) = MODE_OFF;
   }

   //
   // See how they want names demangled.
   //
   p = getenv("A2N_DEMANGLE_COMPLETE");
   if (p == NULL) {
      p = getenv("a2n_demangle_complete");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)
         G(demangle_complete) = 0;
      else
         G(demangle_complete) = 1;
   }

   //
   // See if they want duplicate names renamed to unique names
   //
   p = getenv("A2N_RENAME_DUPLICATE_SYMBOLS");
   if (p == NULL) {
      p = getenv("a2n_rename_duplicate_symbols");
   }
   if (p != NULL) {
      api_lock.RenameDuplicateSymbolsMode = 1; // Don't let API change it
      if (stricmp(p, "no") == 0)
         G(rename_duplicate_symbols) = 0;
      else
         G(rename_duplicate_symbols) = 1;
   }

   //
   // See if they want to change the NoSymbolFound action.
   //
   p = getenv("A2N_NSF_ACTION");
   if (p == NULL) {
      p = getenv("a2n_nsf_action");
   }
   if (p != NULL) {
      api_lock.NoSymbolFoundAction = 1;     // Don't let API change it
      if (stricmp(p, "null") == 0)          // No symbol (ie. NULL symbol name)
         G(nsf_action) = NSF_ACTION_NOSYM;
      else if (stricmp(p, "section") == 0)  // Return section name
         G(nsf_action) = NSF_ACTION_SECTION;
      else                                  // NSF or invalid - return "NoSymbolFound"
         G(nsf_action) = NSF_ACTION_NSF;
   }

   //
   // See if they want to change the default NoModuleFound dump threshold.
   // Value must be a number.
   //
   p = getenv("A2N_NOMOD_THRESHOLD");
   if (p == NULL) {
      p = getenv("a2n_nomod_threshold");
   }
   if (p != NULL) {
      api_lock.NoModuleFountThreshold = 1;  // Don't let API change it
      G(nmf_threshold) = atoi(p);
   }

   //
   // See if they want to change the default NoSymbolFound dump threshold.
   // Value must be a number.
   //
   p = getenv("A2N_NOSYM_THRESHOLD");
   if (p == NULL) {
      p = getenv("a2n_nosym_threshold");
   }
   if (p != NULL) {
      api_lock.NoSymbolFountThreshold = 1;  // Don't let API change it
      G(nsf_threshold) = atoi(p);
   }

   //
   // See if they want to change the default NoSymbolFound dump threshold.
   // Value must be a number.
   //
   p = getenv("A2N_SYMBOL_QUALITY");
   if (p == NULL) {
      p = getenv("a2n_symbol_quality");
   }
   if (p != NULL) {
      api_lock.SymbolQuality = 1;           // Don't let API change it
      if (stricmp(p, "debug") == 0 || stricmp(p, "good") == 0)  // Good symbols only
         G(symbol_quality) = MODE_GOOD_SYMBOLS;
      else                                  // Anything
         G(symbol_quality) = MODE_ANY_SYMBOL;
   }

   //
   // See if they want to return line number information
   //
   p = getenv("A2N_LINENO");
   if (p == NULL) {
      p = getenv("a2n_lineno");
   }
   if (p != NULL) {
      api_lock.ReturnLineNumbersMode = 1;   // Don't let API change it
      if (stricmp(p, "no") == 0 || stricmp(p, "off") == 0)  // No
         G(lineno) = 0;
      else                                  // Anything else means Yes
         G(lineno) = 1;
   }

#if defined (_WINDOWS)
   //
   // See if they want to ignore TS/CS when checking if on-disk imaage
   // is OK to use.
   //
   p = getenv("A2N_IGNORE_TSCS");
   if (p == NULL) {
      p = getenv("a2n_ignore_tscs");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0 || stricmp(p, "off") == 0)  // No
         G(ignore_ts_cs) = 0;
      else                                  // Anything else means Yes
         G(ignore_ts_cs) = 1;
   }

   //
   // See if they want force A2N to only harvest exports (and nothing else).
   //
   p = getenv("A2N_EXPORTS_ONLY");
   if (p == NULL) {
      p = getenv("a2n_exports_only");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)
         G(harvest_exports_only) = 0;
      else
         G(harvest_exports_only) = 1;
   }

   //
   // See if they want to set DbgHelp.dll options.
   //
   p = getenv("A2N_SYMOPT_DEBUG");
   if (p == NULL) {
      p = getenv("a2n_symopt_debug");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)
         G(symopt_debug) = 0;
      else
         G(symopt_debug) = 1;
   }

   p = getenv("A2N_SYMOPT_LOAD_ANYTHING");
   if (p == NULL) {
      p = getenv("a2n_load_anything");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)
         G(symopt_load_anything) = 0;
      else
         G(symopt_load_anything) = 1;
   }
#endif

   //
   // See if they want to always harvest symbols even if they're bad quality.
   //
   p = getenv("A2N_ALWAYS_HARVEST");
   if (p == NULL) {
      p = getenv("a2n_always_harvest");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)
         G(always_harvest) = 0;
      else
         G(always_harvest) = 1;
   }

   //
   // See if they want change the size of the SN array.
   // Must be a valid, non-zero number, greater than 1000.
   // The number indicates the number of elements in the SN array.
   //
   p = getenv("A2N_SNP_ELEMENTS");
   if (p == NULL) {
      p = getenv("a2n_snp_elements");
   }
   if (p != NULL) {
      G(snp_elements) = atoi(p);
      if (G(snp_elements) < 1000) {
         G(snp_elements) = SNP_ELEMENTS;
         G(snp_size) = SNP_SIZE;
      }
      else {
         G(snp_size) = G(snp_elements) * sizeof(SYM_NODE *);
      }
   }

   //
   // See if they want to add to the Symbol Search Path.
   // Whatever they enter will be put in front of the search path.
   // Value must be a search path string.  No error checking.
   //
   p = getenv("A2N_SEARCH_PATH");
   if (p == NULL) {
      p = getenv("a2n_search_path");
   }
   if (p != NULL) {
      AdditionalSearchPath = zstrdup(p);    // Save the path
   }

#if defined (_LINUX)
   //
   // See if they want to set the kernel image name.
   // Value must be a search path string.  No error checking.
   // ***** Not used on Windows *****
   //
   p = getenv("A2N_KERNEL_IMAGE_NAME");
   if (p == NULL) {
      p = getenv("a2n_kernel_image_name");
   }
   if (p != NULL) {
      rc = verify_kernel_image_name(p);
      if (rc == 0)
         api_lock.SetKernelImageName = 1;   // Don't let API change it
   }

   //
   // See if they want to set the kernel map name.
   // Value must be a search path string.  No error checking.
   // ***** Not used on Windows *****
   //
   p = getenv("A2N_KERNEL_MAP_NAME");
   if (p == NULL) {
      p = getenv("a2n_kernel_map_name");
   }
   if (p != NULL) {
      rc = verify_kernel_map_name(p);
      if (rc == 0)
         api_lock.SetKernelMapName = 1;     // Don't let API change it
   }

   //
   // See if they want to set the kallsyms file information.
   // ***** Not used on Windows *****
   //
   p = getenv("A2N_KALLSYMS_OFFSET");
   if (p == NULL) {
      p = getenv("a2n_kallsyms_offset");
   }
   if (p != NULL) {
      G(kallsyms_file_offset) = atoll(p);;
   }

   p = getenv("A2N_KALLSYMS_SIZE");
   if (p == NULL) {
      p = getenv("a2n_kallsyms_size");
   }
   if (p != NULL) {
      G(kallsyms_file_size) = atoll(p);;
   }

   p = getenv("A2N_KALLSYMS_NAME");
   if (p == NULL) {
      p = getenv("a2n_kallsyms_name");
   }
   if (p != NULL) {
      rc = verify_kallsyms_file_info(p, G(kallsyms_file_offset), G(kallsyms_file_size));
      if (rc == 0)
         api_lock.SetKallsymsLocation = 1;  // Don't let API change it
   }

   //
   // See if they want to set the modules file information.
   // ***** Not used on Windows *****
   //
   p = getenv("A2N_MODULES_OFFSET");
   if (p == NULL) {
      p = getenv("a2n_modules_offset");
   }
   if (p != NULL) {
      G(modules_file_offset) = atoll(p);;
   }

   p = getenv("A2N_MODULES_SIZE");
   if (p == NULL) {
      p = getenv("a2n_modules_size");
   }
   if (p != NULL) {
      G(modules_file_size) = atoll(p);;
   }

   p = getenv("A2N_MODULES_NAME");
   if (p == NULL) {
      p = getenv("a2n_modules_name");
   }
   if (p != NULL) {
      rc = verify_modules_file_info(p, G(modules_file_offset), G(modules_file_size));
      if (rc == 0)
         api_lock.SetModulesLocation = 1;   // Don't let API change it
   }

   //
   // See if they want to tell us where *NOT* to get kernel symbols from.
   // These are just environment variable overrides to we can force where
   // we want the symbols from, regardless of what the APIs (or other
   // environment variables) say.
   // ***** Not used on Windows *****
   //
   p = getenv("A2N_DONT_USE_KERNEL_IMAGE");
   if (p == NULL) {
      p = getenv("a2n_dont_use_kernel_image");
   }
   if (p != NULL) {
      G(ok_to_use_kernel_image) = 0;
   }

   p = getenv("A2N_DONT_USE_KERNEL_MAP");
   if (p == NULL) {
      p = getenv("a2n_dont_use_kernel_map");
   }
   if (p != NULL) {
      G(ok_to_use_kernel_map) = 0;
   }

   p = getenv("A2N_DONT_USE_KALLSYMS");
   if (p == NULL) {
      p = getenv("a2n_dont_use_kallsyms");
   }
   if (p != NULL) {
      G(ok_to_use_kallsyms) = 0;
   }

   p = getenv("A2N_DONT_USE_MODULES");
   if (p == NULL) {
      p = getenv("a2n_dont_use_modules");
   }
   if (p != NULL) {
      G(ok_to_use_modules) = 1;
   }
#endif

   //
   // See if they want to change blanks to any other character in
   // symbol names.  Whatever the *FIRST* character is following the
   // equals sign is used to substitute blanks.
   // Value must be a single character.  No error checking.
   //
   p = getenv("A2N_BLANK_SUB_CHAR");
   if (p == NULL) {
      p = getenv("a2n_blank_sub_char");
   }
   if (p != NULL) {
      G(blank_sub) = *p;                    // Save the first character
   }

   //
   // See if they want to disable Fast Symbol lookups.
   //
   p = getenv("A2N_NO_FAST_LOOKUP");
   if (p == NULL) {
      p = getenv("a2n_no_fast_lookup");
   }
   if (p != NULL) {
      if (stricmp(p, "no") == 0)            // Do fast lookups (default)
         G(do_fast_lookup) = 1;
      else                                  // Yes/Invalid. Do slow lookups.
         G(do_fast_lookup) = 0;
   }

#if defined(_X86)
   //
   // See if they want to collapse the java MMI Interpreter symbol range
   //
   p = getenv("A2N_COLLAPSE_MMI");          // Don't let API change it
   if (p == NULL) {
      p = getenv("a2n_collapse_mmi");          // Don't let API change it
   }
   if (p != NULL) {
      api_lock.SetCollapseMMIRange = 1;
      if (stricmp(p, "no") == 0)            // Don't collapse
         G(collapse_ibm_mmi) = 0;
      else                                  // Collapse
         G(collapse_ibm_mmi) = 1;
   }

   //
   // See if they want to name the collapsed MMI Interpreter symbol range
   //
   p = getenv("A2N_MMI_RANGE_NAME");
   if (p == NULL) {
      p = getenv("a2n_mmi_range_name");
   }
   if (p != NULL) {
      api_lock.SetMMIRangeName = 1;         // Don't let API change it
      G(collapsed_mmi_range_name) = zstrdup(p);  // Save the name
   }
#endif

   //
   // See if they want to collapse a symbol range
   // Value must be a string with at least 3 values separated by blanks,
   // such as: set A2N_COLLAPSE_RANGE=value1 value2 value3 value4
   // where:
   // - value1 is the module filename
   // - value2 is the first symbol in the range, case sensitive
   // - value3 is the last symbol in the range, case sensitive
   // - value4 is the name to be given to the range.  If not given the name
   //          becomes that of the first symbol in the range (value2)
   //
   // ex: set A2N_COLLAPSE_RANGE=jvm.dll Symbol1 SymbolX MySymbolRange
   //
   p = getenv("A2N_COLLAPSE_RANGE");
   if (p == NULL) {
      p = getenv("a2n_collapse_range");
   }
   if (p != NULL) {
      char m[64], s1[128], s2[128], n[64];
      int rc;

      rc = sscanf(p, "%s %s %s %s", m, s1, s2, n);
      if (rc == 4) {
         dbgmsg(("A2N_COLLAPSE_RANGE: %s %s %s %s\n", m, s1, s2, n));
         AddRangeDescNode(zstrdup("ENV"), zstrdup(m), zstrdup(s1), zstrdup(s2), zstrdup(n));
      } else if (rc == 3) {
         dbgmsg(("A2N_COLLAPSE_RANGE: %s %s %s\n", m, s1, s2));
         AddRangeDescNode(zstrdup("ENV"), zstrdup(m), zstrdup(s1), zstrdup(s2), zstrdup(s1));
      }
   }

   //
   // See if they want to use a file to describe symbol ranges.
   //
   p = getenv("A2N_RANGE_DESC_FILENAME");
   if (p == NULL) {
      p = getenv("a2n_range_desc_filename");
   }
   if (p != NULL) {
      rc = check_if_good_filename(p);
      if (rc == 0)
         ReadRangeDescriptorFile(p);
   }

   //
   // See if they want us to display symbols within a range
   //
   p = getenv("A2N_RETURN_RANGE_SYMBOLS");
   if (p == NULL) {
      p = getenv("a2n_return_range_symbols");
   }
   if (p != NULL) {
      api_lock.ReturnRangeSymbolsMode = 1;  // Don't let API change it
      if (stricmp(p, "no") == 0)            // No, just range name
         G(return_range_symbols) = 0;
      else                                  // Yes, range,symbol
         G(return_range_symbols) = 1;
   }

#if defined(_WINDOWS)
   //
   // See if they want to change the name of the jvmmap file. They can
   // use either one (compressed or not).  We'll decompress it if need be.
   // Value must be a name.
   //
   p = getenv("A2N_JVMMAP_NAME");
   if (p == NULL) {
      p = getenv("a2n_jvmmap_name");
   }
   if (p != NULL) {
      rc = check_if_good_filename(p);
      if (rc == 0) {
         zfree(G(jvmmap_fn));
         G(jvmmap_fn) = zstrdup(p);         // Save the name
      }
   }

   //
   // See if they want to add to the special Windows DLLs whose names
   // need to be fixed up.
   // Value must be a search path string.  No error checking.
   //
   p = getenv("A2N_SPECIAL_DLLS");
   if (p == NULL) {
      p = getenv("a2n_special_dlls");
   }
   if (p != NULL) {
      AdditionalSpecialDlls = zstrdup(p);   // Save the list of DLLs
   }
#endif

   //
   // That's it
   //
   return;
}


//
// InitOsSpecificStuff()
// *********************
//
static void InitOsSpecificStuff(void)
{
   char * p;
   char c[1024];
   int l;
#if defined(_WINDOWS)
   char w[1024];
   int i;
   uint res;
#endif


   //
   // Beginning of symbol search path (if any)
   //
   c[0] = 0;
#if defined(_WINDOWS)
   w[0] = 0;
#endif
   if (AdditionalSearchPath) {
      //
      // Pre-pend given directories to the search path.
      // - If it ends in / or \ change it to : or ;
      // - Add : or ; if there isn't one.
      //
      l = (int)strlen(AdditionalSearchPath);
      strcpy(c, AdditionalSearchPath);

      if (AdditionalSearchPath[l-1] == G(PathSeparator)) {
         c[l-1] = G(SearchPathSeparator);
      }
      else if (AdditionalSearchPath[l-1] != G(SearchPathSeparator)) {
         c[l] = G(SearchPathSeparator);
         c[l+1] = 0;
      }

#if defined(_WINDOWS)
      strcpy(w, c);
#endif
   }

#if defined(_LINUX) || defined(_ZOS)
   //
   // Build the rest of the symbol search path
   //
   strcat(c, G(PathSeparator_str));
   strcat(c, "symbols");
   strcat(c, G(SearchPathSeparator_str));
   strcat(c, G(PathSeparator_str));
   strcat(c, "usr");
   strcat(c, G(PathSeparator_str));
   strcat(c, "symbols");
   p = getenv("HOME");
   if (p) {
      strcat(c, G(SearchPathSeparator_str));
      strcat(c, p);
      strcat(c, G(PathSeparator_str));
      strcat(c, "symbols");
   }
   G(SymbolSearchPath) = zstrdup(c);
#endif

#if defined(_WINDOWS)
   //
   // Get system-wide, shared Windows directory.
   // On a single-user system it's the same as the Windows Directory.
   // On a multi-user system the Windows Directory is is private to each user.
   //
   // *** NOTE ***
   // The right way to do this is to use GetSystemWindowsDirectory() to get
   // the value of the System Directory.  That API is not available on
   // Windows NT and I don't want to have 2 versions of a2n.dll so I'm
   // using the values defined in the environment variables "SystemRoot"
   // and "SystemDrive".
   // ************
   //
   p = getenv("SystemDrive");
   if (p)
      SystemDrive = zstrdup(p);
   else {
      msg_se("*W* a2n.c: Unable to determine System Drive. Assuming C:\n");
      SystemDrive = zstrdup("C:");
   }

   p = getenv("SystemRoot");
   if (p) {
      SystemRoot = zstrdup(p);
      WinSysDir = strlwr(zstrdup(p));
      strcpy(WinSysDir, &WinSysDir[2]);  // Start at "\"
      strcat(WinSysDir, "\\");           // End it with a "\"
   }
   else {
      SystemRoot = zcalloc(1, 16);
      WinSysDir = zcalloc(1, 16);
      strcpy(SystemRoot, SystemDrive);
      strcat(SystemRoot, SYSTEM_ROOT_DIR);
      strcpy(WinSysDir, WIN_SYS_DIR);
      msg_se("*W* a2n.c: Unable to determine System Directory. Assuming %s\n", SystemRoot);
   }

   //
   // Build the rest of the symbol search path
   // c: normal search path
   //    32-bit:  \Symbols
   //    64-bit:  \Symbols;\Symbols\SysWow64
   // w: search path with Symbols\SysWow64 ahead of Symbols
   //    32-bit:  n/a
   //    64-bit:  \Symbols\SysWow64;\Symbols
   //
   // %SystemRoot%\Symbols
   strcat(c, SystemRoot);
   strcat(c, G(PathSeparator_str));
   strcat(c, "Symbols");

   // %SystemRoot%\Symbols\SysWow64
   strcat(c, G(SearchPathSeparator_str));
   strcat(c, SystemRoot);
   strcat(c, G(PathSeparator_str));
   strcat(c, "Symbols");
   strcat(c, G(PathSeparator_str));
   strcat(c, "SysWow64");

   // %SystemRoot%\Symbols\SysWow64
   strcat(w, SystemRoot);
   strcat(w, G(PathSeparator_str));
   strcat(w, "Symbols");
   strcat(w, G(PathSeparator_str));
   strcat(w, "SysWow64");
   // %SystemRoot%\Symbols
   strcat(w, G(SearchPathSeparator_str));
   strcat(w, SystemRoot);
   strcat(w, G(PathSeparator_str));
   strcat(w, "Symbols");

   p = getenv("_NT_SYMBOL_PATH");
   if (p) {
      strcat(c, G(SearchPathSeparator_str));
      strcat(c, p);
      strcat(w, G(SearchPathSeparator_str));
      strcat(w, p);
   }

   p = getenv("_NT_ALT_SYMBOL_PATH");
   if (p) {
      strcat(c, G(SearchPathSeparator_str));
      strcat(c, p);
      strcat(w, G(SearchPathSeparator_str));
      strcat(w, p);
   }

   p = getenv("_NT_ALTERNATE_SYMBOL_PATH");
   if (p) {
      strcat(c, G(SearchPathSeparator_str));
      strcat(c, p);
      strcat(w, G(SearchPathSeparator_str));
      strcat(w, p);
   }

   G(SymbolSearchPath) = zstrdup(c);
   G(SymbolSearchPathWow64) = zstrdup(w);

   // %SystemRoot%\System32
   strcpy(c, SystemRoot);
   strcat(c, "\\System32\\");
   _strlwr(c);
   OS_System32 = zstrdup(c);

   // %SystemRoot%\SysWOW64
   strcpy(c, SystemRoot);
   strcat(c, "\\SysWOW64\\");
   _strlwr(c);
   OS_SysWow64 = zstrdup(c);

   // %SystemRoot%\WinSxS
   strcpy(c, SystemRoot);
   strcat(c, "\\WinSxS\\");
   _strlwr(c);
   OS_WinSxS = zstrdup(c);

   //
   // Get a list of the symbolic links in the object name space associated
   // with each DOS device (drive letter).  Can't make any assumptions
   // because links change from system to system.
   //
   for (i = 0; i < MAX_DEVICES; i++) {
      res = QueryDosDevice(drv_name[i], c, 128);
      if (res == 0) {
         dev_name[i][0] = 0;                // No link for this device
         rdev_name[i][0] = 0;               // No link for this device
      }
      else {
         strcpy(rdev_name[i], c);           // Name as returned
         p = strstr(c, "\\Device");         // Does name start with "\Device" ?
         if (p == c) {
            //
            // Must be something like:
            //
            // * \Device\HarddiskVolume15\Tools\test.exe
            //   - This is a local hard drive.
            //   - Save as is
            //
            // * \Device\Floppy0\test.exe
            //   - This is a floppy drive.
            //   - Save as is
            //
            // * \Device\CdRom0\test.exe
            //   - This is a CD drive.
            //   - Save as is
            //
            // * \Device\LanmanRedirector\;R:XXXXXXXXXXXXX\machine\share
            //   - This is a mounted network drive.
            //     It is local drive R: for network share \\machine\share
            //     (XXXXXXXXX, which can be any number of numbers, is the LUID
            //     associated with the logon session of the use that mounted the drive)
            //   - Save as \machine\share  (notice only 1 leading \)
            //
            p = strstr(p+1, "\\");          // find 2nd "\"
            if (strncmp(p+1, "LanmanRedirector", 16) == 0) {
               // network drive
               p = strstr(p+1, "\\");       // find 3rd "\"
               p = strstr(p+1, "\\");       // find 4th "\"
               //*****// strcpy(dev_name[i],"\\");
               strcat(dev_name[i], p);
               dev_type[i] = DEVICE_NETWORK_DRIVE;
            }
            else if (strncmp(p+1, "HarddiskVolume", 14) == 0) {
               // local drive
               strcpy(dev_name[i], c);
               dev_type[i] = DEVICE_LOCAL_DRIVE;
            }
            else if (strncmp(p+1, "Floppy", 6) == 0) {
               // floppy/removable drive
               strcpy(dev_name[i], c);
               dev_type[i] = DEVICE_FLOPPY_DRIVE;
            }
            else if (strncmp(p+1, "CdRom", 5) == 0) {
               // cd/dvd drive
               strcpy(dev_name[i], c);
               dev_type[i] = DEVICE_CD_DVD_DRIVE;
            }
            else {
               strcpy(dev_name[i], c);
               dev_type[i] = DEVICE_UNKNOWN_DRIVE;
            }
         }
         else {
            p = strstr(c, "\\??\\");        // Does name start with "\??\" ?
            //
            // Must be something like:
            //
            // * \??\C:\WINNT\system32\csrss.exe
            //   - Substituted drive
            //   - Remove "\??\"  before saving
            //
            if (p == c) {
               // substituted drive
               strcpy(dev_name[i], p+4);
               dev_type[i] = DEVICE_SUBST_DRIVE;
            }
            else {
               strcpy(dev_name[i], c);
               dev_type[i] = DEVICE_UNKNOWN_DRIVE;
            }
         }
      }
   }

   //
   // Special-case DLLs that sometimes don't have fully qualified path
   //
   strcpy(c, DEFAULT_SPECIAL_DLLS1);
   strcat(c, DEFAULT_SPECIAL_DLLS2);
   strcat(c, DEFAULT_SPECIAL_DLLS3);
   if (AdditionalSpecialDlls != NULL) {
      strcat(c, AdditionalSpecialDlls);
      zfree(AdditionalSpecialDlls);
   }
   G(SpecialWindowsDlls) = zstrdup(c);

   //
   // Set jvmmap filename
   //
   if (G(jvmmap_fn) == NULL)
      G(jvmmap_fn) = zstrdup(DEFAULT_JVMMAP_NAME);
#endif

   return;
}


#if defined(_WINDOWS)
//
//
// have_required_dbghelp_apis()
// ****************************
//
int have_required_dbghelp_apis(void)
{
   // These are all required and available in dbghelp v5.1 and above
   if (pSymInitialize == NULL || pSymRegisterCallback64 == NULL ||
       pSymSetSearchPath == NULL || pSymCleanup == NULL || pSymGetOptions == NULL ||
       pSymSetOptions == NULL || pSymEnumSymbols == NULL || pSymLoadModule64 == NULL ||
       pSymUnloadModule64 == NULL || pSymGetModuleInfo64 == NULL)
      return (0);
   else
      return (1);
}


//
// load_dbghelp()
// **************
//
void load_dbghelp(char * fn)
{
   char loaded_fn[256];
   int major, minor, rev, spin;

   dbgmsg(("** load_dbghelp: Attempting to load %s\n", fn));
   hDbgHelp = LoadLibrary(fn);
   if (hDbgHelp) {
      loaded_fn[0] = 0;
      if (GetModuleFileName(hDbgHelp, loaded_fn, sizeof(loaded_fn)) != 0) {
         get_file_version(loaded_fn, &major, &minor, &rev, &spin);
         msg_log("*** Using: %s (v%d.%d.%d.%d) loaded at 0x%"_PZP" ***\n", loaded_fn, major, minor, rev, spin, hDbgHelp);
      }
      else {
         get_file_version(fn, &major, &minor, &rev, &spin);
         msg_log("!!! Using: %s (v%d.%d.%d.%d) loaded at 0x%"_PZP" ***\n", fn, major, minor, rev, spin, hDbgHelp);
      }

      dbghelp_version_major = major;

      pSymInitialize = (PFN_SymInitialize)GetProcAddress(hDbgHelp, "SymInitialize");
      pSymRegisterCallback64 = (PFN_SymRegisterCallback64)GetProcAddress(hDbgHelp, "SymRegisterCallback64");
      pSymSetSearchPath = (PFN_SymSetSearchPath)GetProcAddress(hDbgHelp, "SymSetSearchPath");
      pSymCleanup = (PFN_SymCleanup)GetProcAddress(hDbgHelp, "SymCleanup");
      pSymGetOptions = (PFN_SymGetOptions)GetProcAddress(hDbgHelp, "SymGetOptions");
      pSymSetOptions = (PFN_SymSetOptions)GetProcAddress(hDbgHelp, "SymSetOptions");
      pUnDecorateSymbolName = (PFN_UnDecorateSymbolName)GetProcAddress(hDbgHelp, "UnDecorateSymbolName");
      pSymEnumSymbols = (PFN_SymEnumSymbols)GetProcAddress(hDbgHelp, "SymEnumSymbols");
      pSymEnumLines   = (PFN_SymEnumLines)GetProcAddress(hDbgHelp, "SymEnumLines");
      pSymEnumSourceLines = (PFN_SymEnumSourceLines)GetProcAddress(hDbgHelp, "SymEnumSourceLines");
      pSymLoadModuleEx = (PFN_SymLoadModuleEx)GetProcAddress(hDbgHelp, "SymLoadModuleEx");
      pSymLoadModule64 = (PFN_SymLoadModule64)GetProcAddress(hDbgHelp, "SymLoadModule64");
      pSymUnloadModule64 = (PFN_SymUnloadModule64)GetProcAddress(hDbgHelp, "SymUnloadModule64");
      pSymGetModuleInfo64 = (PFN_SymGetModuleInfo64)GetProcAddress(hDbgHelp, "SymGetModuleInfo64");

      if (G(debug) & DEBUG_MODE_INTERNAL) {
         msg_log(" pSymInitialize         = 0x%"_PZP"\n", pSymInitialize);
         msg_log(" pSymRegisterCallback64 = 0x%"_PZP"\n", pSymRegisterCallback64);
         msg_log(" pSymSetSearchPath      = 0x%"_PZP"\n", pSymSetSearchPath);
         msg_log(" pSymCleanup            = 0x%"_PZP"\n", pSymCleanup);
         msg_log(" pSymGetOptions         = 0x%"_PZP"\n", pSymGetOptions);
         msg_log(" pSymSetOptions         = 0x%"_PZP"\n", pSymSetOptions);
         msg_log(" pUnDecorateSymbolName  = 0x%"_PZP"\n", pUnDecorateSymbolName);
         msg_log(" pSymEnumSymbols        = 0x%"_PZP"\n", pSymEnumSymbols);
         msg_log(" pSymEnumLines          = 0x%"_PZP"\n", pSymEnumLines);
         msg_log(" pSymEnumSourceLines    = 0x%"_PZP"\n", pSymEnumSourceLines);
         msg_log(" pSymLoadModuleEx       = 0x%"_PZP"\n", pSymLoadModuleEx);
         msg_log(" pSymLoadModule64       = 0x%"_PZP"\n", pSymLoadModule64);
         msg_log(" pSymUnloadModule64     = 0x%"_PZP"\n", pSymUnloadModule64);
         msg_log(" pSymGetModuleInfo64    = 0x%"_PZP"\n", pSymGetModuleInfo64);
      }

      if (!have_required_dbghelp_apis()) {
         msg_log("\n*W* DbgHelp.dll does not export required APIs. Will not get PDB symbols.\n");
         msg_log("    In order to get PDB symbols dbghelp.dll version 5.1 or higher is required.\n");
         msg_log("    You can get the latest dbghelp.dll by downloading the most recent Debugging\n");
         msg_log("    Tools from Microsoft. As an alternative, if there is a 'dbghelp.dll' in the\n");
         msg_log("    tools 'symtools' directory, you can set your PATH to include the tools\n");
         msg_log("    'symtools' directory first.\n");
         return;
      }

      // UnDecorateSymbolName() is available in dbghelp v5.1 and above
      if (pUnDecorateSymbolName == NULL) {
         msg_log("\n*I* DbgHelp.dll does not export UnDecorateSymbolName(). Will not demangle C++ symbol names.\n");
         G(demangle_cpp_names) = 0;
      }

      if (pSymEnumLines == NULL && pSymEnumSourceLines == NULL) {
         G(lineno) = 0;
         msg_log("\n*I* DbgHelp.dll does not export SymEnumLines() nor SymEnumSourceLines(). Will not get line numbers.\n");
         msg_log("    In order to get source line numbers dbghelp.dll version 6.1 or higher is required.\n");
         msg_log("    You can get the latest dbghelp.dll by downloading the most recent Debugging\n");
         msg_log("    Tools from Microsoft. As an alternative, if there is a 'dbghelp.dll' in the\n");
         msg_log("    tools 'symtools' directory, you can set your PATH to include the tools\n");
         msg_log("    'symtools' directory first.\n");
      }
   }
   else {
      dbgmsg(("** load_dbghelp: Unable to load %s\n", fn));
   }

   return;
}


//
// find_good_dbghelp()
// *******************
//
// Look along the PATH for a dbghelp.dll that is at v6.1 or above and load it.
// If can't find one with a good version then load whatever the system loads.
//
void find_good_dbghelp(void)
{
   char * sp, * p;
   char fn[256];
   int major, minor, rev, spin;
   LONG rc;
   HKEY key;
   DWORD buf_len;
   HMODULE handle = NULL;


   dbgmsg(("\n>> find_good_dbghelp: Looking for dbghelp.dll at v6.1.x.x or above ...\n"));
   // - The current directory.
   // - The directory where a2n.dll was loaded from
   // - The directory where the Debugging Tools for Windows are installed, if any.
   // - Directories listed in PATH.
   sp = zmalloc(2048);
   if (sp == NULL) {
      msg_log("*W* Failed malloc() in find_good_dbghelp(). Will attempt to load default dbghelp.dll.\n");
      load_dbghelp("dbghelp.dll");
      if (hDbgHelp == NULL) {
         msg_log("\n*W* Unable to load dbghelp.dll. Will not get PDB symbols nor line numbers.\n");
         G(lineno) = 0;
      }
      dbgmsg(("<< find_good_dbghelp\n"));
      return;
   }
   sp[0] = '\0';

   // Current directory
   strcat(sp, ".;");

   // Directory where A2N was loaded from (if we can figure it out)
   handle = GetModuleHandle("a2n");
   if (handle) {
      fn[0] = 0;
      if (GetModuleFileName(handle, fn, 256) != 0) {
         get_file_version(fn, &major, &minor, &rev, &spin);
         msg_log("\n*** Using: %s (v%d.%d.%d) loaded at 0x%"_PZP" ***\n", fn, major, minor, rev, handle);
         p = strrchr(fn, '\\');
         *p = '\0';
         strcat(sp, fn);
         strcat(sp, ";");
      }
   }

   // See if Debugging Tools for Windows installed.
   // HKCU\Software\Microsoft\DebuggingTools
   rc = RegOpenKeyEx(HKEY_CURRENT_USER,
                     TEXT("Software\\Microsoft\\DebuggingTools"),
                     0, KEY_QUERY_VALUE, &key);
   if (rc == ERROR_SUCCESS) {
      buf_len = 256;
      rc = RegQueryValueEx(key, TEXT("WinDbg"), NULL, NULL, (LPBYTE)fn, &buf_len);
      if (rc == ERROR_SUCCESS) {
         strcat(sp, fn);
         strcat(sp, ";");
      }
      RegCloseKey(key);
   }

   // Directories listed in PATH.
   p = getenv("PATH");
   strcat(sp, p);

   dbgmsg(("- find_good_dbghelp: search_path=%s\n", sp));

   // Start looking
   p = strtok(sp, ";");
   while (p) {
      strcpy(fn, p);                        // Path
      strcat(fn, "\\");                     // "\"
      strcat(fn, "dbghelp.dll");            // filename

      if (GetFileAttributes(fn) != INVALID_FILE_ATTRIBUTES) {
         get_file_version(fn, &major, &minor, &rev, &spin);
         if (major > 6 || (major == 6 && minor > 1)) {
            load_dbghelp(fn);
            if (hDbgHelp) {
               // Loaded a good one
               zfree(sp);
               dbgmsg(("<< find_good_dbghelp\n"));
               return;
            }
            // Didn't load. Keep trying
         }
         else {
            // Incorrect version. Keep trying
            dbgmsg(("** %s is v%d.%d.%d.%d *** REJECTING - INCORRECT VERSION ***\n", fn, major, minor, rev, spin));
         }
      }
      p = strtok(NULL, ";");                // Try the next path component
   }

   // Could not find/load a dbghelp at a version we like.
   // As the last resort just load whatever the system loads.
   msg_log("*W* Unable to find a DbgHelp.dll at v6.1 or above. Will load default one.\n");
   load_dbghelp("dbghelp.dll");
   if (hDbgHelp == NULL) {
      msg_log("\n*W* Unable to load dbghelp.dll. Will not get PDB symbols nor line numbers.\n");
      G(lineno) = 0;
   }

   // Done whether we loaded something or not
   zfree(sp);
   dbgmsg(("<< find_good_dbghelp\n"));
   return;
}
#endif


//
// A2nListControls()
// *****************
//
// List environment variables and how to use them.
//
void A2nListControls(void)
{
   char * ns = "Not Set";
   char * p;

   msg_so("\n");
   msg_so("The following environment variables can be used to control A2N processing:\n");
   msg_so("\n");

   msg_so(" A2N_DEBUG\n");
   msg_so(" ---------\n");
   msg_so("   Enables all debug output. Forces all messages and proc/mod exit dumps.\n");
   msg_so("   Output written to file a2n.err. This setting produces A LOT of output.\n");
   msg_so("   * on/yes Turns on full debug mode.\n");
   msg_so("   * off/no Turns off full debug mode. Leave exit dump mode unchanged.\n");
   msg_so("   * ????   (none of the above): Same as on/yes.\n");
   msg_so("   If not set, the default is OFF (no debug messages).\n");
   p = getenv("A2N_DEBUG");
   if (p == NULL) {
      p = getenv("a2n_debug");
      if (p == NULL) {
         p = getenv("A2NDEBUG");
         if (p == NULL) {
            p = getenv("a2ndebug");
         }
      }
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_DEBUG_API\n");
   msg_so(" -------------\n");
   msg_so("   Enables API entry/exit debug output. Forces all messages and proc/mod exit\n");
   msg_so("   dumps. Output written to file a2n.err.\n");
   msg_so("   * on/yes Turns on API entry/exit debug mode.\n");
   msg_so("   * off/no Turns off API entry/exit debug mode. Leave exit dump mode unchanged.\n");
   msg_so("   * ????   (none of the above): Same as on/yes.\n");
   msg_so("   If not set, the default is OFF (no debug messages).\n");
   p = getenv("A2N_DEBUG_API");
   if (p == NULL)  {
      p = getenv("a2n_debug_api");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");


   msg_so(" A2N_DEBUG_HV\n");
   msg_so(" ------------\n");
   msg_so("   Enables symbol harvester debug output. Forces all messages and proc/mod exit\n");
   msg_so("   dumps. Output written to file a2n.err.\n");
   msg_so("   * on/yes Turns harvester debug mode.\n");
   msg_so("   * off/no Turns harvester debug mode. Leave exit dump mode unchanged.\n");
   msg_so("   * ????   (none of the above): Same as on/yes.\n");
   msg_so("   If not set, the default is OFF (no debug messages).\n");
   p = getenv("A2N_DEBUG_HV");
   if (p == NULL)  {
      p = getenv("a2n_debug_hv");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_DUMP_SD\n");
   msg_so(" -----------\n");
   msg_so("   Controls whether SYMDATA is dumped on every call to A2nGetSymbol().\n");
   msg_so("   This only works if A2N_DEBUG is set to a debug mode.  A2N_DUMP_SD\n");
   msg_so("   is a debug aid and should never be set unless you're asked to set it.\n");
   msg_so("   * yes    Dump SYMDATA structure.\n");
   msg_so("   * no     Don't dump it.\n");
   msg_so("   If not set, the default is NO (don't dump SYMDATA).\n");
   p = getenv("A2N_DUMP_SD");
   if (p == NULL)  {
      p = getenv("a2n_dump_sd");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_ERROR_MESSAGES\n");
   msg_so(" ------------------\n");
   msg_so("   Controls the level of error messages written to a2n.err.\n");
   msg_so("   * none   No messages of any kind written to log file.\n");
   msg_so("   * error  Only error messages written to log file.\n");
   msg_so("   * warn   Error and Warning messages written to log file.\n");
   msg_so("   * info   Error, Warning and Informational messages written to log file.\n");
   msg_so("   * all    All messages written to log file.\n");
   msg_so("   * ????   (none of the above): Same as all.\n");
   msg_so("   The number of messages increases as you work your way down the list:\n");
   msg_so("   - all produces *A LOT* of messages.\n");
   msg_so("   - info produces more messages than warn.\n");
   msg_so("   - warn produces more messages than errror.\n");
   msg_so("   - error produces more messages than none.\n");
   msg_so("   If not set, the default is ERROR (error messages only).\n");
   p = getenv("A2N_ERROR_MESSAGES");
   if (p == NULL)  {
      p = getenv("a2n_error_messages");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_IMMEDIATE_GATHER\n");
   msg_so(" --------------------\n");
   msg_so("   Controls when symbols are gathered.\n");
   msg_so("   * yes    Gather symbols when modules are added (ie. first seen).\n");
   msg_so("   * no     Gather symbols only when necessary to do so.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (gather when necessary).\n");
   p = getenv("A2N_IMMEDIATE_GATHER");
   if (p == NULL)  {
      p = getenv("a2n_immediate_gather");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_GATHER_CODE\n");
   msg_so(" ---------------\n");
   msg_so("   Controls whether or not code is gathered from executables/images.\n");
   msg_so("   * yes    Gather code (if possible).\n");
   msg_so("   * no     Don't gather code.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (don't gather code).\n");
   p = getenv("A2N_GATHER_CODE");
   if (p == NULL)  {
      p = getenv("a2n_gather_code");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_MODULE_DUMP\n");
   msg_so(" ---------------\n");
   msg_so("   Controls whether or not A2N dumps out module/symbol information\n");
   msg_so("   to file on exit. This is a debugging aid but does contain useful\n");
   msg_so("   information. The file contains information about every module seen\n");
   msg_so("   and what data was gathered from it (code, symbols, etc.).\n");
   msg_so("   * yes    Dump module information file (a2n.mod) on exit.\n");
   msg_so("   * no     Don't dump module information file.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (no module/symbol dump on exit).\n");
   p = getenv("A2N_MODULE_DUMP");
   if (p == NULL)  {
      p = getenv("a2n_module_dump");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_PROCESS_DUMP\n");
   msg_so(" ----------------\n");
   msg_so("   Controls whether or not A2N dumps out process information to file\n");
   msg_so("   on exit. This is a debugging aid but does contain useful information.\n");
   msg_so("   The file contains information about process (PID) seen and what\n");
   msg_so("   information is known about the process (clone/fork, loaded modules,\n");
   msg_so("   name, children/siblings, etc.),\n");
   msg_so("   * yes    Dump process information file (a2n.proc) on exit.\n");
   msg_so("   * no     Don't dump process information file.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (no process dump on exit).\n");
   p = getenv("A2N_PROCESS_DUMP");
   if (p == NULL)  {
      p = getenv("a2n_process_dump");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_MSI_DUMP\n");
   msg_so(" ------------\n");
   msg_so("   Controls whether or not A2N dumps out its internal module/symbol\n");
   msg_so("   tables to file on exit. The MSI file can be used as input to POST\n");
   msg_so("   at a later time thus removing the need to re-harvest symbols.\n");
   msg_so("   * yes    Creates MSI file (a2n.msi) on exit.\n");
   msg_so("   * no     Don't create MSI file.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (no MSI file on exit).\n");
   p = getenv("A2N_MSI_DUMP");
   if (p == NULL)  {
      p = getenv("a2n_msi_dump");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_DONT_VALIDATE\n");
   msg_so(" -----------------\n");
#if defined(_LINUX) || defined(_ZOS)
   msg_so("   Controls whether or not kernel symbols are validated against the\n");
   msg_so("   ksyms (/proc/ksyms) for the currently running kernel.\n");
   msg_so("   * yes    Don't validate kernel symbols. (don't yes => don't)\n");
   msg_so("   * no     Make sure kernel image symbols match /proc/ksyms. (don't not => do)\n");
#else
   msg_so("   Controls whether or not symbols are validated to make sure they\n");
   msg_so("   match the image/loaded module.\n");
   msg_so("   * yes    Don't check if symbol file matches image. (don't yes => don't)\n");
   msg_so("   * no     Make sure symbol file(s) match image. (don't not => do)\n");
#endif
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (validate).\n");
   msg_so("\n");
   msg_so("   Function can also be controlled via post with \"-nv\" option.\n");
   p = getenv("A2N_DONT_VALIDATE");
   if (p == NULL)  {
      p = getenv("a2n_dont_validate");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

#if defined(_LINUX)
   msg_so(" A2N_QUIT_ON_VALIDATION_ERROR\n");
   msg_so(" ----------------------------\n");
   msg_so("   Controls whether or not processing stops if validation of kernel\n");
   msg_so("   symbols fails.\n");
   msg_so("   * yes    Stop processing if kernel validation fails.\n");
   msg_so("   * no     Continue processing even if kernel symbol validation fails.\n");
   msg_so("            Kernel symbols are discarded.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (continue processing).\n");
   p = getenv("A2N_QUIT_ON_VALIDATION_ERROR");
   if (p == NULL)  {
      p = getenv("a2n_quit_on_validation_error");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");
#endif

   msg_so(" A2N_ADD_MODULE_CACHE_HEAD\n");
   msg_so(" -------------------------\n");
   msg_so("   Causes A2N to add module information nodes at the head (front) of the\n");
   msg_so("   cached module list. This is an internal control and you should not change\n");
   msg_so("   it unless told to do so.\n");
   msg_so("   Valid values are:\n");
   msg_so("   * any    Add module info to head of cached module list.\n");
   msg_so("   If not set, the default is add module information at the tail of the list.\n");
   p = getenv("A2N_ADD_MODULE_CACHE_HEAD");
   if (p == NULL)  {
      p = getenv("a2n_add_module_cache_head");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_ADD_MODULE_CACHE_TAIL\n");
   msg_so(" -------------------------\n");
   msg_so("   Causes A2N to add module information nodes at the tail (back) of the\n");
   msg_so("   cached module list. This is an internal control and you should not change\n");
   msg_so("   it unless told to do so.\n");
   msg_so("   Valid values are:\n");
   msg_so("   * any    Add module info to tail of cached module list.\n");
   msg_so("   If not set, the default is add module information at the tail of the list.\n");
   p = getenv("A2N_ADD_MODULE_CACHE_TAIL");
   if (p == NULL)  {
      p = getenv("a2n_add_module_cache_tail");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_DONT_DEMANGLE\n");
   msg_so(" -----------------\n");
   msg_so("   Controls whether or not an attempt is made to demangle C++ symbol names.\n");
   msg_so("   The default when demangling names is to just demangle the name, without\n");
   msg_so("   including access specifiers, function arguments, return type, etc.\n");
   msg_so("   If you want to completely demangle the name then set A2N_DEMANLGE_COMPLETE.\n");
   msg_so("   * yes    Don't demangle C++ symbol names. (don't yes => don't)\n");
   msg_so("   * no     Attempt to demangle C++ symbol names. (don't not => do)\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (attempt to demangle names).\n");
   p = getenv("A2N_DONT_DEMANGLE");
   if (p == NULL)  {
      p = getenv("a2n_dont_demangle");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_DEMANGLE_COMPLETE\n");
   msg_so(" ---------------------\n");
   msg_so("   Causes mangled C++ names to be completely demangled. The demangled\n");
   msg_so("   names include access specifiers, function arguments, return type, etc.\n");
   msg_so("   This setting is only in effect if names are being demangled.\n");
   msg_so("   * yes    Completely demangle C++ symbol names.\n");
   msg_so("   * no     Demangle just the names.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (demangle just names).\n");
   p = getenv("A2N_DEMANGLE_COMPLETE");
   if (p == NULL)  {
      p = getenv("a2n_demangle_complete");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_RENAME_DUPLICATE_SYMBOLS\n");
   msg_so(" ----------------------------\n");
   msg_so("   Causes A2N to make an extra pass after symbols are harvested to\n");
   msg_so("   check if there are duplicate symbols (symbols withe the same name)\n");
   msg_so("   and rename them to make them unique and be able to tell them apart.\n");
   msg_so("   * yes    Check and rename duplicate symbols.\n");
   msg_so("   * no     Don't check for duplicate symbols.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (don't check for duplicate symbols).\n");
   p = getenv("A2N_RENAME_DUPLICATE_SYMBOLS");
   if (p == NULL)  {
      p = getenv("a2n_rename_duplicate_symbols");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_NSF_ACTION\n");
   msg_so(" --------------\n");
   msg_so("   Controls what is returned for a symbol name if the image contains\n");
   msg_so("   symbols but no symbol is found (NO_SYMBOL_FOUND) or if there are\n");
   msg_so("   no symbols (NO_SYMBOLS).\n");
   msg_so("   * null     Return NULL symbol name on NO_SYMBOL_FOUND.\n");
   msg_so("   * nsf      Return \"NoSymbolFound\" as symbol name on NO_SYMBOL_FOUND.\n");
   msg_so("              or \"NoSymbols\" as symbol name on NO_SYMBOLS,\n");
   msg_so("   * section  Return section name (if available) as symbol name.\n");
   msg_so("   * ????     (none of the above): Same as nsf.\n");
   msg_so("   If not set, the default is NSF (\"NoSymbolFound\"/\"NoSymbols\" name).\n");
   p = getenv("A2N_NSF_ACTION");
   if (p == NULL)  {
      p = getenv("a2n_nsf_action");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_NOMOD_THRESHOLD\n");
   msg_so(" -------------------\n");
   msg_so("   Sets the number of times NO_MODULE is returned by A2nGetSymbol()\n");
   msg_so("   after which the modulde/symbol and process information will be\n");
   msg_so("   dumped on exit.\n");
   msg_so("   * number   NoModuleFound forced dump threshold = number\n");
   msg_so("   If not set, the default is infinity (never force dump).\n");
   p = getenv("A2N_NOMOD_THRESHOLD");
   if (p == NULL)  {
      p = getenv("a2n_nomod_threshold");
   }
   if (p == NULL)
      msg_so("   -------> current value: %s\n", ns);
   else
      msg_so("   -------> current value: %d\n", atoi(p));
   msg_so("\n");

   msg_so(" A2N_NOSYM_THRESHOLD\n");
   msg_so(" -------------------\n");
   msg_so("   Sets the number of times NO_SYMBOL_FOUND is returned by A2nGetSymbol()\n");
   msg_so("   after which the modulde/symbol and process information will be\n");
   msg_so("   dumped on exit.\n");
   msg_so("   * number   NoSymbolFound forced dump threshold = number\n");
   msg_so("   If not set, the default is infinity (never force dump).\n");
   p = getenv("A2N_NOSYM_THRESHOLD");
   if (p == NULL)  {
      p = getenv("a2n_nosym_threshold");
   }
   if (p == NULL)
      msg_so("   -------> current value: %s\n", ns);
   else
      msg_so("   -------> current value: %d\n", atoi(p));
   msg_so("\n");

   msg_so(" A2N_SYMBOL_QUALITY\n");
   msg_so(" ------------------\n");
   msg_so("   Controls the quality of symbols harvested/returned.\n");
   msg_so("   * any    Harvest and return whatever symbols are available\n");
   msg_so("   * good   Harvest and return only debug symbols\n");
   msg_so("   * ????   (none of the above): Same as any.\n");
   msg_so("   If not set, the default is DEBUG (harvest debug quality symbols only).\n");
   p = getenv("A2N_SYMBOL_QUALITY");
   if (p == NULL)  {
      p = getenv("a2n_symbol_quality");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_LINENO\n");
   msg_so(" ----------\n");
   msg_so("   Controls whether or not source line numbers are returned.\n");
   msg_so("   * yes    Return source line numbers (with symbols) if available\n");
   msg_so("   * no     Don't return source line numbers\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (don't return source line numbers).\n");
   p = getenv("A2N_LINENO");
   if (p == NULL)  {
      p = getenv("a2n_lineno");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

#if defined (_WINDOWS)
   msg_so(" A2N_IGNORE_TSCS\n");
   msg_so(" ---------------\n");
   msg_so("   Controls whether or not an on-disk image is OK to use\n");
   msg_so("   by allowing a timestamp and/or checksum mismatch to occur.\n");
   msg_so("   * yes    Ignore timestamp/checksum mismatch of on-disk image.\n");
   msg_so("   * no     Don't allow mismatch - on-disk image TS/CS must match.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (on-disk image TS/CS must match).\n");
   p = getenv("A2N_IGNORE_TSCS");
   if (p == NULL)  {
      p = getenv("a2n_ignore_tscs");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_EXPORTS_ONLY\n");
   msg_so(" ----------------\n");
   msg_so("   Controls whether or not only EXPORTS are harvested/returned.\n");
   msg_so("   * yes    Only harvest EXPORTS, even if better quality symbols available.\n");
   msg_so("   * no     Follow normal harvesting rules.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (never harvest exports).\n");
   p = getenv("A2N_EXPORTS_ONLY");
   if (p == NULL)  {
      p = getenv("a2n_exports_only");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_SYMOPT_DEBUG\n");
   msg_so(" ----------------\n");
   msg_so("   Controls whether or not the SYMOPT_DEBUG option is set for DbgHelp.dll.\n");
   msg_so("   * yes    Set SYMOPT_DEBUG option. Will display DbgHelp debug messages.\n");
   msg_so("   * no     Don't set SYMOPT_DEBUG option.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (don't set SYMOPT_DEBUG option).\n");
   p = getenv("A2N_SYMOPT_DEBUG");
   if (p == NULL)  {
      p = getenv("a2n_symopt_debug");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_SYMOPT_LOAD_ANYTHING\n");
   msg_so(" ------------------------\n");
   msg_so("   Controls whether or not the A2N_SYMOPT_LOAD_ANYTHING option is set for DbgHelp.dll.\n");
   msg_so("   * yes    Set A2N_SYMOPT_LOAD_ANYTHING option. Allows loading any symbols.\n");
   msg_so("   * no     Don't set A2N_SYMOPT_LOAD_ANYTHING option.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (don't set A2N_SYMOPT_LOAD_ANYTHING option).\n");
   p = getenv("A2N_SYMOPT_LOAD_ANYTHING");
   if (p == NULL)  {
      p = getenv("a2n_symopt_load_anything");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

         G(symopt_debug) = 1;
         G(symopt_load_anything) = 1;
#endif

   msg_so(" A2N_ALWAYS_HARVEST\n");
   msg_so(" ------------------\n");
   msg_so("   Controls whether or not an attempt to harvest symbols is made.\n");
   msg_so("   * yes    Always harvest symbols, regardless of quality.\n");
   msg_so("   * no     Only harvest if desired quality symbols available.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (harvest only if desired quality).\n");
   p = getenv("A2N_ALWAYS_HARVEST");
   if (p == NULL)  {
      p = getenv("a2n_always_harvest");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_SNP_ELEMENTS\n");
   msg_so(" ----------------\n");
   msg_so("   * number   Sets number of elements in staging Symbol Node Pointer array.\n");
   msg_so("              ***** You should not be mucking with this one! *****\n");
   p = getenv("A2N_SNP_ELEMENTS");
   if (p == NULL)  {
      p = getenv("a2n_snp_elements");
   }
   if (p == NULL)
      msg_so("   -------> current value: %s\n", ns);
   else
      msg_so("   -------> current value: %d\n", atoi(p));
   msg_so("\n");

   msg_so(" A2N_SEARCH_PATH\n");
   msg_so(" ---------------\n");
   msg_so("   Sets the path string to be pre-pended to the default symbol\n");
   msg_so("   search path. Useful if symbols are not in the default location(s).\n");
   msg_so("   Value is a path string, each path separated by either ':' on Linux\n");
   msg_so("   or ';' on Windows.\n");
   msg_so("   There is no default for this variable.\n");
   msg_so("\n");
   msg_so("   Search path can also be specified via post with \"-sympath\" option.\n");
   p = getenv("A2N_SEARCH_PATH");
   if (p == NULL)  {
      p = getenv("a2n_search_path");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

#if defined (_LINUX)
   msg_so(" A2N_KERNEL_IMAGE_NAME\n");
   msg_so(" ---------------------\n");
   msg_so("   Sets the fully qualified name of the non-stripped kernel image file.\n");
   msg_so("   Default:   \"%s\"\n", DEFAULT_KERNEL_IMAGENAME);
   msg_so("\n");
   msg_so("   Image name can also be specified via post with \"-k\" option.\n");
   p = getenv("A2N_KERNEL_IMAGE_NAME");
   if (p == NULL)  {
      p = getenv("a2n_kernel_image_name");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_KERNEL_MAP_NAME\n");
   msg_so(" -------------------\n");
   msg_so("   Sets the fully qualified name of the kernel map file.\n");
   msg_so("   Default:   \"%s\"\n", DEFAULT_KERNEL_MAPNAME);
   p = getenv("A2N_KERNEL_MAP_NAME");
   if (p == NULL)  {
      p = getenv("a2n_kernel_map_name");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_KALLSYMS_NAME\n");
   msg_so(" A2N_KALLSYMS_OFFSET\n");
   msg_so(" A2N_KALLSYMS_SIZE\n");
   msg_so(" -------------------\n");
   msg_so("   Describe the location of the 'kallsyms' file to use for harvesting\n");
   msg_so("   kernel and kernel modules symbols. Why can't we use /proc/kallsyms?\n");
   msg_so("   We can, but PI sometimes copies /proc/kallsyms to some other location\n");
   msg_so("   and wants to use that copy for symbol resolution. These variables are\n");
   msg_so("   here so they can tell use what file to use and where/how much to read.\n");
   msg_so("   * A2N_KALLSYMS_OFFSET:\n");
   msg_so("     - Sets the name of the file to use.\n");
   msg_so("   * A2N_KALLSYMS_OFFSET\n");
   msg_so("     - Sets the offset within the file where to begin reading.\n");
   msg_so("     - Default is 0.\n");
   msg_so("   * A2N_KALLSYMS_SIZE\n");
   msg_so("     - Sets the number of bytes to read. 0 means: read to EOF.\n");
   msg_so("     - Default is 0 (to EOF).\n");
   msg_so("   ex: set A2N_KALLSYMS_NAME=/pi/bin/swtrace.nrm2\n");
   msg_so("       set A2N_KALLSYMS_OFFSET=512067\n");
   msg_so("       set A2N_KALLSYMS_SIZE=537351\n");
   msg_so("       Extract the kallsyms file from /pi/bin/swtrace.nrm2 at offset\n");
   msg_so("       512067 for 537351 bytes.\n");
   msg_so("   ex: set A2N_KALLSYMS_NAME=/proc/kallsyms\n");
   msg_so("       set A2N_KALLSYMS_OFFSET=0\n");
   msg_so("       set A2N_KALLSYMS_SIZE=0\n");
   msg_so("       Use the real /proc/kallsyms file as is.\n");
   msg_so("   There is no default for this variable.\n");
   msg_so("   Obviously, it doesn't do anything on Windows.\n");
   p = getenv("A2N_KALLSYMS_NAME");
   if (p == NULL)  {
      p = getenv("a2n_kallsyms_name");
   }
   msg_so("   -------> A2N_KALLSYMS_NAME current value: %s\n", (p == NULL) ? ns : p);
   p = getenv("A2N_KALLSYMS_OFFSET");
   if (p == NULL)  {
      p = getenv("a2n_kallsyms_offset");
   }
   msg_so("   -------> A2N_KALLSYMS_OFFSET current value: %s\n", (p == NULL) ? ns : p);
   p = getenv("A2N_KALLSYMS_SIZE");
   if (p == NULL)  {
      p = getenv("a2n_kallsyms_size");
   }
   msg_so("   -------> A2N_KALLSYMS_SIZE current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_MODULES_NAME\n");
   msg_so(" A2N_MODULES_OFFSET\n");
   msg_so(" A2N_MODULES_SIZE\n");
   msg_so(" -------------------\n");
   msg_so("   Describe the location of the 'modules' file to use for harvesting\n");
   msg_so("   kernel modules symbols. Why can't we use /proc/modules?\n");
   msg_so("   We can, but PI sometimes copies /proc/modules to some other location\n");
   msg_so("   and wants to use that copy for symbol resolution. These variables are\n");
   msg_so("   here so they can tell use what file to use and where/how much to read.\n");
   msg_so("   * A2N_MODULES_OFFSET:\n");
   msg_so("     - Sets the name of the file to use.\n");
   msg_so("   * A2N_MODULES_OFFSET\n");
   msg_so("     - Sets the offset within the file where to begin reading.\n");
   msg_so("     - Default is 0.\n");
   msg_so("   * A2N_MODULES_SIZE\n");
   msg_so("     - Sets the number of bytes to read. 0 means: read to EOF.\n");
   msg_so("     - Default is 0 (to EOF).\n");
   msg_so("   ex: set A2N_MODULES_NAME=/pi/bin/swtrace.nrm2 456780 805\n");
   msg_so("       set A2N_MODULES_OFFSET=456780\n");
   msg_so("       set A2N_MODULES_SIZE=805\n");
   msg_so("       Extract the modules file from /pi/bin/swtrace.nrm2 at offset\n");
   msg_so("       456780 for 805 bytes.\n");
   msg_so("   ex: set A2N_MODULES_NAME=/proc/modules\n");
   msg_so("       set A2N_MODULES_OFFSET=0\n");
   msg_so("       set A2N_MODULES_SIZE=0\n");
   msg_so("       Use the real /proc/modules file as is.\n");
   msg_so("   There is no default for this variable.\n");
   msg_so("   Obviously, it doesn't do anything on Windows.\n");
   p = getenv("A2N_MODULES_NAME");
   if (p == NULL)  {
      p = getenv("a2n_modules_name");
   }
   msg_so("   -------> A2N_MODULES_NAME current value: %s\n", (p == NULL) ? ns : p);
   p = getenv("A2N_MODULES_OFFSET");
   if (p == NULL)  {
      p = getenv("a2n_modules_offset");
   }
   msg_so("   -------> A2N_MODULES_OFFSET current value: %s\n", (p == NULL) ? ns : p);
   p = getenv("A2N_MODULES_SIZE");
   if (p == NULL)  {
      p = getenv("a2n_modules_size");
   }
   msg_so("   -------> A2N_MODULES_SIZE current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_DONT_USE_KERNEL_IMAGE\n");
   msg_so(" A2N_DONT_USE_KERNEL_MAP\n");
   msg_so(" A2N_DONT_USE_KALLSYMS\n");
   msg_so(" A2N_DONT_USE_MODULES\n");
   msg_so(" -------------------------\n");
   msg_so("   Override where to get kernel/kernel modules symbols from.n");
   msg_so("   If set, they override whatever the APIs or other environment\n");
   msg_so("   variables have set up.\n");
   msg_so("   * A2N_DONT_USE_KERNEL_IMAGE:\n");
   msg_so("     - Never harvest kernel symbols from the kernel image.\n");
   msg_so("   * A2N_DONT_USE_KERNEL_MAP\n");
   msg_so("     - Never harvest kernel symbols from the kernel (system) map.\n");
   msg_so("   * A2N_DONT_USE_KALLSYMS\n");
   msg_so("     - Never harvest kernel symbols from the /proc/kallsyms file.\n");
   msg_so("   * A2N_DONT_USE_MODULES\n");
   msg_so("     - Never harvest kernel module symbols.\n");
   msg_so("   There is no default for this variable.\n");
   msg_so("   Obviously, it doesn't do anything on Windows.\n");
   p = getenv("A2N_DONT_USE_KERNEL_IMAGE");
   if (p == NULL)  {
      p = getenv("a2n_dont_use_kernel_image");
   }
   msg_so("   -------> A2N_DONT_USE_KERNEL_IMAGE current value: %s\n", (p == NULL) ? ns : p);
   p = getenv("A2N_DONT_USE_KERNEL_MAP");
   if (p == NULL)  {
      p = getenv("a2n_dont_use_kernel_map");
   }
   msg_so("   -------> A2N_DONT_USE_KERNEL_MAP current value: %s\n", (p == NULL) ? ns : p);
   p = getenv("A2N_DONT_USE_KALLSYMS");
   if (p == NULL)  {
      p = getenv("a2n_dont_use_kallsyms");
   }
   msg_so("   -------> A2N_DONT_USE_KALLSYMS current value: %s\n", (p == NULL) ? ns : p);
   p = getenv("A2N_DONT_USE_MODULES");
   if (p == NULL)  {
      p = getenv("a2n_dont_use_modules");
   }
   msg_so("   -------> A2N_DONT_USE_MODULES current value: %s\n", (p == NULL) ? ns : p);
#endif

   msg_so("\n");
   msg_so(" A2N_BLANK_SUB_CHAR\n");
   msg_so(" ------------------\n");
   msg_so("   Sets character to which you want blanks changed in symbol and\n");
   msg_so("   module names. For example, if set to '@' and a name is 'name 1',\n");
   msg_so("   the name is converted to 'name@1'.\n");
   msg_so("   If not set, the default is to change blanks to '%c'.\n", DEFAULT_BLANK_SUB);
   p = getenv("A2N_BLANK_SUB_CHAR");
   if (p == NULL)  {
      p = getenv("a2n_blank_sub_char");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_NO_FAST_LOOKUP\n");
   msg_so(" ------------------\n");
   msg_so("   Enable/disable fast symbol lookups.\n");
   msg_so("   * yes    Disable fast symbol lookup. (no yes => no)\n");
   msg_so("   * no     Enable fast symbol lookup. (no no => yes)\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (always attempt fast lookup).\n");
   p = getenv("A2N_NO_FAST_LOOKUP");
   if (p == NULL)  {
      p = getenv("a2n_no_fast_lookup");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

#if defined(_X86)
   msg_so(" A2N_COLLAPSE_MMI\n");
   msg_so(" ----------------\n");
   msg_so("   Causes the MMI Interpreter symbol range to be collapsed into\n");
   msg_so("   a single symbol.  On IBM JVMs, the MMI Interpreter is bound\n");
   msg_so("   by symbols %s and %s. The default\n", MMI_RANGE_SYMSTART, MMI_RANGE_SYMEND);
   msg_so("   name for the range is %s.  You can override the name via the\n", DEFAULT_MMI_RANGE_NAME);
   msg_so("   A2N_MMI_RANGE_NAME environment variable.\n");
   msg_so("   * yes    Collapse MMI Interpreter symbol range\n");
   msg_so("   * no     Leave MMI Interpreter alone.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (do not collapse symbol range).\n");
   p = getenv("A2N_COLLAPSE_MMI");
   if (p == NULL)  {
      p = getenv("a2n_collapse_mmi");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_MMI_RANGE_NAME\n");
   msg_so(" ------------------\n");
   msg_so("   Sets the name of the collaped symbol range for the MMI Interpreter.\n");
   msg_so("   Only used if A2N_COLLAPSE_MMI environment variable is set to YES.\n");
   msg_so("   You can enter any name you want.\n");
   msg_so("   If not set, the default name used is %s.\n", DEFAULT_MMI_RANGE_NAME);
   p = getenv("A2N_MMI_RANGE_NAME");
   if (p == NULL)  {
      p = getenv("a2n_mmi_range_name");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");
#endif

   msg_so(" A2N_COLLAPSE_RANGE\n");
   msg_so(" ------------------\n");
   msg_so("   Describes a range of symbols the user wishes to collapse into\n");
   msg_so("   one symbol. The variable must be set to a string with at least 3\n");
   msg_so("   values separated by blanks. A 4th value is optional. The values,\n");
   msg_so("   from left to right, are as follows:\n");
   msg_so("   * value1  filename of module that contains the range.\n");
   msg_so("   * value2  name of first symbol in the range, case sensitive,\n");
   msg_so("   * value3  name of last symbol in the range, case sensitive,\n");
   msg_so("   * value4  name to be given to range. If not present the name\n");
   msg_so("             of the range becomes value2.\n");
   msg_so("   ex: set A2N_COLLAPSE_RANGE=itracec.exe func1 func6 AllFunctions\n");
   msg_so("       Creates a range in module itracec.exe, named AllFunctions,\n");
   msg_so("       which contains all symbols between func1 and func6 inclusive.\n");
   msg_so("       The symbol name returned by A2nGetSymbol() for any address\n");
   msg_so("       contained in the range will be AllFunctions.\n");
   msg_so("   ex: set A2N_COLLAPSE_RANGE=itracec.exe func1 func6\n");
   msg_so("       Creates a range in module itracec.exe, named func1,\n");
   msg_so("       which contains all symbols between func1 and func6 inclusive.\n");
   msg_so("       The symbol name returned by A2nGetSymbol() for any address\n");
   msg_so("       contained in the range will be func1.\n");
   msg_so("   NOTE:\n");
   msg_so("     If you need to define more than one symbol range then you must\n");
   msg_so("     create a \"range description\" file containing a description for\n");
   msg_so("     each range, and specify the filename via A2N_RANGE_DESC_FILENAME.\n");
   msg_so("   There is no default for this variable.\n");
   p = getenv("A2N_COLLAPSE_RANGE");
   if (p == NULL)  {
      p = getenv("a2n_collapse_range");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_RANGE_DESC_FILENAME\n");
   msg_so(" -----------------------\n");
   msg_so("   Sets the name of a file containing symbol range descriptions.\n");
   msg_so("   Useful if you have more than one range to describe and can't\n");
   msg_so("   do it with just A2N_COLLAPSE_RANGE.\n");
   msg_so("   The file contains one range description per line, each description\n");
   msg_so("   being identical to the values described above for A2N_COLLAPSE_RANGE.\n");
   msg_so("   There is no default for this variable.\n");
   p = getenv("A2N_RANGE_DESC_FILENAME");
   if (p == NULL)  {
      p = getenv("a2n_range_desc_filename");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_RETURN_RANGE_SYMBOLS\n");
   msg_so(" ------------------------\n");
   msg_so("   Controls whether or not individual symbols within a user-defined\n");
   msg_so("   symbol range are returned as part of the symbol.\n");
   msg_so("   * yes    Append symbol name to range name (ie. range.symbol).\n");
   msg_so("   * no     Only return the range name.\n");
   msg_so("   * ????   (none of the above): Same as yes.\n");
   msg_so("   If not set, the default is NO (only return range name).\n");
   p = getenv("A2N_RETURN_RANGE_SYMBOLS");
   if (p == NULL)  {
      p = getenv("a2n_return_range_symbols");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

#if defined(_WINDOWS)
   msg_so(" A2N_JVMMAP_NAME\n");
   msg_so(" ---------------\n");
   msg_so("   Sets the name of jvmmap file containing JVM symbols. It can either\n");
   msg_so("   be the name of the compressed file (typically \"jvmmap\" or the\n");
   msg_so("   uncompressed file (typically \"jvmmap.X\". A2N will uncompress \n");
   msg_so("   compressed files *ONLY IF* either unzip or jar is accessible (via PATH).\n");
   msg_so("   A2N only looks for the jvmmap file in the SDK's \\jre\\bin directory.\n");
   msg_so("   If not set, the default name used is %s.\n", DEFAULT_JVMMAP_NAME);
   p = getenv("A2N_JVMMAP_NAME");
   if (p == NULL)  {
      p = getenv("a2n_jvmmap_name");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");

   msg_so(" A2N_SPECIAL_DLLS\n");
   msg_so(" ----------------\n");
   msg_so("   Path string, containing DLL names (with the .DLL extension), that\n");
   msg_so("   reside in th %%SystemRoot%%\\System32 directory but whose path\n");
   msg_so("   information is missing/incomplete in the MTE. If you see one you'll\n");
   msg_so("   know it because A2N will not be able to find symbols for it.\n");
   msg_so("   If not set, the default list of DLLs is:\n");
   msg_so("   * %s\n", DEFAULT_SPECIAL_DLLS1);
   msg_so("   * %s\n", DEFAULT_SPECIAL_DLLS2);
   msg_so("   * %s\n", DEFAULT_SPECIAL_DLLS3);
   p = getenv("A2N_SPECIAL_DLLS");
   if (p == NULL)  {
      p = getenv("a2n_special_dlls");
   }
   msg_so("   -------> current value: %s\n", (p == NULL) ? ns : p);
   msg_so("\n");
#endif

   return;
}



//****************************************************************************//
//****************************************************************************//
//                                                                            //
//    "Undocumented/private/debug/official use only" external interfaces      //
//                                                                            //
//****************************************************************************//
//****************************************************************************//

static char * SYMBOL_TYPE_UNKNOWN_str =                 " ???";
static char * SYMBOL_TYPE_NONE_str =                    "   -";
static char * SYMBOL_TYPE_USER_SUPPLIED_str =           "User";
static char * SYMBOL_TYPE_JITTED_METHOD_str =           "  JM";
static char * SYMBOL_TYPE_FUNCTION_str =                " Fnc";
static char * SYMBOL_TYPE_LABEL_str =                   " Lbl";
static char * SYMBOL_TYPE_FUNCTION_WEAK_str =           "WFnc";
static char * SYMBOL_TYPE_ELF_PLT_str =                 " PLT";
static char * SYMBOL_TYPE_EXPORT_str =                  " Exp";
static char * SYMBOL_TYPE_FORWARDER_str =               " Fwd";
static char * SYMBOL_TYPE_OBJECT_str =                  " Obj";
static char * SYMBOL_TYPE_GENERATED_str =               " Gen";
static char * SYMBOL_TYPE_SYMBOL_MS_PDB_str =           "*pdb";
static char * SYMBOL_TYPE_SYMBOL_MS_COFF_str =          "*cof";
static char * SYMBOL_TYPE_SYMBOL_MS_CV_str =            " *cv";
static char * SYMBOL_TYPE_SYMBOL_MS_SYM_str =           "*sym";
static char * SYMBOL_TYPE_SYMBOL_MS_EXP_str =           "*exp";
static char * SYMBOL_TYPE_SYMBOL_MS_DIA_str =           "*dia";
static char * SYMBOL_TYPE_PE_ILT_str =                  " ILT";


static char * MODULE_TYPE_UNKNOWN_small_str =           " ?";
static char * MODULE_TYPE_DOS_small_str =               "Cm";
static char * MODULE_TYPE_PE_small_str =                "PE";
static char * MODULE_TYPE_LX_small_str =                "LX";
static char * MODULE_TYPE_VXD_small_str =               "VX";
static char * MODULE_TYPE_DBG_small_str =               "DB";
static char * MODULE_TYPE_SYM_small_str =               "SY";
static char * MODULE_TYPE_ELF_REL_small_str =           "R3";
static char * MODULE_TYPE_ELF_EXEC_small_str =          "E3";
static char * MODULE_TYPE_ELF64_REL_small_str =         "R6";
static char * MODULE_TYPE_ELF64_EXEC_small_str =        "E6";
static char * MODULE_TYPE_MAP_VC_small_str =            "Mc";
static char * MODULE_TYPE_MAP_VA_small_str =            "Ma";
static char * MODULE_TYPE_MAP_NM_small_str =            "Mn";
static char * MODULE_TYPE_MAP_OBJDUMP_small_str =       "Mo";
static char * MODULE_TYPE_MAP_READELF_small_str =       "Mr";
static char * MODULE_TYPE_JITTED_CODE_small_str =       "JM";
static char * MODULE_TYPE_MYS_small_str =               "YS";
static char * MODULE_TYPE_PDB_small_str =               "PD";
static char * MODULE_TYPE_LE_small_str =                "LE";
static char * MODULE_TYPE_NE_small_str =                "NE";
static char * MODULE_TYPE_DBGPDB_small_str =            "DP";
static char * MODULE_TYPE_MAP_JVMX_small_str =          "Mx";
static char * MODULE_TYPE_MAP_JVMC_small_str =          "Mj";
static char * MODULE_TYPE_KALLSYMS_MODULE_small_str =   "Km";
static char * MODULE_TYPE_KALLSYMS_VMLINUX_small_str =  "Kv";
static char * MODULE_TYPE_VSYSCALL32_small_str =        "V3";
static char * MODULE_TYPE_VSYSCALL64_small_str =        "V6";
static char * MODULE_TYPE_ANON_small_str =              "An";

static char * MODULE_TYPE_UNKNOWN_str =                 "Unknown";
static char * MODULE_TYPE_DOS_str =                     "DOS Command";
static char * MODULE_TYPE_PE_str =                      "PE Executable";
static char * MODULE_TYPE_LX_str =                      "LX Executable";
static char * MODULE_TYPE_VXD_str =                     "VXD";
static char * MODULE_TYPE_DBG_str =                     "DBG File";
static char * MODULE_TYPE_SYM_str =                     "SYM File";
static char * MODULE_TYPE_ELF_REL_str =                 "ELF32 Relocatable";
static char * MODULE_TYPE_ELF_EXEC_str =                "ELF32 Executable";
static char * MODULE_TYPE_ELF64_REL_str =               "ELF64 Relocatable";
static char * MODULE_TYPE_ELF64_EXEC_str =              "ELF64 Executable";
static char * MODULE_TYPE_MAP_VC_str =                  "VisualC Map";
static char * MODULE_TYPE_MAP_VA_str =                  "VisualAge Map";
static char * MODULE_TYPE_MAP_NM_str =                  "nm Map";
static char * MODULE_TYPE_MAP_OBJDUMP_str =             "objdump Map";
static char * MODULE_TYPE_MAP_READELF_str =             "readelf Map";
static char * MODULE_TYPE_JITTED_CODE_str =             "JittedMethod";
static char * MODULE_TYPE_MYS_str =                     "MYS File";
static char * MODULE_TYPE_PDB_str =                     "PDB File";
static char * MODULE_TYPE_LE_str =                      "OS/2 LE or Windows VXD";
static char * MODULE_TYPE_NE_str =                      "OS/2 NE";
static char * MODULE_TYPE_DBGPDB_str =                  "DBG/PDB File Combination";
static char * MODULE_TYPE_MAP_JVMX_str =                ".jmap from jvmmap.X";
static char * MODULE_TYPE_MAP_JVMC_str =                "Compressed jvmmap";
static char * MODULE_TYPE_KALLSYMS_MODULE_str =         "kallsyms Kernel Module";
static char * MODULE_TYPE_KALLSYMS_VMLINUX_str =        "kallsyms vmlinux";
static char * MODULE_TYPE_VSYSCALL32_str =              "32-bit virtual syscall segment";
static char * MODULE_TYPE_VSYSCALL64_str =              "64-bit virtual syscall segment";
static char * MODULE_TYPE_ANON_str =                    "Anonymous segment";


static char * GetSymbolTypeString(uint type)
{
   switch(type) {
      case SYMBOL_TYPE_USER_SUPPLIED:
         return(SYMBOL_TYPE_USER_SUPPLIED_str);
      case SYMBOL_TYPE_JITTED_METHOD:
         return(SYMBOL_TYPE_JITTED_METHOD_str);
      case SYMBOL_TYPE_FUNCTION:
         return(SYMBOL_TYPE_FUNCTION_str);
      case SYMBOL_TYPE_LABEL:
         return(SYMBOL_TYPE_LABEL_str);
      case SYMBOL_TYPE_FUNCTION_WEAK:
         return(SYMBOL_TYPE_FUNCTION_WEAK_str);
      case SYMBOL_TYPE_ELF_PLT:
         return(SYMBOL_TYPE_ELF_PLT_str);
      case SYMBOL_TYPE_EXPORT:
         return(SYMBOL_TYPE_EXPORT_str);
      case SYMBOL_TYPE_FORWARDER:
         return(SYMBOL_TYPE_FORWARDER_str);
      case SYMBOL_TYPE_OBJECT:
         return(SYMBOL_TYPE_OBJECT_str);
      case SYMBOL_TYPE_GENERATED:
         return(SYMBOL_TYPE_GENERATED_str);
      case SYMBOL_TYPE_MS_PDB:
         return(SYMBOL_TYPE_SYMBOL_MS_PDB_str);
      case SYMBOL_TYPE_MS_COFF:
         return(SYMBOL_TYPE_SYMBOL_MS_COFF_str);
      case SYMBOL_TYPE_MS_CODEVIEW:
         return(SYMBOL_TYPE_SYMBOL_MS_CV_str);
      case SYMBOL_TYPE_MS_SYM:
         return(SYMBOL_TYPE_SYMBOL_MS_SYM_str);
      case SYMBOL_TYPE_MS_EXPORT:
         return(SYMBOL_TYPE_SYMBOL_MS_EXP_str);
      case SYMBOL_TYPE_MS_DIA:
         return(SYMBOL_TYPE_SYMBOL_MS_DIA_str);
      case SYMBOL_TYPE_PE_ILT:
         return(SYMBOL_TYPE_PE_ILT_str);
      case SYMBOL_TYPE_NONE:
         return(SYMBOL_TYPE_NONE_str);
      default:
         return(SYMBOL_TYPE_UNKNOWN_str);
   }
}


static char * GetModuleTypeShortString(uint type)
{
   switch(type) {
      case MODULE_TYPE_DOS:
         return(MODULE_TYPE_DOS_small_str);
      case MODULE_TYPE_PE:
         return(MODULE_TYPE_PE_small_str);
      case MODULE_TYPE_LX:
         return(MODULE_TYPE_LX_small_str);
      case MODULE_TYPE_VXD:
         return(MODULE_TYPE_VXD_small_str);
      case MODULE_TYPE_DBG:
         return(MODULE_TYPE_DBG_small_str);
      case MODULE_TYPE_SYM:
         return(MODULE_TYPE_SYM_small_str);
      case MODULE_TYPE_ELF_REL:
         return(MODULE_TYPE_ELF_REL_small_str);
      case MODULE_TYPE_ELF_EXEC:
         return(MODULE_TYPE_ELF_EXEC_small_str);
      case MODULE_TYPE_ELF64_REL:
         return(MODULE_TYPE_ELF64_REL_small_str);
      case MODULE_TYPE_ELF64_EXEC:
         return(MODULE_TYPE_ELF64_EXEC_small_str);
      case MODULE_TYPE_MAP_VC:
         return(MODULE_TYPE_MAP_VC_small_str);
      case MODULE_TYPE_MAP_VA:
         return(MODULE_TYPE_MAP_VA_small_str);
      case MODULE_TYPE_MAP_NM:
         return(MODULE_TYPE_MAP_NM_small_str);
      case MODULE_TYPE_MAP_OBJDUMP:
         return(MODULE_TYPE_MAP_OBJDUMP_small_str);
      case MODULE_TYPE_MAP_READELF:
         return(MODULE_TYPE_MAP_READELF_small_str);
      case MODULE_TYPE_JITTED_CODE:
         return(MODULE_TYPE_JITTED_CODE_small_str);
      case MODULE_TYPE_MYS:
         return(MODULE_TYPE_MYS_small_str);
      case MODULE_TYPE_PDB:
         return(MODULE_TYPE_PDB_small_str);
      case MODULE_TYPE_LE:
         return(MODULE_TYPE_LE_small_str);
      case MODULE_TYPE_NE:
         return(MODULE_TYPE_NE_small_str);
      case MODULE_TYPE_DBGPDB:
         return(MODULE_TYPE_DBGPDB_small_str);
      case MODULE_TYPE_MAP_JVMX:
         return(MODULE_TYPE_MAP_JVMX_small_str);
      case MODULE_TYPE_MAP_JVMC:
         return(MODULE_TYPE_MAP_JVMC_small_str);
      case MODULE_TYPE_KALLSYMS_MODULE:
         return (MODULE_TYPE_KALLSYMS_MODULE_small_str);
      case MODULE_TYPE_KALLSYMS_VMLINUX:
         return (MODULE_TYPE_KALLSYMS_VMLINUX_small_str);
      case MODULE_TYPE_VSYSCALL32:
         return (MODULE_TYPE_VSYSCALL32_small_str);
      case MODULE_TYPE_VSYSCALL64:
         return (MODULE_TYPE_VSYSCALL64_small_str);
      case MODULE_TYPE_ANON:
         return (MODULE_TYPE_ANON_small_str);
      default:
         return(MODULE_TYPE_UNKNOWN_small_str);
   }
}


static char * GetModuleTypeLongString(uint type)
{
   switch(type) {
      case MODULE_TYPE_DOS:
         return(MODULE_TYPE_DOS_str);
      case MODULE_TYPE_PE:
         return(MODULE_TYPE_PE_str);
      case MODULE_TYPE_LX:
         return(MODULE_TYPE_LX_str);
      case MODULE_TYPE_VXD:
         return(MODULE_TYPE_VXD_str);
      case MODULE_TYPE_DBG:
         return(MODULE_TYPE_DBG_str);
      case MODULE_TYPE_SYM:
         return(MODULE_TYPE_SYM_str);
      case MODULE_TYPE_ELF_REL:
         return(MODULE_TYPE_ELF_REL_str);
      case MODULE_TYPE_ELF_EXEC:
         return(MODULE_TYPE_ELF_EXEC_str);
      case MODULE_TYPE_ELF64_REL:
         return(MODULE_TYPE_ELF64_REL_str);
      case MODULE_TYPE_ELF64_EXEC:
         return(MODULE_TYPE_ELF64_EXEC_str);
      case MODULE_TYPE_MAP_VC:
         return(MODULE_TYPE_MAP_VC_str);
      case MODULE_TYPE_MAP_VA:
         return(MODULE_TYPE_MAP_VA_str);
      case MODULE_TYPE_MAP_NM:
         return(MODULE_TYPE_MAP_NM_str);
      case MODULE_TYPE_MAP_OBJDUMP:
         return(MODULE_TYPE_MAP_OBJDUMP_str);
      case MODULE_TYPE_MAP_READELF:
         return(MODULE_TYPE_MAP_READELF_str);
      case MODULE_TYPE_JITTED_CODE:
         return(MODULE_TYPE_JITTED_CODE_str);
      case MODULE_TYPE_MYS:
         return(MODULE_TYPE_MYS_str);
      case MODULE_TYPE_PDB:
         return(MODULE_TYPE_PDB_str);
      case MODULE_TYPE_LE:
         return(MODULE_TYPE_LE_str);
      case MODULE_TYPE_NE:
         return(MODULE_TYPE_NE_str);
      case MODULE_TYPE_DBGPDB:
         return(MODULE_TYPE_DBGPDB_str);
      case MODULE_TYPE_MAP_JVMX:
         return(MODULE_TYPE_MAP_JVMX_str);
      case MODULE_TYPE_MAP_JVMC:
         return(MODULE_TYPE_MAP_JVMC_str);
      case MODULE_TYPE_KALLSYMS_MODULE:
         return (MODULE_TYPE_KALLSYMS_MODULE_str);
      case MODULE_TYPE_KALLSYMS_VMLINUX:
         return (MODULE_TYPE_KALLSYMS_VMLINUX_str);
      case MODULE_TYPE_VSYSCALL32:
         return (MODULE_TYPE_VSYSCALL32_str);
      case MODULE_TYPE_VSYSCALL64:
         return (MODULE_TYPE_VSYSCALL64_str);
      case MODULE_TYPE_ANON:
         return (MODULE_TYPE_ANON_str);
      default:
         return(MODULE_TYPE_UNKNOWN_str);
   }
}


static char * Requested_str =    "R";
static char * NotRequested_str = " ";


static char * GetRequestedString(uint flags)
{
   if (flags & A2N_FLAGS_REQUESTED)
      return (Requested_str);
   else
      return (NotRequested_str);
}


static char * Inherited_str =    "I";
static char * NotInherited_str = " ";


static char * GetInheritedString(uint flags)
{
   if (flags & A2N_FLAGS_INHERITED)
      return (Inherited_str);
   else
      return (NotInherited_str);
}


static char * HarvestedFromModule_str = "H ";
static char * HarvestedFromOther_str =  "H*";
static char * NotHarvested_str =        "  ";


static char * GetHarvestedString(MOD_NODE * mn)
{
   if (mn->flags & A2N_FLAGS_SYMBOLS_HARVESTED) {
      if (mn->hvname != NULL  &&  COMPARE_NAME(mn->name, mn->hvname) == 0)
         return (HarvestedFromModule_str);
      else
         return (HarvestedFromOther_str);
   }
   else
      return (NotHarvested_str);
}


//
// DumpGlobals()
// *************
//
// Dump global information
//
static void DumpGlobals(FILE * fh)
{
   PID_NODE * pn;
   MOD_NODE * mn;
   RDESC_NODE * rd;
   int tot_lm = 0, tot_ldlm = 0, tot_mod = 0, base_sym = 0;
   int tot_sym = 0, tot_sec = 0, tot_str = 0, tot_code = 0;
   int err_other, total_fast;
   int pid_mem, lastsym_mem, jm_mem, lm_mem, mod_mem, sym_mem, basesnp_mem, sec_mem, tot_mem = 0;
   int i;
   int dump_to_a2n_mod_proc = 0;            // assume dumping to a2n.err
   char * ct, * cr, * ch;


   // Calculate module totals (what we'd be writing to file on A2nSaveModules)
   mn = G(ModRoot);
   while (mn) {
      tot_mod++;
      base_sym += mn->base_symcnt;
      tot_sym += mn->symcnt;
      tot_sec += mn->seccnt;
      tot_str += mn->strsize;
      tot_code += mn->codesize;
      mn = mn->next;
   } // while

   // Calculate total loaded modules
   pn = G(PidRoot);
   while (pn) {
      tot_ldlm += pn->lmcnt;
      pn = pn->next;
   } // while
   tot_lm = tot_ldlm;

   if (fh == G(msgfh)) {
      // Called to dump data in a2n.err. We don't display everything.
      FilePrint(fh, "\n\n############################################################################\n");
      FilePrint(fh, "###                       Memory and API statistics                      ###\n");
      FilePrint(fh, "############################################################################\n");
   }
   else {
      // Called to dump data in a2n.mod or a2n.proc. We display everything.
      dump_to_a2n_mod_proc = 1;
      FilePrint(fh, "********** %s A2N Version %d.%d.%d **********\n", _PLATFORM_STR, V_MAJOR, V_MINOR, V_REV);
   }

   // Write to a2n.err, a2n.mod and a2n.proc
   FilePrint(fh, "        PidCnt = %d\n", G(PidCnt));
   FilePrint(fh, "     ReusedCnt = %d\n", G(ReusedPidCnt));
   FilePrint(fh, "       LastSym = %d\n", G(PidCnt));
   FilePrint(fh, "     JmPoolCnt = %d\n", G(JmPoolCnt));
   FilePrint(fh, "      LdModCnt = %d\n", tot_lm);
   FilePrint(fh, "        ModCnt = %d\n", G(ModCnt));
   FilePrint(fh, "        SymCnt = %d  (Base = %d)\n", tot_sym, base_sym);
   FilePrint(fh, "        SecCnt = %d\n", tot_sec);
   FilePrint(fh, "      SNP Elem = %d\n", G(snp_elements));

   FilePrint(fh, "\n");
   pid_mem = (G(PidCnt) + G(ReusedPidCnt)) * sizeof(PID_NODE);   tot_mem += pid_mem;
   lastsym_mem = G(PidCnt) * sizeof(LASTSYM_NODE);               tot_mem += lastsym_mem;
   jm_mem = G(JmPoolCnt) * sizeof(JM_NODE);                      tot_mem += jm_mem;
   lm_mem = tot_lm * sizeof(LMOD_NODE);                          tot_mem += lm_mem;
   mod_mem = G(ModCnt) * sizeof(MOD_NODE);                       tot_mem += mod_mem;
   sym_mem = tot_sym * sizeof(SYM_NODE);                         tot_mem += sym_mem;
   basesnp_mem = base_sym * sizeof(SYM_NODE *);                  tot_mem += basesnp_mem;
   sec_mem = tot_sec * sizeof(SEC_NODE);                         tot_mem += sec_mem;
   tot_mem += tot_code + tot_str + G(snp_size);
   FilePrint(fh, "    Pid Memory = %8d bytes\n", pid_mem);
   FilePrint(fh, "LastSym Memory = %8d bytes\n", lastsym_mem);
   FilePrint(fh, " JmPool Memory = %8d bytes\n", jm_mem);
   FilePrint(fh, "  LdMod Memory = %8d bytes\n", lm_mem);
   FilePrint(fh, "    Mod Memory = %8d bytes\n", mod_mem);
   FilePrint(fh, "    Sym Memory = %8d bytes\n", sym_mem);
   FilePrint(fh, "    Sec Memory = %8d bytes\n", sec_mem);
   FilePrint(fh, "   Code Memory = %8d bytes\n", tot_code);
   FilePrint(fh, " String Memory = %8d bytes\n", tot_str);
   FilePrint(fh, "BaseSNP Memory = %8d bytes\n", basesnp_mem);
   FilePrint(fh, "    SNP Memory = %8d bytes\n", G(snp_size));
   FilePrint(fh, "                 --------\n");
   FilePrint(fh, "  Total Memory = %8d bytes\n", tot_mem);

   // Write to a2n.mod and a2n.proc only
   if (dump_to_a2n_mod_proc) {
      FilePrint(fh, "\n");
      FilePrint(fh, "SystemPid                = %d (0x%x)  ", G(SystemPid), G(SystemPid));
      FilePrint(fh, "%s\n", (G(SystemPidSet)) ? "**SET BY CALLER**" : "**ASSUMED**");

      FilePrint(fh, "SystemPidNode            = 0x%"_PZP"\n", G(SystemPidNode));

#if defined(_WINDOWS)
      FilePrint(fh, "\n");
      FilePrint(fh, "%%SystemRoot%%             = %s\n", SystemRoot);
      FilePrint(fh, "%%SystemDrive%%            = %s\n", SystemDrive);
      FilePrint(fh, "OS_System32              = %s\n", OS_System32);
      FilePrint(fh, "OS_SysWow64              = %s\n", OS_SysWow64);
      FilePrint(fh, "OS_WinSxS                = %s\n", OS_WinSxS);
      FilePrint(fh, "WinSysDir                = %s\n", WinSysDir);
      FilePrint(fh, "SpecialDlls              = %s\n\n", G(SpecialWindowsDlls));
      DumpDriveMapping(fh);
#endif

#if defined(_LINUX)
      FilePrint(fh, "\n");
      FilePrint(fh, "KernelImageName          = %s\n", (G(KernelImageName) == NULL) ? "*NONE*" : G(KernelImageName));
      FilePrint(fh, "use_kernel_image         = >> %d <<\n", G(use_kernel_image));
      FilePrint(fh, "ok_to_use_kernel_image   = ** %d **\n", G(ok_to_use_kernel_image));
      FilePrint(fh, "\n");
      FilePrint(fh, "KernelMapName            = %s\n", (G(KernelMapName) == NULL) ? "*NONE*" : G(KernelMapName));
      FilePrint(fh, "use_kernel_map           = >> %d <<\n", G(use_kernel_map));
      FilePrint(fh, "ok_to_use_kernel_map     = ** %d **\n", G(ok_to_use_kernel_map));
      FilePrint(fh, "\n");
      FilePrint(fh, "KallsymsFilename         = %s\n", (G(KallsymsFilename) == NULL) ? "*NONE*" : G(KallsymsFilename));
      FilePrint(fh, "use_kallsyms             = >> %d <<\n", G(use_kallsyms));
      FilePrint(fh, "ok_to_use_kallsyms       = ** %d **\n", G(ok_to_use_kallsyms));
      FilePrint(fh, "kallsyms_file_offset     = %d\n", G(kallsyms_file_offset));
      FilePrint(fh, "kallsyms_file_size       = %d\n", G(kallsyms_file_size));
      FilePrint(fh, "\n");
      FilePrint(fh, "ModulesFilename          = %s\n", (G(ModulesFilename) == NULL) ? "*NONE*" : G(ModulesFilename));
      FilePrint(fh, "use_modules              = >> %d <<\n", G(use_modules));
      FilePrint(fh, "ok_to_use_modules        = ** %d **\n", G(ok_to_use_modules));
      FilePrint(fh, "modules_file_offset      = %d\n", G(modules_file_offset));
      FilePrint(fh, "modules_file_size        = %d\n", G(modules_file_size));
      FilePrint(fh, "kernel_modules_added     = %d\n", G(kernel_modules_added));
      FilePrint(fh, "kernel_modules_harvested = %d\n", G(kernel_modules_harvested));
#endif

      FilePrint(fh, "\n");
      FilePrint(fh, "SymbolSearchPath         = %s\n", G(SymbolSearchPath));
      FilePrint(fh, "PathSeparator            = '%c'\n", G(PathSeparator));
      FilePrint(fh, "SearchPathSeparator      = '%c'\n", G(SearchPathSeparator));
      FilePrint(fh, "BlankSubstitutionChar    = '%c'\n", G(blank_sub));

      if (G(collapse_ibm_mmi)) {
         FilePrint(fh, "CollapseMMISymbolRange   = YES\n");
         FilePrint(fh, "MMISymbolRangeName       = %s\n", G(collapsed_mmi_range_name));
      }

      if (G(range_desc_root) != NULL) {
         FilePrint(fh, "\nRange Descriptors:\n");
         rd = G(range_desc_root);
         i = 1;
         while (rd) {
            FilePrint(fh, "  (%d) %s %s %s %s %s\n",
                         i, rd->filename, rd->mod_name, rd->sym_start,
                         rd->sym_end, rd->range_name);
            rd = rd->next;
            i++;
         }
         FilePrint(fh, "\n");
      }


#if defined(_WINDOWS)
      FilePrint(fh, "\n");
      FilePrint(fh, "jvmmap_cnt               = %d\n", G(jvmmap_cnt));
      FilePrint(fh, "jvmmap data              = %"_PP"\n", G(jvmmap_data));
      DumpJvmmaps(fh);
#endif

      FilePrint(fh, "\n");
      FilePrint(fh, "big_endian               = %d\n", G(big_endian));
      FilePrint(fh, "word_size                = %d\n", G(word_size));

      FilePrint(fh, "\n");
      FilePrint(fh, "debug                    = 0x%08x : ", G(debug));
      if (G(debug) == DEBUG_MODE_NONE)
         FilePrint(fh, "NONE\n");
      else {
         if (G(debug) & DEBUG_MODE_INTERNAL)     FilePrint(fh, "INTERNAL  ");
         if (G(debug) & DEBUG_MODE_HARVESTER)    FilePrint(fh, "HARVESTER  ");
         if (G(debug) & DEBUG_MODE_API)          FilePrint(fh, "API  ");
         FilePrint(fh,"\n");
      }

      FilePrint(fh, "return_range_symbols     = %d\n", G(return_range_symbols));
      FilePrint(fh, "validate_symbols         = %d\n", G(validate_symbols));
      FilePrint(fh, "quit_on_validation_error = %d\n", G(quit_on_validation_error));
      FilePrint(fh, "module_dump_on_exit      = %d\n", G(module_dump_on_exit));
      FilePrint(fh, "process_dump_on_exit     = %d\n", G(process_dump_on_exit));
      FilePrint(fh, "msi_dump_on_exit         = %d\n", G(msi_dump_on_exit));
      FilePrint(fh, "add_module_cache_head    = %d\n", G(add_module_cache_head));

      FilePrint(fh, "\n");
      FilePrint(fh, "display_error_messages   = %d: ", G(display_error_messages));
      if (G(display_error_messages) == MSG_NONE)        FilePrint(fh, "NONE\n");
      else if (G(display_error_messages) == MSG_ERROR)  FilePrint(fh, "ERROR and above\n");
      else if (G(display_error_messages) == MSG_WARN)   FilePrint(fh, "WARNING and above\n");
      else if (G(display_error_messages) == MSG_INFO)   FilePrint(fh, "INFORMATIONAL and above\n");
      else                                              FilePrint(fh, "ALL\n");

      FilePrint(fh, "immediate_gather         = %d\n", G(immediate_gather));
      FilePrint(fh, "gather_code              = %d\n", G(gather_code));
      FilePrint(fh, "symbol_quality           = ");
      if (G(symbol_quality) == MODE_ANY_SYMBOL)    FilePrint(fh, "ANY_SYMBOL\n");
      if (G(symbol_quality) == MODE_GOOD_SYMBOLS)  FilePrint(fh, "GOOD_SYMBOLS\n");

      FilePrint(fh, "return_line_numbers      = %d\n", G(lineno));
      FilePrint(fh, "demangle_cpp_names       = %d\n", G(demangle_cpp_names));
      FilePrint(fh, "rename_duplicate_symbols = %d\n", G(rename_duplicate_symbols));
      FilePrint(fh, "demangle_complete        = %d\n", G(demangle_complete));
      FilePrint(fh, "no_module_found dump threshold = %d\n", G(nmf_threshold));
      FilePrint(fh, "no_symbol_found dump threshold = %d\n", G(nsf_threshold));

      FilePrint(fh, "no_symbol_found_action   = ");
      if (G(nsf_action) == NSF_ACTION_NOSYM)         FilePrint(fh, "NSF_ACTION_NOSYM\n");
      else if (G(nsf_action) == NSF_ACTION_NSF)      FilePrint(fh, "NSF_ACTION_NSF\n");
      else if (G(nsf_action) == NSF_ACTION_SECTION)  FilePrint(fh, "NSF_ACTION_SECTION\n");
      else                                           FilePrint(fh, "??????????\n");

      FilePrint(fh, "\n");
      FilePrint(fh, "using_static_module_info = %d\n", G(using_static_module_info));
      FilePrint(fh, "changes_made             = %d (A2nAddModule + A2nAddSymbol + A2nAddSection)\n", G(changes_made));
   }

   // Write to a2n.err, a2n.mod and a2n.proc
   FilePrint(fh, "\n");
   FilePrint(fh, "                   Total       OK      Error\n");
   FilePrint(fh, "                  -------    ------    ------\n");
   FilePrint(fh, "      AddModule:  %7d    %6d    %6d\n",
             api_cnt.AddModule, api_cnt.AddModule_OK, (api_cnt.AddModule - api_cnt.AddModule_OK));
   FilePrint(fh, "AddJittedMethod:  %7d    %6d    %6d\n",
             api_cnt.AddJittedMethod, api_cnt.AddJittedMethod_OK, (api_cnt.AddJittedMethod - api_cnt.AddJittedMethod_OK));
   FilePrint(fh, "      AddSymbol:  %7d    %6d    %6d\n",
             api_cnt.AddSymbol, api_cnt.AddSymbol_OK, (api_cnt.AddSymbol - api_cnt.AddSymbol_OK));
   FilePrint(fh, "   CloneProcess:  %7d    %6d    %6d\n",
             api_cnt.CloneProcess, api_cnt.CloneProcess_OK, (api_cnt.CloneProcess - api_cnt.CloneProcess_OK));
   FilePrint(fh, "    ForkProcess:  %7d    %6d    %6d\n",
             api_cnt.ForkProcess, api_cnt.ForkProcess_OK, (api_cnt.ForkProcess - api_cnt.ForkProcess_OK));
   FilePrint(fh, "  CreateProcess:  %7d    %6d    %6d\n",
             api_cnt.CreateProcess, api_cnt.CreateProcess_OK, (api_cnt.CreateProcess - api_cnt.CreateProcess_OK));
   FilePrint(fh, " SetProcessName:  %7d    %6d    %6d\n",
             api_cnt.SetProcessName, api_cnt.SetProcessName_OK, (api_cnt.SetProcessName - api_cnt.SetProcessName_OK));

   FilePrint(fh, "\n");
   err_other = (api_cnt.GetSymbol - api_cnt.GetSymbol_OKSymbol - api_cnt.GetSymbol_OKJittedMethod -
                api_cnt.GetSymbol_NoModule - api_cnt.GetSymbol_NoSymbols -
                api_cnt.GetSymbol_NoSymbolsValFailed - api_cnt.GetSymbol_NoSymbolsQuality -
                api_cnt.GetSymbol_NoSymbolFound);
   total_fast = api_cnt.GetSymbol_FastpathOk + api_cnt.GetSymbol_FastpathMod + api_cnt.GetSymbol_FastpathNone;

   FilePrint(fh, "                  ******* Successful ******    ******************* Error *******************     *********** FastPath *********\n");
   FilePrint(fh, "                   Total     Symbol  JitMth    NoMod   NoSyms  NoSymV  NoSymQ  NoSymF  Other     Total   FstOk   FstMod  FstMis\n");
   FilePrint(fh, "                  -------    ------  ------    ------  ------  ------  ------  ------  ------    ------  ------  ------  ------\n");
   FilePrint(fh, "      GetSymbol:  %7d    %6d  %6d    %6d  %6d  %6d  %6d  %6d  %6d    %6d  %6d  %6d  %6d\n",
             api_cnt.GetSymbol, api_cnt.GetSymbol_OKSymbol, api_cnt.GetSymbol_OKJittedMethod,
             api_cnt.GetSymbol_NoModule, api_cnt.GetSymbol_NoSymbols, api_cnt.GetSymbol_NoSymbolsValFailed,
             api_cnt.GetSymbol_NoSymbolsQuality, api_cnt.GetSymbol_NoSymbolFound, err_other,
             total_fast, api_cnt.GetSymbol_FastpathOk, api_cnt.GetSymbol_FastpathMod, api_cnt.GetSymbol_FastpathNone);


   // Write to a2n.err only
   if (!dump_to_a2n_mod_proc) {
      // Traverse module cache again and dump summary info for each requested module ...
      FilePrint(fh, "\n\n############################################################################\n");
      FilePrint(fh, "###                       Requested modules summary                      ###\n");
      FilePrint(fh, "############################################################################\n");
      FilePrint(fh, "syms  secs    ts     chksum  ty rqcnt  R H  name\n");
      FilePrint(fh, "----- ---- -------- -------- -- ------ - -- --------------------------------------\n");
      mn = G(ModRoot);
      while (mn) {
         if (mn->flags & A2N_FLAGS_REQUESTED) {
             ct = GetModuleTypeShortString(mn->type);
             cr = GetRequestedString(mn->flags);
             ch = GetHarvestedString(mn);
             FilePrint(fh, "%5d %4d %08X %08X %s %6d %s %s ",
                       mn->symcnt, mn->seccnt, mn->ts, mn->chksum, ct, mn->req_cnt, cr, ch);

             if (mn->flags & A2N_FLAGS_VALIDATION_FAILED)
                FilePrint(fh, "*VALIDATION_FAILED*  %s\n", mn->name);
             else if (mn->flags & A2N_FLAGS_VALIDATION_NOT_DONE)
                FilePrint(fh, "*VALIDATION_NOT_DONE*  %s\n", mn->name);
             else
                FilePrint(fh, "%s\n", mn->name);
         }

         mn = mn->next;
      } // while

      if (api_cnt.GetSymbol_OKJittedMethod != 0) {
         FilePrint(fh, "    -    - -------- -------- JM %6d R H  JITTED_CODE\n", api_cnt.GetSymbol_OKJittedMethod);
      }
   }

   fflush(fh);
   return;
}


//
// DumpDecendants()
// ****************
//
static void DumpDescendants(FILE * fh, PID_NODE * pn, int lvl)
{
   PID_NODE * tpn;

   // Write out this node's pid (with the correct decoration)
   if (pn->cloner != NULL)
      FilePrint(fh, "%*s %04x (%s)\n", lvl-1, "-", pn->pid, pn->name);  // clone
   else if (pn->forker != NULL)
      FilePrint(fh, "%*s %04x (%s)\n", lvl-1, "*", pn->pid, pn->name);  // fork
   else
      FilePrint(fh, "%04x (%s)\n", pn->pid, pn->name);                  // initial

   // Dump this node's forked pids tree
   tpn = pn->forked;
   while (tpn) {
      DumpDescendants(fh, tpn, lvl+2);
      tpn = tpn->fnext;
   }

   // Dump this node's cloned pids tree
   tpn = pn->cloned;
   while (tpn) {
      DumpDescendants(fh, tpn, lvl+2);
      tpn = tpn->cnext;
   }
   return;
}


//
// A2nDumpProcesses()
// ******************
//
// Force dump of the process data structures to disk for diagnostics purpose.
// Data dumped to file "a2n.proc" if filename is NULL. This is an ascii dump.
//
void A2nDumpProcesses(char * filename)
{
   PID_NODE * pn, * tpn;
   LMOD_NODE * lmn;
   JM_NODE * jmn;
   MOD_NODE * mn;

   int i, kernel_in_lm_list;
   char tstr[21];
   char * ct, * ci, * cr, * ch;

   FILE * fh = NULL;


   dbgapi(("\n>> A2nDumpProcesses: fn='%s'\n", filename));
   if (filename == NULL  ||  *filename == '\0') {
      errmsg(("*E* A2nDumpProcesses: invalid filename - NULL\n"));
      return;
   }

   remove(filename);

   fh = fopen(filename, "w");
   if (fh == NULL) {
      errmsg(("*E* A2nDumpProcesses: unable to open '%s'. Terminating.\n", filename));
      goto TheEnd;
   }

   DumpGlobals(fh);                         // Dump global stuff

   //
   // Write process hash table ...
   //
   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###                         Process Hash Table                           ###\n");
   FilePrint(fh, "############################################################################\n");
   FilePrint(fh, "elm    1    2    3    4    5    6    7    8    9   10   11   12   13   14\n");
   FilePrint(fh, "---  ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----\n");
   for (i = 0; i < PID_HASH_TABLE_SIZE; i++) {
      pn = G(PidTable[i]);
      if (pn == NULL)
         FilePrint(fh, "%3d\n", i);
      else {
         FilePrint(fh, "%3d  ", i);
         while (pn) {
            FilePrint(fh, "%04x ", pn->pid);
            pn = pn->hash_next;
         }
         FilePrint(fh, "\n");
      }
   } // for i

   //
   // Write process summary report ...
   //
   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###                       Process Summary Report                         ###\n");
   FilePrint(fh, "############################################################################\n");
   FilePrint(fh, "pid  name                 lm    jm    type/parent\n");
   FilePrint(fh, "---- -------------------- ----- ----- -------------------\n");
   pn = G(PidRoot);
   while (pn) {
      if (pn->name == NULL) {
         strcpy(tstr, "NO_NAME             ");
      }
      else if (strlen(pn->name) > 20) {
         memcpy(tstr, pn->name, 20);
         tstr[20] = 0;
      }
      else {
         memset(tstr, 0x20, 20);
         memcpy(tstr, pn->name, strlen(pn->name));
         tstr[20] = 0;
      }

      if (pn->rootcloner != NULL) {
         FilePrint(fh, "%04x %s %5d %5d CLONE (ppid: %04x)",
                   pn->pid, tstr, pn->lmcnt, pn->jmcnt, pn->cloner->pid);
      }
      else if (pn->forker != NULL) {
         FilePrint(fh, "%04x %s %5d %5d FORK  (ppid: %04x)",
                   pn->pid, tstr, pn->lmcnt, pn->jmcnt, pn->forker->pid);
      }
      else {
         FilePrint(fh, "%04x %s %5d %5d INITIAL           ",
                   pn->pid, tstr, pn->lmcnt, pn->jmcnt);
      }

      if (pn->cloned != NULL) {
         FilePrint(fh, " -C(%d):", pn->cloned_cnt);
         tpn = pn->cloned;
         while (tpn) {
            FilePrint(fh, " %04x", tpn->pid);
            tpn = tpn->cnext;
         }
         FilePrint(fh, "  ");
      }

      if (pn->forked != NULL) {
         FilePrint(fh, " *F(%d):", pn->forked_cnt);
         tpn = pn->forked;
         while (tpn) {
            FilePrint(fh, " %04x", tpn->pid);
            tpn = tpn->fnext;
         }
      }

      pn = pn->next;
      FilePrint(fh, "\n");
   } // while

   //
   // Write process heritage report ...
   //
   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###                       Process Heritage Report                        ###\n");
   FilePrint(fh, "############################################################################\n");
   pn = G(PidRoot);
   while (pn) {
      if (pn->cloner == NULL  &&  pn->forker == NULL) {
         DumpDescendants(fh, pn, 0);        // Dump descendants of INITIAL pids only
      }
      pn = pn->next;
   } // while
   FilePrint(fh, "\n");


   //
   // Traverse the pid node list and dump info for each process ...
   //
   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###                       Process Details Report                         ###\n");
   FilePrint(fh, "############################################################################\n");
   pn = G(PidRoot);
   while (pn) {
      kernel_in_lm_list = 0;
      FilePrint(fh, "\n\nPid: 0x%04x  lm: %d (m: %d jm: %d)  im: %d (m: %d jm: %d)  name: %s\n",
                pn->pid, pn->lmcnt, (pn->lmcnt - pn->jmcnt), pn->jmcnt,
                pn->imcnt, (pn->imcnt - pn->ijmcnt), pn->ijmcnt, pn->name);

      if (pn->flags & A2N_FLAGS_32BIT_PROCESS)
         FilePrint(fh, "   *32-bit Process*\n");
      else if (pn->flags & A2N_FLAGS_64BIT_PROCESS)
         FilePrint(fh, "   *64-bit Process*\n");

      if (pn->rootcloner != NULL)
         FilePrint(fh, "   *CLONED*  Parent: 0x%04x   RootCloner: 0x%04x\n", pn->cloner->pid, pn->rootcloner->pid);
      else if (pn->forker != NULL)
         FilePrint(fh, "   *FORKED*  Parent: 0x%04x\n", pn->forker->pid);
      else
         FilePrint(fh, "   *INITIAL* Not forked and not cloned\n");

      if (pn->flags & A2N_FLAGS_PID_REUSED)
         FilePrint(fh, "   *RE-USED* at least once. This is most recent instance.\n");

      if (pn->cloned != NULL) {
         FilePrint(fh, "   Have cloned (%d): ", pn->cloned_cnt);
         tpn = pn->cloned;
         while (tpn) {
            FilePrint(fh, "0x%04x ", tpn->pid);
            tpn = tpn->cnext;
         }
         FilePrint(fh, "\n");
      }

      if (pn->forked != NULL) {
         FilePrint(fh, "   Have forked (%d): ", pn->forked_cnt);
         tpn = pn->forked;
         while (tpn) {
            FilePrint(fh, "0x%04x ", tpn->pid);
            tpn = tpn->fnext;
         }
         FilePrint(fh, "\n");
      }

      if (pn->lmreq_cnt > 0) {
         FilePrint(fh, "   lm_req: %d  lm_req_notfound: %d  (%.2f%%)",
                   pn->lmreq_cnt, pn->lmerr_cnt,
                   ((double)pn->lmerr_cnt / (double)pn->lmreq_cnt)*100.0);
         if (pn->rootcloner != NULL)
            FilePrint(fh, "  (*** Totals are included in owning pid: 0x%04x ***)\n", pn->rootcloner->pid);
         else
            FilePrint(fh, "\n");
      }

      if (pn->lmcnt != 0) {
         FilePrint(fh, "\n");
         FilePrint(fh, "    arv        start             end         length  ty sec/syms H  R rqcnt  I-ipid name\n");
         FilePrint(fh, "   ------ ---------------- ---------------- -------- -- -- ----- -- - ------ ------ ----------------------------------\n");

         lmn = pn->lmroot;
         i = pn->lmcnt;
         while (lmn) {
            ci = GetInheritedString(lmn->flags);
            cr = GetRequestedString(lmn->flags);

            if (lmn->flags & A2N_FLAGS_KERNEL)
               kernel_in_lm_list = 1;

            if (lmn->mn != NULL) {
               // Normal module
               mn = lmn->mn;
               ct = GetModuleTypeShortString(mn->type);
               ch = GetHarvestedString(mn);
               if (lmn->flags & A2N_FLAGS_INHERITED) {
                  FilePrint(fh, "   %6d %"_LZ64X" %"_LZ64X" %08X %s %2d %5d %s %s %6d %s-%04X %s\n",
                            i, lmn->start_addr, lmn->start_addr + lmn->total_len - 1,
                            lmn->total_len, ct, mn->seccnt, mn->symcnt, ch, cr, lmn->req_cnt, ci, lmn->pid, mn->name);
               }
               else {
                  FilePrint(fh, "   %6d %"_LZ64X" %"_LZ64X" %08X %s %2d %5d %s %s %6d        %s\n",
                            i, lmn->start_addr, lmn->start_addr + lmn->total_len - 1,
                            lmn->total_len, ct, mn->seccnt, mn->symcnt, ch, cr, lmn->req_cnt, mn->name);
               }
               if (lmn->seg_cnt > 1) {
                  int j;
                  FilePrint(fh, "          %"_LZ64X" %"_LZ64X" %08X S1\n",
                            lmn->start_addr, lmn->start_addr + lmn->length - 1, lmn->length);
                  for (j = 0; j <= lmn->seg_cnt - 2; j++) {
                     FilePrint(fh, "          %"_LZ64X" %"_LZ64X" %08X S%d\n",
                               lmn->seg_start_addr[j], lmn->seg_start_addr[j] + lmn->seg_length[j] - 1, lmn->seg_length[j], j+2);
                  }
               }
            }
            else {
               // Jitted method
               jmn = lmn->jmn;
               if (lmn->flags & A2N_FLAGS_INHERITED) {
                  FilePrint(fh, "   %6d %"_LZ64X" %"_LZ64X" %08X JM %2d %5d H  %s %6d %s-%04X %s\n",
                            i, lmn->start_addr, lmn->start_addr + jmn->length - 1,
                            jmn->length, 1, 1, cr, lmn->req_cnt, ci, lmn->pid, jmn->name);
               }
               else {
                  FilePrint(fh, "   %6d %"_LZ64X" %"_LZ64X" %08X JM %2d %5d H  %s %6d        %s\n",
                            i, lmn->start_addr, lmn->start_addr + jmn->length - 1,
                            jmn->length, 1, 1, cr, lmn->req_cnt, jmn->name);
               }
            }

            lmn = lmn->next;
            i--;
         }

         if (pn != G(SystemPidNode)  &&  pn->kernel_req != 0  &&  kernel_in_lm_list == 0)
            FilePrint(fh, "          ---------------- ---------------- --------                R %6d        vmlinux\n", pn->kernel_req);
      }
      else {
         if (pn != G(SystemPidNode)  &&  pn->kernel_req != 0)
            FilePrint(fh, "   GetSymbol requests resolved to vmlinux: %d\n", pn->kernel_req);
      }

      pn = pn->next;
   } // while


#if defined(_WINDOWS)
   //
   // Traverse the pid node list and dump info for each process whose PID was reused
   //
   if (G(ReusedPidCnt) == 0)
      goto TheEnd;

   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###                       Re-used Process Report                         ###\n");
   FilePrint(fh, "############################################################################\n");
   pn = G(ReusedPidNode);
   while (pn) {
      kernel_in_lm_list = 0;
      FilePrint(fh, "\n\nPid: 0x%04x  lm: %d (m: %d jm: %d)  im: %d (m: %d jm: %d)  name: %s\n",
                pn->pid, pn->lmcnt, (pn->lmcnt - pn->jmcnt), pn->jmcnt,
                pn->imcnt, (pn->imcnt - pn->ijmcnt), pn->ijmcnt, pn->name);

      FilePrint(fh, "   *INITIAL* Not forked and not cloned\n");

      if (pn->lmreq_cnt > 0) {
         FilePrint(fh, "   lm_req: %d  lm_req_notfound: %d  (%.2f%%)",
                   pn->lmreq_cnt, pn->lmerr_cnt,
                   ((double)pn->lmerr_cnt / (double)pn->lmreq_cnt)*100.0);
      }

      if (pn->lmcnt != 0) {
         FilePrint(fh, "\n");
         FilePrint(fh, "    arv        start             end         length  ty sec/syms H  R rqcnt  I-ipid name\n");
         FilePrint(fh, "   ------ ---------------- ---------------- -------- -- -- ----- -- - ------ ------ ----------------------------------\n");

         lmn = pn->lmroot;
         i = pn->lmcnt;
         while (lmn) {
            ci = GetInheritedString(lmn->flags);
            cr = GetRequestedString(lmn->flags);

            if (lmn->flags & A2N_FLAGS_KERNEL)
               kernel_in_lm_list = 1;

            if (lmn->mn != NULL) {
               // Normal module
               mn = lmn->mn;
               ct = GetModuleTypeShortString(mn->type);
               ch = GetHarvestedString(mn);
               if (lmn->flags & A2N_FLAGS_INHERITED) {
                  FilePrint(fh, "   %6d %"_LZ64X" %"_LZ64X" %08X %s %2d %5d %s %s %6d %s-%04X %s\n",
                            i, lmn->start_addr, lmn->start_addr + lmn->total_len - 1,
                            lmn->total_len, ct, mn->seccnt, mn->symcnt, ch, cr, lmn->req_cnt, ci, lmn->pid, mn->name);
               }
               else {
                  FilePrint(fh, "   %6d %"_LZ64X" %"_LZ64X" %08X %s %2d %5d %s %s %6d        %s\n",
                            i, lmn->start_addr, lmn->start_addr + lmn->total_len - 1,
                            lmn->total_len, ct, mn->seccnt, mn->symcnt, ch, cr, lmn->req_cnt, mn->name);
               }
               if (lmn->seg_cnt > 1) {
                  int j;
                  FilePrint(fh, "          %"_LZ64X" %"_LZ64X" %08X S1\n",
                            lmn->start_addr, lmn->start_addr + lmn->length - 1, lmn->length);
                  for (j = 0; j <= lmn->seg_cnt - 2; j++) {
                     FilePrint(fh, "          %"_LZ64X" %"_LZ64X" %08X S%d\n",
                               lmn->seg_start_addr[j], lmn->seg_start_addr[j] + lmn->seg_length[j] - 1, lmn->seg_length[j], j+2);
                  }
               }
            }
            else {
               // Jitted method
               jmn = lmn->jmn;
               if (lmn->flags & A2N_FLAGS_INHERITED) {
                  FilePrint(fh, "   %6d %"_LZ64X" %"_LZ64X" %08X JM %2d %5d H  %s %6d %s-%04X %s\n",
                            i, lmn->start_addr, lmn->start_addr + jmn->length - 1,
                            jmn->length, 1, 1, cr, lmn->req_cnt, ci, lmn->pid, jmn->name);
               }
               else {
                  FilePrint(fh, "   %6d %"_LZ64X" %"_LZ64X" %08X JM %2d %5d H  %s %6d        %s\n",
                            i, lmn->start_addr, lmn->start_addr + jmn->length - 1,
                            jmn->length, 1, 1, cr, lmn->req_cnt, jmn->name);
               }
            }

            lmn = lmn->next;
            i--;
         }
      }
      pn = pn->next;
   } // while
#endif

   // Done
TheEnd:
   dbgmsg(("<< A2nDumpProcesses: OK\n"));
   if (fh != NULL) {
      fflush(fh);
      fclose(fh);
   }
   return;
}


//
// DumpSymbol()
// ************
//
// Dumps symbol tree (aliases/contained) for a symbol
//
static void DumpSymbol(SYM_NODE * sn, FILE * fh, int lvl)
{
   SYM_NODE * tsn;

   char * ct = GetSymbolTypeString(sn->type);

   //
   // Write out this symbol node (with the correct decoration)
   // " " base symbol
   // "A:" aliased symbol
   // "C:" contained (sub) symbol
   //
   if (sn->flags & A2N_FLAGS_SYMBOL_ALIASED) {
      FilePrint(fh, "      %08X %08X %08X %"_PZP" %7X %s %08X %6d %*s %s\n",
                sn->offset_start, sn->offset_end, sn->length, sn->code, sn->section,
                ct, (sn->flags & ~NODE_SIGNATURE_MASK), sn->req_cnt, lvl, "A:", sn->name);
   }
   else if (sn->flags & A2N_FLAGS_SYMBOL_CONTAINED) {
      FilePrint(fh, "      %08X %08X %08X %"_PZP" %7X %s %08X %6d %*s %s\n",
                sn->offset_start, sn->offset_end, sn->length, sn->code, sn->section,
                ct, (sn->flags & ~NODE_SIGNATURE_MASK), sn->req_cnt, lvl, "C:", sn->name);
   }
   else {
      if (sn->flags & A2N_FLAGS_SYMBOL_COLLAPSED)
         FilePrint(fh, "      %08X %08X %08X %"_PZP" %7X %s %08X %6d >> %s (%08X/%08X/%08X)\n",
                   sn->rn->offset_start, sn->rn->offset_end, sn->rn->length,
                   sn->rn->code, sn->rn->section, ct, (sn->flags & ~NODE_SIGNATURE_MASK),
                   sn->rn->req_cnt, sn->name, sn->offset_start,
                   sn->offset_end, sn->length);
      else
         FilePrint(fh, "      %08X %08X %08X %"_PZP" %7X %s %08X %6d %s\n",
                   sn->offset_start, sn->offset_end, sn->length, sn->code, sn->section,
                   ct, (sn->flags & ~NODE_SIGNATURE_MASK), sn->req_cnt, sn->name);
   }

   // Dump this symbol's aliases
   tsn = sn->aliases;
   while (tsn) {
      DumpSymbol(tsn, fh, lvl+3);
      tsn = tsn->next;
   }

   // Dump this symbol's contained
   tsn = sn->contained;
   while (tsn) {
      DumpSymbol(tsn, fh, lvl+3);
      tsn = tsn->next;
   }

   return;
}


//
// A2nDumpModules()
// ****************
//
// Force dump of the module data structures to disk for diagnostics purpose.
// Data dumped to file "a2n.mod" if filename is NULL. This is an ascii dump.
//
void A2nDumpModules(char * filename)
{
   PID_NODE * pn;
   MOD_NODE * mn;
   LMOD_NODE * lmn;
   SYM_NODE * sn;
   SEC_NODE * cn;
   JM_NODE  * jmn;

   FILE * fh = NULL;
   char * ct, * cr, * ch;
   int first = 1;
   int i;


   dbgapi(("\n>> A2nDumpModules: fn='%s'\n", filename));
   if (filename == NULL  ||  *filename == '\0') {
      errmsg(("*E* A2nDumpModules: invalid filename - NULL\n"));
      return;
   }

   remove(filename);

   fh = fopen(filename, "w");
   if (fh == NULL) {
      errmsg(("*E* A2nDumpModules: unable to open '%s'. Terminating.\n", filename));
      goto TheEnd;
   }

   DumpGlobals(fh);                         // Dump global stuff

   //
   // Traverse module cache again and dump summary info for each real module ...
   //
   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###                        Module Summary Report                         ###\n");
   FilePrint(fh, "############################################################################\n");
   FilePrint(fh, "syms  secs    ts     chksum  ty rqcnt  R H  name\n");
   FilePrint(fh, "----- ---- -------- -------- -- ------ - -- --------------------------------------\n");
   mn = G(ModRoot);
   while (mn) {
      ct = GetModuleTypeShortString(mn->type);
      cr = GetRequestedString(mn->flags);
      ch = GetHarvestedString(mn);

      FilePrint(fh, "%5d %4d %08X %08X %s %6d %s %s ",
                mn->symcnt, mn->seccnt, mn->ts, mn->chksum, ct, mn->req_cnt, cr, ch);

      if (mn->flags & A2N_FLAGS_VALIDATION_FAILED)
         FilePrint(fh, "*VALIDATION_FAILED*  %s\n", mn->name);
      else if (mn->flags & A2N_FLAGS_VALIDATION_NOT_DONE)
         FilePrint(fh, "*VALIDATION_NOT_DONE*  %s\n", mn->name);
      else
         FilePrint(fh, "%s\n", mn->name);

      mn = mn->next;
   } // while

   if (api_cnt.GetSymbol_OKJittedMethod != 0) {
      FilePrint(fh, "    -    - -------- -------- JM %6d R H  JITTED_CODE\n", api_cnt.GetSymbol_OKJittedMethod);
   }

   //
   // Traverse module cache again and dump summary info for each requested module ...
   //
   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###                        Requested Modules                             ###\n");
   FilePrint(fh, "############################################################################\n");
   FilePrint(fh, "syms  secs    ts     chksum  ty rqcnt  R H  name\n");
   FilePrint(fh, "----- ---- -------- -------- -- ------ - -- --------------------------------------\n");
   mn = G(ModRoot);
   while (mn) {
      if (mn->flags & A2N_FLAGS_REQUESTED) {
          ct = GetModuleTypeShortString(mn->type);
          cr = GetRequestedString(mn->flags);
          ch = GetHarvestedString(mn);
          FilePrint(fh, "%5d %4d %08X %08X %s %6d %s %s ",
                    mn->symcnt, mn->seccnt, mn->ts, mn->chksum, ct, mn->req_cnt, cr, ch);

          if (mn->flags & A2N_FLAGS_VALIDATION_FAILED)
             FilePrint(fh, "*VALIDATION_FAILED*  %s\n", mn->name);
          else if (mn->flags & A2N_FLAGS_VALIDATION_NOT_DONE)
             FilePrint(fh, "*VALIDATION_NOT_DONE*  %s\n", mn->name);
          else
             FilePrint(fh, "%s\n", mn->name);
      }

      mn = mn->next;
   } // while

   if (api_cnt.GetSymbol_OKJittedMethod != 0) {
      FilePrint(fh, "    -    - -------- -------- JM %6d R H  JITTED_CODE\n", api_cnt.GetSymbol_OKJittedMethod);
   }

   //
   // Traverse module cache once and dump info for each real module ...
   //
   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###                        Module Details Report                         ###\n");
   FilePrint(fh, "############################################################################\n");
   mn = G(ModRoot);
   while (mn) {
      FilePrint(fh, "\n\nModule: %s\n", mn->name);

      ct = GetModuleTypeLongString(mn->type);
      if (mn->flags & A2N_FLAGS_64BIT_MODULE)
         FilePrint(fh, "    type: 64-bit %s  ts: 0x%08x  chksum: 0x%08x\n", ct, mn->ts, mn->chksum);
      else if (mn->flags & A2N_FLAGS_32BIT_MODULE)
         FilePrint(fh, "    type: 32-bit %s  ts: 0x%08x  chksum: 0x%08x\n", ct, mn->ts, mn->chksum);
      else
         FilePrint(fh, "    type: %s  ts: 0x%08x  chksum: 0x%08x\n", ct, mn->ts, mn->chksum);

      FilePrint(fh, "    sections: %d  ne_sections: %d  codesize: %d  strsize: %d\n",
                mn->seccnt, mn->ne_seccnt, mn->codesize, mn->strsize);
#if defined(_WINDOWS)
      FilePrint(fh, "    symbols: %d  base_symbols: %d  src_lines: %d\n",
                mn->symcnt, mn->base_symcnt, mn->li.linecnt);
#else
      FilePrint(fh, "    symbols: %d  base_symbols: %d\n",
                mn->symcnt, mn->base_symcnt);
#endif

      if (mn->req_cnt > 0) {
         FilePrint(fh, "    req_cnt: %d  symreq_cached: %d  (%.2f%%)\n",
                   mn->req_cnt, mn->symcachehits, ((double)mn->symcachehits / (double)mn->req_cnt)*100.0);
      }

      if ((mn->flags & ~NODE_SIGNATURE_MASK) != 0) {
         FilePrint(fh, "    mnflags = 0x%08x : ", (mn->flags & ~NODE_SIGNATURE_MASK));
         if (mn->flags & A2N_FLAGS_KERNEL)                  FilePrint(fh, "KERNEL  ");
         if (mn->flags & A2N_FLAGS_REQUESTED)               FilePrint(fh, "REQUESTED  ");
         if (mn->flags & A2N_FLAGS_SYMBOLS_HARVESTED)       FilePrint(fh, "SYMBOLS_HARVESTED  ");
         if (mn->flags & A2N_SYMBOL_ARRAY_BUILT)            FilePrint(fh, "SYMBOL_ARRAY_BUILT  ");
         if (mn->flags & A2N_FLAGS_VALIDATION_FAILED)       FilePrint(fh, "*VALIDATION_FAILED*  ");
         if (mn->flags & A2N_FLAGS_VALIDATION_NOT_DONE)     FilePrint(fh, "*VALIDATION_NOT_DONE*  ");
         if (mn->flags & A2N_FLAGS_SYMBOLS_NOT_FROM_IMAGE)  FilePrint(fh, "SYMBOLS_NOT_FROM_IMAGE ");
         if (mn->flags & A2N_FLAGS_COFF_SYMBOLS)            FilePrint(fh, "COFF_SYMBOLS ");
         if (mn->flags & A2N_FLAGS_CV_SYMBOLS)              FilePrint(fh, "CODEVIEW_SYMBOLS ");
         if (mn->flags & A2N_FLAGS_EXPORT_SYMBOLS)          FilePrint(fh, "EXPORT_SYMBOLS ");
         if (mn->flags & A2N_FLAGS_ILT_SYMBOLS_ONLY)        FilePrint(fh, "ILT_SYMBOLS_ONLY ");
         if (mn->flags & A2N_FLAGS_PLT_SYMBOLS_ONLY)        FilePrint(fh, "PLT_SYMBOLS_ONLY ");
         FilePrint(fh, "\n");
      }

      if (mn->orgname != NULL) {
         if (strcmp(mn->name, mn->orgname) != 0)
            FilePrint(fh, "    Name given in A2nAddModule: %s\n", mn->orgname);
      }
      if (mn->hvname != NULL) {
         if (strcmp(mn->name, mn->hvname) != 0)
            FilePrint(fh, "    Symbols harvested from:     %s\n", mn->hvname);
      }

      if (mn->seccnt != 0) {
         if (mn->secroot) {
            FilePrint(fh, "\n    ******** EXECUTABLE_SECTIONS:\n");
#if defined(_32BIT)
            FilePrint(fh, "      num.   offset        start@             end@         length    flags      code    name\n");
            FilePrint(fh, "      ----  --------  ----------------  ----------------  --------  --------  --------  ----------------\n");
#else
            FilePrint(fh, "      num.   offset        start@             end@         length    flags         code         name\n");
            FilePrint(fh, "      ----  --------  ----------------  ----------------  --------  --------  ----------------  ----------------\n");
#endif
            cn = mn->secroot;
            while (cn != NULL) {
               FilePrint(fh, "      %04X  %08X  %"_LZ64X"  %"_LZ64X"  %08X  %08X  %"_PZP"  %s\n",
                         cn->number, cn->offset_start, cn->start_addr, cn->end_addr, cn->size, cn->sec_flags, cn->code, cn->name);
               cn = cn->next;
            }
         }
         if (mn->ne_secroot) {
            FilePrint(fh, "\n    ******** NON-EXECUTABLE_SECTIONS:\n");
#if defined(_32BIT)
            FilePrint(fh, "      num.   offset        start@             end@         length    flags      code    name\n");
            FilePrint(fh, "      ----  --------  ----------------  ----------------  --------  --------  --------  ----------------\n");
#else
            FilePrint(fh, "      num.   offset        start@             end@         length    flags         code         name\n");
            FilePrint(fh, "      ----  --------  ----------------  ----------------  --------  --------  ----------------  ----------------\n");
#endif
            cn = mn->ne_secroot;
            while (cn != NULL) {
               FilePrint(fh, "      %04X  %08X  %"_LZ64X"  %"_LZ64X"  %08X  %08X  %"_PZP"  %s\n",
                         cn->number, cn->offset_start, cn->start_addr, cn->end_addr, cn->size, cn->sec_flags, cn->code, cn->name);
               cn = cn->next;
            }
         }
         FilePrint(fh, "    ******** END_SECTIONS ********\n");
      }

      if (mn->symcnt != 0) {
         FilePrint(fh, "\n    ******** SYMBOLS:\n");
#if defined(_32BIT)
         FilePrint(fh, "      off_strt off_end   length    code   section type  flags   rqcnt  name\n");
         FilePrint(fh, "      -------- -------- -------- -------- ------- ---- -------- ------ -------------------------\n");
#else
         FilePrint(fh, "      off_strt off_end   length        code       section type  flags   rqcnt  name\n");
         FilePrint(fh, "      -------- -------- -------- ---------------- ------- ---- -------- ------ -------------------------\n");
#endif
         if (mn->flags & A2N_SYMBOL_ARRAY_BUILT) {
            for (i = 0; i < mn->base_symcnt; i++) {
               sn = mn->symroot[i];
               DumpSymbol(sn, fh, 0);       // Dump out "base" symbol
            }
         }
         FilePrint(fh, "    ******** END_SYMBOLS ********\n");
      }

#if defined(_WINDOWS)
      if (mn->li.linecnt != 0) {
         int j;
         SRCLINE_NODE **lines = mn->li.srclines;

         FilePrint(fh, "\n    ******** SYMBOLS/SRC_LINES:\n");
         FilePrint(fh, "      off_strt off_end  sym/line src_file\n");
         FilePrint(fh, "      -------- -------- -------- --------------------------------------\n");
         for (i = 0; i < mn->base_symcnt; i++) {
            sn = mn->symroot[i];
            FilePrint(fh, "      %08X %08X %s\n", sn->offset_start, sn->offset_end, sn->name);
            if (sn->line_cnt != 0) {
               for (j = sn->line_start; j <= sn->line_end; j++) {
                  FilePrint(fh, "      %8X %8X %8d %s\n", lines[j]->offset_start,
                            lines[j]->offset_end, lines[j]->lineno, lines[j]->sfn->fn);
               }
            }
         }
         FilePrint(fh, "    ******** END_SYMBOLS/SRC_LINES ********\n");
      }
#endif

      mn = mn->next;
   } // while

   //
   // Traverse pid node chain and dump jitted methods ...
   //
   if (G(JmPoolCnt) == 0)
      goto TheEnd;                          // No jitted methods - done

   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###               Jitted Methods By Active Process Report                ###\n");
   FilePrint(fh, "############################################################################\n");
   pn = G(PidRoot);
   while (pn) {
      if (pn->lmcnt != 0) {
         lmn = pn->lmroot;
         while (lmn != NULL) {
            if (lmn->jmn != NULL) {
               if (first == 1) {
                  FilePrint(fh, "\n\nModule: JITTED_CODE_Pid-%04X_%s\n\n", pn->pid, pn->name);
#if defined(_32BIT)
                  FilePrint(fh, "            addr         length     code     flags   rqcnt  name\n");
                  FilePrint(fh, "     ------------------ -------- ---------- -------- ------ -------------------------\n");
#else
                  FilePrint(fh, "            addr         length         code         flags   rqcnt  name\n");
                  FilePrint(fh, "     ------------------ -------- ------------------ -------- ------ -------------------------\n");
#endif
                  first = 2;
               }
               jmn = lmn->jmn;
               FilePrint(fh, "     %"_LZ64X" %08X %"_PZP" %08X %6d %s\n",
                         lmn->start_addr, jmn->length, jmn->code,
                         (jmn->flags & ~NODE_SIGNATURE_MASK), jmn->req_cnt,jmn->name);
            }

            lmn = lmn->next;
         }
      }

      if (first == 2)
         first = 1;

      pn = pn->next;
   } // while


#if defined(_WINDOWS)
   //
   // Traverse re-used pid node chain and dump jitted methods ...
   //
   if (G(ReusedPidCnt) == 0)
      goto TheEnd;

   FilePrint(fh, "\n\n############################################################################\n");
   FilePrint(fh, "###              Jitted Methods By Re-used Process Report                ###\n");
   FilePrint(fh, "############################################################################\n");
   pn = G(ReusedPidNode);
   while (pn) {
      if (pn->lmcnt != 0) {
         lmn = pn->lmroot;
         while (lmn != NULL) {
            if (lmn->jmn != NULL) {
               if (first == 1) {
                  FilePrint(fh, "\n\nModule: JITTED_CODE_Pid-%04X_%s\n", pn->pid, pn->name);
#if defined(_32BIT)
                  FilePrint(fh, "            addr         length     code     flags   rqcnt  name\n");
                  FilePrint(fh, "     ------------------ -------- ---------- -------- ------ -------------------------\n");
#else
                  FilePrint(fh, "            addr         length         code         flags   rqcnt  name\n");
                  FilePrint(fh, "     ------------------ -------- ------------------ -------- ------ -------------------------\n");
#endif
                  first = 2;
               }
               jmn = lmn->jmn;
               FilePrint(fh, "     %"_LZ64X" %08X %"_PZP" %08X %6d %s\n",
                         lmn->start_addr, jmn->length, jmn->code,
                         (jmn->flags & ~NODE_SIGNATURE_MASK), jmn->req_cnt, jmn->name);
            }

            lmn = lmn->next;
         }
      }

      if (first == 2)
         first = 1;

      pn = pn->next;
   } // while
#endif

   // Done
TheEnd:
   dbgmsg(("<< A2nDumpModules: OK\n"));
   if (fh != NULL) {
      fflush(fh);
      fclose(fh);
   }
   return;
}
