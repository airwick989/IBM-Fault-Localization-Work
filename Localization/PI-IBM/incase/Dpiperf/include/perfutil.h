/*
 * IBM Performance Inspector
 * Copyright (c) International Business Machines Corp., 1996 - 2010
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

//----------------------------------------------------------------------------
// API availability macros ("HAVE_*") and which APIs are available if defined:
//
//
//
// * HAVE_TraceGetIntervalId
//   - TraceGetIntervalId()
//
// * HAVE_CallstackSamplingSupport
//   - All Callstack Sampling APIs
//
// * HAVE_GetTscSpeed
//   - GetTscSpeed()
//
// * HAVE_IsTscInvariant
//   - IsTscInvariant()
//
// * HAVE_IsAperfSupported
//   - IsAperfSupported()
//
// * HAVE_PpmApis
//   - GetPmDisabledMask()
//   - PmDisabledItrace/Tprof/Scs/Ai/InterruptHooks/DispatchHook()
//   - PmDisabledSysCallHooks/PerfCounterEvents()
//   - TscFrequencySameAsRatedCpuFrequency()
//   - GetCurrentProcessorFrequency()
//   - IsAc/DcProcessorThrottlingActive()
//   - DisableAc/DcProcessorThrottling()
//   - GetAc/DcProcessorThrottlingPolicyLimits()
//   - GetAc/DcProcessorThrottlingPolicyType()
//
// * HAVE_SlowTsc_Detection
//   - PiBeginSlowTscDetection()
//   - PiEndSlowTscDetection()
//   - PiReadSlowTscCount()
//   - PiIsSlowTscDetectionActive()
//
//----------------------------------------------------------------------------
#ifndef _PERFUTIL_H_
#define _PERFUTIL_H_

#if defined(_WIN32) || defined(_WIN64)
   // *** WINDOWS ***
   #include <basetsd.h>
   #include <limits.h>

   #if !defined(_WINDOWS)
      #define _WINDOWS
   #endif
   #if defined(_WIN64) || defined (WIN64)
      #if !defined(_64BIT)
         #define _64BIT
      #endif
      #if !defined(_WINDOWS64)
         #define _WINDOWS64
      #endif
      #if defined(_M_IA64) || defined(_IA64_)
         #if !defined(_PLATFORM_STR)
            #define _PLATFORM_STR "Windows-IA64"
         #endif
         #if !defined(_ARCH_STR)
            #define _ARCH_STR "IA64"
         #endif
         #if !defined(_IA64)
            #define _IA64
         #endif
      #elif defined(_M_AMD64) || defined(_AMD64_) || defined(_M_X64)
         #if !defined(_PLATFORM_STR)
            #define _PLATFORM_STR "Windows-x64"
         #endif
         #if !defined(_ARCH_STR)
            #define _ARCH_STR "x64"
         #endif
         #if !defined(_X86_64)
            #define _X86_64
         #endif
         #if !defined(_X64)
            #define _X64
         #endif
         #if !defined(_AMD64)
            #define _AMD64
         #endif
      #else
         #error ***** PERFUTIL.H: Unknown 64-bit Windows platform *****
      #endif
   #else
      #if !defined(_32BIT)
         #define _32BIT
      #endif
      #if !defined(_WINDOWS32)
         #define _WINDOWS32
      #endif
      #if defined (_M_IX86) || defined (_X86_)
         #if !defined(_PLATFORM_STR)
            #define _PLATFORM_STR "Windows-x86"
         #endif
         #if !defined(_ARCH_STR)
            #define _ARCH_STR "x86"
         #endif
         #if !defined(_X86)
            #define _X86
         #endif
         #if !defined(_I386)
            #define _I386
         #endif
         #if !defined(_IA32)
            #define _IA32
         #endif
      #else
         #error ***** PERFUTIL.H: Unknown 32-bit Windows platform *****
      #endif
   #endif

   #define CONST64(const_value)       const_value
#elif defined(_AIX)
   // *** AIX ***
   #define CONST64(const_value)       const_value##ll
#else
   // *** LINUX ***
   #include "itypes.h"

   #ifndef PERFDD_INC
      #include <sys/types.h>
      #include <unistd.h>
   #endif

   typedef UINTPTR  UINT_PTR;

   #define CONST64(const_value)       const_value##LL
#endif






//
// Forward declarations
// ********************
//

// CPU Utilization (Above Idle)
typedef struct _ai_counters        AI_COUNTERS,        ai_counters_t;

// CPI Measurement
typedef struct _cpi_data           CPI_DATA,           cpi_data_t;

// CPU Utilization Measurement
typedef struct _cpuutil_data       CPUUTIL_DATA,       cpuutil_data_t;

// Trace Hooks
typedef struct _VDATA              VDATA,              vdata_t;

// Performance Counters
typedef union  _perfctr_desc       PERFCTR_DESC,       perfctr_desc_t;
typedef struct _perfctr_data       PERFCTR_DATA,       perfctr_data_t;
typedef struct _perfctr_event_info PERFCTR_EVENT_INFO, perfctr_event_info_t;
#if defined(_AIX)
typedef struct _pmu_data           PMU_DATA,           pmu_data_t;
typedef struct _pmsvcs_state       PMSVCS_STATE,       pmsvcs_state_t;
#endif

// Callstack sampling
typedef struct _scs_callstack_data scs_callstack_data;
typedef int (*scs_callback)(void *env, scs_callstack_data *callstack_data);

// PU Worker Thread
typedef struct _pu_worker_thread_args pu_worker_thread_args;
typedef int (*pu_worker_thread_create)(pu_worker_thread_args *);

// NRM2 file parser
typedef struct _trace_hook         TRACE_HOOK,         trace_hook_t;
typedef struct _nrm2_file_info     NRM2_FILE_INFO,     nrm2_file_info_t;

// Kernel Information
#if defined(_WINDOWS) || defined(_LINUX)
typedef struct _pi_kernel_info     PI_KERNEL_INFO,     pi_kernel_info_t;
#endif


//
// Exported APIs
// *************
//
//#undef PERFDD_INC
#ifndef PERFDD_INC
#ifdef __cplusplus
extern "C" {
#endif

#if defined(_LINUX)
   #define __cdecl
   typedef int           HANDLE;
#elif defined(_AIX)
   typedef void *  HANDLE;
#endif

// PU Worker
int __cdecl init_pu_worker(pu_worker_thread_create thread_create_callback, void *extraArgs);
void * __cdecl perfutil_worker(void *data);

//
// Miscellaneous
// -------------
//
void   __cdecl printf_se(const char * format, ...);
void   __cdecl printf_so(const char * format, ...);
int    __cdecl StrToUlong(const char * s, UINT32 * result);
const char * __cdecl RcToString(const int rc);
#if defined(_WINDOWS)
BOOL   __cdecl ProcessIsWow64(HANDLE process);
void   __cdecl SingleStep(void);
void   __cdecl BreakPoint(void);
void   __cdecl Serialize(void);
#endif
#if defined(_WINDOWS) || defined(_AIX)
BOOL   __cdecl pi_is_64bit_os(void);
BOOL   __cdecl pi_is_32bit_os(void);
#endif
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef void (__cdecl * PFN_printf_se) (const char *, ...);
typedef void (__cdecl * PFN_printf_so) (const char *, ...);
typedef int  (__cdecl * PFN_StrToUlong)(const char *, UINT32 *);
typedef const char * (__cdecl * PFN_RcToString) (int);
#if defined(_WINDOWS)
typedef BOOL  (__cdecl * PFN_ProcessIsWow64) (HANDLE);
typedef void  (__cdecl * PFN_SingleStep) (void);
typedef void  (__cdecl * PFN_BreakPoint) (void);
typedef void  (__cdecl * PFN_Serialize) (void);
#endif
#if defined(_WINDOWS) || defined(_AIX)
typedef BOOL  (__cdecl * PFN_pi_is_64bit_os) (void);
typedef BOOL  (__cdecl * PFN_pi_is_32bit_os) (void);
#endif

#if defined(_LINUX)
pid_t  pi_gettid(void);
#endif

//
// PerfUtil.dll and Perfdd.sys access/information
// ----------------------------------------------
//
UINT32 __cdecl GetDLLVersionEx(void);
UINT32 __cdecl GetDDVersionEx(void);
HANDLE __cdecl OpenDevice(char * DriverName);
int    __cdecl IsDeviceDriverInstalled(void);
#if defined(_WINDOWS)
int    __cdecl IsDebuggerHookedUp(void);
//
// IsLimitedFunctionMode()
// ***********************
//
// Return values:
// - Zero:     Not Limited Function Mode (ie. Full Function Mode)
// - Non-zero: Limited Function Mode
//             1: Driver not loaded
//             2: 32-bit, driver loaded, can't hook dispatcher (unrecognized kernel)
//             3: 64-bit, driver loaded, can't hook dispatched, no debugger
//             4: 64-bit, driver loaded, can't hook dispatched, debugger (unrecognized kernel)
//
int    __cdecl IsLimitedFunctionMode(void);

void   __cdecl DriverDebugPrintOn(void);
void   __cdecl DriverDebugPrintOff(void);
int    __cdecl IsDriverDebugPrintOn(void);
const char * __cdecl PiGetDriverLoadDateTime(void);
const char * __cdecl PiGetDriverBuildDateTime(void);
const char * __cdecl PiGetLoadedDriverPath(void);
#endif
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef UINT32 (__cdecl * PFN_GetDLLVersionEx) (void);
typedef UINT32 (__cdecl * PFN_GetDDVersionEx) (void);
typedef HANDLE (__cdecl * PFN_OpenDevice) (char *);
typedef int    (__cdecl * PFN_IsDeviceDriverInstalled) (void);
#if defined(_WINDOWS)
typedef int    (__cdecl * PFN_IsDebuggerHookedUp) (void);
typedef int    (__cdecl * PFN_IsLimitedFunctionMode) (void);
typedef void   (__cdecl * PFN_DriverDebugPrintOn) (void);
typedef void   (__cdecl * PFN_DriverDebugPrintOff) (void);
typedef int    (__cdecl * PFN_IsDriverDebugPrintOn) (void);
typedef const char * (__cdecl * PFN_PiGetDriverLoadDateTime) (void);
typedef const char * (__cdecl * PFN_PiGetDriverBuildDateTime) (void);
typedef const char * (__cdecl * PFN_PiGetLoadedDriverPath) (void);
#endif

//
// System-wide function support/query
// ----------------------------------
//
int    __cdecl IsTprofSupported(void);
int    __cdecl IsEventTprofSupported(void);
int    __cdecl IsTprofActive(void);
int    __cdecl IsItraceSupported(void);
int    __cdecl IsRmssSupported(void);
int    __cdecl IsAiSupported(void);
int    __cdecl IsAiActive(void);
int    __cdecl IsCpiSupported(void);
int    __cdecl IsCallstackSamplingSupported(void);
int    __cdecl IsEventCallstackSamplingSupported(void);
int    __cdecl IsCallstackSamplingActive(void);
int    __cdecl IsPerfCtrAccessSupported(void);
int    __cdecl IsPerfCtrEventAccessSupported(void);
int    __cdecl AreInterruptHooksSupported(void);
int    __cdecl IsDispatchHookSupported(void);
int    __cdecl AreInterruptsHooked(void);
int    __cdecl IsDispatcherHooked(void);
int    __cdecl IsVirtualMachine(void);
#if defined(_WINDOWS)
#define HAVE_IsTscInvariant
#define HAVE_IsAperfSupported
#define HAVE_AreFixedFunctionPerfCtrsAvailable
int    __cdecl IsTscInvariant(void);
int    __cdecl IsAperfSupported(void);
int    __cdecl AreFixedFunctionPerfCtrsAvailable(void);
#endif
void * __cdecl GetSystemAddressRangeStart(void);                     // Not supported on Linux
#if defined(_WINDOWS) || defined(_LINUX)
int    __cdecl pi_get_kernel_info(pi_kernel_info_t * pki);
#endif
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_IsTprofSupported) (void);
typedef int (__cdecl * PFN_IsEventTprofSupported) (void);
typedef int (__cdecl * PFN_IsTprofActive) (void);
typedef int (__cdecl * PFN_IsItraceSupported) (void);
typedef int (__cdecl * PFN_IsRmssSupported) (void);
typedef int (__cdecl * PFN_IsAiSupported) (void);
typedef int (__cdecl * PFN_IsAiActive) (void);
typedef int (__cdecl * PFN_IsCpiSupported) (void);
typedef int (__cdecl * PFN_IsCallstackSamplingSupported) (void);
typedef int (__cdecl * PFN_IsEventCallstackSamplingSupported) (void);
typedef int (__cdecl * PFN_IsCallstackSamplingActive) (void);
typedef int (__cdecl * PFN_IsPerfCtrAccessSupported) (void);
typedef int (__cdecl * PFN_IsPerfCtrEventAccessSupported) (void);
typedef int (__cdecl * PFN_AreInterruptHooksSupported) (void);
typedef int (__cdecl * PFN_IsDispatchHookSupported) (void);
typedef int (__cdecl * PFN_AreInterruptsHooked) (void);
typedef int (__cdecl * PFN_IsDispatcherHooked) (void);
typedef int (__cdecl * PFN_IsVirtualMachine) (void);
#if defined(_WINDOWS)
typedef int (__cdecl * PFN_IsTscInvariant)(void);
typedef int (__cdecl * PFN_IsAperfSupported)(void);
typedef int (__cdecl * PFN_AreFixedFunctionPerfCtrsAvailable)(void);
#endif
typedef void * (__cdecl * PFN_GetSystemAddressRangeStart) (void);
#if defined(_WINDOWS) || defined(_LINUX)
typedef int (__cdecl * PFN_pi_get_kernel_info) (pi_kernel_info_t *);
#endif

//
// Processor information/affinity
// ------------------------------
//
int    __cdecl GetProcessorType(void);
#if defined(_WINDOWS)
int    __cdecl GetProcessorFamily(void);
#endif
int    __cdecl GetProcessorArchitecture(void);
const char * __cdecl GetProcessorArchitectureString(void);
int    __cdecl GetProcessorManufacturer(void);
const char * __cdecl GetProcessorManufacturerString(void);
int    __cdecl GetProcessorByteOrdering(void);
int    __cdecl GetProcessorWordSize(void);
int    __cdecl GetLogicalProcessorsPerPackage(void);                 // Not supported on Linux
int    __cdecl GetCoresPerPackage(void);                             // Not supported on Linux
UINT32 __cdecl GetProcessorSignature(void);
UINT32 __cdecl GetProcessorFeatureFlagsEdx(void);
UINT32 __cdecl GetProcessorFeatureFlagsEcx(void);
UINT32 __cdecl GetProcessorExtendedFeatures(void);
INT64  __cdecl GetProcessorSpeed(void);
int    __cdecl GetFrontSideBusSpeed(void);                           // Not supported on Linux
#if defined(_WINDOWS)
#define HAVE_GetTscSpeed
INT64  __cdecl GetTscSpeed(void);
#endif
#if defined(_AIX)
UINT64 __cdecl pi_msec_to_tbticks(int ms);
int    __cdecl pi_tbticks_to_msec(UINT64 tb);
INT64  __cdecl pi_get_timebase_speed(void);
int    __cdecl pi_get_max_logical_processor_number(void);
int    __cdecl pi_is_logical_processor_online(int lcpu);
int    __cdecl pi_lcpu_to_pcpu(int lcpu);
int    __cdecl pi_pcpu_to_lcpu(int cpu);
int    __cdecl pi_lcpu_to_bcpu(int lcpu);
int    __cdecl pi_bcpu_to_lcpu(int bcpu);
int    __cdecl pi_get_bind_processor_list(int cpu_list[], int elements);
int    __cdecl pi_get_configured_processor_count(void);
#endif
int    __cdecl InitScs(UINT32 requested_rate, scs_callback callback);
int    __cdecl SetScsRate(UINT32 requested_rate);
int    __cdecl ScsInitThread(UINT32 scs_active, int tid);
int    __cdecl ScsUninitThread(int tid);
int    __cdecl ScsOn();
int    __cdecl ScsOff();
int    __cdecl ScsPause();
int    __cdecl ScsResume();
int    __cdecl GetCurrentCpuNumber(void);
int    __cdecl PiGetActiveProcessorCount(void);
int    __cdecl GetActiveProcessorList(int cpu_list[], int elements);
UINT64 __cdecl GetActiveProcessorAffinityMask(void);
int    __cdecl GetPhysicalProcessorCount(void);
UINT64 __cdecl GetPhysicalProcessorAffinityMask(void);
#if defined(_WINDOWS)
int    __cdecl PiGetApicIdForProcessor(int processor);
#endif
int    __cdecl SwitchToProcessor(int cpu, UINT64 * PrevAffinityMask);
int    __cdecl IsSystemSmp(void);
int    __cdecl IsSystemUni(void);
int    __cdecl IsProcessor32Bits(void);
int    __cdecl IsProcessor64Bits(void);
int    __cdecl IsProcessorLittleEndian(void);
int    __cdecl IsProcessorBigEndian(void);
int    __cdecl IsProcessorIntel(void);
int    __cdecl IsProcessorAmd(void);
int    __cdecl IsProcessorX86(void);             // Intel or AMD
int    __cdecl IsProcessorX64(void);             // Intel or AMD
int    __cdecl IsProcessorAMD64(void);           // AMD ***** DEPRECATED *****
int    __cdecl IsProcessorEM64T(void);           // Intel ***** DEPRECATED *****
int    __cdecl IsProcessorAmd64(void);           // AMD
int    __cdecl IsProcessorIntel64(void);         // Intel
int    __cdecl IsProcessorP4(void);              // Intel
int    __cdecl IsProcessorP15(void);             // AMD
int    __cdecl IsProcessorP6(void);              // Intel or AMD
int    __cdecl IsProcessorPentiumM(void);        // Intel
int    __cdecl IsProcessorCoreSD(void);          // Intel Core Solo/Duo
int    __cdecl IsProcessorCore(void);            // Intel Core 2
int    __cdecl IsProcessorNehalem(void);         // Intel Nehalem (Core i7/Core i5)
int     __cdecl IsProcessorSandybridge(void);   // Intel Sandybridge (Core i7-2xxx/Core i5-2xxx)  
#if defined(_WINDOWS)
int    __cdecl IsProcessorWestmere(void);        // Intel Westmere
int    __cdecl IsProcessorSandyBridge(void);     // Intel Sandy Bridge
#endif
int    __cdecl IsProcessorAtom(void);            // Intel Atom
int    __cdecl IsProcessorOpteron(void);         // AMD
int    __cdecl IsProcessorAthlon(void);          // AMD
int    __cdecl IsProcessorPPC64(void);
int    __cdecl IsProcessorMultiCore(void);       // Intel or AMD
int    __cdecl IsHyperThreadingSupported(void);  // Intel (so far)
int    __cdecl IsHyperThreadingEnabled(void);    // Intel (so far)
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int    (__cdecl * PFN_GetProcessorType) (void);
#if defined(_WINDOWS)
typedef int    (__cdecl * PFN_GetProcessorFamily) (void);
#endif
typedef int    (__cdecl * PFN_GetProcessorArchitecture) (void);
typedef const char * (__cdecl * PFN_GetProcessorArchitectureString) (void);
typedef int    (__cdecl * PFN_GetProcessorManufacturer) (void);
typedef const char * (__cdecl * PFN_GetProcessorManufacturerString) (void);
typedef int    (__cdecl * PFN_GetProcessorByteOrdering) (void);
typedef int    (__cdecl * PFN_GetProcessorWordSize) (void);
typedef int    (__cdecl * PFN_GetLogicalProcessorsPerPackage) (void);
typedef int    (__cdecl * PFN_GetCoresPerPackage) (void);
typedef UINT32 (__cdecl * PFN_GetProcessorSignature) (void);
typedef UINT32 (__cdecl * PFN_GetProcessorFeatureFlagsEdx) (void);
typedef UINT32 (__cdecl * PFN_GetProcessorFeatureFlagsEcx) (void);
typedef UINT32 (__cdecl * PFN_GetProcessorExtendedFeatures) (void);
typedef INT64  (__cdecl * PFN_GetProcessorSpeed) (void);
typedef int    (__cdecl * PFN_GetFrontSideBusSpeed) (void);
#if defined(_WINDOWS)
typedef INT64  (__cdecl * PFN_GetTscSpeed) (void);
#endif
#if defined(_AIX)
typedef UINT64 (__cdecl * PFN_pi_msec_to_tbticks) (int);
typedef int    (__cdecl * PFN_pi_tbticks_to_msec) (UINT64);
typedef INT64  (__cdecl * PFN_pi_get_timebase_speed) (void);
typedef int    (__cdecl * PFN_pi_get_max_logical_processor_number) (void);
typedef int    (__cdecl * PFN_pi_is_logical_processor_online) (int);
typedef int    (__cdecl * PFN_pi_lcpu_to_pcpu) (int);
typedef int    (__cdecl * PFN_pi_pcpu_to_lcpu) (int);
typedef int    (__cdecl * PFN_pi_lcpu_to_bcpu) (int);
typedef int    (__cdecl * PFN_pi_bcpu_to_lcpu) (int);
typedef int    (__cdecl * PFN_pi_get_bind_processor_list) (int *, int);
typedef int    (__cdecl * PFN_pi_get_configured_processor_count) (void);
#endif
typedef int    (__cdecl * PFN_GetCurrentCpuNumber) (void);
typedef int    (__cdecl * PFN_PiGetActiveProcessorCount) (void);
typedef int    (__cdecl * PFN_GetActiveProcessorList) (int *, int);
typedef UINT64 (__cdecl * PFN_GetActiveProcessorAffinityMask) (void);
typedef int    (__cdecl * PFN_GetPhysicalProcessorCount) (void);
typedef UINT64 (__cdecl * PFN_GetPhysicalProcessorAffinityMask) (void);
#if defined(_WINDOWS)
typedef int    (__cdecl * PFN_PiGetApicIdForProcessor) (int);
#endif
typedef int    (__cdecl * PFN_SwitchToProcessor) (int, UINT64 *);
typedef int    (__cdecl * PFN_IsSystemSmp) (void);
typedef int    (__cdecl * PFN_IsSystemUni) (void);
typedef int    (__cdecl * PFN_IsProcessor32Bits) (void);
typedef int    (__cdecl * PFN_IsProcessor64Bits) (void);
typedef int    (__cdecl * PFN_IsProcessorLittleEndian) (void);
typedef int    (__cdecl * PFN_IsProcessorBigEndian) (void);
typedef int    (__cdecl * PFN_IsProcessorIntel) (void);
typedef int    (__cdecl * PFN_IsProcessorAmd) (void);
typedef int    (__cdecl * PFN_IsProcessorX86) (void);
typedef int    (__cdecl * PFN_IsProcessorX64) (void);
typedef int    (__cdecl * PFN_IsProcessorAMD64) (void);  // ***** DEPRECATED *****
typedef int    (__cdecl * PFN_IsProcessorEM64T) (void);  // ***** DEPRECATED *****
typedef int    (__cdecl * PFN_IsProcessorAmd64) (void);
typedef int    (__cdecl * PFN_IsProcessorIntel64) (void);
typedef int    (__cdecl * PFN_IsProcessorP4) (void);
typedef int    (__cdecl * PFN_IsProcessorP15) (void);
typedef int    (__cdecl * PFN_IsProcessorP6) (void);
typedef int    (__cdecl * PFN_IsProcessorPentimM) (void);
typedef int    (__cdecl * PFN_IsProcessorCoreSD) (void);
typedef int    (__cdecl * PFN_IsProcessorCore) (void);
typedef int    (__cdecl * PFN_IsProcessorNehalem) (void);
#if defined(_WINDOWS)
typedef int    (__cdecl * PFN_IsProcessorWestmere) (void);
typedef int    (__cdecl * PFN_IsProcessorSandyBridge) (void);
#endif
typedef int    (__cdecl * PFN_IsProcessorAtom) (void);
typedef int    (__cdecl * PFN_IsProcessorOpteron) (void);
typedef int    (__cdecl * PFN_IsProcessorAthlon) (void);
typedef int    (__cdecl * PFN_IsProcessorPPC64) (void);
typedef int    (__cdecl * PFN_IsProcessorMultiCore) (void);
typedef int    (__cdecl * PFN_IsHyperThreadingSupported) (void);
typedef int    (__cdecl * PFN_IsHyperThreadingEnabled) (void);

//
// NRM2 file access
// ----------------
//
#if defined (_WINDOWS)
int    __cdecl Nrm2OpenTraceFile(const char * fnm);
void   __cdecl Nrm2CloseTraceFile(void);
trace_hook_t * __cdecl Nrm2GetNextHook(void);
int    __cdecl Nrm2GetFileInfo(NRM2_FILE_INFO * fi);
void   __cdecl Nrm2SetQuietMode(int on);
void   __cdecl Nrm2SetRawDataMode(int on);
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int       (__cdecl * PFN_Nrm2OpenTraceFile) (const char *);
typedef void      (__cdecl * PFN_Nrm2CloseTraceFile) (void);
typedef trace_hook_t * (__cdecl * PFN_Nrm2GetNextHook) (void);
typedef int       (__cdecl * PFN_Nrm2GetFileInfo) (NRM2_FILE_INFO *);
typedef void      (__cdecl * PFN_Nrm2SetQuietMode)(int);
typedef void      (__cdecl * PFN_Nrm2SetRawDataMode) (int);
#endif

//
// Cycles-to-time conversion
// -------------------------
// TODO: depricate this API
// There should be a single function for getting standard time
void   __cdecl GetCycles(UINT64 * cycles);
UINT64 __cdecl ReadCycles();
#if defined(_AIX)
UINT64 __cdecl pi_read_timebase(void);
#endif
double __cdecl CyclesToSeconds(double cycles_start, double cycles_end);
double __cdecl CyclesToMilliseconds(double cycles_start, double cycles_end);
double __cdecl CyclesToMicroseconds(double cycles_start, double cycles_end);
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef void   (__cdecl * PFN_GetCycles) (UINT64 *);
typedef UINT64 (__cdecl * PFN_ReadCycles) (void);
#if defined(_AIX)
typedef UINT64 (__cdecl * PFN_pi_read_timebase) (void);
#endif
typedef double (__cdecl * PFN_CyclesToSeconds) (double, double);
typedef double (__cdecl * PFN_CyclesToMilliseconds) (double, double);
typedef double (__cdecl * PFN_CyclesToMicroseconds) (double, double);


//
// SWTRACE controls
// ----------------
// TODO: clean up this API and remove unwanted functions
#if defined(_WINDOWS) || defined(_LINUX)
#define HAVE_TraceGetIntervalId
#endif
int    __cdecl TraceInit(UINT32 size);
int    __cdecl TraceSetScope(int scope, int pid);
int    __cdecl TraceSetMetrics(int metric_cnt, int metrics[], int priv_level);  // Not supported on Linux
int    __cdecl TraceSetTprofEvent(int cpu, int event, int event_cnt, int priv_level);
int    __cdecl TraceEnable(int major);
int    __cdecl TraceDisable(int major);
int    __cdecl TraceAllowMultiMetric(int major);                                // Not supported on Linux
int    __cdecl TraceDisallowMultiMetric(int major);                             // Not supported on Linux
int    __cdecl TraceOn(void);
int    __cdecl TraceOff(void);
int    __cdecl TraceSuspend(void);
int    __cdecl TraceResume(void);
int    __cdecl TraceTerminate(void);
int    __cdecl TraceDiscard(void);
int    __cdecl TraceQueryMajor(int major);
int    __cdecl TraceGetMaxNumberOfMetrics(void);
int    __cdecl TraceWriteBufferToFile(const char * fn);
int    __cdecl IsTraceOn(void);
int    __cdecl IsTraceActive(void);
int    __cdecl SetProfilerRate(UINT32 TprofRate);
int    __cdecl TraceGetIntervalId(void);
#if defined(_LINUX)
int    __cdecl TraceSetMode(UINT32 trace_mode);
int    __cdecl TraceSetName(char * raw_file_name);
char * __cdecl TraceGetName(void);
int    __cdecl IsContMode(void);
int    __cdecl IsWrapMode(void);
int    __cdecl TraceSetMteSize(int size);
int    __cdecl TraceSetSectionSize(int size);
void   __cdecl SetTraceAnonMTE(int val);
#endif
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_TraceInit) (UINT32, int);
typedef int (__cdecl * PFN_TraceSetMetrics) (int, int *, int);
typedef int (__cdecl * PFN_TraceSetTprofEvent) (int, int, int, int);
typedef int (__cdecl * PFN_TraceEnable) (int);
typedef int (__cdecl * PFN_TraceDisable) (int);
typedef int (__cdecl * PFN_TraceAllowMultiMetric) (int);
typedef int (__cdecl * PFN_TraceDisallowMultiMetric) (int);
typedef int (__cdecl * PFN_TraceOn) (void);
typedef int (__cdecl * PFN_TraceOff) (void);
typedef int (__cdecl * PFN_TraceSuspend) (void);
typedef int (__cdecl * PFN_TraceResume) (void);
typedef int (__cdecl * PFN_TraceTerminate) (void);
typedef int (__cdecl * PFN_TraceDiscard) (void);
typedef int (__cdecl * PFN_TraceQueryMajor) (int);
typedef int (__cdecl * PFN_TraceGetMaxNumberOfMetrics) (void);
typedef int (__cdecl * PFN_TraceWriteBufferToFile) (const char *);
typedef int (__cdecl * PFN_IsTraceOn) (void);
typedef int (__cdecl * PFN_IsTraceActive) (void);
typedef int (__cdecl * PFN_SetProfilerRate) (UINT32);
typedef int (__cdecl * PFN_TraceGetIntervalId) (void);
#if defined(_LINUX)
typedef int (__cdecl * PFN_TraceSetMode) (int);
typedef int (__cdecl * PFN_TraceSetName) (char *);
typedef char * (__cdecl * PFN_TraceGetName) (void);
typedef int (__cdecl * PFN_IsContMode) (void);
typedef int (__cdecl * PFN_IsWrapMode) (void);
typedef int (__cdecl * PFN_TraceSetMteSize) (int);
typedef int (__cdecl * PFN_TraceSetSectionSize) (int);
#endif

//
// SWTRACE user hooks
// ------------------
// TODO: I think all hooks belong in bputil_common.h, that is where the structures are declared
// 		All hook read/write as well as buffers should be there
int __cdecl TraceHook(int cpu, UINT32 major, UINT32 minor, UINT64 ucnt, UINT64 vcnt, UINT64 intvals[], UINT64 vlen[], char *vdata[]);
int    __cdecl WriteAddressHookEx(UINT32 major, UINT32 extminor, UINT32 ulType, void * addr);  // Not supported on Linux
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_TraceHook) (UINT32, UINT32, INT32, INT32, ...);
typedef int (__cdecl * PFN_WriteAddressHookEx) (UINT32, UINT32, UINT32, void *);

//
// ITrace (Instruction Trace) controls
// -----------------------------------
//
int    __cdecl ITraceInit(UINT32 type);
int    __cdecl ITraceTerminate(void);
int    __cdecl ITraceOn(void);
int    __cdecl ITraceOff(void);
int    __cdecl ITraceEnableAll(void);
int    __cdecl ITraceEnableCurrent(void);
int    __cdecl ITraceEnablePid(UINT32 pid);
int    __cdecl ITraceEnableGreater(UINT32 start_pid);
int    __cdecl ITraceEnableRange(UINT32 start_pid, UINT32 end_pid);
int    __cdecl ITraceEnableList(UINT32 pid_list[], int pid_cnt);
int    __cdecl ITraceSetSkipCount(UINT32 timer_skip_count);
int    __cdecl ITraceDisable(void);
// ** DEPRECATED APIs **
int    __cdecl ITrace_Set_Skip_Count(UINT32 timer_skip_count);
int    __cdecl ITrace_On_CurPID(void);
int    __cdecl ITrace_On_GTPID(UINT32 pid);
int    __cdecl ITrace_On_PIDList(UINT32 pid_list[], int pid_cnt);
int    __cdecl ITrace_Off(void);
// ** DEPRECATED APIs **
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int  (__cdecl * PFN_ITraceInit) (UINT32);
typedef int  (__cdecl * PFN_ITraceTerminate) (void);
typedef int  (__cdecl * PFN_ITraceOn) (void);
typedef int  (__cdecl * PFN_ITraceOff) (void);
typedef int  (__cdecl * PFN_ITraceEnableAll) (void);
typedef int  (__cdecl * PFN_ITraceEnableCurrent) (void);
typedef int  (__cdecl * PFN_ITraceEnablePID) (UINT32);
typedef int  (__cdecl * PFN_ITraceEnableGreater) (UINT32);
typedef int  (__cdecl * PFN_ITraceEnableRange) (UINT32, UINT32);
typedef int  (__cdecl * PFN_ITraceEnableList) (UINT32 *, int);
typedef int  (__cdecl * PFN_ITraceSetSkipCount) (UINT32);
typedef int  (__cdecl * PFN_ITraceDisable) (void);
// ** DEPRECATED APIs **
typedef int (__cdecl * PFN_ITrace_Set_Skip_Count) (UINT32);
typedef int (__cdecl * PFN_ITrace_On_CurPID) (void);
typedef int (__cdecl * PFN_ITrace_On_GTPID) (UINT32);
typedef int (__cdecl * PFN_ITrace_On_PIDList) (UINT32 *, int);
typedef int (__cdecl * PFN_ITrace_Off) (void);
// ** DEPRECATED APIs **

//
// Hardware performance counters and MSR access
// --------------------------------------------
//
int    __cdecl OpenPerfCounterFacilityExclusive(void);               // Not supported on Linux
int    __cdecl ClosePerfCounterFacility(void);                       // Not supported on Linux
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_OpenPerfCounterFacilityExclusive) (void);
typedef int (__cdecl * PFN_ClosePerfCounterFacility) (void);

// *** Allocate/deallocate memory for reading counters/events
UINT64 * __cdecl AllocCtrValueArray(int * size);
void   __cdecl FreeCtrValueArray(UINT64 * cv);
void   __cdecl ZeroCtrValueArray(UINT64 * cv);
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef UINT64 * (__cdecl * PFN_AllocCtrValueArray) (int *);
typedef void (__cdecl * PFN_FreeCtrValueArray) (UINT64 *);
typedef void (__cdecl * PFN_ZeroCtrValueArray) (UINT64 *);


// *** Performance counter events information
int    __cdecl GetNumberOfPerfCounterEventsSupported(void);
int    __cdecl GetNumberOfSimultaneousPerfCounterEventsSupported(void);
int    __cdecl IsPerfCounterEventSupported(int event_id);
int    __cdecl IsPerfCounterEventActive(int event_id);
int    __cdecl AnyPerfCounterEventActive(void);
int    __cdecl GetSupportedPerfCounterEventIdList(int * * event_id_list);
int    __cdecl GetSupportedPerfCounterEventNameList(const char * * * event_name_list);
int    __cdecl GetPerfCounterEventsInfo(const PERFCTR_EVENT_INFO * * event_info_list);
int    __cdecl GetPerfCounterEventIdFromName(const char * event_name);
const char * __cdecl GetPerfCounterEventNameFromId(int event_id);
int    __cdecl GetPerfCounterEventInfoIndexFromId(int event_id);
int    __cdecl GetPerfCounterEventInfoIndexFromName(const char * event_name);
const char * __cdecl GetPerfCounterEventInfoFromId(int event_id);
const char * __cdecl GetPerfCounterEventInfoFromName(const char * event_name);
#if defined(_AIX)
int    __cdecl pi_is_event_name_valid(const char * event_name);
int    __cdecl pi_get_pmsvcs_state(PMSVCS_STATE * ps);
#endif
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_GetNumberOfPerfCounterEventsSupported) (void);
typedef int (__cdecl * PFN_GetNumberOfSimultaneousPerfCounterEventsSupported) (void);
typedef int (__cdecl * PFN_IsPerfCounterEventSupported) (int);
typedef int (__cdecl * PFN_IsPerfCounterEventActive) (int);
typedef int (__cdecl * PFN_AnyPerfCounterEventActive) (void);
typedef int (__cdecl * PFN_GetPerfCounterEventIdList) (const int * *);
typedef int (__cdecl * PFN_GetPerfCounterEventNameList) (const char * const * *);
typedef int (__cdecl * PFN_GetPerfCounterEventsInfo) (const PERFCTR_EVENT_INFO * *);
typedef int (__cdecl * PFN_GetPerfCounterEventIdFromName) (char *);
typedef const char * (__cdecl * PFN_GetPerfCounterEventNameFromId) (int);
typedef int (__cdecl * PFN_GetPerfCounterEventInfoIndexFromId) (int);
typedef int (__cdecl * PFN_GetPerfCounterEventInfoIndexFromName) (const char *);
typedef const PERFCTR_EVENT_INFO * (__cdecl * PFN_GetPerfCounterEventInfoFromId) (int);
typedef const PERFCTR_EVENT_INFO * (__cdecl * PFN_GetPerfCounterEventInfoFromName) (const char *);
#if defined(_AIX)
typedef int (__cdecl * PFN_pi_is_event_name_valid) (const char *);
typedef int (__cdecl * PFN_pi_get_pmsvcs_state) (PMSVCS_STATE *);
#endif

// *** Performance counter events access
int    __cdecl InitPerfCounterEvent(int event_id, int priv_level);
int    __cdecl TerminatePerfCounterEvent(int event_id);
int    __cdecl StopPerfCounterEvent(int event_id);
int    __cdecl ResumePerfCounterEvent(int event_id);
int    __cdecl ResetPerfCounterEvent(int event_id);
int    __cdecl ReadPerfCounterEvent(int event_id, UINT64 * ctr_value, UINT64 * cyc_value);
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_InitPerfCounterEvent) (int, int);
typedef int (__cdecl * PFN_TerminatePerfCounterEvent) (int);
typedef int (__cdecl * PFN_StopPerfCounterEvent) (int);
typedef int (__cdecl * PFN_ResumePerfCounterEvent) (int);
typedef int (__cdecl * PFN_ResetPerfCounterEvent) (int);
typedef int (__cdecl * PFN_ReadPerfCounterEvent) (int, UINT64 *, UINT64 *);

//
// CPU utilization (Above Idle)
// ----------------------------
//
int    __cdecl AiInit(void);
void   __cdecl AiTerminate(void);
int    __cdecl AiGetCounters(AI_COUNTERS * ac);
AI_COUNTERS * __cdecl AiAllocCounters(int * size);
void   __cdecl AiFreeCounters(AI_COUNTERS * ac);
void   __cdecl AiZeroCounters(AI_COUNTERS * ac);
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int  (__cdecl * PFN_AiInit) (void);
typedef void (__cdecl * PFN_AiTerminate) (void);
typedef int  (__cdecl * PFN_AiGetCounters) (AI_COUNTERS *);
typedef AI_COUNTERS * (__cdecl * PFN_AiAllocCounters) (int *);
typedef void (__cdecl * PFN_AiFreeCounters) (AI_COUNTERS *);
typedef void (__cdecl * PFN_AiZeroCounters) (AI_COUNTERS *);

//
// CPU Utilization Measurement
// ---------------------------
// !!!!! THIS IS STILL UNDER DEVELOPMENT AND IT MAY CHANGE !!!!!
//
#if defined(_WINDOWS)
int    __cdecl CpuUtilInit(UINT32 trace_mode, void * * handle);
int    __cdecl CpuUtilTerminate(void * handle);
int    __cdecl CpuUtilStart(void * handle);
int    __cdecl CpuUtilGet(void * handle, const CPUUTIL_DATA * * cd);
int    __cdecl CpuUtilGetAndTrace(void * handle, int minor_code, UINT32 trace_mode, const char * label, const CPUUTIL_DATA * * cd);
int    __cdecl CpuUtilSetTraceHookId(void * handle, int minor_code, const char * label);
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_CpuUtilInit) (UINT32, void * *);
typedef int (__cdecl * PFN_CpuUtilTerminate) (void *);
typedef int (__cdecl * PFN_CpuUtilStart) (void *);
typedef int (__cdecl * PFN_CpuUtilGet) (void *, const CPUUTIL_DATA * *);
typedef int (__cdecl * PFN_CpuUtilGetAndTrace) (void *, int, UINT32, const char *, const CPUUTIL_DATA * *);
typedef int (__cdecl * PFN_CpuUtilSetTraceHookId) (void * handle, int minor_code, const char * label);
#endif

//
// CPI Measurement
// ---------------
// !!!!! THIS IS STILL UNDER DEVELOPMENT AND IT MAY CHANGE !!!!!
//
#if defined(_WINDOWS)
int    __cdecl CpiInit(UINT32 mode, void * * handle);
int    __cdecl CpiTerminate(void * handle);
int    __cdecl CpiStart(void * handle);
int    __cdecl CpiGet(void * handle, const CPI_DATA * * cd);
int    __cdecl CpiGetAndTrace(void * handle, int minor_code, UINT32 flags, const char * label, const CPI_DATA * * cd);
int    __cdecl CpiSetTraceHookId(void * handle, int minor_code, const char * label);
UINT32 __cdecl CpiGetCurrentMode(void * handle);
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_CpiInit) (UINT32, void * *);
typedef int (__cdecl * PFN_CpiTerminate) (void *);
typedef int (__cdecl * PFN_CpiStart) (void *);
typedef int (__cdecl * PFN_CpiGet) (void *, const CPI_DATA * *);
typedef int (__cdecl * PFN_CpiGetAndTrace) (void *, int, UINT32, const char *, const CPI_DATA * *);
typedef int (__cdecl * PFN_CpiSetTraceHookId) (void * handle, int minor_code, const char * label);
typedef UINT32 (__cdecl * PFN_CpiGetCurrentMode) (void * handle);
#endif

//
// Per-Thread Time
// ---------------
//
#if defined(_WINDOWS) || defined(_LINUX)
int    __cdecl PttInit(int metric_cnt, int metrics[], int priv_level);
int    __cdecl PttInitThread(int tid);
int    __cdecl PttUnInitThread(int tid);
int    __cdecl PttTerminate(void);
int    __cdecl PttGetMetricData(UINT64 *metricsArray);
//-------------------------------------


/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_PttInit) (int, int *, int);
typedef int (__cdecl * PFN_PttTerminate) (void);
//-------------------------------------

#endif

//
// Java Callstack Sampling
// -----------------------
//
#define HAVE_CallstackSamplingSupport
int  __cdecl SetScsRate(UINT32 rate);
int  __cdecl SetScsDivisor(UINT32 divisor);
int  __cdecl SetTprofDivisor(UINT32 divisor);
#if defined(_LINUX)
uint32_t  __cdecl pi_get_default_scs_rate(void);
#endif
#if defined(_AIX)
int  __cdecl pi_pmctl_state(int req);
int  __cdecl GetScsDivisor(void);
#endif
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int  (__cdecl * PFN_SetScsDivisor) (UINT32);
typedef int  (__cdecl * PFN_SetTprofDivisor) (UINT32);
#if defined(_LINUX)
typedef uint32_t  (__cdecl * PFN_pi_get_default_scs_rate) (void);
#endif
#if defined(_AIX)
typedef int  (__cdecl * PFN_pi_pmctl_state) (int);
typedef int  (__cdecl * PFN_GetScsDivisor) (void);
#endif

//
// PM/Application Notifications
// ----------------------------
//
#if defined(_WINDOWS)
#define HAVE_PpmApis

typedef void (__cdecl * PFN_PERF_NOTIFY_CALLBACK) (UINT32 reason, void * context);

UINT32 __cdecl GetPmDisabledMask(void);    // See PM_DISABLED_* flags
BOOL __cdecl PmDisabledItrace(void);
BOOL __cdecl PmDisabledTprof(void);
BOOL __cdecl PmDisabledScs(void);
BOOL __cdecl PmDisabledAi(void);
BOOL __cdecl PmDisabledInterruptHooks(void);
BOOL __cdecl PmDisabledDispatchHook(void);
BOOL __cdecl PmDisabledSysCallHooks(void);
BOOL __cdecl PmDisabledPerfCounterEvents(void);

BOOL __cdecl TscFrequencySameAsRatedCpuFrequency(void);
int  __cdecl GetCurrentProcessorFrequency(int frequency[], int elements);
BOOL __cdecl IsSystemOnAc(void);
BOOL __cdecl IsSystemOnDc(void);
BOOL __cdecl IsProcessorThrottlingActive(void);
BOOL __cdecl IsAcProcessorThrottlingActive(void);
BOOL __cdecl IsDcProcessorThrottlingActive(void);
int  __cdecl DisableProcessorThrottling(void);
int  __cdecl DisableAcProcessorThrottling(void);
int  __cdecl DisableDcProcessorThrottling(void);
int  __cdecl GetAcProcessorThrottlingPolicyLimits(UINT32 * ac_min, UINT32 * ac_max);
int  __cdecl GetDcProcessorThrottlingPolicyLimits(UINT32 * dc_min, UINT32 * dc_max);
int  __cdecl GetAcProcessorThrottlingPolicyType(UINT32 * throttle_type);
int  __cdecl GetDcProcessorThrottlingPolicyType(UINT32 * throttle_type);
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_RegisterNotificationCallback) (PFN_PERF_NOTIFY_CALLBACK, void *, void * *);
typedef int (__cdecl * PFN_DeregisterNotificationCallback) (void *);

typedef UINT32 (__cdecl * PFN_GetPmDisabledMask) (void);
typedef BOOL (__cdecl * PFN_PmDisabledItrace) (void);
typedef BOOL (__cdecl * PFN_PmDisabledTprof) (void);
typedef BOOL (__cdecl * PFN_PmDisabledScs) (void);
typedef BOOL (__cdecl * PFN_PmDisabledAi) (void);
typedef BOOL (__cdecl * PFN_PmDisabledInterruptHooks) (void);
typedef BOOL (__cdecl * PFN_PmDisabledDispatchHook) (void);
typedef BOOL (__cdecl * PFN_PmDisabledSysCallHooks) (void);
typedef BOOL (__cdecl * PFN_PmDisabledPerfCounterEvents) (void);

typedef BOOL (__cdecl * PFN_TscFrequencySameAsRatedCpuFrequency) (void);
typedef int  (__cdecl * PFN_GetCurrentProcessorFrequency) (int *, int);
typedef BOOL (__cdecl * PFN_IsSystemOnAc) (void);
typedef BOOL (__cdecl * PFN_IsSystemOnDc) (void);
typedef BOOL (__cdecl * PFN_IsProcessorThrottlingActive) (void);
typedef BOOL (__cdecl * PFN_IsAcProcessorThrottlingActive) (void);
typedef BOOL (__cdecl * PFN_IsDcProcessorThrottlingActive) (void);
typedef int  (__cdecl * PFN_DisableProcessorThrottling) (void);
typedef int  (__cdecl * PFN_DisableAcProcessorThrottling) (void);
typedef int  (__cdecl * PFN_DisableDcProcessorThrottling) (void);
typedef int  (__cdecl * PFN_GetAcProcessorThrottlingPolicyLimits) (UINT32 *, UINT32 *);
typedef int  (__cdecl * PFN_GetDcProcessorThrottlingPolicyLimits) (UINT32 *, UINT32 *);
typedef int  (__cdecl * PFN_GetAcProcessorThrottlingPolicyType) (UINT32 *);
typedef int  (__cdecl * PFN_GetDcProcessorThrottlingPolicyType) (UINT32 *);
#endif

//
// Slow TSC Detection
// ------------------
//
#if defined(_WINDOWS)
#define HAVE_SlowTsc_Detection

int __cdecl PiBeginSlowTscDetection(void);
int __cdecl PiEndSlowTscDetection(void);
UINT32 __cdecl PiReadSlowTscCount(void);
BOOL __cdecl PiIsSlowTscDetectionActive(void);
/****************************************************/
/***** For use when dynamically loading the dll *****/
/****************************************************/
typedef int (__cdecl * PFN_PiBeginSlowTscDetection) (void);
typedef int (__cdecl * PFN_PiEndSlowTscDetection) (void);
typedef UINT32 (__cdecl * PFN_PiReadSlowTscCount) (void);
typedef BOOL (__cdecl * PFN_PiIsSlowTscDetectionActive) (void);
#endif

#ifdef __cplusplus
}
#endif
#endif // PERFDD_INC

int readSysError(const char *errStr);
int translateSysError(int syserr);

//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  Return codes                                                             //
//  ------------                                                             //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
enum pu_return_code {
#ifndef NO_ERROR
	NO_ERROR		= 0,
#endif
	PU_SUCCESS		= 0,
	PU_FAILURE		= -1,
	PU_ERROR_INVALID_FUNCTION		= -2,
	PU_ERR_NULL_PTR = -3,
	PU_ERR_FILE_ERROR = -4,
	PU_ERROR_ACCESS_DENIED		= -5,
	PU_ERR_NOT_INITIALIZED		= -6,
	PU_ERROR_NOT_ENOUGH_MEMORY		= -8,
	PU_ERROR_INVALID_PARAMETER		= -87,
	PU_ERROR_INSUFFICIENT_BUFFER		= -122,
	PU_ERROR_INTERNAL_ERROR		= -200,
	PU_ERROR_INCORRECT_STATE		= -201,
	PU_ERROR_NO_BUFFER		= -202,
	PU_ERROR_THREAD_NOT_FOUND		= -203,
	PU_ERROR_NO_TARGET_CPU		= -204,
	PU_ERROR_BUFFER_FULL		= -205,
	PU_ERROR_WRONG_PROCESSOR_TYPE		= -206,
	PU_ERROR_NOT_ALLOWED_ON_SMP		= -207,
	PU_ERROR_HOOK_TOO_LONG		= -208,
	PU_ERROR_DRIVER_NOT_LOADED		= -209,
	PU_ERROR_NOT_SUPPORTED		= -210,
	PU_ERROR_FACILITY_BUSY		= -211,
	PU_ERROR_FACILITY_NOT_OWNED		= -212,
	PU_ERROR_EVENT_NOT_SUPPORTED		= -213,
	PU_ERROR_HTT_ENABLED		= -214,
	PU_ERROR_JNI		= -215,
	PU_ERROR_INVALID_CPI_DATA		= -216,
	PU_ERROR_BUFFER_EXCEPTION		= -217,
	PU_ERROR_INSTALLING_REQ_HOOKS		= -218,
	PU_ERROR_INVALID_METRIC		= -219,
	PU_ERROR_MIXED_METRICS		= -220,
	PU_ERROR_STARTING_METRIC		= -221,
	PU_ERROR_INVALID_PRIV_LEVEL		= -222,
	PU_ERROR_DUPLICATE_METRICS		= -223,
	PU_ERROR_TOO_MANY_METRICS		= -224,
	PU_ERROR_EVENT_MAX_EXCEEDED		= -225,
	PU_ERROR_EVENT_ALL_CTRS_IN_USE		= -226,
	PU_ERROR_NOT_APIC_SYSTEM		= -227,
	PU_ERROR_STARTING_EVENT		= -228,
	PU_ERROR_EVENT_NOT_COUNTING		= -229,
	PU_ERROR_INVALID_EVENT		= -230,
	PU_ERROR_EVENT_ALREADY_COUNTING		= -231,
	PU_ERROR_DISPATCHER_NOT_HOOKED		= -232,
	PU_ERROR_NOT_VALID_HTT_ENABLED		= -233,
	PU_ERROR_NOT_VALID_IN_VM		= -234,
	PU_ERROR_PTT_USER_TABLE_FULL		= -235,
	PU_ERROR_SAMPLING_NOTIFY_MODE_ENABLED		= -236,
	PU_ERROR_DIDNT_GET_32BIT_MTE		= -237,
	PU_ERROR_EVENT_ALL_ESCR_IN_USE		= -238,
	PU_ERROR_NO_EVENT_DEFINITIONS		= -239,
	PU_ERROR_BUFFER_ALREADY_ALLOCATED		= -240,
	PU_ERROR_INSUFFICIENT_RESOURCES		= -241,
	PU_ERROR_CANT_MAP_BUFFER		= -242,
	PU_ERROR_BUFFER_NOT_MAPPED		= -243,
	PU_ERROR_BUFFER_ADDR_MISMATCH		= -244,
	PU_ERROR_PEBS_ENABLE_IN_USE		= -245,
	PU_ERROR_PEBS_MATRIX_VERT_IN_USE		= -246,
	PU_ERROR_INVALID_ARRAY_SIZE		= -247,
	PU_ERROR_PTT_REQUESTED_MODE_INVALID		= -248,
	PU_ERROR_PID_TOO_BIG		= -249,
	PU_ERROR_TID_TOO_BIG		= -250,
	PU_ERROR_INVALID_SIGNATURE		= -251,
	PU_ERROR_INCONSISTENT_PTT_STATE		= -252,
	PU_ERROR_NOT_ALL_ENABLED		= -253,
	PU_ERROR_REGISTRATION_TABLE_FULL		= -254,
	PU_ERROR_INVALID_THREAD		= -255,
	PU_ERROR_ENABLE_THREAD_PROFILING		= -256,
	PU_ERROR_READ_PERFORMANCE_DATA		= -257,
	PU_ERROR_UNABLE_TO_LOAD_REQUIRED_DLL		= -258,
#define DONT_USE_RC_259_SAME_AS_STATUS_PENDING
	PU_ERROR_REQUIRED_APIS_MISSING		= -260,
	PU_ERROR_PTT_TERMINATED_TABLE_OVF		= -261,
	PU_ERROR_PTT_TABLE_WILL_OVF		= -262,
	PU_ERROR_EVENT_ALL_EVTSEL_IN_USE		= -263,
	PU_ERROR_INCOMPATIBLE_OPTIONS		= -264,

	PU_ERROR_INVALID_PMC		= -265,
	PU_ERROR_ENTER_FUNNEL		= -266,
	PU_ERROR_REGISTRATION_FAILED		= -267,
	PU_ERROR_PMSVCS_STATE		= -268,
	PU_ERROR_UEVT_TO_SNAME		= -269,
	PU_ERROR_SNAME_TO_UEVT		= -270,
	PU_ERROR_SCS_EVENT_MISMATCH		= -271,
	PU_ERROR_SCS_RATE_MISMATCH		= -272,
	PU_ERROR_SCS_INVALID_PROFILING_MODE		= -273,
   PU_ERROR_SCS_CALLBACK_ERROR = -274,
};


//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  General-use constants                                                    //
//  ---------------------                                                    //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//

//
// Device driver and installed service names
// -----------------------------------------
//
#if defined(_WINDOWS)
   #define DRIVER_NAME    "PERFDD.SYS"
   #define SERVICE_NAME   "PERFDD"
#elif defined(_LINUX)
   #define DRIVER_NAME    "pidd"
#elif defined(_AIX)
   #define DRIVER_NAME    "pidd"
#endif

//
//
// For APIs that specify a processor number
// ----------------------------------------
//
// * ALL refers to all active processors.
//   - If HTT is enabled it refers to all LPs in the physical package.
//
// * THIS refers to the current processor.
//   - Note that the current processor *CAN* change from one instruction to the next.
//
// * LP0 refers to Logical Processor 0 in a physical package.
//   - If HTT is not enabled LP0 is equivalent to ALL.
//
// * LP1 refers to Logical Processor 1 in a physical package.
//   - If HTT is not enabled LP1 is undefined and set to 0 (no LP1 processors).
//
// * CORE0 refers to Processor Core 0 in a physical package.
//   - If single core processors then CORE0 is equivalent to ALL.
//
// * CORE1 refers to Processor Core 1 in a physical package.
//   - If single core processors then CORE1 is undefined and set to 0 (no Core 1 processors).
//
#define ALL_CPUS            -1           // Perform action on all active processors
#define ALL_PROCESSORS      ALL_CPUS     // Perform action on all active processors
#define THIS_CPU            -2           // Perform action on this (current) processor
#define THIS_PROCESSOR      THIS_CPU     // Perform action on this (current) processor

//
// Event Id used to indicate an invalid event
// ------------------------------------------
//
#define EVENT_INVALID_ID             0
#define EVENT_INVALID_INFO_INDEX    -1
#define CTR_INVALID_ID              -1

//
// For TraceEnable()/TraceDisable()
// --------------------------------
//
#define ALL_MAJORS           -1           // Enable/Disable all major codes

//
// For APIs that require a PID/TID
// -------------------------------
//
enum pid_type {
	CURRENT_PID		= -1,
	CURRENT_TID		= -2,
	ALL_PIDS		= -3,
	ALL_TIDS		= -4,
	PIDS_GREATER	= -5,
	PID_RANGE		= -6,
	SINGLE_PID		= -7,
	PID_LIST		= -8,
};

//
// For APIs that require an ON/OFF mode
// ------------------------------------
//
#define MODE_OFF             0
#define MODE_ON              1
#define MODE_FORCE_OFF       0x80000000
#define MODE_FORCE_ON        0x40000000
#define MODE_DEFAULT         0x20000000

//
// Max number of CPUs supported by PerfUtil/PERFDD
// -----------------------------------------------
//
// MAX_CPUS:
// - Largest number of CPUs (as seen by the OS) supported
//
// MAX_CPUS_LOGICAL:
// - Largest number of logical processors (threads) per core supported
// - Only has meaning if HyperThreading/SMT is enabled
//
// MAX_CPUS_CORE:
// - Largest number of cores (per socket) supported
//
#ifndef MAX_CPUS
#if defined(_WINDOWS)
   #define MAX_CPUS             64
   #define MAX_CPUS_32BIT       32
#elif defined(_LINUX)
   #ifndef STAP_ITRACE // DEFS
   #define MAX_CPUS             128
   #endif // STAP_ITRACE DEFS
#elif defined(_AIX)
   #define MAX_CPUS             64
#else
   #error ***** PERFUTIL.H: MAX_CPUS is undefined *****
#endif
#endif // MAX_CPUS
#define MAX_CPUS_LOGICAL         4
#define MAX_CPUS_CORE           32

//
// MSR value returned when invalid MSR is read
// -------------------------------------------
//
#if defined(_WINDOWS) || defined(_LINUX)
   #define INVALID_MSR_VALUE    CONST64(0xBAD0BAD1BAD2BAD3)
#elif defined(_AIX)
   #define INVALID_MSR_VALUE    0xBADC0FFE
#else
   #define INVALID_MSR_VALUE    0xBADC0FFE
#endif

//
// Default number of timer ticks to skip when doing ITrace
// -------------------------------------------------------
//
#define ITRACE_DEFAULT_SKIP_COUNT   50

//
// Largest number of PIDs allowed in list to ITrace_On_PIDList()
// -------------------------------------------------------------
//
#define ITRACE_MAX_PID_LIST_COUNT   50



//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  AI_COUNTERS used with AiAlloc/FreeCounters() and AiGetCounters()         //
//  ----------------------------------------------------------------         //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
struct _ai_counters
{
   UINT64  CurTime;         // Current TSC (cycles)
   UINT64  IdleTime;        // Current idle time counter
   UINT64  BusyTime;        // Current busy time counter
   UINT64  IntrTime;        // Current interrupt time counter
   UINT32  DispCnt;         // Current dispatch counter
   UINT32  IntrCnt;         // Current interrupt counter
};


//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  CPUUTIL_DATA used with CpuUtilGet/CpuUtilGetAndTrace()                   //
//  ------------------------------------------------------                   //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
struct _cpuutil_data {
   UINT64  elapsed_cycles;                  // Elapsed cycles from CpuUtilStart() to CpuUtilGet()
   double  cpu_busy[MAX_CPUS];              // Per-processor percent busy
   double  cpu_idle[MAX_CPUS];              // Per-processor percent idle
   double  cpu_intr[MAX_CPUS];              // Per-processor percent interrupt
   double  cpu_actual_perf[MAX_CPUS];       // Per-processor actual processor performance over interval
                                            // - Multiply by cpu_busy[] to get scaled percent busy
   double  sys_busy;                        // System-wide percent busy
   double  sys_idle;                        // System-wide percent idle
   double  sys_intr;                        // System-wide percent interrupt
   double  sys_actual_perf;                 // System-wide average actual processor performance over interval
                                            // - Multiply by sys_busy to get scaled percent busy
   UINT32  cpu_disp_cnt[MAX_CPUS];          // Per-processor dispatch count
   UINT32  cpu_intr_cnt[MAX_CPUS];          // Per-processor interrupt count
   int     num_cpus;                        // Number of processors for which data is available
   UINT32  flags;                           // Flags. See CPUUTIL_DATA_FLAG_* below.
};

//
// Values for CPUUTIL_DATA.flags
//
#define CPUUTIL_DATA_FLAG_SLOWTSC    0x00000001   // TSC slowed/stopped counting during interval


//
// Trace Type flags to be used with CpuUtilInit()
// ----------------------------------------------
//
#define CPUUTIL_TRACE_NONE           0x00000000   // No tracing
#define CPUUTIL_TRACE_SUMMARY        0x00000001   // Trace CPUUTIL_DATA structure
#define CPUUTIL_TRACE_DETAILED       0x00000002   // Trace CPUUTIL_RAW_DATA structure
#define CPUUTIL_TRACE_EVERYTHING     (CPUUTIL_TRACE_SUMMARY | CPUUTIL_TRACE_DETAILED)



//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  CPI_DATA used with CpiGet/CpiGetAndTrace()                               //
//  ------------------------------------------                               //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
#if defined(_WINDOWS)

struct _cpi_data {
   int       num_cpus;                      // Number of processors for which data is available
   UINT32    tid;                           // TID of thread on which CpiInit() was done
   UINT64    elapsed_cycles;                // Elapsed cycles from CpiStart() to CpiGet()
   // Data returned if CPI_TYPE_NOMINAL
   double    nom_cpi[MAX_CPUS];             // Per-processor nominal CPI
   double    nom_sys_cpi;                   // System-wide nominal CPI
   // Data returned if CPI_TYPE_NONHALTED
   double    nh_cpi[MAX_CPUS];              // Per-processor non-halted CPI
   double    nh_sys_cpi;                    // System-wide non-halted CPI
   // Data returned if CPI_TYPE_CPUUTIL
   double    cpu_busy[MAX_CPUS];            // Per-processor percent busy
   double    cpu_app[MAX_CPUS];             // Per-processor actual processor performance
   double    sys_busy;                      // System-wide percent busy
   double    sys_app;                       // System-wide actual processor performance
   // Data returned if CPI_TYPE_ONTHREAD
   double    thread_cpi;                    // Current thread's non-halted (by definition) CPI
};

//
// Type flags to be used with CpiInit()
// ------------------------------------
//
// - CPI_TYPE_NOMINAL is always in effect and does not need to be specified.
//
#define CPI_TYPE_NOMINAL             0x00000001   // Measure Nominal CPI
#define CPI_TYPE_NONHALTED           0x00000002   // Measure Non-halted CPI
#define CPI_TYPE_ONTHREAD            0x00000004   // Measure On-thread CPI (requires PTT)
#define CPI_TYPE_CPUUTIL             0x00000008   // Measure CPU Utilization (requires CpuUtil)
#define CPI_TYPE_TRACE_ON_GET        0x00000010   // Write data to trace buffer after CpiGet()
#define CPI_TYPE_TRACE_ONLY_ON_GET   0x00000020   // JAVA-ONLY: No data returned on Cpi.get() - only written to trace buffer
#define CPI_TYPE_TRACE_SUMMARY       0x00000040   // Trace summary data only (CPI_DATA structure)
#define CPI_TYPE_TRACE_DETAILED      0x00000080   // Trace detailed data only (Cpi class public data members)
#define CPI_TYPE_ALL                 0x000000FF

#define CPI_TYPE_TRACE_EVERYTHING    (CPI_TYPE_TRACE_SUMMARY | CPI_TYPE_TRACE_DETAILED)

#elif defined(_LINUX)

struct _cpi_data {
   int       num_cpus;                      // Number of processors for which data is available
   UINT32    tid;                           // TID of thread on which CpiInit() was done
   UINT64    elapsed_cycles;                // Elapsed cycles from CpiStart() to CpiGet()
   // Data returned if CPI_TYPE_RAW - always
   double    raw_cpi[MAX_CPUS];             // Per-processor raw (not scaled) CPI
   double    raw_sys_cpi;                   // System-wide raw (not scaled) CPI
   // Data returned if CPI_TYPE_SCALED
   double    scaled_cpi[MAX_CPUS];          // Per-processor scaled CPI
   double    scaled_sys_cpi;                // System-wide scaled CPI
   double    cpu_busy[MAX_CPUS];            // Per-processor percent busy
   double    sys_busy;                      // System-wide percent busy
   // Data returned if CPI_TYPE_ONTHREAD
   double    thread_cpi;                    // Current thread's scaled (by definition) CPI
};

//
// Type flags to be used with CpiInit()
// ------------------------------------
//
// - CPI_TYPE_RAW is always in effect and does not need to be specified.
//
#define CPI_TYPE_RAW                 0x00000001   // Measure Raw CPI (always enabled)
#define CPI_TYPE_SCALED              0x00000002   // Measure Scaled CPI (requires AI)
#define CPI_TYPE_ONTHREAD            0x00000004   // Measure On-thread CPI (requires PTT)
#define CPI_TYPE_TRACE_ON_GET        0x00000008   // Write data to trace buffer after CpiGet()
#define CPI_TYPE_TRACE_ONLY_ON_GET   0x00000010   // JAVA-ONLY: No data returned on Cpi.get() - only written to trace buffer
#define CPI_TYPE_TRACE_SUMMARY       0x00000020   // Trace summary data only (CPI_DATA structure)
#define CPI_TYPE_TRACE_DETAILED      0x00000040   // Trace detailed data only (Cpi class public data members)
#define CPI_TYPE_ALL                 0x0000007F

#define CPI_TYPE_TRACE_EVERYTHING    (CPI_TYPE_TRACE_SUMMARY | CPI_TYPE_TRACE_DETAILED)

#endif

//
// Endiannes
// ---------
//
#define CPU_LITTLE_ENDIAN           1         // Little endian byte ordering
#define CPU_BIG_ENDIAN              2         // Big endian byte ordering

//
// Width
// -----
//
#define CPU_32BITS                  32        // 32-bit processor
#define CPU_64BITS                  64        // 64-bit processor

//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  Performance Counter numbers                                              //
//  ---------------------------                                              //
//                                                                           //
//  They are not all valid on all platforms!                                 //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
#define PERF_CTR0            0
#define PERF_CTR1            1
#define PERF_CTR2            2
#define PERF_CTR3            3
#define PERF_CTR4            4
#define PERF_CTR5            5
#define PERF_CTR6            6
#define PERF_CTR7            7
#define PERF_CTR8            8
#define PERF_CTR9            9
#define PERF_CTR10           10
#define PERF_CTR11           11
#define PERF_CTR12           12
#define PERF_CTR13           13
#define PERF_CTR14           14
#define PERF_CTR15           15
#define PERF_CTR16           16
#define PERF_CTR17           17



#if defined(_WINDOWS) || defined(_LINUX)
//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  Per-Thread Time                                                          //
//  ---------------                                                          //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
//
// Maximum number of accounting metrics supported by platform.
// -----------------------------------------------------------
//
// * If not 1 it must be an even number.
//
#define PTT_MAX_METRICS         8


//
// PTT metric values
// -----------------
//
#define PTT_METRIC_CTR0            PERF_CTR0
#define PTT_METRIC_CTR1            PERF_CTR1
#define PTT_METRIC_CTR2            PERF_CTR2
#define PTT_METRIC_CTR3            PERF_CTR3
#define PTT_METRIC_CTR4            PERF_CTR4
#define PTT_METRIC_CTR5            PERF_CTR5
#define PTT_METRIC_CTR6            PERF_CTR6
#define PTT_METRIC_CTR7            PERF_CTR7
#define PTT_METRIC_CTR8            PERF_CTR8
#define PTT_METRIC_CTR9            PERF_CTR9
#define PTT_METRIC_CTR10           PERF_CTR10
#define PTT_METRIC_CTR11           PERF_CTR11
#define PTT_METRIC_CTR12           PERF_CTR12
#define PTT_METRIC_CTR13           PERF_CTR13
#define PTT_METRIC_CTR14           PERF_CTR14
#define PTT_METRIC_CTR15           PERF_CTR15
#define PTT_METRIC_CTR16           PERF_CTR16
#define PTT_METRIC_CTR17           PERF_CTR17

#define PTT_METRIC_INSTR_RETIRED   254
#define PTT_METRIC_CYCLES          255

#endif


//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  SWTRACE/User Hooks                                                       //
//  ------------------                                                       //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
//
// Hook types for WriteAddressHookEx()
// -----------------------------------
//
#define TRACE_ENTRY          0              // indicates entry hook
#define TRACE_EXIT           1              // indicates exit hook

//
// Values returned by TraceQueryMajor()
// ------------------------------------
//
#define MAJOR_NOT_ENABLED    0              // Major code not enabled
#define MAJOR_ENABLED_NOMM   1              // Major code enabled, no multi-metric (ie. cycles only)
#define MAJOR_ENABLED_MM    -1              // Major code enabled, multi-metric

//
// Major code range
// ----------------
//
#define SMALLEST_HOOK_MAJOR  0
#define LARGEST_HOOK_MAJOR   255
#define NUM_MAJORS           256

//
// "VData" (aka. string or variable-length data) to be written to trace buffer
// ---------------------------------------------------------------------------
//
struct _VDATA {
   int     len;                             // Length of buffer (in bytes)
#if defined(_64BIT)
   int     ___;                             // Alignment. Not used.
#endif
   char    * data;                          // Actual buffer
};

//
// Max number of trace metrics
// ---------------------------
//
#define TRACE_MAX_METRICS   8

//
// Trace metric values
// -------------------
//
#define TRACE_METRIC_CTR0            PERF_CTR0
#define TRACE_METRIC_CTR1            PERF_CTR1
#define TRACE_METRIC_CTR2            PERF_CTR2
#define TRACE_METRIC_CTR3            PERF_CTR3
#define TRACE_METRIC_CTR4            PERF_CTR4
#define TRACE_METRIC_CTR5            PERF_CTR5
#define TRACE_METRIC_CTR6            PERF_CTR6
#define TRACE_METRIC_CTR7            PERF_CTR7
#define TRACE_METRIC_CTR8            PERF_CTR8
#define TRACE_METRIC_CTR9            PERF_CTR9
#define TRACE_METRIC_CTR10           PERF_CTR10
#define TRACE_METRIC_CTR11           PERF_CTR11
#define TRACE_METRIC_CTR12           PERF_CTR12
#define TRACE_METRIC_CTR13           PERF_CTR13
#define TRACE_METRIC_CTR14           PERF_CTR14
#define TRACE_METRIC_CTR15           PERF_CTR15
#define TRACE_METRIC_CTR16           PERF_CTR16
#define TRACE_METRIC_CTR17           PERF_CTR17

#define TRACE_METRIC_INSTR_RETIRED   PTT_METRIC_INSTR_RETIRED
#define TRACE_METRIC_CYCLES          PTT_METRIC_CYCLES

//
// Constants/Structures for reading NRM2 files
// -------------------------------------------
//
#define HOOK_MAX_INTS            128
#define HOOK_MAX_STRS            16
#define HOOK_MAX_STR_LEN         2048

//
// Trace hook
// ----------
//
struct _trace_hook {
   INT64 file_offset;                             // NRM2 file offset for this hook
   UCHAR  * raw;                                  // Treat as read-only. hook_len bytes.
   USHORT hook_type;                              // See HOOK_TYPE_* below
   USHORT hook_len;                               // In bytes
   USHORT cpu_no;                                 // CPU on which the hook was written
   USHORT major;                                  // Major code (0x01 - 0xFF)
   UINT32 minor;                                  // Minor code
   UINT32 tlo;                                    // Timestamp low 32 bits
   UINT32 thi;                                    // Timestamp high 32 bits
   USHORT ints;                                   // Number of ints in ival array
   USHORT strings;                                // Number of strings in sval array
   UINT32 ival[HOOK_MAX_INTS];                    // Array of ints (UINT32s)
   UINT32 slen[HOOK_MAX_STRS];                    // Array of string lengths
   UCHAR  sval[HOOK_MAX_STRS][HOOK_MAX_STR_LEN];  // Array of stings
};


//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  Kernel Information                                                       //
//  ------------------                                                       //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
#if defined(_WINDOWS) || defined(_LINUX)
struct _pi_kernel_info {
   UINT64 addr;                             // Start address
   UINT32 length;                           // Length (in bytes)
   UINT32 ts;                               // Timestamp (always 0 on Linux)
   UINT32 cs;                               // Checksum (always 0 on Linux)
   UINT32 version;                          // Version ((LINUX_VERSION_CODE)
   UINT32 hz;                               // Timer tick rate (HZ)
   UINT32 flags;                            // Variuos flags
};

//
// Valid flag values
//
#define KFLAGS_FRAME_POINTERS           0x00000001     // Kernel compiled with frame pointers (CONFIG_FRAME_POINTER)
#define KFLAGS_VDSO_SEGMENT             0x00000002     // VDSO segment mapped to each process
#define KFLAGS_64BIT                    0x00000004     // 64-bit kernel
#define KFLAGS_VARIABLE_TIMER_TICK      0x00000008     // Timer ticks only when needed (CONFIG_NO_HZ)
#define KFLAGS_LOCAL_APIC               0x00000010     // Local APIC available (CONFIG_X86_LOCAL_APIC)

#define PI_VDSO_SEGMENT_NAME            "[vdso]"
#endif


//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  Callstack Sampling                                                       //
//  ------------------                                                       //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
// Sampling Notification fake event for time
// -----------------------------------------
//
// SAMPLING_NOTIFY_EVENT_TIME
// - Sampling driven by time (as opposed to a performance counter event)
//
#define SAMPLING_NOTIFY_EVENT_TIME        PTT_METRIC_CYCLES

#define MAX_STACK_FRAMES 32
struct _scs_callstack_data {
   uint32_t pid;
   uint32_t tid;
   uint64_t ip;
   uint32_t num_stack_frames;
   uint64_t stack_frames[MAX_STACK_FRAMES];
};


//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  PU Worker Thread                                                         //
//  ----------------                                                         //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
struct _pu_worker_thread_args {
   void **thread_handle;
   void **thread_start_func;
   void *extraArgs;
};


//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  Application Notification                                                 //
//  ------------------------                                                 //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
// Notification Reason flags
// --------------------------
//
// First argument to user callback notification function.
//
//
// PERF_NOTIFY_REASON_SYSTEM_SLEEP
// - The system is about to go to sleep (standby/sleep/hibernate).
//   Application can save away any data/state it deems necessary.
//   Whatever it does it must do very quickly: the system will not hold up
//   the transition to sleep.
//
// PERF_NOTIFY_REASON_SYSTEM_WAKEUP
// - The system is about to resume from sleep (standby/sleep/hibernate).
//   Application needs to handle the resumption gracefully. If it is not
//   able to survive the transition, it must clean up and terminate.
//   This is the chance to write out any data accumulated so far, etc.
//
// PERF_NOTIFY_REASON_POWER_DC
// - The system is running on DC power (battery).
//
// PERF_NOTIFY_REASON_POWER_AC
// - The system is running on AC power (plugged in).
//
// PERF_NOTIFY_REASON_LID_OPEN
// - Laptop lid has been opened.
//
// PERF_NOTIFY_REASON_LID_CLOSE
// - Laptop lid has been closed.
//
// PERF_NOTIFY_REASON_PROCESSOR_POLICY_CHANGE
// - The processor power management policy (in the active power scheme)
//   has been modified.
//
// PERF_NOTIFY_REASON_SYSTEM_POLICY_CHANGE
// - The system power management policy (in the active power scheme)
//   has been modified.
//
// PERF_NOTIFY_REASON_SLOW_TSC
// - Processor power management has powered down part(s) of the processor
//   and the TSC is no longer incrementinc continuously.
//
#define PERF_NOTIFY_REASON_SYSTEM_SLEEP            1  // System is going to sleep (S0 -> Sx)
#define PERF_NOTIFY_REASON_SYSTEM_WAKEUP           2  // System is back from sleep (Sx -> S0)
#define PERF_NOTIFY_REASON_POWER_DC                3  // System is running on DC power (battery)
#define PERF_NOTIFY_REASON_POWER_AC                4  // System is running on AC power (plugged in)
#define PERF_NOTIFY_REASON_LID_OPEN                5  // Laptop lid has been opened
#define PERF_NOTIFY_REASON_LID_CLOSE               6  // Laptop lid has been closed
#define PERF_NOTIFY_REASON_PROCESSOR_POLICY_CHANGE 7  // Processor PM policy changed
#define PERF_NOTIFY_REASON_SYSTEM_POLICY_CHANGE    8  // System PM policy changed
#define PERF_NOTIFY_REASON_SLOW_TSC                9  // TSC has slowed down/stopped counting on some processor(s)

//
// Flags returned by GetPmDisabledMask()
// -------------------------------------
//
// Can be ORed together.
//
#define PM_DISABLED_NONE                0x00000000
#define PM_DISABLED_ITRACE              0x00000001
#define PM_DISABLED_TPROF               0x00000002
#define PM_DISABLED_SCS                 0x00000004
#define PM_DISABLED_PTT                 0x00000008
#define PM_DISABLED_AI                  0x00000010
#define PM_DISABLED_INTR_HOOKS          0x00000020
#define PM_DISABLED_DISP_HOOK           0x00000040
#define PM_DISABLED_SYSCALL_HOOKS       0x00000080
#define PM_DISABLED_PERFCTRS            0x00000100


//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  Performance Counter Raw Access                                           //
//  ------------------------------                                           //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
//
// Number of performance counters by processor type
// ------------------------------------------------
//
#define NUM_P6_CTRS       2                 // Number of counters in P6/Core processors
#define MAX_P6_CTR_NUM    1                 // Highest counter number

#define NUM_P4_CTRS      18                 // Number of counters in Pentium 4
#define MAX_P4_CTR_NUM   17                 // Highest counter number

#define NUM_NEHALEM_CTRS  4                 // Number of counters in Nehalem processors
#define MAX_NEHALEM_CTRS  3                 // Highest counter number

#define NUM_ATOM_CTRS     2                 // Number of counters in Atom processors
#define MAX_ATOM_CTRS     1                 // Highest counter number

#define NUM_AMD_CTRS      4                 // Number of counters in AMD processors
#define MAX_AMD_CTR_NUM   3                 // Highest counter number

#define NUM_PPC_CTRS      8                 // Number of counters on PPC processors
#define MAX_PPC_CTR_NUM   7                 // Highest counter number
#define NUM_MMCRS         5                 // Number of MMCRs

#define MAX_PERFCTRS      NUM_P4_CTRS       // Max number of ctrs in any CPU we support

//
// Performance Counter Data
// ------------------------
//
// Data returned by ReadAllPerfCounters().
//
// * Counter array:
//   - One row per processor
//   - One column per counter
//   - Row/column index indicates processor or counter number
// * Cycles array:
//   - One element per processor
//   - Index indicates processor to which the cycle counter belongs
//
// P4: Counter overflow indicator is high order bit (bit 63) of value.
//     If using the 64-bit value you must reset bit 63 before using
//     the value in any computation.
//
struct _perfctr_data {
   int        NumCpus;                         // Number of processors for which counters returned
   int        NumCtrs;                         // Number of counters returned per processor
   UINT64     Counter[MAX_CPUS][MAX_PERFCTRS]; // Perf counter values per processor
   UINT64     Cycles[MAX_CPUS];                // Time Stamp Counter per processor
};

//
// Max number of simultaneous events supported.
// -------------------------------------------
//
// The actual number of events supported must be obtained by calling
// the GetMaxNumberOfPerfCounterEvents() API.
//
#define CTR_MAX_P6_EVENTS       2
#define CTR_MAX_CORE_EVENTS     2
#define CTR_MAX_NEHALEM_EVENTS  4
#define CTR_MAX_P4_EVENTS       8
#define CTR_MAX_AMD_EVENTS      4
#define CTR_MAX_EVENTS          8           // Largest of above

//
// Performance Counter Event Information. See: GetPerfCounterEventInfo()
// ---------------------------------------------------------------------
//
struct _perfctr_event_info {
   char * name;                             // Event name
   char * description;                      // Event description (about one sentence)
   int  event_id;                           // Event Id
};

#if defined(_WINDOWS) || defined(_LINUX)
#pragma pack()
#else
#pragma pack(pop)
#endif

#endif  // _PERFUTIL_H_
