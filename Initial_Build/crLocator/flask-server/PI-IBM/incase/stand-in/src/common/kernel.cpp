

#include <sys/time.h>
#include <unistd.h>

#include "common/kernel.h"
#include "common/events.h"
#include "common/types.h"

int64_t get_system_time_millis() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int64_t get_system_time_micros() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

void sleep_micros(int64_t nanos) {
	usleep(nanos);
}
