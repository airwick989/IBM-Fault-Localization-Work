#ifndef _TASKINFO_HPP
#define _TASKINFO_HPP

#include <vector>
#include <string>

//#include "a2n.h"

namespace pi {
namespace pu {

class PiFacility;
class RecordHeader;
class SampleRecord;

class ResolvedSymbol {
public:
   uint64_t offset;
   uint64_t symbolId;
   uint64_t moduleId;
};

class TaskInfo {
public:
   static int getMaxPid();
   static int getProcessTidList(int pid, std::vector<int> &tidList);

   TaskInfo();
   virtual ~TaskInfo();
   virtual int initialize(PiFacility *facility);
   virtual int resetData();

   virtual int setProcessName(RecordHeader &header, std::string name) = 0;
   virtual int newProcess(RecordHeader &header) = 0;
   virtual int newThread(RecordHeader &header) = 0;
   virtual int addModule(RecordHeader &header, uint64_t addr, long len,
                         std::string name) = 0;

   virtual int resolveSymbol(SampleRecord &sample, ResolvedSymbol &symbol) = 0;

   /*
    * handles to check if pid has been processed and handle them if necessary
    */
   int handleProcess(int pid, int tid);

protected:
   virtual int _init() = 0;
   virtual int handleProcessMaps(int pid) = 0;
   PiFacility *piFacility;

private:
   int handleProcessName(int pid, int tid);

   std::vector<bool> pidVector;
};

class NRM2TaskInfo: public TaskInfo {
public:
   virtual int setProcessName(RecordHeader &header, std::string name);
   virtual int newProcess(RecordHeader &header);
   virtual int newThread(RecordHeader &header);
   virtual int addModule(RecordHeader &header, uint64_t addr, long len,
                 std::string name);

   virtual int resolveSymbol(SampleRecord &sample, ResolvedSymbol &symbol) { return 0; };

protected:
   virtual int _init() { return 0; };
   virtual int handleProcessMaps(int pid);
};

}
}

#endif
