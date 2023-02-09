
#ifndef _PI_TIME_H
#define _PI_TIME_H

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>


#define GET_UTC_TIME time(0)

static inline void get_time(int *time_sec, int *minuteswest, int *dsttime) {
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	if (time_sec)
		*time_sec = tv.tv_sec;
	if (minuteswest)
		*minuteswest = tz.tz_minuteswest;
	if (dsttime)
		*dsttime = tz.tz_dsttime;
}

static inline int64_t get_system_time_millis() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static inline int64_t get_system_time_micros() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

static inline int64_t get_monotonic_time_nanos() {
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return tp.tv_sec * 1000000000 + tp.tv_nsec;
}

static inline void sleep_micros(long long int micros) {
	usleep(micros);
}

static inline int64_t get_timestamp() {
	return get_monotonic_time_nanos();
}

#endif
