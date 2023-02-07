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
#include "NRM2SampleProcessor.hpp"
#include "pu_trace.hpp"
#include "pu_worker.hpp"
#include "PiFacility.hpp"
#include "TaskInfo.hpp"

pi::pu::PiFacility traceFacility;

/*
 * 		Setters
 * **********************
 */

//
// TraceSetMode()
// **************
//
int TraceSetMode(UINT32 trace_mode)
{
	if (trace_mode != COLLECTION_MODE_CONTINUOS)
		return PU_ERROR_NOT_SUPPORTED;
	pmd->TraceMode = trace_mode;
	return PU_SUCCESS;
}

//
// TraceSetMteSize()
// *****************
//
int TraceSetMteSize(int size)
{
   pmd->MTEBufferSize = size;
   return(PU_SUCCESS);
}

//
// TraceSetSectionSize()
// *********************
//
int TraceSetSectionSize(int size)
{
   pmd->SectionSize = size;
   return(PU_SUCCESS);
}

//
// TraceSetName()
// **************
//
int TraceSetName(char * raw_file_name)
{
   strncpy((char*)&(pmd->trace_file_name), raw_file_name, PI_MAX_PATH - 1);

   printf("Setting trace_file_name to: %s\n", pmd->trace_file_name);

   return(PU_SUCCESS);
}

//
// SetProfilerRate()
// *****************
//
// Called to set TProf rate when doing Time-based TProf (running off the
// system's Timer Tick interrupt).
//
int __cdecl SetProfilerRate(UINT32 requested_rate) {
	if (pmd->TraceOn)
		return PU_ERROR_INCORRECT_STATE;
	if (requested_rate == 0) {
		requested_rate = pmd->DefaultTprofRate;
	}
	int rc = _set_profiler_rate(requested_rate);
	RETURN_RC_FAIL(rc);
	pmd->CurrentTprofRate = requested_rate;
	pmd->TprofMode         = TPROF_MODE_TIME;
	return rc;
}

//
// TraceSetTprofEvent()
// ********************
//
// Set up metrics to be traced.
//
int TraceSetTprofEvent(int cpu, int event, int event_cnt, int priv_level) {
	PDEBUG("** CMD_PERFCTR_TPROF (0x%08X)\n", CMD_PERFCTR_TPROF);

	if (pmd->TraceOn)
		return PU_ERROR_INCORRECT_STATE;

	if (!IsProcessorNumberValid(cpu))
		return (PU_ERROR_INVALID_PARAMETER);

	//if (pmd->ApicSystem == 0)
		//return (PU_ERROR_NOT_APIC_SYSTEM);

	if (!(event >= PERF_CTR0 && event < pmd->NumPerfCtrs)
	        && !(event >= EVENT_LOWEST_NUMBER && event <= EVENT_HIGHEST_NUMBER))
      return (PU_ERROR_INVALID_METRIC);

	int rc = _trace_set_tprof_event(cpu,  // cpu
			event,  // event
			event_cnt,  // event_cnt
         priv_level); // priv_level
	RETURN_RC_FAIL(rc);

	pmd->TprofCtrEvent         = event;
	pmd->CurrentTprofRate = event_cnt;
	return rc;
}

//
// SetTraceAnonMTE
// ****************
//
void __cdecl SetTraceAnonMTE(int val)
{
   pmd->TraceAnonMTE = val;
}

/*
 * 		Getters
 * **********************
 */

//
// TraceGetMaxNumberOfMetrics()
// ****************************
//
// Returns the maximum allowed number of metrics for the system.
// The maximum number for a system (OS/hardware architecture) may not
// necessarily be TRACE_MAX_METRICS.
//
int __cdecl TraceGetMaxNumberOfMetrics(void) {
	// FIXME: this is incorrect. fix this or get rid of it
   return(1);
}

//
// TraceQueryMEC()
// ***************
//
int TraceQueryMEC(char * Maj, int len)
{
   if (Maj == NULL)
      return(PU_ERROR_INVALID_PARAMETER);

   if (len <= 0 || len > sizeof(pmd->MecTable))
      return(PU_ERROR_INVALID_PARAMETER);

   memcpy(Maj, pmd->MecTable, len);
   return(0);
}

//
// TraceQueryMajor()
// *****************
//
int TraceQueryMajor(int major)
{
   if (major <= 0 || major > 0xFF)
      return(MAJOR_NOT_ENABLED);

   // Multiple metrics are not currently supported

   if (pmd->MecTable[major]) {
      return(MAJOR_ENABLED_NOMM);
   }
   else {
      return(MAJOR_NOT_ENABLED);
   }
}

//
// IsTraceOn()
// ***********
//
int __cdecl IsTraceOn(void)
{
	return pmd->TraceOn;
}

//
// IsTraceActive()
// ***************
//
int __cdecl IsTraceActive(void)
{
	return pmd->TraceActive;
}


//
// TraceGetIntervalId()
// ********************
//
int TraceGetIntervalId(void)
{
	// this is useless now
   return pmd->TraceIntervalId;
}

/*
 TraceGetName()
 **************
 * Returns the name of the trace file
*/

char * TraceGetName(void)
{
   return((char*)&(pmd->trace_file_name));
}

//
// IsContMode()
// ************
//
int IsContMode(void)
{
	// this should always be true
   return pmd->TraceMode == COLLECTION_MODE_CONTINUOS;
}

//
// IsWrapMode()
// ************
//
int IsWrapMode(void)
{
	// always false
   return (pmd->TraceMode == COLLECTION_MODE_WRAP);
}

/*
 * 		Initialization
 * **********************
 */

//
// set_tprof_defaults()
// ********************
//
// The default TProf rate is calculated to be about 100 ticks/sec
// The default SCS rate is calculated to be about 32 ticks/sec
//
void set_tprof_defaults()
{
	// none of this is relevant anymore
	if (pmd->CpuTimerTick == 100) {
		pmd->DefaultTimeTprofDivisor = 1;	// Sample on every timer tick (100 ticks/sec)
		pmd->DefaultTimeScsDivisor = 3;		// Sample on every 3rd timer tick (33.33 ticks/sec)
	}
	else if (pmd->CpuTimerTick == 250) {
		pmd->DefaultTimeTprofDivisor = 2;	// Sample on every 2nd timer tick (125 ticks/sec)
		pmd->DefaultTimeScsDivisor = 8;		// Sample on every 8th timer tick (31.25 ticks/sec)
	}
	else if (pmd->CpuTimerTick == 300) {
		pmd->DefaultTimeTprofDivisor = 3;	// Sample on every 3rd timer tick (100 ticks/sec)
		pmd->DefaultTimeScsDivisor = 9;		// Sample on every 9th timer tick (33.33 ticks/sec)
	}
	else if (pmd->CpuTimerTick == 1000) {
		pmd->DefaultTimeTprofDivisor = 10;	// Sample on every 10th timer tick (100 ticks/sec)
		pmd->DefaultTimeScsDivisor = 30;	// Sample on every 30th timer tick (33.33 ticks/sec)
	}
	else {
		pmd->DefaultTimeTprofDivisor = pmd->CpuTimerTick / 100;
		pmd->DefaultTimeScsDivisor = pmd->CpuTimerTick / 32;
      PI_PRINTK("*** HZ=???? DefaultTimeTprofDivisor=%d DefaultTimeScsDivisor=%d\n", pmd->DefaultTimeTprofDivisor, pmd->DefaultTimeScsDivisor);
	}

	// also useless now
	pmd->TprofTickDelta = pmd->DefaultTimeTprofDivisor;
	pmd->DefaultEventDivisor = 8;              // Sample on every 8th event

	// this is the only useful part
	pmd->TprofMode       = TPROF_MODE_TIME; // default to time based mode
    pmd->TprofCtrEvent   = 0;
	pmd->DefaultTprofRate  = 100; // 100 ticks/sec
	pmd->CurrentTprofRate   = pmd->DefaultTprofRate;

    PDEBUG("Done\n");
	return;
}

// sets the scope of the tracing facilities
int TraceSetScope(int scope, int pid) {
	if (traceFacility.sampleProcessor){
		// already initialized, can't change scope now
		return PU_ERROR_INCORRECT_STATE;
	}
	if (scope == TS_PROCESS && pid > 0) {
		pmd->TprofScope = TS_PROCESS;
		pmd->TprofPidMain = pid;
	}
	else if (scope == TS_SYSTEM) {
		pmd->TprofScope = TS_SYSTEM;
		pmd->TprofPidMain = -1;
	}
	else {
		return PU_ERROR_INVALID_PARAMETER;
	}
	return PU_SUCCESS;
}

/*

 TraceInit()
 ***********

 Routine to initialize the trace buffer.
 Returns 0 if there are no errors.
*/
int TraceInit(uint32_t bufsize)
{
   if (traceFacility.sampleProcessor == 0) {
      // set the default if not already provided
      traceFacility.sampleProcessor = new pi::pu::NRM2SampleProcessor();
   }
   if (traceFacility.taskInfo == 0) {
      // set the default if not already provided
      traceFacility.taskInfo = new pi::pu::NRM2TaskInfo();
   }

	int rc;
	// init the trace buffers
	std::string name((char *)pmd->trace_file_name);
	rc = traceFacility.sampleProcessor->initialize(name, &traceFacility);
	RETURN_RC_FAIL(rc);

	// set up the pid bool vector. this sets its size dynamically
	rc = traceFacility.taskInfo->initialize(&traceFacility);
	RETURN_RC_FAIL(rc);

	// call the os stub as the last item
	rc = _trace_init();
	return rc;
}


/*
 * 		Control
 * **********************
 */

//
// TraceSuspend()
// **************
//
int __cdecl TraceSuspend(void)
{
   if (pmd->TraceOn == 0)
      return(PU_ERROR_INCORRECT_STATE); // Tracing has to be on

   if (pmd->TraceActive == 0)
      return(0);                        // Tracing already suspended

   if (pmd->file_error)
	   return PU_ERR_FILE_ERROR;

   int rc = _trace_suspend();

   if (rc == PU_SUCCESS)
	   pmd->TraceActive = 0;

   return rc;
}

//
// TraceResume()
// *************
//
int __cdecl TraceResume(void)
{
   if (pmd->TraceOn == 0)
      return(PU_ERROR_INCORRECT_STATE); // Tracing has to be on

   if (pmd->TraceActive == 1)
      return(0);                        // Tracing already running

   if (pmd->file_error)
	   return PU_ERR_FILE_ERROR;

   int rc = _trace_resume();

   if (rc == PU_SUCCESS)
	   pmd->TraceActive = 1;

   return rc;
}

//
// TraceOn()
// *********
//
// Function to turn on trace and also output the startup hooks
//
int TraceOn(void) {
   if (!traceFacility.sampleProcessor || !traceFacility.eventManager) {
      return PU_ERR_NOT_INITIALIZED;
   }
	PDEBUG("** CMD_SOFTTRACE_ON (0x%08X)\n", CMD_SOFTTRACE_ON);
	if (pmd->TraceOn)
		return PU_ERROR_INCORRECT_STATE;
	PDEBUG("** CMD_SOFTTRACE_ALLOC (0x%08X)\n", CMD_SOFTTRACE_ALLOC);

	int rc;
	rc = traceFacility.sampleProcessor->startedRecording();
   RETURN_RC_FAIL(rc);

	// reset the pid vector
   traceFacility.taskInfo->resetData();

	rc = _trace_on();
	RETURN_RC_FAIL(rc);
   pmd->MteActive 				= 1;
	pmd->TraceActive           = 1;
	pmd->TraceOn               = 1;
	return rc;
}

//
// TraceOff()
// **********
//
// Function to turn off tracing
//
int TraceOff(void)
{
	PDEBUG("** CMD_SOFTTRACE_OFF (0x%08X)\n", CMD_SOFTTRACE_OFF);

	// suspend OS tracing for now. as we stop collecting data
	TraceSuspend();

	// idk why we unset the mectable. old habits die hard
	memset(&pmd->MecTable,0, LARGEST_HOOK_MAJOR+1);

	// collect everything left from the OS and write out the sections
	_pu_worker_tprof(true);

	traceFacility.sampleProcessor->stoppedRecording();

	// tell the OS to close the trace facility
	_trace_off();

	pmd->TraceOn = false;
	return PU_SUCCESS;
}

//
// TraceTerminate()
// ****************
//
// Free trace buffers and other trace resources;
// returns a 0 if there are no errors
//
int __cdecl TraceTerminate(void) {
	PDEBUG("** CMD_SOFTTRACE_TERMINATE (0x%08X)\n", CMD_SOFTTRACE_TERMINATE);
	int rc;
	if (pmd->TraceOn) {
		rc = TraceOff();
		if (rc < PU_SUCCESS) {
			PI_PRINTK_ERR("Failed to turn off tracing first. Cannot terminate");
			return rc;
		}
	}

	rc = _trace_terminate();

	/* If buffer exists */
	if (traceFacility.sampleProcessor)
	   traceFacility.sampleProcessor->terminate();

	pmd->TraceBufferSize = 0;

	//
	// Set TPROF defaults
	//
	if (!pmd->scs_enabled)
		set_tprof_defaults();

	return rc;
}

//
// TraceEnable()
// *************
//
// Now a routine to enable a Major code in the MEC table in the kernel
// Note : if 0 is passed in majorCode, then all the major codes will be
//        enabled in the kernel MEC table
// Also if majorCode > LARGEST_HOOK_MAJOR then it is ignored
// Returns 0 if sucessful
//
int TraceEnable(int majorCode) {
	PDEBUG("** CMD_SOFTTRACE_ENABLE (0x%08X)\n", CMD_SOFTTRACE_ENABLE);
	/* When 0 is passed, enable all major codes. */
	if (majorCode == 0) {
		for (int i = 0; i <= LARGEST_HOOK_MAJOR; i++) {
			pmd->MecTable[i] = 1;
		}
	}
	else {
		pmd->MecTable[majorCode] = 1;
	}

	return _perfTraceEnableDisable();
}

//
// TraceDisable()
// **************
//
// Now a routine to disable a Major code in the MEC table in the kernel
// Note : if 0 is passed in majorCode, then all the major codes will be
//        disabled in the kernel MEC table
// Also if majorCode > LARGEST_HOOK_MAJOR then it is ignored
// Returns 0 if sucessful
//
int TraceDisable(int majorCode)
{
	PDEBUG("** CMD_SOFTTRACE_DISABLE (0x%08X)\n", CMD_SOFTTRACE_DISABLE);
	/* When 0 is passed, disable all major codes. */
	if (majorCode == 0) {
		for (int i = 0; i <= LARGEST_HOOK_MAJOR; i++) {
			pmd->MecTable[i]         = 0;
		}
	} else {
		pmd->MecTable[majorCode]         = 0;
	}

	return _perfTraceEnableDisable();
}

//
// TraceDiscard()
// **************
//
int __cdecl TraceDiscard(void) {
	PDEBUG("** CMD_SOFTTRACE_DISCARD (0x%08X)\n", CMD_SOFTTRACE_DISCARD);
	//SoftTraceOff();

	// TODO: tell sample processor to dump

	int rc ;
	if (rc != PU_SUCCESS) {
		TraceOff();
		return rc;
	}

	// FIXME: cannot discard if already turned off tracing
	return _trace_discard();
}

//
// MapTraceBuffer()
// ****************
// Mapping trace buffers to user space
//
int MapTraceBuffer(void) {

	return (0);
}

int __cdecl setTraceProcessor(pi::pu::SampleProcessor *processor, pi::pu::TaskInfo *taskInfo) {
   if (traceFacility.isInitialized())
      return PU_ERROR_INCORRECT_STATE;
   if (processor)
      traceFacility.sampleProcessor = processor;
   if (taskInfo)
      traceFacility.taskInfo = taskInfo;
}
