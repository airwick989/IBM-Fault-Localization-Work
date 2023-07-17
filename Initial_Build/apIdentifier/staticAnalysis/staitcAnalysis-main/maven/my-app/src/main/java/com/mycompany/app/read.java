package com.mycompany.app;
//mvn clean install
//mvn dependency:copy-dependencies
//mvn package
//run normally
//java -cp "target/dependency/*;target/my-app-1.0-SNAPSHOT.jar" com.mycompany.app.App false false null null
//get keys
//java -cp "target/dependency/*;target/my-app-1.0-SNAPSHOT.jar" com.mycompany.app.App false true
//dot -Tpng ast.dot > ast.png

import java.io.*;
import java.util.Arrays;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.ArrayList;
import java.util.List;
import java.util.HashMap;

import com.github.javaparser.ast.CompilationUnit;
import com.github.javaparser.ast.Node.BreadthFirstIterator;
import com.github.javaparser.ast.Node;

import com.github.javaparser.*;
import com.github.javaparser.printer.YamlPrinter;
import com.github.javaparser.printer.DotPrinter;



class read{
  public static HashMap out =new HashMap<String, ArrayList<Node>>();
  public static HashMap locations =new HashMap<String, ArrayList<String>>();
  public static HashMap methods =new HashMap<String, ArrayList<String>>();
  public static HashMap structurePlace =new HashMap<String, ArrayList<fileStructure>>();
  public static fileStructure base;
  static boolean print;
  public read(){
    print=false;
  }
  public read(boolean output){
    print=output;
  }

  public static String upToClass(Node look){
    while(true){
      if (look instanceof com.github.javaparser.ast.body.ClassOrInterfaceDeclaration){
        String[] name=((com.github.javaparser.ast.body.ClassOrInterfaceDeclaration)look).getName().toString().split("\n");
        return name[name.length-1];
      }
      look=look.getParentNode().get();
    }
  }
  public static String upToMethod(Node look){
    while(true){
      //System.out.println(look);
      String[] name;
      if (look instanceof com.github.javaparser.ast.body.MethodDeclaration){
        name= ((com.github.javaparser.ast.body.MethodDeclaration)look).getName().toString().split("\n");
        return name[name.length-1];
      }
      else if(look instanceof com.github.javaparser.ast.body.ConstructorDeclaration){
        name= ((com.github.javaparser.ast.body.ConstructorDeclaration)look).getName().toString().split("\n");
        return name[name.length-1];
      }
      look=look.getParentNode().get();
    }
  }
  public static fileStructure readThrough (File base){
    fileStructure F=new fileStructure();
    File[] listOfFiles = base.listFiles();
    for (File file : listOfFiles) {
      if (file.isDirectory()){
        //System.out.println(file);
        fileStructure newFolder=readThrough(file);
        if(newFolder.countSize()>0){
          newFolder.name=file.getName();
          newFolder.parent=F;
          F.subFolders.add(newFolder);
        }
      }
      else{
        String[] name=file.toString().split("\\.");
        String type=name[name.length-1];
        if(type.equals("java")){
          System.out.println(file);
          String line,lines="";
          CompilationUnit cu;
          try{
            BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(file)));
            while ((line = br.readLine()) != null) {
              lines+=line+"\n";
            }
          }
          catch(IOException e){
            System.out.println("error reading file\n"+e);
          }
          //System.out.println(lines);
          cu=StaticJavaParser.parse(lines);
          //System.out.println(cu);
          F.javaFiles.add(cu);
          F.fileNames.add(file.getName());
        }
      }
    }
    return F;
  }
  public static void findSync(fileStructure base){
    for (int x=0;x<base.subFolders.size();x++){
      findSync(base.subFolders.get(x));
    }
    for (int x=0;x<base.javaFiles.size();x++){
      readFile(base.javaFiles.get(x),base);
    }
  }
  private static void readFile(Node node,fileStructure f){
    BreadthFirstIterator bfi = new BreadthFirstIterator(node);
    while (bfi.hasNext()) {
      Node node2 = bfi.next();

      if (node2 instanceof com.github.javaparser.ast.stmt.SynchronizedStmt){
        String key=syncRegion(node2);
        addSync(node2,upToMethod(node2),upToClass(node2),key,f);
      }
      else if (node2 instanceof com.github.javaparser.ast.body.MethodDeclaration){
        if(((com.github.javaparser.ast.body.MethodDeclaration)node2).isSynchronized()){
          String key=syncMethod(node2);
          addSync(node2,upToMethod(node2),upToClass(node2),key,f);
        }
      }
    }
  }
  private static void addSync(Node node, String method, String loc,String key,fileStructure file){
    if (!out.containsKey(key)){
      out.put(key,new ArrayList<Node>());
      locations.put(key,new ArrayList<String>());
      methods.put(key,new ArrayList<String>());
      structurePlace.put(key,new ArrayList<String>());
    }
    ((ArrayList)out.get(key)).add(node);
    ((ArrayList)locations.get(key)).add(loc);
    ((ArrayList)methods.get(key)).add(method);
    ((ArrayList)structurePlace.get(key)).add(file);
    if(print){
      try{
        YamlPrinter printer = new YamlPrinter(true);
        System.out.println(printer.output(node));
        DotPrinter dPrinter = new DotPrinter(true);
        try (FileWriter fileWriter = new FileWriter("images"+File.separator+key.replace(".","_").replace("\\","")+"_"+(((ArrayList)out.get(key)).size()-1)+".dot");
            PrintWriter printWriter = new PrintWriter(fileWriter)) {
            printWriter.print(dPrinter.output(node));
        }
      }
      catch(IOException e){
        System.out.println("Error writing dot files\n"+e);
      }
    }
  }
  private static String syncRegion(Node node){
    //System.out.println(node);
    String monitor = ((com.github.javaparser.ast.stmt.SynchronizedStmt)node).getExpression().toString();
    if (monitor.equals("this")){
      monitor = syncMethod(node);
    }
    return monitor;
  }
  private static String syncMethod(Node node){
      return upToClass(node)+"."+upToMethod(node);
  }
  public static void run(){
    try{
      String file_name="input";
      File folder = new File(file_name);
      //System.out.println(files);
      CompilationUnit cu;
      YamlPrinter printer;
      DotPrinter dPrinter;

      base = readThrough(folder);
      //System.out.println(base.countSize());
      findSync(base);
      //find sync regions and export relevant nodes



      /*for (Object k: m.keySet()){
        ArrayList A=(ArrayList)m.get(k);

        if(print){
          System.out.println(k);
          System.out.println(A);
        }
        Node crit=null;
        for(int a=0;a<A.size();a++){
          try{
            cu=StaticJavaParser.parse("public class BLANK{"+A.get(a).toString()+"}");
            //crit=(cu.getChildNodes().get(0).getChildNodes().get(2).getChildNodes().get(4));
            //crit=(cu.getChildNodes().get(0).getChildNodes().get(2).getChildNodes().get(4).getChildNodes().get(0).getChildNodes().get(1).getChildNodes().get(0));
            //System.out.println(cu);
            crit=cu.getChildNodes().get(0).getChildNodes().get(2);
            crit=crit.getChildNodes().get(crit.getChildNodes().size()-1);
          }
          catch(Exception e){
            cu=StaticJavaParser.parse("public class BLANK{public void A(){"+A.get(a).toString()+"}}");
            //crit=(cu.getChildNodes().get(0).getChildNodes().get(2).getChildNodes().get(3));
            //crit=(cu.getChildNodes().get(0).getChildNodes().get(2).getChildNodes().get(3).getChildNodes().get(0).getChildNodes().get(1).getChildNodes().get(0));
            crit=cu.getChildNodes().get(0).getChildNodes().get(2);
            crit=crit.getChildNodes().get(crit.getChildNodes().size()-1).getChildNodes().get(0).getChildNodes().get(1);
          }


          //System.out.println(crit);

          if (!out.containsKey(k)){
            out.put(k,new ArrayList<Node>());
            locations.put(k,new ArrayList<String>());
            methods.put(k,new ArrayList<String>());
          }
          ((ArrayList)out.get(k)).add(crit);
          ((ArrayList)locations.get(k)).add(upToClass((Segment)A.get(a)));
          ((ArrayList)methods.get(k)).add(upToMethod((Segment)A.get(a)));

          if(print){

            printer = new YamlPrinter(true);
            System.out.println(printer.output(crit));
            dPrinter = new DotPrinter(true);
            try (FileWriter fileWriter = new FileWriter("images/"+k.toString().replace(".","_")+"_"+a+".dot");
                PrintWriter printWriter = new PrintWriter(fileWriter)) {
                printWriter.print(dPrinter.output(crit));
            }
          }

        }
      }*/
      //cu=StaticJavaParser.parse("public class test{        public static void main(String args[]){          synchronized(test.class){            System.out.println(\"hi\");          }        }      }");
      //System.out.println(cu);
      //printer = new YamlPrinter(true);
      //System.out.println(printer.output(cu));

      //dPrinter = new DotPrinter(true);
      //try (FileWriter fileWriter = new FileWriter("ast.dot");
      //    PrintWriter printWriter = new PrintWriter(fileWriter)) {
      //    printWriter.print(dPrinter.output(cu));
      //}

    }
    catch(Exception e){
      e.printStackTrace();
      System.out.println(e);
    }
  }

}
