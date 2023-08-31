
#ifndef TPROF_PLUGIN_H
#define TPROF_PLUGIN_H

#include <iostream>
#include <fstream>

#define PLUGIN_API_VERSION "17.06.07"
#include "AgentExtensions.h"
#include "Receiver.h"

#define PULL_SOURCE
#define TPROF_PULL_INTERVAL 2
#define DEFAULT_CAPACITY   1024 * 1024
#define SRC_ID             "Tprof"

class pi::pu::RTSampleProcessor;

class TprofPlugin: public ibmras::monitoring::connector::Receiver {
public:
   static void (*callback)(monitordata*);
   static uint32 provID;
   
   virtual ~TprofPlugin();
   static agentCoreFunctions aCF;
   static TprofPlugin* getInstance();
   
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
   int pluginStop();
   
   // reciever functions
   void receiveMessage(const std::string &id, uint32 size, void *data);
   
private:
   std::ofstream dataFile;
   static TprofPlugin* instance;
   bool hasError;
   pi::pu::RTSampleProcessor *processor;

   monitordata* generateSampleData();
   std::string *getStatusString();
   void updateStatus();
   TprofPlugin();

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


