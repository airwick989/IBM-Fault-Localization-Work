#include "common/types.h"
#include "common/events.h"
#include "common/kernel.h"
#include "common/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tprof.h"

int main(int argc, char *argv[])
{
   enum {
      ALL,
      TRACING_ONLY,
      POST_PROCESSING_ONLY
   } selected_mode = ALL;

   pi_event selected_event = TIME_PI_EVENT;
   pi_sample_mode sampling_mode = PI_SET_FREQUENCY_MODE;
   int32_t sample_rate = -1;
   int32_t start_delay = -1;
   int32_t tracing_time = -1;


   /* Process User Options */
   int i;
   for (i = 1; i < argc; i++)
   {
      if (!strncmp(argv[i], "--", 2))
      {
         /* Long-mode option follows... */
         char *name = argv[i] + 2;

         if (!strncmp(name, "event=", strlen("events=")))
         {
            char *value = name + strlen("events=");
            selected_event = get_pi_event_from_name(value);
         }
         else if (!strncmp(name, "sample-rate=", strlen("sample-rate=")))
         {
            char *value = name + strlen("sample-rate=");
         }
         else if (!strncmp(name, "start-delay=", strlen("start-delay=")))
         {
            char *value = name + strlen("start-delay=");
         }
         else if (!strncmp(name, "tracing-time=", strlen("tracing-time=")))
         {
            char *value = name + strlen("tracing-time=");
         }
         else if (!strncmp(name, "help", strlen("help")))
         {
            print_help();
            return 0;
         }
         else
         {
            unexpected_option(i, argv[i]);
            return 1;
         }

      }

      else if (!strncmp(argv[i], "-", 1))
      {
         /* Short-mode option follows... */
         /* a|t|p|e|c|s|r */
         char *name = argv[i] + 1;

         while(strlen(name) > 0)
         {
            if (name[0] == 'a')
            {
               /* Mode = All */
               selected_mode = ALL;
            }
            else if (name[0] == 'c')
            {
               /* Set sampling rate */
            }
            else if (name[0] == 'e')
            {
               /* Set event */
            }
            else if (name[0] == 'f')
            {
               /* Frequency-based mode */
            }
            else if (name[0] == 'g')
            {
               /* Fixed sampling period based mode */
            }
            else if (name[0] == 'p')
            {
               /* Mode = Only post process */
               selected_mode = POST_PROCESSING_ONLY;
            }
            else if (name[0] == 'r')
            {
               /* Set run time (time tracing is on) */
            }
            else if (name[0] == 's')
            {
               /* Set delay time */
            }
            else if (name[0] == 't')
            {
               /* Mode = only tracing */
               selected_mode = TRACING_ONLY;
            }

            name = name + 1;
         }
      }

      else
      {
         /* Non-optional input. There are currently no options of this form... */
         unexpected_option(i, argv[i]);
         return 1;
      }

   }


   /* Options processed, now we check valid state */
   if (selected_event == INVALID_PI_EVENT)
   {
      fprintf(stderr, "ERROR\n");
      return 1;
   }

   if (sample_rate <= 0)
   {
      fprintf(stderr, "ERROR\n");
      return 1;
   }


   /* And run tprof... */
   if (selected_mode == ALL || selected_mode == TRACING_ONLY)
   {
      /* Perform trace */
      int err_code = profile(selected_event, sampling_mode, sample_rate, "swtrace.nrm2", start_delay, tracing_time);

      if (err_code)
      {
         /* Tracing failure */
      }
      else
      {
         /* Tracing successful */
      }
   }
   
   if (selected_mode == ALL || selected_mode == POST_PROCESSING_ONLY)
   {
      /* Perform post-processing */
   }

   return 0;
}


int print_help()
{
   fprintf(stderr, "tprof usage message...\n"); /*TODO*/
   return 0;
}


int unexpected_option(int i, char *option)
{
   fprintf(stderr, "Unexpected option at position %d: %s\n", i, option);
   print_help();
   return 0;
}


int profile(pi_event selected_event, pi_sample_mode sampling_mode, int32_t sampling_rate, const char * const output_file_name, int32_t start_delay, int32_t tracing_time)
{
   /* Kernel open event */

   /* Kernel begin recording */

   /* Kernel process data as it arrives */

   /* Wait for user / time out */

   /* Close kernel event */

   /* Write out data */

   return 0;
}


int write_data()
{
   return 0;
}


int read_data()
{
   return 0;
}


int post_process()
{
   return 0;
}

