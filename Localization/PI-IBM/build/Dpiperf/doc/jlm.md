# General Description
JLM is tool under JPROF that provides monitor hold time accounting and 
contention statistics on monitors used in Java applications and the JVM itself. 
Functionality provided includes:
* Counters associated with contended locks
   * Total number of successful acquires
   * Recursive acquires - Number of times the monitor was requested and was already owned by the requesting thread.
   * Number of times the requesting thread was blocked waiting on the monitor because the monitor was already owned by another thread.
   * Cumulative time the monitor was held. 
   * For platforms that support 3 Tier Spin Locking the following are also collected:
      * Number of times the requesting thread went through the inner (spin loop) while attempting acquire the monitor.
      * Number of times the requesting thread went through the outer (thread yield loop) while attempting acquire the monitor.
* Full collection control via the rtdriver facility. Commands include:
   * jlmlitestart: collect counters only.
   * jlmstart: collect counters plus hold time accounting. Note: Hold time accounting incurs a greater overhead.
   * jlmstop: stops data collection.
   * jlmdump: dumps collected data. Data collection continues.
* Removal of Garbage Collection time from lock hold times
   * GC time is removed from hold times for all monitors held across a GC cycle.

# Time and time units
Time (as in hold time) is accounted for in units of cycles on all xSeries 
and zSeries platforms and in Time Base ticks for pSeries platforms.

# Supported Java Versions and Platforms
JLM support is shipped as part of the IBM Java Runtime Environment, v1.4.0 and above.

# Usage
1. Start Java application with the following

   `java   -agentlib:jprof=[,logpath=path]   classfile`

   where `logpath=path` sets teh path directory as the location for the JPROF output files.
2. For runtime control, you must execute `rtdriver -l` in another terminal
   * jlm | jlmstart
   * jlml | jlmstartlite
   * jd | jlmdump
   * jreset | jlmreset
   * jstop | jlmstop
3. Output is in `log-jlm.n`

# Detailed Usage
#### Terminal 1 (java)
1. Go to the PI Installation directory:

   `cd $PI_DIR/Dpiperf/bin`
2. Set the environment variables: 

   `. setrunenv`
3. Go to the java application directory: 

   `cd $BENCHMARK_DIR`
4. Start the java application: 

   `java -agentlib:jprof <rest of java args>`

   This command will run jprof in socket mode, which means JProf will listen for 
   commands to start/stop JLM data collection or to dump JLM data.JProf provides 
   additional options to start JLM immediately for instances where you are interested 
   in monitor usage during application start up. 

#### Terminal 2 (rtdriver)
1. Start the rtdriver

   `./rtdriver -l`

   This command will cause rtdriver to attempt to connect on the local machine and 
   then enter command mode.
2. Start collecting JLM data

   `jlmstart`

   or

   `jlmlitestart`

   The `jlmstart` command collects counters and monitor hold times whereas the 
   `jlmlitestart` command only collects counters.
3. Dump JLM data

   `jlmdump`

   JLM dump files are named `log-jlm.#_pppp` where `#` is a sequence number, 
   starting with 1 (one) for the first jlmdump and incrementing by one for each 
   subsequent jlmdump, and `pppp` is the PID of the Java process. The files are 
   located in the Java current working directory (i.e. the directory from where 
   Java was started).
4. Stop JLM

   `jlmstop`

#### Terminal 1
5. Stop java program

# JLM Reported Statistics Description
The following descriptions refer to this sample JLM output produced by JProf:
```
   JLM_Interval_Time 28503135251


  System (Registered) Monitors

 %MISS   GETS NONREC   SLOW    REC TIER2 TIER3 %UTIL AVER-HTM  MON-NAME
    11     91     91     10      0     0     0     1  4550728  JITC Global_Compile lock
     0   2466   2217      0    249     0     0     0     1780  Thread queue lock
     0    752    751      0      1     0     0     0    11160  Binclass lock
     0    701    695      0      6     0     0     0    71449  JITC CHA lock
     0    286    286      0      0     0     0     0   408679  Classloader lock
     0    131    131      0      0     0     0     0    26877  Heap lock
     0     61     61      0      0     0     0     0     2188  Sleep lock
     0     51     50      0      1     0     0     0      718  Monitor Cache lock
     0      7      7      0      0     0     0     0      608  JNI Global Reference lock
     0      5      5      0      0     0     0     0      780  Monitor Registry lock
     0      0      0      0      0     0     0     0        0  Heap Promotion lock
     0      0      0      0      0     0     0     0        0  Evacuation Region lock
     0      0      0      0      0     0     0     0        0  Method trace lock
     0      0      0      0      0     0     0     0        0  JNI Pinning lock

  Java (Inflated) Monitors

 %MISS   GETS NONREC   SLOW    REC TIER2 TIER3 %UTIL AVER-HTM  MON-NAME
    33      3      3      1      0     0     0     0     8155  java.lang.Class@7E8EF8/7E8F00
    33      3      3      1      0     0     0     0     8441  java.lang.Class@7E8838/7E8840
     0 3314714 3314714    809      0     0     0     3      278  testobject@104D3150/104D3158
     0 3580384 3580384    792      0     0     0     4      281  testobject@104D3160/104D3168
     0      1      1      0      0     0     0     0      735  java.lang.ref.ReferenceQueue$Lock@101BDE50/101BDE58
     0      1      1      0      0     0     0     0      833  java.lang.ref.Reference$Lock@101BE118/101BE120

 LEGEND:
                %MISS : 100 * SLOW / NONREC
                 GETS : Lock Entries
               NONREC : Non Recursive Gets
                 SLOW : Non Recursives that Wait
                  REC : Recursive Gets
                TIER2 : SMP: Total try-enter spin loop cnt (middle for 3 tier)
                TIER3 : SMP: Total yield spin loop cnt (outer for 3 tier)
                %UTIL : 100 * Hold-Time / Total-Time
              AVER-HT : Hold-Time / NONREC
```

#### Example of contention on one monitor
```
KEY:
         - work (thread is busy)
         | request lock
         > lock ownership granted
         ~ thread holds lock
         < thread releases lock
         * thread waiting to be granted ownership
Thread 1:   -----|>~~~~~~~~~~~~~~~~~~~~~~~~~~~<-----------------------------
Thread 2:   ------------|**********************>~~~~~~~~~~~~<---------------
Thread 3:   ------------------|******************************>~~~~~~<-------
Thread 4:   ----------------------|**********************************>~~~<--
       Hold time: |<------------------------->|
       Thread 1
                                               |<---------->|               Thread 2
                                                             |<---->|       Thread 3
                                                                     |<->|  Thread 4
```

#### Background

A monitor can be acquired in one of two ways:
* **Recursively**, when the requesting thread already owns it.
* **Non-recursively**, when the requesting thread does not already own it. Non-recursive acquires can be further divided into"
   * **Fast**, when the requested monitor is not already owned and the requesting thread gains ownership immediately. 
   On platforms that implement 3-Tier Spin Locking any monitor acquired while spinning is considered a Fast acquire, regardless of the number of iterations in each tier.
   * **Slow**, when the requested monitor is already owned by another thread and the requesting thread is blocked.

#### Fields in the report
* `JLM_Interval_Time` - Time interval between jlmstart and jlmdump. Time is expressed in the units appropriate for the hardware platform.
* `%MISS` - Percentage of the total GETS (acquires) where the requesting thread was blocked waiting on the monitor. 
    `%MISS` = (`SLOW` / `NONREC`) * 100
* `GETS` - Total number of successful acquires. 
    `GETS` = `FAST` + `SLOW` + `REC`
* `NONREC` - Total number of non-recursive acquires. This number includes SLOW gets.
* `SLOW` - Total number of non-recursive acquires which caused the requesting thread to block waiting for the monitor to become unowned. This number is included in NONREC.
   To calculate the number of non-recursive acquires in which the requesting thread obtained ownership immediately (FAST), subtract SLOW from NONREC. 
   On platforms that support 3-Tier Spin Locking, monitors acquired while spinning are considered FAST acquires.
* `REC` - Total number of recursive acquires. A recursive acquire is one where the requesting thread already owns the monitor.
* `TIER2` - Total number of Tier 2 (inner spin loop) iterations on platforms that support 3-Tier Spin Locking.
* `TIER3` - Total number of Tier 3 (outer thread yield loop) iterations on platforms that support 3-Tier Spin Locking.
* `%UTIL` - Monitor hold time divided by JLM_Interval_Time. Hold time accounting must be turned on.
* `AVER-HTM` - Average amount of time the monitor was held. Recursive acquires are not included because the monitor is already owned when acquired recursively. 
    `AVER-HTM` = `Total hold time` / `NONREC`
* `MON-NAME` - Monitor name or NULL (blank) if name not known.

#### Time, time units, and miscellaneous information
* Time (as in hold time) is accounted for in different units depending on the hardware platform. All, however, account for time in some absolute units which have a relationship to real (i.e. wall clock) time but do not display time in the traditional time units (seconds, milliseconds, etc.). Time units for the different platforms are as follows:
   * On x86 all "times" are based on cycle counts.
   * On PPC all "times" are based on Time Base ticks.
   * On S390 all "times" are based on cycles counts.
* JLM does not provide a mechanism to convert "time units" (cycles or timebase ticks) to real time.
* JLM does not provide a mechanism to account for the impact of thread dispatching and/or interrupts. Those are considered as normal part of system operation and any impact they have on acquiring/holding monitors are reflected in the appropriate times.
* For hold-time accounting, only completed holds are considered. Any hold that is active at the time the JLM data is dumped is not included in the report.
* On SMP systems hold times are not related to processors (other than as listed below). Utilization statistics are per monitor regardless of the number of processors.
* For hold-time accounting, only completed holds are considered. Any hold that is active at the time the JLM data is dumped is not included in the report.
* Hold time accounting (`jlmstart`) incurs a greater overhead than "normal" (`jlmlitestart`) because time must be read on every acquire and release, time deltas must be calculated, and total time must be maintained.
* On SMP Intel x86, the CPU clocks may not synchronized. JLM does not provide a mechanism to "normalize" times. If a monitor is acquired on one processor and released on another, the "hold time" for that particular acquire/release instance may be:
   * Overstated if the processor on which the monitor is released has a clock that's ahead of the processor on which the monitor was acquired. 
   * Understated if the processor on which the monitor is released has a clock that's behind that of the processor on which the monitor was acquired. 
   * Zero if understated and time when monitor is released is earlier than time when monitor was acquired, causing a negative hold time. 
* If the acquire/release occurs on the same processor then the hold time is guaranteed to be correct.
* JLM dumps are very intrusive. All threads must be quiesced in order to enumerate monitors and retrieve JLM data. You should minimize the number of dumps in order to minimize impact to the scenario being measured.

#### Tips on understanding JLM output
To locate "hot" (contended) locks, it is most useful to run the application 
in question on as many processors as possible. That is, hot locks will be easier 
to spot on a 4-way system than on a 2-way, and accordingly, an 8-way system is 
even better. Once data is collected, the most important column is the 
`%MISS column`. One rule of thumb is that locks with a `%MISS` greater than 15% 
cause performance problems and reducing contention on that lock is worthwhile.

Another area of the report which is worth paying close attention to is the 
`%UTIL` column. This indicates how long a lock is held and should be below 15-20%.

Finally, a lot of confusion stems from the `TIER2` and `TIER3` columns. 
Simple, but useful advise, would be to ignore these columns.
