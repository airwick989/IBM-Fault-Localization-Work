// TODO: rewrite file headers

#ifndef SAMPLE_PROCESSOR_H
#define SAMPLE_PROCESSOR_H

#include<string>

namespace pi {
namespace pu {

class PiFacility;

class RecordHeader {
public:
   int64_t  timeStamp;
   uint64_t itemId;
   int32_t  flag;
   int32_t  cpu;
   int32_t  pid, tid;
};

class SampleRecord : public RecordHeader {
public:
   uint64_t address;
   bool     isUserMode;
};

class SampleProcessor {
public:
   SampleProcessor() : piFacility(0) {};
   virtual ~SampleProcessor() {}
   virtual int initialize(std::string &name, PiFacility *facility) = 0;
   virtual int startedRecording() = 0;
   virtual int stoppedRecording() = 0;
   virtual int terminate() = 0;

   virtual int taskName(RecordHeader &header, std::string &tname) = 0;
   virtual int taskStart(RecordHeader &header, bool newProc) = 0;
   virtual int taskEnd(RecordHeader &header) = 0;
   virtual int memorySegment(RecordHeader &header, uint64_t address, uint64_t size, std::string &name) = 0;
   virtual int symbol(RecordHeader &header,uint64_t moduleId, uint64_t address, uint64_t size, std::string &name) = 0;
   virtual int sample(SampleRecord &sample) = 0;

protected:
   PiFacility *piFacility;
};

} // PU
} // PI

#endif				/* PU_SAMPLE_PROCESSOR_H */
