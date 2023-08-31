extern "C" {
#include "encore.h"
#include "a2n.h"
}

#include "perfutil_core.hpp"
#include "SampleProcessor.hpp"
#include "A2NTaskInfo.hpp"
#include "PiFacility.hpp"

#if defined(_LINUX)
#if defined(_AMD64)
#include <sys/user.h>
#include <asm/vsyscall.h>
const uint64_t vsyscall32_base = (uint64_t) 0xFFFFE000;
const uint64_t vsyscall32_end = (uint64_t) 0xFFFFF000;
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0)
const uint64_t vsyscall64_base = (uint64_t)VSYSCALL_START;
const uint64_t vsyscall64_end = (uint64_t)VSYSCALL_END;
#else
const uint64_t vsyscall64_base = (uint64_t)VSYSCALL_ADDR;
const uint64_t vsyscall64_end = (uint64_t) VSYSCALL_ADDR;
#endif
#elif defined(_X86)
const uint64_t vsyscall32_base = (uint64_t)0xFFFFE000;
const uint64_t vsyscall32_end = (uint64_t)0xFFFFF000;
const uint64_t vsyscall64_base = 0;
const uint64_t vsyscall64_end = 0;
#else
const uint64_t vsyscall32_base = 0;
const uint64_t vsyscall32_end = 0;
const uint64_t vsyscall64_base = 0;
const uint64_t vsyscall64_end = 0;
#endif
#endif

// FIXME: needs to accept args
#define LOG_A2N_ERR(str, rc) PI_PRINTK_ERR(str " (%d) %s\n", rc, A2nRcToString(rc))

namespace pi {
namespace pu {

/**********************************/
int initSymUtil() {
   unsigned long v;
   int rc;
   A2nInit();                      // REQUIRED for _ZOS, harmless for all others

   v = A2nGetVersion();
   PDEBUG(" A2nVersion: %d.%d.%d\n", (int)A2N_VERSION_MAJOR(v),
         (int)A2N_VERSION_MINOR(v), (int)A2N_VERSION_REV(v));

   // a2n debug
#ifdef PU_TRACE
   //A2nSetDebugMode(DEBUG_MODE_ALL);
#endif

   // a2n rename duplicate symbols
   //  BADDOG. add/stub to LX & AIX
#ifdef _WINDOWS
   A2nRenameDuplicateSymbols();
#endif

   // Tell A2N what to return if NoSymbols or NoSymbolFound:
   //   a2n_nsf = 1: Return NoSymbols or NoSymbolFound
   //   a2n_nsf = 0: Return <<section_name>> or NoSymbols
   A2nReturnSectionNameOnNSF();

   // add post option to error control
   A2nSetErrorMessageMode(4);           // E:1 W:2 I:3 A:4

   // gather all symbols when a module is loaded. This should improve resolve times
   // for real time processing
   A2nImmediateSymbolGather();

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
   if (vsyscall32_base) {
      int vsyscall32_size = (int) (vsyscall32_end - vsyscall32_base);
      A2nAddModule(0, vsyscall32_base, vsyscall32_size, A2N_VSYSCALL32_STR,
            TS_VSYSCALL32, 0, NULL);
   }
   if (vsyscall64_base) {
      int vsyscall64_size = (int) (vsyscall64_end - vsyscall64_base);
      A2nAddModule(0, vsyscall64_base, vsyscall64_size, A2N_VSYSCALL64_STR,
            TS_VSYSCALL64, 0, NULL);
   }

   A2nAddModule(0, 0, 0, "vmlinux", 0, 0, 0);
   char *kas = "/proc/kallsyms";
   rc = A2nSetKallsymsFileLocation(kas, 0, 0);
   if (rc != 0) {
      PI_PRINTK_ERR("A2nSetKallsymsFileLocation() rc %d (%s)\n", rc,
            A2nRcToString(rc));
      return rc;
   }

   char *kmd = "/proc/modules";
   rc = A2nSetModulesFileLocation(kmd, 0, 0);
   if (rc != 0) {
      PI_PRINTK_ERR("A2nSetModulesFileLocation() rc %d (%s)\n", rc,
            A2nRcToString(rc));
      return rc;
   }

   int pid = getpid();
#endif
   A2nLoadMap(pid, 0);

   return rc;
}

int A2NTaskInfo::_init() {
   return initSymUtil();
}

int A2NTaskInfo::setProcessName(RecordHeader &header, std::string name) {
   int rc = 0;
   rc = A2nSetProcessName(header.pid, (char *) name.c_str());
   if (rc != 0) {
      LOG_A2N_ERR("A2N failed to set process name", rc);
   }
   rc = piFacility->sampleProcessor->taskName(header, name);
   return rc;
}

int A2NTaskInfo::newProcess(RecordHeader &header) {
   int rc = 0;
#if defined(_LINUX)
   rc = A2nForkProcess(header.pid, header.tid);
#elif defined(_WINDOWS)
   rc = A2nCreateProcess(header.pid);
#endif
   if (rc != 0) {
      LOG_A2N_ERR("A2N create process returned", rc);
   }
   return rc;
}

int A2NTaskInfo::newThread(RecordHeader &header) {
   int rc = 0;
#if defined(_LINUX)
   rc = A2nCloneProcess(header.pid, header.tid);
#endif
   if (rc != 0) {
      LOG_A2N_ERR("A2N create thread returned", rc);
   }
   return rc;
}

int A2NTaskInfo::addModule(RecordHeader &header, uint64_t addr, long len,
                           std::string name) {
   int rc = 0;
   rc = A2nAddModule(header.pid, addr, len, (char *) name.c_str(), 0, 0, NULL);
   if (rc != 0) {
      LOG_A2N_ERR("A2N failed to add module", rc);
   }
   return rc;
}

int A2NTaskInfo::handleProcessMaps(int pid) {
   int rc = 0;
   rc = A2nLoadMap(pid, 0);
   if (rc != 0) {
      LOG_A2N_ERR("A2N failed to load maps", rc);
   }
   return rc;
}

int A2NTaskInfo::resolveSymbol(SampleRecord &sample, ResolvedSymbol &symbol) {
   int rc = 0;
   SYMDATA sdp;
   rc = A2nGetSymbol(sample.pid, sample.address, &sdp);
   if (rc == 0) {
      symbol.offset = sample.address - sdp.sym_addr;
      symbol.moduleId = (uint64_t) sdp.module;
      symbol.symbolId = (uint64_t) sdp.symbol;
      handleModule(sample, sdp);
      handleSymbol(sample, sdp);
   }
   else if (rc < A2N_FLAKY_SYMBOL){
      if ((rc == 1 || rc == 2) && sdp.module) {
         handleModule(sample, sdp);
         symbol.moduleId = (uint64_t) sdp.module;
      }
      else {
         symbol.moduleId = 0;
      }
      symbol.offset = sample.address;
      symbol.symbolId = 0;
   }
   else {
      LOG_A2N_ERR("A2N Failed to resolve Symbol", rc);
   }
   return rc;
}

int A2NTaskInfo::handleModule(SampleRecord &sample, SYMDATA &sd) {
   int rc = 0;
   if (sd.module) {
      if (sd.mod_req_cnt < 2) {
         RecordHeader header;
         header.timeStamp = sample.timeStamp;
         header.pid = sample.pid;
         header.tid = sample.tid;

         header.itemId = (uint64_t) sd.module;
         header.cpu = 0;
         std::string name(sd.mod_name);
         rc = piFacility->sampleProcessor->memorySegment(header, sd.mod_addr,
               sd.mod_length, name);
      }
   }
   return rc;
}

int A2NTaskInfo::handleSymbol(SampleRecord &sample, SYMDATA &sd) {
   int rc = 0;
   if (sd.symbol) {
      if (sd.sym_req_cnt < 2) {
         RecordHeader header;
         header.timeStamp = sample.timeStamp;
         header.pid = sample.pid;
         header.tid = sample.tid;

         header.itemId = (uint64_t) sd.symbol;
         header.cpu = 0;
         std::string name(sd.sym_name);
         rc = piFacility->sampleProcessor->symbol(header, (uint64_t) sd.module,
               sd.sym_addr, sd.sym_length, name);
      }
   }
   return rc;

}

}
}
