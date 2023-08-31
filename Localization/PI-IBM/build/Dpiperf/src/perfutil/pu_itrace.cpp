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

#include "perfutil_core.hpp"

//
// Instruction Trace controls
// ---------------------------
//
//

//
// ITraceHandlersInstalled()
// *************************
//
// Returns whether or not the ITrace interrupt handlers are still installed.
//
int __cdecl ITraceHandlersInstalled(void)
{
   return(pmd->ItraceHandlersInstalled);
}

//
// ITraceEnableAll()
// *****************
//
// Enable tracing of all pids
//
int ITraceEnableAll(void)
{
   return(ITraceEnableRange(0, pmd->PttMaxThreadId));
}


//
// ITraceEnableCurrent()
// *********************
//
// Enable tracing only for the current pid
//
int ITraceEnableCurrent(void)
{
   return(ITraceEnableRange(CURRENT_PID, CURRENT_PID));
}


//
// ITraceEnablePid()
// *****************
//
// Enable tracing of the given pid
//
int ITraceEnablePid(UINT32 pid)
{
   return(ITraceEnableRange(pid, pid));
}


//
// ITraceEnableGreater()
// *********************
//
// Enable tracing of all pids >= the given pid
//
int ITraceEnableGreater(UINT32 startpid)
{
   return(ITraceEnableRange(startpid, pmd->PttMaxThreadId));
}


//
// ITraceEnableList()
// ******************
//
// Enable tracing of a list of pids
//
int ITraceEnableList(UINT32 pid_list[], int pid_cnt)
{
   int i;
   int rc;

   for (i = 0; i < pid_cnt; ++i) {
      rc = ITraceEnablePid(pid_list[i]);
      if (rc != PU_SUCCESS)
         break;
   }

   return(rc);
}


//
// ITraceInit()
//*************
//
int ITraceInit(UINT32 type) {
   int               rc = 0;
   pitrace_request   trace_request;
   pitrace_request * pr = &trace_request;

   printf("\n ITraceInit \n");

   if (!IsDeviceDriverInstalled())
      return(PU_ERROR_DRIVER_NOT_LOADED);

   if (!IsItraceSupported())
      return(PU_ERROR_NOT_SUPPORTED);

   pr->val1 = type;

   rc = ioctl(hDriver, CMD_ITRACE_INSTALL, pr);

   return(rc);
}


//
// ITraceTerminate()
// *****************
//
// Remove itrace support
//
int ITraceTerminate()
{
   printf("\n ITraceTerminate \n");

   if (!IsDeviceDriverInstalled())
      return(PU_ERROR_DRIVER_NOT_LOADED);

   if (!IsItraceSupported())
      return(PU_ERROR_NOT_SUPPORTED);

   if (!ITraceHandlersInstalled())
      return(PU_ERROR_INCORRECT_STATE);

   return(ioctl(hDriver, CMD_ITRACE_REMOVE, 0));
}


//
// ITraceEnableRange()
// *******************
//
// The main ITraceEnable function
//
int ITraceEnableRange(UINT32 startpid, UINT32 stoppid)
{
   int               rc = 0;
   pitrace_request   trace_request;
   pitrace_request * pr = &trace_request;

   if (!IsDeviceDriverInstalled())
      return(PU_ERROR_DRIVER_NOT_LOADED);

   if (!IsItraceSupported())
      return(PU_ERROR_NOT_SUPPORTED);

   if (!ITraceHandlersInstalled())
      return(PU_ERROR_INCORRECT_STATE);

   pr->val1 = startpid;
   pr->val2 = stoppid;

   rc = ioctl(hDriver, CMD_ITRACE_ENABLE, pr);
   return(rc);
}


//
// ITraceDisable()
// ***************
//
// Disables tracing of all pids - should be run before enabling any
//
int ITraceDisable(void)
{
   int rc = 0;

   if (!IsDeviceDriverInstalled())
      return(PU_ERROR_DRIVER_NOT_LOADED);

   if (!IsItraceSupported())
      return(PU_ERROR_NOT_SUPPORTED);

   if (!ITraceHandlersInstalled())
      return(PU_ERROR_INCORRECT_STATE);

   rc = ioctl(hDriver, CMD_ITRACE_DISABLE, 0);
   return(rc);
}


//
// ITraceSetSkipCount()
// ********************
//
int ITraceSetSkipCount(UINT32 timer_skip_count)
{
   if (!IsItraceSupported())
      return(PU_ERROR_NOT_SUPPORTED);

   if (timer_skip_count == 0)
      timer_skip_count = ITRACE_DEFAULT_SKIP_COUNT;

   pmd->ItraceSkipCount = timer_skip_count;

   return(PU_SUCCESS);
}


//
// ITraceOn()
// **********
//
// Turns on the tracing
//
int ITraceOn(void)
{
   int rc = 0;
   int i  = 0;

   if (!IsDeviceDriverInstalled()) {
      printf_se("PU_ERROR_DRIVER_NOT_LOADED\n");
      return(PU_ERROR_DRIVER_NOT_LOADED);
   }

   if (!IsItraceSupported()) {
      printf_se("PU_ERROR_NOT_SUPPORTED\n");
      return(PU_ERROR_NOT_SUPPORTED);
   }

   if (!ITraceHandlersInstalled()) {
      printf_se("PU_ERROR_INCORRECT_STATE\n");
      return(PU_ERROR_INCORRECT_STATE);
   }

   rc = ioctl(hDriver, CMD_ITRACE_ON, NULL);
   printf("ITraceOn\n");

   return(rc);
}


//
// ITraceOff()
// ***********
//
// Turns off the tracing
//
int ITraceOff(void) {
   int rc = 0;

   if (!IsDeviceDriverInstalled())
      return(PU_ERROR_DRIVER_NOT_LOADED);

   if (!IsItraceSupported())
      return(PU_ERROR_NOT_SUPPORTED);

   if (!ITraceHandlersInstalled())
      return(PU_ERROR_INCORRECT_STATE);

   rc = ioctl(hDriver, CMD_ITRACE_OFF, NULL);
   return(rc);
}
