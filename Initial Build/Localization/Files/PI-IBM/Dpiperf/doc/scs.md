# General Description
Callstack sampling uses timer ticks or other events in the same way as 
time profiling (TPROF). However, instead of merely recording the address 
being executed, it calls the JVM to record the entire java callstack for that 
thread. By treating this callstack as if it were a series of method 
entries and exits, the information can be stored in the same way that 
callflow information is stored. This allows callflow analysis of an 
application with far less overhead than would be encountered by 
handling all of the method entry and exit events.

# Usage
1. Start your Java application with the following command

   `java -agentlib:jprof=scs[rest of scs options] classfile`

2. For runtime control,  you must execute `rtdriver -l` in another terminal
   * `start` - Starts data collection.
   * `end` - Ends data collection, writes the data that has been collected, then resets all internal buffers.
   * `flush` - Writes the data that has been collected, but does not end or reset data collection.

3. Output is in `log-rt.1_pid`

4. View the results with `mxview`

   `java -jar $PI_DIR/Dpiperf/tools/mxview.jar log-rt.1_pid`

# Detailed Usage
#### Terminal 1 (java)

1. Go to the PI Installation directory:

   `cd $PI_DIR/Dpiperf/bin`

2. Set the environment variables:

   `. setrunenv`

3. Go to the java application directory:

   `cd $BENCHMARK_DIR`

4. Start the java application:

   `java -agentlib:jprof=scs[=<event>[<event options>][=nn][+]][,hr][,logpath=path][,start] <rest of java args>`

   SCS options details are as follows:
   * `scs` - generates callstacks on JVM threads using a timer based sampling mechanism
   * `scs=<event>` - generates callstacks based on the following events:
      * `allocated_bytes` - all allocations
      * `allocations[=mm:nn]` - every `mm allocations or after `nn` or more bytes have been allocated
      * `monitor_wait` - monitor wait
      * `monitor_waited` - monitor waited
      * `monitor_contended_enter` - contented monitor enter
      * `monitor_contended_entered` - contended monitor entered
      * `class_load` - class load  
   * The following options can used with `scs=<events>`:
      * `=nn` - sample callstacks after `nn` events
      * `+` - display object classes associated with the events
   * `objectinfo` - indicates that we are interested in tracking both object allocations and frees. It also  eliminates the distinction between interpreted and compiled methods.
   * `sobjs` - separates objects of the same class by size. 
   * `hr` - when used with the + option, improves readability by adding extra blank lines around the lines describing the object classes associated with the events.
   * `start` - begins data collection immediately, instead of waiting until `start` is received from `rtdriver`
   * `logpath=path` sets the path directory as the location for the JPROF output files

#### Terminal 2 (rtdriver)
1. Start the rtdriver

   `./rtdriver -l`

   This command will cause rtdriver to attempt to connect on the local machine and then enter command mode.
2. Start collecting SCS data (unless `start` was specified to `JPROF`)

   `start`
3. Stop collecting SCS data

   `end`

#### Terminal 1
5. Stop java program
6. View results with `mxview`

   `java -jar $PI_DIR/Dpiperf/tools/mxview.jar log-rt.1_pid`

# Examples
#### Java Object Profiling with Reduced Call Tree
`java -agentlib:jprof=scs=allocations[=mm,nn],objectinfo[,hr][,sobjs][,start][,logpath=path] <rest of java args>`

Output:
```
 Base Values:

  AO         :: Callstack Allocated Objects
  AB         :: Callstack Allocated Bytes
  LO         :: Callstack Live Objects
  LB         :: Callstack Live Bytes

 Column Labels:

  LV         :: Level of Nesting       (Call Depth)
  AO         :: Callstack Allocated Objects
  AB         :: Callstack Allocated Bytes
  LO         :: Callstack Live Objects
  LB         :: Callstack Live Bytes
  NAME       :: Name of Method or Thread

 Elapsed Cycles  : 28167127438 (10.46 sec)

 Process_Number_ID=33972

  LV         AO         AB         LO         LB  NAME

   0          0          0          0          0  000084b5_main

   1        393      12576        393      12576  (java/lang/Class)
   1          1         16          0          0  (java/lang/String)
   1          1         40          0          0  (CHAR[])

   1          0          0          0          0  java/lang/ClassLoader.loadClass(Ljava/lang/String;)Ljava/lang/Class;
   2          0          0          0          0  sun/misc/Launcher$AppClassLoader.loadClass(Ljava/lang/String;Z)Ljava/lang/Class;
   3          0          0          0          0  java/lang/ClassLoader.loadClass(Ljava/lang/String;Z)Ljava/lang/Class;
   4          0          0          0          0  java/lang/ClassLoader.loadClassHelper(Ljava/lang/String;ZZ)Ljava/lang/Class;
   5          0          0          0          0  java/lang/ClassLoader.getClassLoadingLock(Ljava/lang/String;)Ljava/lang/Object;

   6          5         80          0          0  (java/lang/ClassLoader$ClassNameBasedLock)
   6          5        160          0          0  (java/lang/ClassLoader$ClassNameLockRef)

   6          0          0          0          0  java/util/Hashtable.put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;
   7          0          0          0          0  java/util/Hashtable.addEntry(ILjava/lang/Object;Ljava/lang/Object;I)V
...
```

#### Java Profiling via Monitor Events
`java -agentlib:jprof=scs=monitor_wait[+][=nn][,hr][,start][,logpath=path] <rest of java args>`

Output:
```
 Base Values:

  EVENT      :: Callstack Monitor Event samples

 Column Labels:

  LV         :: Level of Nesting       (Call Depth)
  EVENT      :: Callstack Monitor Event samples
  NAME       :: Name of Method or Thread

 Elapsed Cycles  : 29746470451 (11.04 sec)

 Process_Number_ID=46306

  LV      EVENT  NAME

   0          0  0000b4e3_main
   1          0  spec/jbb/JBBmain.main([Ljava/lang/String;)V
   2          0  spec/jbb/JBBmain.doIt()V
   3          0  spec/jbb/JBBmain.runWarehouse(IIF)Z
   4          0  spec/jbb/JBBmain.DoARun(Lspec/jbb/Company;SII)V
   5          0  spec/jbb/Company.displayResultTotals(Z)V
   6          0  java/lang/Object.wait()V

   7          1  (java/lang/Object)


   0          0  0000b4fd_Thread-8
   1          0  java/lang/Thread.run()V
   2          0  spec/jbb/JBBmain.run()V
   3          0  spec/jbb/TransactionManager.go()V
   4          0  java/lang/Object.wait()V

   5          1  (java/lang/Object)
...
```

#### Callstack Sampling
`java -agentlib:jprof=scs[,start][,logpath=path <rest of java args>`

Output:
```
 Base Values:

  SAMPLES    :: Callstack Samples

 Column Labels:

  LV         :: Level of Nesting       (Call Depth)
  SAMPLES    :: Callstack Samples
  NAME       :: Name of Method or Thread

 Method Types:

 : I = Interp
 : J = Jitted
 : N = Native
 : B = Builtin
 : C = Compiling
 : O = Other
 : M = Method

 Elapsed Cycles  : 17531945114 (6.51 sec)

 Process_Number_ID=46410

  LV    SAMPLES  NAME
   0          0  0000b54b_main
   1          0  I:spec/jbb/JBBmain.main([Ljava/lang/String;)V
   2          0  I:spec/jbb/JBBmain.doIt()V
   3          0  I:spec/jbb/JBBmain.runWarehouse(IIF)Z
   4          0  I:spec/jbb/JBBmain.DoARun(Lspec/jbb/Company;SII)V
   5          0  I:spec/jbb/Company.displayResultTotals(Z)V
   6          0  I:spec/jbb/JBButil.SecondsToSleep(J)V
   7          0  I:java/lang/Thread.sleep(J)V
   8          1  N:java/lang/Thread.sleep(JI)V
   0          2  0000b561_PuWorkerThread
   0          0  0000b566_Thread-8
   1          0  I:java/lang/Thread.run()V
   2          0  I:spec/jbb/JBBmain.run()V
   3         12  I:spec/jbb/TransactionManager.go()V
   4          0  J:spec/jbb/TransactionManager.goManual(ILspec/jbb/TimerData;)J
   5          3  J:spec/jbb/TransactionManager.runTxn(Lspec/jbb/Transaction;JJD)J
   6          3  J:spec/jbb/CustomerReportTransaction.processTransactionLog()V
   7          0  J:spec/jbb/infra/Util/TransactionLogBuffer.putDate(Ljava/util/Date;III)V
   8          1  J:java/util/Calendar.setTime(Ljava/util/Date;)V
   9          0  J:java/util/Calendar.setTimeInMillis(J)V
  10          0  J:java/util/GregorianCalendar.computeFields()V
  11          4  J:java/util/Calendar.setFieldsComputed(I)V
  11          4  J:java/util/GregorianCalendar.computeFields(II)I
   7          2  J:spec/jbb/infra/Util/XMLTransactionLog.populateXML(Lspec/jbb/infra/Util/TransactionLogBuffer;)V
   8          1  J:spec/jbb/infra/Util/XMLTransactionLog.putLine(Ljava/lang/String;I)V
   9          0  J:org/apache/xerces/dom/NodeImpl.appendChild(Lorg/w3c/dom/Node;)Lorg/w3c/dom/Node;
  10          0  J:org/apache/xerces/dom/ParentNode.insertBefore(Lorg/w3c/dom/Node;Lorg/w3c/dom/Node;)Lorg/w3c/dom/Node;
  11          1  J:org/apache/xerces/dom/ParentNode.internalInsertBefore(Lorg/w3c/dom/Node;Lorg/w3c/dom/Node;Z)Lorg/w3c/dom/Node;
  12          1  J:org/apache/xerces/dom/CoreDocumentImpl.isKidOK(Lorg/w3c/dom/Node;Lorg/w3c/dom/Node;)Z
   7          1  J:spec/jbb/infra/Util/XMLTransactionLog.clear()V
   8          0  J:org/apache/xerces/dom/ParentNode.removeChild(Lorg/w3c/dom/Node;)Lorg/w3c/dom/Node;
   9          2  J:org/apache/xerces/dom/ParentNode.internalRemoveChild(Lorg/w3c/dom/Node;Z)Lorg/w3c/dom/Node;
   7          5  J:spec/jbb/infra/Util/TransactionLogBuffer.clearBuffer()V
   7          0  J:spec/jbb/CustomerReportTransaction.setupCustomerLog()V
   8          1  J:spec/jbb/infra/Util/TransactionLogBuffer.putText(Ljava/lang/String;III)V
   7          0  J:spec/jbb/infra/Util/TransactionLogBuffer.putTime(Ljava/util/Date;III)V
   8          1  J:spec/jbb/infra/Util/TransactionLogBuffer.privIntLeadingZeros(IIII)I
   7          0  J:spec/jbb/infra/Util/TransactionLogBuffer.putDollars(Ljava/math/BigDecimal;III)V
   8          1  J:spec/jbb/infra/Util/TransactionLogBuffer.putText(Ljava/lang/String;IIIS)V
   6          1  J:spec/jbb/NewOrderTransaction.process()Z
   7          3  J:spec/jbb/Order.processLines(Lspec/jbb/Warehouse;SZ)Z
   8          2  J:java/math/BigDecimal.setScale(II)Ljava/math/BigDecimal;
   9          2  J:java/math/BigDecimal.divide(Ljava/math/BigDecimal;II)Ljava/math/BigDecimal;
   8          0  J:spec/jbb/Orderline.validateAndProcess(Lspec/jbb/Warehouse;)Z
   9          4  J:spec/jbb/Orderline.process(Lspec/jbb/Item;Lspec/jbb/Stock;)V
...
```
