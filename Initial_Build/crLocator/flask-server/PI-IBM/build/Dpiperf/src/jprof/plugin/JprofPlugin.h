
#ifndef JPROF_PLUGIN_H
#define JPROF_PLUGIN_H

#include <iostream>
#include <fstream>

#define PLUGIN_API_VERSION "17.06.07"
#include "AgentExtensions.h"
#include "Receiver.h"

//#define PULL_SOURCE
#define JPROF_PULL_INTERVAL 2
#define DEFAULT_CAPACITY   1024 * 1024
#define SRC_ID             "Jprof"

class JprofPlugin: public ibmras::monitoring::connector::Receiver {
public:
   static void (*callback)(monitordata*);
   static uint32 provID;
   
   virtual ~JprofPlugin();
   static agentCoreFunctions aCF;
   static JprofPlugin* getInstance();
   
   // data collection
#ifdef PULL_SOURCE
   pullsource* registerPullSource(agentCoreFunctions aCF, uint32 provID);
   static monitordata* pullCallback();
   static void pullComplete(monitordata*);
#else
   static void sendPushData(const char *data, size_t size, bool persistant);
   pushsource* registerPushSource(agentCoreFunctions aCF, uint32 provID);
#endif
   
   // plugin functions
   int pluginStart();
   int pluginInit();
   int pluginStop();
   void updateStatus();
   
   // reciever functions
   void receiveMessage(const std::string &id, uint32 size, void *data);
   
   // Agent interface functions
   static void setPluginInitialized() { pluginInitialized = true; }
   static bool isPluginInitialized () { return pluginInitialized; }
   static void setAgentInitialized() { agentInitialized = true; }
   static bool isAgentInitialized() { return agentInitialized; }
   
private:
   static JprofPlugin* instance;
   static bool agentInitialized;
   static bool pluginInitialized;

   std::ofstream dataFile;
   bool hasError;

   monitordata* generateSampleData();
   std::string *getStatusString();
   JprofPlugin();

   // control functions
   int startTracing();
   int stopTracing();

   // perfutil data
};

extern "C" {
#ifdef PULL_SOURCE
PLUGIN_API_DECL pullsource* ibmras_monitoring_registerPullSource(agentCoreFunctions aCF, uint32 provID);
#else
PLUGIN_API_DECL pushsource* ibmras_monitoring_registerPushSource(agentCoreFunctions aCF, uint32 provID);
#endif
PLUGIN_API_DECL void* ibmras_monitoring_getReceiver();
PLUGIN_API_DECL int ibmras_monitoring_plugin_init(const char* properties);
PLUGIN_API_DECL int ibmras_monitoring_plugin_start();
PLUGIN_API_DECL int ibmras_monitoring_plugin_stop();
PLUGIN_API_DECL const char* ibmras_monitoring_getVersion();
}
const std::string COMMA = ",";
const std::string EQUALS = "=";

#endif


