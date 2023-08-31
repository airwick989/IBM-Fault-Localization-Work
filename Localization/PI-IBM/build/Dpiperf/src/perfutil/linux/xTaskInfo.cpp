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
// Linux Perf Util - Trace
//

/*
 * This is the visible layer
 * It is mainly used for translation and control
 */

#define  _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>

#include "utils/pi_time.h"

#include "perfutil_core.hpp"
#include "pu_trace.hpp"
#include "PiFacility.hpp"
#include "SampleProcessor.hpp"
#include "TaskInfo.hpp"

namespace pi {
namespace pu {

int TaskInfo::getProcessTidList(int pid, std::vector<int> &tidList) {
	// make sure the list is empty
	tidList.clear();

	DIR *dp;
	struct dirent *dirp;
	char buff[HOOK_STRLEN_MAX];

	sprintf(buff, "/proc/%d/task", pid);
	if((dp  = opendir(buff)) == NULL) {
		PI_PRINTK_ERR("Failed to open directory: %s\n", buff);
		return readSysError("Failed to open directory");
	}

	while ((dirp = readdir(dp)) != NULL) {
		int tid;
		sscanf(dirp->d_name, "%d", &tid);
		if (tid > 0)
		   tidList.push_back(tid);
	}
	closedir(dp);

	return 0;
}

/*
 * process_pid_name(process id, thread id)
 * ************ Internal *****************
 * Tries to collect the process/thread name
 * Writes a pid name hook
 */
int TaskInfo::handleProcessName(int pid, int tid) {
	FILE *commf = 0;
	char buff[HOOK_STRLEN_MAX];
	int rc = 0;
	char *str = 0;
	pi::pu::RecordHeader header;

	header.timeStamp = get_monotonic_time_nanos();
	header.pid = pid;
	header.tid = tid;
	header.cpu = 0;

	sprintf(buff, "/proc/%d/task/%d/comm", pid, tid);
	commf = fopen(buff, "r");
	if (commf <= 0) {
		PI_PRINTK_ERR("Failed to open file: %s\n", buff);
		return readSysError("Failed to open file");
	}

	str = fgets(buff, HOOK_STRLEN_MAX, commf);
	if (str && buff[0]) {
		str = strtok(buff, "\n"); // get rid of the new line character at the end of the string
		std::string name(buff);
		PDEBUG("unprocessed pid:%d tid:%d name:%s\n", pid, tid, buff);
		int64_t timeStamp = get_monotonic_time_nanos();
		// send it to mte on cpu 0. FIXME if using more than 1 MTE buff
		rc = setProcessName(header, name);
	}
	else {
		PDEBUG("unprocessed pid:%d tid:%d No name found\n", pid, tid);
	}

	fclose(commf);
	return rc;
}

// The max number of items that can appear in a line of the maps file
// this actually more like 7
#define MAX_MAPS_TOKENS		16

int TaskInfo::getMaxPid() {
   FILE *fpid = 0;
   int max_pids = 0;

   fpid = fopen("/proc/sys/kernel/pid_max", "r");
   if (fpid <= 0) {
      PI_PRINTK_ERR("Failed to open file: %s\n", "/proc/sys/kernel/pid_max");
      return readSysError("Failed to open file");
   }
   if (fscanf(fpid, "%d", &max_pids) <= 0) {
      PI_PRINTK_ERR("Failed read contents of file: %s\n", "/proc/sys/kernel/pid_max");
      return readSysError("Failed to open file");
   }
   fclose(fpid);

   return max_pids;
}

/*
 * process_pid_maps(process id)
 * ********* Internal *********
 * Collects the memory mapping for the process
 * This reads the maps file for that process and writes out the hooks for each segment
 */
int NRM2TaskInfo::handleProcessMaps(int pid) {
   PDEBUG("unprocessed maps for pid:%d\n", pid);
   FILE *mapsf = 0;
   char buff[HOOK_STRLEN_MAX << 1];
   char *tokens[MAX_MAPS_TOKENS] = {0};
   uint64_t addr_start, addr_end, len;
   int num_toks = 0;
   int num_lines = 0;
   int rc = 0;
   pi::pu::RecordHeader header;

   header.timeStamp = get_monotonic_time_nanos();
   header.pid = header.tid = pid;
   header.cpu = 0;
   std::string name;

   sprintf(buff, "/proc/%d/maps", pid);
   mapsf = fopen(buff, "r");
   if (mapsf <= 0) {
      PI_PRINTK_ERR("Failed to open file: %s\n", buff);
      return readSysError("Failed to open file");
   }

   // read each line
   while (fgets(buff, HOOK_STRLEN_MAX << 1, mapsf)) {
      num_lines++;
      num_toks = 0;
      // parse out the tokens. should be 5 or 6. sometimes 7
      tokens[num_toks] = strtok(buff, " \t\n\r");
      while (tokens[num_toks]) {
         if (strlen (tokens[num_toks])) {
            num_toks++;
         }
         tokens[num_toks] = strtok(0, " \t\n\r");
      }
      // check if the segment is executable using the x in the third position
      // if not then do not process it
      if (tokens[1][2] != 'x')
         continue;
      //PDEBUG("Maps open pid:%d line:%d num_toks:%d\n", pid, num_lines, num_toks);
      if (num_toks >= 5) {
         sscanf(tokens[0], "%llx-%llx", &addr_start, &addr_end);
         len = addr_end - addr_start;

         // send it to mte on cpu 0
         if (num_toks >= 6)
            name = std::string(tokens[5]);
         else if (num_toks == 5 && pmd->TraceAnonMTE) // properly handle anon segments according to setting
            name = std::string("//anon");
         else
            continue;
         rc = addModule(header, addr_start, len, name);
         // failed to write the hook for some unknown reason. buffer might be full.
         // Stop processing the maps file.
         if (rc != 0)
            break;
      }
      else {
         PI_PRINTK_ERR("Unrecognized number of tokens in maps file pid:%d line:%d num_toks:%d\n", pid, num_lines, num_toks);
         break;
      }
   }
   PDEBUG("Maps open pid:%d lines:%d\n", pid, num_lines);

   fclose(mapsf);
   return PU_SUCCESS;
}

}
}
