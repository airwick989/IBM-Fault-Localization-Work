#ifndef fileLogging_h
#define fileLogging_h

enum pi_verbose_level
   {
   PI_VL_NONE = 0,
   PI_VL_CRITICAL,
   PI_VL_WARNING,
   PI_VL_INFO
   };
   
void init_file_log(char *fileName, pi_verbose_level vLevel);

void close_file_log();

void log_to_file(pi_verbose_level vLevel, const char *format, ...);

#endif
