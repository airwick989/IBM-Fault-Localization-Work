import java.io.*;
import java.io.*;
public class test{
  public static void main(String args[]){
    synchronized(test.class){
      System.out.println("hi");
    }
  }
}
class other{
  public static Object lock_1=new Object();
  public static Object lock_2=new Object();
  public static Object lock_3=new Object();
  public void A(int x){
    synchronized(lock_1){
      synchronized(lock_2){
        synchronized(lock_3){
          System.out.println(x);
        }
        synchronized(lock_3){
          System.out.println("abc");
        }
      }
    }
  }
  public synchronized void B(){
    for(int x=0;x<100;x++){
      //A.update();
      //Thread.sleep(20);
    }
  }
}
