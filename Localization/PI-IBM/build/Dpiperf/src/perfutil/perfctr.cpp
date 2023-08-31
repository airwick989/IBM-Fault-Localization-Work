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
// Change History ...
//
// Date      Description
//----------------------------------------------------------------------------
// 03/06/07  Breaking out of perfutil.c
// 03/19/07  The same for Linux
// 05/21/07  Added ifdefs PI_COUNTERS_SUPPORTED
// 07/31/07  Ported Intel Core specifics (fix for Core2 bugs)
// 01/12/10  Ported Intel Nehalem support
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

#include <syslog.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <ctype.h>


#include <sys/types.h>
#include <linux/unistd.h>

#include "perfutil_core.hpp"

#ifndef stricmp
#define stricmp strcasecmp
#endif


//
// Externs
// *******
//
extern MAPPED_DATA   * pmd;                 // Data mapped by device driver

bool event_isValid(int event_id) {
	if (event_id > EVENT_HIGHEST_NUMBER || event_id < EVENT_LOWEST_NUMBER)
		return false;
	return true;
}

//
// IsPerfCounterEventSupported()
// *****************************
//
// Returns TRUE (1) if valid event and supported on this processor.
// Returns FALSE (0) if invalid event or not supported on this processor.
//
int __cdecl IsPerfCounterEventSupported(int event_id) {
	if (!event_isValid(event_id))
		return false;
	return _IsPerfCounterEventSupported(event_id);
}

//
// IsPerfCounterEventActive()
// **************************
//
int __cdecl IsPerfCounterEventActive(int event_id)
{
   int s;

   for (s = 0; s < pmd->MaxCounterEvents; s++) {
      if (pmd->PerfEvtInfo[0].CtrEvent[s] == event_id)
         return (1);
   }

   return (0);
}


//
// AnyPerfCounterEventActive()
// ***************************
//
int __cdecl AnyPerfCounterEventActive(void)
{
   int s;

   for (s = 0; s < pmd->MaxCounterEvents; s++) {
      if (pmd->PerfEvtInfo[0].CtrEvent[s] != EVENT_INVALID_ID)
         return (1);
   }

   return (0);
}

bool eventIds_listed					= 0;
int num_supported_events				= 0;
int supported_eventId_list[EVENT_COUNT] = {0};
bool eventNames_listed					= 0;
const char *supported_eventName_list[EVENT_COUNT] = {0};

//
// GetPerfCounterEventIdList()
// ***************************
//
// Returns a list of supported event ids on this processor.
// - Return value is the number of elements in the list.
// - event_id_list is an array of event ids. It is read-only.
//
int __cdecl GetSupportedPerfCounterEventIdList(int * * event_id_list) {
	if (!eventIds_listed) {
		for (int event = EVENT_LOWEST_NUMBER; event <= EVENT_HIGHEST_NUMBER; event++) {
			if (_IsPerfCounterEventSupported (event)) {
				supported_eventId_list[num_supported_events] = event;
				num_supported_events++;
			}
		}
		eventIds_listed = true;
	}

	if (event_id_list)
		*event_id_list = supported_eventId_list;
	return num_supported_events;
}

//
// GetPerfCounterEventNameList()
// *****************************
//
// Returns a list of supported event names on this processor.
// - Return value is the number of elements in the list.
// - event_name_list is an array of event names (strings). It is read-only.
//
int __cdecl GetSupportedPerfCounterEventNameList(const char * * * event_name_list) {
	if (!eventNames_listed) {
		int *eventList = 0;
		int num_events = 0;
		num_events = GetSupportedPerfCounterEventIdList(&eventList);
		for (int i=0; i<num_events; i++) {
			supported_eventName_list[i] = GetPerfCounterEventNameFromId(eventList[i]);
		}
		eventNames_listed = true;
	}
	if (event_name_list)
		*event_name_list = supported_eventName_list;
	return num_supported_events;
}


//
// GetPerfCounterEventsInfo()
// **************************
//
// Returns information about all supported events on this processor.
// - Return value is the number of PERFCTR_EVENT_INFO structures returned.
// - event_info_list is an array of read-only PERFCTR_EVENT_INFO structures.
//
int __cdecl GetPerfCounterEventsInfo(const PERFCTR_EVENT_INFO * * event_info_list) {
	return -1;
}


//
// GetPerfCounterEventIdFromName()
// *******************************
//
int __cdecl GetPerfCounterEventIdFromName(const char * event_name) {

	for (int i = EVENT_LOWEST_NUMBER; i <= EVENT_HIGHEST_NUMBER; i++) {
		if (stricmp(event_name, event_name_list[i - EVENT_LOWEST_NUMBER]) == 0)
			return i;
	}

	return (EVENT_INVALID_ID);
}


//
// GetPerfCounterEventNameFromId()
// *******************************
//
const char * __cdecl GetPerfCounterEventNameFromId(int event_id) {
	if (!event_isValid(event_id))
		return NULL;

	return event_name_list[event_id - EVENT_LOWEST_NUMBER];
}

//
// GetPerfCounterEventInfoFromId()
// *******************************
//
const char * __cdecl GetPerfCounterEventInfoFromId(int event_id) {
	if (!event_isValid(event_id))
			return NULL;

	return event_info_list[event_id - EVENT_LOWEST_NUMBER];
}


//
// GetPerfCounterEventInfoFromName()
// *********************************
//
const char * __cdecl GetPerfCounterEventInfoFromName(const char * event_name) {
	int event_id = GetPerfCounterEventIdFromName(event_name);
	if (event_id)
		return event_info_list[event_id - EVENT_LOWEST_NUMBER];
	return (NULL);
}


//
// InitPerfCounterEvent()
// **********************
//
// Set up and start counting 'event' in all processors.
//
// - There is no guarantee that the event will be started
//   in all processors at exactly the same time.
//
// Fails if:
// - No available control slot to allocate to event (ie. already counting
//   as many events as can be counted simultaneously)
// - Required resources (hardware counter, control registers) already in
//   use by another event
//
int __cdecl InitPerfCounterEvent(int event_id, int priv_level)
{
   pitrace_request   trace_request;
   pitrace_request * pr = &trace_request; 
   int               rc;

   if (!IsDeviceDriverInstalled())
      return (PU_ERROR_DRIVER_NOT_LOADED);

   if (!IsPerfCounterEventSupported(event_id))
      return (PU_ERROR_EVENT_NOT_SUPPORTED);

   if (IsPerfCounterEventActive(event_id))
      return (PU_ERROR_EVENT_ALREADY_COUNTING);

   pr->val1 = event_id;
   pr->val2 = priv_level;
   
   rc = ioctl(hDriver, CMD_CTREVENT_START, pr);

   return (rc);
}


//
// TerminatePerfCounterEvent()
// ***************************
//
// Terminate (stop) counting 'event' in all processors and release the
// associated resources.
//
// - If event is ALL_EVENTS then all events currently being counted will
//   be terminated.
//
int __cdecl TerminatePerfCounterEvent(int event_id)
{
   pitrace_request   trace_request;
   pitrace_request * pr = &trace_request; 
   int rc;

   if (!IsDeviceDriverInstalled())
      return (PU_ERROR_DRIVER_NOT_LOADED);

   if (event_id == ALL_EVENTS) {
      if (!AnyPerfCounterEventActive())
         return (PU_ERROR_EVENT_NOT_COUNTING);
   }
   else {
      if (!IsPerfCounterEventSupported(event_id))
         return (PU_ERROR_EVENT_NOT_SUPPORTED);

      if (!IsPerfCounterEventActive(event_id))
         return (PU_ERROR_EVENT_NOT_COUNTING);
   }

   pr->val1 = event_id;

   rc = ioctl(hDriver, CMD_CTREVENT_TERM, pr);
   return (rc);
}


//
// StopPerfCounterEvent()
// **********************
//
// Stop counting 'event'.
//
// - Counter value is not reset.
// - Counter associated with event is no longer counting.
// - If event is ALL_EVENTS then all events currently being counted will
//   be stopped.
// - There is no guarantee that the event will be stopped
//   in all processors at exactly the same time.
//
int __cdecl StopPerfCounterEvent(int event_id)
{
   pitrace_request   trace_request;
   pitrace_request * pr = &trace_request; 
   int rc;

   if (event_id == ALL_EVENTS) {
      if (!AnyPerfCounterEventActive())
         return (PU_ERROR_EVENT_NOT_COUNTING);
   }
   else {
      if (!IsPerfCounterEventSupported(event_id))
         return (PU_ERROR_EVENT_NOT_SUPPORTED);

      if (!IsPerfCounterEventActive(event_id))
         return (PU_ERROR_EVENT_NOT_COUNTING);
   }

   pr->val1 = event_id;

   rc = ioctl(hDriver, CMD_CTREVENT_STOP, pr);
   return (rc);
}


//
// ResumePerfCounterEvent()
// ************************
//
// Resumes counting 'event'.
//
// - Most likely use is to continue counting after a stop.
// - If event is ALL_EVENTS then all events currently being counted will
//   be resumed.
// - There is no guarantee that the event will be resumed
//   in all processors at exactly the same time.
// - Event continues counting from the current counter value.
//
int __cdecl ResumePerfCounterEvent(int event_id)
{
   pitrace_request   trace_request;
   pitrace_request * pr = &trace_request; 
   int rc;

   if (event_id == ALL_EVENTS) {
      if (!AnyPerfCounterEventActive())
         return (PU_ERROR_EVENT_NOT_COUNTING);
   }
   else {
      if (!IsPerfCounterEventSupported(event_id))
         return (PU_ERROR_EVENT_NOT_SUPPORTED);

      if (!IsPerfCounterEventActive(event_id))
         return (PU_ERROR_EVENT_NOT_COUNTING);
   }

   pr->val1 = event_id;

   rc = ioctl(hDriver, CMD_CTREVENT_RESUME, pr);

   return (rc);
}


//
// ResetPerfCounterEvent()
// ***********************
//
// Resets (sets to zero) count for 'event'.
//
// - Counter associated with 'event' is not stopped and continues
//   to count after the reset.
// - If event is ALL_EVENTS then all events currently being counted will
//   be reset.
// - There is no guarantee that the event count will be reset
//   in all processors at exactly the same time.
//
int __cdecl ResetPerfCounterEvent(int event_id)
{
   pitrace_request   trace_request;
   pitrace_request * pr = &trace_request; 
   int rc;

   if (event_id == ALL_EVENTS) {
      if (!AnyPerfCounterEventActive())
         return (PU_ERROR_EVENT_NOT_COUNTING);
   }
   else {
      if (!IsPerfCounterEventSupported(event_id))
         return (PU_ERROR_EVENT_NOT_SUPPORTED);

      if (!IsPerfCounterEventActive(event_id))
         return (PU_ERROR_EVENT_NOT_COUNTING);
   }

   pr->val1 = event_id;

   rc = ioctl(hDriver, CMD_CTREVENT_RESET, pr);
   return (rc);
}


//
// ReadPerfCounterEvent()
// **********************
//
// Returns the current count 'event' from each processor.
//
// - 'ctr_value' and 'cyc_value' are arrays allocated by the caller, using the
//   AllocCtrValueArray() API.
// - ctr_value[] will contain one counter for each processor
// - cyc_value[] will contain the TSC (when counter was read) for each processor.
//   It is optional and can be NULL.
// - There is no guarantee that the event counts will be obtained
//   in all processors at exactly the same time.
//
int __cdecl ReadPerfCounterEvent(int event_id, UINT64 * ctr_value, UINT64 * cyc_value)
{
   pitrace_request   trace_request;
   pitrace_request * pr = &trace_request; 
   int rc;

   if (ctr_value == NULL)
      return (PU_ERROR_INVALID_PARAMETER);

   if (!IsPerfCounterEventSupported(event_id))
      return (PU_ERROR_EVENT_NOT_SUPPORTED);

   if (!IsPerfCounterEventActive(event_id))
      return (PU_ERROR_EVENT_NOT_COUNTING);

   memset((void *)ctr_value, 0, pmd->CtrValueArraySize);

   if (cyc_value != NULL) {
      memset((void *)cyc_value, 0, pmd->CtrValueArraySize);
   }
   
   pr->val1  = event_id;
   pr->addr  = PtrToUint64(ctr_value);
   pr->addr2 = PtrToUint64(cyc_value);
   
   rc = ioctl(hDriver, CMD_CTREVENT_GET, pr);
   return (rc);
}

