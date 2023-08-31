
#ifndef _PU_TRACE_HPP
#define _PU_TRACE_HPP

#include <stdint.h>

namespace pi {
namespace pu {
class PiFacility;
class TaskInfo;
class SampleProcessor;
}
}

extern pi::pu::PiFacility traceFacility;

int setTraceProcessor(pi::pu::SampleProcessor *processor, pi::pu::TaskInfo *taskInfo);

// common functions
void set_tprof_defaults();

// OS specific stubs
int _wait_on_trace_data(uint32_t timeout_millis);
int _wait_and_read_trace_data(uint32_t timeout_millis, int max_events = 1);
int _read_trace_data(bool force);
int _set_profiler_rate(uint32_t requested_rate);
int _trace_set_tprof_event(int cpu,int event,int event_cnt,int priv_level);
int _trace_discard();
int _trace_resume();
int _trace_suspend();
int _perfTraceEnableDisable(void);
int _trace_init();
int _trace_terminate();
int _trace_off();
int _trace_on();

// mte processing
int set_pid_processed(int pid, int tid);
int process_pid(int pid, int tid);
int init_pid_vector();
int reset_pid_vector();

#endif
