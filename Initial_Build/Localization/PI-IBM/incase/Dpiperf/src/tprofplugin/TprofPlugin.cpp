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

#include "jvmti.h"
extern "C" {
#include "perfutil.h"
#include "perfmisc.h"
#include "pitrace.h"
#ifdef STATIC_POST
#include "a2n.h"
#include "post/post.h"
#include "post/bputil.h"
#endif
}
#include "perfutil/pu_trace.hpp"
#include "perfutil/RTSampleProcessor.hpp"
#include "perfutil/A2NTaskInfo.hpp"

#include "TprofPlugin.h"

#define ACF_LOG(lvl, msg)  TprofPlugin::aCF.logMessage(ibmras::common::logging::lvl, msg)

#define DATA_REPORT_START	"REPORT"
#define DATA_STATUS_STR		"STATUS"

#define STATUS_ON_STR		"RUNNING"
#define STATUS_OFF_STR		"OFF"
#define STATUS_ERROR_STR	"ERROR"
#define STATUS_PAUSED_STR	"PAUSED"
#define STATUS_READY_STR	"READY"

#define PI_TRACE_BUF_DEFAULT    (0x100000 * 3)

TprofPlugin* TprofPlugin::instance = 0;
agentCoreFunctions TprofPlugin::aCF;
uint32 TprofPlugin::provID;
void (*TprofPlugin::callback)(monitordata*);

MAPPED_DATA * pmd = NULL;    // Data mapped by device driver
bool initialized = false;

int start_worker_thread(pu_worker_thread_args *args) {
   return pthread_create((pthread_t*) args->thread_handle, 0,
         (void* (*)(void*))args->thread_start_func, 0);
}

TprofPlugin::TprofPlugin() :
      hasError(true), processor(0) {
}

TprofPlugin* TprofPlugin::getInstance() {
   if (!instance) {
      instance = new TprofPlugin();
   }
   return instance;
}

TprofPlugin::~TprofPlugin() {
}

int TprofPlugin::pluginStart() {
   int rc;
   aCF.logMessage(ibmras::common::logging::debug, ">>>TprofPlugin::start()");
   dataFile.open("tprof.data");

   pmd = GetMappedDataAddress();	// Get ptr to DD mapped data

   // initialize the buffers and the trace facilities
   rc = TraceSetMode(COLLECTION_MODE_CONTINUOS);
   if (rc != PU_SUCCESS) {
      fprintf(stderr, "TraceSetMode returned with error.\n");
      fprintf(stderr, "Look at the system log for additional information.\n");
      goto start_done;
   }

   // set the file name
   TraceSetName("tprofplugin.nrm2");
   rc = init_pu_worker(start_worker_thread, NULL);
   if (rc != 0) {
      fprintf(stderr, "Failed to start worker thread (.\n");
      goto start_done;
   }

   // set the scope to this process
   rc = TraceSetScope(TS_PROCESS, getpid());

   processor = new pi::pu::RTSampleProcessor();
   setTraceProcessor(processor, new pi::pu::A2NTaskInfo());

   // initialize the trace buffers and facilities
   rc = TraceInit(PI_TRACE_BUF_DEFAULT);
   start_done: if (rc == 0) {
      hasError = false;
      initialized = true;
   }
   aCF.logMessage(ibmras::common::logging::debug, "<<<TprofPlugin::start()");
   updateStatus();
   //startTracing();
   return rc;
}

int TprofPlugin::pluginStop() {
   aCF.logMessage(ibmras::common::logging::debug, ">>>TprofPlugin::stop()");
   stopTracing();
   // shut down all trace facilities and free buffers
   TraceTerminate();

   if (dataFile.is_open())
      dataFile.close();
   aCF.logMessage(ibmras::common::logging::debug, "<<<TprofPlugin::stop()");
   return 0;
}

std::string *TprofPlugin::getStatusString() {
   if (hasError) {
      return new std::string(DATA_STATUS_STR " " STATUS_ERROR_STR "\n");
   }
   else if (pmd->TraceOn) {
      if (pmd->TraceActive) {
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

void TprofPlugin::updateStatus() {
#ifndef PULL_SOURCE
   std::string *statusMsg = getStatusString();
   sendPushData(statusMsg->c_str(), statusMsg->length(), false);
   // attempt to send messages to the gui. not working..
   //std::string msg = "tprof_subsystem=on\ntprof.status=off";
   //aCF.agentSendMessage("configuration/tprof",  msg.length(), (void *) msg.c_str());
   //aCF.agentSendMessage("Tprof",  msg.length(), (void *) msg.c_str());
   delete statusMsg;
#endif
}

/*****************************************************************************
 * MESSAGE RECIEVER & CONTROL FUNCTIONS
 *****************************************************************************/

int TprofPlugin::startTracing() {
   int rc = 0;
   std::string str = "Profiling Started..\n";
   aCF.agentSendMessage(SRC_ID, str.length(), (void *) str.c_str());
   if (!pmd->TraceOn) {
      SetProfilerRate(100);
      rc = TraceOn();
   }
   if (rc != 0)
      hasError = true;
   updateStatus();
   return 0;
}

int TprofPlugin::stopTracing() {
   int rc = 0;
   std::string str = "Profiling Stopped\n";
   aCF.agentSendMessage(SRC_ID, str.length(), (void *) str.c_str());
   if (pmd->TraceOn) {
      // stop tracing and finalize the nrm2 file
      rc = TraceOff();

#ifndef PULL_SOURCE
      std::string reportData = processor->getUnreadData();
      if (dataFile.is_open())
         dataFile << reportData;
      sendPushData(reportData.c_str(), reportData.length(), false);
#endif
   }

   if (rc != 0)
      hasError = true;
   updateStatus();
   return rc;
}

void TprofPlugin::receiveMessage(const std::string &id, uint32 size,
                                 void *data) {
   if (id.compare(SRC_ID) != 0)
      return; // not for Tprof, ignore it

   // TODO: make a logger method and move the formatter to there
   std::ostringstream stringStream;
   std::string message((const char*) data, size);
   stringStream << "[tprofdata] received message id: ";
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
monitordata* TprofPlugin::generateSampleData() {
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

monitordata* TprofPlugin::pullCallback() {
   ACF_LOG(fine, "[tprofdata] Generating data for pull from agent");
   monitordata* data = getInstance()->generateSampleData();
   return data;
}

void TprofPlugin::pullComplete(monitordata* data) {
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
   std::string desc("Tprof Plugin");
   src->header.description = newCString(desc);
   src->header.sourceID = srcid; // source id as specified by this plugin
   src->next = NULL;
   src->header.capacity = DEFAULT_CAPACITY;
   src->callback = TprofPlugin::pullCallback;
   src->complete = TprofPlugin::pullComplete;
   src->pullInterval = TPROF_PULL_INTERVAL; // seconds
   return src;
}

pullsource* TprofPlugin::registerPullSource(agentCoreFunctions aCF,
                                            uint32 provID) {
   aCF.logMessage(ibmras::common::logging::fine,
         "[tprofdata] Registering pull sources");
   pullsource *head = createPullSource(0, "Tprof");
   TprofPlugin::provID = provID;
   TprofPlugin::aCF = aCF;
   return head;
}
#else

void TprofPlugin::sendPushData(const char *data, size_t size, bool persistant) {
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
   std::string desc("Tprof Plugin");
   src->header.description = newCString(desc);
   src->header.sourceID = srcid;
   src->header.capacity = DEFAULT_CAPACITY;
   src->next = NULL;
   return src;
}

pushsource* TprofPlugin::registerPushSource(agentCoreFunctions aCF, uint32 provID) {
   aCF.logMessage(ibmras::common::logging::fine, "[tprofdata] Registering push sources");
   pushsource *head = createPushSource(0, SRC_ID);
   TprofPlugin::callback = aCF.agentPushData;
   TprofPlugin::provID = provID;
   TprofPlugin::aCF = aCF;
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
         "[tprofdata] Registering pull source");
   pullsource *src = TprofPlugin::getInstance()->registerPullSource(aCF,
         provID);
   return src;
}
#else
pushsource* ibmras_monitoring_registerPushSource(agentCoreFunctions aCF, uint32 provID) {
   aCF.logMessage(ibmras::common::logging::debug, "[tprofdata] Registering push source");
   return TprofPlugin::getInstance()->registerPushSource(aCF, provID);
}
#endif

void* ibmras_monitoring_getReceiver() {
   ACF_LOG(fine, "[tprofdata] returning reciever");
   return (void *) TprofPlugin::getInstance();
}

int ibmras_monitoring_plugin_init(const char* properties) {
   return 0;
}

int ibmras_monitoring_plugin_start() {
   TprofPlugin::aCF.logMessage(ibmras::common::logging::fine,
         "[tprofdata] Starting");
   TprofPlugin::getInstance()->pluginStart();
   return 0;
}

int ibmras_monitoring_plugin_stop() {
   TprofPlugin::aCF.logMessage(ibmras::common::logging::fine,
         "[tprofdata] Stopping");
   TprofPlugin::getInstance()->pluginStop();
   return 0;
}

const char* ibmras_monitoring_getVersion() {
   return PLUGIN_API_VERSION;
}
} // extern "C"
