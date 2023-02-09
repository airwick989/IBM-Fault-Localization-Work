
//
// Linux Perf Util - Perf Events Helper
//

#include<sys/syscall.h>
#include <sys/epoll.h>

#include "utils/pi_time.h"

#include "linux/xpu_perf_event.hpp"
#include "perfutil_core.hpp"
#include "pu_trace.hpp"
#include "SampleProcessor.hpp"
#include "PiFacility.hpp"
#include "TaskInfo.hpp"

int precise_ip = 2;

#define CACHE_EVENT_CONF(cache_id, op_id, result_id) cache_id | (op_id << 8) | (result_id << 16)

int piEvent_to_perfEvent(int pi_event, int *pe_type, int *perfEvent) {
   int pet, pe;
   int conf = 0;
   switch (pi_event) {
   /*
    * Hardware Events
    */
   case EVENT_NONHALTED_CYCLES:
      pet = PERF_TYPE_HARDWARE;
      pe = PERF_COUNT_HW_CPU_CYCLES;
      break;
   case EVENT_CPU_CLK_UNHALTED_REF:
      pet = PERF_TYPE_HARDWARE;
      pe = PERF_COUNT_HW_REF_CPU_CYCLES;
      break;
   case EVENT_INSTR:
   case EVENT_INSTR_COMPLETED:
      pet = PERF_TYPE_HARDWARE;
      pe = PERF_COUNT_HW_INSTRUCTIONS;
      break;
   case EVENT_BRANCH:
      pet = PERF_TYPE_HARDWARE;
      pe = PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
      break;
   case EVENT_MISPRED_BRANCH:
      pet = PERF_TYPE_HARDWARE;
      pe = PERF_COUNT_HW_BRANCH_MISSES;
      break;
   /*
    * Cache Events
    */
   case EVENT_ITLB_MISS:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_ITLB, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS);
      break;
   case EVENT_L2_READ_MISS:
   case EVENT_L3_READ_MISS:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_LL, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS);
      break;
   case EVENT_L2_READ_REFS:
   case EVENT_L3_READ_REFS:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_LL, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_ACCESS);
      break;
   case EVENT_LOADS:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_NODE, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_ACCESS);
      break;
   case EVENT_STORES:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_NODE, PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_ACCESS);
      break;
   // L1 data cache
   case EVENT_DC_MISS:
      // read miss
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS);
      break;
   case EVENT_DC_ACCESS:
   case EVENT_L1D_CACHE_LD:
      // read access
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_ACCESS);
      break;
   case EVENT_L1D_CACHE_ST:
      // write
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_ACCESS);
      break;
   // L1 instruction cache
   case EVENT_IC_MISS:
   case EVENT_L1I_MISSES:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_L1I, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS);
      break;
   case EVENT_IC_FETCH:
   case EVENT_L1I_READS:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_L1I, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_ACCESS);
      break;
   case EVENT_DTLB_MISS:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_DTLB, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS);
      break;
   case EVENT_DTLB_MISS_LD:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_DTLB, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_ACCESS);
      break;
   case EVENT_DTLB_MISS_ST:
      pet = PERF_TYPE_HW_CACHE;
      pe = CACHE_EVENT_CONF(PERF_COUNT_HW_CACHE_DTLB, PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_ACCESS);
      break;
   /*
    * Software Events
    */
   case EVENT_MISALIGN_MEM_REF:
      pet = PERF_TYPE_SOFTWARE;
      pe = PERF_COUNT_SW_ALIGNMENT_FAULTS;
      break;
   default:
      return PU_ERROR_EVENT_NOT_SUPPORTED;
   }
   if (pe_type)
      *pe_type = pet;
   if (perfEvent)
      *perfEvent = pe;
   return PU_SUCCESS;
}

static inline int perf_event_open(struct perf_event_attr *attr,
                       pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, attr, pid, cpu,
                   group_fd, flags);
}

void init_pe_attr(struct perf_event_attr &attr) {
   memset(&attr, 0, sizeof(struct perf_event_attr));
   attr.size = sizeof(struct perf_event_attr);
   attr.disabled = 1;
   attr.read_format = PERF_FORMAT_GROUP;
   attr.precise_ip = precise_ip;
}

// initialize to tprof default
void init_tprof_attr(struct perf_event_attr &attr) {
   init_pe_attr(attr);
   attr.type = PERF_TYPE_SOFTWARE;
   attr.config = PERF_COUNT_SW_CPU_CLOCK;
   attr.sample_freq = pmd->DefaultTprofRate;
   attr.freq = 1;

   attr.sample_type |= PERF_SAMPLE_IP;
   attr.sample_type |= PERF_SAMPLE_TID;
   attr.sample_type |= PERF_SAMPLE_TIME;
   attr.sample_type |= PERF_SAMPLE_READ;
   // wake up when 1 page of data is available
   attr.watermark = 1;
   attr.wakeup_watermark = 128; // make this small so we process it more often

   // mte stuff
   attr.sample_id_all = 1;
   attr.mmap = 1;
   attr.comm = 1;
   attr.task = 1;
   attr.mmap_data = 1;
   //attr.mmap2 = 1;
}

void init_mte_attr(struct perf_event_attr &attr) {
   init_pe_attr(attr);
    attr.type = PERF_TYPE_HARDWARE;
   attr.config = PERF_COUNT_HW_CPU_CYCLES;

   attr.sample_type |= PERF_SAMPLE_TID;
   attr.sample_type |= PERF_SAMPLE_TIME;
   attr.sample_type |= PERF_SAMPLE_READ;

   attr.sample_id_all = 1;
   attr.mmap = 1;
   attr.comm = 1;
   attr.task = 1;
   attr.mmap_data = 1;

   // wake up when 1 page of data is available
   attr.watermark = 1;
   attr.wakeup_watermark = pmd->PageSize;
}

// initialize to scs default
void init_scs_attr(struct perf_event_attr &attr) {
   init_pe_attr(attr);
   attr.type = PERF_TYPE_SOFTWARE;
   attr.config = PERF_COUNT_SW_CPU_CLOCK;
   attr.sample_freq = pmd->CurrentScsRate;
   attr.freq = 1;
   attr.exclude_kernel = 1;
   attr.exclude_hv = 1;

   attr.sample_type |= PERF_SAMPLE_IP;
   attr.sample_type |= PERF_SAMPLE_TID;
   attr.sample_type |= PERF_SAMPLE_TIME;
   attr.sample_type |= PERF_SAMPLE_READ;

   // wake up when any sample is received
   attr.watermark = 1;
   attr.wakeup_watermark = 1;
}

// initialize to ptt default
void init_ptt_attr(struct perf_event_attr &attr) {
   init_pe_attr(attr);
   attr.type = PERF_TYPE_SOFTWARE;
   attr.config = PERF_COUNT_SW_CPU_CLOCK;
}

int _IsPerfCounterEventSupported(int event_id) {
   int pe_type, pe_config;
   if (piEvent_to_perfEvent(event_id, &pe_type, &pe_config) < 0) {
      return false;
   }

   struct perf_event_attr attr;
   init_pe_attr(attr);
   attr.type = pe_type;
   attr.config = pe_config;
   attr.read_format = 0;
   int fd = perf_event_open(&attr, 0, -1, -1, 0);
   while (fd <= 0) {
      int err = errno;
      if (err == EINVAL) {
         if (attr.precise_ip == 0) {
            PDEBUG("attr.precise_ip=%d failed, giving up", attr.precise_ip);
            return false;
         }
         else {
            PDEBUG("attr.precise_ip=%d failed, trying with %d-1", attr.precise_ip, attr.precise_ip);
            attr.precise_ip--;
            fd = perf_event_open(&attr, 0, -1, -1, 0);
         }
      }
      else {
         return false;
      }
   }
   // Correct the precise_ip global variable if changed
   if (attr.precise_ip != precise_ip) {
      fprintf(stderr, "Changing precise_ip from %d to %d\n", precise_ip, attr.precise_ip);
      precise_ip = attr.precise_ip;
   }

   // perf event opened so event is available
   close(fd);
   return true;
}

namespace pi {
namespace pu {

int Pi_PerfEvent::eventsOpen = 0;
long Pi_PerfEvent::mmapMemSize = 0;

/*
 * perf_event mmap reader
 */

/*
 * _unring_mmap_data(perf_event mmap, temp buffer, buffer size)
 * *************** Internal ************************
 * This function will read the next unread section of the perf_event record buffer
 * If necessary, the records will be unwrapped and written to the provided buffer
 * ** does not update the data_tail
 * Returns the number of bytes read from the buffer
 */
int Pi_PerfEvent::unring_mmap_data(char *buffer, int buff_size) {
	PE_MMAP_PG *pe_mmap = (PE_MMAP_PG *)mappedmem;
   int read_size = pe_mmap->data_head - pe_mmap->data_tail;
   // if nothing left to read, return 0
   if (!read_size)
      return read_size;
   // read a maximum of the buffer size
   if(read_size > buff_size)
      read_size = buff_size;

   // postion to start reading from
   uint64_t mod_data_offset = pe_mmap->data_tail % (PE_DATA_SIZE);
   // the starting region of the data
   char *region_start = (char *)pe_mmap + PE_DATA_OFFSET;

   // The size of the data after the start and before the wrap
   uint64_t split = (PE_DATA_SIZE) - mod_data_offset;
   if ( split < read_size) {
      // the split is smaller than the read size meaning that the data has to read
      // in two parts. This is why we use this function
      // Read the split area up to the end of the mmap data region
      memcpy(buffer, region_start + mod_data_offset, split);
      // read the rest of the data which starts at the beginning of the mmap
      // data region
      memcpy(buffer + split, region_start, read_size - split);
   }
   else {
      // yay. no wrapping. very simple read
      memcpy(buffer, region_start + mod_data_offset, read_size);
   }

   return read_size;
}

/*
 * _get_next_record(unwrapped buffer, current record pointer,
 * 						bytes of data in the buffer)
 * ******************** Internal **************************
 * This reads your unwrapped record buffer and updates your record pointer
 * to point at the next available record.
 * If you pass in a null record pointer but the buffer has data then the
 * first record should start at the beginning of the buffer.
 * If there isn't enough data left in the buffer to hold the next record then
 * this will return 0 and unset the record pointer
 * Returns size of the record
 * Returns 0 if no more record left in the buffer
 */
int Pi_PerfEvent::get_next_record(char *buffer, PE_HEADER **rec_ptr, int bytes_left) {
   // case of a new buffer
   if (*rec_ptr == 0 && bytes_left) {
      *rec_ptr = (PE_HEADER *)buffer;
      return (*rec_ptr)->size;
   }

   int rec_size = (*rec_ptr)->size;
   char *ptr = (char *)*rec_ptr + rec_size;
   // not enough left in buffer for even a header
   if (ptr + sizeof(PE_HEADER) > buffer+bytes_left) {
      *rec_ptr = 0;
      return 0;
   }

   *rec_ptr = (PE_HEADER *)ptr;
   rec_size = (*rec_ptr)->size;
   // not enough left in the buffer for the whole record
   if ((char *)*rec_ptr + rec_size > buffer + bytes_left) {
      *rec_ptr = 0;
      return 0;
   }

   return rec_size;
}

int Pi_PerfEvent::read_sample_id(PE_HEADER *record, PE_ATTR &attr, uint32_t *pid, uint32_t *tid, uint64_t *time) {
   if (!attr.sample_id_all)
      return 0;

   char *ptr = (char *)record + record->size;
   if (attr.sample_type & PERF_SAMPLE_IDENTIFIER) {
      ptr -= sizeof(uint64_t);
   }
   if (attr.sample_type & PERF_SAMPLE_CPU) {
      ptr -= sizeof(uint32_t);
      // read res
      ptr -= sizeof(uint32_t);
      // read cpu
   }
   if (attr.sample_type & PERF_SAMPLE_STREAM_ID) {
      ptr -= sizeof(uint64_t);
   }
   if (attr.sample_type & PERF_SAMPLE_ID) {
      ptr -= sizeof(uint64_t);
   }

   if (attr.sample_type & PERF_SAMPLE_TIME) {
      ptr -= sizeof(uint64_t);
      if (time)
         *time = *(uint64_t *)ptr;
   }
   if (attr.sample_type & PERF_SAMPLE_TID) {
      ptr -= sizeof(uint32_t);
      if (pid) {
         *pid = *(uint32_t *)ptr;
      }
      ptr -= sizeof(uint32_t);
      if (tid) {
         *tid = *(uint32_t *)ptr;
      }
   }

   return 0;
}

int Pi_PerfEvent::process_perf_record(PE_HEADER *record, PE_ATTR &attr, int cpu, PiFacility *facility) {
   // FIXME: insert null checks for all facility members
   SampleProcessor *sp = facility->sampleProcessor;
   if (sp == 0) {
      // dont do anything with the data
      return 0;
   }

	int rec_size = record->size;
	char *ptr = (char *)record + sizeof(PE_HEADER);
	uint32_t pid, tid, tmp32;
	uint64_t addr, len, tmp64;
	std::string name;
	char *str = 0;

	SampleRecord sample;
	sample.isUserMode = (record->misc & PERF_RECORD_MISC_CPUMODE_MASK) == PERF_RECORD_MISC_USER;

	sample.timeStamp = get_timestamp();
	sample.cpu = cpu;
	switch (record->type) {
	case PERF_RECORD_MMAP:
		/*
		 * The MMAP events record the PROT_EXEC mappings so that we can
		 * correlate userspace IPs to code. They have the following structure:
		 *
		 * struct {
		 *	struct perf_event_header	header;
		 *
		 *	u32				pid, tid;
		 *	u64				addr;
		 *	u64				len;
		 *	u64				pgoff;
		 *	char				filename[];
		 * 	struct sample_id		sample_id;
		 * };
		 */
		if (record->misc & PERF_RECORD_MISC_MMAP_DATA)
			return 0;
		sample.pid = READ_RAW_INC(uint32_t, ptr);
		sample.tid = READ_RAW_INC(uint32_t, ptr);
		addr = READ_RAW_INC(uint64_t, ptr);
		len  = READ_RAW_INC(uint64_t, ptr);
		ptr += sizeof(uint64_t);
		str = (char *)ptr;
		name = std::string(str, strlen(str));
		if (strcmp(str, "//anon") != 0 || pmd->TraceAnonMTE) {
			// if the segment is anon mte then check if we allow it
			return facility->taskInfo->addModule(sample, addr, len, name);
		}
		break;
	case PERF_RECORD_LOST:
		/*
		 * struct {
		 *	struct perf_event_header	header;
		 *	u64				id;
		 *	u64				lost;
		 * 	struct sample_id		sample_id;
		 * };
		 */
		ptr += sizeof(uint64_t);
		tmp64  = READ_RAW_INC(uint64_t, ptr);
		// FIXME: Add stat collection
		//header = get_perfHeader(TRACE_TYPE, cpu);
		//header->datalosscount += tmp64;
		PI_PRINTK_ERR("Pid:%d, Cpu:%d ; Perf Data Lost:%ld records\n", epid, ecpu, tmp64);
		return 0;
		break;
	case PERF_RECORD_COMM:
		/*
		 * struct {
		 *	struct perf_event_header	header;
		 *
		 *	u32				pid, tid;
		 *	char				comm[];
		 * 	struct sample_id		sample_id;
		 * };
		 */
		sample.pid = READ_RAW_INC(uint32_t, ptr);
		sample.tid = READ_RAW_INC(uint32_t, ptr);
		str = (char *)ptr;
		len = strlen((const char *)ptr) + 1;
		ptr += len;
		name = std::string(str, len);
		//_read_sample_id(record, attr, 0, 0, &timeStamp);
		return facility->taskInfo->setProcessName(sample, name);
		break;
	case PERF_RECORD_EXIT:
		/*
		 * struct {
		 *	struct perf_event_header	header;
		 *	u32				pid, ppid;
		 *	u32				tid, ptid;
		 *	u64				time;
		 * 	struct sample_id		sample_id;
		 * };
		 */
		sample.pid = READ_RAW_INC(uint32_t, ptr);
		ptr += sizeof(uint32_t); // skip ppid
		sample.tid = READ_RAW_INC(uint32_t, ptr);
		ptr += sizeof(uint32_t); // skip ptid

		// We will no longer use the perf_event timestamps since they can be unreliable
		// we will for now use our own timestamp
		//timeStamp = READ_RAW_INC(uint64_t, ptr);

		// do not attempt to read the task files at this point. they are already gone
		// process_pid(pid, tid);

		// tell the event manager to remove the event
		if (facility->eventManager->getScope() == TS_PROCESS) {
		   PDEBUG("Pid:%d, Cpu:%d ; Task exited, closing event for tid: %d\n", epid, ecpu, sample.tid);
		   // do not try to read this event again. That can cause an infinite loop.
		   facility->eventManager->remove(sample.tid, sample.tid != epid);
		}
		return sp->taskEnd(sample); // FIXME: should go to task info?
		break;
	case PERF_RECORD_FORK:
		/*
		 * struct {
		 *	struct perf_event_header	header;
		 *	u32				pid, ppid;
		 *	u32				tid, ptid;
		 *	u64				time;
		 * 	struct sample_id		sample_id;
		 * };
		 */
		pid = READ_RAW_INC(uint32_t, ptr);
		tmp32 =  READ_RAW_INC(uint32_t, ptr); // read ppid
		tid = READ_RAW_INC(uint32_t, ptr);
		ptr += sizeof(uint32_t); // skip ptid
		//timeStamp = READ_RAW_INC(uint64_t, ptr);
		// since the pid was opened while recording then perf_events will provide
		// the MTE data. set this so that we do not try to process it ourselves
		// set_pid_processed(pid, tid);

		// tell the event manager to add a new event
      if (facility->eventManager->getScope() == TS_PROCESS && pmd->TraceOn) {
         PDEBUG("Pid:%d, Cpu:%d ; Fork record, starting new event for thread %d\n", epid, ecpu, tid);
         facility->eventManager->add((int)tid, pmd->TraceActive);
      }

      bool newProc;
		if (pid == tmp32) { // is a clone. same process id
		   sample.pid = pid;
		   sample.tid = tid;
         return facility->taskInfo->newThread(sample);
		}
		else { // is a fork. new process
		   sample.tid = pid;
		   sample.pid = tmp32;
         return facility->taskInfo->newProcess(sample);
		}
		break;
	case PERF_RECORD_SAMPLE:
		if (attr.sample_type & PERF_SAMPLE_IDENTIFIER) {
			ptr += sizeof(uint64_t);
		}
		if (attr.sample_type & PERF_SAMPLE_IP) {
			sample.address = READ_RAW_INC(uint64_t, ptr);
		}
		if (attr.sample_type & PERF_SAMPLE_TID) {
			sample.pid = READ_RAW_INC(uint32_t, ptr);
			sample.tid = READ_RAW_INC(uint32_t, ptr);
		}
		if (attr.sample_type & PERF_SAMPLE_TIME) {
			tmp64 = READ_RAW_INC(uint64_t, ptr);
		}
		facility->taskInfo->handleProcess(sample.pid, sample.tid);
		return sp->sample(sample);
		break;
	default:
		return 0;
	}
	return 0;
}

int Pi_PerfEvent::read_trace_data(bool force, int cpu, PiFacility *facility) {
	int rc;
	if (!fdisc)
	   return PU_ERR_NOT_INITIALIZED;

	PE_MMAP_PG *pe_mmap = (PE_MMAP_PG *)mappedmem;
	if (!pe_mmap)
		return PU_ERROR_BUFFER_NOT_MAPPED;

	uint64_t tail = pe_mmap->data_tail, head = pe_mmap->data_head;
	if( ((head - tail) < attr.wakeup_watermark) && !force)
		return 0;

	int num_rec = 0, bytes_read = 0;
	char buffer[MAX_RECORD_SIZE] = {0};
   int bytes_left = unring_mmap_data(buffer, MAX_RECORD_SIZE);
	while (bytes_left > 0) {
		PE_HEADER *rec_ptr = 0;
		int rec_size = get_next_record(buffer, &rec_ptr, bytes_left);
		while (rec_size > 0) {
			rc = process_perf_record(rec_ptr, attr, cpu, facility);
			if (rc < 0)
				goto read_stop;
			// finished with this record. increment tail
			pe_mmap->data_tail += rec_size;
			bytes_read += rec_size;
			num_rec++;
			// get next one from buffer if available
			rec_size = get_next_record(buffer, &rec_ptr, bytes_left);
		}

		// finished with this buffer, load up more data
		bytes_left = unring_mmap_data(buffer, MAX_RECORD_SIZE);
	}

read_stop:
	records += num_rec;
	if (num_rec)
		PDEBUG("Pid:%d, Cpu:%d, Records:%d, total:%ld, Bytes:%d, head:%ld, tail:%ld\n",
		      epid, ecpu, num_rec, records, bytes_read, head, tail);
	return rc;
}

Pi_PerfEvent::Pi_PerfEvent(PE_ATTR &pe_attr) {
   fdisc			= 0;
   mappedmem	= 0;
   mmap_size	= 0;
   records		= 0;
   epid = -1;
   ecpu = -1;
   memcpy(&attr, &pe_attr, sizeof(struct perf_event_attr));
}

Pi_PerfEvent::~Pi_PerfEvent() {
   close_fd();
}

int Pi_PerfEvent::init(pid_t pid, int cpu, int group_fd) {
   unsigned long flags = 0;
   epid = pid;
   ecpu = cpu;
   /*
    Causes issues in PTT, need to figure out what the right value
    for this is.

    if (group_fd <= 0)
    flags = 0; //PERF_FLAG_FD_NO_GROUP;
    else
    flags = PERF_FLAG_FD_OUTPUT;
    */
   PDEBUG("pid:%d, cpu:%d, group_fd:%d, flags:%lu, period/freq:%ld\n", pid, cpu,
         group_fd, flags, attr.sample_period);
   int fd = perf_event_open(&attr, pid, cpu, group_fd, flags);
   if (fd <= 0) {
      PDEBUG("Events open: %d, mmapped bytes: %ld\n", eventsOpen, mmapMemSize);
      return readSysError("Failed to open perf_event");
   }
   fdisc = fd;
   eventsOpen++;

   if (group_fd <= 0 && attr.sample_period) {
      mmap_size = PE_MMAP_SIZE;
      void *mmp = mmap(0, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if ((intptr_t) mmp <= 0) {
         PDEBUG("Events open: %d, mmapped bytes: %ld\n", eventsOpen,
               mmapMemSize);
         close_fd();
         return readSysError("Failed to map memory to perf_event");
      }
      mappedmem = mmp;
   }
   mmapMemSize += mmap_size;
   return PU_SUCCESS;
}

int		Pi_PerfEvent::ioctl_call(unsigned long int opcode, int arg) {
   TRACE_START("pid=%d, cpu=%d, opcode=%lu, arg=%d", epid, ecpu, opcode, arg);
   int rc = ioctl(fdisc, opcode, arg);
   if (rc != 0)
      return readSysError("perf_event ioctl failed:");
   return PU_SUCCESS;
}

int		Pi_PerfEvent::close_fd() {
   if (!fdisc)
      return 0;
   ioctl(fdisc, PERF_EVENT_IOC_DISABLE, 0);

   if(mappedmem) {
      munmap(mappedmem, mmap_size);
      mmapMemSize -= mmap_size;
   }
   mappedmem = 0;
   mmap_size = 0;

   close(fdisc);
   fdisc = 0;
   eventsOpen--;
   return 0;
}


Pi_PerfEvent_Group::Pi_PerfEvent_Group(pid_t inpid, int cpu, PE_ATTR &leader_attr) {
   pid			= inpid;
   cpu_num		= cpu;
   num_events	= 0;
   valid			= false;
   memset(events, 0, sizeof(events));
   if (leader_attr.size) {
      leader = new Pi_PerfEvent(leader_attr);
   }
   else
      leader = 0;
}

Pi_PerfEvent_Group::Pi_PerfEvent_Group() {
   pid         = -1;
   cpu_num     = -1;
   num_events  = 0;
   valid       = false;
   memset(events, 0, sizeof(events));
   leader = 0;
}

Pi_PerfEvent_Group::~Pi_PerfEvent_Group() {
   if (leader) {
      delete leader;
      leader = 0;
   }

   for (int i=0; i<num_events; i++) {
       delete events[i];
   }
   num_events = 0;
}

void     Pi_PerfEvent_Group::free_all_memory() {
   if (leader) {
      delete leader;
      leader = 0;
   }

   for (int i=0; i<num_events; i++) {
       delete events[i];
   }
   memset(events, 0, sizeof(events));
   num_events = 0;
}

int     Pi_PerfEvent_Group::init_leader(PE_ATTR &leader_attr) {
   if (leader_attr.size) {
      leader = new Pi_PerfEvent(leader_attr);
   }
   else
      leader = 0;
   return 0;
}

int		Pi_PerfEvent_Group::init_all() {
   if (!leader && !num_events)
      return PU_ERR_NOT_INITIALIZED;
   int rc, i;
   int fd = -1;
   if (leader) {
      rc = leader->init(pid, cpu_num, -1);
      RETURN_RC_FAIL(rc);
      fd = leader->fdisc;
   }
   for (i=0; i<num_events; i++) {
      rc = events[i]->init(pid, cpu_num, fd);
      RETURN_RC_FAIL(rc);
   }
   valid = true;
   return PU_SUCCESS;
}

int		Pi_PerfEvent_Group::add_child_pe(PE_ATTR &pe_attr) {
   if (num_events == PE_MAX_GROUP_EVENTS)
      return PU_ERROR_EVENT_MAX_EXCEEDED;
   if (!leader)
      return PU_ERR_NOT_INITIALIZED;

   void *ptr = malloc(sizeof(Pi_PerfEvent));
   if (!ptr)
      return PU_ERROR_NOT_ENOUGH_MEMORY;
   events[num_events] = new (ptr) Pi_PerfEvent(pe_attr);
   num_events++;
   return PU_SUCCESS;
}

int		Pi_PerfEvent_Group::ioctl_call(unsigned long int opcode, int arg) {
   int rc = 0;
   if (leader) {
      rc = leader->ioctl_call(opcode, arg);
      RETURN_RC_FAIL(rc);
   }

   for (int i=0; i < num_events; i++) {
      rc = events[i]->ioctl_call(opcode, arg);
      RETURN_RC_FAIL(rc);
   }
   return PU_SUCCESS;
}

int		Pi_PerfEvent_Group::close_fd() {
   int rc = 0;

   for (int i=0; i < num_events; i++) {
      rc = events[i]->close_fd();
      //RETURN_RC_FAIL(rc);
   }

   if (leader) {
      rc = leader->close_fd();
      //RETURN_RC_FAIL(rc);
   }
   valid = false;
   return PU_SUCCESS;
}


PiEventManager::PiEventManager() :
      epfd(0), read_fifo(0), write_fifo(0), scope(0), piFacility(0),
      readRequests(0) {
}

PiEventManager::~PiEventManager() {
	if (epfd)
		close(epfd);
	eventGroups.clear();
}

int	PiEventManager::init(int scope, PE_ATTR &leader_attr, PiFacility *facility) {
   piFacility = facility;
   attr = leader_attr;
	this->scope = scope;
	epfd = epoll_create1(0);

	int pipe_array[2];
	pipe2(pipe_array, O_NONBLOCK);
	struct epoll_event *newEpollEvent = (struct epoll_event *)malloc(sizeof(epoll_event));
	newEpollEvent->data.ptr = NULL;
	newEpollEvent->events = EPOLLIN | EPOLLRDHUP | EPOLLPRI | EPOLLERR;

	epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_array[0], newEpollEvent);
	write_fifo = pipe_array[1];
	read_fifo = pipe_array[0];

	return 0;
}

int	PiEventManager::add(std::vector<int> &keys, bool enable) {
   int rc = 0;
   for (int i = 0; i < keys.size(); i++) {
      rc = add(keys[i], enable, attr);
      RETURN_RC_FAIL(rc);
   }
   PDEBUG("Events open: %d, mmapped bytes: %ld\n", Pi_PerfEvent::eventsOpen,
          Pi_PerfEvent::mmapMemSize);
   return 0;
}

int   PiEventManager::add(int key, bool enable) {
   return add(key, enable, attr);
}

int	PiEventManager::add(int key, bool enable, PE_ATTR &leader_attr) {
   int rc = 0;
   if (eventGroups[key].leader == 0) {
      // group is new or not initialized
      if (scope == TS_SYSTEM) {
         eventGroups[key].cpu_num = key;
         eventGroups[key].pid = -1;
      }
      else {
         eventGroups[key].cpu_num = -1;
         eventGroups[key].pid = key;
      }

      rc = eventGroups[key].init_leader(leader_attr);
   }
   if (!eventGroups[key].valid) {
      rc = eventGroups[key].init_all();
      if (rc == PU_ERROR_THREAD_NOT_FOUND) {
         // threads can close before we start tracing, ignore the error
         // TODO log failure
         eventGroups.erase(key);
         return 0;
      }
      else if (rc < 0) {
         eventGroups.erase(key);
         return rc;
      }

      rc = registerGroup(key);
      RETURN_RC_FAIL(rc);
   }

   if (eventGroups[key].valid && enable) {
      eventGroups[key].ioctl_call(PERF_EVENT_IOC_ENABLE, 0);
   }

   return 0;
}

int	PiEventManager::remove(int key, bool read) {
   if (eventGroups[key].valid) {
      eventGroups[key].ioctl_call(PERF_EVENT_IOC_DISABLE, 0);
      unregisterGroup(key);

      if (read) {
         // read data first
         readRecords(key, true);
      }
   }
   removeList.push_back(key);
   return 0;
}

int	PiEventManager::clear() {
   if (epfd) {
      close(epfd);
      init(scope, attr, piFacility);
   }
   eventGroups.clear();
   return 0;
}

int	PiEventManager::readAllRecords(bool force) {
   for(Pi_PerfEvent_Group_Map::iterator iter = eventGroups.begin(); iter != eventGroups.end(); ++iter) {
      int rc = readRecords(iter->first, force);
      if (rc == PU_ERR_NOT_INITIALIZED && pmd->TraceOn) {
         PI_PRINTK_ERR("Error, tracing on but event in list is not initialized: %d\n", iter->first);
      }
      else RETURN_RC_FAIL(rc);
   }
   processRemoveList();
   return 0;
}

int   PiEventManager::readRecords(int key, bool force) {
   if (piFacility == 0)
      return PU_ERR_NOT_INITIALIZED;
   // TODO: register events after reading
   if (eventGroups[key].valid && eventGroups[key].leader) {
      int cpu = (scope == TS_SYSTEM)? key : 0;
      return eventGroups[key].leader->read_trace_data(force, cpu, piFacility);
   }
   else {
      return PU_ERR_NOT_INITIALIZED;
   }
}

int	PiEventManager::ioctl_call(unsigned long int opcode, int arg) {
   int rc;
   for(Pi_PerfEvent_Group_Map::iterator iter = eventGroups.begin(); iter != eventGroups.end(); ++iter) {
      if (iter->second.valid) {
         iter->second.ioctl_call(opcode, arg);
      }
   }
   return 0;
}

std::vector<int> PiEventManager::waitOnData(uint32 timeout_millis, int max_events) {
   std::vector<int> keys;
   struct epoll_event *events = new struct epoll_event [max_events];

   int rc = 0;
   do {
      rc = epoll_wait((int)epfd, events, max_events, timeout_millis);
      // sometimes get interrupted, just retry
   } while (rc < 0 && errno == EINTR);
   if (rc < 0) {
      readSysError("Error polling on perf_events!");
   }
   else if (rc > 0) {
      for (int i=0; i < rc; i++) {
         if (events[i].data.ptr) {
            Pi_PerfEvent_Group_ptr pegp = (Pi_PerfEvent_Group_ptr)events[i].data.ptr;
            int key = (scope == TS_SYSTEM)? pegp->cpu_num : pegp->pid;
            keys.push_back(key);
         }
      }
   }
   delete [] events;
   return keys;
}

int PiEventManager::waitAndReadData(uint32 timeout_millis, int maxEvents) {
   int rc = 0;
   readRequests++;
   if ((readRequests % 1000) == 0) {
      return readAllRecords(true);
   }
   std::vector<int> keys = waitOnData(timeout_millis, maxEvents);
   for (int i=0; i < keys.size(); i++) {
      rc = readRecords(keys[i], false);
      // TODO: reset epoll for unread events
      if (rc == PU_ERR_NOT_INITIALIZED && pmd->TraceOn) {
         PI_PRINTK_ERR("Error, tracing on but event in list is not initialized: %d\n", keys[i]);
      }
      else RETURN_RC_FAIL(rc);
   }
   processRemoveList();
   return keys.size();
}

void  PiEventManager::discardData() {
   for(Pi_PerfEvent_Group_Map::iterator iter = eventGroups.begin(); iter != eventGroups.end(); ++iter) {
      if (iter->second.valid) {
         PE_MMAP_PG *pe_mmap = (PE_MMAP_PG *) iter->second.leader->mappedmem;
         pe_mmap->data_tail = pe_mmap->data_head;
      }
   }
}

int PiEventManager::registerGroup(int key) {
   TRACE_START("%d", key);
   // TODO: make events unregister automatically when returned from epoll
   struct epoll_event newEpollEvent;

   newEpollEvent.data.ptr = &eventGroups[key];
   newEpollEvent.events = EPOLLIN | EPOLLRDHUP | EPOLLPRI | EPOLLERR;
   int rc = epoll_ctl(epfd, EPOLL_CTL_ADD, eventGroups[key].leader->fdisc, &newEpollEvent);
   if (rc < 0) {
      rc = readSysError("Failed to register epoll event");
   }
   return rc;
}

int PiEventManager::unregisterGroup(int key) {
   epoll_ctl(epfd, EPOLL_CTL_DEL, eventGroups[key].leader->fdisc, NULL);
   return 0;
}

int PiEventManager::processRemoveList() {
   if (removeList.size()) {
      PDEBUG("Processing remove list, removing %d events\n", removeList.size());
      for (int i=0; i < removeList.size(); i++) {
         eventGroups.erase(removeList[i]);
      }
      removeList.clear();
      PDEBUG("Events open: %d, mmapped bytes: %ld\n", Pi_PerfEvent::eventsOpen,
             Pi_PerfEvent::mmapMemSize);
   }
   return 0;
}

void PiEventManager::validate(std::vector<int> &keys) {
   // make sure all keys are added
   add(keys, true);
   // make sure all the existing events should exist
   for(Pi_PerfEvent_Group_Map::iterator iter = eventGroups.begin(); iter != eventGroups.end(); ++iter) {
      // TODO: check if key exists in keys
   }
}

}
}
