
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "fileLogging.h"
#include "kernel.h"

//Static Variables...
FILE *log_file = NULL;
pi_verbose_level verbose_level = PI_VL_NONE;


void init_file_log(char *fileName, pi_verbose_level vLevel) {
	char logname[500];
	snprintf(logname, sizeof(logname), "%s-%ld.log", fileName, get_system_time_millis());
	
	log_file = fopen(logname, "w");
	if (!log_file)
		printf("Failed to open log file: %s", logname);
	verbose_level = vLevel;
}

void close_file_log() {
	if (log_file)
      fclose(log_file);
}

void log_to_file(pi_verbose_level vLevel, const char *format, ...) {
	va_list arglist;
	va_start( arglist, format );
	
	if (vLevel <= verbose_level) {
		if (log_file) {
			//fprintf( log_file, "" );
			vfprintf( log_file, format, arglist );
		} else {
			vprintf(format, arglist);
		}
	}
	
	va_end( arglist );
}

