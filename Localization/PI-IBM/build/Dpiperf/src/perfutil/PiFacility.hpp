#ifndef _PIFACILITY_HPP
#define _PIFACILITY_HPP

#include <vector>
#include <string>

namespace pi {
namespace pu {

class TaskInfo;
class SampleProcessor;
class PiEventManager;

class PiFacility {
public:
   PiFacility() :
         taskInfo(0), sampleProcessor(0), eventManager(0) {
   }

   bool isInitialized() {
      return taskInfo && sampleProcessor && eventManager;
   }

   TaskInfo * taskInfo;
   SampleProcessor * sampleProcessor;
   PiEventManager * eventManager;
};

}
}

#endif
