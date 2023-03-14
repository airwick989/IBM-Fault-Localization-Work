//----------------------------------------------------------------------------
//
//   IBM Performance Inspector
//   Copyright (c) International Business Machines Corp., 2003 - 2010
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public License
//   along with this library; if not, write to the Free Software Foundation,
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//----------------------------------------------------------------------------
//
// General purpose routines that provide interface to the kernel driver.
//

//
// Linux Perf Util - Trace
//

/*
 * This is the visible layer
 * It is mainly used for translation and control
 */

#define  _GNU_SOURCE
#include <new>
#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <poll.h>

#include "utils/pi_time.h"

#include "linux/xpu_perf_event.hpp"
#include "perfutil_core.hpp"
#include "pu_trace.hpp"
#include "PiFacility.hpp"
#include "TaskInfo.hpp"

/*
 * _wait_on_trace_data(timeout in milliseconds)
 * ********** Internal ****************
 * polls on the perf buffers for data.
 * Returns 0 if polling timed out
 * Returns >0 the number of perf buffers that have data
 * returns <0 if there is an error
 */
int _wait_on_trace_data(uint32 timeout_millis) {
   std::vector<int> keys = traceFacility.eventManager->waitOnData(timeout_millis);
   return keys.size();
}

int _wait_and_read_trace_data(uint32 timeout_millis, int max_events) {
   int rc = traceFacility.eventManager->waitAndReadData(timeout_millis, max_events);
   return rc;
}

/*
 * _trace_on()
 * ******* Internal *******
 * Stub to turn on tprof tracing facilities
 * for each cpu this opens up the perfevents and then enables them
 * it also sets up the polling structures for when _wait_on_trace_data
 * is called.
 * Returns <0 on error
 */
int _trace_on() {
   PDEBUG("start\n");
   int rc;
   std::vector<int> keys;
   if (pmd->TprofScope == TS_PROCESS) {
      rc = pi::pu::TaskInfo::getProcessTidList(pmd->TprofPidMain, keys);
   }
   else {
      int cpu;
      PE_ATTR attr = traceFacility.eventManager->attr;
      // add the first cpu
      traceFacility.eventManager->add(0, false);

      // only the first cpu will handle mte data
      traceFacility.eventManager->attr.mmap = 0;
      traceFacility.eventManager->attr.comm = 0;
      traceFacility.eventManager->attr.task = 0;
      traceFacility.eventManager->attr.mmap_data = 0;
      FOR_EACH_CPU(cpu)
      {
         if (cpu != 0) {
            keys.push_back(cpu);
         }
      }
   }

   rc = traceFacility.eventManager->add(keys, false);
   RETURN_RC_FAIL(rc);

   rc = traceFacility.eventManager->ioctl_call(PERF_EVENT_IOC_ENABLE, 0);
   RETURN_RC_FAIL(rc);

   PDEBUG("finish\n");
   return PU_SUCCESS;
}

/*
 * _trace_off()
 * *********** Internal **********
 * Closes all the tprof related perf_event file descriptors
 */
int _trace_off() {
   traceFacility.eventManager->clear();
   return PU_SUCCESS;
}

/*
 * _trace_terminate()
 * *********** Internal **********
 * Frees all the tprof related data structures
 */
int _trace_terminate() {
   traceFacility.eventManager->clear();
   return PU_SUCCESS;
}

int _trace_init() {
   PDEBUG("stub start\n");
   PE_ATTR traceAttr;
   init_tprof_attr(traceAttr);
   int rc;

   if (pmd->TprofScope == TS_SYSTEM) {
   }
   else {
      traceAttr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED;
      traceAttr.inherit = 0; // tells perf_events to record all child threads as well
      traceAttr.sample_type |= PERF_SAMPLE_CPU; // need to record the cpu number
   }

   traceFacility.eventManager = new pi::pu::PiEventManager();
   rc = traceFacility.eventManager->init(pmd->TprofScope, traceAttr, &traceFacility);
   RETURN_RC_FAIL(rc);

   PDEBUG("stub done\n");
   return PU_SUCCESS;
}

//
// PerfTraceEnableDisable()
// ************************
//
int _perfTraceEnableDisable(void) {
   return PU_SUCCESS;
}

int _trace_suspend() {
   int rc;
   rc = traceFacility.eventManager->ioctl_call(PERF_EVENT_IOC_DISABLE, 0);
   return rc;
}

int _trace_resume() {
   int rc;
   rc = traceFacility.eventManager->ioctl_call(PERF_EVENT_IOC_ENABLE, 0);
   return rc;
}

int _trace_discard() {
   traceFacility.eventManager->discardData();
   return PU_SUCCESS;
}

int _trace_set_tprof_event(int cpu, int event, int event_cnt, int priv_level) {
   int rc, pe_type, pevent;

   PDEBUG("PerfTraceSetTprofEvent: start. cpu=0x%X, event=%d, cnt=%d, priv=%d\n",
         cpu, event, event_cnt, priv_level);

   rc = piEvent_to_perfEvent(event, &pe_type, &pevent);
   RETURN_RC_FAIL(rc);

   traceFacility.eventManager->attr.type = pe_type;
   traceFacility.eventManager->attr.config = pevent;
   traceFacility.eventManager->attr.sample_period = event_cnt;
   traceFacility.eventManager->attr.freq = 0;

   PDEBUG("PerfTraceSetTprofEvent: end\n");
   return (0);
}

int _set_profiler_rate(UINT32 requested_rate) {
   traceFacility.eventManager->attr.sample_period = requested_rate;
   traceFacility.eventManager->attr.freq = 1;
   traceFacility.eventManager->attr.type = PERF_TYPE_SOFTWARE;
   traceFacility.eventManager->attr.config = PERF_COUNT_SW_CPU_CLOCK;

   return PU_SUCCESS;
}

int _read_trace_data(bool force) {
   int cpu, rc = 0;

   rc = traceFacility.eventManager->readAllRecords(force);
   return rc;
}
