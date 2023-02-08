
#include <sys/time.h>

#include "linux_kernel.h"
#include "common/kernel.h"
#include "common/events.h"
#include "common/types.h"

int pi_register_event_callback(callback_funct_type callback, pi_event event, uint32_t interrupt_period)
{
   return 0;
}

int pi_create_event_delta_function(event_delta_funct_type *delta_func, pi_event event)
{
   return 0;
}

int pi_populate_sample_buffer(pi_sample *sample_buffer, uint32_t sample_buffer_size)
{
   return 0;
}

int64_t get_system_time_millis() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
