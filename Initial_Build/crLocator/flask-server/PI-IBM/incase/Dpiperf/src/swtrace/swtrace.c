/*
 * IBM Performance Inspector
 * Copyright (c) International Business Machines Corp.,  2002 - 2010
 *
 * This program is free software; you can redistribute it and/or
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
/*
 *  Description: Tool code to support tracing and above idle functions.
 *
 * Change History
 *
 *
 * Date      Description
 *----------------------------------------------------------------------------
 * 02/06/02  Add Header and Copyright Info
 * ...
 * 06/16/06  Added AboveIdle -t option(timestamp)
 * 07/19/06  Started performance counter/event support
 *           (ported from Windows and merged with the old code)
 * 09/14/06  Added SIGINT, SIGTERM handling for above idle termination
 * 09/26/06  Added more error checks, replaced usage() with PrintHelp()
 * 10/19/06  Removed copying of the original nrm2 file from append_kallsyms_and_modules();
 *           changed kallsyms size and offset fields to uint64
 * 10/23/06  Added init -t trace_mode option (norm, cont, wrap)
 * 01/26/07  Added init -f option for file name when cont tracing
 * 01/30/07  Added init -ss section_size, -sm mte_buffer_size
 * 04/13/07  Changed perfutil call names;
 *           print_driver_status() moved here from perfutil.c
 * 04/20/07  Moved top-level AboveIdle function here (ai()),
 *           now with the same output format and options as on Windows
 * 05/22/07  ITrace API calls updated to new interfaces;
 *           added -i, -c options for instruction count flag and skip_count
 * 07/03/07  Removed -x option for enable/disable
 * 10/28/08  Added -anon option to include/exclude anon-MTE data
 * 02/20/09  Added -intr option for itrace through the interrupts
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/utsname.h>

#include <errno.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <signal.h>

#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#include "perfutil.h"
#include "perfutilbase.h"
#include "perfmisc.h"

#ifndef stricmp
#define stricmp strcmp
#endif

#define RUN_FOREVER           0
#define RUN_SECONDS           1
#define RUN_MINUTES           2
#define RUN_HOURS             3
#define RUN_DAYS              4


#define SHORT_HELP   0
#define LONG_HELP    1

#define PI_1MB  0x100000
#define PI_1KB  1024

#define AI_PROCESSOR_ONLY         0
#define AI_SUMMARY_ONLY           1
#define AI_SUMMARY_AND_PROCESSOR  2

int    ai_interval        = 1;
int    ai_samples         = 0;
int    ai_runtime         = 0;
int    ai_summary         = AI_PROCESSOR_ONLY;
int    ai_timestamp       = 0;
FILE * ai_logfh           = NULL;
char * ai_logfn           = NULL;

int    OrgRunTime         = 0;
int    OrgRunTimeUnits    = RUN_SECONDS;
char * Units[5] = {"Forever", "Second", "Minute", "Hour", "Day"};

AI_COUNTERS  * curctrs, * prvctrs;


static int fn;                          /* file number */

int  tprof_event_num  = -1;
int  tprof_event_cnt  = 10000000;

void swtrace_off(void);
int  print_driver_status(void);
void ai(int sleep_sec, int num_samples);
void printf_so_log(char *format, ...);
void printf_so_log_time(void);

MAPPED_DATA  * pmd = NULL;    // Data mapped by device driver

//
// printf_se_exit()
// ****************
//
// Write messages to stderr, flush the stream and exit.
//
void printf_se_exit(char *format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stderr, format, argptr);
   fflush(stderr);
   va_end(argptr);

   exit (-1);
}

//
// AICleanUp()
// ***********
//
//  This routine receives control if ...
//  * the user enters ctrl-c at the keyboard to kill AI,
//  * AI is terminated by another process,
//  * the window where AI is running is closed,
//  * we detect an error and want to exit gracefully
//
//  The reason we need to get control is so that we can issue
//  a command to the kernel to disable KI measurements. Otherwise,
//  the kernel will be busy taking cpu measurements when it really
//  doesn't need to do so.
//
void AICleanUp(void)
{
    AiTerminate();
    fflush(stdout);
    fflush(stderr);
    exit(0);
}


//
// PrintHelp()
// ***********
//
void PrintHelp(int type)
{
   printf("\n SWTRACE Version: %x\n\n", TRACE_VERSION);

   if (type == SHORT_HELP) {
      printf("Valid SWTRACE commands:\n");
      printf(" \n");
      printf("  swtrace <[? | -? | help | -help | ayuda | -ayuda | syntax]>\n");
      printf("    * Display summary SWTRACE command syntax help.\n");
      printf("  swtrace <[?? | -?? | --help | --ayuda]>\n");
      printf("    * Display detailed SWTRACE command help.\n");
      printf("  swtrace init <-s trace_buffer_size> <-t trace_mode> <-f file_name> \n");
      printf("               <-sm mte_buffer_size> <-ss section_size> <-anon yes_no>\n");
      printf("    * Allocate/reallocate SWTRACE trace buffer(s). \n");
      printf("  swtrace free\n");
      printf("    * Deallocate SWTRACE trace buffer(s).\n");
      printf("  swtrace get <nrm2_filename>\n");
      printf("    * Get copy of SWTRACE trace buffer.\n");
      printf("  swtrace on\n");
      printf("    * Turn SWTRACE ON (start collecting hooks).\n");
      printf("  swtrace off\n");
      printf("    * Turn SWTRACE OFF (stop collecting hooks).\n");
      printf("  swtrace enable <M ...>\n");
      printf("    * Enable SWTRACE major codes.\n");
      printf("  swtrace disable <M ...>\n");
      printf("    * Disable SWTRACE major codes.\n");
      printf("  swtrace maj\n");
      printf("    * Display active SWTRACE major codes.\n");
      printf("  swtrace setrate N\n");
      printf("    * Set TPROF tick rate to approximately N hooks/second.\n");
      printf("  swtrace event [ctr_name | event_name | list] <-c event_count>\n");
      printf("    * Set TPROF event and event count.\n");
      printf("  swtrace info\n");
      printf("    * Display SWTRACE status/information.\n");
      printf("  swtrace ai <interval> <[num_samples | -r run_time]> <-t> <-s | -sp> <-l log_file>\n");
      printf("    * Display CPU utilization.\n");
      printf("  swtrace it_install <-c timer_skip_cnt> <-i> <-ss> <-intr>\n");
      printf("    * Enable ITrace.\n");
      printf("  swtrace it_remove\n");
      printf("    * Disable ITrace.\n");
      printf(" \n");
      printf("  - Arguments in \"<>\" (angle brackets) are OPTIONAL.\n");
      printf("  - Arguments in \"[]\" (square brackets) are REQUIRED.\n");
      printf("  - Arguments not preceded by an option keyword (\"-x\") are POSITIONAL.\n");
      printf(" \n");
      printf("  Enter \"swtrace -??\" for more detailed information.\n");
      printf(" \n");
      return;
   }

   printf("Syntax:\n");
   printf(" \n");
   printf("  STWRACE command <options>\n");
   printf(" \n");
   printf("Commands:\n");
   printf(" \n");
   printf("  [? | -? | help | -help | ayuda | -ayuda | syntax]\n");
   printf("    * Display summary SWTRACE command syntax help.\n");
   printf(" \n");
   printf("  [?? | -?? | --help | --ayuda]\n");
   printf("    * Display detailed SWTRACE command help.\n");
   printf(" \n");

   printf("  init <-s trace_buffer_size> <-t trace_mode> <-f file_name>\n");
   printf("       <-sm mte_buffer_size> <-ss section_size> <-anon yes_no>\n");

   printf("    * Allocate/reallocate SWTRACE trace buffers. \n");
   printf("    * '-s trace_buffer_size' specifies trace buffer size in MB, per processor. \n");
   printf("      Default trace buffer size: 3MB per processor.\n");
   printf("    * '-sm mte_buffer_size' specifies MTE buffer size in MB, per processor. \n");
   printf("      Default mte buffer size: 5MB per processor.\n");
   printf("    * '-t trace_mode specifies the mode of tracing. \n");
   printf("      trace_mode values:\n");
   printf("       - norm - normal mode: when a trace buffer is full, tracing stops \n");
   printf("       - wrap - wraparound mode: when a trace buffer is full,\n");
   printf("         new records are written from the buffer start \n");
   printf("       - cont - continuous mode: trace records are continuously written to a file\n");
   printf("      Default trace_mode is norm.\n");
   printf("    * '-f file_name' specifies file name for continuous mode.\n");
   printf("      This option is ignored with other modes.\n");
   printf("      Default name is swtrace.nrm2\n");
   printf("    * '-ss section_size' specifies the size of each section in KB written to the trace file \n");
   printf("       in continuous mode. \n");
   printf("      This option is ignored with other modes.\n");
   printf("      Default value is 64KB. \n");
   printf("    * '-anon yes_no' specifies whether the mapped segments without a file name will be recorded\n");
   printf("      Default value is 1(yes), 0 means no. \n");

   printf(" \n");

   printf("  free\n");
   printf("    * Deallocate SWTRACE trace buffer(s), disable all major codes,\n");
   printf("      and turn off tracing.\n");
   printf(" \n");

   printf("  get <nrm2_filename>\n");
   printf("    * Get copy of SWTRACE trace buffer.\n");
   printf("      - Default nrm2_filename: swtrace.nrm2\n");
   printf(" \n");

   printf("  on\n");
   printf("    * Turn SWTRACE ON (start collecting hooks).\n");
   printf(" \n");

   printf("  off\n");
   printf("    * Turn SWTRACE OFF (stop collecting hooks).\n");
   printf(" \n");

   printf("  enable <M ...>\n");
   printf("    * Enable SWTRACE major codes.\n");
   printf("      - M is a blank-delimited list of major codes and/or the word TPROF.\n");
   printf("      - Some often-used major codes are:\n");
   printf("          4: Interrupts\n");
   printf("          16: tprof (can also use the word 'tprof')\n");
   printf("          18: Dispatches\n");
   printf(" \n");

   printf("  disable <M ...>\n");
   printf("    * Disable SWTRACE major codes.\n");
   printf("      - M is a blank-delimited list of major codes in decimal or hexadecimal.\n");
   printf(" \n");
   printf("  maj\n");
   printf("    * Display active SWTRACE major codes.\n");
   printf("      - Major codes are displayed in decimal and hexadecimal.\n");
   printf(" \n");

   printf("  setrate N\n");
   printf("    * Set TPROF tick rate to approximately N hooks/second.\n");
   printf("    * Used when doing \"time-based\" profiling.\n");
   printf("    * Note: N will be rounded to CPU tick rate /(CPU tick rate / N) \n");
   printf(" \n");

   printf("  event [ctr_name | event_name | list] <-c event_count> \n");
   printf("    * Set TPROF event and event count.\n");
   printf("    * Used when doing \"event-based\" profiling.\n");
   printf("      - 'event_name' specifies the event to use to drive TPROF. To get a list\n");
   printf("        of supported names use the \"swtrace event list\" command.\n");
   printf("        Supported events vary by hardware architecture.\n");
   printf("        + SWTRACE takes care of setting up the hardware performance counters\n");
   printf("          for the selected event.\n");
   printf("      - 'ctr_name' specifies the performance counter to use to drive TPROF. To\n");
   printf("        get a list of supported names use the \"swtrace evet list\" command.\n");
   printf("        Supported counters vary by hardware architecture.\n");
   printf("        + If you specify a counter then you are responsible for setting it up\n");
   printf("          to count the desired event.\n");
   printf("        + You *CANNOT* specify a counter if HyperThreading is enabled.\n");
   printf("      - 'list' lists the available events and counters for the system.\n");
   printf("      - '-c event_cnt' specifies the number of events to be counted before a\n");
   printf("        TPROF interrupt occurs.\n");
   printf("        + If not specified the default count is 10,000,000 events.\n");
   printf("        + Too low a count may cause you not to get enough samples.\n");
   printf("        + Too high a count may cause you to flood the system with interrupts\n");
   printf("          thus distorting the scenario you are profiling.\n");
   printf(" \n");

   printf("  info\n");
   printf("    * Display SWTRACE status/information.\n");
   printf(" \n");

   printf("  ai <interval> <[num_samples | -r run_time]> <-t> <-s | -sp> <-l log_file>\n");
   printf("    * Display CPU utilization.\n");
   printf("      - 'interval' specifies how often CPU utilization is displayed.\n");
   printf("        + It is specified as a decimal number, >= 1, in seconds.\n");
   printf("        + Default is 1 (ie. display utilization once a second).\n");
   printf("      - 'num_samples' specifies the number of times CPU utilization is\n");
   printf("        displayed.\n");
   printf("        + It is specified as a decimal number, >= 1.\n");
   printf("        + Default is infinity (ie. display utilization forever).\n");
   printf("        + You cannot specify both 'num_samples' and 'run_time'.\n");
   printf("      - 'run_time' is the length of time for which to run AI.\n");
   printf("        + It is specified as a decimal number, >= 1, immediately\n");
   printf("          followed by the desired time unit suffix.\n");
   printf("        + If no unit is specified the default is Seconds.\n");
   printf("        + Valid unit suffixes are:\n");
   printf("            ' ' no suffix    (ex. -r 60   means: run for 60 seconds)\n");
   printf("            's' for Seconds  (ex. -r 60s  means: run for 60 seconds)\n");
   printf("            'm' for Minutes  (ex. -r 20m  means: run for 20 minutes)\n");
   printf("            'h' for Hours    (ex. -r 5h   means: run for 5 hours)\n");
   printf("            'd' for Days     (ex. -r 10d  means: run for 10 days)\n");
   printf("      - '-t' causes a timestamp to be appended to each line displayed.\n");
   printf("        + Default is to not append a timestamp.\n");
   printf("      - '-s' causes system-wide utilization information to be displayed.\n");
   printf("      - '-sp' causes both per-processor and system-wide information to be\n");
   printf("        displayed.\n");
   printf("        + Default is to display utilization per-processor.\n");
   printf("        + This option is ignored on non-SMP systems.\n");
   printf("      - 'log_file' is the name of a file to which AI will log its output.\n");
   printf("        + Output is always displayed (sent to stdout).\n");
   printf("        + This option works much the way \"ai | tee log_file\" would.\n");
   printf(" \n");

   printf("  it_install <-c timer_skip_cnt> <-i> <-ss> \n");
   printf("    * Enable ITrace.\n");
   printf("      - 'timer_skip_cnt' specifies the number of timer ticks to skip\n");
   printf("        once ITrace is turned on.\n");
   printf("        + If not specified, the default is 50. That means that once\n");
   printf("          tracing, the OS will be allowed to handle every 50th timer tick.\n");
   printf("          The other 49 will be handled (skipped) by ITrace.\n");
   printf("        + You should really not change this value unless you understand\n");
   printf("          the consequence.\n");
   printf("      - '-i' causes ITrace records to have number of instructions field.\n");
   printf("         + If not specified, ITrace records do not have that field\n");
   printf("      - '-ss' is a valid option only for PPC64.\n");
   printf("        + It enables single step tracing.\n");
   printf("        + If not specifed, ITrace does branch tracing.\n");
   printf("      - '-intr' enables tracing through interrupts on x86, x86_64\n");
   printf("        + If not specified, tracing is disabled within interrupts.\n");
   printf("        + Note that exceptions with a an error code (e.g., page fault) are never traced.\n");

   printf(" \n");

   printf("  it_remove\n");
   printf("    * Disable ITrace.\n");
   printf(" \n");

#if defined(CONFIG_X86) || defined(CONFIG_X86_64)
   printf("   ptt_int on | off\n");
   printf("    * Enable/disable counting irq time in ptt time *\n");
   printf("    * Default value is on.\n");
   printf(" \n");
#endif

   printf("  - Arguments in \"<>\" (angle brackets) are OPTIONAL.\n");
   printf("  - Arguments in \"[]\" (square brackets) are REQUIRED.\n");
   printf("  - Arguments not preceded by an option keyword (\"-x\") are POSITIONAL.\n");
   printf(" \n");
   return;
}

//
// append_kallsyms_and_modules()
// *****************************
//
void append_kallsyms_and_modules(char *fn)
{
   char *kas = "/proc/kallsyms";
   char *kmd = "/proc/modules";
   FILE *fh = NULL;
   char cmd[256];
   char trace_header[PERFHEADER_BUFFER_SIZE];
   uint64 *pzap;

   struct stat s;
   uint64 fn_size_org, fn_size_kas, fn_size_kmd;

   if (access(fn, F_OK | R_OK) != 0) {
      fprintf(stderr, "ERROR (swtrace): Can't access %s\n", fn);
      return;
   }

   if (access(kas, F_OK | R_OK) != 0) {
      fprintf(stderr, "ERROR (swtrace): Can't access %s\n", kas);
      return;
   }

   if (access(kmd, F_OK | R_OK) != 0) {
      fprintf(stderr, "ERROR (swtrace): Can't access %s\n", kmd);
      return;
   }

   if (stat(fn, &s) < 0) {
      fprintf(stderr, "ERROR (swtrace): Can't stat %s\n", fn);
      return;
   }

   fn_size_org     = s.st_size;
   printf("%s size = %lu original\n", fn, fn_size_org);
   fflush(stdout);

   strcpy(cmd, "cat ");
   strcat(cmd, kas);
   strcat(cmd, " >> ");
   strcat(cmd, fn);
   if (system(cmd) == -1) {
      fprintf(stderr, "ERROR (swtrace): system(cat kas) failed \n");
      return;
   }

   if (stat(fn, &s) < 0) {
      fprintf(stderr, "ERROR (swtrace): Can't stat %s after appending %s\n", fn, kas);
      return;
   }

   fn_size_kas     = s.st_size;
   printf("%s size = %lu after appending %s\n", fn, fn_size_kas, kas);
   fflush(stdout);

   strcpy(cmd, "cat ");
   strcat(cmd, kmd);
   strcat(cmd, " >> ");
   strcat(cmd, fn);
   if (system(cmd) == -1) {
      fprintf(stderr, "ERROR (swtrace): system(cat kmd) failed \n");
      return;
   }


   if (stat(fn, &s) < 0) {
      fprintf(stderr, "ERROR (swtrace): Can't stat %s after appending %s\n", fn, kmd);
      return;
   }

   fn_size_kmd = s.st_size;
   printf("%s size = %lu after appending %s\n", fn, fn_size_kmd, kmd);
   fflush(stdout);

   fh = fopen(fn, "r+");
   if (!fh) {
      fprintf(stderr, "ERROR (swtrace): Error opening %s for final update.\n", fn);
      return;
   }

   if (fread(trace_header, sizeof(char), PERFHEADER_BUFFER_SIZE, fh) != PERFHEADER_BUFFER_SIZE) {
      fprintf(stderr, "ERROR (swtrace): Error reading trace header\n");
      fclose(fh);
      return;
   }
   pzap = (uint64 *)&trace_header[offsetof(PERFHEADER, kallsyms_offset)];
   *pzap = fn_size_org;                 // offset to copy of kallsyms
   pzap++;
   *pzap = fn_size_kas - fn_size_org;   // size of kallsyms
   pzap++;
   *pzap = fn_size_kas;                 // offset to copy of modules
   pzap++;
   *pzap = fn_size_kmd - fn_size_kas;   // size of modules
   fseek(fh, 0, SEEK_SET);
   if (fwrite(trace_header, sizeof(char), PERFHEADER_BUFFER_SIZE, fh) != PERFHEADER_BUFFER_SIZE) {
      fprintf(stderr, "ERROR (swtrace): Error writing updated trace header\n");
   }
   fclose(fh);

   return;
}


//
// CheckRunTime()
// **************
//
// Returns: 0  if invalid
//          #  desired run time in seconds
//
int CheckRunTime(char * time_str)
{
   int val;
   char b, * pb, c, * pc;

   pc = time_str + strlen(time_str) - 1;    // pointer to last character
   c  = *pc;                                 // last character
   if (isdigit(c)) {
      val = atoi(time_str);                 // is a digit.  No units given
      OrgRunTime = val;
   }
   else {
      pb = pc - 1;                          // pointer to next-to-last character
      b = *pb;                              // next-to-last character
      if (!isdigit(b)) {
         printf_se("Invalid run time unit suffix. Must be s, h, m, or d.\n\n");
         return (0);
      }
      *pc = '\0';                           // zap units
      val = atoi(time_str);                 // value (without units)
      OrgRunTime = val;

      if (c == 's' || c == 'S')
         val = val * 1;
      else if (c == 'm' || c == 'M') {
         val = val * 60;
         OrgRunTimeUnits = RUN_MINUTES;
      }
      else if (c == 'h' || c == 'H') {
         val = val * 60 * 60;
         OrgRunTimeUnits = RUN_HOURS;
      }
      else if (c == 'd' || c == 'D') {
         val = val * 60 * 60 * 24;
         OrgRunTimeUnits = RUN_DAYS;
      }
      else {
         printf_se("Invalid run time unit suffix. Must be s, h, m, or d.\n\n");
         return (0);
      }
   }

   if (val == 0)
      printf_se("Invalid run time value. Must be numeric and non-zero.\n\n");

   return (val);
}


int start_worker_thread(pu_worker_thread_args *args)
{
   return pthread_create(args->thread_handle, 0, args->thread_start_func, 0);
}

//
//
void parse_command(int argc, char *argv[]) {
	char * raw_file_name, *out_file_name;
	uint32_t temp = 0;
	char default_raw_file_name[] = "swtrace.nrm2";
	char default_out_file_name[] = "swtrace.out";

	int rc = 0;

	int event_id, event_cnt, ctr_cnt;
	const char * * event_name_list = NULL;
	const char * const * ctr_name_list = NULL;

	int NumActiveCPUs;     // number of active CPUs

	int opt_start;
	int itrace_skip_count = ITRACE_DEFAULT_SKIP_COUNT;
	int itrace_flag;

	int i;
	unsigned char major_table[NUM_MAJORS];

	uint32_t trace_buff_size = PI_TRACE_BUF_DEFAULT;
	uint32_t trace_mode = COLLECTION_MODE_CONTINUOS;
	int last_parsed = 1;
	char full_name[PI_MAX_PATH];

	raw_file_name = default_raw_file_name;
	out_file_name = default_out_file_name;

	if ((strcmp(argv[1], "?") == 0) || (strcmp(argv[1], "-?") == 0)
	        || (strcmp(argv[1], "help") == 0) || (strcmp(argv[1], "-help") == 0)
	        || (strcmp(argv[1], "ayuda") == 0)
	        || (strcmp(argv[1], "-ayuda") == 0)
	        || (strcmp(argv[1], "syntax") == 0)) {
		PrintHelp(SHORT_HELP);
		return;
	}
	else if ((strcmp(argv[1], "??") == 0) || (strcmp(argv[1], "-??") == 0)
	        || (strcmp(argv[1], "--help") == 0)
	        || (strcmp(argv[1], "--ayuda") == 0)) {
		PrintHelp(LONG_HELP);
		return;
	}


	NumActiveCPUs = PiGetActiveProcessorCount();
	//
	// Not help - parse each possible command
	//
	if (strcmp(argv[1], "init") == 0) {

		// set raw_file_name to the default value
		if (NULL == getcwd((char *) &full_name,
		PI_MAX_PATH - strlen((char *) &default_raw_file_name) - 1)) {
			fprintf(stderr, "default file name is too long\n");
			exit(1);
		}

		strcat((char *) &full_name, "/");
		;
		strcat((char *) &full_name, (char *) &default_raw_file_name);
		raw_file_name = (char *) &full_name;

		// set TraceAnonMTE to yes - the default value
		SetTraceAnonMTE(1);

		++last_parsed;
		while (last_parsed < argc) {
			if (strcasecmp(argv[last_parsed], "-s") == 0) {

				++last_parsed;
				trace_buff_size = PI_1MB * atoi(argv[last_parsed]);
				++last_parsed;
			}
			else if (strcasecmp(argv[last_parsed], "-sm") == 0) {

				++last_parsed;
				temp = PI_1MB * atoi(argv[last_parsed]);
				TraceSetMteSize(temp);
				++last_parsed;
			}
			else if (strcasecmp(argv[last_parsed], "-ss") == 0) {

				++last_parsed;
				temp = PI_1KB * atoi(argv[last_parsed]);
				TraceSetSectionSize(temp);
				++last_parsed;
			}
			else if (strcasecmp(argv[last_parsed], "-anon") == 0) {

				++last_parsed;
				SetTraceAnonMTE(atoi(argv[last_parsed]));
				++last_parsed;
			}
			else if (strcasecmp(argv[last_parsed], "-t") == 0) {

				++last_parsed;

				if (strcasecmp(argv[last_parsed], "norm") == 0) {
					trace_mode = COLLECTION_MODE_NORMAL;
				}
				else if (strcasecmp(argv[last_parsed], "wrap") == 0) {
					trace_mode = COLLECTION_MODE_WRAP;
				}
				else if (strcasecmp(argv[last_parsed], "cont") == 0) {
					trace_mode = COLLECTION_MODE_CONTINUOS;
				}
				else
					printf_se_exit("%s is not an allowed trace mode\n",
					        argv[last_parsed]);

				++last_parsed;
			}
			else if (strcasecmp(argv[last_parsed], "-f") == 0) {

				++last_parsed;
				if (argv[last_parsed]) {
					sscanf(argv[last_parsed], "%s", raw_file_name);
				}
				else
					printf_se_exit("No filename with init -n \n");

				++last_parsed;
			}
			else
				printf_se_exit("Unknown parameters %s\n", argv[last_parsed]);

		}

		rc = TraceSetMode(trace_mode);
		if (rc != PU_SUCCESS) {
			fprintf(stderr, "TraceSetMode returned with error.\n");
			fprintf(stderr,
			        "Look at the system log for additional information.\n");
			exit(1);
		}
		if (IsContMode() || IsWrapMode()) {
			printf("Writing trace data to %s \n", raw_file_name);
			TraceSetName(raw_file_name);
		}

      rc = init_pu_worker(start_worker_thread, NULL);
      if (rc != 0) {
         fprintf(stderr, "Failed to start worker thread (.\n");
         exit(1);
      }

		rc = TraceInit(trace_buff_size);

		if (rc != PU_SUCCESS) {
			printf("err rc=%d",rc);
			fprintf(stderr,
			        "Error returned from initializing trace buffer. \n");
			fprintf(stderr,
			        "Not enough memory - try with smaller trace or MTE buffer sizes.\n");
			fprintf(stderr,
			        "Look at the system log for additional information.\n");
			exit(1);
		}
		return;

		if (NumActiveCPUs > 1)
			printf("%d trace buffers, %u bytes each\n", NumActiveCPUs,
			        pmd->TraceBufferSize);
		else
			printf("1 trace buffer allocated %u bytes long\n",
			        pmd->TraceBufferSize);

		//fflush(stdout);
		return;
	}
	else if (strcmp(argv[1], "free") == 0) {

		TraceTerminate();
		printf("Trace Buffer freed.\n");
	}
	else if (strcmp(argv[1], "on") == 0) {

		printf("Turning PI Trace ON...\n");
		rc = TraceOn();
		if (rc != 0) {
			fprintf(stderr, "ERROR (swtrace): Failed to turn on tracing \n");

			if (PU_ERROR_INSTALLING_REQ_HOOKS == rc) {
				fprintf(stderr, "OProfile not installed!!!\n");
				// turn tracing off
				swtrace_off();
			}
			else
				fprintf(stderr, "Is the trace device driver, pidd, active?\n");

			fprintf(stderr,
			        "Look at the system log for additional information.\n");
			exit(1);
		}
	}
	else if (strcmp(argv[1], "off") == 0) {

		swtrace_off();
	}
	else if (strcmp(argv[1], "enable") == 0) {

		if (argc == 2) {
			printf("Enabling ALL Major codes ...\n");
			rc = TraceEnable(0);
			if (rc != 0) {
				fprintf(stderr,
				        "ERROR (swtrace): Failed to enable major codes \n");
				fprintf(stderr,
				        "                 Is the drive driver active?\n");
				exit(1);
			}

		}
		else {

			last_parsed = 2;

			if (strcmp(argv[last_parsed], "tprof") == 0) { // Enable tprof hooks
				printf("Enabling tprof major event codes\n");
				rc = TraceEnable(TPROF_HOOKS);
				if (rc != 0)
					printf_se_exit("Error enabling tprof hooks\n");
			}
			else {
				printf("Enabling the following Major codes ...");
				for (last_parsed; last_parsed < argc; last_parsed++) {
					i = (int) strtol(argv[last_parsed], NULL, 0);

					if (i != 0) {
						rc = TraceEnable(i);
						if (rc != 0) {
							fprintf(stderr,
							        "ERROR (swtrace): Failed to enable major code \n");
							fprintf(stderr,
							        "                 Is the drive driver active ?\n");
							exit(1);
						}
					}
					printf(" %d ", i);
				}
				printf("\n");
			}
		}
	}
	else if (strcmp(argv[1], "disable") == 0) {

		if (argc == 2) {
			printf("Disabling ALL Major codes ...\n");
			rc = TraceDisable(0);
			if (rc != 0) {
				fprintf(stderr,
				        "ERROR (swtrace): Failed to disable major codes \n");
				fprintf(stderr,
				        "                 Is the trace device driver, pidd, active?\n");
				exit(1);
			}
		}
		else {

			printf("Disabling the following Major codes ...");
			last_parsed = 2;

			for (last_parsed; last_parsed < argc; last_parsed++) {
				i = (int) strtol(argv[last_parsed], NULL, 0);

				if (i != 0) {
					rc = TraceDisable(i);
					if (rc != 0) {
						printf(
						        "ERROR (swtrace): Failed to disable major codes \n");
						printf(
						        "                 Is the trace device driver, pidd, active?\n");
						exit(1);
					}
				}
				printf(" %d ", i);
			}
			printf("\n");
		}
	}
	else if (strcmp(argv[1], "maj") == 0) {

		/*

		 if (TraceQueryMEC(&major_table[0], NUM_MAJORS) != 0)
		 printf_se_exit("ERROR (swtrace): Failed to get the table of enabled major codes.\n");
		 */

		printf("\nEnabled Major Trace Hooks\n");
		printf("\n Decimal Hexadecimal\n");
		for (i = 0; i <= LARGEST_HOOK_MAJOR; i++) {
			if (major_table[i] != 0)
				printf("  %3d      %3x\n", i, i);
		}

	}
	else if (strcmp(argv[1], "ai") == 0) {
		//
		// AI [sample_interval [num_samples | -r run_time]] [-t] [-s | -sp] [-l filename]
		//
		// - Can't have num_samples and -r together
		// - sample_interval (if specified) must be first
		// - num_samples (if specified) must be second
		//
		//

		opt_start = 0;                        // Assume no options

		if (argc > 9) {
			printf_se("Invalid command line options for AI.\n");
			printf_se_exit("Enter \"swtrace\" for command help.\n");
		}

		if (argc == 2) {                      // No arguments. Take defaults
			// swtrace ai
			ai_interval = 1;
			ai_samples = INT32_MAX;
		}
		else {                                // Some arguments. Figure it out
			if (argv[2][0] == '-') {
				// Starts with options: can't have sample_interval nor num_samples
				opt_start = 2;
			}
			else {
				// Starts with number: must be sample_interval
				ai_interval = atoi(argv[2]);       // Convert to number
				if (ai_interval <= 0)
					printf_se_exit("Sample interval must be >= 1.\n");

				if (argc > 3) {
					// Can have either num_samples or other options
					if (argv[3][0] == '-') {
						// It's other options ...
						opt_start = 3;
					}
					else {
						// It's a number so must be num_samples
						ai_samples = atoi(argv[3]);  // Convert to number
						if (ai_samples <= 0)
							printf_se_exit("Number of samples must be >= 1.\n");

						if (argc > 4) {
							// There must be other options ...
							opt_start = 4;
						}
					}
				}
				else {
					// Only sample_interval. Default on num_samples
					ai_samples = INT32_MAX;
				}
			}
		}

		//
		// Parse the rest of the AI command line, if any
		//
		if (opt_start != 0) {
			for (i = opt_start; i < argc; i++) {
				if (stricmp(argv[i], "-r") == 0) {
					if (++i == argc)
						printf_se_exit("-r option requires run time value.\n");
					ai_runtime = CheckRunTime(argv[i]);
					if (ai_runtime == 0)
						exit(-1);
				}
				else if (stricmp(argv[i], "-t") == 0) {
					ai_timestamp = 1;
				}
				else if (stricmp(argv[i], "-s") == 0) {
					if (ai_summary == AI_SUMMARY_AND_PROCESSOR)
						printf_se_exit("-s and -sp not allowed together.\n");
					ai_summary = AI_SUMMARY_ONLY;
				}
				else if (stricmp(argv[i], "-sp") == 0) {
					if (ai_summary == AI_SUMMARY_ONLY)
						printf_se_exit("-sp and -s not allowed together.\n");
					ai_summary = AI_SUMMARY_AND_PROCESSOR;
				}
				else if (stricmp(argv[i], "-l") == 0) {
					if (++i == argc)
						printf_se_exit("-l option requires a filename.\n");
					if (argv[i][0] == '-')
						printf_se_exit(
						        "'%s' not valid filename: filenames can't start with '-'.\n",
						        argv[i]);

					ai_logfn = argv[i];
					remove(ai_logfn);
					ai_logfh = fopen(ai_logfn, "w");
					if (ai_logfh == NULL) {
						printf_se(
						        "Unable to open '%s'. Logging will be turned off.\n",
						        ai_logfn);
						ai_logfn = NULL;
					}
				}
				else {
					printf_se_exit("Invalid command line options for AI: %s\n",
					        argv[i]);
				}
			}

			//
			// Check num_samples and -r combinations:
			//   -r  num_samples   action
			//    0      0         ai_samples = forever
			//    1      0         ai_samples = calculate from ai_runtime
			//    0      1         ai_samples = already set
			//    1      1         error
			//
			if (ai_runtime != 0 && ai_samples != 0)  // (1 1) -r and num_samples
				printf_se_exit("num_samples and -r not allowed together.\n");
			else if (ai_runtime == 0 && ai_samples == 0) // (0 0) No -r and no num_samples
				ai_samples = INT32_MAX;
			else if (ai_runtime != 0)             // (1 0) -r and no num_samples
				ai_samples = ai_runtime / ai_interval;

			// Disable summary if UNI
			NumActiveCPUs = PiGetActiveProcessorCount();
			if (NumActiveCPUs == 1 && ai_summary == AI_SUMMARY_AND_PROCESSOR) {
				printf_so(
				        "Summary option (-sp) ignored on non-SMP machines.\n");
				ai_summary = AI_PROCESSOR_ONLY;
			}
		}

		// Call ai(), all parameters parsed & set now

		ai(ai_interval, ai_samples);

	}
	else if (strcmp(argv[1], "get") == 0) {

		if (argv[2]) {
			sscanf(argv[2], "%s", raw_file_name);
			printf("Writing trace data to %s \n", raw_file_name);
		}

		if (IsContMode() || IsWrapMode()) {
			raw_file_name = TraceGetName();
			if (raw_file_name == NULL)
				printf_se_exit("Error swtrace get - raw_file_name is NULL \n");
		}

		//if (TraceWriteBufferToFile(raw_file_name) != 0)
		//    printf_se_exit("Error swtrace get\n");

		append_kallsyms_and_modules(raw_file_name);

	}
	else if (strcmp(argv[1], "info") == 0) {
		print_driver_status();
	}
	else if (strcmp(argv[1], "setrate") == 0) {

		if (argc > 2)
			temp = atoi(argv[2]);

		if (temp == 0)
			printf("Setting default TPROF tick rate.\n");
		else
			printf("Setting TPROF tick rate to %d.\n", temp);

		if (SetProfilerRate((UINT32) temp) != PU_SUCCESS) {
			printf_se_exit("Error while setting tick rate\n");
		}
		else {
			printf_so(
			        "Actual TPROF tick rate is ~%d hooks/sec (ticks every ~%0.1f uSec)\n",
			        pmd->CurrentTprofRate, (1000000.0 / pmd->CurrentTprofRate));
		}
	}
	else if (strcasecmp(argv[1], "event") == 0) {
		// event [ctr_name | event_name | list] <-c event_count>

		if (argc < 3)
			printf_se_exit("Command requires an event or counter name.\n");

		if (argc > 5)
			printf_se_exit("Invalid command line options for EVENT.\n");

		if (strcasecmp(argv[2], "list") == 0) {
			event_cnt = GetSupportedPerfCounterEventNameList(&event_name_list);
			if (event_cnt == 0) {
				fprintf(stderr,
				       "It seems there are no events supported on this processor family. Time based only.\n");
				return;
			}
			printf("  Valid TPROF events on this machine:\n");

			printf("  - Performance Counter Events:\n");
			for (i = 0; i < event_cnt; i++) {
				printf("      %s\n", event_name_list[i]);
			}
			return;
		}

		event_id = GetPerfCounterEventIdFromName(argv[2]);
		if (event_id != EVENT_INVALID_ID) {
			// Valid counter event
			tprof_event_num = event_id;
		}
		else
			printf_se_exit(
			        "Invalid event name. Use \"SWTRACE EVENT LIST\" for the list of valid event names.\n");

		// Have valid event/counter. Now look for optional arguments
		// Note: we currently support only one optional argument, -c
		//
		if (argc > 3) {
			// -c event_count
			if (strcasecmp(argv[3], "-c") != 0)
				printf_se_exit("\n%s is not a valid option for EVENT.\n",
				        argv[3]);

			if (argc < 5)
				printf_se_exit("Not enough arguments - missing event count.\n");

			tprof_event_cnt = strtoul(argv[4], NULL, 0);
			if (tprof_event_cnt <= 0)
				printf_se_exit("\n%s not a valid event count.\n", argv[4]);
		}

		rc = TraceSetTprofEvent(ALL_CPUS, tprof_event_num, tprof_event_cnt,
              0);
		if (rc == 0)
			printf(
			        "Command completed successfully. Requested TPROF event set.\n");
		else if (rc == PU_ERROR_NOT_APIC_SYSTEM)
			printf_se_exit(
			        "Command failed: Events are only supported on APIC systems.\n");
		else if (rc == PU_ERROR_INVALID_METRIC)
			printf_se_exit("Command failed: Invalid or non-supported event.\n");
		else if (rc == PU_ERROR_STARTING_EVENT)
			printf_se_exit(
			        "Command failed: Unable to start requested event.\n");
		else
			printf_se_exit(
			        "Command failed: TraceSetTprofEvent() error. rc = %d (0x%X)\n",
			        rc, rc);
	}
	else if ((strcmp(argv[1], "it_install") == 0)
	        || (strcmp(argv[1], "itinit") == 0)) {

		//
		// it_install [-i] [-c skip_count] [-ss] [-intr]
		//

		if (argc > 7)
			printf_se_exit("Invalid command line options for it_install.\n");

		itrace_flag = 0;

		for (i = 2; i < argc; i++) {
			if ((strcmp(argv[i], "-i") == 0) || (strcmp(argv[i], "/i") == 0)) { // Count instructions
				itrace_flag |= PI_ITRACE_FLAG_INST_COUNT;
			}
			else if ((strcmp(argv[i], "-c") == 0)
			        || (strcmp(argv[i], "/c") == 0)) { // Skip count
				if (++i < argc) {
					itrace_skip_count = (int) strtol(argv[i], NULL, 0);
				}
				else
					printf_se_exit("-c option requires a skip_count.\n");
			}
			else if (strcmp(argv[i], "-ss") == 0) {
				if (!IsProcessorPPC64()) {
					printf_se_exit(
					        "Single-step ITrace is supported only on PPC64\n");
				}
				itrace_flag |= PI_ITRACE_FLAG_SS;
			}
			else if ((strcmp(argv[i], "-intr") == 0)
			        || (strcmp(argv[i], "/i") == 0)) {     // Count instructions
				itrace_flag |= PI_ITRACE_FLAG_TRACE_INTERRUPTS;
			}
			else
				printf_se_exit(
				        "Invalid command line options for it_install.\n");
		}

		ITraceSetSkipCount(itrace_skip_count);
		rc = ITraceInit(itrace_flag);

		if (rc != 0) {
			fprintf(stderr, "ERROR (itrace): Failed to install itrace \n");
			fprintf(stderr,
			        "                Is the tracing initialized and the device driver, pidd, active?\n");
			exit(1);
		}
		else {
			printf("Instruction tracing turned on.\n");
		}
	}

	else if (strcmp(argv[1], "it_remove") == 0) {

		rc = ITraceTerminate();
		switch (rc) {
		case 1:
			fprintf(stderr, "WARNING: Trace was already off\n");
			fprintf(stderr,
			        "         Could indicate a buffer full condition\n");
			exit(0);
		case 0:
			printf("Instruction tracing turned off.\n");
			break;
		default:
			fprintf(stderr,
			        "ERROR (swtrace): Failed to turn off tracing, rc = %d \n",
			        rc);
			fprintf(stderr,
			        "                 Is the trace device driver, pidd, active?\n");
			exit(1);
		}

	}
	else if (strcmp(argv[1], "quit") == 0) {
		if (pmd->TraceOn) {
			TraceOff();
		}
		exit(0);
	}
	else {
		/* if you get here, it is not a valid command */
		fprintf(stderr, "ERROR invalid command: %s\n", argv[1]);
		PrintHelp(SHORT_HELP);
		return;

	}

}

#define MAX_ARGS 32
#define CMD_LEN 1024

//
// main()
// ******
//
int main(int argc, char *argv[]) {
	int i;
	pmd = GetMappedDataAddress();            // Get ptr to DD mapped data

	//PrintHelp(SHORT_HELP);
	fprintf(stderr, "swtrace starting \n");

	FILE *infd = stdin;
	char *args[MAX_ARGS];
	int num_args = argc;
	args[0] = argv[0];
	if (num_args > MAX_ARGS)
		num_args = MAX_ARGS;

	if (argc > 1) {
		for (i = 1; i < num_args; i++) {
			args[i] = argv[i];
		}
		//parse_command(num_args, args);
	}

	// FIXME
	if (argc == 2 && strcmp(argv[1], "test") == 0) {
		// getting tired of entering them everytime. remove this block
		args[1] = "init";
		args[2] = "-f";
		args[3] = "swtrace.nrm2";
		num_args = 4;
		parse_command(num_args, args);
		args[1] = "enable";
		args[2] = "16";
		num_args = 3;
		parse_command(num_args, args);
		args[1] = "setrate";
		args[2] = "1000";
		num_args = 3;
		parse_command(num_args, args);
		args[1] = "on";
		num_args = 2;
		//parse_command(num_args, args);
	}
	else if (argc == 3 && strcmp(argv[1], "-fifo") == 0) {
		infd = fopen(argv[2], "r");
		if (!infd) {
			perror("Could not open pipe file!");
			exit(-1);
		}
	}

	char cmds[CMD_LEN];
	while (fgets(cmds, CMD_LEN, infd) != NULL) {
		if (strlen(cmds) <= 0) {
			continue;
		}
		i = num_args = 1;
		args[1] = strtok(cmds, " \t\n");
		if (!args[i]) {
			continue;
		}
		while (args[i]) {
			if (strlen(args[i])) {
				i++;
				num_args++;
			}
			args[i] = strtok(0, " \t\n");
		}
		parse_command(num_args, args);
		fflush(stdout);
	}
	fprintf(stderr, "no more input \n");

	return 0;
}

//-----------------------------------------------------------
// turn trace off - called by user or when swtrace on failed
//
void swtrace_off(void)
{
   int rc;

   printf("Turning PI Trace OFF...\n");

   rc = TraceOff();
   switch (rc) {
   case PU_ERROR_BUFFER_FULL:
      fprintf(stderr, "WARNING: Trace was already off\n");
      fprintf(stderr, "Could indicate a buffer full condition\n");
      break;
   case 0:
      break;
   default:
      fprintf(stderr, "ERROR (swtrace): Failed to turn off tracing \n");
      exit(1);
   }

}

//
// print_driver_status()
// ********************
//
int print_driver_status(void)
{
   printf_so("\nDriver status:\n");

   //if (!IsDeviceDriverInstalled())
    //  return(PU_FAILURE);

   //printf_so(" PIDD Version  0x%08X\n", GetDDVersionEx());
   printf_so(" CPU Signature 0x%08X\n", pmd->CpuSignature);
   printf_so(" CPU Family    %d\n", GetProcessorType());
   printf_so(" CPU Model No. %d\n", pmd->CpuModel);
   printf_so(" CPU Architecture: %s\n", GetProcessorArchitectureString());


   printf_so(" CPU Speed:    %.2f MHz\n",  (float)GetProcessorSpeed()/1000/1000);
   printf_so(" No. of CPUs:  %d\n", PiGetActiveProcessorCount());
   printf_so(" Buffer Size:  %d\n", pmd->TraceBufferSize);
   printf_so(" Tprof rate:   %d\n", pmd->CurrentTprofRate);

   if ( IsHyperThreadingEnabled() )
      printf_so(" HyperThreading Active\n");

   if ( IsTraceActive() )
      printf_so(" Tracing           active.\n");
   else
      printf_so(" Tracing           off.\n");

   if ( IsAiActive() )
      printf_so(" Above idle        active.\n");
   else
      printf_so(" Above idle        off.\n");

   return(PU_SUCCESS);
}


//
// ai()
// ****
//
void ai(int sleep_sec, int num_samples)
{
   int        num_cpus;
   int        rc, i;
   int        iter;
   double     ts_delta, cpu_idle[MAX_CPUS], cpu_busy[MAX_CPUS], cpu_intr[MAX_CPUS];
   double     sys_idle, sys_busy, sys_intr;

   AI_COUNTERS * tp;                         // Previous and temp copy of double counters

   //
   // Check AI parameters
   //
   if (sleep_sec <= 0 || num_samples <= 0) {
      printf_se("Invalid aguments to ai().\n");
      return;
   }

   //
   // Get number of processors so we can handle SMP boxes
   //
   num_cpus = PiGetActiveProcessorCount();

   //
   // Display arguments
   //
   printf_so_log("**        Processors: %d\n", num_cpus);
   if (ai_logfh != NULL) {
      printf_so_log("**      Log filename: %s\n", ai_logfn);
   }
   printf_so_log("**   Sample interval: %d Second(s)\n", sleep_sec);
   if (ai_samples == INT32_MAX)
      printf_so_log("** Number of samples: infinite (until stopped via Ctrl-c)\n");
   else {
      printf_so_log("** Number of samples: %d", num_samples);
      if (ai_runtime == 0)
         printf_so_log("\n");
      else {
         if (OrgRunTime == 1)
            printf_so_log(" (Run time: %d %s)\n", OrgRunTime, Units[OrgRunTimeUnits]);
         else
            printf_so_log(" (Run time: %d %ss)\n", OrgRunTime, Units[OrgRunTimeUnits]);
      }
   }

   //
   // Allocate buffers for the KI counters
   //
   curctrs = AiAllocCounters(NULL);
   prvctrs = AiAllocCounters(NULL);
   if (curctrs == NULL || prvctrs == NULL) {
      printf_se("Unable to allocate storage for CPU measurement counters.\n");
      AICleanUp();
   }

   //
   // Must do AiInit().
   //
   rc = AiInit();
   if (rc) {
      printf_se("AiInit() failed. rc=%d.\n",rc);
      AICleanUp();
   }

   //
   // Install signal handler for ctrl-c and abnormal process
   // termination (closing the window or killed by another process).
   //
   signal(SIGINT, (void(*)(int))AICleanUp );   // Ctrl+C handler
   signal(SIGQUIT, (void(*)(int))AICleanUp );  // Ctrl+Break handler
   signal(SIGTERM, (void(*)(int))AICleanUp);   // TerminateProcess

   //
   // Tell'em when we started.
   //

   printf_so_log("** Data collection started on ");
   printf_so_log_time();
   printf_so_log("\n");

   //
   // Loop and report ...
   //
   iter = 0;                                // Haven't done any yet

   do {
      //
      // Read counters
      //
      rc = AiGetCounters(curctrs);
      if (rc) {
         printf_se("AiGetCounters() failed. rc=0x%X\n",rc);
         AICleanUp();
      }

      //
      // Display information ... but skip the first time through because we don't
      // have prior measurements for doing the percentages yet.
      //
      if (iter > 0) {
         num_samples--;

         if (ai_summary) {
            sys_idle = sys_busy = sys_intr = 0.0;
         }

         //
         // Calculate utilization percentages
         //
         for (i = 0; i < num_cpus; i++) {
            ts_delta    = (double)((INT64)(curctrs[i].CurTime - prvctrs[i].CurTime));
            cpu_idle[i] = (double)((INT64)(curctrs[i].IdleTime - prvctrs[i].IdleTime)) / ts_delta * 100.0;
            cpu_busy[i] = (double)((INT64)(curctrs[i].BusyTime - prvctrs[i].BusyTime)) / ts_delta * 100.0;
            cpu_intr[i] = (double)((INT64)(curctrs[i].IntrTime - prvctrs[i].IntrTime)) / ts_delta * 100.0;

            if (ai_summary) {
               // summary - just sum up the percentages
               sys_idle += cpu_idle[i];
               sys_busy += cpu_busy[i];
               sys_intr += cpu_intr[i];
            }
         }

         //
         // Every 20 lines print a header (SMP only and not doing summary only)
         //
         if ((num_cpus > 1) && ((iter % 20) == 1) && (ai_summary != AI_SUMMARY_ONLY)) {
            if (ai_summary == AI_SUMMARY_AND_PROCESSOR)
               printf_so_log("System Summary      ");

            for (i = 0; i < num_cpus-1; i++) {
               printf_so_log("processor #%d        ", i);
            }
            printf_so_log("processor #%d\n", i);         // No trailing blanks for last processor

            if (ai_summary == AI_SUMMARY_AND_PROCESSOR)
               printf_so_log("(idle%%,busy%%,intr%%) ");

            for (i = 0; i < num_cpus-1; i++) {
               printf_so_log("(idle%%,busy%%,intr%%) ");
            }
            printf_so_log("(idle%%,busy%%,intr%%)\n");   // No trailing blank for last processor
         }

         //
         // Display the data
         //
         if (num_cpus == 1) {
            // UNI - display requested form
            if (ai_summary == AI_SUMMARY_ONLY) {
               printf_so_log("System idle: %5.2f%%  busy: %5.2f%%  intr: %5.2f%%",
                             cpu_idle[0], cpu_busy[0], cpu_intr[0]);
            }
            else {
               printf_so_log("idle: %5.2f%%  busy: %5.2f%%  intr: %5.2f%%",
                             cpu_idle[0], cpu_busy[0], cpu_intr[0]);
            }

            if (ai_timestamp) {
               printf_so_log_time();
            }
            else {
               printf_so_log("\n");
            }

            goto FinishUpInterval;
         }

         if (ai_summary == AI_SUMMARY_ONLY) {
            // SMP and System summary only
            printf_so_log("System idle: %5.2f%%  busy: %5.2f%%  intr: %5.2f%%",
                         (sys_idle / num_cpus), (sys_busy / num_cpus), (sys_intr / num_cpus));

            if (ai_timestamp) {
               printf_so_log_time();
            }
            else {
               printf_so_log("\n");
            }

            goto FinishUpInterval;
         }

         // SMP and processor-only or summary and processors
         for (i = 0; i < num_cpus; i++) {
            if (ai_summary == AI_PROCESSOR_ONLY) {
               // Per-processor only
               printf_so_log("(%5.2f,%5.2f,%5.2f)", cpu_idle[i], cpu_busy[i], cpu_intr[i]);
               if (i < num_cpus-1)
                  printf_so_log(" ");       // Don't do this for last processor.
                                            // Trying to squeeze 4 in one line.
            }
            else {
               // System summary and per-processor
               if (i == 0) {
                  printf_so_log("(%5.2f,%5.2f,%5.2f)", (sys_idle / num_cpus),
                                (sys_busy / num_cpus), (sys_intr / num_cpus));
               }
               printf_so_log(" (%5.2f,%5.2f,%5.2f)", cpu_idle[i], cpu_busy[i], cpu_intr[i]);
            }
         }

         if (ai_timestamp) {
            printf_so_log_time();
         }
         else {
            printf_so_log("\n");
         }

         goto FinishUpInterval;
      }  // end - if iter > 0

FinishUpInterval:
      //
      // Switch curctrs and prvctrs.
      // Basically all this does is allows us to flip-flop using
      // the 2 buffers, thus we don't need to actually copy data
      // from current to previous.
      //
      tp      = prvctrs;
      prvctrs = curctrs;
      curctrs = tp;

      //
      // Time to stop?
      //
      iter++;
      if (num_samples == 0)
         break;

      //
      // Sleep for the specified time period.
      //
      sleep(sleep_sec);
   } while (1);

   //
   // If we fall out then we've done the requested number of iterations.
   //
   printf_so_log("\n** Requested number of samples reached.  Ending.\n");
   AICleanUp();
   return;
}

//
// Prints current date and time
// ****************************
//
void printf_so_log_time(void)
{
   struct timeval tv;
   struct timezone tz;
   time_t now;

   gettimeofday(&tv, &tz);
   now = tv.tv_sec;
   printf(" %s\n", ctime(&now));
   fflush(stdout);
   if (ai_logfh != NULL) {
      fprintf(ai_logfh, " %s\n", ctime(&now));
      fflush(ai_logfh);
   }
   return;
}

//
// printf_so_log()
// ***************
//
// Write messages to stdout and log them in log file
//
void printf_so_log(char *format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stdout, format, argptr);
   fflush(stdout);
   if (ai_logfh != NULL) {
      vfprintf(ai_logfh, format, argptr);
      fflush(ai_logfh);
   }
   va_end(argptr);
   return;
}
