#ifndef JPROF_PLUGIN_INTERFACE_H
#define JPROF_PLUGIN_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

// Implemented in JprofPluginInterface.cpp
int isPluginInitialized();
int setAgentInitialized();
int HealthCenterListenerLoop(void *args);
int StartTracing();
int StopTracing();

// Implemented outside of JprofPluginInterface.cpp
int parseJProfOptionsFromPlugin(char * options);
int procSockCmdFromPlugin(void *tp, char * nline );
void RestartTiJprofFromPlugin();

#ifdef __cplusplus
}
#endif

#endif // JPROF_PLUGIN_INTERFACE_H
