/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2010
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

#include"NRM2SampleProcessor.hpp"
#include"perfutil_core.hpp"
#include"utils/pi_time.h"

#define PI_LINUX	0x2000

const char DEF_NRM[] = "NRM2";
const uint32_t DEF_ENDIAN = 0x04030201; // dont know what this is for
const char DEF_CR = DEFINED_CRETURN; // or this

namespace pi {
namespace pu {

NRM2SampleProcessor::NRM2SampleProcessor() {
   cont_section_size = PI_SECTION_DEFAULT;
   MTE_processed = 0;
   memset(ptr_btrace_buffers, 0, 128*sizeof(void *));
   ptr_mte_buffers = 0;
   pi_tracefile = 0;
}

NRM2SampleProcessor::~NRM2SampleProcessor() {
   file_close();
   free_hook_buffers();
}

int NRM2SampleProcessor::initialize(std::string &name, PiFacility *facility) {
   piFacility = facility;
   fileName = name;
   return PITRACE_InitBuffer(PI_TRACE_BUF_DEFAULT);
}

int NRM2SampleProcessor::startedRecording() {
   int rc = 0;
   // moved here so that file can be reopened
   switch (pmd->TraceMode) {
      case COLLECTION_MODE_CONTINUOS:
         rc = init_continuos_mode();
         RETURN_RC_FAIL(rc);
         break;
      default:
         PI_PRINTK_ERR("Unknown trace mode %d\n", pmd->TraceMode);
         return(PU_FAILURE);
   }
   PI_PRINTK("driver_info.mte_type: %d\n", pmd->mte_type);

   rc = SoftTraceOn();
   if (rc != 0) {
      PI_PRINTK_ERR("ERROR SoftTraceStartOn()\n");
   }
   return rc;
}

int NRM2SampleProcessor::stoppedRecording() {
   // write out anything smaller than the section size
   int rc = finish_reading();

   // close the norm 2 file
   rc = file_close();
   return rc;
}

int NRM2SampleProcessor::terminate() {
   /* If buffer exists */
   return free_hook_buffers();
}

int NRM2SampleProcessor::taskName(RecordHeader &header, std::string &tname) {
   return write_pidname_hook(header.cpu, header.timeStamp, header.tid, (char*)tname.c_str());
}

int NRM2SampleProcessor::taskStart(RecordHeader &header, bool newProc) {
   bool isClone = !newProc;
   return write_fork_hook(header.cpu, header.timeStamp, header.tid, header.pid, isClone);
}

int NRM2SampleProcessor::taskEnd(RecordHeader &header) {
   return write_task_exit(header.cpu, header.timeStamp, header.tid);
}

int NRM2SampleProcessor::memorySegment(RecordHeader &header, uint64_t address, uint64_t size, std::string &name) {
   return write_segment_hook(header.cpu, header.timeStamp, header.pid, address, size, (char*)name.c_str());
}

int NRM2SampleProcessor::symbol(RecordHeader &header, uint64_t moduleId, uint64_t address, uint64_t size, std::string &name) {
   return 0;
}

int NRM2SampleProcessor::sample(SampleRecord &sample) {
   return write_tprof_tick(sample.cpu, sample.timeStamp, sample.pid, sample.tid, sample.address, sample.isUserMode);
}


//
// bytes_in_buffer()
// *****************
//
uint32_t bytes_in_buffer(PERFHEADER * ptr_buf) {
    uint32 num_bytes, rd, wr;
    PERFHEADER * PerfHeader;

    PerfHeader = ptr_buf;
    wr = PerfHeader->buffer_current;
    rd = PerfHeader->buffer_rd_offset;

    if (rd < wr) {
        num_bytes = wr - rd;
        return (num_bytes);
    }

    if (rd > wr) {
        num_bytes = wr - PerfHeader->buffer_start + PerfHeader->buffer_varend
                    - rd;
        return (num_bytes);
    }

    // else rd = wr - the buffer is either empty or full

    if (PerfHeader->rd_behind_wr)  // empty
        return (0);

    if (PerfHeader->wrap_rd)
        return (0);

    // else full

    return (PerfHeader->buffer_start - PerfHeader->buffer_end);
}

/*
 * has_data_to_write(perf buffer header)
 * *********** Internal ******************
 * If the buffer has more than a predefined amount of data in it then write it
 * to the return true
 * This threshold controls the ratio of headers and data in the nrm2 file
 * If it is too low, the nrm2 file size will grow as we must keep rewriting the header
 * to the file.
 */
bool NRM2SampleProcessor::has_data_to_write(PERFHEADER * ptr_buf) {
	if(!ptr_buf)
		return false;
	return bytes_in_buffer(ptr_buf) >= cont_section_size;
}

//
// init_trace_header()
// ******************
// Initializes constant trace header fields
//
void init_trace_header(PERFHEADER * PerfHeaderPtr, int type, int cpu,
                       int cont_section_size) {
   unsigned char * cptr;

   if (PerfHeaderPtr == NULL)
      return;

   cptr = (unsigned char *) PerfHeaderPtr;
   memcpy(cptr, DEF_NRM, 4);

   PerfHeaderPtr->cpu_no = cpu;
   PerfHeaderPtr->num_cpus = pmd->InstalledCPUs;
   PerfHeaderPtr->datalosscount = 0;
   PerfHeaderPtr->variable_timer_tick = 0; // nobody uses this i guess

   PerfHeaderPtr->collection_mode = COLLECTION_MODE_MTE_END | pmd->TraceMode;
   PerfHeaderPtr->inston = pmd->ItraceInstOn;
   PerfHeaderPtr->cr = DEF_CR;
   PerfHeaderPtr->header_size = PERFHEADER_BUFFER_SIZE;
   PerfHeaderPtr->Metric_cnt = 1;
   PerfHeaderPtr->endian = DEF_ENDIAN;

   PerfHeaderPtr->OS_type = LINUX64_FLAG;

   // Set the offsets

   PerfHeaderPtr->buffer_start = PERFHEADER_BUFFER_SIZE;
   PerfHeaderPtr->buffer_end = PerfHeaderPtr->buffer_size;
   PerfHeaderPtr->buffer_current = PERFHEADER_BUFFER_SIZE;
   PerfHeaderPtr->buffer_oldest = 0;
   PerfHeaderPtr->buffer_varend = PerfHeaderPtr->buffer_end;
   PerfHeaderPtr->buffer_rd_offset = PERFHEADER_BUFFER_SIZE;
   PerfHeaderPtr->data_size = cont_section_size;
   PerfHeaderPtr->rd_behind_wr = 1;

   PerfHeaderPtr->record_count = 0;
   PerfHeaderPtr->records_written = 0;
}

//
// ContTraceWrite()
// ****************
// SoftTraceRawWrite for continuos trace mode
int NRM2SampleProcessor::ContTraceWrite(PI_HOOK_HEADER *rawData, uint16_t rawDataLen, int type, int cpu) {
retry:
    int rc = PU_ERROR_BUFFER_FULL;
    PERFHEADER * PerfHeader = 0;
    uint8_t * pBuffer = 0;
    int trecsz = 0;
    union rdval temps;
    unsigned long flags;
    int wr = 0;
    int rd = 0;
    int num_bytes;

    // interrupts are disabled, so no kernel preemption

    PerfHeader = get_perfHeader(type, cpu);

    if ((rawDataLen == 0) || (rawData == 0)) {
        return PU_ERR_NULL_PTR;
    }

    if (PerfHeader == 0)
        return PU_ERROR_NO_BUFFER;

    trecsz = rawDataLen;

    // Now we know the record size - can we write it to the buffer?
    // First check whether it is too big to fit into the empty buffer
    if (trecsz > (PerfHeader->buffer_end - PerfHeader->buffer_start)) {
        rc = PU_ERROR_INSUFFICIENT_BUFFER;
        goto data_lost;
    }

    // We may have enough space, proceed
    // Get the buffer offsets & current pointer

    wr = PerfHeader->buffer_current;
    rd = PerfHeader->buffer_rd_offset;

    if ((wr == rd) && (PerfHeader->rd_behind_wr == 0)
            && (PerfHeader->wrap_rd == 0)) {

        goto data_lost;
    }

    if (wr >= rd) {
        // Can we fit after wr?
        // Note that wr = rd means empty buffer -
        // the full buffer case is checked for above

        num_bytes = wr - rd;

        if (trecsz <= PerfHeader->buffer_end - wr) {
            goto ok_to_write;
        }

        // If not after wr, can we wraparound?
        if (trecsz <= rd - PerfHeader->buffer_start) {
            PerfHeader->buffer_varend = wr;	 // end of the last trace record + 1
            wr = PerfHeader->buffer_start;
            PerfHeader->wrap_rd = 0;
            goto ok_to_write;
        }

        // Cannot write
        goto data_lost;
    }
    // else wr < rd

    if (trecsz > rd - wr) {
        goto data_lost;
    }

    num_bytes = wr - PerfHeader->buffer_start + PerfHeader->buffer_varend - rd;

ok_to_write:
    pBuffer = (uint8_t *) PerfHeader + wr;

    rawData->typeLength &= RAWFMT_HOOK_MASK;
	rawData->typeLength |= (rawDataLen & RAWFMT_HOOK_LEN_MASK);

	PerfHeader->record_count++;
	/* now update the timestamp - always 0 since post will handle it
	 * ************* I do not know what that means
	 * This was a very rough way of pointing at hook.timeStamp
	 */
	if (!rawData->timeStamp)
		rawData->timeStamp = get_timestamp();
	if (rawData->timeStamp > PerfHeader->time)
		PerfHeader->time = rawData->timeStamp;

    /* now copy the raw data to the trace buffer */
    memcpy(pBuffer, rawData, rawDataLen);

    // update buffer_current to the new value
    PerfHeader->buffer_current = wr + trecsz;

    if (rd < PerfHeader->buffer_current) {
        PerfHeader->rd_behind_wr = 1;
    } else {
        if (rd > PerfHeader->buffer_current)
            PerfHeader->rd_behind_wr = 0;
    }

    if (has_data_to_write(PerfHeader)) {
       pi_consume(type, cpu);
    }

    return (PU_SUCCESS);

data_lost:

   if (rc == PU_ERROR_BUFFER_FULL) {
      PDEBUG("emptying full buffer");
      rc = pi_consume(type, cpu);
      if (rc < 0) {
         PDEBUG("consume failed\n");
      }
      else {
         goto retry;
      }
   }

    if (rd < wr)
        PerfHeader->rd_behind_wr = 1;
    else if (rd > wr)
        PerfHeader->rd_behind_wr = 0;

    //++PerfHeader->datalosscount;
    // pi_wakeup_if_sleep(); // dont have this yet

    return (rc);
}

//
// MTEWrite()
// **********
// MTERawWrite wrapper
// returns error code or number of retries
//
int NRM2SampleProcessor::MTEWrite(PI_HOOK_HEADER *rawData, uint16_t rawDataLen, int cpu) {
    int rc;

    rc = ContTraceWrite(rawData, rawDataLen, pmd->mte_type, cpu);

    return (rc);
}

int NRM2SampleProcessor::write_raw_hook(PI_HOOK_HEADER *rawData, uint16_t rawDataLen, UINT32 type, int cpu) {
	if (MTE_TYPE == type || (type == 0 && rawData->majorCode == MTE_MAJOR))
		return MTEWrite(rawData, rawDataLen, cpu);
	else
		return ContTraceWrite(rawData, rawDataLen, type, cpu);
}

int NRM2SampleProcessor::write_RAW64_HOOK(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                     UINT64 time, UINT64 intvals[], int num_ints) {
    if (num_ints > NUM_RAWINT64)
        num_ints = NUM_RAWINT64;
    if (num_ints < 0)
        return PU_ERROR_INVALID_PARAMETER;

    int rc;
    RAWINT_HOOK_64BIT trace_data;
    int size = (num_ints * sizeof(UINT64));

    trace_data.header.typeLength = PERF_64BIT;
    trace_data.header.majorCode = major;
    trace_data.header.minorCode = minor;
    trace_data.header.timeStamp = time;

    memcpy(&trace_data.intval, intvals, size);

    size += HOOK_HEADER_SIZE;
    if (MTE_TYPE == type)
        rc = MTEWrite((PI_HOOK_HEADER *) &trace_data, size, cpu);
    else
        // TRACE_TYPE
        rc = ContTraceWrite((PI_HOOK_HEADER *) &trace_data, size, type, cpu);
    return (rc);
}

//
// PerfTraceHookSimple1
// ********************
//
// 1 UINT32 value
//
int NRM2SampleProcessor::PerfTraceHookSimple1(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                         int64_t timeStamp, UINT64 u1) {
    int num_ints = 1;
    UINT64 intval[num_ints];
    intval[0] = u1;
    return write_RAW64_HOOK(type, cpu, major, minor, timeStamp, intval,
                            num_ints);
}

//
// PerfTraceHookSimple2
// ********************
//
// 2 UINT32 values
//
int NRM2SampleProcessor::PerfTraceHookSimple2(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                         int64_t timeStamp, UINT64 u1, UINT64 u2) {
    int num_ints = 2;
    UINT64 intval[num_ints];
    intval[0] = u1;
    intval[1] = u2;
    return write_RAW64_HOOK(type, cpu, major, minor, timeStamp, intval,
                            num_ints);
}

//
// PerfTraceHookSimple3
// ********************
//
// 3 UINT32 values
//
int NRM2SampleProcessor::PerfTraceHookSimple3(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                         int64_t timeStamp, UINT32 u1, UINT32 u2, UINT32 u3) {
    int num_ints = 3;
    UINT64 intval[num_ints];
    intval[0] = u1;
    intval[1] = u2;
    intval[2] = u3;
    return write_RAW64_HOOK(type, cpu, major, minor, timeStamp, intval,
                            num_ints);
}

//
// PerfTraceHookSimple4
// ********************
//
// 4 UINT32 values
//
int NRM2SampleProcessor::PerfTraceHookSimple4(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                         int64_t timeStamp, UINT64 u1, UINT64 u2, UINT64 u3,
                         UINT64 u4) {
    int num_ints = 4;
    UINT64 intval[num_ints];
    intval[0] = u1;
    intval[1] = u2;
    intval[2] = u3;
    intval[3] = u4;
    return write_RAW64_HOOK(type, cpu, major, minor, timeStamp, intval,
                            num_ints);
}

//
// PerfTraceHookSimple5
// ********************
//
// 5 UINT32 values
//
int NRM2SampleProcessor::PerfTraceHookSimple5(UINT32 type, int cpu, UINT32 major, UINT32 minor,
                         int64_t timeStamp, UINT64 u1, UINT64 u2, UINT64 u3,
                         UINT64 u4, UINT64 u5) {
    int num_ints = 5;
    UINT64 intval[num_ints];
    intval[0] = u1;
    intval[1] = u2;
    intval[2] = u3;
    intval[3] = u4;
    intval[4] = u5;
    return write_RAW64_HOOK(type, cpu, major, minor, timeStamp, intval,
                            num_ints);
}

//
// write_segment_hooks()
// *********************
// Writes process mem map segment
//
int NRM2SampleProcessor::write_segment_hook(int cpu, int64_t timeStamp, int pid, uint64_t addr, int size, char *name) {
    uint32_t hook_size = 0;
    MTE_HOOK hook;

    hook.header.majorCode = MTE_MAJOR;
    hook.header.minorCode = MTE_MINOR;
    hook.header.timeStamp = timeStamp;
    hook.int_cnt = 5;
    hook.pid = pid;
    hook.NT_cs = 0;
    hook.NT_ts = 0;
    hook.addr = addr;
    hook.code_size = size;

    if (name) {
    	hook.str_len = strlen(name);
		if (hook.str_len > HOOK_STRLEN_MAX) {
			PDEBUG("String too long for hook! %d %s \n", hook.str_len, name);
			hook.str_len = HOOK_STRLEN_MAX;
		}
    	memcpy(hook.str_val, name, hook.str_len);
    } else {
    	hook.str_len = 0;
    }

	hook_size = hook.str_len % 8;
	if (hook_size > 0)
		hook_size =  8 - hook_size;
	hook_size += MTE_HOOK_SIZE + hook.str_len;
	hook.header.typeLength = PERF_MIXED_DATA | hook_size;

    return MTEWrite((PI_HOOK_HEADER *)&hook, hook_size, cpu);
}

//
// write_pidname_hook()
// ********************
// Write 19/34 hook
//
int NRM2SampleProcessor::write_pidname_hook(int cpu, int64_t timeStamp, int pid, char *name) {
    uint32_t hook_size = 0;
    PID_NAME_HOOK hook;

	hook.header.majorCode = MTE_MAJOR;
	hook.header.minorCode = MTE_PIDNAME_MINOR;
	hook.header.timeStamp = timeStamp;
	hook.pid = pid;
	hook.int_cnt = 1;

	if (name) {
		hook.str_len = strlen(name);
		if (hook.str_len > HOOK_STRLEN_MAX) {
			PDEBUG("String too long for hook! %d %s \n", hook.str_len, name);
			hook.str_len = HOOK_STRLEN_MAX;
		}
		memcpy(hook.str_val, name, hook.str_len);
	} else {
		hook.str_len = 0;
	}

	hook_size = hook.str_len % 8;
	if (hook_size > 0)
		hook_size =  8 - hook_size;
	hook_size += PID_NAME_HOOK_SIZE + hook.str_len;
	hook.header.typeLength = PERF_MIXED_DATA | hook_size;

	return MTEWrite((PI_HOOK_HEADER *)&hook, hook_size, cpu);
}

//
// write_fork_hook()
// ****************
// Write 19/63 hook.
//
int NRM2SampleProcessor::write_fork_hook(int cpu, int64_t timeStamp, int pid, int ppid, bool isClone) {
    FORK_HOOK hook;

    hook.header.typeLength = PERF_64BIT;
    hook.header.majorCode = MTE_MAJOR;
    hook.header.minorCode = MTE_CLONE_FORK_MINOR;
	hook.header.timeStamp = timeStamp;
    hook.pid = pid;
    hook.ppid = ppid;
    if (isClone)
    	hook.clone_flags = PI_FT_CLONE;
    else
    	hook.clone_flags = PI_FT_FORK;

    return MTEWrite((PI_HOOK_HEADER *) &hook, FORK_HOOK_SIZE, cpu);
}

//
// task_exit_notify
// ****************
// Task is unloading
// walk the parent tree until we find a "known" pid
// walk back dumping the "unknown" parent tree (19 63) hooks
// dump the pid name and MTE info for the exiting task.
//
int NRM2SampleProcessor::write_task_exit(int cpu, int64_t timeStamp, int pid) {
    PerfTraceHookSimple1(pmd->mte_type, cpu, MTE_MAJOR, MTE_EXIT_START_MINOR, timeStamp, pid);

    return PerfTraceHookSimple1(pmd->mte_type, cpu, MTE_MAJOR, MTE_EXIT_END_MINOR, timeStamp, pid);
}

/*
 * Writes the tprof tick
 */
int NRM2SampleProcessor::write_tprof_tick(int cpu, int64_t timeStamp, int pid, int tid, uint64_t addr, bool isUserMode) {
    TPROF_HOOK tdata;

    tdata.header.typeLength = PERF_64BIT;
    tdata.header.majorCode = TPROF_MAJOR;
    tdata.header.minorCode = TPROF_MINOR;
    if (isUserMode)
    	tdata.header.minorCode |= 3; // based off kernel macro user_mode(regs)
    tdata.header.timeStamp = timeStamp;
    tdata.pid = pid;
    tdata.tid = tid;
    tdata.addr = addr;

    return ContTraceWrite((PI_HOOK_HEADER *) &tdata, TPROF_HOOK_SIZE, TRACE_TYPE, cpu);
}

//
// SoftTraceOn()
// *************
// ENTRY: None
// EXIT:  return code.
//
int NRM2SampleProcessor::SoftTraceOn(void) {
	PDEBUG("start\n");
    char kname[] = "vmlinux"; // should find a better way to do this
    uint32_t hook_size = 0;
    MTE_HOOK mdata;
    char * buffer;
    int tsec, minoff, dst;

    if (!is_perf_buf(0))
        return (PU_FAILURE);

    //
    // Write startup hooks
    //

    get_time(&tsec, &minoff, &dst);
    int64_t timeStamp = get_timestamp();

    FOR_EACH_CPU(cpu) {
    	PERFHEADER *pheader = get_perfHeader(TRACE_TYPE, cpu);
    	pheader->start_time = timeStamp;

    	pheader = get_perfHeader(MTE_TYPE, cpu);
    	pheader->start_time = timeStamp;
    }

    write_segment_hook(0,timeStamp, 0, 0, 0, kname);

    // System start hook
	PerfTraceHookSimple4(pmd->mte_type, 0, SYS_INFO_MAJOR,
	                     SYSINFO_CPUSPEED_MINOR, timeStamp, pmd->CpuSpeed / 1000, // in MHz
	        minoff, dst, tsec);

    // OS type
    PerfTraceHookSimple1(pmd->mte_type, 0, SYS_INFO_MAJOR,
                         SYSINFO_TIME_BASE_MINOR, timeStamp, 1000000000);

    // OS type
    PerfTraceHookSimple1(pmd->mte_type, 0, SYS_INFO_MAJOR,
                         SYSINFO_OS, timeStamp, PI_LINUX);

    // tprof info hooks
        int i;

	PerfTraceHookSimple3(pmd->mte_type, 0,  SYS_INFO_MAJOR, SYSINFO_TPROF_MODE, timeStamp,  // Minor
									 pmd->TprofMode,                  // UI1: TprofMode
									 pmd->CurrentTprofRate,
									 0);  // UI3: Unused // why then?

	if (pmd->TprofMode != TPROF_MODE_TIME)  {  // event mode
		FOR_EACH_CPU(i) {
			PerfTraceHookSimple5(pmd->mte_type, 0, SYS_INFO_MAJOR,     // Major
					SYSINFO_TPROF_EVENT_INFO, timeStamp,    // Minor
								 i,                            // UI1:CPU number
								 pmd->TprofCtrEvent,         // UI2: Event number
								 0,             // UI3: Perf counter number
								 0,           // UI4: Perf counter MSR
								 0);  // UI5: Perf counter contrl MSR
		}
	}

    // Trace interval id
    //pmd->TraceIntervalId++;
    PerfTraceHookSimple1(pmd->mte_type, 0, SYS_INFO_MAJOR,
                         SYSINFO_INTERVAL_ID, timeStamp, pmd->TraceIntervalId);

    // End of startup hooks
    PerfTraceHookSimple1(pmd->mte_type, 0, SYS_INFO_MAJOR,
                         SYSINFO_END_MINOR, timeStamp, 0);


    MTE_processed = 0;

    PDEBUG("finish\n");
    return PU_SUCCESS;
}


//
// pi_file_write()
// ***************
//
int NRM2SampleProcessor::pi_file_write(char *data, int data_size, PERFHEADER* pheader) {
    int num_bytes;
    int written_bytes = 0;
    int rc = data_size;
    int ret;

    while (written_bytes < data_size) {
        if (PERFHEADER_BUFFER_SIZE < data_size - written_bytes)
            num_bytes = PERFHEADER_BUFFER_SIZE;
        else
            num_bytes = data_size - written_bytes;

        ret = fwrite(data + written_bytes, 1, num_bytes, pi_tracefile);
        if (ret != num_bytes) {
            rc = PU_FAILURE;
            break;
        }
        if (pheader != NULL) {
            if (pheader->buffer_rd_offset + num_bytes
                    == pheader->buffer_varend) {
                pheader->buffer_rd_offset = pheader->buffer_start;
                pheader->wrap_rd = 1;
            } else {
                pheader->buffer_rd_offset += num_bytes;
            }
        }
        written_bytes += num_bytes;
    }

    return (rc);
}

//
// pi_consume()
// *************
//
int NRM2SampleProcessor::pi_consume(int type, int cpu)
{
	PERFHEADER  * PerfHeader = NULL;
	char	     * pBuffer    = NULL;
	int           wr, rd;
	int           bytes_to_write;

	// pi_consume writes cont_section_size to pi_tracefile
	// When called, we know there is at least that much in the buffer
	// and pi_tracefile is opened in swtrace init ioctl

	PerfHeader    = get_perfHeader(type, cpu);

	// Write section header

	pBuffer = (char *)PerfHeader;
	if (pi_file_write(pBuffer, PERFHEADER_BUFFER_SIZE, NULL) < 0)
		return(PU_FAILURE);

	// Write section data

	wr = PerfHeader->buffer_current;
	rd = PerfHeader->buffer_rd_offset;

	PDEBUG("type:%d, cpu: %d, wr:%d, rd: %d, end: %d, size: %d, records: %d\n",
	        type, cpu, wr, rd, PerfHeader->buffer_end, PerfHeader->data_size,
	        PerfHeader->record_count);

	pBuffer = (char *)PerfHeader + rd;

	if (wr > rd) {
		// Writer ahead of the reader - the most common case
		if (pi_file_write(pBuffer, PerfHeader->data_size, PerfHeader) < 0)
			return(PU_FAILURE);
	} else {
		// Reader ahead of writer
		if ((PerfHeader->buffer_varend - rd) >= PerfHeader->data_size) {
			// whole section fits
			if (pi_file_write(pBuffer, PerfHeader->data_size, PerfHeader) < 0)
				return(PU_FAILURE);
		} else {
			// section wraps
			// write till the varend
			if (pi_file_write((char *)pBuffer, PerfHeader->buffer_varend - rd,
			                  PerfHeader) < 0)
				return(PU_FAILURE);

			// write the rest from the start
			pBuffer        = (char *)PerfHeader + PerfHeader->buffer_start;
			bytes_to_write = PerfHeader->data_size - (PerfHeader->buffer_varend - rd);
			if (pi_file_write((char *)pBuffer, bytes_to_write, PerfHeader) < 0)
				return(PU_FAILURE);
		}
	}
	PerfHeader->records_written += PerfHeader->record_count;
	PerfHeader->record_count = 0;

	return(PU_SUCCESS);
}

//
// write_trace_buffer()
// ********************
// It is the caller's responsibility to check for driver and mmap
//
int NRM2SampleProcessor::write_buffer_final(PERFHEADER* PerfHeader) {
	UCHAR * ptr_trace_buffer = (UCHAR *) PerfHeader;
	UINT32 data_size, temp_size;
	int wr, rd;

	//
	// Write the section header
	//

	data_size = PerfHeader->data_size;
	wr = PerfHeader->buffer_current;
	rd = PerfHeader->buffer_rd_offset;
	PDEBUG("type:%d, cpu: %d, wr:%d, rd: %d, end: %d, size: %d, records: %d, total: %d \n",
	       PerfHeader->section_type, PerfHeader->cpu_no, wr, rd,
	       PerfHeader->buffer_end, PerfHeader->data_size, PerfHeader->record_count,
	       PerfHeader->record_count + PerfHeader->records_written);

	// For wraparound mode,
	// we may need to add the size of the fake A8 hook to PerfHeader->data_size
	// and to write the fake hook before other data

	if (fwrite((void *) ptr_trace_buffer, 1, PERFHEADER_BUFFER_SIZE,
			pi_tracefile) != PERFHEADER_BUFFER_SIZE) {
		PI_PRINTK_ERR("ERROR writing trace file\n");
		return (PU_FAILURE);
	}

	//
	// Write the section data
	//

	if (wr > rd) {
		// Writer ahead of the reader - just write
		temp_size = wr - rd;
		if (temp_size != data_size) {
			PI_PRINTK_ERR("ERROR: data size in the buffer is not correct\n");
			return (PU_FAILURE);
		}
		if (fwrite((void *) (ptr_trace_buffer + rd), 1, temp_size, pi_tracefile)
		        != temp_size) {
			PI_PRINTK_ERR("ERROR writing trace file\n");
			return (PU_FAILURE);
		}
	}
	else {                            // wr <= rd
		if ((data_size == 0) && (wr == rd))   // An empty buffer, do nothing
			return 0;

		// Reader ahead of writer - wraparound
		temp_size = (wr - PerfHeader->buffer_start)
		        + (PerfHeader->buffer_varend - rd);

		if (temp_size != data_size) {
			PI_PRINTK_ERR("ERROR: data size in the buffer is not correct\n");
			return (PU_FAILURE);
		}

		if (fwrite((void *) (ptr_trace_buffer + rd), 1,
		        PerfHeader->buffer_varend - rd, pi_tracefile)
		        != (PerfHeader->buffer_varend - rd)) {
			PI_PRINTK_ERR("ERROR writing trace file\n");
			return (PU_FAILURE);
		}
		if (fwrite((void *) (ptr_trace_buffer + PerfHeader->buffer_start), 1,
		        wr - PerfHeader->buffer_start, pi_tracefile)
		        != (wr - PerfHeader->buffer_start)) {
			PI_PRINTK_ERR("ERROR writing trace file\n");
			return (PU_FAILURE);
		}
	}
	return (PU_SUCCESS);
}


//
// finish_reading()
// ****************
// Called when shutting down the tracing functionality
// This write out all remaining data in the hook buffers
//
int NRM2SampleProcessor::finish_reading(void) {
   PERFHEADER * PerfHeader = NULL;
   unsigned int i;
   int bytes;

   // Write buf_size to the headers
   FOR_EACH_CPU(i)
   {
      PerfHeader = (PERFHEADER *) ptr_btrace_buffers[i];
      bytes = bytes_in_buffer(PerfHeader);
      PerfHeader->data_size = bytes;
      write_buffer_final(PerfHeader);
      // reset the buffer
      init_trace_header(PerfHeader, PerfHeader->buffer_type, PerfHeader->cpu_no,
            cont_section_size);
   }

   PerfHeader = (PERFHEADER *) ptr_mte_buffers;
   bytes = bytes_in_buffer(PerfHeader);
   PerfHeader->data_size = bytes;
   write_buffer_final(PerfHeader);
   // reset the buffer
   init_trace_header(PerfHeader, PerfHeader->buffer_type, PerfHeader->cpu_no,
         cont_section_size);

   return 0;
}

int NRM2SampleProcessor::file_close(void) {
	if (pi_tracefile)
		fclose(pi_tracefile);
	pi_tracefile = 0;
	pmd->file_error = 1;
	return 0;
}

int NRM2SampleProcessor::free_hook_buffers() {
	FOR_EACH_CPU(i) {
		if (ptr_btrace_buffers[i]) {
			free(ptr_btrace_buffers[i]);
			ptr_btrace_buffers[i] = 0;
		}
	}

	if (ptr_mte_buffers) {
		free(ptr_mte_buffers);
		ptr_mte_buffers = 0;
	}
	return 0;
}

//
// PITRACE_InitBuffer()
// *******************
//
int NRM2SampleProcessor::PITRACE_InitBuffer(uint32_t buffer_len) {
    PERFHEADER * traceHeadPtr = (PERFHEADER *)ptr_btrace_buffers[0];
    PERFHEADER * mteHeadPtr = (PERFHEADER *)ptr_mte_buffers;
    int i, x;
    int num_mte_buffers;

    /* Verify that the size is at least the default minimum. */
    if (buffer_len < PI_TRACE_BUF_DEFAULT) {
        PI_PRINTK("Reset buffer length from %lu to default %lu.\n",
                  (ULONG) buffer_len, (ULONG) PI_TRACE_BUF_DEFAULT);
        buffer_len = PI_TRACE_BUF_DEFAULT;
    }

    /*
     * If buffer exists and new buffer length is different
     * from current length, free and reallocate.
     * NO LONGER RESIZING
     * FIXME
     */
    if (traceHeadPtr != 0) {
    	pmd->TraceBufferSize = traceHeadPtr->buffer_size;
    }

    pmd->TraceBufferSize = buffer_len;

    PDEBUG("buffer_len = %lu cpu_count = %d  \n", (long) buffer_len,
           pmd->ActiveCPUs);

    if (ptr_btrace_buffers[0] == NULL) {
    	FOR_EACH_CPU(i) {
    		ptr_btrace_buffers[i] = (char *) malloc(
									pmd->TraceBufferSize + PERFHEADER_BUFFER_SIZE);

			if (ptr_btrace_buffers[i] == NULL) {
				PI_PRINTK_ERR("Trace buffer allocation failed.\n");
				if (i > 0) {
					for (x = 0; x < i; x++) {
						/* Free any allocated buffers */
						free(ptr_btrace_buffers[x]);
					}
				}

				pmd->MTEBufferSize = 0;
				pmd->TraceBufferSize = 0;
				return (PU_ERROR_NOT_ENOUGH_MEMORY);
			}
			/* Put nulls in the buffer */
			memset((char *)ptr_btrace_buffers[i], 0, pmd->TraceBufferSize);

			traceHeadPtr                 = (PERFHEADER *)ptr_btrace_buffers[i];
			traceHeadPtr->buffer_size = pmd->TraceBufferSize;
		    traceHeadPtr->section_type = TRACE_TYPE;    // mark it a TRACE section
		    init_trace_header(traceHeadPtr, TRACE_TYPE, i, cont_section_size);
    	}

        PI_PRINTK("%d Trace Buffers allocated. Buffer size = %lu\n",
                  pmd->ActiveCPUs,
                  (unsigned long) (pmd->TraceBufferSize));
    }

    // Set cont_section_size (used for cont tracing only)
    // TODO: Do we need some kind of check here?
    // e.g., if > max ...
    if (pmd->SectionSize) {
        cont_section_size = pmd->SectionSize;
    } else {
        cont_section_size = PI_SECTION_DEFAULT;
        pmd->SectionSize = cont_section_size;
    }

    // Now allocate mte buffers, if not done yet

    num_mte_buffers = 1;

    if (!pmd->MTEBufferSize) {
        pmd->MTEBufferSize = PI_MTE_BUF_DEFAULT;
    }

    if (ptr_mte_buffers == NULL) {
		ptr_mte_buffers = (char *) malloc(pmd->MTEBufferSize + PERFHEADER_BUFFER_SIZE);

		if (ptr_mte_buffers == NULL) {
			PI_PRINTK_ERR("MTE buffer allocation failed.\n");
			// Free trace buffers
			free_hook_buffers();

			pmd->MTEBufferSize = 0;
			pmd->TraceBufferSize = 0;
			return (PU_ERROR_NOT_ENOUGH_MEMORY);
		}

        PI_PRINTK("%d MTE Buffers allocated. Buffer size = %lu\n",
                  num_mte_buffers, (unsigned long) (pmd->MTEBufferSize));

        mteHeadPtr = (PERFHEADER *)ptr_mte_buffers;
        memset((char *) ptr_mte_buffers, 0, pmd->MTEBufferSize);
        mteHeadPtr->buffer_size = pmd->MTEBufferSize;
        mteHeadPtr->section_type = MTE_TYPE;    // mark it a TRACE section
    	init_trace_header(mteHeadPtr, MTE_TYPE, 0, cont_section_size);
    }

	PDEBUG("Trace buffers initialized.\n");
    return (PU_SUCCESS);
}

//
// init_continuos_mode()
// ********************
// Init for continuos
//
int NRM2SampleProcessor::init_continuos_mode(void)
{
	int            i;

	// TODO: should use commonly available flags
	const char flags[] = "w";
	pi_tracefile = fopen((char *)&(pmd->trace_file_name), flags);

	if (0 == (pi_tracefile)) {
		PI_PRINTK_ERR("ERROR opening the trace file\n");
		return(PU_FAILURE);
	}

	if (pmd->TraceMode == COLLECTION_MODE_CONTINUOS) {
	} else {
		return -1; // FIXME
	}
	pmd->mte_type = MTE_TYPE;

	pmd->file_error = 0;
	return(PU_SUCCESS);
}

}
}
