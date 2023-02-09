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

#include "utils/pi_time.h"

#include "perfutil_core.hpp"
#include "pu_trace.hpp"
#include "pu_worker.hpp"

//
// Globals (instance data)
// ***********************
//

bool         dd_data_mapped = 0;       // 1=DD data mapped, 0=DD data not mapped
MAPPED_DATA * pmd            = NULL;    // Data mapped by device driver
MAPPED_DATA   md;                       // Local mapped data in case we can't map

char * physbuf_va = NULL;               // Virtual of physically contiguous buffer
UINT64 physbuf_pa = 0;                  // Physical of physicall contiguous buffer

UCHAR         * pmecTable = NULL;       // Pointer to MEC table

int           MaxCpuNumber;             // Highest valid CPU number (0 origin)

float  CPUSpeed   = 0.0;	        // both vars are set in get_CPUSpeed()
INT64  llCPUSpeed = 0;

ULONG         PerfddFeatures = 0;       // Features supported by PERFDD
int           Ring3RDPMC     = 0;       // No ring3 access to PerfCtrs

#define INVALID_HANDLE_VALUE   0

int hDriver = INVALID_HANDLE_VALUE;     // driver file decriptor

UINT64 ctr_mask = 0;                    // Counter mask

int           pu_debug = 0;
int           pu_debug_ptt = 0;

//
// CheckForEnvironmenVariables()
// *****************************
//
void CheckForEnvironmenVariables(void)
{
   char * p;

   p = getenv("PERFUTIL_DEBUG");
   if (p)
      pu_debug = 1;                         // Display debugging messages

   p = getenv("PERFUTIL_DEBUG_PTT");
   if (p)
      pu_debug_ptt = 1;                     // Display debugging messages in PTT code

   return;
}

//
// printf_se()
// ***********
//
// Write messages to stderr and flush the stream.
//
void __cdecl printf_se(const char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stderr, format, argptr);
   fflush(stderr);
   va_end(argptr);
   return;
}


//
// printf_so()
// ***********
//
// Write messages to stdout and flush the stream.
//
void __cdecl printf_so(const char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stdout, format, argptr);
   fflush(stdout);
   va_end(argptr);
   return;
}


//
// StrToUlong()
// ************
//
// Convert the input string to a number.
// Can handle decimal numbers (with/without leading zeros) and
// hexadecimal numbers (prefixed with '0x'). '0x' prefix can't have
// leading zeros.
//
// Returns:
// - TRUE/1:  no errors. *result contains number
// - FALSE/0: errors. *result contains 0
//
int __cdecl StrToUlong(const char * s, UINT32 * result)
{
   int  rc = 0, i, hexVal = 0, checkForZero = 0;
   ULONG res;

   if (result == NULL)
      return(0);

   if (s == NULL) {
      *result = 0;
      return(0);
   }

   switch (strlen(s)) {
      case 1:                           // One character
         if (isdigit(s[0]))
            res = atoi(s);              // Decimal digit. Get res
         else {
            *result = 0;                // Not valid decimal. Error.
            return(0);
         }
         break;

      case 2:                           // Two characters
         checkForZero = 1;
         res = atoi(s);                 // Assume it's decimal
         break;

      default:                          // Three or more characters
         checkForZero = 1;
         if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            res = strtoul(s, 0, 0);     // 0x prefix. Assume hex
            hexVal = 1;
         }
         else {
            res = atoi(s);              // No 0x prefix. Assume decimal
         }
         break;
   }

   *result = res;                       // Store value

   if (checkForZero && res == 0) {      // Make sure result is really zeros
                                        // and that strtol/atoi just failed to
                                        // convert.

      for (i = (hexVal) ? 2 : 0; i < strlen(s); i++) {
         if (s[i] != '0')
            return(0);                  // Supposed to contain only 0's.  Error.
      }
   }
   else {                               // Make sure valid digits (hex/dec)
      for (i = (hexVal) ? 2 : 0; i < strlen(s); i++) {
         rc = (hexVal) ? isxdigit(s[i]) : isdigit(s[i]);
         if (!rc)
            return(0);                  // Not the right kind of digits. Error.
      }
   }

   return(1);                           // All digits of the right type
}


//
// RcToString()
// ************
//
const char * __cdecl RcToString(const int rc)
{
	// TODO: update this list
   switch (rc) {
      case PU_FAILURE:                        return("PU_FAILURE");
      case PU_SUCCESS:                        return("PU_SUCCESS");
      case PU_ERROR_INVALID_FUNCTION:         return("PU_ERROR_INVALID_FUNCTION");
      case PU_ERROR_ACCESS_DENIED:            return("PU_ERROR_ACCESS_DENIED");
      case PU_ERR_NOT_INITIALIZED:            return("PU_ERR_NOT_INITIALIZED");
      case PU_ERROR_NOT_ENOUGH_MEMORY:        return("PU_ERROR_NOT_ENOUGH_MEMORY");
      case PU_ERROR_INVALID_PARAMETER:        return("PU_ERROR_INVALID_PARAMETER");
      case PU_ERROR_INSUFFICIENT_BUFFER:      return("PU_ERROR_INSUFFICIENT_BUFFER");
      case PU_ERROR_INTERNAL_ERROR:           return("PU_ERROR_INTERNAL_ERROR");
      case PU_ERROR_INCORRECT_STATE:          return("PU_ERROR_INCORRECT_STATE");
      case PU_ERROR_NO_BUFFER:                return("PU_ERROR_NO_BUFFER");
      case PU_ERROR_THREAD_NOT_FOUND:         return("PU_ERROR_THREAD_NOT_FOUND");
      case PU_ERROR_NO_TARGET_CPU:            return("PU_ERROR_NO_TARGET_CPU");
      case PU_ERROR_BUFFER_FULL:              return("PU_ERROR_BUFFER_FULL");
      case PU_ERROR_WRONG_PROCESSOR_TYPE:     return("PU_ERROR_WRONG_PROCESSOR_TYPE");
      case PU_ERROR_NOT_ALLOWED_ON_SMP:       return("PU_ERROR_NOT_ALLOWED_ON_SMP");
      case PU_ERROR_HOOK_TOO_LONG:            return("PU_ERROR_HOOK_TOO_LONG");
      case PU_ERROR_DRIVER_NOT_LOADED:        return("PU_ERROR_DRIVER_NOT_LOADED");
      case PU_ERROR_NOT_SUPPORTED:            return("PU_ERROR_NOT_SUPPORTED");
      case PU_ERROR_FACILITY_BUSY:            return("PU_ERROR_FACILITY_BUSY");
      case PU_ERROR_FACILITY_NOT_OWNED:       return("PU_ERROR_FACILITY_NOT_OWNED");
      case PU_ERROR_EVENT_NOT_SUPPORTED:      return("PU_ERROR_EVENT_NOT_SUPPORTED");
      case PU_ERROR_HTT_ENABLED:              return("PU_ERROR_HTT_ENABLED");
      case PU_ERROR_JNI:                      return("PU_ERROR_JNI");
      case PU_ERROR_INVALID_CPI_DATA:         return("PU_ERROR_INVALID_CPI_DATA");
      case PU_ERROR_BUFFER_EXCEPTION:         return("PU_ERROR_BUFFER_EXCEPTION");
      case PU_ERROR_INSTALLING_REQ_HOOKS:     return("PU_ERROR_INSTALLING_REQ_HOOKS");
      case PU_ERROR_INVALID_METRIC:           return("PU_ERROR_INVALID_METRIC");
      case PU_ERROR_MIXED_METRICS:            return("PU_ERROR_MIXED_METRICS");
      case PU_ERROR_STARTING_METRIC:          return("PU_ERROR_STARTING_METRIC");
      case PU_ERROR_INVALID_PRIV_LEVEL:       return("PU_ERROR_INVALID_PRIV_LEVEL");
      case PU_ERROR_DUPLICATE_METRICS:        return("PU_ERROR_DUPLICATE_METRICS");
      case PU_ERROR_TOO_MANY_METRICS:         return("PU_ERROR_TOO_MANY_METRICS");
      case PU_ERROR_EVENT_MAX_EXCEEDED:       return("PU_ERROR_EVENT_MAX_EXCEEDED");
      case PU_ERROR_EVENT_ALL_CTRS_IN_USE:    return("PU_ERROR_EVENT_ALL_CTRS_IN_USE");
      case PU_ERROR_NOT_APIC_SYSTEM:          return("PU_ERROR_NOT_APIC_SYSTEM");
      case PU_ERROR_STARTING_EVENT:           return("PU_ERROR_STARTING_EVENT");
      case PU_ERROR_EVENT_NOT_COUNTING:       return("PU_ERROR_EVENT_NOT_COUNTING");
      case PU_ERROR_INVALID_EVENT:            return("PU_ERROR_INVALID_EVENT");
      case PU_ERROR_EVENT_ALREADY_COUNTING:   return("PU_ERROR_EVENT_ALREADY_COUNTING");
      case PU_ERROR_DISPATCHER_NOT_HOOKED:    return("PU_ERROR_DISPATCHER_NOT_HOOKED");
      case PU_ERROR_NOT_VALID_HTT_ENABLED:    return("PU_ERROR_NOT_VALID_HTT_ENABLED");
      case PU_ERROR_NOT_VALID_IN_VM:          return("PU_ERROR_NOT_VALID_IN_VM");
      case PU_ERROR_PTT_USER_TABLE_FULL:      return("PU_ERROR_PTT_USER_TABLE_FULL");
      case PU_ERROR_SAMPLING_NOTIFY_MODE_ENABLED: return("PU_ERROR_SAMPLING_NOTIFY_MODE_ENABLED");
      case PU_ERROR_DIDNT_GET_32BIT_MTE:      return("PU_ERROR_DIDNT_GET_32BIT_MTE");
      case PU_ERROR_EVENT_ALL_ESCR_IN_USE:    return("PU_ERROR_EVENT_ALL_ESCR_IN_USE");
      case PU_ERROR_NO_EVENT_DEFINITIONS:     return("PU_ERROR_NO_EVENT_DEFINITIONS");
      case PU_ERROR_BUFFER_ALREADY_ALLOCATED: return("PU_ERROR_BUFFER_ALREADY_ALLOCATED");
      case PU_ERROR_INSUFFICIENT_RESOURCES:   return("PU_ERROR_INSUFFICIENT_RESOURCES");
      case PU_ERROR_CANT_MAP_BUFFER:          return("PU_ERROR_CANT_MAP_BUFFER");
      case PU_ERROR_BUFFER_NOT_MAPPED:        return("PU_ERROR_BUFFER_NOT_MAPPED");
      case PU_ERROR_BUFFER_ADDR_MISMATCH:     return("PU_ERROR_BUFFER_ADDR_MISMATCH");
      case PU_ERROR_PEBS_ENABLE_IN_USE:       return("PU_ERROR_PEBS_ENABLE_IN_USE");
      case PU_ERROR_PEBS_MATRIX_VERT_IN_USE:  return("PU_ERROR_PEBS_MATRIX_VERT_IN_USE");
      case PU_ERROR_INVALID_ARRAY_SIZE:       return("PU_ERROR_INVALID_ARRAY_SIZE");
      case PU_ERROR_INCOMPATIBLE_OPTIONS:     return("PU_ERROR_INCOMPATIBLE_OPTIONS");
      case 0xC0000005:                        return("STATUS_ACCESS_VIOLATION");
      case 0xC000001D:                        return("STATUS_ILLEGAL_INSTRUCTION");
      case 0xC0000094:                        return("STATUS_INTEGER_DIVIDE_BY_ZERO");
      case 0xC0000096:                        return("STATUS_PRIVILEGED_INSTRUCTION");
      case 0xC0000145:                        return("STATUS_APP_INIT_FAILURE");
      default:                                return("Not a PerfUtil return code");
   }
}



//
// pi_init_module()
// ****************
//
int init_mapped_data(void)
{
	int c, i;

   //
   // Device Driver Information
   //

	// Set number of active CPUs
	pmd->ActiveCPUs = NUM_CPUS_ACTIV;
	if (pmd->ActiveCPUs > MAX_CPUS) {
		PI_PRINTK_ERR("More cpus in system than can be handled. Max = %d\n", MAX_CPUS);
		return (PU_FAILURE);
	}
	for (c = 0; c < pmd->ActiveCPUs; c++) {
		pmd->ActiveCPUSet = (pmd->ActiveCPUSet << 1) || 1;
	}

	// Set physicall processor count values
	pmd->InstalledCPUs = NUM_CPUS_INSTL;
	for (i = 0; i < pmd->InstalledCPUs; ++i) {
		pmd->InstalledCPUSet = (pmd->InstalledCPUSet << 1) || 1;
	}

	pmd->Version        = TRACE_VERSION;
	pmd->TraceIntervalId = 0; // TODO: set as the process id or time.
	pmd->PageSize 		= PAGE_SIZE;
	pmd->TraceMode      = COLLECTION_MODE_CONTINUOS;


	// Set some init values
	// driver_info partly initialized in pi_inspector.c

	// Check for oprofile and system hooks
	pmd->TprofSupported = 1;
	pmd->CallstackSamplingSupported = pmd->TprofSupported;


	pmd->PttMaxThreadId = NR_THREADS - 1;

	for (i = 0; i < MAX_APIC_ID; i++) {
		pmd->ApicIdToCpuNumber[i] = (UINT8)-1;
	}

	pmd->CpuTimerTick       = PI_HZ;

	set_tprof_defaults();

	pmd->PttMaxMetrics  = PTT_MAX_METRICS;

	pmd->CtrValueArraySize = pmd->ActiveCPUs * sizeof(uint64_t);
	pmd->AiCountersSize    = pmd->ActiveCPUs * sizeof(AI_COUNTERS);

	PDEBUG("...Allocation done.\n");

	pmd->CtrValidBitsMask = 0;
	// TODO: idk was pmd->CtrValidBitsMask = CtrValidBitsMask;

	pmd->is_p4 = 1; // FIXME: should do something about the cpuid stuff

	pmd->TprofScope = 0;
	pmd->TprofPidMain = -1;

	PI_PRINTK("Driver version = %08x\n", TRACE_VERSION);

	return (0);
}

//
// lib initialization/termination
// *******************************
//
static void __attribute__ ((constructor)) my_init(void) {
	int rc = 0;
	void * event_defs;
	int event_defs_size;
	int i;

	//
	// Check environment to see if they want to do things
	// we don't normally do.
	//
	CheckForEnvironmenVariables();

	PDEBUG("Allocating shared area...\n");
	// vmalloc is already page aligned, so we can use the address directly
	pmd = (MAPPED_DATA *) malloc(sizeof(MAPPED_DATA) + PAGE_SIZE);

	if (pmd == 0) {
		PI_PRINTK_ERR("Unable to allocate pmd.\n");
		return;
	}

	memset(pmd, 0, sizeof(MAPPED_DATA));
	rc = init_mapped_data();
	if (rc != 0) {
		perror("ERROR setting MAPPED_DATA");
		exit(errno);
	}

	rc = _init_activeCpu_list();
	if (rc != 0) {
		exit(errno);
	}

	//
	// Set globals
	//
	pmecTable = &pmd->MecTable[0];  // Pointer to MEC table

	// Set the counter mask to the correct value

	if (IsProcessorAmd())
		ctr_mask = CTR_VALUE_AMD_MASK;
	else if (IsProcessorIntel())
		ctr_mask = CTR_VALUE_INTEL_MASK;
	else if (IsProcessorPPC64())
		ctr_mask = CTR_VALUE_PPC64_MASK;


	MaxCpuNumber = pmd->ActiveCPUs - 1;

	// set CpuSpeed variables

	get_CPUSpeed();

	return;
}


static void __attribute__ ((destructor)) my_fini(void)
{
   return;
}

//
// ProcessIsWow64()
// ****************
// Always return false on Linux
//
BOOL __cdecl ProcessIsWow64(HANDLE process)
{
   return(FALSE);
}

//
// GetMappedDataAddress()
// **********************
//
MAPPED_DATA * __cdecl GetMappedDataAddress(void)
{
   return (pmd);
}

//
// IsProcessorNumberValid()
// ************************
//
// ##### NOT EXPORTED #####
//
// Given a processor number return:
// * TRUE  if processor number is valid or ALL_CPUS or THIS_CPU
// * FALSE if processor number is invalid
//
int IsProcessorNumberValid(int cpunum)
{
   if (cpunum == ALL_CPUS || cpunum == THIS_CPU)
      return(1);

   if (cpunum < 0 || cpunum > PiGetActiveProcessorCount())
      return(0);
   else
      return(1);
}


//
// ReadCycles()
// ************
// This is an architecture specific routine.
//
UINT64 __cdecl ReadCycles()
{
   union rdval time;
#ifdef CONFIG_IA64
   int result = 0;
#endif
#if defined(CONFIG_X86) || defined(CONFIG_X86_64)
   __asm__ __volatile__("rdtsc":"=a"(time.sval.lval),
                        "=d"(time.sval.hval));
#endif

#if defined(CONFIG_S390) || defined(CONFIG_S390X)
   __asm__ __volatile__("la     1,%0\n stck    0(1)":"=m"(time.cval)
                        ::"cc", "1");
#endif

#if defined(PPC) || defined(CONFIG_PPC64)
   uint32_t temp1 = 1;

   time.sval.hval = 2;
   while (temp1 != time.sval.hval) {
      __asm__ __volatile__("mftbu %0":"=r"(temp1));
      __asm__ __volatile__("mftb  %0":"=r"(time.sval.lval));
      __asm__ __volatile__("mftbu %0":"=r"(time.sval.hval));
   }
#endif

#ifdef CONFIG_IA64
   __asm__ __volatile__("mov %0=ar.itc":"=r"(time.cval)::"memory");
#ifdef CONFIG_ITANIUM
   while (__builtin_expect((__s32) result == -1, 0))
      __asm__ __volatile__("mov %0=ar.itc":"=r"(time.cval)::"memory");
#endif
#endif
   return(time.cval);
}

//
// GetCycles()
//***************
//
void GetCycles(UINT64 * timestamp)
{
   *timestamp = ReadCycles();
}

//
// AiAllocCounters()
// *****************
//
AI_COUNTERS * __cdecl AiAllocCounters(int * size)
{
   if (size != NULL)
      *size = pmd->AiCountersSize;

   if (pmd->AiCountersSize == 0)
      return(NULL);
   else
      return((AI_COUNTERS *)calloc(1, pmd->AiCountersSize));
}

//
// AiFreeCounters()
// ****************
//
void __cdecl AiFreeCounters(AI_COUNTERS * ac)
{
   if (ac != NULL)
      free(ac);

   return;
}


//
// AiZeroCounters()
// ****************
//
void __cdecl AiZeroCounters(AI_COUNTERS * ac)
{
   if (ac != NULL) {
      memset((void *)ac, 0, pmd->AiCountersSize);
   }
   return;
}


//
// CyclesToSeconds()
// *****************
//
double __cdecl CyclesToSeconds(double Cycles0, double Cycles1)
{
   if (CPUSpeed == 0.0)
      return((double)0);
   else
      return(((Cycles1 - Cycles0) / CPUSpeed));
}


//
// CyclesToMilliseconds()
// **********************
//
double __cdecl CyclesToMilliseconds(double Cycles0, double Cycles1)
{
   if (CPUSpeed == 0.0)
      return((double)0);
   else
      return(((Cycles1 - Cycles0) / CPUSpeed) * 1000.0);
}


//
// CyclesToMicroseconds()
// **********************
//
double __cdecl CyclesToMicroseconds(double Cycles0, double Cycles1)
{
   if (CPUSpeed == 0.0)
      return((double)0);
   else
      return(((Cycles1 - Cycles0) / CPUSpeed) * 1000000.0);
}


//
// System-wide function support-query
// -----------------------------------
//


//
// IsS390()
// ********
//
int __cdecl IsS390(void)
{
   return(pmd->is_s390 || pmd->is_s390x);
}


//
// IsTprofSupported()
// ******************
//
int __cdecl IsTprofSupported(void)
{
   return(pmd->TprofSupported);
}

//
// IsTprofActive()
// ***************
//
int __cdecl IsTprofActive(void)
{
   if (!IsTprofSupported())
      return (0);
   if (TraceQueryMajor(0x10) && IsTraceActive())
      return (1);
   else
      return (0);
}

//
// IsEventTprofSupported()
// ***********************
//
int __cdecl IsEventTprofSupported(void)
{
   return(pmd->EventTprofSupported);
}


//
// IsCallstackSamplingSupported()
// ******************************
//
int __cdecl IsCallstackSamplingSupported(void)
{
   return (pmd->CallstackSamplingSupported);
}


//
// IsCallstackSamplingActive()
// ***************************
//
int __cdecl IsCallstackSamplingActive(void)
{
   return (int)pmd->scs_enabled;
}


//
// IsItraceSupported()
// *******************
//
int __cdecl IsItraceSupported(void)
{
   if (IsS390())
      return(0);
   else
      return(1);
}

//
// IsAiSupported()
// ***************
//
int __cdecl IsAiSupported(void)
{
   if (IsS390())
      return(0);
   else
      return(1);
}

//
// IsCpiSupported()
// ****************
//
int __cdecl IsCpiSupported(void)
{
   if (IsS390() || IsProcessorPPC64())
      return(0);
   else
      return(1);

}


//
// IsPerfCtrAccessSupported()
// **************************
//
int __cdecl IsPerfCtrAccessSupported(void)
{
   if (IsS390() || IsProcessorPPC64())
      return(0);
   else
      return(1);
}


//
// IsPerfCtrEventAccessSupported()
// *******************************
//
int __cdecl IsPerfCtrEventAccessSupported(void)
{
   if (IsS390() || IsProcessorPPC64())
      return(0);
   else
      return(1);
}

//
// IsAiActive()
// ************
//
int __cdecl IsAiActive(void)
{
   return(pmd->KI_Enabled);
}


//
// IsTimerHookRegistered()
// ***********************
//
int __cdecl IsTimerHookRegistered(void)
{
   return(pmd->TimerHookRegistered);
}

//
// IsVirtualMachine()
// ******************
//
int __cdecl IsVirtualMachine(void)
{
   return (pmd->is_vm);
}

//
// pi_get_kernel_info()
// ********************
//
int __cdecl pi_get_kernel_info(pi_kernel_info_t * pki)
{
   if (pki == NULL || pmd->kernel_start == 0)
      return (-1);

   pki->addr = pmd->kernel_start;
   pki->length = pmd->kernel_length;
   pki->version = pmd->kernel_version;
   pki->flags = pmd->kernel_flags;
   pki->hz = pmd->CpuTimerTick;
   pki->ts = 0;
   pki->cs = 0;
   return (0);
}

//
// IsDriverDebugPrintOn()
// **********************
//
int __cdecl IsDriverDebugPrintOn(void)
{
   return (pmd->dd_debug_messages);
}
