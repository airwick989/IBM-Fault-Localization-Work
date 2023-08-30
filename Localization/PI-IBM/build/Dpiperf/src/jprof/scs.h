/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2009
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef SCS_H
   #define SCS_H

struct scs_options
{
   #ifndef _LEXTERN
      #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)
   int rate;                            // SCS_RATE specified
   int rate_val;                        // Rate
   int count;                           // SCS_COUNT specified
   int count_val;                       // Count
   int event;                           // SCS_EVENT specified
   int event_val;                       // Event Id (converted if needed)
   int frames;                          // SCS_FRAMES specified
   int frames_val;                      // Frames
   int trace;                           // SCS_TRACE specified
   int a2n;                             // SCS_A2N specified
   int any_other;                       // Other options (not SCS) specified
         #if defined(_AIX)
   char event_name[32];                 // Name of event specified
         #endif
      #endif
   #endif
   int any;                             // Any of the new options
};

struct _scs_counters
{
   int scs_stack_attempt_count;
   int scs_nostack_count;
   int scs_retry_giveup_count;
   int scs_gc_count;
   int deepest_stack;
};

   #if defined(_WINDOWS) || defined(_LINUX) || defined(_AIX)

      #ifndef _LEXTERN
// Environment variable controls

void scs_a2n_send_jitted_method(UINT32 tls_index,
                                void * method_addr,
                                int    method_length,
                                char * class_name,
                                char * method_name,
                                char * method_signature,
                                char * code_name);

void scs_initialize(JNIEnv * env);

void scs_terminate(void);

void scs_clear_counters(void);

void scs_disable_method_entry_exit_events(thread_t * tp);

void scs_deal_with_good_sample( thread_t * tp,
                                UINT64     sample_eip,   // Sample EIP/RIP
                                UINT64     sample_r3_eip,   // Sample ring 3 EIP/RIP
                                char     * buffer);

void scs_deal_with_nostack_sample(thread_t * tp,
                                  UINT64     tick_addr,
                                  UINT64     user_addr,
                                  char     * buffer);

void clear_jvm_stats(void);

      #endif //_LEXTERN

      #define debug_trace_hook(_h_)
/*
 * TODO: Figure this out...
      #define debug_trace_hook(_h_)        \
   do {                                    \
      if (ge.scs_debug_trace) {            \
         TraceHook _h_;                    \
      }                                    \
   } while (0)
*/
      #define scs_verbose_msg(_m_)                                \
   do {                                                           \
      if (gc.mask & (gc_mask_SCS_Verbose | gc_mask_SCS_Debug)) {  \
         msg_log _m_;                                             \
      }                                                           \
   } while (0)

   #else

      #define debug_trace_hook(_h_)

   #endif //_WINDOWS || _LINUX || _AIX

   #define scs_debug_msg(_m_)             \
   do {                                   \
      if (gc.mask & gc_mask_SCS_Debug) {  \
         msg_log _m_;                     \
      }                                   \
   } while (0)

int  scs_check_options(struct scs_options * opt);

void scs_a2n_init(void);


//
// SCS symbol resolution method
//
// * BTS_A2N    backtrace_symbols() followed by A2N
//              - Not allowed if doing on-the-fly symbol resolution
// * BTS        backtrace_symbols() only
//              - Will not get kernel symbols
// * A2N        A2N only
//
#define SCS_RESOLUTION_METHOD_BTS_A2N        0
#define SCS_RESOLUTION_METHOD_BTS_ONLY       1
#define SCS_RESOLUTION_METHOD_A2N_ONLY       2

#endif // SCS_H
