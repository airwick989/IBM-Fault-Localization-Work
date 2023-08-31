public class DeepStackTraceTestTwo extends ScsTestCase {

   public String identity() {
      return "DeepStackTraceTestTwo";
   }

   public int runTest() {
      return DSTTTwoL1();
   }

   public int DSTTTwoL1() {
      return DSTTTwoL2();
   }

   public int DSTTTwoL2() {
      return DSTTTwoL3();
   }

   public int DSTTTwoL3() {
      return DSTTTwoL4();
   }

   public int DSTTTwoL4() {
      return DSTTTwoL5();
   }

   public int DSTTTwoL5() {
      return DSTTTwoL6();
   }

   public int DSTTTwoL6() {
      return DSTTTwoL7();
   }

   public int DSTTTwoL7() {
      return DSTTTwoL8();
   }

   public int DSTTTwoL8() {
      return DSTTTwoL9();
   }

   public int DSTTTwoL9() {
      return DSTTTwoL10();
   }

   public int DSTTTwoL10() {
      return DSTTTwoLL();
   }

   public int DSTTTwoLL() {
      int i = 0;
      while (i < 10000000) {
         i++;
      }

      return 2;
   }

}

