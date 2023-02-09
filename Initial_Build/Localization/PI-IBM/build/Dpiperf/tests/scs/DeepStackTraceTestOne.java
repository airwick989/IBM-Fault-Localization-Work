public class DeepStackTraceTestOne extends ScsTestCase {

   public String identity() {
      return "DeepStackTraceTestOne";
   }

   public int runTest() {
      return DSTTOneL1();
   }

   public int DSTTOneL1() {
      return DSTTOneL2();
   }

   public int DSTTOneL2() {
      return DSTTOneL3();
   }

   public int DSTTOneL3() {
      return DSTTOneL4();
   }

   public int DSTTOneL4() {
      return DSTTOneL5();
   }

   public int DSTTOneL5() {
      return DSTTOneL6();
   }

   public int DSTTOneL6() {
      return DSTTOneL7();
   }

   public int DSTTOneL7() {
      return DSTTOneL8();
   }

   public int DSTTOneL8() {
      return DSTTOneL9();
   }

   public int DSTTOneL9() {
      return DSTTOneL10();
   }

   public int DSTTOneL10() {
      return DSTTOneLL();
   }

   public int DSTTOneLL() {
      int i = 0;
      while (i < 10000000) {
         i++;
      }

      return 1;
   }

}

