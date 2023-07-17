package com.mycompany.app;

import java.util.ArrayList;
import java.util.HashMap;
import java.io.*;
//java -cp "target/dependency/*:target/my-app-1.0-SNAPSHOT.jar" com.mycompany.app.App false false null null
//                                                                                    output read data
//                                                                                          output critical section key list
//                                                                                                 class name (if null process all)
//                                                                                                       method name
public class App
{
    public static void main( String[] args )
    {
      //System.out.println( "Hello World!" );
      read read=new read(Boolean.parseBoolean(args[0]));
      analyze analyze=new analyze(read);
      read.run();
      Tools tools=new Tools();
      //tools.methodPrintoutInline("", read.base);

      if(Boolean.parseBoolean(args[1])){
        try{
          PrintStream stdout = System.out;
          System.setOut(new PrintStream(new File("keys.txt")));
          Object[] sections=read.out.keySet().toArray();
          for(int x=0;x<read.out.size();x++){
              System.out.println(sections[x]);
          }
          System.setOut(stdout);
        }
        catch(IOException e){System.out.println(e);}
      }
      else{
        if(args[2].equals("null")){
          int[] results = new int[7];
          Object[] sections=read.out.keySet().toArray();
          for(int x=0;x<read.out.size();x++){
            System.out.println(x+"/"+(read.out.size()-1));
            //System.out.println("name: "+sections[x]+"\t"+read.methods.get(sections[x])+"\t"+read.locations.get(sections[x]));
            int[] data=analyze.begin((String)sections[x],
              (ArrayList)read.out.get(sections[x]),
              ((ArrayList)read.locations.get(sections[x])),
              ((ArrayList)read.methods.get(sections[x])),
              ((ArrayList)read.structurePlace.get(sections[x])),
              read.out.keySet(),
              read.base);
            for(int y=0;y<results.length;y++){
              results[y]+=data[y];
            }

          }
          for(int y=0;y<results.length;y++){
            System.out.print(results[y]+" ");
          }
        }
        else{
          System.out.println(args[2]+"\t"+args[3]);
          Object[] sections=read.out.keySet().toArray();
          for(int x=0;x<read.out.size();x++){
            if(((ArrayList)read.locations.get(sections[x])).contains(args[2]) && ((ArrayList)read.methods.get(sections[x])).contains(args[3])){
              analyze.begin((String)sections[x],
              (ArrayList)read.out.get(sections[x]),
              ((ArrayList)read.locations.get(sections[x])),
              ((ArrayList)read.methods.get(sections[x])),
              ((ArrayList)read.structurePlace.get(sections[x])),
              read.out.keySet(),
              read.base);
            }
          }
        }
      }
    }
}
