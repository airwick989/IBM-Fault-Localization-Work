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

#ifndef _GNU_SOURCE
#define  _GNU_SOURCE
#endif

#include <linux/unistd.h>
#include "perfutil_core.hpp"

int readSysError(const char *errStr) {
   fflush(stdout);
   if (errStr)
      perror(errStr);
   int err = errno;
   return translateSysError(err);
}

int translateSysError(int syserr) {
   switch(syserr) {
   case EACCES:
      return PU_ERROR_ACCESS_DENIED;
   case EINVAL:
      return PU_ERROR_INVALID_PARAMETER;
   case ENOMEM:
      return PU_ERROR_NOT_ENOUGH_MEMORY;
   case ESRCH:
   	return PU_ERROR_THREAD_NOT_FOUND;
   default:
      return PU_FAILURE;
   }
}

// OpenDevice()
//*************
//
// This routine opens the PI Trace driver to process the requests.
// The file number is saved in a static global area so it can be accessed by the driver operations.
// Right now it only supports one open to the driver.
// DriverName argument is ignored
//
HANDLE __cdecl OpenDevice(char * DriverName)
{
   return 0;
   //
   // If already opened then return that handle
   //
   if (DRIVER_LOADED)
      return(hDriver);

   //
   // Driver not already opened. Do it.
   //
   hDriver = open("/dev/pidd",  O_RDWR | O_SYNC);

   if (hDriver <= 0) {
      hDriver = 0;                      // reset
   }

   return(hDriver);
}

int IsDeviceDriverInstalled(void)
{
   return 1;
   //static msg = 1;

   return(0);
}

//
// SWTRACE user hooks
// ------------------
//


//
// *OPTIONAL*
// Return cumulative thread time for *ANY* thread.
//
int GetPerfTime(UINT64 * time)
{
   pitrace_request   trace_request;
   pitrace_request * pr = &trace_request;

   if (!IsDeviceDriverInstalled())
      return(PU_FAILURE);

   pr->addr = PtrToUint64(time);
   return(ioctl(hDriver, CMD_GetPerfTime, pr));
}


//
// Processor information/affinity
// -------------------------------
//


//
// SwitchToProcessor()
// *******************
//
int SwitchToProcessor(int cpu, UINT64 * prev_mask)
{
   cpu_set_t mask;
   int       rc;

   CPU_ZERO(&mask);
   CPU_SET(cpu, &mask);

   // Set affinity of the current process (0)
   // prev_mask parameter is currently ignored

   rc = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
   sleep(0);
   return(rc);
}


#define GET_TRACE 1
#define GET_MTE   2


//
// pi_gettid()
// ***********
//
// Returns the id of the current thread
//
pid_t     pi_gettid(void)
{
   return(syscall(__NR_gettid));
}

/*
 * _init_activeCpu_list
 * For each installed cpu, check if it is active annd if it is,
 * then amend it to the list of installed cpus
 *
 * Returns 0 on success;
 */
int _init_activeCpu_list() {
   int installed = pmd->InstalledCPUs;
   FILE *cpuf = 0;
   char buff[PI_MAX_PATH];
   int num_active = 1; // cpu 0 is always active so move on
   int online;
   int rc = 0;

   // for each installed cpu, check if it is active
   for (int cpu=1; cpu<installed; cpu++) {
      sprintf(buff, "/sys/devices/system/cpu/cpu%d/online", cpu);
      cpuf = fopen(buff, "r");
      if (cpuf <= 0) {
         PI_PRINTK_ERR("Failed to open file: %s\n", buff);
         return readSysError("Failed to open file");
      }
      if (fscanf(cpuf, "%d", &online) <= 0) {
         PI_PRINTK_ERR("Failed read contents of file: %s\n", buff);
         return readSysError("Failed to open file");
      }
      // done reading the file, close it
      fclose(cpuf);
      cpuf = 0;

      if (!online) // no online. dont add to list
         continue;

      // cpu is active. append it to the active cpu list and increment
      pmd->ActiveCPUList[num_active] = cpu;
      num_active++;
   }

   if (pmd->ActiveCPUs != num_active) {
      // something got counted incorrectly. return error and shut down
      PI_PRINTK_ERR("Active CPU count mismatch pmd->ActiveCPUs=%d num_active=%d\n", pmd->ActiveCPUs, num_active);
      return -1;
   }

   // all good here
   return 0;
}




