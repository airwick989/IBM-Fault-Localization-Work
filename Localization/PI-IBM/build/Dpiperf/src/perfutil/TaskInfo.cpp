
#include "perfutil_core.hpp"
#include "PiFacility.hpp"
#include "SampleProcessor.hpp"
#include "TaskInfo.hpp"

namespace pi {
namespace pu {

TaskInfo::TaskInfo() {
   piFacility= 0;
}

TaskInfo::~TaskInfo() {
}

int TaskInfo::initialize(PiFacility *facility) {
   piFacility = facility;
   int max_pids = getMaxPid();
   if (max_pids <= 0) {
      PI_PRINTK_ERR("failed to set max_pids");
      return -1;
   }
   pidVector.assign(max_pids, false);
   return _init();
}

int TaskInfo::resetData() {
   pidVector.assign(pidVector.size(), false);
   return 0;
}

/*
 * handleProcess(process Id, thread Id)
 * ********** Internal **************
 * This checks if a pid/tid has been processed, if not then it will try to
 * Processing involves collecting memory mappings information and thread names
 */
int TaskInfo::handleProcess(int pid, int tid) {
   if (!pid || !tid)
      return PU_SUCCESS;

   int rc = 0;

   if (pidVector[tid])
      return PU_SUCCESS;

   rc = handleProcessName(pid, tid);
   RETURN_RC_FAIL(rc);

   if (pid == tid) {
      rc = handleProcessMaps(pid);
      RETURN_RC_FAIL(rc);
   }
   else {
      // make sure we have information about the process
      rc = handleProcess(pid, pid);
      RETURN_RC_FAIL(rc);
   }

   pidVector[tid] = 1;
   return rc;

}

int NRM2TaskInfo::setProcessName(RecordHeader &header, std::string name) {
   return piFacility->sampleProcessor->taskName(header, name);
}

int NRM2TaskInfo::newProcess(RecordHeader &header) {
   return piFacility->sampleProcessor->taskStart(header, true);
}

int NRM2TaskInfo::newThread(RecordHeader &header) {
   return piFacility->sampleProcessor->taskStart(header, false);
}

int NRM2TaskInfo::addModule(RecordHeader &header, uint64_t addr, long len,
                        std::string name) {
   return piFacility->sampleProcessor->memorySegment(header, addr, len, name);
}

}
}
