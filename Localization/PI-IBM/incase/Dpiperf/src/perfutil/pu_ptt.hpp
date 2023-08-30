#ifndef _PU_PTT_HPP
#define _PU_PTT_HPP



// OS Specific Stubs
int _init_ptt(int xmetric_cnt, int xmetrics[], int priv);
int _terminate_ptt(void **kpd, int kpd_elements);
int _ptt_init_thread(void **kpd, int tid);
int _ptt_uninit_thread(void **kpd, int tid);
int _ptt_get_metric_data(void **kpd, UINT64 *metricsArray);


#endif
