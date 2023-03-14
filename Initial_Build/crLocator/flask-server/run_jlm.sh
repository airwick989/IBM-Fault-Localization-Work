#!/bin/bash

export LD_LIBRARY_PATH='$(pwd)/PI-IBM/build/Dpiperf/lib' &&
cd PI-IBM/build/Dpiperf/bin &&
. setrunenv &&
cd ../../../.. &&
java -agentlib:jprof=scs=monitor_contended_entered+,logpath=$(pwd)/logs -jar ${@:2} &
pid=$!;
echo $pid;
sleep $1 &&
kill $(ps -o pid= --ppid $pid)