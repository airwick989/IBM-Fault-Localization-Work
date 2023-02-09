/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2009
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef NRM2_SAMPLE_PROCESSOR_H
#define NRM2_SAMPLE_PROCESSOR_H

#include "itypes.h"
#include "pitrace.h"

#include "SampleProcessor.hpp"

namespace pi {
namespace pu {

#define ALIGNMENT			8
#define IS_ALIGNED(size)	(size % ALIGNMENT == 0)
#define	PADDING(size)		(ALIGNMENT - (size & (ALIGNMENT - 1))) & (ALIGNMENT - 1)

class NRM2SampleProcessor : public SampleProcessor {
public:
   NRM2SampleProcessor();
   virtual ~NRM2SampleProcessor();

   virtual int initialize(std::string &name, PiFacility *facility);
   virtual int startedRecording();
   virtual int stoppedRecording();
   virtual int terminate();

   virtual int taskName(RecordHeader &header, std::string &tname);
   virtual int taskStart(RecordHeader &header, bool newProc);
   virtual int taskEnd(RecordHeader &header);
   virtual int memorySegment(RecordHeader &header, uint64_t address, uint64_t size, std::string &name);
   virtual int symbol(RecordHeader &header, uint64_t moduleId, uint64_t address, uint64_t size, std::string &name);
   virtual int sample(SampleRecord &sample);


   PERFHEADER *get_perfHeader(int type, int cpu) {
      PERFHEADER *PerfHeader = 0;
      if (type == TRACE_TYPE) {
         PerfHeader = (PERFHEADER *) ptr_btrace_buffers[cpu];
      } else {       // MTE_TYPE
         PerfHeader = (PERFHEADER *) ptr_mte_buffers;
      }
      return PerfHeader;
   }

   //
   // is_perf_buf()
   // ************
   // Returns an error if a perf buf does not exist
   //
   bool is_perf_buf(int cpu) {
       return ptr_btrace_buffers[cpu] != NULL;
   }

private:
   /*
    * buffer initialization
    */
   int PITRACE_InitBuffer(uint32_t buffer_len);
   int init_continuos_mode(void);
   int free_hook_buffers();
   /*
    * buffer info/gets
    */
   bool has_data_to_write(PERFHEADER * ptr_buf);

   /*
    * hook writing methods
    */
   int ContTraceWrite(PI_HOOK_HEADER *rawData, uint16_t rawDataLen, int type, int cpu);
   int MTEWrite(PI_HOOK_HEADER *rawData, uint16_t rawDataLen, int cpu);
   int write_raw_hook(PI_HOOK_HEADER *rawData, uint16_t rawDataLen, UINT32 type, int cpu);

   int write_RAW64_HOOK(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                        UINT64 time, UINT64 intvals[], int num_ints);
   int PerfTraceHookSimple1(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                            int64_t timeStamp, UINT64 u1);
   int PerfTraceHookSimple2(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                            int64_t timeStamp, UINT64 u1, UINT64 u2);
   int PerfTraceHookSimple3(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                            int64_t timeStamp, UINT32 u1, UINT32 u2, UINT32 u3);
   int PerfTraceHookSimple4(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                            int64_t timeStamp, UINT64 u1, UINT64 u2, UINT64 u3,
                            UINT64 u4);
   int PerfTraceHookSimple5(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                            int64_t timeStamp, UINT64 u1, UINT64 u2, UINT64 u3,
                            UINT64 u4, UINT64 u5);

   int write_segment_hook(int cpu, int64_t timeStamp, int pid, uint64_t addr, int size, char *name);
   int write_pidname_hook(int cpu, int64_t timeStamp, int pid, char *name);
   int write_fork_hook(int cpu, int64_t timeStamp, int pid, int ppid, bool isClone);
   int write_task_exit(int cpu, int64_t timeStamp, int pid);
   int write_tprof_tick(int cpu, int64_t timeStamp, int pid, int tid, uint64_t addr, bool isUserMode);

   /*
    * buffer control w/ hook writing
    */
   int SoftTraceOn(void);

   /*
    * buffer reading logic
    */
   int pi_file_write(char *data, int data_size, PERFHEADER* pheader);
   int pi_consume(int type, int cpu);
   int write_buffer_final(PERFHEADER* PerfHeader);
   int finish_reading(void);
   int file_close(void);

   int cont_section_size; // section size for continous tracing
   bool  MTE_processed;
   char *ptr_btrace_buffers[128]; // per-processor storage area for trace
   char *ptr_mte_buffers;  // storage area for mte
   FILE * pi_tracefile;
   std::string fileName;
};

}
}

#endif				/* NRM2_SAMPLE_PROCESSOR_H */
