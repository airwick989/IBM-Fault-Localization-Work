//
// Linux Perf Util - Perf Events Helper
//

#ifndef XPU_PERF_EVENT
#define XPU_PERF_EVENT

#include<vector>
#include<map>

extern "C" {
#include<linux/perf_event.h>
}
#include"perfutil_core.hpp"

#define PE_MAX_TRACE_EVENTS		2
#define PE_MAX_COUNTER_EVENTS	4
#define PE_MAX_GROUP_EVENTS		4

#define PE_DATA_SIZE_N(n)		(1 << n) * PAGE_SIZE
#define PE_DATA_SIZE			PE_DATA_SIZE_N(2)
#define PE_DATA_OFFSET			PAGE_SIZE
#define PE_MMAP_SIZE			PE_DATA_SIZE + PE_DATA_OFFSET

#define MAX_RECORD_SIZE			4096

typedef struct perf_event_mmap_page PE_MMAP_PG;
typedef struct perf_event_attr PE_ATTR;
typedef struct perf_event_header PE_HEADER;

namespace pi {
namespace pu {

class PiFacility;
class PiEventManager;

class Pi_PerfEvent {
public:
   int fdisc;
   struct perf_event_attr attr;
   void *mappedmem;
   int mmap_size;

   Pi_PerfEvent(PE_ATTR &pe_attr);
   ~Pi_PerfEvent();
   int init(pid_t pid, int cpu, int group_fd);
   int readCounters();
   int readSamples(void *mteBuff);
   int ioctl_call(unsigned long int opcode, int arg);
   int close_fd();
   int read_trace_data(bool force, int cpu, PiFacility *facility);

   /*
    * stats
    */
   int epid, ecpu;
   long records;
   static int eventsOpen;
   static long mmapMemSize;

   int unring_mmap_data(char *buffer, int buff_size);
   int get_next_record(char *buffer, PE_HEADER **rec_ptr, int bytes_left);
   int read_sample_id(PE_HEADER *record, PE_ATTR &attr, uint32_t *pid,
                      uint32_t *tid, uint64_t *time);
   int process_perf_record(PE_HEADER *record, PE_ATTR &attr, int cpu,
                           PiFacility *facility);
};

class Pi_PerfEvent_Group {
public:
   bool valid;
   int num_events;
   Pi_PerfEvent *leader;
   Pi_PerfEvent *events[PTT_MAX_METRICS];
   int cpu_num;
   pid_t pid;

   Pi_PerfEvent_Group(pid_t inpid, int cpu, PE_ATTR &leader_attr);
   Pi_PerfEvent_Group();
   ~Pi_PerfEvent_Group();
   int init_leader(PE_ATTR &leader_attr);
   int init_all();
   int add_child_pe(PE_ATTR &pe_attr);
   int ioctl_call(unsigned long int opcode, int arg);
   int init_mteBuffer(int bufferSize);
   int close_fd();
   void free_all_memory();
};

typedef Pi_PerfEvent_Group* Pi_PerfEvent_Group_ptr;
typedef std::map<int, Pi_PerfEvent_Group> Pi_PerfEvent_Group_Map;

// TODO: make this thread safe
class PiEventManager {
public:
   struct perf_event_attr attr;

   PiEventManager();
   ~PiEventManager();
   int init(int scope, PE_ATTR &leader_attr, PiFacility *facility);
   int add(std::vector<int> &keys, bool enable);
   int add(int key, bool enable);
   int add(int key, bool enable, PE_ATTR &leader_attr);
   int remove(int key, bool read);
   int clear();
   int readAllRecords(bool force);
   int readRecords(int key, bool force);
   int ioctl_call(unsigned long int opcode, int arg);
   std::vector<int> waitOnData(uint32 timeout_millis, int max_events = 1);
   int waitAndReadData(uint32 timeout_millis, int maxEvents = 1);
   void discardData();

   int getScope() {
      return scope;
   }

   PiFacility *piFacility;
private:
   int scope;
   Pi_PerfEvent_Group_Map eventGroups;
   int epfd, read_fifo, write_fifo;
   std::vector<int> removeList;
   unsigned long readRequests;

   int registerGroup(int key);
   int unregisterGroup(int key);
   int processRemoveList();
   void validate(std::vector<int> &keys);
};

}
}

//class PerfEvent_CpuGroup	*pe_count_groups = 0;

int piEvent_to_perfEvent(int pi_event, int *pe_type, int *perfEvent);

void init_pe_attr(struct perf_event_attr &attr);
void init_tprof_attr(struct perf_event_attr &attr);
void init_mte_attr(struct perf_event_attr &attr);
void init_scs_attr(struct perf_event_attr &attr);
void init_ptt_attr(struct perf_event_attr &attr);

#endif
