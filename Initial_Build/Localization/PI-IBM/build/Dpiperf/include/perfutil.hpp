
#include <string>

extern "C" {
#include "perfutil.h"
}

#ifndef _PERFUTIL_HPP_
#define _PERFUTIL_HPP_

namespace PI {
namespace TRACE {


enum TraceScope {
	TS_SYSTEM = 0, TS_PROCESS
};

class __cdecl TraceFacility {
public:
	TraceFacility(std::string name,
					  TraceScope scope = TS_SYSTEM,
					  int pid = -1);
	~TraceFacility();

	int			init();
	int			turnOn();
	int			turnOff();
	int			suspend()
	int			resume();
	int			terminate();

	bool			isTraceOn();
	bool			isTraceActive();

	int			setTprofEvent(int event, int event_cnt);
	int			setProfilerRate(int tprofRate);
	int			setName(std::string name);
	std::string	getName();

private:
	bool			traceOn;
	bool			traceActive;
	int			rate;
	TraceScope	scope;
	int			pid;
};

} // TRACE
} // PI

#endif  // _PERFUTIL_HPP_
