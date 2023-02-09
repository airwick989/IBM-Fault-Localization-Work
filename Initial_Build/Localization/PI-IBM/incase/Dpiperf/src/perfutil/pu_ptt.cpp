//----------------------------------------------------------------------------
//
//   IBM Performance Inspector
//   Copyright (c) International Business Machines Corp., 2003 - 2007
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
// Change History ...
//
// Date      Description
//----------------------------------------------------------------------------
// 04/27/07  Breaking out of perfutil.c ...
//           Add:
//           - PttSetGetDataMode()
//           - PttReturnGetCurrentThreadDataFunctionPtr()
//           Remove:
//           - PttSetResetCounterMode()
//           Allow PttInit() to handle CYCLES
//           - CYCLES and NONHALTED_CYCLES no longer the same
//           Optimize PttGetCurrentThreadDataEx() modes
//           No longer return retry_cnt on any Get*Data() call
// 05/03/07  More tightening, UP/MP for everything
// 08/06/07  Port to Linux
// 01/13/09  Removed SetFITICounts() (fields removed)
// 04/29/09  A regression fix in PttTerminate
//           (first unmap, then free in the kernel), call PttSetDEfault
// 05/11/09  Added IsVirtualMachine check to PttInit
//----------------------------------------------------------------------------
//
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/time.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <ctype.h>


#include <sys/types.h>
#include <linux/unistd.h>

#include "itypes.h"
#include "perfctr.h"
#include "perfutil_core.hpp"
#include "pitrace.h"
#include "utils/pi_thread.hpp"
#include "perfmisc.h"
#include "pu_ptt.hpp"
#include "TaskInfo.hpp"


//
// Externs
// *******
//

extern MAPPED_DATA   * pmd;             // Data mapped by device driver


//
// Globals (instance data)
// ***********************
//
void ** kpd       = NULL;
int kpd_elements  = 0;
CRITICAL_SECTION	pu_ptt_mutex;



int allocatePTTData()
{
   int max_pids = pi::pu::TaskInfo::getMaxPid();
   if (max_pids <= 0) {
      perror("ERROR getting max number of PIDs\n");
      return PU_FAILURE;
   }

   kpd = (void **)malloc(max_pids * sizeof(void *));
   if (!kpd) {
      perror("ERROR Failed to allocate memory for PTT\n");
      return PU_ERROR_NOT_ENOUGH_MEMORY;
   }
   memset(kpd, 0, (max_pids * sizeof(void *)));
   kpd_elements = max_pids;

   return PU_SUCCESS;
}

void deallocatePTTData()
{
   free(kpd);
   kpd = NULL;
}

int PttInit(int metric_cnt, int metrics[], int priv_level)
{
   int i, j, rc;
   int               temp_metrics[PTT_MAX_METRICS];
   int               counters_only = 0;
   int               events_only   = 0;

   pmd->PTT_Enabled = 0;

   if (metric_cnt <= 0) {
      return PU_ERROR_INVALID_PARAMETER;
      RETURN_RC_FAIL(rc);
   }

   if (metric_cnt > pmd->PttMaxMetrics) {
      printf("Exceeded PttMaxMetrics = %d, metric_count = %d\n",
             pmd->PttMaxMetrics, metric_cnt);
      return PU_ERROR_TOO_MANY_METRICS;
   }

   for (i = 0; i < metric_cnt; i++) {
      temp_metrics[i] = metrics[i];

      /*
      if (metrics[i] >= PTT_METRIC_CTR0 && metrics[i] <= pmd->MaxPerfCtrNumber)
         counters_only++;
      else {
      */
      events_only++;

      // Change GV IDs to PU IDs, except for PTT_METRIC_CYCLES
      if (temp_metrics[i] == PTT_METRIC_CYCLES)
         temp_metrics[i] = EVENT_NONHALTED_CYCLES;
      else if (temp_metrics[i] == PTT_METRIC_INSTR_RETIRED)
         temp_metrics[i] = EVENT_INSTR;

      // Check if valid event
      if (!IsPerfCounterEventSupported(temp_metrics[i])) {
         return PU_ERROR_INVALID_METRIC;
      }

      /*
      }
      */
   }

   // Can't mix counters and events
   if ((events_only    &&  events_only != metric_cnt) ||
       (counters_only  &&  counters_only != metric_cnt)){
      return PU_ERROR_MIXED_METRICS;
   }

   // Can't have duplicate metrics
   for (i = 0; i < metric_cnt; i++) {

      for (j = i+1; j < metric_cnt; j++) {
         if (metrics[i] == metrics[j]) {
            return PU_ERROR_DUPLICATE_METRICS;
         }
      }
   }

   rc =_init_ptt(metric_cnt, temp_metrics, priv_level);

   RETURN_RC_FAIL(rc);

   rc = allocatePTTData();

   RETURN_RC_FAIL(rc);

   pmd->PTT_Enabled = 1;

   InitializeCriticalSection(&pu_ptt_mutex);

   return rc;
}

int PttInitThread(int tid)
{
   pi_critical_section init_thread(&pu_ptt_mutex);
   return _ptt_init_thread((void **)kpd, tid);
}

int PttUnInitThread(int tid)
{
   pi_critical_section uninit_thread(&pu_ptt_mutex);
   return _ptt_uninit_thread((void **)kpd, tid);
}

int PttTerminate()
{
   int rc;

   if (pmd->PTT_Enabled == 0)
      return PU_SUCCESS;

   {
      pi_critical_section ptt_terminate(&pu_ptt_mutex);

      rc = _terminate_ptt((void **)kpd, kpd_elements);
      RETURN_RC_FAIL(rc);

      deallocatePTTData();

      pmd->PTT_Enabled = 0;
   }

   return PU_SUCCESS;
}

int PttGetMetricData(UINT64 *metricsArray) {
   // pi_critical_section get_metric_data(&pu_ptt_mutex);
   return _ptt_get_metric_data((void **)kpd, metricsArray);
}
