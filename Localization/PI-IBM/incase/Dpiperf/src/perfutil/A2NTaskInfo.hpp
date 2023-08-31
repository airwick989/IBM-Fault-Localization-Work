#ifndef _A2NTASKINFO_HPP
#define _A2NTASKINFO_HPP

#include <vector>
#include <string>

#include "TaskInfo.hpp"

typedef struct symdata SYMDATA;

namespace pi {
namespace pu {

class A2NTaskInfo : public TaskInfo {
public:
   virtual int setProcessName(RecordHeader &header, std::string name);
   virtual int newProcess(RecordHeader &header);
   virtual int newThread(RecordHeader &header);
   virtual int addModule(RecordHeader &header, uint64_t addr, long len,
                 std::string name);

   virtual int resolveSymbol(SampleRecord &sample, ResolvedSymbol &symbol);

protected:
   virtual int _init();
   virtual int handleProcessMaps(int pid);

private:
   int handleModule(SampleRecord &sample, SYMDATA &symbol);
   int handleSymbol(SampleRecord &sample, SYMDATA &symbol);
};

}
}

#endif
