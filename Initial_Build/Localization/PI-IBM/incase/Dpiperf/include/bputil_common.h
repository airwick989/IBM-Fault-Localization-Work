/*
 * IBM Performance Inspector
 * Copyright (c) International Business Machines Corp., 1996 - 2008
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

#ifndef _BPUTIL_COMMON_H_
#define _BPUTIL_COMMON_H_

   #define TRACE_RECORD_HEADER  16  // 12 bytes header for each record: TY-LEN-MAJ:MIN:TS

enum pi_trace_type {
   MTE_TYPE             = 1,
   TRACE_TYPE           = 2,
   MTE_FINAL_TYPE       = 4
};
   #define BP_OK     0
   #define BP_ERROR -1

#define TRACE_BUFFER_FULL   0x00000001   /* Trace buffer full condition */

#define MIN_WR_BLOCK_SIZE	512
/**********************************************************************************/
#ifndef STAP_ITRACE // DEFS
//
// Trace File Header
// *****************
//
// One of these is pre-pended to each CPU's trace buffer when
// being written to file. Can use up to 512 bytes.
// * First 256 bytes are common to Windows and Linux
// * Second 256 bytes are OS-specific
//
typedef struct _PerfHeader
{
   UCHAR   nrm2[4];                         //   0: "NRM2"
   UINT32  endian;                          //   4: 0x040302010
   UCHAR   cr;                              //   8: Carriage return
   UCHAR   pad[3];                          //   9: Padding
   UINT32  header_size;                     //  12: Size of PerfHeader - set to 512
   UINT32  data_size;                       //  16: Size of data in buffer (that follows header)
   UINT32  buffer_type;                     //  20: ***** Not used and set to zero *****
   UINT32  cpu_no;                          //  24: CPU number to which this buffer belongs
   UINT32  no_ts_hooks;                     //  28: 1=Hooks of type 01 have no timestamp
   UINT32  collection_mode;                 //  32: NORMAL, WRAP, CONTINUOS, MTE_END
   UINT64  start_time;                  //  36: Time of last Trace on
   UINT64  time;                    		//  44: Time of last Trace off
   UINT32  Metric_cnt;                      //  52: Number of metrics
   UINT32  flag;                            //  56: 1=At least one trace buffer full
   UINT32  num_cpus;                        //  60: Number of CPUs
   UINT32  uncompressed_data_size;          //  64: Size of uncompressed data
   UINT32  OS_type;                         //  68: Whether 64 or 32 bit OS
   UINT32  inston;                          //  72: Instruction count on/off in itrace
   UINT32  interval_id;                     //  76: interval id at last trace on or zero
   UINT64  kallsyms_offset;                 //  80: (Linux-only) Offset to kallsyms file data
   UINT64  kallsyms_size;                   //  88: (Linux-only) Size of kallsyms file data
   UINT64  modules_offset;                  //  96: (Linux-only) Offset to modules file data
   UINT64  modules_size;                    // 104: (Linux-only) Size of modules file data
   UINT32  datalosscount;                   // 112: (Linux-only) lost hooks when continous tracing
   UINT32  section_type;                    // 116: (Linux-only) TRACE, MTE, or FINAL_MTE type section
   UINT32  record_count;
   UINT32  records_written;
   // used by the driver only
   UINT32  buffer_size;                     // 120:
   UINT64  cycles;                   		// 124:
   // offsets into the buffer
   UINT32  buffer_start;                    // 128:
   UINT32  buffer_end;                      // 132;
   UINT32  buffer_current;                  // 136:
   UINT32  buffer_oldest;                   // 140:
   // for continuous tracing
   UINT32  buffer_rd_offset;                // 144: Offset to be read next
   UINT32  buffer_varend;                   // 148: When the last record does not fit
   UINT32  wrap_rd;                         // 152: rd offset wrapped
   UINT32  rd_behind_wr;                    // 156: rd behind wr (rd < wr)
   // ***** available *****
   UCHAR   available[80];                   // 160 - 239
   // ***** available *****
   UINT32  slow_tsc_cnt;                    // 240: Slow TSC detected count (for all processors)
   UINT32  version;                         // 244: Trace implementation version (0xMMMMmmmm: MAJOR/minor version)
   CHAR    os[8];                           // 248: OS identification string (8 characters including NULL terminator)

   //***** Linux-specific *****
   int     variable_timer_tick;             // 256 - 259  CONFIG_NO_HZ defined
   UCHAR   available_too[128];              // 260 - 511  ***** available *****
}PERFHEADER;

#define PERFHEADER_BUFFER_SIZE MIN_WR_BLOCK_SIZE // should be sizeof(PERFHEADER)

#define DEFINED_CRETURN 10
#define LINUX64_FLAG 0x64
#define LINUX32_FLAG 0x32

#define OS_ID_STRING_LINUX            "Linux  "

/************************ Trace Hook Control Blocks ************************/

typedef struct _pi_hook_header {
    uint16_t  typeLength;
    uint16_t  majorCode;
    uint32_t  minorCode;
    uint64_t  timeStamp;
} PI_HOOK_HEADER;
#define HOOK_HEADER_SIZE sizeof(PI_HOOK_HEADER) // 16
#define HOOK_HEAD_PTR(hook) (PI_HOOK_HEADER*)&hook

#define NUM_RAWINT64 126

typedef struct RAWINT_HOOK_64BIT {
		PI_HOOK_HEADER  header;
        uint64_t  intval[NUM_RAWINT64];
}RAW64_HOOK;

#define NUM_INT64_SMALL_HOOK 5

typedef struct RAWINT_SMALL_HOOK_64BIT {
	PI_HOOK_HEADER  header;
        uint64_t  intval[NUM_INT64_SMALL_HOOK];
}RAW64_SMALL_HOOK;

#define SMALL_HOOK_SIZE sizeof(RAW64_SMALL_HOOK)

#endif // STAP_ITRACE DEFS

typedef struct _STRUCT_DISPATCH_HOOK
{
	PI_HOOK_HEADER  header;
        uint64_t  parent ;
}DISPATCH_HOOK;
#define DISPATCH_HOOK_SIZE sizeof(DISPATCH_HOOK)

typedef struct STRUCT_IRQ_HOOK_64BIT {
	PI_HOOK_HEADER  header;
        uint64_t  irq;            /* ORed with 0x80000000 for exit */
        uint64_t  count;
}IRQ_HOOK;
#define IRQ_HOOK_SIZE sizeof(IRQ_HOOK)

#define TS_CHANGE_HOOK PI_HOOK_HEADER
#define TS_CHANGE_HOOK_SIZE sizeof(TS_CHANGE_HOOK)

typedef struct STRUCT_TPROF_HOOK_64BIT {
	PI_HOOK_HEADER  header;
	/* Major code 0x10 */
	/* Minor 20 + cpl */
        uint64_t  pid;
        uint64_t  tid;
        uint64_t  addr;
}TPROF_HOOK;
#define TPROF_HOOK_SIZE sizeof(TPROF_HOOK)

#define HOOK_STRLEN_MAX 256

#ifndef STAP_ITRACE // DEFS
typedef struct STRUCT_FORK_HOOK_32BIT {
		PI_HOOK_HEADER header;
        uint64_t  pid;
        uint64_t  ppid;
        uint64_t  clone_flags;
}FORK_HOOK;
#define FORK_HOOK_SIZE sizeof(FORK_HOOK)

typedef struct STRUCT_MTE_HOOK_64BIT {
	PI_HOOK_HEADER header;
	uint64_t int_cnt;
	uint64_t pid;
	uint64_t code_size;
	uint64_t NT_ts, NT_cs;
	uint64_t addr;
	uint64_t str_len;
	char str_val[HOOK_STRLEN_MAX];
} MTE_HOOK;
#define MTE_HOOK_SIZE (sizeof(MTE_HOOK) - HOOK_STRLEN_MAX)

typedef struct STRUCT_PID_NAME_HOOK_64BIT {
	PI_HOOK_HEADER  header;
        uint64_t  int_cnt;
        uint64_t  pid;
        uint64_t  str_len;
        char      str_val[HOOK_STRLEN_MAX];
}PID_NAME_HOOK;
#define PID_NAME_HOOK_SIZE (sizeof(PID_NAME_HOOK) - HOOK_STRLEN_MAX)


// itrace

/* the load/store hook could possibly use all 7 parms */
typedef struct STRUCT_ITRACE_INFO_HOOK {
	PI_HOOK_HEADER  header;
        uint64_t  parm1;
        uint64_t  parm2;
        uint64_t  parm3;
        uint64_t  parm4;
        uint64_t  parm5;
        uint64_t  parm6;
        uint64_t  parm7;
} ITRACE_INFO_HOOK;

#define ITRACE_OFF_HOOK_SIZE         20
#define ITRACE_PROC_SWITCH_HOOK_SIZE 20
#define ITRACE_ON_HOOK_SIZE          28
#define ITRACE_BRANCH_HOOK_SIZE      28
#define ITRACE_INST_VSID_HOOK_SIZE   28

#define ITRACE_INFO_HOOK_SIZE 20  	// branch hook, 32-bit
#define ITRACE_INFO_HOOK_SIZE2 28	// branch hook, 64-bit

#define PI_ITRACE_MINOR_STORE       1
#define PI_ITRACE_MINOR_RELATIVE_IP 2
#define PI_ITRACE_MINOR_RELATIVE_LS 4
#define PI_ITRACE_MINOR_VSID        8
#define PI_ITRACE_MINOR_REPEAT_VSID 16
#define PI_ITRACE_MINOR_DATA_LENGTH 32
#define PI_ITRACE_MINOR_INST_VSID   64

enum pi_fork_type {
	PI_FT_FORK = 0,
	PI_FT_CLONE = 0X100
};

enum pi_itrace_flag {
	PI_ITRACE_FLAG_RING_LEVEL		= 0x00000001,
	PI_ITRACE_FLAG_INST_COUNT		= 0x00000002,
	PI_ITRACE_FLAG_SS		= 0x00000004,
	PI_ITRACE_FLAG_VSID		= 0x00000008,
	PI_ITRACE_FLAG_TRACE_INTERRUPTS		= 0x00000010,
};
#endif // STAP_ITRACE DEFS

#define PIDS_ITRACE_ENABLE    1
#define PIDS_ITRACE_ACTIVE    2

/* Some defines related to writing hook records */
/* Raw File Format Hook Types */

#ifndef STAP_ITRACE // DEFS
#define PERF_64BIT           (0x0000)
#define PERF_VARDATA         (0x0001)
#define PERF_MIXED_DATA      (0x0002)
#define PERF_EXTENDED        (0x0003)

#define RAWFMT_HOOK_LEN_MASK (0xfffc)
#endif // STAP_ITRACE DEFS
#define RAWFMT_HOOK_MASK     (0x0003)

#define TPROF_HOOKS      255
#define TS_PERFCLK       255      // time stamp

#ifndef STAP_ITRACE // DEFS
#define COLLECTION_MODE_MTE_END   1
#endif // STAP_ITRACE DEFS

enum pi_trace_mode {
	COLLECTION_MODE_NORMAL = 0,
	COLLECTION_MODE_WRAP,
	COLLECTION_MODE_CONTINUOS
};

/* constants for header fields */
#define	INST_COUNT_ON 	0x00000001

//
// SWTRACE hook sizes
//
#define MAX_NRM2_HOOK_SIZE        (0xFFFC)   // 65532
#define FAST_NRM2_HOOK_SIZE       (0xFA0)    // 4000

#define NRM2_HOOK_HEADER_SIZE sizeof(PI_HOOK_HEADER)   // NRM2: hook header (includes TS)

/*
 * 			MAJOR HOOKS
 * ***********************************
 */
/* Some defines for Major  codes.  */
enum pi_major_code {
	CYCLES_CHANGE_MAJOR	= 0x01,
	EXCEPTION_MAJOR		= 0x03,
	INTERRUPT_MAJOR		= 0x04,
	SYSSERVICE_MAJOR	= 0x05,
	SYSCALL_MAJOR		= SYSSERVICE_MAJOR,
	TSCHANGE_MAJOR		= 0x08,
	METRICS_MAJOR		= 0x09,
	TPROF_MAJOR			= 0x10,
	SYS_INFO_MAJOR		= 0x11,
	DISPATCH_MAJOR		= 0x12,
	TASK_SWITCH_MAJOR	= 0x13,
	MTE_MAJOR			= 0x19,
	MODULE_MAJOR		= 0x20,
	BRANCH_MAJOR		= 0x20,
	BRANCH_PRIV_MAJOR	= 0x21,
	PROC_SWITCH_MAJOR	= 0x2c,
	TRACE_ON_MAJOR		= 0x2e,
	TRACE_OFF_MAJOR		= 0x2f,
	PTHREAD_MAJOR		= 0x30,
	LOAD_STORE_MAJOR	= 0x31,
	MEMORY_MAJOR		= 0x40,
	SPECIAL_MAJOR		= 0xA8,
	HOOKIT_MAJOR		= 0xB0,
	MAX_MAJOR
};

enum pi_tprof_minor {
	TPROF_MINOR			= 0x20,
};

// Sysinfo minors
enum pi_sysinfo_minor {
	SYSINFO_INTERVAL_ID		= 0xB2,
	SYSINFO_END_MINOR		= 0xB3,
	SYSINFO_START_MINOR		= 0xB4,
	SYSINFO_CPUSPEED_MINOR	= 0xB5,
	SYSINFO_DD_ADDR			= 0xB6,
	SYSINFO_TIME_BASE_MINOR	= 0xB7,
	SYSINFO_TPROF_MODE		= 0xB8,
	SYSINFO_TPROF_EVENT_INFO	= 0xB9,
	SYSINFO_SYSTEM_PIDS		= 0xBA,
	SYSINFO_METRIC_VALUES	= 0xBC,
	SYSINFO_PERFDD			= 0xBD,
	SYSINFO_CPU				= 0xBF,
	SYSINFO_OS				= 0xBE,
};

enum pi_misc_minor {
	TSCHANGE_TIMEHI_MIN		= 0x01,
	IRQ_ENTRY_MINOR			= 0x00000001,
	IRQ_EXIT_MINOR			= 0x80000000
};

enum pi_mte_minor {
	MTE_MINOR				= 0x01,
	MTE_PIDNAME_MINOR		= 0x34,
	MTE_3264BIT_MINOR		= 0x35,
	MTE_JITSYM_MINOR		= 0x41,
	MTE_JTHR_NAME_MINOR		= 0x42,
	MTE_CLONE_FORK_MINOR	= 0x63,
	MTE_CREATE_PID_MINOR	= 0x64,
	MTE_KILL_PID_MINOR		= 0x65,
	MTE_CREATE_THR_MINOR	= 0x66,
	MTE_KILL_THR_MINOR		= 0x67,
	MTE_EXIT_START_MINOR	= 0x00000077,
	MTE_EXIT_END_MINOR		= 0x80000077,
	MTE_UNMAP_START_MINOR	= 0x00000078,
	MTE_UNMAP_END_MINOR		= 0x80000078,
	MTE_UNLOAD_START_MINOR	= 0x00000079,
	MTE_UNLOAD_END_MINOR	= 0x80000079,
	MTE_FORK_START_MINOR	= 0x0000007a,
	MTE_FORK_END_MINOR		= 0x8000007a,
	MTE_EXIT_HOOK			= 0x80000000
};

enum pi_itrace_minor {
	ITRACE_BR_FROM_RING0	= 0x20,
	ITRACE_BR_FROM_RING3	= 0x21,
	ITRACE_RING0_INTR		= 0x28,
	ITRACE_RING3_INTR		= 0x29,
	ITRACE_PIDTID_CHANGE	= 0x2C,
	ITRACE_TRACE_START		= 0x2E,
	ITRACE_TRACE_END		= 0x2F,
};

enum pi_hookit_minor {
	HOOKIT_ENTRY_MINOR		= 0x00,
	HOOKIT_EXIT_MINOR		= 0x80
};

//
// NRM2 hook types
//
//#define RAWFMT_HOOK_LEN_MASK    (0xFFFC)
//#define RAWFMT_HOOK_MASK        (0x0003)
#define RAWFMT_HOOK_64BIT       (0x0000)    // n 32-bit values
#define RAWFMT_HOOK_64BIT_NOTS  (0x0001)    // n 32-bit values, no timestamp
#define RAWFMT_HOOK_VARDATA     (0x0001)    // n buffers ***** NO LONGER USED *****
#define RAWFMT_HOOK_MIXED_DATA  (0x0002)    // n 32-bit values and m buffers
#define RAWFMT_HOOK_EXTENDED    (0x0003)    // user hook

#endif
