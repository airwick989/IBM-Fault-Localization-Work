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


#ifndef _PERFUTIL_CORE_H_
#define _PERFUTIL_CORE_H_

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

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <ctype.h>

#include <semaphore.h>

#include <sched.h>

#include <sys/types.h>

#include "itypes.h"
#include "perfctr.h"
#include "perfutilbase.h"
#include "perfutil.h"
#include "pitrace.h"
#include "perfmisc.h"


#define FOR_EACH_CPU(cpu) for (int fec=0, cpu=pmd->ActiveCPUList[0]; fec < (int)pmd->ActiveCPUs; fec++, cpu=pmd->ActiveCPUList[fec])

#define READ_RAW_INC(type, ptr) *(type *)ptr; \
                        ptr += sizeof(type);
#define READ_RAW_DEC(type, ptr) ptr -= sizeof(type);  \
                        *(type *)ptr;

#define WORKER_TIMEOUT_MILLIS 50

//
// Function prototypes
// *******************
//
//

// perfutil definitions
void	CheckForEnvironmenVariables(void);
int		MapTraceBuffer(void);
void	get_CPUSpeed(void);
int		_init_activeCpu_list();

// perfctr
int _IsPerfCounterEventSupported(int event_id);

//
// Architecture strings
// ********************
//

extern const char *p5_arch_string;
extern const char *p6_arch_string;
extern const char *pentium_m_arch_string;
extern const char *core_sd_arch_string;
extern const char *core_arch_string;
extern const char *p4_arch_string;
extern const char *ppc64_arch_string;
extern const char *amd64_arch_string;
extern const char *em64t_arch_string;
extern const char *s390_arch_string;

extern const char *intel_mfg_string;
extern const char *amd_mfg_string;
extern const char *ibm_mfg_string;


//
// Globals (instance data)
// ***********************
//
extern bool         dd_data_mapped;	// 1=DD data mapped, 0=DD data not mapped
extern MAPPED_DATA * pmd;				// Data mapped by device driver
extern MAPPED_DATA   md;				// Local mapped data in case we can't map

extern char * physbuf_va;				// Virtual of physically contiguous buffer
extern UINT64 physbuf_pa;				// Physical of physicall contiguous buffer

extern UCHAR         *pmecTable;		// Pointer to MEC table

extern int           MaxCpuNumber;             // Highest valid CPU number (0 origin)

extern float  CPUSpeed;	        // both vars are set in get_CPUSpeed()
extern INT64  llCPUSpeed;

extern ULONG         PerfddFeatures;       // Features supported by PERFDD
extern int           Ring3RDPMC;       // No ring3 access to PerfCtrs

#define INVALID_HANDLE_VALUE   0

extern int hDriver;     // driver file decriptor
#define DRIVER_LOADED    (hDriver != INVALID_HANDLE_VALUE)

extern UINT64 ctr_mask;                    // Counter mask

extern int           pu_debug;
extern int           pu_debug_ptt;


//
// Macros
// ******
//

#if defined (PU_TRACE)
#ifndef PU_DEBUG
#define PU_DEBUG
#endif
#endif

#undef PDEBUG
#if defined (PU_DEBUG)
#define PDEBUG(fmt, args...)    \
      printf( "pidd: [%s:%d] " fmt, __FUNCTION__ , __LINE__, ## args);
#else
#define PDEBUG(fmt, args...)
#endif  /* PU_DEBUG */

#ifdef PU_TRACE
#define TRACE_START(fmt, args...)   \
      PDEBUG(">>>> " fmt "\n", ## args)
#define TRACE_END(rc)  \
      PDEBUG("<<<< %d\n", rc)
#else
#define TRACE_START(fmt, args...)
#define TRACE_END(rc)
#endif

#define PI_PRINTK(fmt, args...)    \
	printf("pidd: " fmt, ## args)

// flush stdout so that the output is written in order
#define PI_PRINTK_ERR(fmt, args...)    \
	fflush(stdout); fprintf(stderr, "pidd [%s]: " fmt, __FUNCTION__, ## args)

#define RETURN_RC_FAIL(rc) \
   if (rc < PU_SUCCESS)  { \
      TRACE_END(rc);    \
      return rc; \
   }

#endif // _PERFUTIL_CORE_H_
