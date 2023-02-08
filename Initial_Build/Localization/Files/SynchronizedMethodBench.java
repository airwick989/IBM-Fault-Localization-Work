package net.adoptopenjdk.bumblebench.examples;

import net.adoptopenjdk.bumblebench.core.MiniBench;

public final class SynchronizedMethodBench extends MiniBench {

        public static long next;

        public static long computeFib(long num) {
            long prev = 0, curr = 1;
            while (num-- > 0) {
                synchronized (SynchronizedMethodBench.class) {
                  next = prev + curr;
                  prev = curr;
                  curr = next;
                  if (pauseDuration > 0) {
                    try {
                      Thread.sleep(0, pauseDuration);
                      // Thread.sleep(pauseDuration, 0);
                    } catch (InterruptedException e) {
                    }
                  }
                }
            }
            return prev;
        }

        public static long computeFib2(long num) {
            long prev = 0, curr = 1;
            // while (num-- > 0) {
                synchronized (SynchronizedMethodBench.class) {
                  while(num-- > 0){
                    next = prev + curr;
                    prev = curr;
                    curr = next;
                    if (pauseDuration > 0) {
                      try {
                        Thread.sleep(0, pauseDuration);
                        // Thread.sleep(pauseDuration, 0);
                      } catch (InterruptedException e) {
                      }
                    }
                  }
                }
            // }
            return prev;
        }

	// Illustrates a MicroBench with an infinite score, for testing purposes

        private volatile long result;

        protected int maxIterationsPerLoop() { return maxIterations; }

      	protected long doBatch(long numLoops, int numIterationsPerLoop) throws InterruptedException {
            startTimer();
            for (long i = 0; i < numLoops; ++i) {
                result = computeFib(numIterationsPerLoop);
                // result = computeFib2(numIterationsPerLoop);
            }
            pauseTimer();
        		return numLoops * numIterationsPerLoop;
      	}

        private static int maxIterations;
        private static int pauseDuration;
        static {
             String iters = System.getProperty("BumbleBench.iterationCount");
             // maxIterations = 9072;
             // System.out.println(iters);
             if (iters != null) {
                try {
                    maxIterations = Integer.parseInt(iters);
                    System.out.println("Max Iterations:" + maxIterations);
                } catch (NumberFormatException e) {
                }
             }
             String pause = System.getProperty("BumbleBench.pauseDuration");
             // pauseDuration = 500;
             // System.out.println(pause);
             if (pause != null) {
                try {
                    pauseDuration = Integer.parseInt(pause);
                    System.out.println("Pause Duration: " + pauseDuration);
                } catch (NumberFormatException e) {
                }
             }
        }

}
