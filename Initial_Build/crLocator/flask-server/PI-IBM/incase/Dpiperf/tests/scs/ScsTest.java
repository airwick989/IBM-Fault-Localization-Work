import java.lang.*;
import java.util.Random;


public class ScsTest implements Runnable {

   private static int numThreads;
   private static int verbose;
   private static boolean stop;

   private static final int NUM_ARGS = 2;

   public static void runTest(int iter) {
      Random rand = new Random();
      int dart = rand.nextInt(100);
      ScsTestCase scsTestCase;

      if (dart < 20) {
         scsTestCase = new DeepStackTraceTestOne();
      } else if (dart < 50) {
         scsTestCase = new DeepStackTraceTestTwo();
      } else {
         scsTestCase = new DeepStackTraceTestThree();
      }

      int rc = scsTestCase.runTest();

      if (iter%1000000 == 0 && verbose > 0) {
         System.out.println(scsTestCase.identity() + " rc=" + Integer.toString(rc));
      }

   }

   public static void main (String[] args) throws Exception {

      numThreads = 0;
      verbose = 0;
      stop = false;

      if (args.length == NUM_ARGS) {
         try {
            numThreads = Integer.parseInt(args[0]);

            verbose = Integer.parseInt(args[args.length - 1]);
         } catch (NumberFormatException e) {
            System.err.println("Invalid Arguments");
         }
      } else {
         System.err.println("Incorrect number of arguments\n");
         System.exit(1);
      }

      Thread[] threadArray = new Thread[numThreads];

      for (int i = 0; i < threadArray.length; i++) {
         threadArray[i] = new Thread(new ScsTest());
      }

      for (int i = 0; i < threadArray.length; i++) {
         threadArray[i].start();
      }

      while (stop == false) {
         String cmd = System.console().readLine();
         stop = cmd.equals("stop");
      }

      System.out.println("Stopping");

		for (int i = 0; i < threadArray.length; i++) {
         threadArray[i].join();
      }

   }

   public void run() {

      int iter = 0;
      while (stop == false) {
         runTest(iter);
         iter++;
      }

      // Delay
      try {
         Thread.sleep(2000);
      } catch (InterruptedException e) {
         // Do Nothing
      }
   }

}
