#ifndef types_h
#define types_h

typedef int (*callback_funct_type)(void);

typedef int (*event_delta_funct_type)(void);

typedef struct pi_sample_type
   {
   int sample_1;
   int sample_2;
   } pi_sample;

typedef enum pi_sample_mode_type
   {
   PI_SET_FREQUENCY_MODE,
   PI_SET_PERIOD_MODE
   } pi_sample_mode;

#endif
