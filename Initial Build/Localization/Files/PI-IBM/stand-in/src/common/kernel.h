#ifndef kernel_h
#define kernel_h

#include <stdint.h>
#include "common/types.h"
#include "common/events.h"

int pi_register_event_callback(callback_funct_type callback, pi_event event, uint32_t interrupt_period);

int pi_create_event_delta_function(event_delta_funct_type *delta_func, pi_event event);

int pi_populate_sample_buffer(pi_sample *sample_buffer, uint32_t sample_buffer_size);

int pi_open_kernel_event(pi_event event, pi_sample_mode sample_mode, uint32_t interrupt_period);

int64_t get_system_time_millis();

int64_t get_system_time_micros();

void sleep_micros(int64_t nanos);

#endif
