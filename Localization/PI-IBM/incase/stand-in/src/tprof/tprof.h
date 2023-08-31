#ifndef tprof_h
#define tprof_h

#include "common/types.h"
#include "common/events.h"

#include <stdint.h>

int main(int argc, char *argv[]);

int print_help();

int unexpected_option(int i, char *option);

int profile(pi_event selected_event, pi_sample_mode sampling_mode, int32_t sampling_rate, const char * const output_file_name, int32_t start_delay, int32_t tracing_time);



#endif
