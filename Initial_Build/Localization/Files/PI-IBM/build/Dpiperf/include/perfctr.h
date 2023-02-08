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

#ifndef _PERFCTR_H_
#define _PERFCTR_H_


//
//***************************************************************************//
//***************************************************************************//
//                                                                           //
//  Performance Counter Event IDs                                            //
//  -----------------------------                                            //
//                                                                           //
//  These are architecture specific: not all events may be available         //
//  in all architectures and on all counters within the architecture         //
//                                                                           //
//***************************************************************************//
//***************************************************************************//
//
#define EVENT_NONHALTED_CYCLES              101  // Cycles where CPU was not halted
                                                 // P6_CPU_CLK_UNHALTED
                                                 // P4_global_power_events (RUNNING)
                                                 // Core Solo/Duo: NonHlt_Cycles (UM=00)
                                                 // Core2: CPU_CLK_UNHALTED.CORE_P (0x3C)/UM=00

#define EVENT_INSTR                         102  // Instructions (P4=NBOGUS) retired
                                                 // P6_INST_RETIRED
                                                 // P4_instr_retired (NBOGUSNTAG,NBOGUSTAG)
                                                 // Core Solo/Duo: Instr_Ret (UM=00)
                                                 // Core2: INST_RETIRED.ANY_P (0xC0)/UM=00

#define EVENT_INSTR_COMPLETED               103  // Instructions (P4=NBOGUS) completed
                                                 // P6: N/A
                                                 // P4_instr_completed (NBOGUS)

#define EVENT_UOPS                          104  // uOps (P4=NBOGUS) retired
                                                 // P6_UOPS_RETIRED
                                                 // P4_uops_retired (NBOGUS)
                                                 // Core2: UOPS_RETIRED.ANY (0xC2)/UM=0x0F

#define EVENT_ALL_UOPS                      105  // uOps (P4=NBOGUS and BOGUS) retired
                                                 // P6_UOPS_RETIRED
                                                 // P4_uops_retired (NBOGUS, BOGUS)

#define EVENT_BRANCH                        106  // Branch instructions retired
                                                 // P6_BR_INST_RETIRED
                                                 // P4_branch_retired (MMTM,MMNM,MMTP,MMNP)
                                                 // Core2: BR_INST_RETIRED.ANY (0xC4)/UM=0x00

#define EVENT_MISPRED_BRANCH                107  // Mispredicted (P4=NBOGUS) BR instr retired
                                                 // P6_BR_MISS_PRED_RETIRED
                                                 // P4_mispred_branch_retired (NBOGUS)
                                                 // Core2: BR_INST_RETIRED.MISPRED (0xC5)/UM=0x00

#define EVENT_CALLS                         108  // Direct and indirect calls
                                                 // P6: N/A
                                                 // P4_retired_branch_type (CALL)

#define EVENT_ITLB_MISS                     109  // ITLB lookups resulting in a miss
                                                 // P6_ITLB_MISS
                                                 // P4_ITLB_reference (MISS)
                                                 // Core2: ITLB.MISSES (0x82)/UM=0x12

#define EVENT_TC_FLUSH                      110  // Number of TC flushes
                                                 // P6: N/A
                                                 // P4_TC_misc (FLUSH)

#define EVENT_TC_DELIVER_LP0                111  // Cycles during which traces being delivered to LP0
                                                 // P6: N/A
                                                 // P4_TC_deliver_mode (DD, DB, DI)

#define EVENT_TC_DELIVER_LP1                112  // Cycles during which traces being delivered to LP1
                                                 // P6: N/A
                                                 // P4_TC_deliver_mode (DD, BD, ID)

#define EVENT_TC_MISS                       113  // TC miss
                                                 // P6: N/A
                                                 // P4_BPU_fetch_request (TCMISS)

#define EVENT_L2_READ_MISS                  114  // Number of L2 read misses
                                                 // P6_L2_LINES_IN
                                                 // P4_BSQ_cache_reference (RD_2ndL_MISS)

#define EVENT_L2_READ_REFS                  115  // Number of L2 read references (hits and misses)
                                                 // P6_L2_RQSTS
                                                 // P4_BSQ_cache_reference (RD_2ndL_MISS/HITS/HITE/HITM)

#define EVENT_L3_READ_MISS                  116  // Number of L3 read misses
                                                 // P6: N/A
                                                 // P4_BSQ_cache_reference (RD_3rdL_MISS)

#define EVENT_L3_READ_REFS                  117  // Number of L3 read references (hits and misses)
                                                 // P6: N/A
                                                 // P4_BSQ_cache_reference (RD_3rdL_MISS/HITS/HITE/HITM)

#define EVENT_LOADS                         118  // Retired load operations tagged at the front end
                                                 // P6: N/A
                                                 // P4_front_end_event/Memory_loads (NBOGUS)

#define EVENT_STORES                        119  // Retired store operations tagged at the front end
                                                 // P6: N/A
                                                 // P4_front_end_event/Memory_stores (NBOGUS)

#define EVENT_MACHINE_CLEAR                 120  // Pipeline clears because of all reasons
                                                 // P6: N/A
                                                 // P4_machine_clear (CLEAR)

#define EVENT_MACHINE_CLEAR_MO              121  // Pipeline clears because of memory ordering issues
                                                 // P6: N/A
                                                 // P4_machine_clear (MOCLEAR)

#define EVENT_MACHINE_CLEAR_SM              122  // Pipeline clears because of memory self modifying code issues
                                                 // P6: N/A
                                                 // P4_machine_clear (SMCLEAR)

#define EVENT_BRANCH_TAKEN                  123  // "Number of taken branches
                                                 // P6_BR_TAKEN_RETIRED
                                                 // P4_branch_retired  (MMTP | MMTM)
                                                 // Core2: BR_INST_RETIRED.TAKEN (0xC4)/UM=0x0C

#define EVENT_DATA_MEM_REFS                 124  // All loads and stores from/to any memory type
                                                 // P6_DATA_MEM_REFS
                                                 // P4: N/A

#define EVENT_MISALIGN_MEM_REF              125  // Number of misaligned data memory references
                                                 // P6_MISALIGN_MEM_REF
                                                 // P4: N/A

#define EVENT_PROC_BUS_ACCESS               126  // Number of all bus transactions allocated in the IO Queue from this processor
                                                 // P6: N/A
                                                 // P4: IOQ_allocation (ALL_READ | ALL_WRITE | MEM_WB | MEM_WT | MEM_WP | MEM_WC | MEM_UC | OWN | PREFETCH)

#define EVENT_PROC_NONPREFETCH_BUS_ACCESS   127  // Number of all bus transactions allocated in the IO Queue from this processor, excluding prefetch sectors
                                                 // P6: N/A
                                                 // P4: IOQ_allocation (ALL_READ | ALL_WRITE | MEM_WB | MEM_WT | MEM_WP | MEM_WC | MEM_UC | OWN)

#define EVENT_PROC_READS                    128  // Number of all read transactions on the bus allocated in IO Queue from this processor
                                                 // P6: N/A
                                                 // P4: IOQ_allocation (ALL_READ | MEM_WB | MEM_WT | MEM_WP | MEM_WC | MEM_UC | OWN | PREFETCH)

#define EVENT_PROC_WRITES                   129  // Number of all write transactions on the bus allocated in IO Queue from this processor
                                                 // P6: N/A
                                                 // P4: IOQ_allocation (ALL_WRITE | MEM_WB | MEM_WT | MEM_WP | MEM_WC | MEM_UC | OWN)

#define EVENT_PROC_READS_NONPREFETCH        130  // Number of read transactions on the bus originating from this processor
                                                 // P6: N/A
                                                 // P4: IOQ_allocation (ALL_READ | MEM_WB | MEM_WT | MEM_WP | MEM_WC | MEM_UC | OWN)

#define EVENT_PROC_BUS_ACCESS_UW            131  // Accrued sum of the durations of all bus transactions by this processor (also count EVENT_PROC_BUS_ACCESS)
                                                 // P6: N/A
                                                 // P4: IOQ_active_entries (ALL_READ | ALL_WRITE | MEM_WB | MEM_WT | MEM_WP | MEM_WC | MEM_UC | OWN | PREFETCH)

#define EVENT_PROC_BUS_READS_UW             132  // Accrued sum of the durations of all read bus transactions by this processor (also count EVENT_PROC_READS)
                                                 // P6: N/A
                                                 // P4: IOQ_active_entries (ALL_READ | MEM_WB | MEM_WT | MEM_WP | MEM_WC | MEM_UC | OWN | PREFETCH)

#define EVENT_PROC_BUS_WRITES_UW            133  // Accrued sum of the durations of all write bus transactions by this processor (also count EVENT_PROC_WRITES)
                                                 // P6: N/A
                                                 // P4: IOQ_active_entries (ALL_WRITE | MEM_WB | MEM_WT | MEM_WP | MEM_WC | MEM_UC | OWN)

#define EVENT_PROC_READS_NONPREFETCH_UW     134  // Accrued sum of the durations of read transactions originating from this processor (also count EVENT_PROC_READS_NONPREFETCH)
                                                 // P6: N/A
                                                 // P4: IOQ_active_entries (ALL_READ | MEM_WB | MEM_WT | MEM_WP | MEM_WC | MEM_UC | OWN)

#define EVENT_L1ITLB_MISS_L2ITLB_MISS       135  // L1 ITLB miss and L2 ITLB miss
                                                 // P6: N/A
                                                 // P4: N/A
                                                 // AMD_IC_L1_ITLB_MISS_AND_L2_ITLB_MISS

#define EVENT_L1ITLB_MISS_L2ITLB_HIT        136  // L1 ITLB miss and L2 ITLB hit
                                                 // P6: N/A
                                                 // P4: N/A
                                                 // AMD_IC_L1_ITLB_MISS_AND_L2_ITLB_HIT

#define EVENT_DC_MISS                       137  // DC (Data Cache) miss
                                                 // P6: N/A
                                                 // P4: N/A
                                                 // AMD_DC_MISS

#define EVENT_DC_ACCESS                     138  // DC (Data Cache) access
                                                 // P6: N/A
                                                 // P4: N/A
                                                 // AMD_DC_ACCESS

#define EVENT_IC_MISS                       139  // IC (Instruction Cache) miss
                                                 // P6: N/A
                                                 // P4: N/A
                                                 // AMD_IC_MISS

#define EVENT_IC_FETCH                      140  // IC (Instruction Cache) fetch
                                                 // P6: N/A
                                                 // P4: N/A
                                                 // AMD_IC_FETCH

#define EVENT_DTLB_MISS                     141  // DTLB misses
                                                 // Core2: DTLB_MISSES.ANY (0x08)/UM=0x01

#define EVENT_DTLB_MISS_LD                  142  // DTLB misses due to load operations
                                                 // Core2: DTLB_MISSES.MISS_LD (0x08)/UM=0x02

#define EVENT_DTLB_MISS_ST                  143  // DTLB misses due to store operations
                                                 // Core2: DTLB_MISSES.MISS_ST (0x08)/UM=0x08

#define EVENT_L1I_READS                     144  // Instr fetches that miss IFU and cause memory requests
                                                 // Core2: L1I_MISSES (0x80)/UM=0x00

#define EVENT_L1I_MISSES                    145  // Instr fetches that bypass the IFU
                                                 // Core2: L1I_READS (0x81)/UM=0x00

#define EVENT_L2_LINES_IN                   146  // L2 misses
                                                 // Core2: L2_LINES_IN (0x24)/UM=0x70

#define EVENT_L2_LD                         147  // L2 reads
                                                 // Core2: L2_LD (0x29)/UM=0x7F

#define EVENT_L2_ST                         148  // L2 stores
                                                 // Core2: L2_ST (0x2A)/UM=0x4F

#define EVENT_L2_REQS                       149  // L2 requests
                                                 // Core2: L2_RQSTS (0x2E)/UM=0x7F

#define EVENT_CPU_CLK_UNHALTED_REF          150  // Bus cycles when core is not halted
                                                 // Core2: CPU_CLK_UNHALTED.BUS (0x3C)/UM=0x01

#define EVENT_INSTR_LOADS                   151  // Instructions retired that contain load operations
                                                 // Core2: INST_RETIRED.LOADS (0xC0)/UM=0x01

#define EVENT_INSTR_STORES                  152  // Instructions retired that contain store operations
                                                 // Core2: INST_RETIRED.STORES (0xC0)/UM=0x02

#define EVENT_INSTR_OTHER                   153  // Instructions retired that do not contain load/store operations
                                                 // Core2: INST_RETIRED.OTHER (0xC0)/UM=0x04

#define EVENT_L2_M_LINES_IN                 154  // Modified L1 line written back to L2
                                                 // Core2: L2_M_LINES_IN (0x25)/UM=40

#define EVENT_L2_LINES_OUT                  155  // Number of L2 cache lines evicted
                                                 // Core2: L2_LINES_OUT (0x26)/UM=70

#define EVENT_L2_M_LINES_OUT                156  // Number of modified L2 cache lines evicted
                                                 // Core2: L2_M_LINES_OUT (0x27)/UM=70

#define EVENT_L1D_CACHE_LD                  157  // Number of L1 cacheable data reads
                                                 // Core2: L1D_CACHE_LD (0x40)/UM=0F

#define EVENT_L1D_CACHE_ST                  158  // Number of L1 cacheable data writes
                                                 // Core2: L1D_CACHE_ST (0x41)/UM=0F

#define EVENT_L1D_ALL_REF                   159  // Number of references to the L1 DCache ...
                                                 // Core2: L1D_ALL_REF (0x43)/UM=10

#define EVENT_L1D_ALL_CACHE_REF             160  // Number of L1 cacheable data reads and writes ...
                                                 // Core2: L1D_ALL_CACHE_REF (0x43)/UM=02

#define EVENT_L1D_REPL                      161  // Number of cache lines allocated in the L1 DCache (ie. L1 cache misses)
                                                 // Core2: L1D_REPL (0x45)/UM=0F

#define EVENT_L1D_M_REPL                    162  // Number of modified cache lines allocated in the L1 DCache
                                                 // Core2: L1D_M_REPL (0x46)/UM=00

#define EVENT_L1D_SPLIT_LD                  163  // Number of load ops that span two cache lines
                                                 // Core2: L1D_SPLIT.LOADS (0x49)/UM=01

#define EVENT_L1D_SPLIT_ST                  164  // Number of store ops that span two cache lines
                                                 // Core2: L1D_SPLIT.STORES (0x49)/UM=02

#define EVENT_ITLB_FLUSH                    165  // Number of ITLB flushes
                                                 // Core2: ITLB.FLUSH (0x82)/UM=40

#define EVENT_EXT_SNOOP_HITM_THIS_AGENT     166  // Number of snoop HITM responses
                                                 // Core2: EXT_SNOOP.THIS_AGENT.HITM (0x77)/UM=48

#define EVENT_EXT_SNOOP_HITM_ALL_AGENTS     167  // Number of snoop HITM responses
                                                 // Core2: EXT_SNOOP.ALL_AGENTS.HITM (0x77)/UM=68

#define EVENT_L2_REQS_LD_MISS               168  // Number of loads that miss the L2 cache
                                                 // Nehalem: L2_RQSTS.LD_MISS (0x24)/UM=02

#define EVENT_L2_REQS_RFO_MISS              169  // Number of store RFO requests that miss the L2 cache
                                                 // Nehalem: L2_RQSTS.RFO_MISS (0x24)/UM=08

#define EVENT_L2_REQS_IFETCH_MISS           170  // Number of instruction fetches that miss the L2 cache
                                                 // Nehalem: L2_RQSTS.IFETCH_MISS (0x24)/UM=20

#define EVENT_L2_REQS_PREFETCH_MISS         171  // Number of L2 prefetch misses for both code and data
                                                 // Nehalem: L2_RQSTS.PREFETCH_MISS (0x24)/UM=80

#define EVENT_L2_REQS_MISS                  172  // Number of L2 misses for both code and data
                                                 // Nehalem: L2_RQSTS.MISS (0x24)/UM=AA

#define EVENT_L2_REQS_DATA                  173  // Number of L2 data requests
                                                 // Nehalem: L2_DATA_RQSTS.ANY (0x26)/UM=FF

#define EVENT_L1D_WB_L2                     174  // Number of L1 writebacks to L2
                                                 // Nehalem: L1D_WB_L2.MESI (0x28)/UM=0F

#define EVENT_L3_LAT_CACHE_MISS             175  // Number of L3 cache misses
                                                 // Nehalem: L3_LAT_CACHE.MISS (0x2E)/UM=41

#define EVENT_L1D_M_SNOOP_EVICT             176  // Number of modified cache lines evicted from the L1D cache due to snoop HITM intervention
                                                 // Nehalem: L1D.M_SNOOP_EVICT (0x51)/UM=08

#define EVENT_RESOURCE_STALLS               177  // Number of Allocator resource-related stalls
                                                 // Nehalem: RESOURCE_STALLS.ANY (0xA2)/UM=01

#define EVENT_OFFCORE_REQUESTS_ANY_READ     178  // Number of offcore read requests
                                                 // Nehalem: OFFCORE_REQUESTS.ANY.READ (0xB0)/UM=08

#define EVENT_OFFCORE_REQUESTS_ANY_RFO      179  // Number of offcore RFO requests
                                                 // Nehalem: OFFCORE_REQUESTS.ANY.RFO (0xB0)/UM=10

#define EVENT_OFFCORE_REQUESTS_UNCACHED_MEM 180  // Number of offcore uncached memory requests
                                                 // Nehalem: OFFCORE_REQUESTS.UNCACHED_MEM (0xB0)/UM=20

#define EVENT_OFFCORE_REQUESTS_L1D_WB       181  // Number of L1D writeback requests to the uncore
                                                 // Nehalem: OFFCORE_REQUESTS.L1D_WRITEBACK (0xB0)/UM=40

#define EVENT_OFFCORE_REQUESTS              182  // Number of offcore requests
                                                 // Nehalem: OFFCORE_REQUESTS.ANY (0xB0)/UM=80

#define EVENT_SNOOP_RESPONSE_HITM           183  // Number of HITM snoop responses sent by this thread in response to a snoop request
                                                 // Nehalem: SNOOP_RESPONSE.HITM (0xB8)/UM=04

#define EVENT_MEM_LOAD_RETIRED_L3_MISS      184  // Number of retired loads that miss the L3 cache
                                                 // Nehalem: MEM_LOAD_RETIRED.L3_MISS (0xCB)/UM=10

#define EVENT_MEM_LOAD_RETIRED_DTLB_MISS    185  // Number of retired loads that miss the DTLB
                                                 // Nehalem: MEM_LOAD_RETIRED.DTLB_MISS (0xCB)/UM=80

#define EVENT_BACLEAR                       186  // Number of times the front end is resteered
                                                 // Nehalem: BACLEAR.CLEAR (0xE6)/UM=01

#define EVENT_BPU_CLEARS                    187  // Number of Branch Prediction Unit (BPU) clears
                                                 // Nehalem: BPU_CLEARS.ANY (0xE8)/UM=03

#define EVENT_L2_TRANSACTIONS               188  // Number of L2 operations
                                                 // Nehalem: L2_TRANSACTIONS.ANY (0xF0)/UM=80

#define EVENT_FP_X87                        189
#define EVENT_FP_SSE_PACKED_SINGLE          190
#define EVENT_FP_SSE_PACKED_DOUBLE          191
#define EVENT_FP_SSE_PACKED_SCALAR_SINGLE   192
#define EVENT_FP_SSE_PACKED_SCALAR_DOUBLE   193
#define EVENT_FP_SIMD_PACKED_SINGLE         194
#define EVENT_FP_SIMD_PACKED_DOUBLE         195


#define EVENT_LOWEST_NUMBER      101
#define EVENT_HIGHEST_NUMBER     195
#define EVENT_COUNT               ((EVENT_HIGHEST_NUMBER - EVENT_LOWEST_NUMBER) + 1)  // Number of events defined


//
// Event Id used to indicate an invalid event
// ------------------------------------------
//
#define EVENT_INVALID_ID             0
#define EVENT_INVALID_INFO_INDEX    -1
#define CTR_INVALID_ID              -1

   //
   // Counter Event Names
   // *******************
   //
const char event_name_list[][30] = {
		"NONHALTED_CYCLES",                // 101
		"INSTR",                           // 102
		"INSTR_COMPLETED",                 // 103
		"UOPS",                            // 104
		"ALL_UOPS",                        // 105
		"BRANCH",                          // 106
		"MISPRED_BRANCH",                  // 107
		"CALLS",                           // 108
		"ITLB_MISS",                       // 109
		"TC_FLUSH",                        // 110
		"TC_DELIVER_LP0",                  // 111
		"TC_DELIVER_LP1",                  // 112
		"TC_MISS",                         // 113
		"L2_READ_MISS",                    // 114
		"L2_READ_REFS",                    // 115
		"L3_READ_MISS",                    // 116
		"L3_READ_REFS",                    // 117
		"LOADS",                           // 118
		"STORES",                          // 119
		"MACHINE_CLEAR",                   // 120
		"MACHINE_CLEAR_MO",                // 121
		"MACHINE_CLEAR_SM",                // 122
		"BRANCH_TAKEN",                    // 123
		"DATA_MEM_REFS",                   // 124
		"MISALIGN_MEM_REF",                // 125
		"PROC_BUS_ACCESS",                 // 126
		"PROC_NONPREFETCH_BUS_ACCESS",     // 127
		"PROC_READS",                      // 128
		"PROC_WRITES",                     // 129
		"PROC_READS_NONPREFETCH",          // 130
		"PROC_BUS_ACCESS_UW",              // 131
		"PROC_BUS_READS_UW",               // 132
		"PROC_BUS_WRITES_UW",              // 133
		"PROC_READS_NONPREFETCH_UW",       // 134
		"L1ITLB_MISS_L2ITLB_HIT",          // 135
		"L1ITLB_MISS_L2ITLB_MISS",         // 136
		"DC_MISS",                         // 137
		"DC_ACCESS",                       // 138
		"IC_MISS",                         // 139
		"IC_FETCH",                        // 140
		"DTLB_MISS",                       // 141
		"DTLB_MISS_LD",                    // 142
		"DTLB_MISS_ST",                    // 143
		"L1I_READS",                       // 144
		"L1I_MISSES",                      // 145
		"L2_LINES_IN",                     // 146
		"L2_LD",                           // 147
		"L2_ST",                           // 148
		"L2_REQS",                         // 149
		"CPU_CLK_UNHALTED_REF",            // 150
		"INSTR_LOADS",                     // 151
		"INSTR_STORES",                    // 152
		"INSTR_OTHER",                     // 153
		"L2_M_LINES_IN",                   // 154
		"L2_LINES_OUT",                    // 155
		"L2_M_LINES_OUT",                  // 156
		"L1D_CACHE_LD",                    // 157
		"L1D_CACHE_ST",                    // 158
		"L1D_ALL_REF",                     // 159
		"L1D_ALL_CACHE_REF",               // 160
		"L1D_REPL",                        // 161
		"L1D_M_REPL",                      // 162
		"L1D_SPLIT_LD",                    // 163
		"L1D_SPLIT_ST",                    // 164
		"ITLB_FLUSH",                      // 165
		"EXT_SNOOP_HITM_THIS_AGENT",       // 166
		"EXT_SNOOP_HITM_ALL_AGENTS",       // 167
		"L2_REQS_LD_MISS",                 // 168
		"L2_REQS_RFO_MISS",                // 169
		"L2_REQS_IFETCH_MISS",             // 170
		"L2_REQS_PREFETCH_MISS",           // 171
		"L2_REQS_MISS",                    // 172
		"L2_REQS_DATA",                    // 173
		"L1D_WB_L2",                       // 174
		"L3_LAT_CACHE_MISS",               // 175
		"L1D_M_SNOOP_EVICT",               // 176
		"RESOURCE_STALLS",                 // 177
		"OFFCORE_REQUESTS_ANY_READ",       // 178
		"OFFCORE_REQUESTS_ANY_RFO",        // 179
		"OFFCORE_REQUESTS_UNCACHED_MEM",   // 180
		"OFFCORE_REQUESTS_L1D_WB",         // 181
		"OFFCORE_REQUESTS",                // 182
		"SNOOP_RESPONSE_HITM",             // 183
		"MEM_LOAD_RETIRED_L3_MISS",        // 184
		"MEM_LOAD_RETIRED_DTLB_MISS",      // 185
		"BACLEAR",                         // 186
		"BPU_CLEARS",                      // 187
		"L2_TRANSACTIONS",                 // 188
		"FP_X87",
		"FP_SSE_PACKED_SINGLE",
		"FP_SSE_PACKED_DOUBLE",
		"FP_SSE_PACKED_SCALAR_SINGLE",
		"FP_SSE_PACKED_SCALAR_DOUBLE",
		"FP_SIMD_PACKED_SINGLE",
		"FP_SIMD_PACKED_DOUBLE"
};

// TODO: complete this list!
const char event_info_list[][128] = {
		"Number of cycles that the processor is not halted nor in sleep",	// "NONHALTED_CYCLES",// 101
		"Non-bogus instructions executed to completion",					//"INSTR", // 102
		"INSTR_COMPLETED",                 // 103
		"Non-bogus uOps executed to completion", 							// "UOPS", // 104
		"All uOps retired (for instructions retired and speculatively executed in the path of branch misprediction)", // "ALL_UOPS", // 105
		"All branch instructions executed to completion (taken/not taken, predicted/mispredicted)", "BRANCH", // 106
		"Mispredicted, non-bogus branch instructions executed to completion" // "MISPRED_BRANCH", // 107
		"All direct and indirect calls",									// "CALLS", // 108
		"Number of ITLB lookups that resulted in a miss",					// "ITLB_MISS",                       // 109
		"Number of TC (Trace Cache) flushes. Counts twice for each occurence.", // "TC_FLUSH",                        // 110
		"Cycles during which traces are being delivered to LP0",			// "TC_DELIVER_LP0",                  // 111
		"Cycles during which traces are being delivered to LP1",			// "TC_DELIVER_LP1",                  // 112
		"Number of times a significant delay occurred in order to decode and build uOps because of a TC miss",//"TC_MISS",                         // 113
		"Number of 2nd-level cache read misses",							// "L2_READ_MISS",                    // 114
		"Number of 2nd-level cache read references (hits and misses)",		// "L2_READ_REFS",                    // 115
		"Number of 3rd-level cache read misses",							// "L3_READ_MISS",                    // 116
		"Number of 3rd-level cache read references (hits and misses)",		// "L3_READ_REFS",                    // 117
		"LOADS",                           // 118
		"STORES",                          // 119
		"Number of cycles that the entire pipeline is cleared for all causes", // "MACHINE_CLEAR",                   // 120
		"Number of cycles that the entire pipeline is cleared due to memory ordering issues", // "MACHINE_CLEAR_MO",                // 121
		"Number of cycles that the entire pipeline is cleared due to self-modifying code issues", // "MACHINE_CLEAR_SM",                // 122
		"Number of taken branches (predicted and mispredicted) executed to completion", // "BRANCH_TAKEN",                    // 123
		"DATA_MEM_REFS",                   // 124
		"MISALIGN_MEM_REF",                // 125
		"PROC_BUS_ACCESS",                 // 126
		"PROC_NONPREFETCH_BUS_ACCESS",     // 127
		"PROC_READS",                      // 128
		"PROC_WRITES",                     // 129
		"PROC_READS_NONPREFETCH",          // 130
		"PROC_BUS_ACCESS_UW",              // 131
		"PROC_BUS_READS_UW",               // 132
		"PROC_BUS_WRITES_UW",              // 133
		"PROC_READS_NONPREFETCH_UW",       // 134
		"L1ITLB_MISS_L2ITLB_HIT",          // 135
		"L1ITLB_MISS_L2ITLB_MISS",         // 136
		"DC_MISS",                         // 137
		"DC_ACCESS",                       // 138
		"IC_MISS",                         // 139
		"IC_FETCH",                        // 140
		"DTLB_MISS",                       // 141
		"DTLB_MISS_LD",                    // 142
		"DTLB_MISS_ST",                    // 143
		"L1I_READS",                       // 144
		"L1I_MISSES",                      // 145
		"L2_LINES_IN",                     // 146
		"L2_LD",                           // 147
		"L2_ST",                           // 148
		"L2_REQS",                         // 149
		"CPU_CLK_UNHALTED_REF",            // 150
		"INSTR_LOADS",                     // 151
		"INSTR_STORES",                    // 152
		"INSTR_OTHER",                     // 153
		"L2_M_LINES_IN",                   // 154
		"L2_LINES_OUT",                    // 155
		"L2_M_LINES_OUT",                  // 156
		"L1D_CACHE_LD",                    // 157
		"L1D_CACHE_ST",                    // 158
		"L1D_ALL_REF",                     // 159
		"L1D_ALL_CACHE_REF",               // 160
		"L1D_REPL",                        // 161
		"L1D_M_REPL",                      // 162
		"L1D_SPLIT_LD",                    // 163
		"L1D_SPLIT_ST",                    // 164
		"ITLB_FLUSH",                      // 165
		"EXT_SNOOP_HITM_THIS_AGENT",       // 166
		"EXT_SNOOP_HITM_ALL_AGENTS",       // 167
		"L2_REQS_LD_MISS",                 // 168
		"L2_REQS_RFO_MISS",                // 169
		"L2_REQS_IFETCH_MISS",             // 170
		"L2_REQS_PREFETCH_MISS",           // 171
		"L2_REQS_MISS",                    // 172
		"L2_REQS_DATA",                    // 173
		"L1D_WB_L2",                       // 174
		"L3_LAT_CACHE_MISS",               // 175
		"L1D_M_SNOOP_EVICT",               // 176
		"RESOURCE_STALLS",                 // 177
		"OFFCORE_REQUESTS_ANY_READ",       // 178
		"OFFCORE_REQUESTS_ANY_RFO",        // 179
		"OFFCORE_REQUESTS_UNCACHED_MEM",   // 180
		"OFFCORE_REQUESTS_L1D_WB",         // 181
		"OFFCORE_REQUESTS",                // 182
		"SNOOP_RESPONSE_HITM",             // 183
		"MEM_LOAD_RETIRED_L3_MISS",        // 184
		"MEM_LOAD_RETIRED_DTLB_MISS",      // 185
		"BACLEAR",                         // 186
		"BPU_CLEARS",                      // 187
		"L2_TRANSACTIONS",                 // 188
		"FP_X87",
		"FP_SSE_PACKED_SINGLE",
		"FP_SSE_PACKED_DOUBLE",
		"FP_SSE_PACKED_SCALAR_SINGLE",
		"FP_SSE_PACKED_SCALAR_DOUBLE",
		"FP_SIMD_PACKED_SINGLE",
		"FP_SIMD_PACKED_DOUBLE"
};


#endif // _PERFCTR_H_
