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

#include "a2nhdr.h"

extern void Initialize(void);               // initterm.c


//****************************************************************************//
//****************************************************************************//
//                                                                            //
//                    Shared object/DLL instance variables                    //
//                                                                            //
//****************************************************************************//
//****************************************************************************//

//
// Globals
// *******
//
gv_t a2n_gv;                                // A2N globals
SYM_NODE ** snp;                            // SN pointer array
SYM_NODE ** dsnp;                           // SN pointer array for checking dups
API_LOCKS  api_lock;                        // API lockout flags
API_COUNTS  api_cnt;                        // API counters
int initialized = 0;                        // Have not initialized
int kernel_symbols_choice_made = 0;         // Haven't done it

// A2nGetSymbolEx global stuff
PID_NODE * ex_pn = NULL;
PID_NODE * ex_opn = NULL;
unsigned int ex_pid = 0;


#if defined(_WINDOWS)
// DbgHelp APIs. Set in initterm.c
int dbghelp_version_major = 0;
PFN_SymInitialize            pSymInitialize = NULL;          // >= 5.1: Required
PFN_SymRegisterCallback64    pSymRegisterCallback64 = NULL;  // >= 5.1: Required
PFN_SymSetSearchPath         pSymSetSearchPath = NULL;       // >= 5.1: Required
PFN_SymCleanup               pSymCleanup = NULL;             // >= 5.1: Required
PFN_SymGetOptions            pSymGetOptions = NULL;          // >= 5.1: Required
PFN_SymSetOptions            pSymSetOptions = NULL;          // >= 5.1: Required
PFN_UnDecorateSymbolName     pUnDecorateSymbolName = NULL;   // >= 5.1: Required to demangle names
PFN_SymEnumSourceLines       pSymEnumSourceLines = NULL;     // >= 6.4: Optional. Use if available
PFN_SymEnumLines             pSymEnumLines = NULL;           // >= 6.1: Optional. Use if SymEnumSourceLines() not available
PFN_SymEnumSymbols           pSymEnumSymbols = NULL;         // >= 5.1: Required
PFN_SymLoadModuleEx          pSymLoadModuleEx = NULL;        // >= 6.0: Use instead of SymLoadModule64() if available
PFN_SymLoadModule64          pSymLoadModule64 = NULL;        // >= 5.1: Required
PFN_SymUnloadModule64        pSymUnloadModule64 = NULL;      // >= 5.1: Required
PFN_SymGetModuleInfo64       pSymGetModuleInfo64 = NULL;     // >= 5.1: Required

char * SystemRoot;                          // Windows System Directory
char * SystemDrive;                         // Windows System Drive
char * WinSysDir = NULL;                    // Windows System Directory (without drive letter)
char * OS_System32;                         // %SystemRoot%\System32
char * OS_SysWow64;                         // %SystemRoot%\SysWOW64
char * OS_WinSxS;                           // %SystemRoot%\WinSxS
int  dev_type[MAX_DEVICES];                 // Device type
char rdev_name[MAX_DEVICES][128];           // Valid sym links for DOS devices (as is)
char dev_name[MAX_DEVICES][128];            // Valid sym links for DOS devices (fixed up)
char * drv_name[MAX_DEVICES] = {"A:", "B:", "C:", "D:", "E:", "F:", "G:",
                                "H:", "I:", "J:", "K:", "L:", "M:", "N:",
                                "O:", "P:", "Q:", "R:", "S:", "T:", "U:",
                                "V:", "W:", "X:", "Y:", "Z:"};
#endif


#if defined(_ZOS)
CB *    pCB;                                // ptr to binder Control Block
#endif


#if defined(DEBUG)
// tags for A2nGetSymbol() exit messages so we can grep them out
char * gs_res_ok[4] = {
         "**GS-EXIT-OK**",        // (0) A2N_SUCCESS
         "**GS-EXIT-NSF**",       // (1) A2N_NO_SYMBOL_FOUND
         "**GS-EXIT-NOSYM**",     // (2) A2N_NO_SYMBOLS
         "**GS-EXIT-NOMOD**"      // (3) A2N_NO_MODULE
};

char * gs_res_fastok = "**GS-EXIT-FAST-OK**";   // successful fast lookup
char * gs_res_error  = "**GS-EXIT-ERROR**";     // some error
char * gs_res_other  = "**GS-EXIT-OTHER**";     // something else
#endif



//
// Defaults
// ********
//
char * JittedCodeModuleName = A2N_JITTED_CODE_STR;  // Pseudo module for jitted code
char * JittedMmiCodeModuleName = A2N_MMI_CODE_STR;  // Pseudo module for jitted MMI code
char * ModuleNotFound = A2N_NO_MODULE_STR;          // No module loaded contains address
char * NoSymbolData = A2N_NO_SYMBOLS_STR;           // Module loaded but no symbols harvested.
char * SymbolNotFound = A2N_NO_SYMBOL_FOUND_STR;    // Module loaded, symbols harvested, ...
char * SymbolPlt = A2N_SYMBOL_PLT_STR;              // Module loaded, symbol is <plt>
#if defined(_LINUX)
char * ModuleVdso = VDSO_SEGMENT_NAME;              // VDSO segment
#endif
#if defined(_WINDOWS) && defined(_X86)
char * SharedUserDataName = SHARED_USER_DATA_SYMBOL;
char * SystemCallStubName = SYSTEMCALL_STUB_SYMBOL;
char * SharedUserDataUnknownSymbol = SHARED_USER_DATA_NO_SYMBOL;
#endif



//****************************************************************************//
//****************************************************************************//
//                                                                            //
//                           The code starts here ...                         //
//                                                                            //
//****************************************************************************//
//****************************************************************************//

//
// A2nInit()
// *********
//
// Initialize A2N.
// Only required on platforms where the shared object's initialization
// entry point is not automatically invoked when the shared object is
// loaded. Doesn't hurt if invoked on any platform.
//
void A2nInit(void)
{
   if (!initialized) {
      Initialize();
   } else {
      dbgapi(("\n>< A2nInitialize\n"));
   }
   return;
}


//
// A2nGeVersion()
// **************
//
// Returns A2N version information.
// Version information is returned as an unsigned 32-bit quantity interpreted
// as follows:
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
unsigned int A2nGetVersion(void)
{
   dbgapi(("\n>< A2nGetVersion: 0x%08x\n", VERSION_NUMBER));
   return ((uint)VERSION_NUMBER);
}


//
// A2nAddModule()
// **************
//
// Associate module (MTE/Executable) information with a process and
// automatically adds a process block for the process on the first module
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
int A2nAddModule(unsigned int pid,          // Process id
                 uint64 base_addr,          // Module base address
                 unsigned int length,       // Module length (in bytes)
                 char * name,               // Fully qualified module name
                 unsigned int ts,           // Module timestamp (0 if not applicable)
                 unsigned int chksum,       // Module check sum (0 if not applicable)
                 void ** not_used)          // Here for backward compatibility
{
   PID_NODE * pn;
   LMOD_NODE * lmn;
   char * n;
   char made_up_name[64];
   uint64 end_addr;

   api_cnt.AddModule++;
   dbgapi(("\n>> A2nAddModule: pid=0x%04x, addr=%"_LZ64X", len=0x%08x, ts=0x%08x, cs=0x%08x, nm='%s'\n",
           pid, base_addr, length, ts, chksum, name));

   // Do a little bit of checking
#if !defined(_AIX)
   if (length == 0 && strcmp(name,G(KernelFilename)) !=0 ) {
      errmsg(("*E* A2nAddModule: module loaded at %"_LZ64X" for pid 0x%04x has length = 0.\n", base_addr, pid));
      return (A2N_INVALID_MODULE_LENGTH);
   }
#endif
   if (name == NULL) {
      errmsg(("*E* A2nAddModule: module loaded at %"_LZ64X" for pid 0x%04x has NULL name pointer.\n", base_addr, pid));
      return (A2N_NULL_MODULE_NAME);
   }

   //
   // Find process node.  If there isn't one then create a new one.
   // This process will be a "root" process - it's not a descendant of any
   // other process and does not inherit modules from any other process.
   // So far the instrumentation returns the process name (executable name)
   // in the first MTE hook, so we use that name as the default name
   // for the process.
   //
   pn = GetProcessNode(pid);
   if (pn == NULL) {                        // First add.  Need PID node
      pn = CreateProcessNode(pid);
      if (pn == NULL) {                     // Unable to add PID node
         errmsg(("*E* A2nAddModule: unable to create process node for 0x%04x-%s\n", pid, name));
         return (A2N_CREATEPROCESS_ERROR);
      }
   }

   // If the name is blank make one up
   if ( (name[0] == ' ' && name[1] == '\0') || (name[0] == '\0') ) {
      name = made_up_name;
      end_addr = base_addr + length - 1;
      sprintf(made_up_name, "no_name_%"_LZ64X"_%"_LZ64X, base_addr, end_addr);
      warnmsg(("*I* A2nAddModule: module loaded at %"_LZ64X" for pid 0x%04x has no name. Name '%s' made up.\n", base_addr, pid, made_up_name));
      if (ts == 0) {
         ts = TS_ANON;
      }
   }

   // If not named yet then set the name to the name of this module
   if (pn->name == NULL) {
      n = GetFilenameFromPath(name);
      if (n != NULL)
         pn->name = zstrdup(n);
      else
         pn->name = G(UnknownName);

      dbgmsg(("- A2nAddModule: pid 0x%04x not already named. Setting name to '%s'\n", pn->pid, pn->name));
   }

#if defined(_AIX)
   if (syml_install_one_pid(pid, 0, 0, 0, name, INIT_SYMS) == 0) {
      errmsg(("*E* syml_install_one_pid failure\n"));
      return (A2N_ADDMODULE_ERROR);
   }
#else
   // Add loaded module node to process node and to module cache if necessary
   lmn = AddLoadedModuleNode(pn, name, base_addr, length, ts, chksum, 0);

   // Done
   if (lmn == NULL) {
      dbgapi(("<< A2nAddModule: FAIL-could not add loaded module node.\n"));
      return (A2N_ADDMODULE_ERROR);
   }
#endif

   api_cnt.AddModule_OK++;
   G(changes_made)++;
   dbgapi(("<< A2nAddModule: OK\n"));
   return (0);
}


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
int A2nAddJittedMethod(unsigned int pid,     // Process id
                       char * name,          // Symbol name
                       uint64 addr,          // Symbol address
                       unsigned int length,  // Symbol length
                       char * code)          // Code (if any)
{
   PID_NODE * pn;
   LMOD_NODE * lmn;
   JM_NODE * jmn;


   api_cnt.AddJittedMethod++;
   dbgapi(("\n>> A2nAddJittedMethod: pid=0x%04x, addr=%"_LZ64X", len=0x%08x, code=0x%"_PP", nm='%s'\n",
           pid, addr, length, code, name));

   // Do a little bit of checking
   if (length == 0) {
      errmsg(("*E* A2nAddJittedMethod: method loaded at %"_LZ64X" for pid 0x%04x has length = 0.\n", addr, pid));
      return (A2N_INVALID_METHOD_LENGTH);
   }
   if (name == NULL) {
      errmsg(("*E* A2nAddJittedMethod: method loaded at %"_LZ64X" for pid 0x%04x has NULL name pointer.\n", addr, pid));
      return (A2N_NULL_METHOD_NAME);
   }

#if defined(_AIX)
   //
   // Find process node.  If there isn't one then create a new one.
   // This process will be a "root" process - it's not a descendant of any
   // other process and does not inherit modules from any other process.
   //
   pn = GetProcessNode(pid);
   if (pn == NULL) {                        // First add.  Need PID node
      pn = CreateProcessNode(pid);
      if (pn == NULL) {                     // Unable to add PID node
         errmsg(("*E* A2nAddJittedMethod: unable to create process node for 0x%04x-%s\n", pid, name));
         return (A2N_CREATEPROCESS_ERROR);
      }
   }
#else
   // Find process node
   pn = GetProcessNode(pid);
   if (pn == NULL) {                        // First add.  Need PID node
      errmsg(("*E* A2nAddJittedMethod: unable to find process node for 0x%04x\n", pid));
      return (A2N_GETPROCESS_ERROR);
   }
#endif

   // Add the loaded module node to the process node
   lmn = AddLoadedModuleNode(pn, name, addr, length, 0, 0, 1);
   if (lmn == NULL) {
      errmsg(("*E* A2nAddJittedMethod: unable to add LMOD_NODE for 0x%04x-%s\n", pid, name));
      return (A2N_ADD_JITTEDMETHOD_ERROR);
   }

   // Hang jitted method node off loaded module and add to jitted method pool
   jmn = AddJittedMethodNode(lmn, name, length, code, 0, 0, NULL);
   if (jmn == NULL) {
      errmsg(("*E* A2nAddJittedMethod: unable to add JM_NODE for 0x%04x-%s\n", pid, name));
      return (A2N_ADD_JITTEDMETHOD_ERROR);
   }

   api_cnt.AddJittedMethod_OK++;
   dbgapi(("<< A2nAddJittedMethod: OK\n"));
   return (0);
}


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
int A2nAddJittedMethodEx(unsigned int pid,     // Process id
                         char * name,          // Jitted method name
                         uint64 addr,          // Jitted method address
                         unsigned int length,  // Jitted method length (in bytes)
                         char * code,          // Code (or NULL if no code)
                         int jln_type,         // JVMTI-style line number information
                         int jln_cnt,          // Number of JTLINENUMBER/JPLINENUMBER structures
                         void * jln)           // Array of JTLINENUMBER/JPLINENUMBER structures
{
   PID_NODE * pn;
   LMOD_NODE * lmn;
   JM_NODE * jmn;


   api_cnt.AddJittedMethod++;
   dbgapi(("\n>> A2nAddJittedMethodEx: pid=0x%04x, addr=%"_LZ64X", len=0x%08x, code=0x%"_PP", jln_type=%d, jln_cnt=0x%08x, jln=0x%"_PP" nm='%s'\n",
           pid, addr, length, code, jln_type, jln_cnt, jln, name));

   // Do a little bit of checking
   if (length == 0) {
      errmsg(("*E* A2nAddJittedMethodEx: method loaded at %"_LZ64X" for pid 0x%04x has length = 0.\n", addr, pid));
      return (A2N_INVALID_METHOD_LENGTH);
   }
   if (name == NULL) {
      errmsg(("*E* A2nAddJittedMethodEx: method loaded at %"_LZ64X" for pid 0x%04x has NULL name pointer.\n", addr, pid));
      return (A2N_NULL_METHOD_NAME);
   }
   if (jln != NULL) {
      if (jln_type != JLN_TYPE_JVMTI) {
         errmsg(("*E* A2nAddJittedMethodEx: invalid jln_type value (%d).\n", jln_type));
         return (A2N_INVALID_ARGUMENT);
      }
      if (jln_cnt <= 0) {
         errmsg(("*E* A2nAddJittedMethodEx: invalid jln_cnt value (%d). Must be > 0.\n", jln_cnt));
         return (A2N_INVALID_ARGUMENT);
      }
   }

#if defined(_AIX)
   //
   // Find process node.  If there isn't one then create a new one.
   // This process will be a "root" process - it's not a descendant of any
   // other process and does not inherit modules from any other process.
   //
   pn = GetProcessNode(pid);
   if (pn == NULL) {                        // First add.  Need PID node
      pn = CreateProcessNode(pid);
      if (pn == NULL) {                     // Unable to add PID node
         errmsg(("*E* A2nAddJittedMethodEx: unable to create process node for 0x%04x-%s\n", pid, name));
         return (A2N_CREATEPROCESS_ERROR);
      }
   }
#else
   // Find process node
   pn = GetProcessNode(pid);
   if (pn == NULL) {                        // First add.  Need PID node
      errmsg(("*E* A2nAddJittedMethodEx: unable to find process node for 0x%04x\n", pid));
      return (A2N_GETPROCESS_ERROR);
   }
#endif

   // Add the loaded module node to the process node
   lmn = AddLoadedModuleNode(pn, name, addr, length, 0, 0, 1);
   if (lmn == NULL) {
      errmsg(("*E* A2nAddJittedMethodEx: unable to add LMOD_NODE for 0x%04x-%s\n", pid, name));
      return (A2N_ADD_JITTEDMETHOD_ERROR);
   }

   // Hang jitted method node off loaded module and add to jitted method pool
   jmn = AddJittedMethodNode(lmn, name, length, code, jln_type, jln_cnt, jln);
   if (jmn == NULL) {
      errmsg(("*E* A2nAddJittedMethodEx: unable to add JM_NODE for 0x%04x-%s\n", pid, name));
      return (A2N_ADD_JITTEDMETHOD_ERROR);
   }

   api_cnt.AddJittedMethod_OK++;
   dbgapi(("<< A2nAddJittedMethodEx: OK\n"));
   return (0);
}


//
// A2nAddMMIMethod()
// *****************
//
// Add dynamic code (by the MMI) information to a process.
// The process to which the MMI method symbol will be added *MUST* exist.
//
// Returns
//    0:        Jitted method symbol added successfully
//    non-zero: Error
//
int A2nAddMMIMethod(unsigned int pid,        // Process id
                    char * name,             // Jitted method name
                    uint64 addr,             // Jitted method address
                    unsigned int length,     // Jitted method length (in bytes)
                    char * code)             // Code (or NULL if no code)
{
   PID_NODE * pn;
   LMOD_NODE * lmn;
   JM_NODE * jmn;


   dbgapi(("\n>> A2nAddMMIMethod: pid=0x%04x, addr=%"_LZ64X", len=0x%08x, code=%p, nm='%s'\n",
           pid, addr, length, code, name));

   // Do a little bit of checking
   if (length == 0) {
      errmsg(("*E* A2nAddMMIMethod: MMI method loaded at %"_LZ64X" for pid 0x%04x has length = 0.\n", addr, pid));
      return (A2N_INVALID_METHOD_LENGTH);
   }
   if (name == NULL) {
      errmsg(("*E* A2nAddMMIMethod: MMI method loaded at %"_LZ64X" for pid 0x%04x has NULL name pointer.\n", addr, pid));
      return (A2N_NULL_METHOD_NAME);
   }

   //
   // Find process node.  If there isn't one then create a new one.
   // This process will be a "root" process - it's not a descendant of any
   // other process and does not inherit modules from any other process.
   //
   pn = GetProcessNode(pid);
   if (pn == NULL) {                        // First add.  Need PID node
      pn = CreateProcessNode(pid);
      if (pn == NULL) {                     // Unable to add PID node
         errmsg(("*E* A2nAddMMIMethod: unable to create process node for 0x%04x-%s\n", pid, name));
         return (A2N_CREATEPROCESS_ERROR);
      }
   }

   // Add the loaded module node to the process node
   lmn = AddLoadedModuleNode(pn, name, addr, length, 0, 0, 1);
   if (lmn == NULL) {
      errmsg(("*E* A2nAddMMIMethod: unable to add LMOD_NODE for 0x%04x-%s\n", pid, name));
      return (A2N_ADD_MMIMETHOD_ERROR);
   }

   // Hang jitted method node off loaded module and add to jitted method pool
   jmn = AddJittedMethodNode(lmn, name, length, code, 0, 0, NULL);
   if (jmn == NULL) {
      errmsg(("*E* A2nAddMMIMethod: unable to add JM_NODE for 0x%04x-%s\n", pid, name));
      return (A2N_ADD_MMIMETHOD_ERROR);
   }

   jmn->flags |= A2N_FLAGS_MMI_METHOD;

   dbgapi(("<< A2nAddMMIMethod: OK\n"));
   return (0);
}


//
// A2nLoadMap()
// ************
//
// Loads the current mappings of shared objects for a specified process.
//
// Returns
//    0:        Mappings successfully loaded
//    non-zero: Error
//
int A2nLoadMap(unsigned int pid, int loadAnon)
{
   int rc;

   dbgapi(("> A2nLoadMap(%d/0x%x): starting\n", pid, pid));
   rc = LoadMap(pid, loadAnon);
   dbgapi(("< A2nLoadMap(%d/0x%x) rc = %d\n", pid, pid, rc));
   return (rc);
}


//
// SetSymbolNameAddrLenth()
// ************************
//
// Set sym_name, sym_addr and sym_length for the cases where there
// are no symbols or no symbol was found.
//
static void SetSymbolNameAddrLenth(char * name, SYMDATA * sd, LMOD_NODE * lmn, SEC_NODE * cn)
{
   if (G(nsf_action) == NSF_ACTION_NSF) {
      sd->sym_name = name;                  // Want "SymbolNotFound" name
      sd->sym_addr = sd->mod_addr;
      sd->sym_length = sd->mod_length;
   }
   else if (G(nsf_action) == NSF_ACTION_SECTION) {
      if (cn == NULL) {
         sd->sym_name = name;               // Want section name but don't have it
         sd->sym_addr = sd->mod_addr;
         sd->sym_length = sd->mod_length;
      }
      else {
         sd->sym_name = cn->alt_name;       // Want section name and we have it
         sd->sym_addr = lmn->start_addr + cn->offset_start;
         sd->sym_length = cn->size;
      }
   }
   else {
      sd->sym_name = NULL;                  // Want NULL symbol name
      sd->sym_addr = sd->mod_addr;
      sd->sym_length = sd->mod_length;
   }

   return;
}


//
// OnlyFakeSymbols()
// *****************
//
static int OnlyFakeSymbols(MOD_NODE * mn)
{
#if defined(_WINDOWS64)
   if (mn->flags & A2N_FLAGS_ILT_SYMBOLS_ONLY) {
      // The only symbols are ILTs which are fake.
      dbgmsg(("* OnlyFakeSymbols: Only symbol in %s is ILT\n", mn->name));
      return (1);
   }
#elif defined(_LINUX) || defined(_ZOS)
   if (mn->flags & A2N_FLAGS_PLT_SYMBOLS_ONLY) {
      // The only symbol is the PLT (which is fake).
      dbgmsg(("* OnlyFakeSymbols: Only symbol in %s is PLT\n", mn->name));
      return (1);
   }
#endif
   // The fake symbols aren't the only ones
   return (0);
}


//
// DumpLsnData()
// *************
//
#if defined(DEBUG)
static void DumpLsnData(LASTSYM_NODE * lsn)
{
   SYMDATA * lsnsd = &lsn->sd;

   if (lsn->rc != A2N_SUCCESS || G(dump_sd)) {
      dbgmsg((">> lsn->opn               = %p\n"
              ">> lsn->lmn               = %p\n"
              ">> lsn->cn                = %p\n"
              ">> lsn->sn                = %p\n"
              ">> lsn->sym_code          = %p\n"
              ">> lsn->valid             = %d\n"
              ">> lsn->rc                = %d\n"
              ">> lsnsd->owning_pid_name = %s\n"
              ">> lsnsd->owning_pid      = %x\n"
              ">> lsnsd->mod_name        = %s\n"
              ">> lsnsd->mod_addr        = %"_LZ64X"\n"
              ">> lsnsd->mod_length      = %x\n"
              ">> lsnsd->sym_name        = %s\n"
              ">> lsnsd->sym_addr        = %"_LZ64X"\n"
              ">> lsnsd->sym_length      = %x\n"
              ">> lsnsd->code            = %p\n"
              ">> lsnsd->flags           = 0x%08X\n",
              lsn->opn,
              lsn->lmn,
              lsn->cn,
              lsn->sn,
              lsn->sym_code,
              lsn->valid,
              lsn->rc,
              lsnsd->owning_pid_name,
              lsnsd->owning_pid,
              lsnsd->mod_name,
              lsnsd->mod_addr,
              lsnsd->mod_length,
              lsnsd->sym_name,
              lsnsd->sym_addr,
              lsnsd->sym_length,
              lsnsd->code,
              lsnsd->flags));
   }
   return;
}
#else
#define DumpLsnData(a)
#endif


//
// DumpReturnedSymData()
// *********************
//
#if defined(DEBUG)
static void DumpReturnedSymData(int rc, SYMDATA * sd, int lsnvalid)
{
   if (rc != A2N_SUCCESS || G(dump_sd)) {
      dbgmsg(("<< sd->owning_pid_name = %s\n"
              "<< sd->owning_pid      = %x\n"
              "<< sd->mod_name        = %s\n"
              "<< sd->mod_addr        = %"_LZ64X"\n"
              "<< sd->mod_length      = %x\n"
              "<< sd->sym_name        = %s\n"
              "<< sd->sym_addr        = %"_LZ64X"\n"
              "<< sd->sym_length      = %x\n"
              "<< sd->code            = %p\n"
              "<< sd->src_file        = %s\n"
              "<< sd->src_lineno      = %d\n"
              "<< sd->process         = %p\n"
              "<< sd->module          = %p\n"
              "<< sd->symbol          = %p\n"
              "<< sd->flags           = 0x%08X\n"
              "<< lsnvalid            = %d\n",
              sd->owning_pid_name,
              sd->owning_pid,
              sd->mod_name,
              sd->mod_addr,
              sd->mod_length,
              sd->sym_name,
              sd->sym_addr,
              sd->sym_length,
              sd->code,
              sd->src_file,
              sd->src_lineno,
              sd->process,
              sd->module,
              sd->symbol,
              sd->flags,
              lsnvalid));
   }
   return;
}
#else
#define DumpReturnedSymData(a, b, c)
#endif


//
// A2nGetSymbol()
// **************
//
// Get minimal symbol information for a given address in a given pid.
//
// Returns
//    A2N_SUCCESS:         Complete symbol information returned.
//    A2N_NO_SYMBOL_FOUND: Only module name (and maybe process name) returned.
//                         (Module had symbols but no symbol found for addr)
//    A2N_NO_SYMBOLS:      Only module name (and maybe process name) returned.
//                         (Module did not have symbols)
//    A2N_NO_MODULE:       No module nor symbol name returned.
//                         (addr did not fall in address range of any loaded module)
//    Other non-zero:      Error or symbol not found.
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
int A2nGetSymbol(unsigned int pid,          // Process id
                 uint64 addr,               // Address
                 SYMDATA * sd)              // Pointer to SYMDATA structure
{
   PID_NODE * pn = NULL, * opn = NULL;
   LMOD_NODE * lmn = NULL;
   MOD_NODE * mn = NULL;
   JM_NODE * jmn = NULL;
   SYM_NODE * sn = NULL;
   SEC_NODE * cn = NULL;
   LASTSYM_NODE * lsn = NULL;
   SYMDATA * lsnsd;
   void * sym_code = NULL;
   uint sym_offset = 0;
   unsigned int code_offset;
   uint64 range_start, range_end, caddr;
   int rc;
   int lsnvalid = 1;
   int in_a_range = 0;
   char * r;


   api_cnt.GetSymbol++;
   dbgapi(("\n>> A2nGetSymbol: **ENTRY** pid=0x%04x, addr=%"_LZ64X", pSYMDATA=%p\n", pid, addr, sd));

   if (sd == NULL) {                        // SYMDATA pointer must not be NULL
      errmsg(("*E* A2nGetSymbol: %s NULL SYMDATA pointer.\n", gs_res_error));
      return (A2N_NULL_SYMDATA_ERROR);
   }
   memset(sd, 0, sizeof(SYMDATA));          // Initialize SYMDATA structure
   caddr = addr;

   // Get process node for requested process
   pn = GetProcessNode(pid);
   if (pn == NULL) {
      //
      // Haven't seen this pid so the only place we'll look for symbols is
      // in the kernel.  If we haven't seen the kernel either then there's
      // nothing we can do for this request.
      //
      if (G(SystemPidNode) == NULL) {
         errmsg(("*E* A2nGetSymbol: %s unable to find valid process node for 0x%04x.\n", gs_res_error, pid));
         sd->mod_name = ModuleNotFound;
         sd->sym_name = NoSymbolData;
         sd->owning_pid_name = G(UnknownName);
         sd->owning_pid = (uint)-1;
         return (A2N_GETPROCESS_ERROR);     // Can't find valid process node.
      }

      pn = opn = G(SystemPidNode);
      sd->owning_pid = pid;
      sd->owning_pid_name = G(UnknownName);
   }
   else {
      // Have seen this pid. Get owning process information
      opn = GetOwningProcessNode(pn);
      sd->owning_pid = opn->pid;
      sd->owning_pid_name = opn->name;
   }
   sd->process = pn;
   if (pn->flags & A2N_FLAGS_32BIT_PROCESS)
      sd->flags |= SDFLAG_PROCESS_32BIT;
   else if (pn->flags & A2N_FLAGS_64BIT_PROCESS)
      sd->flags |= SDFLAG_PROCESS_64BIT;

#if defined(_AIX)
   rc = GetSymlibSymbol(pid, addr, sd);
   if (rc == A2N_SUCCESS)
      goto TheEnd;
#endif

   //
   // Attempt fastpath ...
   // * If the given address falls within the last symbol returned
   //   for this pid then it must be the same symbol so return the
   //   information from the last_symbol node.
   // * If the given address falls within the same module as the last
   //   symbol returned for this pid then it must be in the same module.
   //   Go lookup symbol in the module
   // * Do full symbol lookup.
   //
   // * lsn->valid is *NEVER* set if the previous A2nGetSymbol() returned
   //   A2N_NO_MODULE or A2N_NO_SYMBOL_FOUND.
   //
   if (G(do_fast_lookup) == 1) {
      lsn = pn->lastsym;                    // Last symbol (data node) returned on this pid
      if (lsn->valid == 0) {
         // pid doesn't match or last symbol node invalid. Do full lookup.
         dbgmsg(("* A2nGetSymbol: **FASTPATH-INVALID_LSN** Doing full lookup.\n"));
         goto LookupLoadedModule;
      }

      DumpLsnData(lsn);                     // DEBUG stuff
      lsnsd = &lsn->sd;                     // Last SD returned

      range_end = lsnsd->sym_addr + (lsnsd->sym_length - 1);
      dbgmsg(("* A2nGetSymbol: range_end for last symbol = %"_LZ64X"\n", range_end));
      if (caddr >= lsnsd->sym_addr && caddr <= range_end) {
         dbgmsg(("* A2nGetSymbol: %"_LZ64X" within last symbol returned\n", caddr));
         //
         // Within last symbol (which can also be a range) returned.
         // - If last symbol was within a range *AND* they want the symbols in
         //   the range identified then make sure we find the right symbol.
         // - If last symbol was not within a range then return it again.
         //
         lmn = lsn->lmn;
         sn = lsn->sn;
         if (lsn->range == 1  &&  G(return_range_symbols) == 1) {
            //
            // Last symbol was in a range *AND* they want to know the
            // symbol within range. We need to make sure the last symbol
            // is the same as the one where this addr falls.
            // If addr is outside the virtual address range of the last
            // symbol (not the range) then go look up the new symbol.
            //
            if (sn == NULL) {
               errmsg(("*E* A2nGetSymbol: %s **BUG** sn == NULL for symbol withn range.\n", gs_res_error));
               exit (-1);
            }
            range_start = lmn->start_addr + sn->offset_start;
            range_end   = range_start + (sn->length - 1);
            dbgmsg(("* A2nGetSymbol: lastsymbol range: %"_LZ64X" - %"_LZ64X"\n", range_start, range_end));
            if (caddr < range_start || caddr > range_end) {
               // addr is not in the range of the last symbol returned
               mn = lmn->mn;
               lmn->req_cnt++;                    // Loaded module requested again
               if (lmn->flags & A2N_FLAGS_KERNEL)
                  pn->kernel_req++;               // Requested address in kernel

               sd->mod_addr = lmn->start_addr;
               sd->mod_length = lmn->total_len;
               sd->module = lmn;
               sd->mod_req_cnt = lmn->req_cnt;

               api_cnt.GetSymbol_FastpathMod++;
               dbgmsg(("* A2nGetSymbol: **FASTPATH-SAMERANGE_DIFFSYM**  Looking in module.\n"));
               goto LookupSymbol;
            }
         }

         //
         // Handle case where there really are no symbols BUT there is the fake
         // PLT or ILT symbol. If the requested address is in one of those then
         // we want to return that, otherwise we want to return NO_SYMBOLS (instead
         // of NO_SYMBOL_FOUND). Just go look up the symbol.
         // Don't forget (like I did) that this is for real modules/symbols, not
         // for jitted methods.
         //
         if (lmn->type != LMOD_TYPE_JITTED_CODE) {
            mn = lmn->mn;
            if ((mn->flags & A2N_FLAGS_ILT_SYMBOLS_ONLY) || (mn->flags & A2N_FLAGS_PLT_SYMBOLS_ONLY)) {
               lmn->req_cnt++;                    // Loaded module requested again
               sd->mod_addr = lmn->start_addr;
               sd->mod_length = lmn->total_len;
               sd->module = lmn;
               sd->mod_req_cnt = lmn->req_cnt;

               api_cnt.GetSymbol_FastpathMod++;
               dbgmsg(("* A2nGetSymbol: **FASTPATH-PLT-ILT-ONLY**  Look up symbol to be sure.\n"));
               goto LookupSymbol;
            }
         }

         //
         // * Not in a range.
         // * In a range and they don't want symbols within the range.
         // * In a range, they want symbols within the range, and it's the
         //   same symbol we returned last.
         // * Not in a module that only has ILT/PLT pseudo symbol.
         //
         dbgmsg(("* A2nGetSymbol: not in a range ...\n"));
         memcpy(sd, lsnsd, sizeof(SYMDATA));
         lmn->req_cnt++;                    // Loaded module requested again
         if (lmn->flags & A2N_FLAGS_KERNEL)
            pn->kernel_req++;               // Requested address in kernel

         if (lmn->type == LMOD_TYPE_JITTED_CODE) {
            lmn->jmn->req_cnt++;            // Method requested again
            dbgmsg(("* A2nGetSymbol: this is jitted code ...\n"));
         }
         else {
            dbgmsg(("* A2nGetSymbol: this is not jitted code ...\n"));
            lmn->mn->req_cnt++;             // Module requested again
            if (sn != NULL) {
               sn->req_cnt++;               // Symbol requested again
               if (sn->rn != NULL)
                  sn->rn->req_cnt++;        // Range requested again
            }
         }

         if (lsn->rc == A2N_NO_SYMBOLS) {
            cn = GetSectionNode(lmn, addr); // Find section ...
            if (cn != NULL  &&  cn->code != NULL) {
               code_offset = (unsigned int)PtrSub(addr, PtrAdd(lmn->start_addr, cn->offset_start));
               sd->code = (char *)PtrAdd(cn->code, code_offset);
            }
            else
               sd->code = NULL;
         }
         else {
            if (lsn->sym_code != NULL) {
               dbgmsg(("* A2nGetSymbol: Calculate code addr. sym_addr=%"_LZ64X"\n", sd->sym_addr));
               code_offset = (uint)PtrSub(addr, sd->sym_addr);
               sd->code = (char *)PtrAdd(lsn->sym_code, code_offset);
            }
         }

         sd->module = lmn;
         sd->symbol = sn;
         sd->flags = lsnsd->flags;
         sd->mod_req_cnt = lmn->req_cnt;
         if (sn != NULL)
            sd->sym_req_cnt = sn->req_cnt;
         else
            sd->sym_req_cnt = 0;

         // If they want line numbers and we have a good symbol then get them ...
         if (G(lineno) && (lsn->rc == A2N_SUCCESS)) {
            GetLineNumber(lmn, sn, addr, &(sd->src_file), &(sd->src_lineno));
         }
         api_cnt.GetSymbol_FastpathOk++;
         dbgapi(("<< A2nGetSymbol: %s pid=0x%04x  addr=%"_LZ64X"  rc=%d  sym='%s' off=0x%X  mod='%s' src=%s line=%d\n",
                 gs_res_fastok, pid, addr, lsn->rc, sd->sym_name, PtrSub(addr,sd->sym_addr), sd->mod_name, sd->src_file, sd->src_lineno));
         return (lsn->rc);
      }

      range_end = lsnsd->mod_addr + (lsnsd->mod_length - 1);
      dbgmsg(("* A2nGetSymbol: range_end_mod = %08X\n", range_end));
      if (caddr >= lsnsd->mod_addr && caddr <= range_end) {
         // Within last module returned. Go find the symbol in this module.
         lmn = lsn->lmn;
         mn = lmn->mn;
         lmn->req_cnt++;                    // Loaded module requested again
         if (lmn->flags & A2N_FLAGS_KERNEL)
            pn->kernel_req++;               // Requested address in kernel

         sd->mod_addr = lmn->start_addr;
         sd->mod_length = lmn->total_len;
         sd->module = lmn;
         sd->mod_req_cnt = lmn->req_cnt;

         api_cnt.GetSymbol_FastpathMod++;
         dbgmsg(("* A2nGetSymbol: **FASTPATH-SAME_MOD**  Looking in module.\n"));
         goto LookupSymbol;
      }

      // Not in same symbol or module. Do full lookup ...
      api_cnt.GetSymbol_FastpathNone++;
      dbgapi(("* A2nGetSymbol: **FASTPATH-DIFF_SYM/MOD** Doing full lookup.\n"));
      goto LookupLoadedModule;
   }

LookupLoadedModule:

   // Get loaded module node where this symbol falls
   lmn = GetLoadedModuleNode(pn, addr);
   if (lmn == NULL) {
#if defined(_WINDOWS) && defined(_X86)
      // Check if it's the SharedUserData page and special-case it.
      if (addr >= SHARED_USER_DATA_START && addr <= SHARED_USER_DATA_END) {
         sd->mod_name = SharedUserDataName;
         lsnvalid = 0;                      // >>>>> Force full lookup on next A2nGetSym on this pid <<<<<
         if (addr >= SYSTEMCALL_STUB_START && addr <= SYSTEMCALL_STUB_END) {
            // It's the SystemCallStub
            sd->flags |= SDFLAG_SYMBOL_SYSCALLSTUB | SDFLAG_SYMBOL_IS_FAKE;
            sd->sym_name = SystemCallStubName;
            sd->sym_addr = SYSTEMCALL_STUB_START;
            sd->sym_length = SYSTEMCALL_STUB_LENGTH;
         }
         else {
            // It's somewhere in SharedUserData. Why would that be?
            sd->flags |= SDFLAG_SYMBOL_IS_FAKE;
            sd->sym_name = SharedUserDataUnknownSymbol;
            sd->sym_addr = SHARED_USER_DATA_START;
            sd->sym_length = SHARED_USER_DATA_LENGTH;

         }
         sd->flags |= SDFLAG_MODULE_32BIT;
         api_cnt.GetSymbol_OKSymbol++;
         rc = A2N_SUCCESS;
         goto TheEnd;
      }
#endif
      infomsg(("*I* A2nGetSymbol: NoModule for (0x%04x-%"_LZ64X").\n", pid, addr));
      sd->mod_name = ModuleNotFound;
      sd->sym_name = NoSymbolData;
      lsnvalid = 0;                         // >>>>> Force full lookup on next A2nGetSym on this pid <<<<<
      api_cnt.GetSymbol_NoModule++;
      rc = A2N_NO_MODULE;                   // Can't find loaded module node
      goto TheEnd;
   }
   sd->module = lmn;
   mn = lmn->mn;

   lmn->flags |= A2N_FLAGS_REQUESTED;       // Loaded module requested at least once
   lmn->req_cnt++;                          // Loaded module requested again
   if (lmn->flags & A2N_FLAGS_KERNEL)
      pn->kernel_req++;                     // Requested address in kernel

   sd->mod_addr = lmn->start_addr;
   sd->mod_length = lmn->total_len;
   sd->mod_req_cnt = lmn->req_cnt;

   // Handle jitted methods ...
   if (lmn->type == LMOD_TYPE_JITTED_CODE) {
      jmn = lmn->jmn;

      // ##### <HACK> #####
      // Until I figure out why this happens. Only happens when running
      // SCS, because of the multiple threads.
      if (jmn == NULL) {
         sd->mod_name = ModuleNotFound;
         sd->sym_name = NoSymbolData;
         lsnvalid = 0;
         api_cnt.GetSymbol_NoModule++;
         rc = A2N_INTERNAL_NON_FATAL_ERROR;
         errmsg(("*E* A2nGetSymbol: %s A2N_INTERNAL_NON_FATAL_ERROR: NULL jmn for (0x%04x-%"_LZ64X").\n", gs_res_error, pid, addr));
         goto TheEnd;
      }
      // ##### </HACK> #####

      sd->symbol = jmn;
      sd->sym_name = jmn->name;

      if (lmn->jmn->flags & A2N_FLAGS_MMI_METHOD) {
         sd->mod_name = JittedMmiCodeModuleName;
         sd->flags |= SDFLAG_SYMBOL_MMI;
      }
      else {
         sd->mod_name = JittedCodeModuleName;
         sd->flags |= SDFLAG_SYMBOL_JITTED;
      }

      // If we know the type of process then set the type of method
      if (pn->flags & A2N_FLAGS_32BIT_PROCESS)
         sd->flags |= SDFLAG_MODULE_32BIT;
      else if (pn->flags & A2N_FLAGS_64BIT_PROCESS)
         sd->flags |= SDFLAG_MODULE_64BIT;

      if (!(jmn->flags & A2N_FLAGS_BLANKS_CHANGED)) {
         ChangeBlanksToSomethingElse(jmn->name);
         jmn->flags |= A2N_FLAGS_BLANKS_CHANGED;
      }
      sd->sym_addr = lmn->start_addr;
      sd->sym_length = jmn->length;
      if (jmn->code != NULL) {
         code_offset = (unsigned int)PtrSub(addr, sd->sym_addr);
         sd->code = (char *)PtrAdd(code_offset, jmn->code);
      }

      if (G(lineno))
         GetLineNumber(lmn, NULL, addr, &(sd->src_file), &(sd->src_lineno));

      sym_code = jmn->code;                 // >>>>> Remember code at start of symbol <<<<<
      jmn->flags |= A2N_FLAGS_REQUESTED;    // Method requested at least once
      jmn->req_cnt++;                       // Method requested again
      sd->sym_req_cnt = jmn->req_cnt;
      api_cnt.GetSymbol_OKJittedMethod++;
      rc = A2N_SUCCESS;
      goto TheEnd;
   }

   // Don't cache anonymous sections. Always look them up.
   if (lmn->type == LMOD_TYPE_ANON)
      lsnvalid = 0;

LookupSymbol:
   //
   // Handle regular (non-jitted method) symbols ...
   // sn = NULL       don't have a symbol
   // sn = -1         have a symbol but validation failed so it's no good
   // sn = non-NULL   have a symbol
   //
   sn = GetSymbolNode(lmn, addr);           // Get symbol information
   sd->symbol = sn;

   // ##### <HACK> #####
   // Until I figure out why this happens. Only happens when running
   // SCS, because of the multiple threads.
   if (mn == NULL) {
      sd->mod_name = ModuleNotFound;
      sd->sym_name = NoSymbolData;
      lsnvalid = 0;
      api_cnt.GetSymbol_NoModule++;
      rc = A2N_INTERNAL_NON_FATAL_ERROR;
      errmsg(("*E* A2nGetSymbol: %s A2N_INTERNAL_NON_FATAL_ERROR: LookupSymbol and mn == NULL for (0x%04x-%"_LZ64X").\n", gs_res_error, pid, addr));
      goto TheEnd;
   }
   // ##### </HACK> #####

   // Fill in module information.
#if defined(_LINUX)
   if (mn->flags & A2N_FLAGS_VDSO)
      sd->mod_name = ModuleVdso;
   else
#endif
      sd->mod_name = mn->name;

   if (mn->flags & A2N_FLAGS_64BIT_MODULE)
      sd->flags |= SDFLAG_MODULE_64BIT;
   else if (mn->flags & A2N_FLAGS_32BIT_MODULE)
      sd->flags |= SDFLAG_MODULE_32BIT;

   if (!(mn->flags & A2N_FLAGS_BLANKS_CHANGED)) {
      ChangeBlanksToSomethingElse(mn->name);
      mn->flags |= A2N_FLAGS_BLANKS_CHANGED;
   }

   mn->flags |= A2N_FLAGS_REQUESTED;        // Module requested at least once
   mn->req_cnt++;                           // Module requested again

   // If there is no symbol information, either because there is no
   // symbol or because validation failed, then set the symbol name accordingly.
   if (sn == NULL  ||  sn == (SYM_NODE *)-1) {
      cn = GetSectionNode(lmn, addr);       // Find section ...

      // If there's code calculate the (local) address to the code stream.
      if (cn != NULL  &&  cn->code != NULL) {
         code_offset = (unsigned int)PtrSub(addr, PtrAdd(lmn->start_addr, cn->offset_start));
         sd->code = (char *)PtrAdd(cn->code, code_offset);
         sym_code = (void *)cn->code;       // >>>>> Remember code at start of symbol <<<<<
      }
      else {
         sym_code = NULL;                   // >>>>> Remember there's no code <<<<<
      }

      if (sn == NULL) {
         // No symbols *OR* symbol not found ...
         if (mn->symcnt != 0) {
            //
            // Special-case the pseudo symbols ILT (Windows on IA64) or PLT (Linux/z/OS).
            // If we added one of these and there are no other symbols then
            // mn->symcnt will be non-zero (hopefully 1). So check and make
            // sure we return the correct symbol/rc.
            //
            if (OnlyFakeSymbols(mn)) {
               // This module really has NO_SYMBOLS so tell them so.
               dbgmsg(("- A2nGetSymbol: NoSymbol for (0x%04x-%"_LZ64X") in %s (*** only pseudo symbol is ILT/PLT ***)\n", pid, addr, sd->mod_name));
               SetSymbolNameAddrLenth(NoSymbolData, sd, lmn, cn);
               api_cnt.GetSymbol_NoSymbols++;
               rc = A2N_NO_SYMBOLS;
            }
            else {
#if defined(_WINDOWS) && defined(_X86)
               // Check if it's the SharedUserData page and special-case it.
               // It should be reported under ntdll.dll and that should
               // already be the module name.
               if (addr >= SHARED_USER_DATA_START && addr <= SHARED_USER_DATA_END) {
                  if (addr >= SYSTEMCALL_STUB_START && addr <= SYSTEMCALL_STUB_END) {
                     // It's the SystemCallStub
                     sd->flags |= SDFLAG_SYMBOL_SYSCALLSTUB | SDFLAG_SYMBOL_IS_FAKE;
                     sd->sym_name = SystemCallStubName;
                     sd->sym_addr = SYSTEMCALL_STUB_START;
                     sd->sym_length = SYSTEMCALL_STUB_LENGTH;
                  }
                  else {
                     // It's somewhere in SharedUserData. Why would that be?
                     sd->flags |= SDFLAG_SYMBOL_IS_FAKE;
                     sd->sym_name = SharedUserDataUnknownSymbol;
                     sd->sym_addr = SHARED_USER_DATA_START;
                     sd->sym_length = SHARED_USER_DATA_LENGTH;
                  }
                  api_cnt.GetSymbol_OKSymbol++;
                  rc = A2N_SUCCESS;
                  goto TheEnd;
               }
#endif
               // NO SYMBOL FOUND: module has symbols but there's no symbol for given address.
               SetSymbolNameAddrLenth(SymbolNotFound, sd, lmn, cn);
               lsnvalid = 0;                      // >>>>> Force full lookup on next A2nGetSym on this pid <<<<<
               api_cnt.GetSymbol_NoSymbolFound++;
               infomsg(("*I* A2nGetSymbol: NoSymbolFound for (0x%04x-%"_LZ64X") in %s\n", pid, addr, sd->mod_name));
               rc = A2N_NO_SYMBOL_FOUND;
            }
            goto TheEnd;
         }
         else {
            // NO SYMBOLS: module has no symbols.
            SetSymbolNameAddrLenth(NoSymbolData, sd, lmn, cn);
            api_cnt.GetSymbol_NoSymbols++;
            rc = A2N_NO_SYMBOLS;
            goto TheEnd;
         }
      }

      if (sn == (SYM_NODE *)-1) {
         // There are symbols but they're invalid (ie. validation failed)
         SetSymbolNameAddrLenth(NoSymbolData, sd, lmn, cn);
         sn = NULL;                         // so we don't break later!
         api_cnt.GetSymbol_NoSymbolsValFailed++;
         rc = A2N_NO_SYMBOLS;               // Have module name and maybe process name
         goto TheEnd;
      }
   }

   // We have symbol information ...
#if defined(_WINDOWS)
   if (G(symbol_quality) == MODE_GOOD_SYMBOLS &&
       (sn->flags & A2N_FLAGS_SYMBOL_LOW_QUALITY)) {
      //
      // There are symbols but not of the quality the user wants.
      // Tell'em there are no symbols.
      //
      // ***NOTE***
      // Any symbol which *I* harvest, with the exception of exports, I consider
      // to be good quality, whether it comes from a map, DBG file or the image
      // itself.
      // Symbols which the MS harvester returns I consider good quality, with
      // the exception of EXPORTS.  Maybe I should consider them all low
      // quality because statics are never returned.
      //
      cn = GetSectionNode(lmn, addr);       // Find section ...

      // If there's code calculate the (local) address to the code stream.
      if (cn != NULL  &&  cn->code != NULL) {
         code_offset = (unsigned int)PtrSub(addr, PtrAdd(lmn->start_addr, cn->offset_start));
         sd->code = (char *)PtrAdd(cn->code, code_offset);
         sym_code = (void *)cn->code;       // >>>>> Remember code at start of symbol <<<<<
      }
      else {
         sym_code = NULL;                   // >>>>> Remember there's no code <<<<<
      }

      SetSymbolNameAddrLenth(NoSymbolData, sd, lmn, cn);
      api_cnt.GetSymbol_NoSymbolsQuality++;
      infomsg(("*I* A2nGetSymbol: NoSymbol (quality_not_met) found for (0x%04x-%"_LZ64X") in %s\n", pid, addr, sd->mod_name));
      rc = A2N_NO_SYMBOLS;                  // Have module name and maybe process name
      goto TheEnd;
   }

   if (sn->type == SYMBOL_TYPE_MS_EXPORT)
      sd->flags |= SDFLAG_SYMBOL_EXPORT;
#endif

   // OK to return symbol information so fill it in ...
   if (sn->rn == NULL) {
      // Not a symbol range. Use data in the symbol node
      sd->sym_name = sn->name;
      if (!(sn->flags & A2N_FLAGS_BLANKS_CHANGED)) {
         ChangeBlanksToSomethingElse(sn->name);
         sn->flags |= A2N_FLAGS_BLANKS_CHANGED;
      }
      sd->sym_addr = lmn->start_addr + sn->offset_start;
      sd->sym_length = sn->length;
      if (sn->code != NULL) {
         if ((lmn->mn->flags & A2N_FLAGS_VALIDATION_FAILED) == 0) {
            code_offset = (unsigned int)(addr - sd->sym_addr);
            sd->code = (char *)PtrAdd(code_offset, sn->code);
         }
      }
      sym_code = sn->code;                  // >>>>> Remember code at start of symbol <<<<<

      // If they want line numbers then get them ...
      if (G(lineno)) {
         GetLineNumber(lmn, sn, addr, &(sd->src_file), &(sd->src_lineno));
      }
   }
   else {
      // Symbol range. Use data in the range node
      // No line numbers for ranges.
      in_a_range = 1;
      sd->flags |= SDFLAG_SYMBOL_RANGE;

      if (G(return_range_symbols) == 1)
         sd->sym_name = sn->name;
      else
         sd->sym_name = sn->rn->name;

      sd->sym_addr = lmn->start_addr + sn->rn->offset_start;
      sd->sym_length = sn->rn->length;
      if (sn->rn->code != NULL) {
         if ((lmn->mn->flags & A2N_FLAGS_VALIDATION_FAILED) == 0) {
            code_offset = (unsigned int)(addr - sd->sym_addr);
            sd->code = (char *)PtrAdd(code_offset, sn->rn->code);
         }
      }
      sn->rn->req_cnt++;                    // Symbol requested again
      sym_code = sn->rn->code;              // >>>>> Remember code at start of symbol <<<<<
   }

   if (sn->contained != NULL)
      sd->flags |= SDFLAG_SYMBOL_HAS_SUBSYMBOLS;

   if (sn->aliases != NULL)
      sd->flags |= SDFLAG_SYMBOL_HAS_ALIASES;

   sn->flags |= A2N_FLAGS_REQUESTED;        // Symbol requested at least once
   sn->req_cnt++;                           // Symbol requested again
   sd->sym_req_cnt = sn->req_cnt;
   api_cnt.GetSymbol_OKSymbol++;
   rc = A2N_SUCCESS;

TheEnd:
   // We're done!
   DumpReturnedSymData(rc, sd, lsnvalid);   // DEBUG stuff

   // Save last symbol data if needed
   if (G(do_fast_lookup) == 1 && lsn != NULL) {
      lsn->valid = lsnvalid;
      if (lsnvalid == 1) {
         lsn->sym_code = sym_code;
         lsn->rc = rc;
         lsn->range = in_a_range;
         lsn->opn = opn;
         lsn->lmn = lmn;
         lsn->cn = cn;
         lsn->sn = sn;
         memcpy(&lsn->sd, sd, sizeof(SYMDATA));
      }
   }

   if (rc != A2N_NO_MODULE)
      sym_offset = PtrToUint32(PtrSub(addr, sd->sym_addr));

#if defined(DEBUG)
   r = (rc <= A2N_NO_MODULE) ? gs_res_ok[rc] : gs_res_other;
   dbgapi(("<< A2nGetSymbol: %s pid=0x%04x  addr=%"_LZ64X"  rc=%d  sym='%s' off=0x%X mod='%s' src=%s line=%d\n",
           r, pid, addr, rc, sd->sym_name, sym_offset, sd->mod_name, sd->src_file, sd->src_lineno));
#endif
   return (rc);
}


//
// A2nSetGetSymbolExPid()
// **********************
//
int A2nSetGetSymbolExPid(unsigned int pid)
{
   dbgapi(("\n>> A2nSetGetSymbolExPid: pid = %d\n", pid));

   if (pid == 0 || (int)pid < 0) {
      errmsg(("*E* A2nSetGetSymbolPid: invalid PID.\n"));
      return (A2N_INVALID_ARGUMENT);
   }

   ex_pn = GetProcessNode(pid);
   if (ex_pn == NULL) {
      // Haven't seen this PID. Can't use it.
      errmsg(("*E* A2nSetGetSymbolPid: Haven't seen PID %d. Can't use it.\n", pid));
      return (A2N_INVALID_ARGUMENT);
   }

   ex_opn = GetOwningProcessNode(ex_pn);
   ex_pid = pid;

   dbgapi(("<< A2nSetGetSymbolExPid: rc = 0\n"));
   return (A2N_SUCCESS);
}


//
// A2nGetSymbolEx()
// ****************
//
// Get symbol information for a given address in a pid set in advance.
//
// * PID must have been set via A2nSetGetSymbolExPid()
// * sd is assumed not to be NULL. No checking.
// * Sets only the following fields in SD::
//   - mod_name, mod_addr, mod_length
//   - sym_name, sym_addr, sym_length
//   - flags
// * Returns same set of return codes as A2nGetSymbol().
//
int A2nGetSymbolEx(uint64_t addr,           // Address
                   SYMDATA * sd)            // Pointer to SYMDATA structure
{
   SYM_NODE * sn = NULL;
   SEC_NODE * cn = NULL;
   JM_NODE * jmn = NULL;
   LMOD_NODE * lmn = NULL;
   MOD_NODE * mn = NULL;
   int rc;


   dbgapi(("\n>> A2nGetSymbolEx: **ENTRY** addr=%"_LZ64X", pSYMDATA=%p\n", addr, sd));
   api_cnt.GetSymbol++;

   if (ex_pn == NULL) {
      // Haven't called A2nSetGetSymbolExPid() successfully.
      errmsg(("*E* A2nGetSymbolEx: Haven't called A2nSetGetSymbolExPid() successfully.\n"));
      return (A2N_ACCESS_DENIED);
   }

   // Get loaded module node where this symbol falls.
   lmn = GetLoadedModuleNode(ex_pn, addr);

   if (lmn == NULL) {
      infomsg(("*I* A2nGetSymbolEx: NoModule for (0x%04x-%"_LZ64X").\n", ex_pn->pid, addr));
      sd->mod_name = ModuleNotFound;
      sd->mod_addr = 0;
      sd->mod_length = 0;

      sd->sym_name = NoSymbolData;
      sd->sym_addr = 0;
      sd->sym_length = 0;

      api_cnt.GetSymbol_NoModule++;
      dbgapi(("<< A2nGetSymbolEx: %s pid=0x%04x  addr=%"_LZ64X"  sym='%s'  mod='%s'\n",
              gs_res_ok[A2N_NO_MODULE], ex_pid, addr, sd->sym_name, sd->mod_name));
      return (A2N_NO_MODULE);
   }

   lmn->flags |= A2N_FLAGS_REQUESTED;       // Loaded module requested at least once
   lmn->req_cnt++;                          // Loaded module requested again

   // Handle jitted methods ...
   if (lmn->type == LMOD_TYPE_JITTED_CODE) {
      jmn = lmn->jmn;

      sd->mod_addr = lmn->start_addr;
      sd->mod_length = lmn->total_len;
      if (lmn->jmn->flags & A2N_FLAGS_MMI_METHOD) {
         sd->mod_name = JittedMmiCodeModuleName;
         sd->flags |= SDFLAG_SYMBOL_MMI;
      }
      else {
         sd->mod_name = JittedCodeModuleName;
         sd->flags |= SDFLAG_SYMBOL_JITTED;
      }

      sd->sym_addr = lmn->start_addr;
      sd->sym_length = jmn->length;
      sd->sym_name = jmn->name;

      if (ex_pn->flags & A2N_FLAGS_32BIT_PROCESS)
         sd->flags |= (SDFLAG_MODULE_32BIT | SDFLAG_PROCESS_32BIT);
      else if (ex_pn->flags & A2N_FLAGS_64BIT_PROCESS)
         sd->flags |= (SDFLAG_MODULE_64BIT | SDFLAG_PROCESS_64BIT);

      jmn->flags |= A2N_FLAGS_REQUESTED;    // Method requested at least once
      jmn->req_cnt++;                       // Method requested again
      api_cnt.GetSymbol_OKJittedMethod++;
      rc = A2N_SUCCESS;
      goto TheEnd;
   }

   //
   // Handle regular (non-jitted method) symbols ...
   // sn = NULL       don't have a symbol
   // sn = -1         have a symbol but validation failed so it's no good
   // sn = non-NULL   have a symbol
   //
   sn = GetSymbolNode(lmn, addr);           // Get symbol information

   mn = lmn->mn;
   sd->mod_addr = lmn->start_addr;
   sd->mod_length = lmn->total_len;

#if defined(_LINUX)
   if (mn->flags & A2N_FLAGS_VDSO)
      sd->mod_name = ModuleVdso;
   else
#endif
      sd->mod_name = mn->name;

   if (mn->flags & A2N_FLAGS_64BIT_MODULE)
      sd->flags |= SDFLAG_MODULE_64BIT;
   else if (mn->flags & A2N_FLAGS_32BIT_MODULE)
      sd->flags |= SDFLAG_MODULE_32BIT;

   if (ex_pn->flags & A2N_FLAGS_32BIT_PROCESS)
      sd->flags |= SDFLAG_PROCESS_32BIT;
   else if (ex_pn->flags & A2N_FLAGS_64BIT_PROCESS)
      sd->flags |= SDFLAG_PROCESS_64BIT;

   mn->flags |= A2N_FLAGS_REQUESTED;        // Module requested at least once
   mn->req_cnt++;                           // Module requested again

   if (sn == NULL) {
      // No symbols *OR* symbol not found ...
      cn = GetSectionNode(lmn, addr);       // Find section ...
      if (mn->symcnt != 0) {
         //
         // Special-case the pseudo symbols ILT (Windows on IA64) or PLT (Linux/z/OS).
         // If we added one of these and there are no other symbols then
         // mn->symcnt will be non-zero (hopefully 1). So check and make
         // sure we return the correct symbol/rc.
         //
         if (OnlyFakeSymbols(mn)) {
            // This module really has NO_SYMBOLS so tell them so.
            dbgmsg(("- A2nGetSymbol: NoSymbol for (0x%04x-%"_LZ64X") in %s (*** only pseudo symbol is ILT/PLT ***)\n", ex_pid, addr, sd->mod_name));
            SetSymbolNameAddrLenth(NoSymbolData, sd, lmn, cn);
            api_cnt.GetSymbol_NoSymbols++;
            rc = A2N_NO_SYMBOLS;
         }
         else {
            // NO SYMBOL FOUND: module has symbols but there's no symbol for given address.
            SetSymbolNameAddrLenth(SymbolNotFound, sd, lmn, cn);
            api_cnt.GetSymbol_NoSymbolFound++;
            infomsg(("*I* A2nGetSymbol: NoSymbolFound for (0x%04x-%"_LZ64X") in %s\n", ex_pid, addr, sd->mod_name));
            rc = A2N_NO_SYMBOL_FOUND;
         }
      }
      else {
         // NO SYMBOLS: module has no symbols.
         SetSymbolNameAddrLenth(NoSymbolData, sd, lmn, cn);
         api_cnt.GetSymbol_NoSymbols++;
         rc = A2N_NO_SYMBOLS;
      }
      goto TheEnd;
   }

   if (sn == (SYM_NODE *)-1) {
      // There are symbols but they're invalid (ie. validation failed)
      SetSymbolNameAddrLenth(NoSymbolData, sd, lmn, cn);
      api_cnt.GetSymbol_NoSymbolsValFailed++;
      rc = A2N_NO_SYMBOLS;               // Have module name and maybe process name
      goto TheEnd;
   }

   // OK to return symbol information so fill it in ...
   if (sn->rn == NULL) {
      // Not a symbol range. Use data in the symbol node
      sd->sym_name = sn->name;
      sd->sym_addr = lmn->start_addr + sn->offset_start;
      sd->sym_length = sn->length;
   }
   else {
      // Symbol range. Use data in the range node
      // No line numbers for ranges.
      sd->flags |= SDFLAG_SYMBOL_RANGE;

      if (G(return_range_symbols) == 1)
         sd->sym_name = sn->name;
      else
         sd->sym_name = sn->rn->name;

      sd->sym_addr = lmn->start_addr + sn->rn->offset_start;
      sd->sym_length = sn->rn->length;

      sn->rn->req_cnt++;                    // Symbol requested again
   }

   sn->flags |= A2N_FLAGS_REQUESTED;        // Symbol requested at least once
   sn->req_cnt++;                           // Symbol requested again
   sd->sym_req_cnt = sn->req_cnt;
   api_cnt.GetSymbol_OKSymbol++;
   rc = A2N_SUCCESS;

TheEnd:
   // We're done!
   dbgapi(("<< A2nGetSymbolEx: %s pid=0x%04x  addr=%"_LZ64X"  rc=%d  sym='%s'  mod='%s'\n",
          (rc <= A2N_NO_MODULE) ? gs_res_ok[rc] : gs_res_other, ex_pid, addr, rc, sd->sym_name, sd->mod_name));

   return (rc);
}


//
// A2nGetLineNumber()
// ******************
//
// Get source line number and source file, if available, for a given symbol.
//
// Should only be used after A2nGetSymbol() by using the module and symbol
// pointers returned in the SYMDATA structure.
//
// Returns
//    0          no line numbers or errors. *src_file not valid.
//    non-zero:  valid line number. *src_file valid but can be NULL.
//
int A2nGetLineNumber( void * module,
                      void * symbol,
                      uint64 addr,
                      char ** src_file)
{
   LMOD_NODE * lmn = module;
   SYM_NODE * sn = symbol;
   int lineno = 0;


   dbgapi(("\n>> A2nGetLineNumber: module=%p, symbol=%p, addr=%"_LZ64X", src_file=%p\n",
            module, symbol, addr, src_file));

   if (G(lineno) == 0) {
      dbgapi(("<< A2nGetLineNumber: line numbers not collected.\n"));
      return (0);
   }
   if (lmn == NULL || sn == NULL || src_file == NULL) {
      errmsg(("*E* A2nGetLineNumber: NULL module/symbol/src_file pointer.\n"));
      return (0);
   }
   if ((lmn->flags & A2N_FLAGS_LMOD_NODE_SIGNATURE) != A2N_FLAGS_LMOD_NODE_SIGNATURE) {
      // Wrong module node signature
      dbgapi(("<< A2nGetLineNumber: wrong module node signature. lmn=0x%"_PP"\n",lmn));
      return (0);
   }
   if (lmn->type == LMOD_TYPE_ANON) {
      // No line numbers for anonymous segments
      dbgapi(("<< A2nGetLineNumber: LMN is anonymous segment - no line numbers available.\n"));
      return (0);
   }
   if ( (lmn->type == LMOD_TYPE_JITTED_CODE) ||
        ((sn->flags & A2N_FLAGS_SYM_NODE_SIGNATURE) == A2N_FLAGS_SYM_NODE_SIGNATURE) ) {
      // Jitted method and correct symbol node signature
      GetLineNumber(lmn, sn, addr, src_file, &lineno);
      dbgapi(("<< A2nGetLineNumber: line number is %d\n", lineno));
      return (lineno);
   }
   else {
      // Not jitted method and wrong symbol node signature
      errmsg(("*E* A2nGetLineNumber: not jitted method and wrong symbol node signature. sn=0x%"_PP"\n", sn));
      return (0);
   }
}


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
//    Non-NULL   list of line numbers returned. Caller *MUST* zfree() the storage.
//    NULL       no line numbers or errors
//
LINEDATA * A2nGetLineNumbersForSymbol(void * module, void * symbol)
{
   LMOD_NODE * lmn = module;
   SYM_NODE * sn = symbol;
   LINEDATA * ld;


   dbgapi(("\n>> A2nGetLineNumbersForSymbol: module=%p, symbol=%p\n", module, symbol));

   if (G(lineno) == 0) {
      dbgapi(("<< A2nGetLineNumbersForSymbol: line numbers not collected.\n"));
      return (NULL);
   }
   if (lmn == NULL || sn == NULL) {
      errmsg(("*E* A2nGetLineNumbersForSymbol: NULL module/symbol pointer.\n"));
      return (NULL);
   }
   if ((lmn->flags & A2N_FLAGS_LMOD_NODE_SIGNATURE) != A2N_FLAGS_LMOD_NODE_SIGNATURE) {
      // Wrong module node signature
      dbgapi(("<< A2nGetLineNumbersForSymbol: wrong module node signature. lmn=0x%"_PP"\n", lmn));
      return (NULL);
   }
   if (lmn->type == LMOD_TYPE_ANON) {
      // No line numbers for anonymous segments
      dbgapi(("<< A2nGetLineNumberForSymbol: LMN is anonymous segment - no line numbers available.\n"));
      return (0);
   }
   if ( (lmn->type == LMOD_TYPE_JITTED_CODE) ||
        ((sn->flags & A2N_FLAGS_SYM_NODE_SIGNATURE) == A2N_FLAGS_SYM_NODE_SIGNATURE) ) {
      // Jitted method and correct symbol node signature
      ld = GetLineNumbersForSymbol(lmn, sn);
      dbgapi(("<< A2nGetLineNumbersForSymbol: ld = %p\n", ld));
      return (ld);
   }
   else {
      // Not jitted method and wrong symbol node signature
      errmsg(("*E* A2nGetLineNumbersForSymbol: not jitted method and wrong symbol node signature. sn=0x%"_PP"\n", sn));
      return (NULL);
   }
}


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
SYMDATA * * A2nGetAllFunctionsByName(unsigned int pid, const char * name)
{
   MOD_NODE * mod_ptr;
   SYM_NODE ** symlist_ptr;
   SYM_NODE * sym_ptr;

   SYMDATA * sd;
   SYMDATA ** symbols = NULL;
   int capacity = 10;
   int count = 0;
   int mod_count;

   // Check arguments
   if (!name)
      return (NULL);

   symbols = (SYMDATA **)zmalloc(capacity * sizeof (SYMDATA *));

   // Walk through the list of modules
   for (mod_ptr = G(ModRoot); mod_ptr != NULL; mod_ptr = mod_ptr->next) {
      // Walk through the module's list of symbols
      mod_count = 0;
      HarvestSymbols(mod_ptr, mod_ptr->last_lmn);
      // Walk through array and/or list of symbols
      for (symlist_ptr = mod_ptr->symroot;
           mod_count < mod_ptr->base_symcnt && *symlist_ptr != NULL;
           symlist_ptr++) {
         for (sym_ptr = *symlist_ptr;
              mod_count < mod_ptr->base_symcnt && sym_ptr != NULL;
              sym_ptr = sym_ptr->next) {
            // Only look at functions
            mod_count++;
            if (sym_ptr->type == SYMBOL_TYPE_FUNCTION) {
               // Check if this is the symbol we are looking for
               if (!strcmp(sym_ptr->name, name)) {
                  // Add symbol to our list
                  // ##### Should check for NULL #####
                  sd = (SYMDATA *)zmalloc(sizeof(SYMDATA));
                  if (count == capacity) {
                     capacity *= 2;
                     symbols = (SYMDATA **)zrealloc(symbols, capacity * sizeof(SYMDATA *));
                  }
                  // Fill in the symbol data structure
                  // Since we're standing on the symbol A2nGetSymbol() should work!
                  A2nGetSymbol(pid, PtrToUint64(mod_ptr->last_lmn->start_addr + sym_ptr->offset_start), sd);
                  symbols[count++] = sd;
               }
            }
         }
      }
   }

   // Trim array and terminate with NULL entry
   if (count > 0) {
      capacity = count + 1;
      symbols = (SYMDATA **)zrealloc(symbols, capacity * sizeof(SYMDATA *));
      symbols[count] = NULL;
      return (symbols);
   }

   // No matching symbols found
   return (NULL);
}


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
SYMDATA * * A2nGetAllFunctions (unsigned int pid)
{
   MOD_NODE * mod_ptr;
   SYM_NODE ** symlist_ptr;
   SYM_NODE * sym_ptr;

   SYMDATA * sd;
   SYMDATA ** symbols = NULL;
   int capacity = 10;
   int count = 0;
   int mod_count;

   symbols = (SYMDATA **)zmalloc(capacity * sizeof(SYMDATA *));

   // Walk through the list of modules
   for (mod_ptr = G(ModRoot); mod_ptr != NULL; mod_ptr = mod_ptr->next) {
      // Walk through the module's list of symbols
      mod_count = 0;
      HarvestSymbols (mod_ptr, mod_ptr->last_lmn);
      // Walk through array and/or list of symbols
      for (symlist_ptr = mod_ptr->symroot;
           mod_count < mod_ptr->base_symcnt && *symlist_ptr != NULL;
           symlist_ptr++) {
         for (sym_ptr = *symlist_ptr;
              mod_count < mod_ptr->base_symcnt && sym_ptr != NULL;
              sym_ptr = sym_ptr->next) {
            // Only look at functions
            mod_count++;
            if (sym_ptr->type == SYMBOL_TYPE_FUNCTION) {
               // Add symbol to our list
               // ##### Should check for NULL #####
               sd = (SYMDATA *)zmalloc(sizeof(SYMDATA));
               if (count == capacity) {
                  capacity <<= 1;
                  symbols = zrealloc(symbols, capacity * sizeof(SYMDATA *));
               }
               // Fill in the symbol data structure
               A2nGetSymbol(pid, PtrToUint64(mod_ptr->last_lmn->start_addr + sym_ptr->offset_start), sd);
               symbols[count++] = sd;
            }
         }
      }
   }

   // Trim array and terminate with NULL entry
   if (count > 0) {
      capacity = count + 1;
      symbols = zrealloc(symbols, capacity * sizeof(SYMDATA *));
      symbols[count] = NULL;
      return (symbols);
   }

   // No matching symbols found
   return (NULL);
}


//
// A2nFreeSymbols()
// ****************
//
// Free array of symbol returned by A2nGetAllFunctionsByName() and
// A2nGetAllFunction().
//
void A2nFreeSymbols(SYMDATA ** sym_array)
{
    SYMDATA ** ptr;

    if (!sym_array)
       return;

    // Free symbol blocks
    for (ptr = sym_array; ptr != NULL && *ptr != NULL; ptr++) {
       zfree(*ptr);
       *ptr = NULL;
    }
    // Free array
    zfree(sym_array);

    return;
}


//
// A2nGetAddress()
// ***************
//
// Get offset of a given symbol within a module.
//
// Assumes symbols are already harvested (run in immediate_gather mode)
//
// Returns
//    0:        Address, and optionally code pointer, returned.
//    non-zero: Error
//
int A2nGetAddress(unsigned int pid,         // Process id
                  char * symbol_name,       // Symbol name
                  char * module_name,       // Module name
                  uint64 * addr,            // Pointer to where address returned (or NULL)
                  void ** code)             // Pointer to where code pointer is returned (or NULL)
{
   PID_NODE * pn;
   LMOD_NODE * lmn;
   MOD_NODE * mn;
   SYM_NODE * sn;
   uint hv;


   dbgapi(("\n>> A2nGetAddress: pid=0x%04x, sym='%s', mod='%s', paddr=%p, pcode=%p\n",
            pid, symbol_name, module_name, addr, code));

   // Quit if results pointers are NULL
   if (addr == NULL  &&  code == NULL) {
      errmsg(("*E* A2nGetAddress: addr and code pointers are both NULL.\n"));
      return (0);                           // Nothing to do
   }
   if (symbol_name == NULL || *symbol_name == '\0' ||
       module_name == NULL || *module_name == '\0') {
      errmsg(("*E* A2nGetAddress: symbol and/or module name is/are NULL.\n"));
      return (0);                           // Nothing to do
   }

   // Find the module node or quit
   hv = hash_module_name(module_name);
   mn = GetModuleNodeFromCache(module_name, hv, (uint)-1, (uint)-1);
   if (mn == NULL) {
      infomsg(("*I* A2nGetAddress: module '%s' not found.\n", module_name));
      return (A2N_GETMODULE_ERROR);         // Can't find module node.
   }

   // Find the symbol node or quit
   sn = GetSymbolNodeFromName(mn, symbol_name);
   if (sn == NULL) {
      infomsg(("*I* A2nGetAddress: symbol '%s' not found in module '%s'.\n", symbol_name, module_name));
      return (A2N_GETSYMBOL_ERROR);         // Can't find symbol node.
   }

   // Find process node
   pn = GetProcessNode(pid);
   if (pn == NULL) {                        // First add.  Need PID node
      errmsg(("*E* A2nGetAddress: unable to find process node for 0x%04x\n", pid));
      return (A2N_GETPROCESS_ERROR);
   }

   //
   // Find last loaded module which points to the module node containing
   // the symbol. I assume that's the loaded module I need in order to get
   // the module load address so I can return an address.
   //
   lmn = GetLoadedModuleNodeForModule(pn, mn);
   if (lmn == NULL) {
      errmsg(("*E* A2nGetAddress: no module loaded to hold symbol '%s'\n", symbol_name));
      return (A2N_NO_MODULE_LOADED);
   }

   // Return requested information
   if (addr != NULL)
      *addr = lmn->start_addr + sn->offset_start;
   if (code != NULL)
      *code = sn->code;

   dbgapi(("<< A2nGetAddress: OK\n"));
   return (0);
}


//
// A2nCollapseSymbolRange()
// ************************
//
//
// Collapses all symbols between symbol_start and symbol_end, included, into a
// single symbol name. The new symbol name can be given or, if missing, it is
// assumed to be symbol_start.
// Symbols are collapsed in all modules currently loaded and the information
// is queued and applied if and when a module with the requested name is
// loaded at a later time.
//
// Returns
//    0:        Success.
//    non-zero: Error
//
void A2nCollapseSymbolRange(char * module_filename, // Module name
                            char * symbol_start,    // Starting symbol name
                            char * symbol_end,      // Ending symbol name
                            char * symbol_new)      // New name for resulting symbol (or NULL)
{
   dbgapi(("\n>> A2nCollapseSymbolRange: module='%s'  sym_start='%s'  sym_end='%s'  range_name='%s'\n",
            module_filename, symbol_start, symbol_end, symbol_new));

   if (module_filename == NULL || module_filename[0] == 0 || module_filename[0] == ' ' ||
       symbol_start == NULL    ||  symbol_start[0] == 0   ||  symbol_start[0] == ' ' ||
       symbol_end == NULL      ||  symbol_end[0] == 0     ||  symbol_end[0] == ' ') {
      errmsg(("*E* A2nCollapseSymbolRange: one or more arguments are NULL, NULL strings, or blank.\n"));
      return;
   }

   if (symbol_new == NULL)
      symbol_new = symbol_start;

   CollapseSymbolRange(module_filename, symbol_start, symbol_end, symbol_new);
   dbgapi(("<< A2nCollapseSymbolRange: done\n"));
   return;
}


//
// A2nCloneProcess()
// *****************
//
// Add process which inherits and shares the address space (and therefore the
// loaded module/symbol information) with the process that cloned it.  A cloned
// process is similar to a thread.
// Cloned processes can themselves clone/fork other processes but they can't
// own loaded modules.
//
// Returns
//    0:        Process cloned successfully
//    non-zero: Error
//
int A2nCloneProcess(unsigned int cloner_pid,     // Cloner process id
                    unsigned int cloned_pid)     // Cloned process id
{
#if defined(_LINUX) || defined(_ZOS)
   PID_NODE * pn;


   api_cnt.CloneProcess++;
   dbgapi(("\n>> A2nCloneProcess: cloner=0x%04x, cloned=0x%04x\n", cloner_pid, cloned_pid));

   pn = CloneProcessNode(cloner_pid, cloned_pid);
   if (pn == NULL) {
      errmsg(("*E* A2nCloneProcess: unable to clone process 0x%04x from 0x%04x\n", cloner_pid, cloned_pid));
      return (A2N_CLONEPROCESS_ERROR);
   }

   api_cnt.CloneProcess_OK++;
   dbgapi(("<< A2nCloneProcess: OK\n"));
#endif
   return (0);
}


//
// A2nForkProcess()
// ****************
//
// Add process which inherits but *DOES NOT* share the address space
// (and therefore the loaded module/symbol information) with the process
// forked it.
// After creation, the forked/forker processes become independant: changes
// affecting one do not affect the other.
// Forked processes can themselves fork/clone other processes.
//
// Returns
//    0:        Process added successfully
//    non-zero: Error
//
int A2nForkProcess(unsigned int forker_pid,      // Forker process id
                   unsigned int forked_pid)      // Forked process id
{
#if defined(_LINUX) || defined(_ZOS) || defined(_AIX)
   PID_NODE * pn;


   api_cnt.ForkProcess++;
   dbgapi(("\n>> A2nForkProcess: forker=0x%04x, forked=0x%04x\n", forker_pid, forked_pid));

   pn = ForkProcessNode(forker_pid, forked_pid);
   if (pn == NULL) {
      errmsg(("*E* A2nForkProcess: unable to fork process 0x%04x from 0x%04x\n", forker_pid, forked_pid));
      return (A2N_FORKPROCESS_ERROR);
   }

   api_cnt.ForkProcess_OK++;
   dbgapi(("<< A2nForkProcess: OK\n"));
#endif
   return (0);
}


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
int A2nCreateProcess(unsigned int pid)
{
#if defined(_WINDOWS)
   PID_NODE * pn;


   api_cnt.CreateProcess++;
   dbgapi(("\n>> A2nCreateProcess: pid=0x%04x\n", pid));

   pn = CreateNewProcess(pid);
   if (pn == NULL) {
      errmsg(("*E* A2nCreateProcess: unable to create process 0x%04x\n", pid));
      return (A2N_CREATEPROCESS_ERROR);
   }

   api_cnt.CreateProcess_OK++;
   dbgapi(("<< A2nCreateProcess: OK\n"));
#endif
   return (0);
}


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
// * The type argument can be one of the PROCESS_* flags defined below.
//
// Returns
//    0:        Process type set successfully
//    non-zero: Error
//
int A2nSetProcessType(unsigned int pid, int type)
{
   PID_NODE * pn;


   dbgapi(("\n>> A2nSetProcessType: pid=0x%04x type=0x%X\n", pid, type));

   if (type != PROCESS_TYPE_32BIT || type != PROCESS_TYPE_64BIT) {
      errmsg(("*E* A2nSetProcessType: invalid process type - 0x%X\n", type));
      return (A2N_INVALID_PROCESS_TYPE);
   }

   pn = GetProcessNode(pid);                // Find process node
   if (pn == NULL) {
      pn = CreateProcessNode(pid);          // If there isn't one then create new one
      if (pn == NULL) {
         errmsg(("*E* A2nSetProcessType: unable to create process node for 0x%04x\n", pid));
         return (A2N_CREATEPROCESS_ERROR);
      }
   }

   //
   // Set the new type.
   //
   if (type == PROCESS_TYPE_32BIT)
      pn->flags |= A2N_FLAGS_32BIT_PROCESS;
   else
      pn->flags |= A2N_FLAGS_64BIT_PROCESS;

   dbgapi(("<< A2nSetProcessType: OK\n"));
   return (0);

}


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
int A2nGetProcessType(unsigned int pid)
{
   PID_NODE * pn;
   int rc;


   dbgapi(("\n>> A2nGetProcessType: pid=0x%04x\n", pid));

   pn = GetProcessNode(pid);
   if (pn == NULL) {
      errmsg(("*E* A2nGetProcessType: unable to find process node for 0x%04x\n", pid));
      rc = A2N_GETPROCESS_ERROR;
   }
   else {
      if (pn->flags & A2N_FLAGS_32BIT_PROCESS)
         rc = PROCESS_TYPE_32BIT;
      else if (pn->flags & A2N_FLAGS_64BIT_PROCESS)
         rc = PROCESS_TYPE_64BIT;
      else
         rc = 0;
   }

   dbgapi(("<< A2nGetProcessType: type=0x%X\n", rc));
   return (rc);

}


//
// A2nSetProcessName()
// *******************
//
// Associate a name with a process.
// If process doesn't exist, a new process node is created.
//
// Returns
//    0:        Process name set successfully
//    non-zero: Error
//
int A2nSetProcessName(unsigned int pid,     // Process id
                      char * name)          // Process name
{
   PID_NODE * pn;


   api_cnt.SetProcessName++;
   dbgapi(("\n>> A2nSetProcessName: pid=0x%04x, nm='%s'\n", pid, name));

   if (name == NULL || *name == '\0') {     // Name must be non-NULL
      errmsg(("*E* A2nSetProcessName: invalid process name - NULL\n"));
      return (A2N_INVALID_NAME);
   }

   pn = GetProcessNode(pid);                // Find process node
   if (pn == NULL) {
      pn = CreateProcessNode(pid);          // If there isn't one then create new one
      if (pn == NULL) {
         errmsg(("*E* A2nSetProcessName: unable to create process node for 0x%04x-%s\n", pid, name));
         return (A2N_CREATEPROCESS_ERROR);
      }
   }

   //
   // Set the new name.
   // Only free the name iff the name isn't "Unknown" 'cause we didn't
   // allocate memory for it - it's just an instance variable.
   //
   if (pn->name != G(UnknownName))
      zfree(pn->name);                      // Not "Unknown" so free memory

   pn->name = zstrdup(name);                // Dup name
   if (pn->name == NULL) {
      errmsg(("*E* A2nSetProcessName: unable to allocate name. pid=0x%x, name=%s\n", pid, name));
      return (A2N_MALLOC_ERROR);            // strdup() failed
   }

   api_cnt.SetProcessName_OK++;
   dbgapi(("<< A2nSetProcessName: OK\n"));
   return (0);
}


//
// A2nGetProcessName()
// *******************
//
// Return name (if known) of given process.
// - Space is allocated via zmalloc() for the name. As such, the caller
//   must zfree() the string.
//
// Returns
//    NULL:     Invalid pid or process name not set/known
//    non-NULL: Process name
//
char * A2nGetProcessName(unsigned int pid)
{
   PID_NODE * pn;
   char * name = NULL;


   dbgapi(("\n>> A2nGetProcessName: pid=0x%04x\n", pid));

   pn = GetProcessNode(pid);                // Find process node
   if (pn == NULL) {
      errmsg(("*E* A2nGetProcessName: unable to find process node for 0x%04x\n", pid));
      return (NULL);
   }

   if (pn->name != NULL)
      name = zstrdup(pn->name);

   dbgapi(("<< A2nGetProcessName: %s\n", name));
   return (name);
}


//
// A2nGetParentPid()
// *****************
//
// Return pid of parent process.
// The "parent process" is the process that forked/cloned the child process.
//
// Returns
//    -1:       Error or no parent
//    Other:    Parent pid
//
unsigned int A2nGetParentPid(unsigned int child_pid)  // Child pid
{
   PID_NODE * pn, * ppn;


   dbgapi(("\n>> A2nGetParentPid: child_pid=0x%04x\n", child_pid));

   pn = GetProcessNode(child_pid);          // Find child's process node
   if (pn == NULL) {
      errmsg(("*E* A2nGetParentPid: unable to find process node for 0x%04x\n", child_pid));
      return ((uint)-1);
   }

   ppn = GetParentProcessNode(pn);          // Find parent's process node
   if (ppn == NULL) {
      errmsg(("*E* A2nGetParentPid: unable to find parent process node for 0x%04x\n", child_pid));
      return ((uint)-1);
   }

   dbgapi(("<< A2nGetParentPid: ppid=0x%04x\n", ppn->pid));
   return (ppn->pid);
}


//
// A2nGetOwningPid()
// *****************
//
// Return pid of owning process.
// The "owning process" is the process that own the addess space.
// - A forked process is an owning process
// - For cloned processes the owning process is the forked process
//   that started the clone tree.
//
// Returns
//    -1:       Error or no owning pid
//    Other:    Parent pid
//
unsigned int A2nGetOwningPid(unsigned int child_pid)  // Child pid
{
   PID_NODE * pn, * opn;


   dbgapi(("\n>> A2nGetOwningPid: child_pid=0x%04x\n", child_pid));

   pn = GetProcessNode(child_pid);          // Find child's process node
   if (pn == NULL) {
      errmsg(("*E* A2nGetOwningPid: unable to find process node for 0x%04x\n", child_pid));
      return ((uint)-1);
   }

   opn = GetOwningProcessNode(pn);          // Find owning process node
   if (opn == NULL) {
      errmsg(("*E* A2nGetOwningPid: unable to find owning process node for 0x%04x\n", child_pid));
      return ((uint)-1);
   }

   dbgapi(("<< A2nGetOwningPid: opid=0x%04x\n", opn->pid));
   return (opn->pid);
}


//
// A2nSetSystemPid()
// *****************
//
// Set the system's (kernel's) process id
// This information is necessary in order to know which modules are "owned"
// by the system since the system can run in the context of any other process.
// Thus symbols that are not found in a process' loaded modules could be
// found in modules loaded by the system.
//
void A2nSetSystemPid(unsigned int pid)
{
   PID_NODE * pn;

   dbgapi(("\n>> A2nSetSystemPid: pid=0x%04x\n", pid));

   pn = GetProcessNode(pid);                // Find process node for new system pid
   if (pn == NULL)
      pn = CreateProcessNode(pid);          // If there isn't one then create it

   // Set the new system pid if we found/created a process node for it
   if (pn == NULL) {
      errmsg(("*E* A2nSetSystemPid: unable to create process node for 0x%04x. System Pid not changed.\n", pid));
   }
   else {
      G(SystemPidNode) = pn;
      G(SystemPid) = pid;
      G(SystemPidSet) = 1;

      // If this is the process hasn't been named yet then make up a name
      if (pn->name == NULL) {
         pn->name = zstrdup("SystemProcess");
         dbgmsg(("- A2nSetSystemPid: pid 0x%04x not already named. Setting name to '%s'\n", pn->pid, pn->name));
      }

#if defined(_AIX)
      InitSymlibSymbols();
#endif
   }

   dbgapi(("<< A2nSetSystemPid: pn=%p  (OK if pn != NULL)\n", pn));
   return;
}


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
int A2nCreateMsiFile(char * filename)       // File to which state is saved
{
   int rc;


   dbgapi(("\n>> A2nCreateMsiFile: fn='%s'\n", filename));

   // Gotta have a filename
   if (filename == NULL  ||  *filename == '\0') {
      errmsg(("*E* A2nCreateMsiFile: invalid filename - NULL\n"));
      return (A2N_INVALID_FILENAME);
   }

   // Do the work
   rc = CreateMsiFile(filename);
   if (rc != 0) {
      errmsg(("*E* A2nCreateMsiFile: error creating MSI file\n"));
   }
   else {
      dbgapi(("<< A2nCreateMsiFile: done\n"));
   }

   return (rc);
}


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
//   - A2nGetParentPid()
//   - A2nGetOwningPid()
//   - A2nSetSystemPid()
//   - A2nSetErrorMessageMode()
//   - A2nSetDebugMode()
//   - A2nDumpProcesses()
//   - A2nDumpModules()
//   - A2nSaveModules()
//   - A2nRestoreModules()
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
//   - A2nSetSymbolValidationMode()
//
// Returns
//    0:        State restored successfully
//    non-zero: Error
//
int A2nLoadMsiFile(char * filename)         // File from which state is restored
{
   int rc;


   dbgapi(("\n>> A2nLoadMsiFile: fn='%s'\n", filename));

   // Not allowed if dynamic changes have been made
   if (G(changes_made)) {
      errmsg(("*E* A2nLoadMsiFile: request rejected - changes have already been made.\n"));
      return (A2N_REQUEST_REJECTED);
   }

   // Gotta have a filename
   if (filename == NULL  ||  *filename == '\0') {
      errmsg(("*E* A2nLoadMsiFile: invalid filename - NULL\n"));
      return (A2N_INVALID_FILENAME);
   }

   // Do the work
   rc = ReadMsiFile(filename);
   if (rc != 0) {
      errmsg(("*E* A2nLoadMsiFile: error reading MSI file\n"));
   }
   else {
      G(using_static_module_info) = 1;
      dbgapi(("<< A2nLoadMsiFile: done\n"));
   }

   return (rc);
}


//
// A2nGetMsiFileInformation()
// **************************
//
// Returns information about the last MSI file loaded.
//
int A2nGetMsiFileInformation(int * wordsize,     // Word size of machine where MSI file written
                             int * bigendian,    // 1 if machine where MSI file written was big endian
                             int * os)           // OS running when MSI file written
{
   dbgapi(("\n>> A2nGetMsiFileInformation: ws=%p, be=%p, os=%p\n", wordsize, bigendian, os));

   if (wordsize != NULL)
      *wordsize = G(msi_file_wordsize);

   if (bigendian != NULL)
      *bigendian = G(msi_file_bigendian);

   if (os != NULL)
      *os = G(msi_file_os);

   dbgapi(("<< A2nLoadMsiFile: done\n"));
   return (0);
}


//
// A2nSetKernelImageName()
// ***********************
//
// Gives the fully qualified name of the non-stripped kernel image file.
//
int A2nSetKernelImageName(char * name)
{
   int rc = 0;

   dbgapi(("\n>> A2nSetKernelImageName: nm='%s'\n", name));

#if defined(_LINUX)
   if (api_lock.SetKernelImageName) {
      infomsg(("*I* A2nSetKernelImageName: Default set via ENV variable. Changes not allowed.\n"));
      return (0);
   }
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetKernelImageName: request ignored - using restored module info\n"));
      return (0);
   }
   rc = verify_kernel_image_name(name);
#endif

   dbgapi(("<< A2nSetKernelImageName: rc = %d\n", rc));
   return (rc);
}


//
// A2nSetKernelMapName()
// *********************
//
// Gives the fully qualified name of the kernel map file.
//
int A2nSetKernelMapName(char * name)
{
   int rc = 0;

   dbgapi(("\n>> A2nSetKernelMapName: nm='%s'\n", name));

#if defined(_LINUX)
   if (api_lock.SetKernelMapName) {
      infomsg(("*I* A2nSetKernelMapName: Default set via ENV variable. Changes not allowed.\n"));
      return (0);
   }
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetKernelMapName: request ignored - using restored module info\n"));
      return (0);
   }
   rc = verify_kernel_map_name(name);
#endif

   dbgapi(("<< A2nSetKernelMapName: rc = %d\n", rc));
   return (rc);
}


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
int A2nSetKallsymsFileLocation(char * name, int64_t offset, int64_t size)
{
   int rc = 0;

   dbgapi(("\n>> A2nSetKallsymsFileLocation: fn='%s' offset=%"_L64d" size=%"_L64d"\n", name, offset, size));

#if defined(_LINUX)
   if (api_lock.SetKallsymsLocation) {
      infomsg(("*I* A2nSetKallsymsFileLocation: Default set via ENV variable. Changes not allowed.\n"));
      return (0);
   }
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetKallsymsFileLocation: request ignored - using restored module info\n"));
      return (0);
   }
   rc = verify_kallsyms_file_info(name, offset, size);
#endif

   dbgapi(("<< A2nSetKallsymsFileLocation: rc = %d\n", rc));
   return (rc);
}


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
int A2nSetModulesFileLocation(char * name, int64_t offset, int64_t size)
{
   int rc = 0;

   dbgapi(("\n>> A2nSetModulesFileLocation: fn='%s' offset=%"_L64d" size=%"_L64d"\n", name, offset, size));

#if defined(_LINUX)
   if (api_lock.SetModulesLocation) {
      infomsg(("*I* A2nSetModulesFileLocation: Default set via ENV variable. Changes not allowed.\n"));
      return (0);
   }
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetModulesFileLocation: request ignored - using restored module info\n"));
      return (0);
   }
   rc = verify_modules_file_info(name, offset, size);
#endif

   dbgapi(("<< A2nSetModulesFileLocation: rc = %d\n", rc));
   return (rc);
}


//
// A2nSetSymbolSearchPath()
// ************************
//
// Provide alternate paths from which to attempt to harvest symbols
// should the actual executable/module not contain symbols.
// "path" is a string containing paths/directories.
// The rules for using the search path is as follows:
// 1) Not used if symbols are obtained from the on-disk executable.
// 2) Try all known file types for the platform *IN THE SAME DIRECTORY
//    AS EXECUTABLE* and attempt to get symbols from any one of them.
// 3) If step 2 does not yield symbols then repeat the process but this
//    time searching for map files in each path listed in the
//    Symbol Search Path.
//
// Paths, if more than one, are separated by a platform-specific separator
// character as follows:
// * AIX and Linux:  ':'  (colon)      (ex: "~:/work:/usr/local/symbols")
// * Windows:        ';'  (semicolon)  (ex: "c:\winnt\system32\symbols;f:\symbols")
//
int A2nSetSymbolSearchPath(const char * searchpath) // List of "paths"
{
   char * p;


   dbgapi(("\n>> A2nSetSymbolSearchPath: path='%s'\n", searchpath));

   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetSymbolSearchPath: request ignored - using restored module info\n"));
      return (0);
   }

   if (searchpath == NULL  ||  *searchpath == '\0') {  // Make sure search path string is not NULL
      warnmsg(("*W* A2nSetSymbolSearchPath: search path deleted (NULL)\n"));
      G(SymbolSearchPath) = NULL;
      return (0);
   }

   // OK, remember search path
   p = zstrdup(searchpath);
   if (p == NULL) {
      errmsg(("*E* A2nSetSymbolSearchPath: unable to allocate memory for search path. Not changed.\n"));
      return (A2N_MALLOC_ERROR);
   }
   else {
      G(SymbolSearchPath) = p;
   }

   dbgapi(("<< A2nSetSymbolSearchPath: OK\n"));
   return (0);
}


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
//   first_symbol_in_range.
//
// Returns
//    0:        Success.
//    non-zero: Error
//
int A2nSetRangeDescFilename(char * fn)
{
   int rc;

   dbgapi(("\n>> A2nSetRangeDescFilename: fn='%s'\n", fn));

   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetRangeDescFilename: request ignored - using restored module info\n"));
      return (0);
   }
   rc = check_if_good_filename(fn);
   if (rc == 0) {
      rc = ReadRangeDescriptorFile(fn);     // Read file and table it up
      if (rc != 0) {
         errmsg(("*E* A2nSetRangeDescFilename: error(s) processing %s.\n", fn));
      }
   }

   dbgapi(("<< A2nSetRangeDescFilename: rc=%d\n", rc));
   return (rc);
}


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
int A2nSetJvmmapFilename(char * fn)
{
   int rc = 0;

#if defined(_WINDOWS)
   dbgapi(("\n>> A2nSetJvmmapFilename: fn='%s'\n", fn));

   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetJvmmapFilename: request ignored - using restored module info\n"));
      return (0);
   }
   rc = check_if_good_filename(fn);
   if (rc == 0) {
      zfree(G(jvmmap_fn));
      G(jvmmap_fn) = zstrdup(fn);
   }

   dbgapi(("<< A2nSetJvmmapFilename: rc = %d\n", rc));
#endif

   return (rc);
}


//
// A2nAlloc()
// **********
//
// Allocates memory. Use A2nFree() to deallocate.
//
void * A2nAlloc(int size)
{
   if (size <= 0)
      return (NULL);
   else
      return (zmalloc(size));
}


//
// A2nFree()
// *********
//
// Deallocates memory allocated via A2nAlloc().
//
void A2nFree(void * mem)
{
   if (mem != NULL)
      zfree(mem);
   return;
}


//
// A2nGenerateHashValue()
// **********************
//
// Generate a hash value given a string.
//
unsigned int A2nGenerateHashValue(char * str)
{
   return (hash_module_name(str));
}


//
// A2nSetCollapseMMIRange()
// ************************
//
// Indicate that the caller wants to automatically collapse MMI symbols,
// in IBM JVMs, into a single symbol name. Actual collapsing takes place
// when (and if) symbols for the JVM are harvested.
// If range_name is NULL then a default name is assigned to the range.
//
// Returns
//    0:        Success.
//    non-zero: Error
//
void A2nSetCollapseMMIRange(char * range_name)
{
   dbgapi(("\n>> A2nSetCollapseMMIRange: name='%s'\n", range_name));

   if (api_lock.SetCollapseMMIRange) {
      infomsg(("*I* A2nSetCollapseMMIRange: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetCollapseMMIRange: request ignored - using restored module info\n"));
      return;
   }

   G(collapse_ibm_mmi) = 1;                 // Remember to do it

   if (api_lock.SetMMIRangeName) {
      infomsg(("*I* A2nSetCollapseMMIRange: Name set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (range_name == NULL  ||  *range_name == '\0') {  // Make sure name is not NULL
      infomsg(("*I* A2nSetCollapseMMIRange: NULL name. Using default.\n"));
      G(collapsed_mmi_range_name) = DEFAULT_MMI_RANGE_NAME;
   }
   else {
      G(collapsed_mmi_range_name) = zstrdup(range_name);
      if (G(collapsed_mmi_range_name) == NULL) {
         errmsg(("*E* A2nSetCollapseMMIRange: unable to allocate memory for range name. Quitting.\n"));
         exit (A2N_MALLOC_ERROR);
      }
   }

   dbgapi(("<< A2nSetCollapseMMIRange: OK\n"));
   return;
}


//
// A2nSetSymbolQualityMode()
// *************************
//
// Tells A2N the desired symbol quality the caller is willing to accept.
// The caller may be willing to accept any symbol, exports only or only
// debug symbols contained in the executable.
//
// *** A2N default is harvest/return debug symbols.
//
void  A2nSetSymbolQualityMode(int mode)
{
   dbgapi(("\n>> A2nSetSymbolQualityMode: mode=%d (%d=ANY/%d=GOOD)\n",
           mode, MODE_ANY_SYMBOL, MODE_GOOD_SYMBOLS));

   if (api_lock.SymbolQuality) {
      infomsg(("*I* A2nSetSymbolQualityMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode != MODE_ANY_SYMBOL && mode != MODE_GOOD_SYMBOLS) {
      warnmsg(("*W* A2nSetSymbolQualityMode: Invalid mode. Mode not changed.\n"));
   }
   else {
      G(symbol_quality) = mode;
   }

   dbgapi(("<< A2nSetSymbolQualityMode: OK\n"));
   return;
}


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
// *** A2N default is to always validate kernel symbols.
//
void A2nSetValidateKernelMode(int mode)
{
   dbgapi(("\n>> A2nSetValidateKernelMode: mode=%d  forwarded to A2nSetSymbolValidationMode()\n", mode));
   A2nSetSymbolValidationMode(mode);
   dbgapi(("\n<< A2nSetValidateKernelMode: done\n"));
   return;
}


//
// A2nStopOnKernelValidationError()
// ********************************
//
// Tells A2N to stop processing, and quit, if kernel symbol validation fails.
// This API is supported only on Linux.
//
// *** A2N default is to continue processing if validation fails.
//
void A2nStopOnKernelValidationError(void)
{
   dbgapi(("\n>> A2nStopOnKernelValidationError: start\n"));
   G(quit_on_validation_error) = MODE_ON;
   dbgapi(("\n<< A2nStopOnKernelValidationError: done\n"));
   return;
}


//
// A2nSetSymbolValidationMode()
// ****************************
//
// Tells A2N whether or not to perform symbol validatation (ie. making symbols
// are in fact correct for the module/image).
//
// Symbol validation varies by plaform:
// Linux:
// ------
//   Since ELF modules don't have no means of uniquely identifying a module
//   A2N has no way to validate symbols *IF* they *DON'T* come from the module
//   itself (ie. a map file, for example).
//   A2N *DOES* validate kernel symbols to make sure they match the currently
//   running kernel. Kernel validation is accomplished by making sure every
//   KSYM in present (with the same name and at the same address) in the harvested
//   symbols.
//
// Windows:
// --------
//   PE images, DBG, PDB and MAP files all contain a 32-bit timestamp generated
//   by the linker when the image is linked.  Symbol files for the image also
//   contain the same timestamp, regardless of when they symbol files are
//   generated.
//   A2N only gathers symbols if the timestamp in the symbol files (and that
//   can be the image itself) matches the timestamp of the loaded module.
//
// *** A2N default is to always validate symbols.
//
void A2nSetSymbolValidationMode(int mode)
{
   dbgapi(("\n>> A2nSetSymbolValidationMode: mode=%d\n", mode));
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetSymbolValidationMode: request ignored - using restored module info\n"));
      return;
   }

   if (api_lock.ValidateSymbolsMode) {
      infomsg(("*I* A2nSetSymbolValidationMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode != MODE_ON  &&  mode != MODE_OFF) {
      warnmsg(("*W* A2nSetSymbolValidationMode: Invalid mode. Mode not changed.\n"));
   }
   else {
      G(validate_symbols) = mode;
   }

   dbgapi(("\n<< A2nSetSymbolValidationMode: OK\n"));
   return;
}


//
// A2nSetErrorMessageMode()
// ************************
//
// Tells A2N whether or not to write messages (to a2n.err) on errors.
//
// *** A2N default is to always write messages on errors.
//
void A2nSetErrorMessageMode(int mode)
{
   dbgapi(("\n>< A2nSetErrorMessageMode: mode=%d (%d=NONE/%d=ERROR/%d=WARN/%d=INFO/%d=ALL)\n",
           mode, MSG_NONE, MSG_ERROR, MSG_WARN, MSG_INFO, MSG_ALL));


   if (api_lock.ErrorMessageMode) {
      infomsg(("*I* A2nSetErrorMessageMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode < MSG_NONE) {
      warnmsg(("*W* A2nSetErrorMessageMode: Invalid mode. Mode set to MSG_NONE.\n"));
      G(display_error_messages) = MSG_NONE;
   }
   else if (mode > MSG_ALL) {
      warnmsg(("*W* A2nSetErrorMessageMode: Invalid mode. Mode set to MSG_ALL.\n"));
      G(display_error_messages) = MSG_ALL;
   }
   else {
      G(display_error_messages) = mode;
   }

   return;
}


//
// A2nSetNoSymbolFoundAction()
// ***************************
//
// Tells A2N what symbol name to return whenever the A2N_NO_SYMBOL_FOUND
// return code is returned on A2nGetSymbol().
// Possible actions are:
// - Return NULL string for symbol name
// - Return section name string for symbol name
// - Return "NoSymbolFound" string for symbol name
//
// *** A2N default is to return "NoSymbolFound" string.
//
void A2nSetNoSymbolFoundAction(int action)
{
   dbgapi(("\n> A2nSetNoSymbolFoundAction: action=%d (%d=NOSYM/%d=NSF/%d=SECTION)\n",
           action, NSF_ACTION_NOSYM, NSF_ACTION_NSF, NSF_ACTION_SECTION));

   if (api_lock.NoSymbolFoundAction) {
      infomsg(("*I* A2nSetNoSymbolFoundAction: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (action != NSF_ACTION_NOSYM && action != NSF_ACTION_NSF && action != NSF_ACTION_SECTION) {
      warnmsg(("*W* A2nSetNoSymbolFoundAction: Invalid action. Action not changed.\n"));
   }
   else {
      G(nsf_action) = action;
   }

   dbgapi(("\n< A2nSetNoSymbolFoundAction: new_action=%d\n",G(nsf_action)));
   return;
}


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
void A2nSetSymbolGatherMode(int mode)
{
   dbgapi(("\n>> A2nSetSymbolGatherMode: mode=%d (%d=LAZY/%d=IMMEDIATE)\n",
           mode, MODE_LAZY, MODE_IMMEDIATE));

   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetSymbolGatherMode: request ignored - using restored module info\n"));
      return;
   }

   if (api_lock.SymbolGatherMode) {
      infomsg(("*I* A2nSetSymbolGatherMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode != MODE_LAZY  &&  mode != MODE_IMMEDIATE) {
      warnmsg(("*W* A2nSetSymbolGatherMode: Invalid mode. Mode not changed.\n"));
   }
   else {
      G(immediate_gather) = mode;
   }

   dbgapi(("<< A2nSetSymbolGatherMode: OK\n"));
   return;
}


//
// A2nSetCodeGatherMode()
// **********************
//
// Tells A2N whether or not to harvest actual code from executables
// along with symbols (if present).
// Option ignored if symbols are not gathered from an executable.
//
// *** A2N default is not to gather code.
//
void A2nSetCodeGatherMode(int mode)
{
   dbgapi(("\n>> A2nSetCodeGatherMode: mode=%d (%d=ON/%d=OFF)\n", mode, MODE_ON, MODE_OFF));
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetCodeGatherMode: request ignored - using restored module info\n"));
      return;
   }

   if (api_lock.GatherCodeMode) {
      infomsg(("*I* A2nSetCodeGatherMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode != MODE_ON  &&  mode != MODE_OFF) {
      warnmsg(("*W* A2nSetCodeGatherMode: Invalid mode. Mode not changed.\n"));
   }
   else {
      G(gather_code) = mode;
   }

   dbgapi(("<< A2nSetCodeGatherMode: OK\n"));
   return;
}


//
// A2nSetDemangleCppNamesMode()
// ****************************
//
// Tells A2N to demangle C++ names (if possible) when gathering symbols.
//
// *** A2N default is not to demangle names.
//
void A2nSetDemangleCppNamesMode(int mode)
{
   dbgapi(("\n>> A2nSetDemangleCppNamesMode: mode=%d (%d=ON/%d=OFF)\n", mode, MODE_ON, MODE_OFF));
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetDemangleCppNamesMode: request ignored - using restored module info\n"));
      return;
   }

   if (api_lock.DemangleMode) {
      infomsg(("*I* A2nSetDemangleCppNamesMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode == MODE_OFF) {
      G(demangle_cpp_names) = 0;
   }
   else if (mode == MODE_ON) {
#if !defined(_WINDOWS)
      G(demangle_cpp_names) = 1;
#else
      if (pUnDecorateSymbolName == NULL) {
         warnmsg(("*W* DbgHelp.dll does not export UnDecorateSymbolName(). Mode forced OFF.\n"));
         G(demangle_cpp_names) = 0;
      }
      else {
         G(demangle_cpp_names) = 1;
      }
#endif
   }
   else {
      warnmsg(("*W* A2nSetDemangleCppNamesMode: Invalid mode. Mode not changed.\n"));
   }

   dbgapi(("<< A2nSetDemangleCppNamesMode: OK\n"));
   return;
}


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
void A2nSetReturnRangeSymbolsMode(int mode)
{
   PID_NODE * pn;


   dbgapi(("\n>> A2nSetReturnRangeSymbolsMode: mode=%d (%d=RANGE/%d=RANGE.SYMBOL)\n", mode, MODE_RANGE, MODE_RANGE_SYMBOL));

   if (api_lock.ReturnRangeSymbolsMode) {
      infomsg(("*I* A2nSetReturnRangeSymbolsMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode != MODE_RANGE  &&  mode != MODE_RANGE_SYMBOL) {
      warnmsg(("*W* A2nSetReturnRangeSymbolsMode: Invalid mode. Mode not changed.\n"));
   }
   else {
      G(return_range_symbols) = mode;
      //
      // Invalidate all LastSymbol nodes (if any) whose last symbol
      // returned was inside a range.
      // ##### Could be smarter and only do the right module and range #####
      //
      pn = G(PidRoot);
      while (pn) {
         if (pn->lastsym != NULL && pn->lastsym->range == 1) {
            pn->lastsym->valid = 0;
         }
         pn = pn->next;
      }
   }

   dbgapi(("<< A2nSetReturnRangeSymbolsMode: OK\n"));
   return;
}


//
// A2nSetReturnLineNumbersMode()
// *****************************
//
// Tells A2N to return source line numbers, if available, along with symbol info.
//
// *** A2N default is to not return line numbers.
//
void A2nSetReturnLineNumbersMode(int mode)
{
   dbgapi(("\n>> A2nSetReturnLineNumbersMode: mode=%d (%d=ON/%d=OFF)\n", mode, MODE_ON, MODE_OFF));
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetReturnLineNumbersMode: request ignored - using restored module info\n"));
      return;
   }

   if (api_lock.ReturnLineNumbersMode) {
      infomsg(("*I* A2nSetReturnLineNumbersMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode == MODE_OFF) {
      G(lineno) = 0;
   }
   else if (mode == MODE_ON) {
#if !defined(_WINDOWS)
      G(lineno) = 1;
#else
      if (pSymEnumLines == NULL && pSymEnumSourceLines == NULL) {
         warnmsg(("*W* A2nSetReturnLineNumbersMode: DbgHelp.dll does not support SymEnumLines/SymEnumSourceLines(). Mode forced OFF.\n"));
         G(lineno) = 0;
      }
      else {
         G(lineno) = 1;
      }
#endif
   }
   else {
      warnmsg(("*W* A2nSetReturnLineNumbersMode: Invalid mode. Mode not changed.\n"));
   }

   dbgapi(("<< A2nSetReturnLineNumbersMode: OK\n"));
   return;
}


//
// A2nSetRenameDuplicateSymbolsMode()
// **********************************
//
// Tells A2N whether or not to give unique names to duplicate symbols.
//
// *** A2N default is to not rename dupliate symbols.
//
void A2nSetRenameDuplicateSymbolsMode(int mode)
{
   dbgapi(("\n>> A2nSetRenameDuplicateSymbolsMode: mode=%d (%d=ON/%d=OFF)\n", mode, MODE_ON, MODE_OFF));
   if (G(using_static_module_info)) {       // Ignore if using restored module information
      infomsg(("*I* A2nSetRenameDuplicateSymbolsMode: request ignored - using restored module info\n"));
      return;
   }

   if (api_lock.RenameDuplicateSymbolsMode) {
      infomsg(("*I* A2nSetRenameDuplicateSymbolsMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode == MODE_ON) {
      if (dsnp == NULL) {
         dsnp = (SYM_NODE **)zmalloc(G(snp_size));
      }
      if (dsnp) {
         G(rename_duplicate_symbols) = MODE_ON;
      }
      else {
         errmsg(("*E* A2N: Unable to malloc %d bytes for DSNP array. Request ignored.\n", G(snp_size)));
         G(rename_duplicate_symbols) = MODE_OFF;
      }
   }
   else if (mode == MODE_OFF) {
      G(rename_duplicate_symbols) = MODE_OFF;
   }
   else {
      warnmsg(("*W* A2nSetRenameDuplicateSymbolsMode: Invalid mode. Mode not changed.\n"));
      return;
   }

   dbgapi(("<< A2nSetRenameDuplicateSymbolsMode: OK\n"));
   return;
}


//
// A2nSetDebugMode()
// *****************
//
// Sets the A2N debugging mode.
// Setting the mode to any of the "on" modes forces ErrorMessageMode on.
//
// *** A2N default is to turn off debugging mode.
//
void A2nSetDebugMode(int mode)
{
   dbgapi(("\n>< A2nSetDebugMode: mode=0x%x\n", mode));

   if (api_lock.DebugMode) {
      infomsg(("*I* A2nSetDebugMode: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   if (mode != DEBUG_MODE_NONE  &&  mode != DEBUG_MODE_API &&
       mode != DEBUG_MODE_HARVESTER  &&  mode != DEBUG_MODE_ALL) {
      warnmsg(("*W* A2nSetDebugMode: Invalid mode. Mode set to DEBUG_MODE_ALL.\n"));
      G(debug) = DEBUG_MODE_ALL;
   }
   else {
      G(debug) = mode;
   }

   if (G(debug) != DEBUG_MODE_NONE)
      G(display_error_messages) = MSG_ALL;  // Any "on" mode forces messages

   if (G(debug) == DEBUG_MODE_ALL) {
      // Full debug forces module and process dumps on exit
      G(module_dump_on_exit) = MODE_ON;
      G(process_dump_on_exit) = MODE_ON;
   }

   return;
}


//
// A2nSetNoModuleFoundDumpThreshold()
// **********************************
//
// Sets maximum number of times A2nGetSymbol() returns A2N_NO_MODULE
// (ie. no module found) before forcing a Process/Module information dump.
// If the threshold is reached, a Process/Module dump is forced when
// liba2n terminates.
// Specifying -1 turns off the feature.
//
void A2nSetNoModuleFoundDumpThreshold(int threshold)
{
   dbgapi(("\n>< A2nSetNoModuleFoundDumpThreshold: threshold=%d\n", threshold));

   if (api_lock.NoModuleFountThreshold) {
      infomsg(("*I* A2nSetNoModuleFoundDumpThreshold: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   G(nmf_threshold) = threshold;
   return;
}


//
// A2nSetNoSymbolFoundDumpThreshold()
// **********************************
//
// Sets maximum number of times A2nGetSymbol() returns A2N_NO_SYMBOL FOUND
// before forcing a Process/Module information dump.
// If the threshold is reached, a Process/Module dump is forced when
// liba2n terminates.
// Specifying -1 turns off the feature.
//
void A2nSetNoSymbolFoundDumpThreshold(int threshold)
{
   dbgapi(("\n>< A2nSetNoSymbolFoundDumpThreshold: threshold=%d\n", threshold));

   if (api_lock.NoSymbolFountThreshold) {
      infomsg(("*I* A2nSetNoSymbolFoundDumpThreshold: Default set via ENV variable. Changes not allowed.\n"));
      return;
   }

   G(nsf_threshold) = threshold;
   return;
}


//
// A2nSetMultiThreadedMode()
// *************************
//
// Causes A2N to be as thread-safe as it can.
//
void A2nSetMultiThreadedMode(int mode)
{
   dbgapi(("\n>< A2nSetMultiThreadedMode: mode=0x%x\n", mode));

   if (mode == MODE_OFF) {
      G(mt_mode) = 0;
      G(st_mode) = 1;
      G(do_fast_lookup) = 1;
   }
   else if (mode == MODE_ON) {
      G(mt_mode) = 1;
      G(st_mode) = 0;
      G(do_fast_lookup) = 0;
   }
   else {
      warnmsg(("*W* A2nSetMultiThreadedMode: Invalid mode. Mode not changed.\n"));
   }

   return;
}


//
// A2nRcToString()
// ***************
//
char * A2nRcToString(int rc)
{
   switch (rc) {
      case -1:                               return ("NO_ACCESS_TO_FILE");
      case -2:                               return ("FILE_TYPE_NOT_SUPPORTED");
      case -3:                               return ("ERROR_ACCESSING_FILE");
      case -4:                               return ("INTERNAL_ERROR");
#if defined(_LINUX)
      case -5:                               return ("EXE_FORMAT_NOT_SUPPORTED");
#else
      case -5:                               return ("NO_SYMBOLS_FOUND");
      case -6:                               return ("DBGHELP_ERROR");
#endif
      case A2N_SUCCESS:                      return ("A2N_SUCCESS");
      case A2N_NO_SYMBOL_FOUND:              return ("A2N_NO_SYMBOL_FOUND");
      case A2N_NO_SYMBOLS:                   return ("A2N_NO_SYMBOLS");
      case A2N_NO_MODULE:                    return ("A2N_NO_MODULE");
      case A2N_FLAKY_SYMBOL:                 return ("A2N_FLAKY_SYMBOL");
      case A2N_CREATEPROCESS_ERROR:          return ("A2N_CREATEPROCESS_ERROR");
      case A2N_ADDMODULE_ERROR:              return ("A2N_ADDMODULE_ERROR");
      case A2N_ADDSECTION_ERROR:             return ("A2N_ADDSECTION_ERROR");
      case A2N_GETPROCESS_ERROR:             return ("A2N_GETPROCESS_ERROR");
      case A2N_GETMODULE_ERROR:              return ("A2N_GETMODULE_ERROR");
      case A2N_GETSYMBOL_ERROR:              return ("A2N_GETSYMBOL_ERROR");
      case A2N_MALLOC_ERROR:                 return ("A2N_MALLOC_ERROR");
      case A2N_NULL_SYMDATA_ERROR:           return ("A2N_NULL_SYMDATA_ERROR");
      case A2N_NOT_SUPPORTED:                return ("A2N_NOT_SUPPORTED");
      case A2N_INVALID_TYPES:                return ("A2N_INVALID_TYPES");
      case A2N_INVALID_PATH:                 return ("A2N_INVALID_PATH");
      case A2N_INVALID_USAGE:                return ("A2N_INVALID_USAGE");
      case A2N_INVALID_HANDLE:               return ("A2N_INVALID_HANDLE");
      case A2N_INVALID_FILENAME:             return ("A2N_INVALID_FILENAME");
      case A2N_INVALID_PARENT:               return ("A2N_INVALID_PARENT");
      case A2N_NO_MODULE_LOADED:             return ("A2N_NO_MODULE_LOADED");
      case A2N_ADDSYMBOL_ERROR:              return ("A2N_ADDSYMBOL_ERROR");
      case A2N_ADD_JITTEDMETHOD_ERROR:       return ("A2N_ADD_JITTEDMETHOD_ERROR");
      case A2N_INVALID_NAME:                 return ("A2N_INVALID_NAME");
      case A2N_TRYING_TO_NAME_CLONE:         return ("A2N_TRYING_TO_NAME_CLONE");
      case A2N_CLONEPROCESS_ERROR:           return ("A2N_CLONEPROCESS_ERROR");
      case A2N_FORKPROCESS_ERROR:            return ("A2N_FORKPROCESS_ERROR");
      case A2N_FILE_NOT_FOUND_OR_CANT_READ:  return ("A2N_FILE_NOT_FOUND_OR_CANT_READ");
      case A2N_REQUEST_REJECTED:             return ("A2N_REQUEST_REJECTED");
      case A2N_FILEWRITE_ERROR:              return ("A2N_FILEWRITE_ERROR");
      case A2N_CREATEMSIFILE_ERROR:          return ("A2N_CREATEMSIFILE_ERROR");
      case A2N_READMSIFILE_ERROR:            return ("A2N_READMSIFILE_ERROR");
      case A2N_INVALID_MODULE_TYPE:          return ("A2N_INVALID_MODULE_TYPE");
      case A2N_INVALID_MODULE_LENGTH:        return ("A2N_INVALID_MODULE_LENGTH");
      case A2N_NULL_MODULE_NAME:             return ("A2N_NULL_MODULE_NAME");
      case A2N_INVALID_ARGUMENT:             return ("A2N_INVALID_ARGUMENT");
      case A2N_COLLAPSE_RANGE_ERROR:         return ("A2N_COLLAPSE_RANGE_ERROR");
      case A2N_FILEOPEN_ERROR:               return ("A2N_FILEOPEN_ERROR");
      case A2N_FILEREAD_ERROR:               return ("A2N_FILEREAD_ERROR");
      case A2N_INVALID_RANGE_FILE:           return ("A2N_INVALID_RANGE_FILE");
      case A2N_INVALID_METHOD_LENGTH:        return ("A2N_INVALID_METHOD_LENGTH");
      case A2N_NULL_METHOD_NAME:             return ("A2N_NULL_METHOD_NAME");
      case A2N_FILE_NOT_RIGHT:               return ("A2N_FILE_NOT_RIGHT");
      case A2N_INTERNAL_NON_FATAL_ERROR:     return ("A2N_INTERNAL_NON_FATAL_ERROR");
      case A2N_INTERNAL_FATAL_ERROR:         return ("A2N_INTERNAL_FATAL_ERROR");
      case A2N_INVALID_PROCESS_TYPE:         return ("A2N_INVALID_PROCESS_TYPE");
      default:                               return ("***UNKNOWN A2N RETURN CODE***");
   }
}
