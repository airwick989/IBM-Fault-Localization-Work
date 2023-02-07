public class DeepStackTraceTestThree extends ScsTestCase {

   public String identity() {
      return "DeepStackTraceTestThree";
   }

   public int runTest() {
      return DSTTThreeL1();
   }

   public int DSTTThreeL1() {
      return DSTTThreeL2();
   }

   public int DSTTThreeL2() {
      return DSTTThreeL3();
   }

   public int DSTTThreeL3() {
      return DSTTThreeL4();
   }

   public int DSTTThreeL4() {
      return DSTTThreeL5();
   }

   public int DSTTThreeL5() {
      return DSTTThreeL6();
   }

   public int DSTTThreeL6() {
      return DSTTThreeL7();
   }

   public int DSTTThreeL7() {
      return DSTTThreeL8();
   }

   public int DSTTThreeL8() {
      return DSTTThreeL9();
   }

   public int DSTTThreeL9() {
      return DSTTThreeL10();
   }

   public int DSTTThreeL10() {
      return DSTTThreeLL();
   }

   public int DSTTThreeLL() {
      int i = 0;
      while (i < 10000000) {
         i++;
      }

      return 3;
   }

}

