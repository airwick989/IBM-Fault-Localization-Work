#include "pi_time.h"
#include "JprofPlugin.h"
#include "JprofPluginInterface.h"

volatile int continue_working = 0;
volatile int worker_running = 0;

enum ProfilingState {
   UNINITIALIZED = 0,
   START,
   STOP
};

volatile ProfilingState state = UNINITIALIZED;

int isPluginInitialized()
{
   return JprofPlugin::isPluginInitialized() ? 1 : 0;
}

int setAgentInitialized()
{
   JprofPlugin::setAgentInitialized();
   if (isPluginInitialized()) {
      // Notify Plugin
      // Notify HC
   }
   return 0;
}

int StartTracing()
{
   state = START;
   return 0;
}

int StopTracing()
{
   state = STOP;
   return 0;
}

int HealthCenterListenerLoop(void *args)
{
   continue_working = 1;
   
   worker_running = 1;
   
   ProfilingState old_state = UNINITIALIZED;
   ProfilingState current_state = state;
   
   while (continue_working) {
   
      if ((old_state == UNINITIALIZED || old_state == STOP) 
          && current_state == START) {
         // Start Profiling
         RestartTiJprofFromPlugin();
         procSockCmdFromPlugin(0, "START");
      }
      else if (old_state == START
               && current_state == STOP) {
         // Stop Profiling
         procSockCmdFromPlugin(0, "STOP");
      }

      old_state = current_state;
      current_state = state;
      
      if (isPluginInitialized()) {
         JprofPlugin::getInstance()->updateStatus();
      }
      
      sleep_micros(1 * 1000);
   }
}
