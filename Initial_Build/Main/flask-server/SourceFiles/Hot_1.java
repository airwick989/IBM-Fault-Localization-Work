public class Hot_1{
  public static void main (String[] args){
    //threads,sleeptime,sleeptype
    int threads=Integer.parseInt(args[0]);
    int sleepTime=Integer.parseInt(args[1]);
    for (int x=0;x<threads;x++){
      Hot a= new Hot(threads,sleepTime, 0);
      a.start();
    }
  }
}
class Hot extends Thread{
  int threads, sleepTime;
  int sleepType; // 0 indicates ms, and 1 indicates nanos

  public static Object dataBase=new Object();
  public Hot(int t, int s, int type){
    threads=t;
    sleepTime=s;
    sleepType=type;
  }
  public void run(){
    while(true){
      retriveData(0);
    }
  }
  public Object retriveData(int ID) {
   try{
    long threadId = Thread.currentThread().getId();
    synchronized (dataBase) {
      if(sleepType == 0){
          Thread.sleep(sleepTime);
      }else {
        Thread.sleep(0, sleepTime);
      }
      System.out.println("Task done from thread : " + threadId);
    }
  }catch(Exception e){}
   //return dataBase.get(ID)
   return null;
 }
}
