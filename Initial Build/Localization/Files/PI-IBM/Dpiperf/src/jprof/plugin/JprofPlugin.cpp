#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctime>
#include <cstring>
#include <sstream>
#include <string>
#include <iterator>
#include <streambuf>
#include <vector>

#if defined(_LINUX)
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#endif

#include "JprofPlugin.h"
#include "JprofPluginInterface.h"

#define ACF_LOG(lvl, msg)  JprofPlugin::aCF.logMessage(ibmras::common::logging::lvl, msg)

#define DATA_REPORT_START	"REPORT"
#define DATA_STATUS_STR		"STATUS"

#define STATUS_ON_STR		"RUNNING"
#define STATUS_OFF_STR		"OFF"
#define STATUS_ERROR_STR	"ERROR"
#define STATUS_PAUSED_STR	"PAUSED"
#define STATUS_READY_STR	"READY"

#define PI_TRACE_BUF_DEFAULT    (0x100000 * 3)

// JPROF TEMP
bool traceon = false;
bool traceactive = true;

JprofPlugin* JprofPlugin::instance = 0;
bool JprofPlugin::agentInitialized = false;
bool JprofPlugin::pluginInitialized = false;
agentCoreFunctions JprofPlugin::aCF;
uint32 JprofPlugin::provID;
void (*JprofPlugin::callback)(monitordata*);

JprofPlugin::JprofPlugin() :
      hasError(false) {
}

JprofPlugin* JprofPlugin::getInstance() {
   if (!instance) {
      instance = new JprofPlugin();
   }
   return instance;
}

JprofPlugin::~JprofPlugin() {
}

int JprofPlugin::pluginStart() {
   int rc;
   
   setPluginInitialized();
   if (isAgentInitialized()) {
      // Notify Agent
      // Notify HC
   }
   
   aCF.logMessage(ibmras::common::logging::debug, ">>>JprofPlugin::start()");
   dataFile.open("jprof.data");

   // DO JPROF RELATED TRACING INIT HERE

   aCF.logMessage(ibmras::common::logging::debug, "<<<JprofPlugin::start()");
   updateStatus();
   //startTracing();
   return rc;
}

int JprofPlugin::pluginInit() {
   return 0;
}

int JprofPlugin::pluginStop() {
   aCF.logMessage(ibmras::common::logging::debug, ">>>JprofPlugin::stop()");

   // DO JPROF RELATED TRACING DEINIT HERE

   if (dataFile.is_open())
      dataFile.close();
   aCF.logMessage(ibmras::common::logging::debug, "<<<JprofPlugin::stop()");
   return 0;
}

std::string *JprofPlugin::getStatusString() {
   if (hasError) {
      return new std::string(DATA_STATUS_STR " " STATUS_ERROR_STR "\n");
   }
   else if (traceon) { // UPDATE WITH JPROF EQUIVALENT
      if (traceactive) { // UPDATE WITH JPROF EQUIVALENT
         return new std::string(DATA_STATUS_STR " " STATUS_ON_STR "\n");
      }
      else {
         return new std::string(DATA_STATUS_STR " " STATUS_PAUSED_STR "\n");
      }
   }
   else {
      return new std::string(DATA_STATUS_STR " " STATUS_OFF_STR "\n");
   }
}

void JprofPlugin::updateStatus() {
#ifndef PULL_SOURCE
   std::string *statusMsg = getStatusString();
   sendPushData(statusMsg->c_str(), statusMsg->length(), false);
   // attempt to send messages to the gui. not working..
   //std::string msg = "jprof_subsystem=on\njprof.status=off";
   //aCF.agentSendMessage("configuration/jprof",  msg.length(), (void *) msg.c_str());
   //aCF.agentSendMessage("Jprof",  msg.length(), (void *) msg.c_str());
   delete statusMsg;
#endif
}

/*****************************************************************************
 * MESSAGE RECIEVER & CONTROL FUNCTIONS
 *****************************************************************************/

int JprofPlugin::startTracing() {
   int rc = 0;
   if (isAgentInitialized()) {
      std::string str = "Profiling Started..\n";
      aCF.agentSendMessage(SRC_ID, str.length(), (void *) str.c_str());
   
      // START JPROF TRACING HERE
      char *options = "calltree";
      parseJProfOptionsFromPlugin(options);
      StartTracing();
      traceon = true;
      if (rc != 0)
         hasError = true;
   }
   updateStatus();
   return 0;
}

int JprofPlugin::stopTracing() {
   int rc = 0;
   if (isAgentInitialized()) {
      std::string str = "Profiling Stopped\n";
      aCF.agentSendMessage(SRC_ID, str.length(), (void *) str.c_str());
      if (traceon) {
         // STOP JPROF TRACING HERE
         StopTracing();
         traceon = false;
   
#ifndef PULL_SOURCE
         std::string reportData; // = getUnreadData - JPROF EQUIVALENT
         if (dataFile.is_open())
            dataFile << reportData;
         sendPushData(reportData.c_str(), reportData.length(), false);
#endif
      }
   
      if (rc != 0)
         hasError = true;
   }
   updateStatus();
   return rc;
}

void JprofPlugin::receiveMessage(const std::string &id, uint32 size,
                                 void *data) {
   if (id.compare(SRC_ID) != 0)
      return; // not for Jprof, ignore it

   // TODO: make a logger method and move the formatter to there
   std::ostringstream stringStream;
   std::string message((const char*) data, size);
   stringStream << "[jprofdata] received message id: ";
   stringStream << id;
   stringStream << ", data: ";
   stringStream << message;
   ACF_LOG(fine, stringStream.str().c_str());

   // parse the request
   // comes in the form:
   // request[,data][,data]*
   std::size_t found = message.find(',');
   if (found != std::string::npos) {
      std::string command = message.substr(0, found);
      std::string rest = message.substr(found + 1);
      std::istringstream ss(rest);
      std::vector<std::string> tokens;
      std::string item;
      while (std::getline(ss, item, ',')) {
         tokens.push_back(item);
      }
      if (rest.compare("start") == 0) {
         startTracing();
      }
      else if (rest.compare("stop") == 0) {
         stopTracing();
      }
   }
}

/*****************************************************************************
 * DATA SOURCE FUNCTIONS
 *****************************************************************************/

char* newCString(const std::string& s) {
   char *result = new char[s.length() + 1];
   std::strcpy(result, s.c_str());
   return result;
}

// pull source method stubs copied from templates
#ifdef PULL_SOURCE
monitordata* JprofPlugin::generateSampleData() {
   std::string reportData = processor->getUnreadData();
   if (dataFile.is_open())
      dataFile << reportData;

   size_t size = reportData.size();
   char *buffer = new char[size];
   memcpy(buffer, (void *) reportData.c_str(), size);
   monitordata *md = new monitordata;
   md->provID = provID;
   md->sourceID = 0;
   md->data = buffer;
   md->persistent = false;
   md->size = size;
   return md;
}

monitordata* JprofPlugin::pullCallback() {
   ACF_LOG(fine, "[jprofdata] Generating data for pull from agent");
   monitordata* data = getInstance()->generateSampleData();
   return data;
}

void JprofPlugin::pullComplete(monitordata* data) {
   if (data != NULL) {
      if (data->data != NULL) {
         delete[] data->data;
      }
      delete data;
   }
}

pullsource* createPullSource(uint32 srcid, const char* name) {
   pullsource *src = new pullsource();
   src->header.name = name;
   std::string desc("Jprof Plugin");
   src->header.description = newCString(desc);
   src->header.sourceID = srcid; // source id as specified by this plugin
   src->next = NULL;
   src->header.capacity = DEFAULT_CAPACITY;
   src->callback = JprofPlugin::pullCallback;
   src->complete = JprofPlugin::pullComplete;
   src->pullInterval = JPROF_PULL_INTERVAL; // seconds
   return src;
}

pullsource* JprofPlugin::registerPullSource(agentCoreFunctions aCF,
                                            uint32 provID) {
   aCF.logMessage(ibmras::common::logging::fine,
         "[jprofdata] Registering pull sources");
   pullsource *head = createPullSource(0, "Jprof");
   JprofPlugin::provID = provID;
   JprofPlugin::aCF = aCF;
   return head;
}
#else

void JprofPlugin::sendPushData(const char *data, size_t size, bool persistant) {
   char *buffer = new char[size];
   memcpy(buffer, (void *) data, size);
   monitordata *md = new monitordata;
   md->provID = provID;
   md->sourceID = 0;
   md->data = buffer;
   md->persistent = persistant;
   md->size = size;
   aCF.agentPushData(md);
   //ACF_LOG(debug, data);
   delete md;
}

pushsource* createPushSource(uint32 srcid, const char* name) {
   pushsource *src = new pushsource();
   src->header.name = name;
   std::string desc("Jprof Plugin");
   src->header.description = newCString(desc);
   src->header.sourceID = srcid;
   src->header.capacity = DEFAULT_CAPACITY;
   src->next = NULL;
   return src;
}

pushsource* JprofPlugin::registerPushSource(agentCoreFunctions aCF, uint32 provID) {
   aCF.logMessage(ibmras::common::logging::fine, "[jprofdata] Registering push sources");
   pushsource *head = createPushSource(0, SRC_ID);
   JprofPlugin::callback = aCF.agentPushData;
   JprofPlugin::provID = provID;
   JprofPlugin::aCF = aCF;
   return head;
}
#endif

/*****************************************************************************
 * FUNCTIONS EXPORTED BY THE LIBRARY
 *****************************************************************************/

extern "C" {
#ifdef PULL_SOURCE
pullsource* ibmras_monitoring_registerPullSource(agentCoreFunctions aCF,
                                                 uint32 provID) {
   aCF.logMessage(ibmras::common::logging::debug,
         "[jprofdata] Registering pull source");
   pullsource *src = JprofPlugin::getInstance()->registerPullSource(aCF,
         provID);
   return src;
}
#else
pushsource* ibmras_monitoring_registerPushSource(agentCoreFunctions aCF, uint32 provID) {
   aCF.logMessage(ibmras::common::logging::debug, "[jprofdata] Registering push source");
   return JprofPlugin::getInstance()->registerPushSource(aCF, provID);
}
#endif

void* ibmras_monitoring_getReceiver() {
   ACF_LOG(fine, "[jprofdata] returning reciever");
   return (void *) JprofPlugin::getInstance();
}

int ibmras_monitoring_plugin_init(const char* properties) {
   return JprofPlugin::getInstance()->pluginInit();
}

int ibmras_monitoring_plugin_start() {
   JprofPlugin::aCF.logMessage(ibmras::common::logging::fine,
         "[jprofdata] Starting");
   JprofPlugin::getInstance()->pluginStart();
   return 0;
}

int ibmras_monitoring_plugin_stop() {
   JprofPlugin::aCF.logMessage(ibmras::common::logging::fine,
         "[jprofdata] Stopping");
   JprofPlugin::getInstance()->pluginStop();
   return 0;
}

const char* ibmras_monitoring_getVersion() {
   return PLUGIN_API_VERSION;
}
} // extern "C"
