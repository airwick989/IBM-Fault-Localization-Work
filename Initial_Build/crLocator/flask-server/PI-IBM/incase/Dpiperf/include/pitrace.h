//----------------------------------------------------------------------------
//
//   IBM Performance Inspector
//   Copyright (c) International Business Machines Corp., 2003 - 2007
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
/*
 *
 * Date      Author       Defect/       Description                   Change
                          Feature                                      Flag
 *----------------------------------------------------------------------------
 * 02/06/02  Ron Cadima     n/a    Add _PerfBuf definition             RAC1
 * ...
 * 06/19/06  Milena                Removed compression variables
 * 07/19/06  Milena                More cleanup
 * 08/25/06  Milena                Changed pitrace_request, counter support
 * 09/25/06  Milena                More cleanup, updated pitrace request
 * 02/09/07  Milena                More cleanup - trace-relevant defines are moved to bputil_common.h
 *                                 so they can be used by post/perfutil/driver
 */

     
#ifndef _PITRACE_H_
#define _PITRACE_H_

#ifdef __KERNEL__
#include <asm/byteorder.h>

#ifndef STAP_ITRACE // DEFS
#if  defined( __BIG_ENDIAN) 
#define __BYTE_ORDER  __BIG_ENDIAN
#elif defined(__LITTLE_ENDIAN)
#define __BYTE_ORDER  __LITTLE_ENDIAN
#else
#error "Undetermined endian!"
#endif
#endif // STAP_ITRACE DEFS

#else  //not kernel
#include <endian.h>
#endif

#include <bputil_common.h>

#define DEFAULT_TPROF_TICK_DELTA 1   /* Skip no timer ticks by default */

#define NR_THREADS    65537           /* Default number of tasks that can be handled */
#define PI_MAX_THREAD 65536

#define PI_TRACE_BUF_DEFAULT    (0x100000 * 3)
#define PI_MTE_BUF_DEFAULT      (0x100000 * 5)
#define PI_SECTION_DEFAULT       0x10000

#ifndef STAP_ITRACE // DEFS
union perf_addr {
     struct {
#if defined __BIG_ENDIAN
  #if __BYTE_ORDER == __BIG_ENDIAN 
       uint32_t high;
       uint32_t low;
  #else
       uint32_t low;
       uint32_t high;
  #endif
#else  // kernel code, little endian
  #if __BYTE_ORDER == __LITTLE_ENDIAN
	uint32_t low;
	uint32_t high;
  #else
  #error "Undetermined endian!"
  #endif
#endif
   
     } sval;

    uint64_t cval;
};


union rdval {
     struct {
#if defined __BIG_ENDIAN
   #if __BYTE_ORDER == __BIG_ENDIAN
       uint32_t hval;
       uint32_t lval;
   #else
       uint32_t lval;
       uint32_t hval;
   #endif
#else  // kernel code, little endian
  #if __BYTE_ORDER == __LITTLE_ENDIAN
        uint32_t lval;
        uint32_t hval;
  #else
  #error "Undetermined endian!"
  #endif
#endif

     } sval;
     uint64_t cval;
  };
#endif // STAP_ITRACE DEFS

/* pitrace driver Request block. */

typedef struct pitrace_request {
    uint64_t addr;
    uint64_t addr2;
    uint32_t val1;
    uint32_t val2;
    uint32_t val3;    
    uint32_t val4;
} pitrace_request;

#endif
