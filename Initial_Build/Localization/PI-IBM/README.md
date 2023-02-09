# General Description
Performance Inspector (PI) is a set of performance tools. It is based off
the original PI tools (hereby referred to as the "old tools") found at 
http://ausgsa.ibm.com/projects/w/wbiperftools/doc/index.html which were first 
developed sometime in the late 1900s / early 2000s. The current PI tools is a 
subset of the old tools (mainly because many equivalent tools now exist by 
default in modern operating systems).

The main PI tools are:
* TPROF - a system profiler consisting of the following components:
   * A2N - symbol lookup
   * SWTRACE - system profiler
   * POST - post processor
* JPROF - a JVMTI agent containing the following tools to profile the J9 VM:
   * TPROF - used in conjunction with the stand alone TPROF tool for JIT compiled method profiling
   * SCS - Callstack Sampling
   * JLM - Java Lock Monitoring
   * Arcflow/Callflow/Calltree/CallTrace - Method entry/exit tracing
* RTDriver - JPROF controller
* ARCF - JPROF output post processor
* MXView - JPROF processed output file viewer

In the old tools, TPROF, SCS, and the Method entry/exit tracing tools depended 
heavily on a kernel module which was used as the source of all profiling data.
This made maintaining the code unsustainable because it would require modifications 
every time the kernel changed. It was also very unstable since a bug could result 
in kernel panics. The current PI tools uses OS provided profiling APIs and runs 
entirely in userspace. As such, it is far easier to maintain and is much more stable. 

Currently, PI tools is only supported on Linux (x, p, and z). However, there are plans 
to add support for Windows, z/OS, and AIX.

# Prerequisites
1. `cmake`
2. `binutils-dev`
3. `libiberty-dev`

# Building PI
These are the recommended steps:
1. clone the repo into `$PI_DIR` and checkout `standalone` branch
2. `cd $PI_DIR`
3. `mkdir build`
4. `cd build`
5. `cmake $PI_DIR/Dpiperf/src`
6. `make`

One issue hit on some machines is that it'll complain about the -std=c++11 
compile option. To fix this:
1. `cd $PI_DIR/build`
2. `rm -r *`
3. edit `$PI_DIR/Dpiperf/src/CMakeLists.txt` and remove `std=c++11` from the line containing 
`set (GCC_COVERAGE_CXX_FLAGS "-std=c++11 -Wno-narrowing -Wno-write-strings")`
4. `cmake $PI_DIR/Dpiperf/src`
5. `make`

# Running TProf
#### Terminal 1 (java)
1. `cd $PI_DIR/Dpiperf/bin`
2. `. setrunenv`
3. `cd $BENCHMARK_DIR`

#### Terminal 2 (tprof)
1. `sudo su` # sudo access is needed to run tprof
2. `cd $PI_DIR/Dpiperf/bin`
3. `. setrunenv`
4. `cd $OUTPUT_FILES_DIR`

#### Terminal 1
4. `java -agentlib:jprof=tprof,logpath=$OUTPUT_FILES_DIR <rest of java args>`

#### Terminal 2
4. `$PI_DIR/Dpiperf/bin/run.tprof <tprofoptions>`

The `tprof.out` file will be created in the `$OUTPUT_FILES_DIR` . It is important 
that run.tprof is called from `$OUTPUT_FILES_DIR` because `post` (the tool that 
generates the tprof.out file) needs the `log-jita2n_pid` and `log-jtnm_pid files` 
for symbol resolution. The default sampling rate for `tprof` is 100 ticks/second, 
you may want to change that to 1000 ticks/second if that's too coarse 
by doing `run.tprof -c 1000` (increasing the frequency any higher results in a 
non negligible overhead).

#### Analyzing Tprof Results

_analyze-tprof-output.py_ in _Dpiperf/tools/_ can be used to analyze the results of (one or multiple) output file(s) and visualize them in a table. More info about the usage of the tool can be found in _Dpiperf/doc_ directory.

# Running SCS
#### Terminal 1 (java)
1. `cd $PI_DIR/Dpiperf/bin`
2. `. setrunenv`
3. `cd $BENCHMARK_DIR`
4. `java -agentlib:jprof=sampling,logpath=$OUTPUT_FILES_DIR <rest of java args>`

#### Terminal 2 (rtdriver)
1. `sudo su` # sudo access is needed to run rtdriver
2. `cd $PI_DIR/Dpiperf/bin`
3. `./rtdriver -l`
4. `start`
5. `stop`
6. `Ctrl-c`

#### Terminal 1
5. End java process
6. `cd $OUTPUT_FILES_DIR`
7. `java -jar $PI_DIR/Dpiperf/tools/mxview.jar log-rt.1_pid`

# Running JLM
#### Terminal 1 (java)
1. `cd $PI_DIR/Dpiperf/bin`
2. `. setrunenv`
3. `cd $BENCHMARK_DIR`
4. `java -agentlib:jprof <rest of java args>`

#### Terminal 2 (rtdriver)
1. `./rtdriver -l`
2. `jlmstart` or `jlmlitestart`
3. `jlmdump`
4. `jlmstop`

#### Terminal 1
5. End java process

# Running Arcflow/Callflow/Calltrace/Calltree
1. `cd $PI_DIR/Dpiperf/bin`
2. `. setrunenv`
3. `cd $BENCHMARK_DIR`
4. `java -agentlib:jprof=rtarcf,start,logpath=$OUTPUT_FILES_DIR <rest of java args>`

Or

4. `java -agentlib:jprof=callflow,start,logpath=$OUTPUT_FILES_DIR <rest of java args>`

Or

4. `java -agentlib:jprof=calltree,start,logpath=$OUTPUT_FILES_DIR <rest of java args>`

# Further Reading
More detailed information can be found in the doc folder

