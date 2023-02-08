# General Description
TPROF is a system profiling tool used for system performance analysis. It is useful to know that there is no single tool named TPROF. In fact, what is referred to a TPROF, is a collection of tools working together to carry out the process:
* OS sample provider (eg the Perf API in Linux)
* `SWTRACE`: Trace facility to record data
* `JPROF`: Agent to collect jitted method information (if profiling Java application)
* `A2N`: Symbol resolution facility (to translate addresses to names)
* `POST`: Post-processor (to coordinate post-processing)

The TPROF process is encapsulated in the run.tprof script. The script interacts with the user (it can also run without user interaction) to:
* Activate TPROF instrumentation
* Prompt for when to start and when to stop
* Invoke post-processor to generate report

The process has the same "look and feel" on all platforms. They all provide the run.tprof script and the TPROF reports look the same. As is to be expected, there are minor differences dictated by the platform. Currently TPROF is only supported on Linux (x, p, and z). However, because of the way the in the current PI tools has been structured, adding support for other platforms is more managable as it only involves implementing the means to interface with the platform's OS sample provider.

The output file tprof.out can be consumed as is or can be analyzed using Visual Performance Analyzer (VPA).

In order to run TPROF as a non-privileged user on Linux:
```
$> su
$> echo -1 > /proc/sys/kernel/perf_event_paranoid
$> exit
```

# Usage
#### Terminal 1 (java)
1. `cd $PI_DIR/Dpiperf/bin`
2. `. setrunenv`
3. `cd $BENCHMARK_DIR`

#### Terminal 2 (tprof)
1. `cd $PI_DIR/Dpiperf/bin`
2. `. setrunenv`
3. `cd $OUTPUT_FILES_DIR`

#### Terminal 1
4. `java -agentlib:jprof=tprof,logpath=$OUTPUT_FILES_DIR <rest of java args>`

#### Terminal 2
4. `$PI_DIR/Dpiperf/bin/run.tprof <tprofoptions>`

# Syntax
```
    run.tprof: Coordinates the TPROF procedure

    Syntax:
    -------
      run.tprof [-a | -t | -p] [-b #] [-m [time | event]] [-s #] [-r #] [-M [fn]]

                Options valid with '-m time':
                   [-c #ticks_per_second]

                Options valid with '-m event':
                   [-e event_name] [-c #evts]

                To get help:
                   [-h | -h verbose]              (Linux only)

    Where:
    ------
      -a        Run entire TPROF procedure.
                * !!! THIS IS THE DEFAULT AND NEED NOT BE SPECIFIED !!!
                * Performs the trace and post-processing steps.
                * Produces a raw trace file (swtrace.nrm2) and a TPROF report
                  file (tprof.out).
                * This option is mutually exclusive with -t and -p.

      -t        Run tracing only.
                * Performs only the tracing (ie. data collection) step of the
                  TPROF procedure.
                * Produces a raw trace file (swtrace.nrm2).
                * The trace will not be post-processed. You can post-process
                  the trace at a later time by invoking run.tprof with the
                  -p option.
                * This option is mutually exclusive with -a and -p.

      -p        Run post-processing only.
                * Performs only the post-processing step of the TPROF procedure.
                * It requires a trace file (swtrace.nrm2) and assumes that you've
                  already done the tracing step, either by itself (via a previous
                  -t) or in a previous complete run (via -a).
                * Produces a TPROF report file (tprof.out).
                * This option is mutually exclusive with -a and -t.

      -b #      Set trace buffer size to # MB **PER PROCESSOR**

      -m mode   Set the TPROF mode.
                * Valid modes are:
                  - TIME
                      This causes TPROF to run in TIME-based mode.
                      In this mode sampling is based on time and the time between
                      samples is constant. The sampling rate can set via the -c
                      option.
                  - EVENT
                      This causes TPROF to run in EVENT-based mode.
                      In this mode sampling is based on event counts and the time
                      between samples is not constant. The sampling event count
                      can set via the -c option.
                * Default mode is TIME.

      -e event  Set event name when in EVENT-based mode.
                * 'event' is the name of an event. Valid event names can be
                  obtained by entering the "SWTRACE EVENT LIST" command.
                  - Event names are not case sensitive. INSTR is the same as
                    Instr or iNStr or instr or ...
                  - An event count can be set using the -c option.
                * Default event name is INSTR.

      -c #evts  Set tick rate (TIME-based) or event count (EVENT-based).
                * If mode is TIME then #evts represents the desired TPROF tick
                  rate, in ticks/second.
                  - Default tick rate is 100 ticks/second.
                * If mode is EVENT then #evts represent the number of events
                  after which a TPROF tick will occur.
                  - Default event count is 10,000,000 events.
                * Setting the rate too high may affect overall system performance.
                * Setting the rate too low may not collect enough samples.
                * Set a rate that is neither too high nor too low :-)

      -s #      Automatically start tracing after approximately # seconds.
                * You WILL NOT be prompted for when to start tracing. Instead,
                  tracing will start automatically after a delay of approximately
                  # seconds.
                * If you DO NOT specify the -r option, you will be prompted for
                  when to stop tracing.
                * If you specify 0 (zero), or a non-numeric value, tracing is
                  started immediately.
                * If you use this option your application should be started and
                  warmed up by the time tracing is started.
                * Only applicable with -a or -t. Ignored otherwise.
                * Default is to prompt for when to start tracing.

      -r #      Run (trace) for approximately # seconds.
                * You WILL NOT be prompted for when to stop tracing. Instead,
                  tracing will stop automatically after a delay of approximately
                  # seconds from the time tracing was started.
                * If you DO NOT specify the -s option, tracing is stopped
                  approximately # seconds after you press ENTER to start the trace.
                * If you specify 0 (zero), or a non-numeric value, tracing is
                  stopped immediately after being started.
                * If you use this option your application should complete the
                  scenario you want to trace in the approximately the amount of
                  time you specify with this option.
                * Only applicable with -a or -t. Ignored otherwise.
                * Default is to prompt for when to stop tracing.

      -M [fn]   Combine the tprof.out, tprof.micro, log-jita2n*, log-jtnm* and
                post.show files into a single file.
                * Done if no errors are encountered generating the TPROF report.
                  environment variables in the toolsenv.cmd configuration file.
                * If fn is specified that will be the name of the combined file.
                * Default fn is: tprof_e.out.  fn can not be specified on Linux.

    Notes:
    ------
      * Options are parsed from left to right. If you specify an option more
        than once, the rightmost option is used.
      * -a, -t and -p are exclusive options: only one of them is allowed.
      * The meaning of -c varies depending on the mode. It is assumed to be a
        rate (ticks/second) in TIME-based mode, or an event count in EVENT-based
        mode.
      * -e is ignored if the current mode is TIME.
      * -s and -r are useful if you are automating the TPROF procedure and have
        an idea of how long it takes for the scenario to start up (-s option)
        and how long you want to TPROF for (-r option).
      * Time delays specified with -s and -r are approximate. Specifying 5
        doesn't mean exactly 5 seconds. It will be about 5 seconds.
      * Valid option combinations:
        No options       Run entire procedure. Prompt to start and stop trace.
        -a               Same as no options.
                         Trace start/stop controlled by -s/-r.
        -t               Trace only. No post-processing.
                         Trace start/stop controlled by -s/-r.
        -p               Post-process only. No tracing.

        No -s and no -r  Prompt to start and prompt stop tracing.
        -s #             Automatically start tracing in # seconds.
                         Prompt to stop.
        -r #             Prompt to start tracing. Automatically stop tracing
                         approximately # seconds after tracing started.
        -s # -r #        Automatically start tracing in approximately # seconds.
                         Automatically stop tracing approximately # seconds
                         after tracing started.
      
    Examples:
    ---------
      1) run.tprof
         Runs the entire TPROF procedure. You are prompted to start and stop
         the trace. Generates TPROF report.
         Same as:  'run.tprof -a'

      2) run.tprof -s 10 -r 60
         Runs the entire TPROF procedure. Tracing starts automatically after
         about a 10 seconds delay. Tracing stops automatically about 60 seconds
         after being started. Generates TPROF report. There are no prompts -
         everything happens automatically.
         Same as: 'run.tprof -a -s 10 -r 60'

      3) run.tprof -t
         Runs the trace step of the TPROF procedure. You are prompted to start
         and stop the trace. Does not generate a
         TPROF report.

      4) run.tprof -t -s 10 -r 60
         Runs the trace step of the TPROF procedure. Tracing starts automatically
         after about a 10 seconds delay. Tracing stops automatically about 60
         seconds after being started. There are no prompts - everything happens
         automatically.

      5) run.tprof -t -s 10
         Runs the trace step of the TPROF procedure. Tracing starts automatically
         after about a 10 seconds delay. You are prompted for when to stop tracing.

      6) run.tprof -p
         Runs the post-processing step of the TPROF procedure. You are prompted
         to start and stop the trace. Only generates a TPROF report.

      7) run.tprof -m event -e l2_read_refs -c 100000 s 10 -r 60
         Runs the entire TPROF procedure. Sampling is EVENT-based, using event
         L2_READ_REFS and sampling every 100,000 occurrences. Tracing starts
         automatically after about a 10 seconds delay. Tracing stops
         automatically about 60 seconds after being started. Generates TPROF
         report. There are no prompts - everything happens automatically.

      8) run.tprof -m event -e l2_read_refs -c 100000 s 10 -r 60 -M mt_file
         Same as number 7) above but the mergetprof command is run.
         MERGETPROF output will be in file 'mt_file'.
```

# TPROF output
Results of the TPROF run are saved in a file named tprof.out in the current directory. The report contains several stanzas (sections), each one offering a different view of the distribution of the samples.

* Process
   * Sample totals by process
* Process_Module
   * Sample totals for each module in a process
* Process_Module_Symbol
   * Sample totals by symbol for each module in a process
* Process_Thread
   * Sample totals for each thread in a process
* Process_Thread_Module
   * Sample totals by module for each thread in a process
* Process_Thread_Module_Symbol
   * Sample totals by symbol for each module and thread in a process
* Module
   * Sample totals by module
* Module_Symbol
   * Sample totals by symbol for each module

#### Sample Tprof Output
The following sample report was generated while profiling a "C" application named watch. The application invokes 5 functions (Sixteen, Eight, Four, Two, One) each of which runs (spins) for an amount of time proportional to the value described by the function name. Output from running watch looks like this on my machine:

```
./watch 
Press the ENTER key to start: 
One
Two
Four
Eight
Sixteen
```

The important thing to notice in the report, in the Process_Module_Symbol stanza, is the proportion of samples/ticks in each watch function:
```
   PID  54037 43.06    watch_5401
    MOD  53776 42.85     /home/dsouzai/myWork/pi/pi/Dpiperf/tests/tprof/watch
     SYM  27940 22.26      Sixteen()
     SYM  13612 10.85      Eight()
     SYM   6943  5.53      Four()
     SYM   3500  2.79      Two()
     SYM   1781  1.42      One()
```
* Sixteen should have about twice the number of ticks as Eight (because it runs twice as long).
* Eight should have about twice the number of ticks as Four.
* Four should have about twice the number of ticks as Two.
* Two should have about twice the number of ticks as One.

This is the report generated:

```
 Tprof Reports

 Platform : Linux-x86_64

           Processors               4
       ProcessorSpeed               0
         TraceTime_ns     56148091249
            TraceTime          56.148(sec)

      Time-based mode, ticks every 1000 microseconds (~1000 ticks/sec)


 TOTAL TICKS         125489
 (Clipping Level :   0.0 %    0 Ticks)


   ================================
      TPROF Report Summary

    )) Process
    )) Process_Module
    )) Process_Module_Symbol
    )) Process_Thread
    )) Process_Thread_Module
    )) Process_Thread_Module_Symbol
    )) Module
    )) Module_Symbol
   ================================


   ================================
    )) Process
   ================================

   LAB    TKS   %%%     NAMES

   PID  67269 53.61    SystemProcess_0000
   PID  54037 43.06    watch_5401
   PID   3759  3.00    gnome-shell_05ec
   PID    243  0.19    gnome-terminal-_07b1
   PID     81  0.06    swtrace_5406
   PID     56  0.04    qtcreator_424d
   PID     10  0.01    Xwayland_05fe
   PID      9  0.01    VBoxClient_06c1
   PID      5  0.00    kworker/3:2_4c87
   PID      4  0.00    kworker/1:1_5294
   PID      3  0.00    VBoxService_03e1
   PID      2  0.00    rngd_02fb
   PID      1  0.00    Unknown_001d
   PID      1  0.00    jbd2/dm-0-8_01d6
   PID      1  0.00    dbus-daemon_05bc
   PID      1  0.00    kworker/3:0_53ae
   PID      1  0.00    Unknown_5415
   PID      1  0.00    kworker/u8:0_4628
   PID      1  0.00    Unknown_5414
   PID      1  0.00    rcuos/0_0009
   PID      1  0.00    Unknown_5412
   PID      1  0.00    kworker/0:1H_0180
   PID      1  0.00    Unknown_5413

   ================================
    )) Process_Module
   ================================

   LAB    TKS   %%%     NAMES

   PID  67269 53.61    SystemProcess_0000
    MOD  67268 53.60     vmlinux
    MOD      1  0.00     [e1000]

   PID  54037 43.06    watch_5401
    MOD  53776 42.85     /home/dsouzai/myWork/pi/pi/Dpiperf/tests/tprof/watch
    MOD    261  0.21     vmlinux

   PID   3759  3.00    gnome-shell_05ec
    MOD   2304  1.84     NoModule
    MOD    934  0.74     [ttm]
    MOD    202  0.16     /usr/lib64/dri/kms_swrast_dri.so
    MOD    145  0.12     vmlinux
    MOD     57  0.05     [vboxvideo]
    MOD     28  0.02     /usr/lib64/libc-2.24.so
    MOD     24  0.02     /usr/lib64/libglib-2.0.so.0.5000.1
    MOD     15  0.01     /usr/lib64/libgobject-2.0.so.0.5000.1
    MOD     10  0.01     /usr/lib64/mutter/libmutter-cogl.so
    MOD      9  0.01     /usr/lib64/mutter/libmutter-clutter-1.0.so
    MOD      6  0.00     /usr/lib64/libmozjs-24.so
    MOD      5  0.00     /usr/lib64/libmutter.so.0.0.0
    MOD      4  0.00     /usr/lib64/libpthread-2.24.so
    MOD      3  0.00     /usr/lib64/libgirepository-1.0.so.1.0.0
    MOD      2  0.00     /usr/lib64/libdbus-1.so.3.16.2
    MOD      2  0.00     /usr/lib64/libxcb.so.1.1.0
    MOD      1  0.00     /usr/lib64/ld-2.24.so
    MOD      1  0.00     /usr/lib64/libgdk-3.so.0.2200.2
    MOD      1  0.00     [drm_kms_helper]
    MOD      1  0.00     [drm]
    MOD      1  0.00     /usr/lib64/libgtk-3.so.0.2200.2
    MOD      1  0.00     /usr/lib64/libpixman-1.so.0.34.0
    MOD      1  0.00     /usr/lib64/libwayland-server.so.0.1.0
    MOD      1  0.00     /usr/lib64/libm-2.24.so
    MOD      1  0.00     /usr/lib64/libX11.so.6.3.0

...

   ================================
    )) Process_Module_Symbol
   ================================

   LAB    TKS   %%%     NAMES

   PID  67269 53.61    SystemProcess_0000
    MOD  67268 53.60     vmlinux
     SYM  66862 53.28      native_safe_halt
     SYM    211  0.17      __do_softirq
     SYM     95  0.08      tick_nohz_idle_enter
     SYM     36  0.03      finish_task_switch
     SYM     23  0.02      _raw_spin_unlock_irqrestore
     SYM     19  0.02      tick_nohz_idle_exit
     SYM      4  0.00      cpu_startup_entry
     SYM      4  0.00      rcu_idle_exit
     SYM      2  0.00      arch_cpu_idle
     SYM      2  0.00      default_idle_call
     SYM      2  0.00      memchr_inv
     SYM      2  0.00      _find_next_bit.part.0
     SYM      2  0.00      load_balance
     SYM      1  0.00      default_idle
     SYM      1  0.00      next_online_pgdat
     SYM      1  0.00      idle_cpu
     SYM      1  0.00      end_page_writeback

    MOD      1  0.00     [e1000]
     SYM      1  0.00      e1000_clean

   PID  54037 43.06    watch_5401
    MOD  53776 42.85     /home/dsouzai/myWork/pi/pi/Dpiperf/tests/tprof/watch
     SYM  27940 22.26      Sixteen()
     SYM  13612 10.85      Eight()
     SYM   6943  5.53      Four()
     SYM   3500  2.79      Two()
     SYM   1781  1.42      One()

    MOD    261  0.21     vmlinux
     SYM    149  0.12      exit_to_usermode_loop
     SYM    109  0.09      __do_softirq
     SYM      1  0.00      load_balance
     SYM      1  0.00      copy_user_generic_string
     SYM      1  0.00      finish_task_switch

   PID   3759  3.00    gnome-shell_05ec
    MOD   2304  1.84     NoModule
     SYM   2304  1.84      NoSymbols

...

   ================================
    )) Process_Thread
   ================================

   LAB    TKS   %%%     NAMES

   PID  67269 53.61    SystemProcess_0000
    TID  67269 53.61     tid_SystemProcess_0000

   PID  54037 43.06    watch_5401
    TID  54037 43.06     tid_watch_5401

   PID   3759  3.00    gnome-shell_05ec
    TID   1180  0.94     tid_gnome-shell_05ec
    TID    649  0.52     tid_llvmpipe-2_05f9
    TID    648  0.52     tid_llvmpipe-3_05fa
    TID    643  0.51     tid_llvmpipe-0_05f7
    TID    639  0.51     tid_llvmpipe-1_05f8

   PID    243  0.19    gnome-terminal-_07b1
    TID    243  0.19     tid_gnome-terminal-_07b1

...

   ================================
    )) Process_Thread_Module
   ================================

   LAB    TKS   %%%     NAMES

   PID  67269 53.61    SystemProcess_0000
    TID  67269 53.61     tid_SystemProcess_0000
     MOD  67268 53.60      vmlinux
     MOD      1  0.00      [e1000]

   PID  54037 43.06    watch_5401
    TID  54037 43.06     tid_watch_5401
     MOD  53776 42.85      /home/dsouzai/myWork/pi/pi/Dpiperf/tests/tprof/watch
     MOD    261  0.21      vmlinux

   PID   3759  3.00    gnome-shell_05ec
    TID   1180  0.94     tid_gnome-shell_05ec
     MOD    931  0.74      [ttm]
     MOD     61  0.05      vmlinux
     MOD     57  0.05      [vboxvideo]
     MOD     28  0.02      /usr/lib64/libc-2.24.so
     MOD     24  0.02      /usr/lib64/libglib-2.0.so.0.5000.1
     MOD     15  0.01      /usr/lib64/libgobject-2.0.so.0.5000.1
     MOD     15  0.01      /usr/lib64/dri/kms_swrast_dri.so
     MOD     10  0.01      /usr/lib64/mutter/libmutter-cogl.so
     MOD      9  0.01      /usr/lib64/mutter/libmutter-clutter-1.0.so
     MOD      6  0.00      /usr/lib64/libmozjs-24.so
     MOD      5  0.00      /usr/lib64/libmutter.so.0.0.0
     MOD      3  0.00      /usr/lib64/libgirepository-1.0.so.1.0.0
     MOD      2  0.00      /usr/lib64/libdbus-1.so.3.16.2
     MOD      2  0.00      /usr/lib64/libxcb.so.1.1.0
     MOD      2  0.00      NoModule
     MOD      1  0.00      /usr/lib64/libgdk-3.so.0.2200.2
     MOD      1  0.00      /usr/lib64/ld-2.24.so
     MOD      1  0.00      [drm_kms_helper]
     MOD      1  0.00      /usr/lib64/libpthread-2.24.so
     MOD      1  0.00      [drm]
     MOD      1  0.00      /usr/lib64/libgtk-3.so.0.2200.2
     MOD      1  0.00      /usr/lib64/libpixman-1.so.0.34.0
     MOD      1  0.00      /usr/lib64/libwayland-server.so.0.1.0
     MOD      1  0.00      /usr/lib64/libm-2.24.so
     MOD      1  0.00      /usr/lib64/libX11.so.6.3.0

...

   ================================
    )) Process_Thread_Module_Symbol
   ================================

   LAB    TKS   %%%     NAMES

   PID  67269 53.61    SystemProcess_0000
    TID  67269 53.61     tid_SystemProcess_0000
     MOD  67268 53.60      vmlinux
      SYM  66862 53.28       native_safe_halt
      SYM    211  0.17       __do_softirq
      SYM     95  0.08       tick_nohz_idle_enter
      SYM     36  0.03       finish_task_switch
      SYM     23  0.02       _raw_spin_unlock_irqrestore
      SYM     19  0.02       tick_nohz_idle_exit
      SYM      4  0.00       cpu_startup_entry
      SYM      4  0.00       rcu_idle_exit
      SYM      2  0.00       arch_cpu_idle
      SYM      2  0.00       default_idle_call
      SYM      2  0.00       memchr_inv
      SYM      2  0.00       _find_next_bit.part.0
      SYM      2  0.00       load_balance
      SYM      1  0.00       default_idle
      SYM      1  0.00       next_online_pgdat
      SYM      1  0.00       idle_cpu
      SYM      1  0.00       end_page_writeback

     MOD      1  0.00      [e1000]
      SYM      1  0.00       e1000_clean

   PID  54037 43.06    watch_5401
    TID  54037 43.06     tid_watch_5401
     MOD  53776 42.85      /home/dsouzai/myWork/pi/pi/Dpiperf/tests/tprof/watch
      SYM  27940 22.26       Sixteen()
      SYM  13612 10.85       Eight()
      SYM   6943  5.53       Four()
      SYM   3500  2.79       Two()
      SYM   1781  1.42       One()

     MOD    261  0.21      vmlinux
      SYM    149  0.12       exit_to_usermode_loop
      SYM    109  0.09       __do_softirq
      SYM      1  0.00       load_balance
      SYM      1  0.00       copy_user_generic_string
      SYM      1  0.00       finish_task_switch

   PID   3759  3.00    gnome-shell_05ec
    TID   1180  0.94     tid_gnome-shell_05ec
     MOD    931  0.74      [ttm]
      SYM    924  0.74       ttm_bo_move_memcpy
      SYM      1  0.00       ttm_mem_global_free_zone
      SYM      1  0.00       ttm_pool_unpopulate
      SYM      1  0.00       ttm_mem_global_free_page
      SYM      1  0.00       ttm_tt_unpopulate
      SYM      1  0.00       ttm_pool_populate
      SYM      1  0.00       ttm_mem_global_alloc_page
      SYM      1  0.00       ttm_bo_man_put_node

...

   ================================
    )) Module
   ================================

   LAB    TKS   %%%     NAMES

   MOD  67799 54.03    vmlinux
   MOD  53776 42.85    /home/dsouzai/myWork/pi/pi/Dpiperf/tests/tprof/watch
   MOD   2306  1.84    NoModule
   MOD    934  0.74    [ttm]
   MOD    202  0.16    /usr/lib64/dri/kms_swrast_dri.so
   MOD     87  0.07    /usr/lib64/libpixman-1.so.0.34.0
   MOD     62  0.05    /usr/lib64/libc-2.24.so
   MOD     57  0.05    [vboxvideo]
   MOD     52  0.04    /usr/lib64/libglib-2.0.so.0.5000.1
   MOD     28  0.02    /usr/lib64/libgobject-2.0.so.0.5000.1

...

   ================================
    )) Module_Symbol
   ================================

   LAB    TKS   %%%     NAMES

   MOD  67799 54.03    vmlinux
    SYM  66862 53.28     native_safe_halt
    SYM    362  0.29     __do_softirq
    SYM    163  0.13     exit_to_usermode_loop
    SYM     95  0.08     tick_nohz_idle_enter
    SYM     72  0.06     _raw_spin_unlock_irqrestore
    SYM     54  0.04     finish_task_switch
    SYM     19  0.02     tick_nohz_idle_exit
    SYM     13  0.01     smp_call_function_many
    SYM     10  0.01     do_sys_poll
    SYM      9  0.01     __do_page_fault
    SYM      8  0.01     __fget
    SYM      5  0.00     _raw_spin_lock
    SYM      5  0.00     clear_page
    SYM      4  0.00     cpu_startup_entry
    SYM      4  0.00     __fget_light
    SYM      4  0.00     rcu_idle_exit
    SYM      3  0.00     unmap_page_range
    SYM      3  0.00     load_balance
    SYM      3  0.00     mutex_lock
    SYM      3  0.00     copy_user_generic_string

...

```
