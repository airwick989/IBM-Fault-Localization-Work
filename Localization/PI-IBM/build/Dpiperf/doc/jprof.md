# General Description
JPROF is a Java profiling agent that responds to events from the JVM Tool 
Interface (JVMTI), based on the invocation options. It is the core engine of
PI tools such as SCS, JLM, and Method Entry/Exit Tracing. It is also used to
help TPROF get symbols for JIT'd methods. It is controlled either via the 
Java Invocation Parameters or the RTDRIVER commands.

# Usage
`java -agentlib:jprof[=<tool>[addition options]`

# Tools
#### Tprof
Refer to the documentation in `tprof.md`

#### JLM
Refer to the documentation in `jlm.md`

#### SCS
Refer to the documentation in `scs.md`

#### Method Entry/Exit Tracing
Jprof provides the following tools for method entry/exit tracing:
* `calltree` - Generates the java method call tree
* `rtarcf` - Generates the java method call tree along with the number of instructions
* `callflow` - Generates the java method call tree along with the number of cycles and instructions

The following options can be used in conjunction with these tools above:
* `start` - Starts collecting data immediately
* `hr` - Generates human readable output
* `objectinfo` - indicates that we are interested in tracking both object allocations and frees. It also  eliminates the distinction between interpreted and compiled methods.
* `sobjs` - separates objects of the same class by size.

Example Output for `java -agentlib:jprof=calltree,start,objectinfo,hr`:
```
 Base Values:

  NOMETRICS  :: Counter of Events Received
  AO         :: Callflow Allocated Objects
  AB         :: Callflow Allocated Bytes
  LO         :: Callflow Live Objects
  LB         :: Callflow Live Bytes

 Column Labels:

  LV         :: Level of Nesting       (Call Depth)
  CALLS      :: Calls to this Method   (Callers)
  CEE        :: Calls from this Method (Callees)
  NOMETRICS  :: Counter of Events Received
  AO         :: Callflow Allocated Objects
  AB         :: Callflow Allocated Bytes
  LO         :: Callflow Live Objects
  LB         :: Callflow Live Bytes
  NAME       :: Name of Method or Thread

 Elapsed Cycles  : Unknown (NOMETRICS specified)

 Process_Number_ID=46871

  LV      CALLS        CEE  NOMETRICS         AO         AB         LO         LB  NAME

   0          0         20          0          0          0          0          0  0000b718_main

   1          0          0          0        394      25216        394      25216  (java/lang/Class)
   1          0          0          0       1346      21536        933      14928  (java/lang/String)
   1          0          0          0       2066     159832       1431      83800  (CHAR[])
   1          0          0          0        394       6304        110       1760  (java/lang/J9VMInternals$ClassInitializationLock)
   1          0          0          0          1         16          1         16  (java/lang/String$CaseInsensitiveComparator)
   1          0          0          0          1         16          1         16  (com/ibm/jit/DecimalFormatHelper)
   1          0          0          0          1         16          1         16  (com/ibm/jit/JITHelpers)
   1          0          0          0         74       3120         60       2160  ([]java/lang/String)
   1          0          0          0          9        360          7        280  (java/util/HashMap)
   1          0          0          0          8        832          4        288  ([]java/util/HashMap$Node)
   1          0          0          0         19        456         16        384  (java/util/HashMap$Node)
   1          0          0          0          1         16          1         16  (sun/misc/Unsafe)
   1          0          0          0          9        144          8        128  (java/lang/Object)
...

```
