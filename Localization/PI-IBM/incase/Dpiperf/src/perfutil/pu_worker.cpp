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
// Change History
//

#include "pu_trace.hpp"
#include "pu_scs.hpp"

#include "utils/pi_time.h"
#include "utils/pi_thread.hpp"

void *jniEnv = NULL;

volatile int continue_working = 0;
volatile int worker_running = 0;

intptr_t				pu_worker_handle = 0;
CRITICAL_SECTION	pu_worker_mutex;

int kill_pu_worker() {
	return 0;
}

int notify_pu_worker() {
	return 0;
}

/*
 * _pu_worker_tprof(parameter not used)
 * ************* Internal **********
 * Handles the collecting of os perf data and writing to file
 * This loops through each cpu and collects the perf data for each one
 */
int _pu_worker_tprof(bool force_write) {
	// make sure relevant parts are running
	int rc;
	bool more_work = true;
	int count = 0;

	EnterCriticalSection(&pu_worker_mutex);
	more_work = 1;
	while ( more_work && !pmd->file_error ) {
		PERFHEADER *perfHeader = 0;
		more_work = 0;

		// read CPU trace buffers round-robin until nothing left
		// CHANGES MADE: now read all trace_data buffers and then empty
		//			the perf buffers if needed

		// read os perf data for this cpu
		if (pmd->TraceOn) {
		   if (force_write) {
		      // read all events
		      rc = _read_trace_data(force_write);
		   }
		   else {
		      // wait and read only ready events. Each thread should only handle
		      // 4 events at a time
		      rc = _wait_and_read_trace_data(WORKER_TIMEOUT_MILLIS, 4);
		   }
		   if (rc > 0) {
		      // increment the count
		      count += rc;
            more_work = 1;
		   }
		   else if (rc == PU_ERROR_BUFFER_FULL || rc == PU_ERROR_INSUFFICIENT_BUFFER) {
				// the buffer is full. write it out and come back for another pass
				more_work = 1;
			}
		   else if (rc < 0) {
				// some other error. print out a warning
				// TODO: should stop tracing if serious error.
				// this shouldnt really happen
				PI_PRINTK_ERR("Hook writing error: %d:%s\n", rc, RcToString(rc));
			}
		}

	} // end while more work

	// handle pmd->file_error - turn tracing off
	if (pmd->TraceActive && pmd->file_error) {
		PI_PRINTK_ERR("Tracing turned off due to file write error\n");
		TraceOff();
      rc = PU_FAILURE;
	}
	LeaveCriticalSection(&pu_worker_mutex);

   return (rc >= 0) ? count : rc;
}

int _pu_worker_scs(void *scs_data)
{
   EnterCriticalSection(&pu_worker_mutex);

   int rc = _read_scs_data(scs_data, jniEnv);
   if (rc < 0) {
      PI_PRINTK_ERR("scs error: %s\n", RcToString(rc));
   }

   LeaveCriticalSection(&pu_worker_mutex);

   return rc;
}

/*
 * perfutil_worker(thread arguments)
 * *********** Internal ************
 * Currently takes in no arguments;
 * Loops infinitely while the program is running to process any perf data
 * If there is nothing to do then will just sleep
 * Currently, this only looks for tprof data but can be updated to look for other
 * types of data
 */
void * perfutil_worker(void *data) {
	bool did_work = false;
	int rc = 0;

   jniEnv = data;

	/*
	 * Initialization
	 */
	worker_running = 1;

	while (continue_working) {

		did_work = false;

		// TODO: wait on worker wake up signal

		// major section for hook buffers
		if (pmd->TraceActive) {
			rc = _pu_worker_tprof(false);
			if (rc > 0) {
				did_work = true;
			}
		}

      if (pmd->scs_enabled) {
         void *scs_data = NULL;
         rc = _wait_on_scs_data(&scs_data, WORKER_TIMEOUT_MILLIS);

         if (rc > 0 && scs_data) {
            did_work = true;
            // polling did not time out. read the data
            rc = _pu_worker_scs(scs_data);
         }
      }

		// TODO: check if there is more work

		if (!did_work) {
			// nothing got done. timed out. sleep.
			sleep_micros(1 * 1000);
		}
		fflush(stdout);
	}

	worker_running = 0;
	return 0;
}

int init_pu_worker(pu_worker_thread_create thread_create_callback, void *extraArgs) {
	continue_working = 1;

   PDEBUG("Initializing PU Worker\n");

   pu_worker_thread_args args;
   args.thread_handle = (void **)&pu_worker_handle;
   args.thread_start_func = (void **)&perfutil_worker;
   args.extraArgs = extraArgs;

   int rc = thread_create_callback(&args);
   if (rc) {
      return translateSysError(rc);
   }

	InitializeCriticalSection(&pu_worker_mutex);

	return 0;
}
