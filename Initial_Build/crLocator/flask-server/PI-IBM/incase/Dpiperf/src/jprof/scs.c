/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2010
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#define _GNU_SOURCE
#include "jprof.h"
#include "tree.h"
#include "scs.h"
#include "pu_scs.hpp"
#include "hash.h"

#if defined(_WINDOWS)
   #include <psapi.h>
#elif defined(_LINUX)
   #include <stdlib.h>
   #include <errno.h>
   #include <execinfo.h>
   #include <ucontext.h>
   #include <unwind.h>
   #include <sys/mman.h>
#elif defined(_AIX)
   #include <errno.h>
   #include <pmapi.h>
#endif

// @@@@@ WE NEED TO CLEARLY DEFINE WHAT IS/ISN'T _LEXTERN
// @@@@@ AND MAYBE GROUP IT TOGETHER. OR MAYBE NOT.

#ifndef _LEXTERN
//
// Internal prototypes
// *******************
//

   #if defined(_LINUX) || defined(_AIX)
static int GetLastError(void);
   #endif

   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)

static void scs_allocate_memory(void);

static void scs_free_memory(void);

static void scs_a2n_get_loaded_modules(void);

static int scs_handle_sample(void *env, scs_callstack_data *callstack_data);

   #endif
#endif //_LEXTERN




// Split UINT64 into high/low 32-bits
#define HIGH32(ll)    (UINT32)((ll) >> 32)
#define LOW32(ll)     (UINT32)(ll)

// Number of times no callstack is returned before we don't request anymore
#define NO_CALLSTACK_GIVEUP_THRESHOLD   5

// Max number of times to display repetitive messages in the log-msg file
#define REPETITIVE_MESSAGE_THRESHOLD    5


//
// Globals
// *******
//


#if defined(_WINDOWS)

   #define MANUAL_RESET_EVENT        TRUE
   #define AUTO_RESET_EVENT          FALSE
   #define INITIALLY_SIGNALED        TRUE
   #define INITIALLY_NOT_SIGNALED    FALSE

   #define A2N_NAME               "a2n.dll"
   #define LookupFunctionAddress   GetProcAddress
#endif

#if defined(_LINUX)
extern int errno;

   #ifndef DWORD
      #define DWORD UINT32
   #endif

   #define CloseHandle(x) sem_destroy(&x)
   #define PRIORITY_INCREASE 5

   #define A2N_NAME               "liba2n.so"
   #define LookupFunctionAddress   dlsym


#ifndef _LEXTERN
//
// ge.scs_a2n                   Do symbol resolution at FLUSH time
//                              - Use SCS_A2N JProf option
// gv_scs_a2n_otf               Do symbol resolution on-the-fly as the tree
//                              is being built.
//                              - Set environment variable SCS_A2N_AT_FLUSH=1
//                                to cause symbol resolution to happen at
//                                flush time, not as the tree is being built.
int gv_scs_a2n_otf = 1;

//
// Controlled by environment variable SCS_LOG_SAMPLES
// - If set (to anything) debug-ish message will be written to log-msg
// - If not set debug-ish message will not be written to log-msg
//
int gv_scs_log_samples = 0;                 // Default is to NOT write messages

//
// Controlled by environment variable SCS_USERMODE_ONLY
// - If set (to anything) only the user-mode stacks will be walked into the tree.
// - If not set user-mode and kernel-mode stacks will be walked into the tree.
//
int gv_scs_usermode_only = 0;               // Default is to walk user-mode and kernel stacks

//
// Controlled by environment variable SCS_HARVEST_DELAYED
// - If set (to anything) symbol harvesting (for a module) will be delayed
//   until symbols are needed from the module. The sampler thread will be
//   blocked, and appear as busy, the whole time symbols are being harvested.
// - If not set then symbol harvesting will be done at JProf load time, for
//   all modules loaded on the process, and for the kernel and kernel modules.
int gv_scs_harvest_on_load = 1;             // Default is harvest symbols at JProf load time

//
// Controlled by environment variable SCS_USE_BACKTRACE
// - If set (to anything) use the backtrace() libc call to get the
//   user-mode callstack. It should work most of the time but backtrace()
//   will segfault on invalid addresses.
//   One nice thing about useing backtrace() is that it knows how to unwind
//   the stack even if no frame pointers are used, because it can read the
//   DWARF CFI (Call Frame Information) for the ELF file.
// - If not set then we walk the user-mode callstack ourselves.
//   Drawback is that the stacks won't look pretty if no frame pointers
//   are used.
// - Defaults:
//   * x86 is to walk the stack ourselves (don't want to take the chance
//     that the unwinder segfaults).
//   * x86_64 is to let the unwinder walk the stack (the unwind information
//     should be in the executable). If the unwinder can't walk the stack
//     then we'll give it a try.
#if defined(_X86)
int gv_scs_use_backtrace = 0;               // Default is to walk stack ourselves
int gv_stop_frame = 0;
#else
int gv_scs_use_backtrace = 1;               // Default is to let the unwinder walk stack
int gv_stop_frame = 2;
#endif

//
// Controlled by environment variable SCS_STACK_WALK_MESSAGES
// - If set (to anything) displays messages (to log-msg) as the stack is
//   being walked, one per frame, plus error/completion messages. This
//   option writes -A LOT- of data to log-msg. It's really a debugging option.
int gv_scs_stack_walk_messages = 0;         // Default is to not write messages

#define sw_msg(...)                       \
   do {                                   \
      if (gv_scs_stack_walk_messages) {   \
         msg_log(__VA_ARGS__);            \
      }                                   \
   } while (0)

#endif // _LEXTERN

// Kernel compiled with frame pointers => can walk kernel stack
int gv_kernel_frame_pointers = 0;

struct stack_frame {
   struct stack_frame * next;        // Next EBP
   void * ret_addr;                  // Return address
};
typedef struct stack_frame stack_frame_t;

#if defined(_X86)
   // EBP must be 4-byte aligned
   #define STACK_ALIGNMENT_MASK     0x00000003

   // EIP and EBP
   #define IP_REG                   REG_EIP
   #define BP_REG                   REG_EBP

   #define IP_REG_NAME              "EIP"
#elif defined(_X86_64)
   // RBP must be 16-byte aligned
   #define STACK_ALIGNMENT_MASK     0x0000000F

   // RIP and RBP
   #define IP_REG                   REG_RIP
   #define BP_REG                   REG_RBP

   #define IP_REG_NAME              "RIP"
#endif

#endif // _LINUX

#if defined(_WINDOWS) || defined(_LINUX)
   #define IDLE_PROCESS       0
#endif //_WINDOWS || _LINUX

#if defined(_AIX)
   #define CloseHandle(x)  sem_destroy(&x)
#endif


//
// scs_a2n_init()
// **************
// ** EXTERNAL **
//
// Load A2N and get it ready for service.
//
void scs_a2n_init(void)
{
   char output_path[MAX_PATH];
   char * p;

   if (ge.a2n_initialized)
      return;
   else
      ge.a2n_initialized = 1;

   scs_debug_msg(("*D* scs_a2n_init(): starting\n"));

#if defined(_WINDOWS)
   // Force A2N output file separation, in case we're running multiple JVMs
   _putenv("A2N_SEPARATE_OUTPUT=1");

   // If JProf's output files are not going in the current directory, tell
   // A2N where they're going so A2N can write its output there too.
   if (strcmp(ge.fnm, "log") != 0) {
      sprintf(output_path, "A2N_OUTPUT_PATH=%s", ge.fnm);
      p = strrchr(output_path, '\\');
      if (p) {
         *p = 0;
         _putenv(output_path);
      }
      else {
         msg_log("**WARNING** scs_a2n_init(): looks like fnm= is missing the prefix\n");
      }
   }

   // Load a2n.dll
   ge.a2n_handle = LoadLibrary(A2N_NAME);
   if (ge.a2n_handle == NULL) {
      msg_se_log("**ERROR** Unable to load %s. rc = %d\n", A2N_NAME, GetLastError());
      return;
   }

#elif defined(_LINUX)

   // Load liba2n.so
   ge.a2n_handle = dlopen(A2N_NAME, RTLD_NOW);
   if (ge.a2n_handle == NULL) {
      msg_se_log("**ERROR** Unable to load %s: %s\n", A2N_NAME, dlerror());
      return;
   }
#endif

#if defined(_WINDOWS) || defined(_LINUX)
   // Resolve the a2n.dll APIs we need
   ge.pfnA2nGetSymbol = (PFN_A2nGetSymbol)LookupFunctionAddress(ge.a2n_handle, "A2nGetSymbol");
   if (ge.pfnA2nGetSymbol == NULL) {
      msg_log("**ERROR** Unable find A2nGetSymbol() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nGenerateHashValue = (PFN_A2nGenerateHashValue)LookupFunctionAddress(ge.a2n_handle, "A2nGenerateHashValue");
   if (ge.pfnA2nGenerateHashValue == NULL) {
      msg_log("**ERROR** Unable find A2nGenerateHashValue() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nAddModule = (PFN_A2nAddModule)LookupFunctionAddress(ge.a2n_handle, "A2nAddModule");
   if (ge.pfnA2nAddModule == NULL) {
      msg_log("**ERROR** Unable find A2nAddModule() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nAddJittedMethod = (PFN_A2nAddJittedMethod)LookupFunctionAddress(ge.a2n_handle, "A2nAddJittedMethod");
   if (ge.pfnA2nAddJittedMethod == NULL) {
      msg_log("**ERROR** Unable find A2nAddJittedMethod() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nSetErrorMessageMode = (PFN_A2nSetErrorMessageMode)LookupFunctionAddress(ge.a2n_handle, "A2nSetErrorMessageMode");
   if (ge.pfnA2nSetErrorMessageMode == NULL) {
      msg_log("**ERROR** Unable find A2nSetErrorMessageMode() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nSetSymbolQualityMode = (PFN_A2nSetSymbolQualityMode)LookupFunctionAddress(ge.a2n_handle, "A2nSetSymbolQualityMode");
   if (ge.pfnA2nSetSymbolQualityMode == NULL) {
      msg_log("**ERROR** Unable find A2nSetSymbolQualityMode() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nSetDemangleCppNamesMode = (PFN_A2nSetDemangleCppNamesMode)LookupFunctionAddress(ge.a2n_handle, "A2nSetDemangleCppNamesMode");
   if (ge.pfnA2nSetDemangleCppNamesMode == NULL) {
      msg_log("**ERROR** Unable find A2nSetDemangleCppNamesMode() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nSetSymbolGatherMode = (PFN_A2nSetSymbolGatherMode)LookupFunctionAddress(ge.a2n_handle, "A2nSetSymbolGatherMode");
   if (ge.pfnA2nSetSymbolGatherMode == NULL) {
      msg_log("**ERROR** Unable find A2nSetSymbolGatherMode() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nSetCodeGatherMode = (PFN_A2nSetCodeGatherMode)LookupFunctionAddress(ge.a2n_handle, "A2nSetCodeGatherMode");
   if (ge.pfnA2nSetCodeGatherMode == NULL) {
      msg_log("**ERROR** Unable find A2nSetCodeGatherMode() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nSetSystemPid = (PFN_A2nSetSystemPid)LookupFunctionAddress(ge.a2n_handle, "A2nSetSystemPid");
   if (ge.pfnA2nSetSystemPid == NULL) {
      msg_log("**ERROR** Unable find A2nSetSystemPid() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nCreateProcess = (PFN_A2nCreateProcess)LookupFunctionAddress(ge.a2n_handle, "A2nCreateProcess");
   if (ge.pfnA2nCreateProcess == NULL) {
      msg_log("**ERROR** Unable find A2nCreateProcess() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nSetProcessName = (PFN_A2nSetProcessName)LookupFunctionAddress(ge.a2n_handle, "A2nSetProcessName");
   if (ge.pfnA2nSetProcessName == NULL) {
      msg_log("**ERROR** Unable find A2nSetProcessName() in %s.\n", A2N_NAME);
      return;
   }
   ge.pfnA2nLoadMap = (PFN_A2nLoadMap)LookupFunctionAddress(ge.a2n_handle, "A2nLoadMap");
   if (ge.pfnA2nLoadMap == NULL) {
      ErrVMsgLog("**INFO** Unable find A2nLoadMap() in %s.\n", A2N_NAME);
   }

   // These are for "faster" on-the-fly resolution. NBD if we can't find them
   ge.pfnA2nSetGetSymbolExPid = (PFN_A2nSetGetSymbolExPid)LookupFunctionAddress(ge.a2n_handle, "A2nSetGetSymbolExPid");
   if (ge.pfnA2nSetGetSymbolExPid == NULL) {
      ErrVMsgLog("**INFO** Unable find A2nSetGetSymbolExPid() in %s.\n", A2N_NAME);
   }
   ge.pfnA2nGetSymbolEx = (PFN_A2nGetSymbolEx)LookupFunctionAddress(ge.a2n_handle, "A2nGetSymbolEx");
   if (ge.pfnA2nGetSymbolEx == NULL) {
      ErrVMsgLog("**INFO** Unable find A2nGetSymbolEx() in %s.\n", A2N_NAME);
   }
   ge.pfnA2nSetMultiThreadedMode = (PFN_A2nSetMultiThreadedMode)LookupFunctionAddress(ge.a2n_handle, "A2nSetMultiThreadedMode");
   if (ge.pfnA2nSetMultiThreadedMode == NULL) {
      ErrVMsgLog("**INFO** Unable find A2nSetMultiThreadedMode() in %s.\n", A2N_NAME);
   }
   ge.pfnA2nSetReturnLineNumbersMode = (PFN_A2nSetReturnLineNumbersMode)LookupFunctionAddress(ge.a2n_handle, "A2nSetReturnLineNumbersMode");
   if (ge.pfnA2nSetReturnLineNumbersMode == NULL) {
      ErrVMsgLog("**INFO** Unable find A2nSetReturnLineNumbersMode() in %s.\n", A2N_NAME);
   }

   // Got all the *required* entry points. We can use a2n.
   ge.cant_use_a2n = 0;

   // Initialize it
   ge.pfnA2nSetErrorMessageMode(MSG_ALL);             // A2nShowAllMessages()
   ge.pfnA2nSetSymbolQualityMode(MODE_ANY_SYMBOL);
   ge.pfnA2nSetDemangleCppNamesMode(MODE_ON);         // A2nDemangleCppNames()
   ge.pfnA2nSetSymbolGatherMode(MODE_LAZY);           // A2nLazySymbolGather()
   ge.pfnA2nSetCodeGatherMode(MODE_OFF);              // A2nDontGatherCode()
   ge.pfnA2nSetSystemPid(IDLE_PROCESS);
   ge.pfnA2nCreateProcess(ge.pid);
   ge.pfnA2nSetProcessName(ge.pid, "java");
   if (ge.pfnA2nSetReturnLineNumbersMode)
      ge.pfnA2nSetReturnLineNumbersMode(MODE_OFF);
#endif

   scs_debug_msg(("*D* scs_a2n_init(): done\n"));
   return;
}


//
// GetLastError
// ************
//
#if defined(_LINUX) || defined(_AIX)
static int GetLastError(void)
{
   return (errno);
}
#endif


#ifndef _LEXTERN
//
// resolve_symbol_name_with_backtrace_symbols()
// ********************************************
//
// This is a last-ditch attempt to resolve symbols for stripped executables.
// backtrace_symbols() uses the .dynamic section to get symbols so
// it may give us a symbol, which is better than no symbols.
//
// Strings coming back from backtrace_symbols() look something like this:
// 1) "/lib/libc.so.6 [0xb7890600]"               - return: [0xb7890600]-@-/lib/libc.so.6
// 2) "/lib/libc.so.6() [0xb7890600]"             - return: [0xb7890600]-@-/lib/libc.so.6
// 3) "/lib/libc.so.6(+0xdcf39) [0xf770ef39]      - return: [0xf770ef39]-@-/lib/libc.so.6
// 4) "/lib/libc.so.6(usleep+0x3d) [0xb77f6fdd]"  - return: !!usleep-@-/lib/libc.so.6
// 5) "[0xffffe424]"                              - return: [0xffffe424]
//
void resolve_symbol_name_with_backtrace_symbols(void * addr, char * buffer)
{
#if defined(_LINUX)
   char * * symbols;
   char * symbol;
   char * s;
   char * p;

   symbols = backtrace_symbols(&addr, 1);
   if (symbols == NULL) {
      sprintf(buffer, "[%p]", addr);
      return;
   }

   symbol = symbols[0];
   if (*symbol == '[') {
      // 5) "[0xffffe424]"  - return: [0xffffe424]
      sprintf(buffer, "[%p]", addr);
      return;
   }

   s = strchr(symbol, '(');
   if (s) {
      *s = '\0';
      s++;
      if (*s == ')' || *s == '+') {
         // 2) "/lib/libc.so.6() [0xb7890600]"  - return: [0xb7890600]-@-/lib/libc.so.6
         // 3) "/lib/libc.so.6(+0xdcf39) [0xf770ef39]      - return: [0xf770ef39]-@-/lib/libc.so.6
         sprintf(buffer, "[%p]-@-%s", addr, symbol);
      }
      else {
         // 4) "/lib/libc.so.6(usleep+0x3d) [0xb77f6fdd]"  - return: !!usleep-@-/lib/libc.so.6
         p = strchr(s, '+');
         *p = '\0';
         sprintf(buffer, "!!%s-@-%s", s, symbol);
      }
   }
   else {
      // 1) "/lib/libc.so.6 [0xb7890600]"  - return: [0xb7890600]-@-/lib/libc.so.6
      p = strrchr(symbol, ' ');
      *p = '\0';
      sprintf(buffer, "[%p]-@-%s", addr, symbol);
   }
#endif
   return;
}


//
// resolve_symbol_name()
// *********************
//
void resolve_symbol_name(uint64_t addr, char * buffer)
{
#if defined(_LINUX)
   SYMDATA sd;
   int rc;


   if (ge.cant_use_a2n || !ge.scs_a2n) {
      // Give it one more shot with backtrace_symbols()
      resolve_symbol_name_with_backtrace_symbols(Uint64ToPtr(addr), buffer);
      return;
   }

   if (ge.pfnA2nGetSymbolEx)
      rc = ge.pfnA2nGetSymbolEx(addr, &sd);
   else
      rc = ge.pfnA2nGetSymbol(ge.pid, addr, &sd);

   if (rc == A2N_SUCCESS) {
      // Have everything
      sprintf(buffer, "%s-@-%s", sd.sym_name, sd.mod_name);
      return;
   }

   if (rc == A2N_NO_SYMBOLS) {
      // No symbols. Give it one more shot with backtrace_symbols()
      resolve_symbol_name_with_backtrace_symbols(Uint64ToPtr(addr), buffer);
   }
   else if (rc == A2N_NO_SYMBOL_FOUND || rc == A2N_NO_MODULE) {
      // A2N_NO_SYMBOL_FOUND or A2N_NO_MODULE
      sprintf(buffer, "%s[%p]-@-%s", sd.sym_name, Uint64ToPtr(addr), sd.mod_name);
   }
   else {
      // Error
      msg_log("**ERROR** A2nGetSymbol() rc=%d for 0x%"_L64X"\n", rc, addr);
      sprintf(buffer, "A2N_ERROR [%p]", Uint64ToPtr(addr));
   }
#endif
   return;
}

#endif // _LEXTERN


//
// scs_check_options()
// *******************
// ** EXTERNAL **
//
// Called from ParseJProfOptions() to check validity of given SCS options.
// Returns true if good to go, false otherwise
//
int scs_check_options(struct scs_options * opt)
{
#ifndef _LEXTERN
   char * p;
#endif

   // No callstack sampling options
   if (!opt->any)
      return (JVMTI_ERROR_NONE);           // Success

   //
   // Don't allow SCS with CALLTREE or CALLTRACE
   //
   if (gv->calltree || gv->calltrace) {
      msg_se_log("**ERROR** SCS is not allowed with CALLTREE or CALLTRACE. Quitting.\n\n");
      return (JVMTI_ERROR_INTERNAL);
   }

#ifndef _LEXTERN
   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
   //
   // Don't allow SCS with OBJECTINFO
   //
   if (ge.scs && ge.ObjectInfo) {
      msg_se_log("**ERROR** SCS is not allowed with OBJECTINFO. Quitting.\n\n");
      return (JVMTI_ERROR_INTERNAL);
   }

   // @@@@@ ALL THESE TRUTH TABLES ARE USELESS NOW!
   // @@@@@ THEY SHOULD BE UPDATED OR WE SHOULD JUST GET RID OF THEM AND LET EACH
   // @@@@@ OF US FIGURE OUT THE RULES EVERY TIME WE LOOK AT THE CODE.

   // SCS _SELF _FRAMES= _RETRY_NOSTACK _RATE= _EVENT= _COUNT= _TRACE _A2N
   //  0    X      X            X         X       X       X       X     X
   //  0    X      X            X         X       X       X       X     1
   //  0    X      X            X         X       X       X       1     X
   //  0    X      X            1         X       X       X       X     X
   //  0    X      X            X         X       X       1       X     X
   //  0    X      X            X         X       1       X       X     X
   //  0    X      X            X         1       X       X       X     X
   //  0    X      1            X         X       X       X       X     X
   //  0    1      X            X         X       X       X       X     X
   //
   if ((ge.scs == 0) && opt->any_other) {
      msg_se_log("\n**ERROR** SCS_* option(s) require SCS option. Quitting.\n\n");
      return (JVMTI_ERROR_INTERNAL);
   }

   // SCS _SELF _FRAMES= _RETRY_NOSTACK _RATE= _EVENT= _COUNT= _TRACE _A2N
   //  1    X      X            X         1       0       1       X     X
   //  1    X      X            X         1       1       0       X     X
   //  1    X      X            X         1       1       1       X     X
   //
   if (opt->rate && (opt->event || opt->count)) {
      msg_se_log("**ERROR** SCS_RATE not valid with SCS_EVENT and/or SCS_COUNT. Quitting.\n\n");
      return (JVMTI_ERROR_INTERNAL);
   }

   // SCS _SELF _FRAMES= _RETRY_NOSTACK _RATE= _EVENT= _COUNT= _TRACE _A2N
   //  1    X      X            X         X       0       1       X     X
   //
   if (!opt->event && opt->count) {
      msg_se_log("**ERROR** SCS_EVENT is required with SCS_COUNT. Quitting.\n\n");
      return (JVMTI_ERROR_INTERNAL);
   }

   // SCS _SELF _FRAMES= _RETRY_NOSTACK _RATE= _EVENT= _COUNT= _TRACE _A2N
   //  1    X      X            X         X       1       0       X     X
   //
   if (opt->event && !opt->count) {
      msg_se_log("**ERROR** SCS_COUNT is required with SCS_EVENT. Quitting.\n\n");
      return (JVMTI_ERROR_INTERNAL);
   }

   // SCS _SELF _FRAMES= _RETRY_NOSTACK _RATE= _EVENT= _COUNT= _TRACE _A2N
   //  1    X      X            X         X       X       X       X     0
   //
   // Only combinations left are all valid. Make sure we have valid
   // values, no coreq/prereq problems, and set the defaults as needed.
   //
   // SCS _SELF _FRAMES= _RETRY_NOSTACK _RATE= _EVENT= _COUNT= _TRACE _A2N
   //  1    0      0            0         0       0       0       0     0  :: Time-based sampling
   //
   //  1    X      X            X         X       0       0       0     0
   //
   //  1    X      X            X         X       0       0       0     1
   //
   //  1    X      X            X         X       0       0       1     0
   //
   //  1    X      X            X         0       1       1       0     0  :: Event-based sampling
   //
   //  1    X      X            X         0       1       1       0     1
   //
   //  1    X      X            X         0       1       1       1     0
   //

   //
   // Check for valid values where specified. Take defaults otherwise.
   //
   if (opt->frames) {
      if (opt->frames_val <= 0) {
         msg_se_log("**ERROR** SCS_FRAMES %d is not valid. Must be > 0. Quitting.\n\n", opt->frames_val);
         return (JVMTI_ERROR_INTERNAL);
      }
      ge.maxframes = opt->frames_val;
   }
   else {
      ge.maxframes = DefaultMaxFrames;
   }

   if (opt->rate) {
      if (opt->rate_val <= 0 || opt->rate_val > 1000) {
         msg_se_log("**ERROR** SCS_RATE %d is not valid. Must be > 0 and <= 1000. Quitting.\n\n", opt->rate_val);
         return (JVMTI_ERROR_INTERNAL);
      }
      gv->scs_rate = opt->rate_val;
      #if defined(_AIX)
      strcpy(gv->pm_event_name, "PM_CYC");
      #endif
   }
   else {
#if defined(_LINUX)
      gv->scs_rate = pi_get_default_scs_rate();
#else
      gv->scs_rate = DefaultSampleRate;
#endif
   }

   if (opt->count) {
      if (opt->count_val <= 0) {
         msg_se_log("**ERROR** SCS_COUNT %d is not valid. Must be > 0. Quitting.\n\n", opt->count_val);
         return (JVMTI_ERROR_INTERNAL);
      }
      gv->scs_rate = opt->count_val;
   }

   if (opt->event) {
      #if defined(_WINDOWS) ||  defined(_LINUX)
      if (opt->event_val == EVENT_INVALID_ID) {
         msg_se_log("\n**ERROR** SCS_EVENT is not valid. Check platform specific limitations.\n\n");
         msg_se_log("**ERROR** Enter \"mpevt -ls\" from a command prompt for list of valid events.\n\n");
         return (JVMTI_ERROR_INTERNAL);
      }
      #else
      if (strcmp(opt->event_name, "PM_CYC") == 0) {
         msg_se_log("\n**ERROR** PM_CYC can't be used as an SCS_EVENT.\n\n");
         msg_se_log("**ERROR** Enter \"pmlist -g -1\" from a command prompt for list of valid events.\n\n");
         return (JVMTI_ERROR_INTERNAL);
      }

      if (!pi_is_event_name_valid(opt->event_name)) {
         msg_se_log("\n**ERROR** SCS_EVENT is not valid. Check platform specific limitations.\n\n");
         msg_se_log("**ERROR** Enter \"pmlist -g -1\" from a command prompt for list of valid events.\n\n");
         return (JVMTI_ERROR_INTERNAL);
      }
      strcpy(gv->pm_event_name, opt->event_name);
      #endif
   }

   // Remember SCS_A2N: it means resolve everything (unless told not to)
   if (opt->a2n) {
      ge.scs_a2n   = 1;
   }

   #endif
#endif //_LEXTERN

   // Done checking. Set up to enable required events.
   // - Need CLASS_LOAD so we can resolve method_ids to method names.
   // - Need GC_START/END so we know we're in GC and don't record samples.
   gv->trees     = 1;                   // Build trees
   ge.ThreadInfo = 1;                   // Enable thread start/end events
   gv->statdyn   = 0;                   // Remove static/non-static from log-rt legend

   if (gv->usermets == 0) {
      gc.posNeg       = 0;              // Reset the metrics
      gv->scaleFactor = 0;              // Assume no scaling factor
      SetMetric(MODE_RAW_CYCLES);       // Need one dummy metric to use as base count
   }
   gv->gatherMetrics = 0;               // Time/Metrics not gathered when Sampling

   if (gv->scs_allocBytes || gv->scs_classLoad || gv->scs_monitorEvent) {
      if ( (    gv->scs_allocBytes   && gv->scs_monitorEvent )
           || ( gv->scs_classLoad    && gv->scs_allocBytes   )
           || ( gv->scs_classLoad    && gv->scs_monitorEvent ) ) {
         ErrVMsgLog("Too many sampling events\n");     // FIX ME:  Consider treating them as separate metrics
         return (JVMTI_ERROR_INTERNAL);
      }

      if (gv->scs_allocBytes == 0  && ge.ObjectInfo) {
         ErrVMsgLog("OBJECTINFO not compatible with the requested sampling event\n");
         return (JVMTI_ERROR_INTERNAL);
      }
   }

#ifndef _LEXTERN
   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
   else {
      gv->GCInfo = 1;                   // Enable GC start/finish events

      // If SCS_ADDR need to force JITA2N
      if (ge.scs_a2n) {
         gv->JITA2N = 1;                // Force JITA2N
      }

      // Select sampling mode
      if (opt->event && opt->count)
         gv->scs_sampling_mode = opt->event_val;
      else
         gv->scs_sampling_mode = SAMPLING_NOTIFY_EVENT_TIME;

      // Check for environment variables
      p = getenv("SCS_DEBUG");
      if (p) {
         // Turn on debug messages and attempt to fire debug trace hooks (major 0x17).
         // The default is to not display debug messages and not try to fire debug hooks.
         ge.scs_debug_trace = 1;
         gc.mask |= gc_mask_SCS_Debug;
         msg_log("***** SCS_DEBUG environment variable set *****\n");
      }

      p = getenv("SCS_DEBUG_TRACE");
      if (p) {
         // Attempt to fire debug trace hooks (major 0x17).
         // The default is to not even try to fire debug hooks.
         gc.mask |= gc_mask_SCS_Debug;
         msg_log("***** SCS_DEBUG_TRACE environment variable set *****\n");
      }

      p = getenv("SCS_DEBUG_MSG");
      if (p) {
         // Turn on debug messages.
         gc.mask |= gc_mask_SCS_Debug;
         msg_log("***** SCS_DEBUG_MSG environment variable set *****\n");
      }

      p = getenv("SCS_VERBOSE");
      if (p) {
         // Display messages we normally don't display.
         // The default is to not be too verbose.
         gc.mask |= gc_mask_SCS_Verbose;
         msg_log("***** SCS_VERBOSE environment variable set *****\n");
      }

#if defined(_LINUX)
      p = getenv("SCS_LOG_SAMPLES");
      if (p) {
         // Log samples that have a callstack in log-msg
         // The default is to not log.
         gv_scs_log_samples = 1;
         msg_log("***** SCS_LOG_SAMPLES environment variable set *****\n");
      }

      p = getenv("SCS_USERMODE_ONLY");
      if (p) {
         // Only walk user-mode stack. Ignore kernel stack.
         // The default is to walk both user-mode and kernel-mode stacks.
         gv_scs_usermode_only = 1;
         msg_log("***** SCS_USERMODE_ONLY environment variable set *****\n");
      }

      p = getenv("SCS_HARVEST_DELAYED");
      if (p) {
         // Harvest symbols when needed.
         // The default is do harvesting when symbols for all modules at
         // JProf load time.
         gv_scs_harvest_on_load = 0;
         msg_log("***** SCS_HARVEST_DELAYED environment variable set *****\n");
      }

      p = getenv("SCS_USE_BACKTRACE");
      if (p) {
         // Use libc's backtrace() to walk user-mode stacks.
         // The default is do the walking ourselves.
         gv_scs_use_backtrace = 1;
         gv_stop_frame = 2;
         msg_log("***** SCS_USE_BACKTRACE environment variable set *****\n");
      }

      p = getenv("SCS_STACK_WALK_MESSAGES");
      if (p) {
         // Write messages (to log-msg) as the stack is being walked.
         // The default is to not write messages.
         gv_scs_stack_walk_messages = 1;
         msg_log("***** SCS_STACK_WALK_MESSAGES environment variable set *****\n");
      }

      p = getenv("SCS_A2N_AT_FLUSH");
      if (p) {
         // Do symbol resolution on the fly.
         // The default (if SCS_A2N) is to do symbol resolution at FLUSH time.
         gv_scs_a2n_otf = 0;
         msg_log("***** SCS_A2N_AT_FLUSH environment variable set *****\n");
      }
#endif

      // Tell the world we're doing multi-threaded sampling
      #if defined(_AIX)
      ge.scs_thread_count = _system_configuration.ncpus;
      #else
      ge.scs_thread_count = PiGetActiveProcessorCount();
      #endif

      if (ge.scs == 0) {
         ge.scs = 1;
      }
   }
   #endif
#endif //_LEXTERN

   // Remember whether we want to enable immediately or wait for start from rtdriver
   if (gv->start == 0) {
      gv->scs_active = 0;               // wait for START to begin sampling
   }
   else if (ge.fNeverActive == 0) {
      gv->scs_active = 1;               // come up sampling
   }

   OptVMsg("gv->scs_active: %d\n", gv->scs_active);
   // Done
   return (JVMTI_ERROR_NONE);
}


#ifndef _LEXTERN
   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
//
// scs_allocate_memory()
// *********************
// ** INTERNAL **
//
static void scs_allocate_memory(void)
{
   int    i;
   int    size;
   char * p;

   // Stack frame structures are different sizes for JVMTI.
   if (ge.getStackTraceExtended)
      gv->stack_frame_size = sizeof(jvmtiFrameInfoExtended);
   else
      gv->stack_frame_size = sizeof(jvmtiFrameInfo);

   ge.scs_counters = (struct _scs_counters *)zMalloc("scs_allocate_memory", sizeof(struct _scs_counters));

   return;
}


//
// scs_free_memory()
// *****************
// ** INTERNAL **
//
static void scs_free_memory(void)
{
   xFree(ge.scs_counters, sizeof(struct _scs_counters));
   ge.scs_counters = NULL;

   return;
}


//
// scs_clear_counters()
// ********************
// ** EXTERNAL **
//
// Called from procSockCmd() when a "reset" command is received.
// Also called internally during initialization.
//
void scs_clear_counters(void)
{
   memset(ge.scs_counters, 0, sizeof(struct _scs_counters));
   return;
}


//
// scs_get_callstack()
// *******************
//
// Exceptions while getting the stack are usually fatal so we don't do
// anything about it. We either survive and keep going or we die.
// Function assumes that we're using either JVMTI.
//
int scs_get_callstack(JNIEnv *jniEnv, thread_t * target_tp, void * buffer, jvmtiFrameInfo *pi_stack_frames)
{
   int num_frames = 0;
   jthread  thread;

   UINT32 sample_tid = PtrToUint32(target_tp->tid);

   //
   // ***** JVMTI
   //
   thread = (*jniEnv)->NewLocalRef(jniEnv, target_tp->thread);
   if (thread) {
      if (ge.getStackTraceExtended) {
         csStatsProlog();
         (ge.getStackTraceExtended)(ge.jvmti,
                                    COM_IBM_GET_STACK_TRACE_EXTRA_FRAME_INFO
                                    + ((gv->prunecallstacks)
                                       ? COM_IBM_GET_STACK_TRACE_PRUNE_UNREPORTED_METHODS : 0),
                                    thread,
                                    0,
                                    ge.maxframes,
                                    (jvmtiFrameInfoExtended *)pi_stack_frames,
                                    &num_frames);
         csStatsEpilog(num_frames);
      }
      else {
         csStatsProlog();
         (*ge.jvmti)->GetStackTrace(ge.jvmti,
                                    thread,
                                    0,
                                    ge.maxframes,
                                    pi_stack_frames,
                                    &num_frames);
         csStatsEpilog(num_frames);
      }

      (*jniEnv)->DeleteLocalRef(jniEnv, thread);
   }
   else {
      if (gc.mask & gc_mask_SCS_Debug) {
         // Only the first five per processor, unless debugging
         msg_log("**WARNING** scs_get_callstack: NewLocalReference() for target_tp->tid %p failed.\n", target_tp->tid);
      }

      sprintf(buffer, "tid_%p", target_tp->tid);

      (*jniEnv)->DeleteWeakGlobalRef(jniEnv, target_tp->thread);
      target_tp->thread = 0;
   }

   return (num_frames);
}

static void checkOtherSamplingModes()
{
   if (gv->scs_allocBytes) {
      // SAMPLING=ALLOCATED_BYTES
      msg_log("**INFO** gv.scs_allocBytes = %d\n", gv->scs_allocBytes);
   }
   else if (gv->scs_allocations) {
      // SAMPLING=ALLOCATIONS[=AllocationsThreshhold:AllocatedBytesThreshold]
      msg_log("**INFO** gv.scs_allocations = %d\n", gv->scs_allocations);  // Allocations Threshhold
      msg_log("**INFO** gv.scs_allocBytes  = %d\n", gv->scs_allocBytes);   // Allocated Bytes Threshold
   }
   else if (gv->scs_classLoad) {
      // SAMPLING=CLASS_LOAD
      msg_log("**INFO** gv.scs_classLoad = %d\n", gv->scs_classLoad);
   }
   else if (gv->scs_monitorEvent == JVMTI_EVENT_MONITOR_WAIT) {
      // SAMPLING=MONITOR_WAIT
      msg_log("**INFO** gv->scs_monitorEvent = %d (MONITOR_WAIT)\n", gv->scs_monitorEvent);
   }
   else if (gv->scs_monitorEvent == JVMTI_EVENT_MONITOR_WAITED) {
      // SAMPLING=MONITOR_WAITED
      msg_log("**INFO** gv->scs_monitorEvent = %d (MONITOR_WAITED)\n", gv->scs_monitorEvent);
   }
   else if (gv->scs_monitorEvent == JVMTI_EVENT_MONITOR_CONTENDED_ENTER) {
      // SAMPLING=MONITOR_CONTENDED_ENTER
      msg_log("**INFO** gv->scs_monitorEvent = %d (MONITOR_CONTENDED_ENTER)\n", gv->scs_monitorEvent);
   }
   else if (gv->scs_monitorEvent == JVMTI_EVENT_MONITOR_CONTENDED_ENTERED) {
      // SAMPLING=MONITOR_CONTENDED_ENTERED
      msg_log("**INFO** gv->scs_monitorEvent = %d (MONITOR_CONTENDED_ENTERED)\n", gv->scs_monitorEvent);
   }
}

static void printScsInfo()
{
   // Display what we think we're working with
   msg_log("**INFO** ge.scs                  = %d", ge.scs);
   if (ge.scs) {
      msg_log("  (Java)\n");
   }
   else {
      msg_log("\n");
   }
   msg_log("**INFO** ge.scs_thread_count     = %d\n", ge.scs_thread_count);

   msg_log("**INFO** ge.can_use_a2n          = %s\n", ge.cant_use_a2n ? "0  (No)" : "1  (Yes)");
   msg_log("**INFO** ge.scs_a2n              = %d%s\n", ge.scs_a2n, ge.cant_use_a2n ? " (Can't Use)" : "");

      #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
   if (gv->scs_sampling_mode == SAMPLING_NOTIFY_EVENT_TIME) {
      msg_log("**INFO** event                   = TIME\n");
      msg_log("**INFO** rate                    = %d\n", gv->scs_rate);
   }
   else {
         #if defined(_WINDOWS) || defined(_LINUX)
      msg_log("**INFO** event                   = %s\n", GetPerfCounterEventNameFromId(gv->scs_sampling_mode));
         #else
      msg_log("**INFO** event                   = %s\n", gv->pm_event_name);
         #endif
      msg_log("**INFO** count                   = %d\n", gv->scs_rate);
   }
      #endif

   msg_log("**INFO** gv->scs_active          = %d\n", gv->scs_active);
   msg_log("**INFO** gv->start               = %d\n", gv->start);
}

//
// scs_initialize()
// ****************
// ** EXTERNAL **
//
// Called from cbVMInit() (in response to the VM_INIT JVMTI event) to perform
// whatever initialization in required.
//
void scs_initialize( JNIEnv * env )
{
   int hh, mm, ss;

   get_current_time(&hh, &mm, &ss, NULL, NULL, NULL);
   msg_log("\n**INFO** (%02d:%02d:%02d) SCS initialization starting\n", hh, mm, ss);

   scs_debug_msg(("*D* scs_initialize(): starting\n"));

   if (ge.scs) {
      // Warn them if running a SOV JVM
      if (ge.JVMversion == JVM_SOV) {
         msg_log("\n********************************************************************\n");
         msg_log("*****                                                          *****\n");
         msg_log("*****   Callstack sampling only works reliably on J9/SUN JVMs  *****\n");
         msg_log("*****        !!! You are using it at your own risk !!!         *****\n");
         msg_log("*****                                                          *****\n");
         msg_log("********************************************************************\n\n");
      }
   }
   else {
      scs_debug_msg(("*D* scs_initialize(): done - not in SCS mode\n"));

      // Check if any of the other SAMPLING= modes
      checkOtherSamplingModes();

      return;
   }

   // Allocate and initialize required data structures
   scs_allocate_memory();               // All counters initialized to zero

   int rc = InitScs(gv->scs_rate, scs_handle_sample);
   if (rc != 0) {
      ErrVMsgLog("**ERROR** scs_initialize() failed. rc = %d (%s)\n", rc, RcToString(rc));
      err_exit(0);
   }

   printScsInfo();

   // Get loaded modules and send them to A2N.
   // Doesn't matter whether it works or not, other than if it doesn't work
   // we won't do symbol resolution.
   // This is the latest we can do it without waiting for an event and then
   // doing it. That's an option if we find that the JVM hasn't finished
   // loading DLLs yet.
   #if defined(_WINDOWS) || defined(_LINUX)
   if (ge.scs_a2n && ge.pfnA2nLoadMap) {
      scs_debug_msg(("*D* A2nLoadMap() starting\n"));

      rc = ge.pfnA2nLoadMap((unsigned int)-1, 0);   // Gets *ALL* modules. Probably overkill!

      scs_debug_msg(("*D* A2nLoadMap() done. rc = %d.\n", rc));
   }
   #endif

   scs_debug_msg(("*D* scs_initialize(): done\n"));
   return;
}

// Callback from perfutil
static int scs_handle_sample(void *env, scs_callstack_data *callstack_data)
{
   JNIEnv *jniEnv = (JNIEnv *)env;

   if (!env || !callstack_data || !ge.jvmti)
      return PU_ERROR_SCS_CALLBACK_ERROR;

   if (callstack_data->tid == -1 ||
       callstack_data->pid == -1 ||
       callstack_data->ip == 0)
      return PU_ERROR_SCS_CALLBACK_ERROR;

   void *buffer = zMalloc(0, 4096);
   if (buffer == NULL) {
      ErrVMsgLog("**ERROR** Unable to allocate 4KB buffer.\n");
      return PU_ERROR_NOT_ENOUGH_MEMORY;
   }

   // JVM is terminating
   if (gc.ShuttingDown) {
      // Terminate this sampler thread. JVM is about to die anyway.
      scs_debug_msg(("*D* JVM shutting down.\n"));
      return PU_SUCCESS;
   }

   if (!gv->scs_active) {
      return PU_SUCCESS;
   }

   if (ge.gc_running) {
      return PU_SUCCESS;
   }

   thread_t *target_tp = insert_thread(callstack_data->tid);
   Node *leaf = NULL;
   jvmtiFrameInfo *pi_stack_frames = NULL;
   int num_frames = 0;
   uint64_t sample_eip = callstack_data->ip;
   uint64_t sample_r3_eip = sample_eip;
   FrameInfo fi;

   if (ge.jvmti && target_tp->thread == NULL) {
      sprintf(buffer, "tid_%p", target_tp->tid);
      return PU_SUCCESS;
   }

   if (target_tp->no_stack_cnt > NO_CALLSTACK_GIVEUP_THRESHOLD) {
      // Likely to never get a callstack so don't keep trying
      num_frames = 0;
      scs_debug_msg(("*D* Unlikely to get stack for tid %p. Not trying\n", target_tp->tid));
      return PU_SUCCESS;
   }
   else {
      pi_stack_frames = malloc(ge.maxframes * gv->stack_frame_size);
      if (!pi_stack_frames)
         return PU_ERROR_NOT_ENOUGH_MEMORY;
      num_frames = scs_get_callstack(jniEnv, target_tp, buffer, pi_stack_frames);
   }

   if (num_frames == ge.maxframes) {
      msg_log("**WARNING** %d stack frames returned for thread %x. Callstack may not be complete.\n",
              num_frames, callstack_data->tid);
   }

   // The array of call_frames comes back with the deepest frame (where we are)
   // first (index 0). That means we have to push it into the tree backwards.
   leaf = target_tp->currT;

   fi.type = gv_type_scs_Method;

   if (num_frames == 0) {
      target_tp->no_stack_cnt++;

      if (target_tp->no_stack_cnt > NO_CALLSTACK_GIVEUP_THRESHOLD) {
         scs_debug_msg(("*D* Give up trying to get a stack on tid %p\n", target_tp->tid));
      }
   }
   else {
      int i;
      for (i = num_frames - 1; i >= 0; i--) {
         getFrameInfo(pi_stack_frames, num_frames, i, &fi);
         leaf = push(leaf, &fi);
      }
   }
   target_tp->currm = leaf;

   scs_deal_with_good_sample(target_tp, sample_eip, sample_r3_eip, buffer);

   // Bump base count of whatever the new leaf is
   Node *np = target_tp->currm;

   if (gv_type_LUAddrs != np->ntype)   // Count is implicitly added to LUAddrs
      np->bmm[0]++;

   if (pi_stack_frames)
      free(pi_stack_frames);

   return PU_SUCCESS;
}


//
// scs_a2n_send_jitted_method()
// ****************************
// ** EXTERNAL **
//
// Called from jmLoadCommon() when handling a COMPILED_METHOD_LOAD
// event (because gv->JITA2N=1).
//
void scs_a2n_send_jitted_method(UINT32 tls_index,
                                void * method_addr,
                                int    method_length,
                                char * class_name,
                                char * method_name,
                                char * method_signature,
                                char * code_name)
{
      #define MAX_METHOD_NAME 2048
   char * name;
   size_t name_len;

      #if defined(_AIX)
   return;
      #endif

   if (ge.cant_use_a2n)
      return;

      #if defined(_WINDOWS)

   name = TlsGetValue(tls_index);
   if (name == NULL) {
      name = xMalloc("scs_a2n_send_jitted_method", DEFAULT_TLS_SIZE);
      TlsSetValue(tls_index, name);
   }

      #elif defined(_LINUX)
   name = xMalloc("scs_a2n_send_jitted_method", MAX_METHOD_NAME);
      #endif

   name_len = 2 + strlen(class_name) + strlen(method_name) + strlen(method_signature);

   if (code_name)
      name_len += strlen(code_name);

   if (name_len > MAX_METHOD_NAME) {
      msg_log("**ERROR** Method name is way too long. Ignored. name=%s.%s%s%s\n",
              class_name, method_name, method_signature, code_name);
   }
   else {
      strcpy(name, class_name);
      strcat(name, ".");
      strcat(name, method_name);
      strcat(name, method_signature);
      if (code_name)
         strcat(name, code_name);

      scs_debug_msg(("*D* send_jitted_method: 0x%"_PZP" %4d %s\n",
                     method_addr, method_length, name));

      ge.pfnA2nAddJittedMethod(ge.pid, name, PtrToUint64(method_addr), method_length, NULL);
   }

      #if defined(_LINUX)
   xFree(name, MAX_METHOD_NAME);
      #endif

   return;
}


//
// scs_deal_with_good_sample()
// ***************************
// ** EXTERNAL **
//
// Invoked on the target thread from getThreadCallStack() (after we've gotten
// a stack) or invoked on the sampler thread after getting a stack.
//
void scs_deal_with_good_sample( thread_t * tp,
                                UINT64     sample_eip,      // Sample EIP/RIP
                                UINT64     sample_r3_eip,   // Sample ring 3 EIP/RIP
                                char     * buffer)
{
   UINT64 sample_r0_eip = 0;
   int rc;
   SYMDATA sd;
   Node * leaf;
   Node * parent;
   Node * child;
   char * name;

   int r3resolved    = 0;

   scs_debug_msg(("*D* scs_deal_with_good_sample: enter\n"));

   // Leaf nodes can only be java methods, because the JVM only returns
   // java methods when we request a callstack.

   if (sample_r3_eip == 0)
      return;

   if (sample_eip != sample_r3_eip)
      sample_r0_eip = sample_eip;

//----------------------------------------------
// Check for drift to nearby nodes
//----------------------------------------------

   leaf = checkNearbyNodes(tp->currm, Uint64ToPtr(sample_r3_eip));

   if (leaf) {
      r3resolved = 1;
      tp->currm = leaf;                 // Switch to correct leaf
   }

   if (ge.scs_a2n && (sample_r0_eip || !r3resolved)) {
      updateLUAddrs(tp, sample_r3_eip, sample_r0_eip);   // Prepare this sample address for delayed resolution
   }

   return;
}


//
// scs_terminate()
// ***************
// **EXTERNAL**
//
// Called from StopJProf() to display final totals and to write log-rt.
//
void scs_terminate(void)
{
   int hh, mm, ss;
   get_current_time(&hh, &mm, &ss, 0, 0, 0 );
   msg_log("\n**INFO** (%02d:%02d:%02d) Begin SCS cleanup ...\n", hh, mm, ss);

   // Disable notifications
   gv->scs_active = 0;
   memory_fence();

   // Disable SCS mode
   msg_log("**INFO** disabling SCS\n");
   int rc = ScsOff();
   msg_log("**INFO** in scs_terminate: rc = %d\n", rc);

   if (ge.scs_counters) {
      msg_log("\n ***** SCS Summary *****\n");
      msg_log("StkAttempt: %6d\n", ge.scs_counters->scs_stack_attempt_count);
      msg_log("StkSuccess: %6d\n", (ge.scs_counters->scs_stack_attempt_count - ge.scs_counters->scs_nostack_count));
      msg_log("Stk Retry Giveup: %6d\n", ge.scs_counters->scs_retry_giveup_count);
      msg_log("GC: %6d\n", ge.scs_counters->scs_gc_count);
      msg_log("Deepest Stack: %6d\n", ge.scs_counters->deepest_stack);
   }

   msg_log(" ***********************\n");

   // Free up everything we allocated
   scs_free_memory();

   return;
}
   #endif // _WINDOWS || _LINUX || _AIX
#endif // _LEXTERN
