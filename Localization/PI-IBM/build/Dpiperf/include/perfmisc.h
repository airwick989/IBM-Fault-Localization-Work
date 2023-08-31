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

#ifndef _PERFMISC_H_
#define _PERFMISC_H_

#include "itypes.h"

#if defined(_LINUX)
#define DECLSPEC_ALIGN(x)
#define __cdecl
#endif

//
//***********************************************************//
//***********************************************************//
//                          Constants                        //
//***********************************************************//
//***********************************************************//
//

#define MAX_APIC_ID   256


#define MAX_INTERRUPT_VECTORS          256 // Number of IDT entries

#define PI_MAX_PATH  1024                  // Max path len

// Values for ThreadData.state field
#define TD_STATE_INVALID      0            // Thread data is not valid
#define TD_STATE_VALID        1            // Thread data is valid

// Divisor values
#if !defined(_LEXTERN)
#define DEFAULT_SCS_DIVISOR           8
#define DEFAULT_TPROF_DIVISOR         8
#else
#define DEFAULT_SCS_DIVISOR           1
#define DEFAULT_TPROF_DIVISOR         1
#endif

// Offsets for mmap
#define PI_PTT_DATA_OFFSET 0

#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif

#define PI_MAPPED_DATA_OFFSET PAGE_SIZE
#define PI_MAPPED_TRACE_OFFSET (PAGE_SIZE*2)
#define PI_MAPPED_MTE_OFFSET   (PAGE_SIZE*3)

// TODO: redifine these for windows
#define NUM_CPUS_ACTIV	sysconf(_SC_NPROCESSORS_ONLN)
#define NUM_CPUS_INSTL	sysconf(_SC_NPROCESSORS_CONF)
#define PI_HZ			sysconf(_SC_CLK_TCK)

//
//***********************************************************//
//***********************************************************//
//                         Structures                        //
//***********************************************************//
//***********************************************************//
//
//


//
// PerfCtr Event Information
//
struct _Perf_Ctr_Event_Info {
    int       CtrEvent[CTR_MAX_EVENTS];      // Event being counted (or zero)
    int       CtrNumber[CTR_MAX_EVENTS];     // Counter number used for event
    uint32    CtrAddr[CTR_MAX_EVENTS];       // Counter MSR
    uint32    CccrAddr[CTR_MAX_EVENTS];      // CCCR MSR (P4/EM64T) / EvtSelReg MSR (P6/AMD64)
    uint32    EscrAddr[CTR_MAX_EVENTS];      // ESCR MSR
};
typedef struct _Perf_Ctr_Event_Info  PERFEVT_INFO;

//
// CPI_RAW_DATA
// ************
//
// Internal data used to maintain information about each CPI measurement instance.
// Not intended for applications to access but made available anyway. It should be
// treated as READ ONLY data. Modifying it in any way will, at the very least,
// cause incorrect CPI data to be calculated.
//
DECLSPEC_ALIGN(8) struct _cpi_raw_data {
   // Raw values at start of measurement interval.
   // - Set by CpiInit() and CpiStart()
   UINT64    ai_busy_start[MAX_CPUS];       // Starting AI busy cycle counters (busy/idle/intr)      [SCALED]
   UINT64    ai_cycles_start[MAX_CPUS];     // Starting AI elapsed cycle counters                    [SCALED]
   UINT64    cpu_instr_start[MAX_CPUS];     // Starting instr retired counters                       [RAW, SCALED]
   UINT64    cpu_cycles_start[MAX_CPUS];    // Cycles when starting instr retires read               [RAW, SCALED]
   UINT64    thread_instr_start;            // Starting on-thread instr count                        [ONTHREAD]
   UINT64    thread_cycles_start;           // Ending on-thread cycles count                         [ONTHREAD]

   // Elapsed values (since start) at end of measurement interval (end-start delta)
   // - Set by CpiGet() and CpiGetAndTrace()
   UINT64    ai_busy[MAX_CPUS];             // Per-processor AI busy cycle counters for interval     [SCALED]
   UINT64    ai_cycles[MAX_CPUS];           // Per-processor AI elapsed cycle counters for interval  [SCALED]
   UINT64    cpu_instr[MAX_CPUS];           // Per-processor Instr retired during interval           [RAW, SCALED]
   UINT64    cpu_cycles[MAX_CPUS];          // Per-processor Cycles elapsed during interval          [RAW, SCALED]
   UINT64    thread_instr;                  // On-thread instr count during interval                 [ONTHREAD]
   UINT64    thread_cycles;                 // On-thread cycles count during interval                [ONTHREAD]

   // Calculated values
   // - Set by CpiGet() and CpiGetAndTrace()
   UINT64    cpu_cycles_scaled[MAX_CPUS];   // Per-processor scaled cycles                           [SCALED]
   UINT64    sys_cycles_scaled;             // Sum of scaled cycles on all processor                 [SCALED]
   UINT64    sys_instr;                     // Sum of intructions executed by all processors         [RAW, SCALED]
   UINT64    sys_cycles;                    // Sum of elapsed cycles on all processor                [RAW, SCALED]

   // Internal data
   char      * hook_label;                  // Optional trace hook label
   int       hook_minor;                    // Optional trace hook minor code
   UINT32    init_type;                     // How facility was initialized
   int       ptt_instr_ix;                  // PTT metric number for instructions
   int       ptt_cycles_ix;                 // PTT metric number for cycles
   UINT32    signature;
   UINT32    unused;                        // Alignment

   // Data returned to users
   CPI_DATA  user_data;                     // For use by the CPI facility (CPI_RAW_DATA *)
};

typedef struct _cpi_raw_data  CPI_RAW_DATA, cpi_raw_data_t;

#define CPI_RAW_SIGNATURE            0x49504323   // "#CPI"
#define CPI_TRACE_MAJOR_CODE         0xC0
#define CPI_TRACE_MINOR_CODE         0x01


//
// CPUUTIL_RAW_DATA
// ****************
//
// Internal data used to maintain information about each CPU utilization
// measurement instance.
// Not intended for applications to access but made available anyway. It should be
// treated as READ ONLY data. Modifying it in any way will, at the very least,
// cause incorrect CPU utilization data to be calculated.
//
DECLSPEC_ALIGN(8) struct _cpuutil_raw_data {
   // Raw counters at start of measurement interval.
   // - Set by CpuUtilInit() and CpuUtilStart()
   AI_COUNTERS  ai_start[MAX_CPUS];         // Starting AI counters

   // Raw counters at end of measurement interval.
   // - Set by CpuUtilGet() and CpuUtilGetAndTrace()
   AI_COUNTERS  ai_end[MAX_CPUS];           // Ending AI counters

   // Elapsed values (since start) at end of measurement interval (end-start delta)
   // - Set by CpuUtilGet() and CpuUtilGetAndTrace()
   UINT64  elapsed[MAX_CPUS];               // Per-processor elapsed cycles
   UINT64  busy[MAX_CPUS];                  // Per-processor AI busy cycle counters for interval
   UINT64  idle[MAX_CPUS];                  // Per-processor AI idle cycle counters for interval
   UINT64  intr[MAX_CPUS];                  // Per-processor AI interrupt cycle counters for interval
   UINT32  disp_cnt[MAX_CPUS];              // Per-processor dispatch count for interval
   UINT32  intr_cnt[MAX_CPUS];              // Per-processor interrupt count for interval

   // Cumulative values (since initialization) at end of measurement interval (end-init delta)
   // - Set by CpuUtilGet() and CpuUtilGetAndTrace()
   UINT64  total_elapsed[MAX_CPUS];         // Per-processor elapsed cycles
   UINT64  total_busy[MAX_CPUS];            // Per-processor total AI busy cycle counters
   UINT64  total_idle[MAX_CPUS];            // Per-processor total AI idle cycle counters
   UINT64  total_intr[MAX_CPUS];            // Per-processor total AI interrupt cycle counters
   UINT32  total_disp_cnt[MAX_CPUS];        // Per-processor total dispatch count for
   UINT32  total_intr_cnt[MAX_CPUS];        // Per-processor total interrupt count for

   // Internal data
   char    * hook_label;                    // Optional trace hook label
   int     hook_minor;                      // Optional trace hook minor code
   UINT32  signature;
   UINT32  trace_mode;                      // Whether or not tracing on Get()
#if defined(_64BIT)
   UINT32  alignment;
#endif
   // Data returned to users
   CPUUTIL_DATA  user_data;
};
typedef struct _cpuutil_raw_data  CPUUTIL_RAW_DATA, cpuutil_raw_data_t;

#define CPUUTIL_RAW_SIGNATURE            0x55555043   // "CPUU"
#define CPUUTIL_TRACE_MAJOR_CODE         0xC8
#define CPUUTIL_TRACE_MINOR_CODE         0x01

enum TraceScope {
	TS_SYSTEM, TS_PROCESS
};

//
// Data mapped to ring3
// ********************
//
// Key:
//      (D)  Dynamic data   - updated by driver as it changes.
//      (S)  Static data    - set once and never updated.
//      (R)  Requested data - set in response to a request.
//
struct _Mapped_Data {
   //
   // Device Driver Information
   //
   UINT64       ActiveCPUSet;                  // (S) Bit mask of active CPUs
   UINT64       InstalledCPUSet;               // (S) Bit mask of installed CPUs
   UINT32       Version;
   UINT32       TraceIntervalId;               // (D) Trace interval id (since DD loaded)
   UINT32       InstalledCPUs;                 // (S) Number of CPUs installed
   UINT32       ActiveCPUs;                    // (S) Number of CPUs active (in use)
   int			ActiveCPUList[MAX_CPUS];
   int			PageSize;						// size of memory pages
   int			TprofScope;							// cpu or pid based
   int			TprofPidMain;						// PID of process main task

   bool			TraceOn;                       // (D) Trace is on (may or may not be active)
   bool			TraceActive;                   // (D) Trace is active (on and tracing)
   UINT32       TraceBufferSize;               // (D) Trace buffer size (per processor)
   UINT32       MTEBufferSize;                 // (D) MTE buffer size (per processor)
   bool			ItraceHandlersInstalled;       // (D) ITrace intr handlers installed
   bool			DispatchHookInserted;          // (D) 0 means not hooked, 1 means hooked
   bool			InterruptHooksInserted;        // (D) 0 means not hooked, 1 means hooked
   bool			TimerHookRegistered;           // (D) 0 means not registered, 1 means registered
   bool			SysHooksRegistered;            // (D) 0 means not registered, 1 means registered
   bool			KI_Enabled;                    // (D) 1= do KI measurements, 0= don't
   bool			PTT_Enabled;                   // (D) 1= PTT enabled, 0= not
   UINT32       AiCountersSize;                // (S) Minimum size (bytes) of AI_COUNTERS struct
   UINT32       PttStatsSize;                  // (S) Minimum size (bytes) of PTT_STATISTICS struct
   UINT32       PttMaxMetrics;                 // (S) Max number of metrics supported
   UINT32       PttNumMetrics;                 // (S) Current number of metrics
   UINT32       TraceMode;                     // (D) Mode of tracing
   UCHAR        MecTable[NUM_MAJORS];          // (D) MEC Table

	//
	// values from driver_info
	//
   bool			MteActive;
   uint32_t   mte_type;                        // Buffer type for MTE hooks

   //
   // Continuous trace information
   //
   UINT32       SectionSize;                   // (D) Section size for cont writing
   UINT32       file_error;                    // (D) Set if error when writing trace
   UINT32       TraceAnonMTE;                  // (S) 1= trace anon MTE, 0= do not trace
   CHAR        trace_file_name[PI_MAX_PATH];  // (D) full path file name for cont tracing

   //
   // Processor Information
   //
   INT64        llCpuSpeed;                    // (S) CPU Speed (in Hz)  (calculated)
   UINT32       FSBSpeed;                      // (S) Front Side (system) bus speed (in Hz)
   UINT32       CpuSignature;                  // (S) CPU signature
   UINT32       CpuFamily;                     // (S) CPU Family: 5=P5, 6=P6, 15=P4
   UINT32       CpuModel;
   UINT32       CpuFeatures;                   // (S) CPU Features (EDX)
   UINT32       CpuFeaturesEcx;                // (S) CPU Features (ECX)
   UINT32       CpuArchitecture;               // (S) CPU Architecture
   UINT32       CpuExtendedFeatures;           // (S) CPU Features [CPUID(0x80000001)]
   UINT32       CpuManufacturer;               // (S) CPU Manufacturer
   UINT32       HyperThreadingSupported;       // (S) Hyper-Threading supported
   UINT32       HyperThreadingEnabled;         // (S) Hyper-Threading enabled
   UINT32       CpuSpeed;                      // (S) CPU Speed (in MHz)  (calculated)
   UINT32       CpuTimerTick;                  // (S) Timer ticks per second (HZ kernel value)
   UINT32       ApicSystem;                    // (S) Using local APIC for TPROF
   UINT32       LogicalCpuCount;               // (S) Number of LPs per physical CPU
   UINT32       Spare1;
   UINT64       LogicalSet[MAX_CPUS_LOGICAL];  // (S) Bit mask of active Logical Processors
                                               //     Bit set for each OS processor assigned to LP/Thread[n]
   UINT64       CoreSet[MAX_CPUS_CORE];        // (S) Bit mask of active Cores
                                               //     Bit set for each OS processor assigned to Core[n]

   UINT32       is_amd;                        // (S) AMD processor
   UINT32       is_intel;                      // (S) Intel processor
   UINT32       is_ppc64;                      // (S) PPC64
   UINT32       is_32bit;                      // (S) 32-bit processor
   UINT32       is_64bit;                      // (S) 64-bit processor
   UINT32       is_x86;                        // (S) x86 (IA-32) processor
   UINT32       is_x64;                        // (S) x86_64/AMD64 processor
   UINT32       is_p4;                         // (S) Pentium 4 processor
   UINT32       is_p5;                         // (S) Pentium 5 processor
   UINT32       is_p6;                         // (S) Pentium 6 processor
   UINT32       is_amd64;                      // (S) 64-bit AMD64 processor
   UINT32       is_em64t;                      // (S) 64-bit EM64T processor
   UINT32       is_s390;                       // (S) 31-bit S390 processor
   UINT32       is_s390x;                      // (S) 64-bit S390x processor
   UINT32       is_smp;                        // (S) SMP system
   UINT32       is_uni;                        // (S) UNI system
   UINT32       is_multicore;                  // (S) multicore
   UINT32       is_pentium_m;                  // (S) Pentium M/Celeron/M processor
   UINT32       is_core_sd;                    // (S) Core Solo/Duo processor (** not Core architecture **)
   UINT32       is_core;                       // (S) Core2 processor (** Core architecture **)
   UINT32       CoresPerPhysical;              // (S) (C) Number of cores per physical processor
   UINT32       is_vm;                         // (S) OS running as a guest VM under something else
   UINT32       is_athlon;                     // (S) AMD Athlon processor
   UINT32       is_opteron;                    // (S) AMD Opteron processor
   UINT32       is_phenom;                     // (S) AMD Phenom processor
   UINT32       is_nehalem;                    // (S) Intel Nehalem processor (Core i7/Core i5)
   UINT32       is_atom;                       // (S) Atom processor
   UINT32       is_westmere;                   // (S) Intel Westmere processor
   UINT32       is_sandybridge;                // (S) Intel Sandybridge processor
   UINT32       is_ivybridge;                  // (S) Intel Ivybridge processor

   //
   // DD Info
   //
   UINT32       cpu_list[MAX_CPUS];            // (S) OS cpu numbers, 0,1,2, etc. unless no smt
   UINT8        ApicIdToCpuNumber[MAX_APIC_ID];// (S) Mapping apic ids to OS cpu numbers

   //
   // TPROF information
   //
   UINT32       TprofSupported;                // (S)
   UINT32	    EventTprofSupported;           // (S)
   UINT32       TprofInterruptVector;          // (S) Tprof interrupt vector number
   UINT32       CurrentTprofRate;              // (D) Current Tprof rate (TprofRate)
   UINT32       DefaultTprofRate;              // (D) default Tprof rate (TprofRate)
   UINT32       TprofMode;                     // (D) Whether time or event (counter) based TProf
   UINT32       TprofCtrEvent;                 // (D) Event driving Tprof
   UINT32       TprofTickDelta;                // (D) Number of timer ticks to skip
   UINT32       DefaultTimeTprofDivisor;       // (D) Calculated for effective rate of about 100

   UINT32       DefaultEventDivisor;           // (S) Same for Tprof and SCS

   //
   // Callstack Sampling
   //
   UINT32        CallstackSamplingSupported;    // (S)
   UINT32        DefaultTimeScsDivisor;         // (D) Calculated for effective rate of about 32
   UINT32        CurrentScsRate;                // (D) Current SCS rate
   UINT32        scs_enabled;


   //
   // Performance counters (RAW) information
   //
   int       PerfCtrAccessSupported;        // (S)
   int       NumPerfCtrs;                   // (S) Number of Performance Counters
   int       MaxPerfCtrNumber;              // (S) Highest valid Performance Counter number
   int       CtrValueArraySize;             // (S) Minimum size (bytes) of the ctr_value array
   UINT64    CtrValidBitsMask;              // (S) Mask with either 40 (Intel) or 48 (AMD) bits

   //
   // Performance counters (EVENTS) information
   //
   UINT32       CtrEventNum;                   // (D) Number of counter events supported
   UINT32       PerfCtrOwner;                  // (D) PID of exclusive user of the perf counters
   UINT32       MaxCounterEvents;              // (S) Max number of simultaneous perfctr events
   UINT32       CtrEventLowestId;              // (S) Lowest event id
   UINT32       CtrEventHighestId;             // (S) Highest event id
   UINT32       HaveCtrEventDef;               // (D) DD knows about counter events
   PERFEVT_INFO PerfEvtInfo[MAX_CPUS];         // (D) Perf Counter Event Information

   //
   // Per-thread time information and ITrace variables
   //
   UINT32       PttMaxThreadId;                // (D) Largest pid/tid we support

   UINT32       ItraceSkipCount;               // (D) Number of timer ticks to skip when itracing
   UINT32       ItraceUOverhead;               // (D) Itrace instruction overhead for user-space
   UINT32       ItraceInstOn;                  // (D) Itrace with instruction counts in the trace (set by the driver)

   UINT32       dd_debug_messages;             // (D) Driver debug messages (1 on, 0 off)

   // Performance counters
   int          individual_ctr_controls;       // (S) Each perfctr is controlled individually

   // Kernel information
   uint64_t     kernel_start;                  // (S) Kernel start address
   uint32_t     kernel_length;                 // (S) Kernel length (in bytes)
   uint32_t     kernel_version;                // (S) Kernel version (LINUX_VERSION_CODE)
   uint32_t     kernel_flags;                  // (S) Flags (see KFLAGS_* in perfutil.h)
                                               //

   //
   // Additions go at the end ...
   //
};
typedef struct _Mapped_Data MAPPED_DATA;


//
// Definition of TprofMode
//

#define TPROF_MODE_TIME       0
#define TPROF_MODE_EVENT      1
#define TPROF_MODE_TIME_APIC_TIMER   2


//
// For APIs that specify an event id
// ---------------------------------
//
#define ALL_EVENTS          -1


//
//***********************************************************//
//***********************************************************//
//    Prototypes for APIs not declared in perfutil.h         //
//***********************************************************//
//***********************************************************//
//

#ifdef __cplusplus
extern "C" {
#endif
int IsProcessorNumberValid(int cpunum);
int ITraceHandlersInstalled(void);

MAPPED_DATA * __cdecl GetMappedDataAddress(void);

void breakpoint(void);
#ifdef __cplusplus
}
#endif

//
// Combine 2 ULONGs (high and low) into one UINT64
// ***********************************************
// ex: ll = Uint64FromUlongs(ulhigh, ulllow)
//
#define Uint64FromUlongs(uh, ul)   (UINT64)(((UINT64)((uh)) << 32) | (ul))


//
// Split a UINT64 into 2 ULONGs (high and low)
// *******************************************
// ex: ulhigh = UlongHighFromUint64(ll)
//     ullow  = UlongLowFromUint64(ll)
//
#define UlongHighFromUint64(ll)    (ULONG)((ll) >> 32)
#define UlongLowFromUint64(ll)     (ULONG)(ll)
#define Uint32HighFromUint64(ll)   (uint32_t)((ll) >> 32)
#define Uint32LowFromUint64(ll)    (uint32_t)(ll)
#define High32(ll)                 (ULONG)((ll) >> 32)
#define Low32(ll)                  (ULONG)(ll)

#endif // _PERFMISC_H_
