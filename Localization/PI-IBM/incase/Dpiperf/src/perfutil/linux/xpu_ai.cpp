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

//
// Linux Perf Util - Above Idle
//

#define  _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include "perfutil_core.hpp"

// Globals
AI_COUNTERS ai_last_read;
const char *proc_stat_filename = "/proc/stat";
FILE *pc_file = 0;

//
// TODO: set up cpu stats
// https://phoxis.org/2013/09/05/finding-overall-and-per-core-cpu-utilization/
// http://www.manpagez.com/man/3/getloadavg/
// http://www.linuxhowtos.org/System/procstat.htm
//

int read_counters(AI_COUNTERS *ac) {
}

//
// AiInit()
// ********
//
// Initialize and enable AI facility.
//
// Returns 0 on success, non-zero on errors.
//
int __cdecl AiInit(void) {
//	pc_file = fopen(proc_stat_filename, "r");
//	if (!pc_file)
//		return -1;
	return 0;
}

//
// AiTerminate()
// *************
//
// Terminate and disable AI facility.
//
void __cdecl AiTerminate(void) {
	//fclose(pc_file);
	return;
}

//
// AiGetCounters()
// ***************
//
// Reads raw AI cpu counters for all processors.
//
// Returns 0 on success, non-zero on errors.
//
int __cdecl AiGetCounters(AI_COUNTERS * ac) {

	return 0;
}
