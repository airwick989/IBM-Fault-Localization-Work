#ifndef _PU_SCS_HPP
#define _PU_SCS_HPP

#include "perfutil_core.hpp"

// OS Specific Stubs
int _init_scs(scs_callback callback);
int _scs_init_thread(uint32_t scs_active, int tid);
int _scs_uninit_thread(int tid);
int _scs_on();
int _scs_off();
int _scs_pause();
int _scs_resume();
int _wait_on_scs_data(void **scs_data, uint32 timeout_millis);
int _read_scs_data(void *scs_data, void *jniEnv);

#endif
