import java.io.*;
import java.io.*;
public class test_2{
  public static void main(String args[]){
    synchronized(test_2.class){
      System.out.println("hi");
    }
  }
}

class more{
  public synchronized void C(){
    while(true){
      //things
    }
  }
}
