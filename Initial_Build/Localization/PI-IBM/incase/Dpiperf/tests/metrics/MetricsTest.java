import java.lang.*;

public class MetricsTest {

   public static boolean cont;

   public static int runTest(int value) {

      int a = value;
      int b = 0;

      value = value + 1024;
      b = a * 2 + value;
      value = value + a % 10;
      
      if (b < value)
         value = value + 1;
      else
         value = value - 1;

      return a + b + value;

   }


   public static void main (String[] args) throws Exception {

      cont = false;

      int value = 0;

      // First Time
      value = runTest(value);

      // Second Time, JIT should compile
      value = runTest(value);

      while (cont == false) {
         String cmd = System.console().readLine();
         cont = cmd.equals("cont");
      }

      // Third Time, with metrics
      value = runTest(value);

   }

}
