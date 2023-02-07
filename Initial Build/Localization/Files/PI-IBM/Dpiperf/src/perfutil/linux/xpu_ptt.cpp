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
// Linux Perf Util - PTT
//

#define  _GNU_SOURCE
#include <new>
#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>

#include "perfutil_core.hpp"
#include "pu_ptt.hpp"
#include "linux/xpu_perf_event.hpp"

#define member_size(type, member) sizeof(((type *)0)->member)

typedef struct ptt_perf_read_format {
   UINT64 nr;
   struct {
      UINT64 value;
   } values[1];
} ptt_perf_read_format;

int  metric_cnt;
int  metric_vals[PTT_MAX_METRICS];

int _close_ptt_events(pi::pu::Pi_PerfEvent_Group_ptr pe_ptt_group) {
   int rc;

   rc = pe_ptt_group->ioctl_call(PERF_EVENT_IOC_DISABLE, 0);
   RETURN_RC_FAIL(rc);

   rc = pe_ptt_group->close_fd();
   RETURN_RC_FAIL(rc);

   pe_ptt_group->free_all_memory();

   return PU_SUCCESS;
}

int _init_ptt(int xmetric_cnt, int xmetrics[], int priv)
{
   PDEBUG("PTT: Initializing PTT\n");

   metric_cnt = xmetric_cnt;
   pmd->PttNumMetrics = xmetric_cnt;

   for (int i = 0; i < metric_cnt; i++) {
      metric_vals[i]    = xmetrics[i];
      PDEBUG("%d: metric_vals %d\n", i, metric_vals[i]);
   }
}

int _terminate_ptt(void **kpd, int kpd_elements)
{
   PDEBUG("PTT: Terminating PTT\n");

   if (!kpd || kpd_elements < 1)
      return PU_SUCCESS;

   for (int i = 0; i < kpd_elements; i++) {
      int rc;

      if (kpd[i]) {
         rc = _ptt_uninit_thread(kpd, i);
         RETURN_RC_FAIL(rc);
      }
   }

   return PU_SUCCESS;
}

int _ptt_init_thread(void **kpd, int tid)
{
   if (!kpd)
      return PU_SUCCESS;

   int rc;

   struct perf_event_attr pttAttr;
   init_ptt_attr(pttAttr);

   if (tid == 0)
      tid = syscall(__NR_gettid);

   if (kpd[tid])
      return PU_SUCCESS;

   PDEBUG("PTT: Initializing thread %d\n", tid);

   kpd[tid] = NULL;

   void *ptr = malloc(sizeof(pi::pu::Pi_PerfEvent_Group));
   if (!ptr)
      return PU_ERROR_NOT_ENOUGH_MEMORY;

   pi::pu::Pi_PerfEvent_Group_ptr pe_ptt_group = new (ptr) pi::pu::Pi_PerfEvent_Group(tid, -1, pttAttr);
   if (!pe_ptt_group->leader) {
      free(pe_ptt_group);
      return PU_ERROR_NOT_ENOUGH_MEMORY;
   }

   for (int i = 0; i < metric_cnt; i++) {
      int pe_type, pe_event;

      rc = piEvent_to_perfEvent(metric_vals[i], &pe_type, &pe_event);
      RETURN_RC_FAIL(rc);

      pttAttr.type = pe_type;
      pttAttr.config = pe_event;

      rc = pe_ptt_group->add_child_pe(pttAttr);
      RETURN_RC_FAIL(rc);
   }

   rc = pe_ptt_group->init_all();
   RETURN_RC_FAIL(rc);

   rc = pe_ptt_group->ioctl_call(PERF_EVENT_IOC_RESET, 0);
   RETURN_RC_FAIL(rc);

   rc = pe_ptt_group->ioctl_call(PERF_EVENT_IOC_ENABLE, 0);
   RETURN_RC_FAIL(rc);

   kpd[tid] = (void *)pe_ptt_group;

   return PU_SUCCESS;
}

int _ptt_uninit_thread(void **kpd, int tid)
{
   if (!kpd)
      return PU_SUCCESS;

   int rc;

   if (tid == 0) {
      tid = syscall(__NR_gettid);
   }
   PDEBUG("PTT: Uninitializing thread %d\n", tid);

   pi::pu::Pi_PerfEvent_Group_ptr pe_ptt_group = (pi::pu::Pi_PerfEvent_Group_ptr)kpd[tid];

   if (!pe_ptt_group) {
      return PU_SUCCESS;
   }

   rc = _close_ptt_events(pe_ptt_group);
   RETURN_RC_FAIL(rc);
   kpd[tid] = NULL;

   return PU_SUCCESS;
}

int _ptt_get_metric_data(void **kpd, UINT64 *metricsArray)
{
   if (!kpd)
      return PU_SUCCESS;

   pid_t tid = syscall(__NR_gettid);

   pi::pu::Pi_PerfEvent_Group_ptr pe_ptt_group = (pi::pu::Pi_PerfEvent_Group_ptr)kpd[tid];

   if (!pe_ptt_group)
      return PU_SUCCESS;

   PDEBUG("PTT: Getting metric data for thread %d\n", tid);

   UINT64 buffer[PTT_MAX_METRICS + 1];
   int size = member_size(ptt_perf_read_format, nr) +
              ((PTT_MAX_METRICS )*member_size(ptt_perf_read_format,values));

   int rc = read(pe_ptt_group->leader->fdisc, buffer, size);
   RETURN_RC_FAIL(rc);

   ptt_perf_read_format *metric_data = (ptt_perf_read_format *)buffer;

   for (int i = 0; i < metric_cnt; i++) {
      metricsArray[i] = metric_data->values[i].value;
   }

   return PU_SUCCESS;
}
