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
// Change History
//
//
// Date		Description
//----------------------------------------------------------------------------
// ...
// 04/24/06	Code cleanup - deprecated PTT functions deleted
// 07/27/06     Added counter/event support
// 08/11/06     More code cleanup - removed unused ioctls
// 10/27/06     Added CMD_SOFTTRACE_SET_MODE ioctl
// 10/03/07     Added SCS ioctls
// 10/30/07     Added CMD_PERFDD_WAIT_FOR_SAMPLING for SCS
// 05/21/09     Added CMD_PERFDD_DBGPRINT
// 07/16/09     Added CMD_PERFDD_DONE_SAMPLING
//

#ifndef _PERFUTIL_BASE_H_
#define _PERFUTIL_BASE_H_

#define TRACE_VERSION (0x12092016)   /* Tools version number */

#include <linux/ioctl.h>
#include "pitrace.h"

/* a per CPU structure for the implementation of the KernelIdle instrumentation */
/*RYH remove extra fields */
typedef struct ki_struct {
    uint64_t KI_statechg;
    uint64_t KI_current;
    uint64_t KI_idle;
    uint64_t KI_busy;
    uint64_t KI_intr;
    uint64_t KI_unk;
    uint64_t KI_proc_state;
    uint32_t KI_next_proc_state;
    uint32_t KI_idepth;                /* interrupt depth */
    uint32_t KI_toplevel;              /* outer interrupt level */
    uint32_t KI_interrupted_state;     /* interrupted */
    uint32_t IdleDispatchCount;
    uint32_t BusyDispatchCount;
    uint32_t UserDispatchCount;
    uint32_t IntrDispatchCount;
}  ki_struct;

//
// Define and equates for PerfCall
//
#define PI_IOC_MAGIC 'i'

//
// SWTRACE
//
#define CMD_SOFTTRACE_ON               _IO(PI_IOC_MAGIC, 0x10)
#define CMD_SOFTTRACE_OFF              _IO(PI_IOC_MAGIC, 0x11)
#define CMD_SOFTTRACE_TERMINATE        _IO(PI_IOC_MAGIC, 0x12)
#define CMD_SOFTTRACE_GET              _IOWR(PI_IOC_MAGIC, 0x13, pitrace_request)
#define CMD_SOFTTRACE_ALLOC            _IOW(PI_IOC_MAGIC, 0x14, pitrace_request)
#define CMD_SOFTTRACE_ENABLE           _IOW(PI_IOC_MAGIC, 0x16, pitrace_request)
#define CMD_SOFTTRACE_DISABLE          _IOW(PI_IOC_MAGIC, 0x17, pitrace_request)
#define CMD_PERFCTR_TPROF              _IOW(PI_IOC_MAGIC, 0X18, pitrace_request)
#define CMD_UserLevelHook              _IOW(PI_IOC_MAGIC, 0X19, char[MAX_CPUS])
#define CMD_SOFTTRACE_SET_MODE         _IOW(PI_IOC_MAGIC, 0x1A, pitrace_request)
#define CMD_SOFTTRACE_SUSPEND          _IO(PI_IOC_MAGIC, 0x1B)
#define CMD_SOFTTRACE_RESUME           _IO(PI_IOC_MAGIC, 0x1C)
#define CMD_SOFTTRACE_DISCARD          _IO(PI_IOC_MAGIC, 0x1D)

//
// CPU Utilization
//
#define CMD_KI_Enable                  _IO(PI_IOC_MAGIC, 0X20) /* Turn on the KernelIdle instrumentation */
#define CMD_KI_Disable                 _IO(PI_IOC_MAGIC, 0X21)
#define CMD_KI_GetData                 _IOWR(PI_IOC_MAGIC, 0X22, pitrace_request)

//
// Performance Counter & MSR access
//
#define CMD_PERFCTR_START              _IOW(PI_IOC_MAGIC, 0X30, pitrace_request)
#define CMD_PERFCTR_STOP               _IOW(PI_IOC_MAGIC, 0X31, pitrace_request)
#define CMD_PERFCTR_RESUME             _IOW(PI_IOC_MAGIC, 0X32, pitrace_request)
#define CMD_PERFCTR_SET                _IOW(PI_IOC_MAGIC, 0X33, pitrace_request)
#define CMD_PERFCTR_READ               _IOWR(PI_IOC_MAGIC, 0X34, pitrace_request)
#define CMD_PERFCTR_READ_ALL           _IOWR(PI_IOC_MAGIC, 0X35, pitrace_request)
#define CMD_WRITE_MSR                  _IOW(PI_IOC_MAGIC, 0X36, pitrace_request)
#define CMD_READ_MSR                   _IOWR(PI_IOC_MAGIC, 0X37, pitrace_request)

#define CMD_CTREVENT_START             _IOW(PI_IOC_MAGIC, 0X38, pitrace_request)
#define CMD_CTREVENT_TERM              _IOW(PI_IOC_MAGIC, 0X39, pitrace_request)
#define CMD_CTREVENT_STOP              _IOW(PI_IOC_MAGIC, 0X3A, pitrace_request)
#define CMD_CTREVENT_RESUME            _IOW(PI_IOC_MAGIC, 0X3B, pitrace_request)
#define CMD_CTREVENT_RESET             _IOW(PI_IOC_MAGIC, 0X3C, pitrace_request)
#define CMD_CTREVENT_GET               _IOWR(PI_IOC_MAGIC, 0X3D, pitrace_request)
#define CMD_SET_EVENT_DEFINITIONS      _IOW(PI_IOC_MAGIC, 0X3E, pitrace_request)

//
// Itrace
//
#define CMD_ITRACE_INSTALL             _IOW(PI_IOC_MAGIC, 0x40, pitrace_request)
#define CMD_ITRACE_REMOVE              _IO(PI_IOC_MAGIC, 0x41)
#define CMD_ITRACE_ON                  _IO(PI_IOC_MAGIC, 0x42)
#define CMD_ITRACE_OFF                 _IO(PI_IOC_MAGIC, 0x43)
#define CMD_ITRACE_ENABLE              _IOW(PI_IOC_MAGIC, 0x45, pitrace_request)
#define CMD_ITRACE_DISABLE             _IO(PI_IOC_MAGIC, 0x46)

//
// Per thread time
//
#define CMD_GetPerfTime                _IOR(PI_IOC_MAGIC, 0X50, union rdval) /* Read current performance clock value */
#define CMD_PttInit                    _IOW(PI_IOC_MAGIC, 0X51, int[PTT_MAX_METRICS])
#define CMD_PttTerminate               _IO(PI_IOC_MAGIC, 0X52)
#define CMD_READ_CYCLES_ON_ALL_CPUS    _IO(PI_IOC_MAGIC, 0x55)

//
// System memory access/stealing
//
#define CMD_STEALSYSMEM               _IOW(PI_IOC_MAGIC, 0X60, pitrace_request)
#define CMD_FREESYSMEM                _IO(PI_IOC_MAGIC, 0X61)
#define CMD_PEEKMEM                   _IOWR(PI_IOC_MAGIC, 0X62, pitrace_request)

//
//  Call stack sampling
//
#define CMD_PERFDD_ENABLE_SAMPLING            _IOW(PI_IOC_MAGIC, 0X70, pitrace_request)
#define CMD_PERFDD_DISABLE_SAMPLING           _IO(PI_IOC_MAGIC, 0X71)
#define CMD_PERFDD_BLAST_SAMPLING             _IO(PI_IOC_MAGIC, 0X72)
#define CMD_PERFDD_PAUSE_SAMPLING             _IO(PI_IOC_MAGIC, 0X73)
#define CMD_PERFDD_RESUME_SAMPLING            _IO(PI_IOC_MAGIC, 0X74)
#define CMD_PERFDD_WAIT_FOR_SAMPLING          _IO(PI_IOC_MAGIC, 0X75)
#define CMD_PERFDD_DONE_SAMPLING              _IOW(PI_IOC_MAGIC, 0X76, pitrace_request)
#define CMD_PERFDD_SET_DELAY                  _IOW(PI_IOC_MAGIC, 0X77, pitrace_request)

//
// Misc
//
#define CMD_PERFDD_DBGPRINT                   _IOW(PI_IOC_MAGIC, 0X80, pitrace_request)

// Windows compatibility error codes
#define STATUS_ACCESS_VIOLATION         0xc0000005
#define STATUS_PRIVILEGED_INSTRUCTION   0xC0000096

#define CTR_VALUE_AMD_MASK    0x0000FFFFFFFFFFFFLL
#define CTR_VALUE_INTEL_MASK  0x000000FFFFFFFFFFLL
#define CTR_VALUE_PPC64_MASK  0xFFFFFFFFFFFFFFFFLL

#endif // _PERFUTIL_BASE_H_
