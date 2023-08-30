#ifndef _PU_WORKER_HPP
#define _PU_WORKER_HPP

#include "perfutil_core.hpp"

int kill_pu_worker();
int notify_pu_worker();
int _pu_worker_tprof(bool force_write);

#endif //_PU_WORKER_HPP
