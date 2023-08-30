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
// Linux Perf Util - SCS
//

#define  _GNU_SOURCE
#include <new>
#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <list>

#include "utils/pi_time.h"
#include "utils/pi_thread.hpp"
#include "utils/pi_allocator.hpp"

#include "perfutil_core.hpp"
#include "pu_scs.hpp"
#include "pu_worker.hpp"
#include "linux/xpu_perf_event.hpp"

int epfd, read_fifo, write_fifo;
CRITICAL_SECTION	pu_scs_mutex;

typedef std::list<pi::pu::Pi_PerfEvent_Group_ptr, pi_allocator<pi::pu::Pi_PerfEvent_Group_ptr> > Pi_PerfEvent_Group_List;
Pi_PerfEvent_Group_List pe_scs_groups;
scs_callback scs_jprof_callback = NULL;

int _init_scs(scs_callback callback)
{
   PDEBUG("SCS: Initializing SCS\n");

   epfd = epoll_create1(0);

   int pipe_array[2];
   pipe2(pipe_array, O_NONBLOCK);
   struct epoll_event *newEpollEvent = (struct epoll_event *)malloc(sizeof(epoll_event));
   newEpollEvent->data.ptr = NULL;
   newEpollEvent->events = EPOLLIN | EPOLLRDHUP | EPOLLPRI | EPOLLERR;

   epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_array[0], newEpollEvent);
   write_fifo = pipe_array[1];
   read_fifo = pipe_array[0];

   scs_jprof_callback = callback;

   InitializeCriticalSection(&pu_scs_mutex);
   return 0;
}

int _scs_init_thread(uint32_t scs_active, int tid)
{
   struct perf_event_attr scsAttr;
   init_scs_attr(scsAttr);

   if (tid == 0)
      tid = syscall(__NR_gettid);

   PDEBUG("SCS: Initializing thread %d\n", tid);

   void *ptr = malloc(sizeof(pi::pu::Pi_PerfEvent_Group));
   if (!ptr)
      return PU_ERROR_NOT_ENOUGH_MEMORY;

   pi::pu::Pi_PerfEvent_Group_ptr pe_scs_group = new (ptr) pi::pu::Pi_PerfEvent_Group(tid, -1, scsAttr);
   if (!pe_scs_group->leader) {
      free(pe_scs_group);
      return PU_ERROR_NOT_ENOUGH_MEMORY;
   }

   int rc = pe_scs_group->init_all();
   RETURN_RC_FAIL(rc);

   struct epoll_event *newEpollEvent = (struct epoll_event *)malloc(sizeof(epoll_event));
   // THIS WILL LEAK Pi_PerfEvent_Group MEMORY
   // TODO: Implement cleanup for Pi_PerfEvent_Group allocated memory
   if (!newEpollEvent)
      return PU_ERROR_NOT_ENOUGH_MEMORY;

   newEpollEvent->data.ptr = pe_scs_group;
   newEpollEvent->events = EPOLLIN | EPOLLRDHUP | EPOLLPRI | EPOLLERR;
   rc = epoll_ctl(epfd, EPOLL_CTL_ADD, pe_scs_group->leader->fdisc, newEpollEvent);

   // THIS WILL ALSO LEAK MEMORY
   RETURN_RC_FAIL(rc);

   {
      pi_critical_section init_thread(&pu_scs_mutex);
      pe_scs_groups.push_front(pe_scs_group);
      if (scs_active)
         {
         rc = pe_scs_group->ioctl_call(PERF_EVENT_IOC_ENABLE, 0);
         // THIS WILL ALSO LEAK MEMORY
         RETURN_RC_FAIL(rc);
         }
   }

   return 0;
}

int _scs_uninit_thread(int tid)
{
   if (tid == 0)
      tid = syscall(__NR_gettid);

   PDEBUG("SCS: Uninitializing thread %d\n", tid);

   pi_critical_section uninit_thread(&pu_scs_mutex);

   Pi_PerfEvent_Group_List::iterator iter = pe_scs_groups.begin();
   while (iter != pe_scs_groups.end()) {
      pi::pu::Pi_PerfEvent_Group_ptr pe_scs_group = *iter;
      if (pe_scs_group->pid == tid) {
         pi::pu::Pi_PerfEvent_Group_ptr pe_scs_group = *iter;
         pe_scs_group->valid = false;
         pe_scs_group->close_fd();
         write(write_fifo, &pe_scs_group, sizeof(&pe_scs_group));
         iter = pe_scs_groups.erase(iter);
         return PU_SUCCESS;
      }

      iter++;
   }

   return PU_ERROR_THREAD_NOT_FOUND;
}

int _scs_on()
{
   int rc = PU_SUCCESS;

   PDEBUG("SCS: Turning on SCS\n");

   pi_critical_section scs_on(&pu_scs_mutex);

   Pi_PerfEvent_Group_List::iterator iter = pe_scs_groups.begin();
   while (iter != pe_scs_groups.end()) {
      pi::pu::Pi_PerfEvent_Group *group = *iter;
      if (!group || !group->leader || !group->leader->fdisc)
         {
         rc = PU_ERR_NOT_INITIALIZED;
         break;
         }
      rc = group->ioctl_call(PERF_EVENT_IOC_ENABLE, 0);
      if (rc < 0) break;

      iter++;
   }

   return rc;
}

int _scs_off()
{
   int rc = PU_SUCCESS;

   PDEBUG("SCS: Turning off SCS\n");

   pi_critical_section scs_off(&pu_scs_mutex);

   Pi_PerfEvent_Group_List::iterator iter = pe_scs_groups.begin();
   while (iter != pe_scs_groups.end()) {
      pi::pu::Pi_PerfEvent_Group *group = *iter;
      group->close_fd();

      iter++;
   }

   return rc;
}

int _scs_pause()
{
   int rc = PU_SUCCESS;

   PDEBUG("SCS: Pausing SCS\n");

   pi_critical_section scs_pause(&pu_scs_mutex);

   Pi_PerfEvent_Group_List::iterator iter = pe_scs_groups.begin();
   while (iter != pe_scs_groups.end()) {
      pi::pu::Pi_PerfEvent_Group *group = *iter;
      if (!group || !group->leader || !group->leader->fdisc)
         {
         rc = PU_ERR_NOT_INITIALIZED;
         break;
         }
      rc = group->ioctl_call(PERF_EVENT_IOC_DISABLE, 0);
      if (rc < 0) break;

      iter++;
   }

   return rc;
}

int _scs_resume()
{
   PDEBUG("SCS: Resuming on SCS\n");

   return _scs_on();
}



int _cleanup_scs_pe_group(pi::pu::Pi_PerfEvent_Group_ptr pe_scs_group)
{
   PDEBUG("SCS: Cleanup\n");
   epoll_ctl(epfd, EPOLL_CTL_DEL, pe_scs_group->leader->fdisc, NULL);
   pe_scs_group->free_all_memory();
   free(pe_scs_group);
   return PU_SUCCESS;
}

int _process_perf_record(PE_HEADER *record, PE_ATTR &attr, scs_callstack_data &callstack_data)
{
   char *ptr = (char *)record + sizeof(PE_HEADER);

   callstack_data.pid = -1;
   callstack_data.tid = -1;
   callstack_data.ip = 0;
   callstack_data.num_stack_frames = 0;

   if (record->type != PERF_RECORD_SAMPLE)
      return 0;

   if (attr.sample_type & PERF_SAMPLE_IDENTIFIER) {
      ptr += sizeof(uint64_t);
   }
   if (attr.sample_type & PERF_SAMPLE_IP) {
      callstack_data.ip = READ_RAW_INC(uint64_t, ptr);
   }
   if (attr.sample_type & PERF_SAMPLE_TID) {
      callstack_data.pid = READ_RAW_INC(uint32_t, ptr);
      callstack_data.tid = READ_RAW_INC(uint32_t, ptr);
   }

   return PU_SUCCESS;
}

int _read_scs_data(void *scs_data, void *jniEnv)
{
   PDEBUG("SCS: Reading SCS data\n");

   pi_critical_section read_scs_data(&pu_scs_mutex);

   // Process data and call callback function to get java stack
   pi::pu::Pi_PerfEvent_Group_ptr group = (pi::pu::Pi_PerfEvent_Group_ptr)scs_data;

   if (!group || !group->leader || !group->leader->fdisc)
      return PU_ERR_NOT_INITIALIZED;

   if (!group->valid)
      return PU_SUCCESS;

   PE_MMAP_PG *pe_mmap = (PE_MMAP_PG *)group->leader->mappedmem;
   if (!pe_mmap)
      return PU_ERROR_BUFFER_NOT_MAPPED;

   char buffer[MAX_RECORD_SIZE] = {0};
   int bytes_read = group->leader->unring_mmap_data(buffer, MAX_RECORD_SIZE);
   if (bytes_read > 0) {
      PE_HEADER *rec_ptr = 0;
      int rec_size = group->leader->get_next_record(buffer, &rec_ptr, bytes_read);
      if (rec_size > 0) {

         scs_callstack_data callstack_data;

         int rc = _process_perf_record(rec_ptr, group->leader->attr, callstack_data);
         if (rc < 0)
            return rc;

         // Call callback to get jvm stack
         if (!scs_jprof_callback)
            return PU_ERROR_SCS_CALLBACK_ERROR;
         scs_jprof_callback(jniEnv, &callstack_data);

         // Reset data_tail
         pe_mmap->data_tail = pe_mmap->data_head;
      }
   }

   return PU_SUCCESS;
}

int _wait_on_scs_data(void **scs_data, uint32 timeout_millis)
{
   int rc = PU_SUCCESS;
   struct epoll_event event;

   rc = epoll_wait((int)epfd, &event, 1, timeout_millis);
   if (rc < 0)
      {
      return readSysError("Error polling on scs perf_events!");
      }

   if (!event.data.ptr) {
      pi::pu::Pi_PerfEvent_Group_ptr pe_scs_group;
      while ( read(read_fifo, &pe_scs_group, sizeof(&pe_scs_group)) > 0 ) {
         rc = _cleanup_scs_pe_group(pe_scs_group);
      }
   }
   else {
      pi::pu::Pi_PerfEvent_Group_ptr pe_scs_group = (pi::pu::Pi_PerfEvent_Group_ptr)event.data.ptr;
      *scs_data = (void *)pe_scs_group;
   }

   return rc;
}
