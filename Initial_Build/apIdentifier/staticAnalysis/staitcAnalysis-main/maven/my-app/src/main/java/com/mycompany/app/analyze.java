package com.mycompany.app;

import java.util.*;
import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;

import java.io.*;
import java.io.FileWriter;

import com.github.javaparser.*;
import com.github.javaparser.ast.*;
import com.github.javaparser.ast.Node;
import com.github.javaparser.ast.stmt.BlockStmt;
import com.github.javaparser.ast.stmt.Statement;
import com.github.javaparser.ast.CompilationUnit;
import com.github.javaparser.ast.Node.BreadthFirstIterator;
import com.github.javaparser.printer.DotPrinter;

class analyze{
  read read;
  ArrayList objects;
  fileStructure base;
  String ext=".java";
  List<String> methodsNotFound;
  public analyze(read r){
    read=r;
    methodsNotFound=new ArrayList();
  }
  public int[] analyzeBlock(Node old, Node looking,String location,String method, ArrayList data, String key,fileStructure structure,int index){
    Node n=looking;

    String name=key;
    int[] results = new int[7];
    PrintStream stdout = System.out;
    try{
      System.setOut(new PrintStream(new File("output"+File.separator+name+"_"+index+".txt")));

      System.out.println("looking:"+name+"\n"+location+"\t"+method+"\n-----------------------\n"+
        old+"\n-----------------------\n"+looking+"-----------------------");

      if(data.size()==1){
        System.out.println("possible Hot1");//single slow
        results[0]=1;
      }
      if(data.size()>1){
        System.out.println("possible Hot2");//many slow
        results[1]=1;
      }
      if(checkupToLoop(n)){
        System.out.println("possible type Hot3_1");//check for loops close to around critical section
        results[2]=1;
      }
      if(checkInnerLoop(looking)>0){//check critical section for loops
        System.out.println("possible type Hot3_2");
        System.out.println(checkInnerLoopDepth(looking));//check maximum depth of internal loops
        results[3]=1;
      }
      if(possibleSplit(n,name)){
        System.out.println("possible type overlySplit");
        results[4]=1;
      }//check for many critical sections with the same obejct in same function
      if(findViolation(n).size()>0){
        System.out.println("possible Simultaneous");//check for use of objects from key list in critical section not defined in class
        results[5]=1;
      }
      if(matchDec(name,n)){
        System.out.println("possible Unpredictable");//check definion against list
        results[6]=1;
      }
    }
    catch(Exception e){}
    System.setOut(stdout);
    return results;
  }
  private boolean matchDec(String name, Node node){
    List<String> bad=Arrays.asList("Boolean.FALSE","Boolean.TRUE");
    while(node.getParentNode().isPresent()){
      node=node.getParentNode().get();
      //System.out.println(node);
      if (node instanceof com.github.javaparser.ast.stmt.BlockStmt){
      	List nodes=node.getChildNodes();
        for(int x=0;x<nodes.size();x++){
          if(nodes.get(x) instanceof com.github.javaparser.ast.body.VariableDeclarator){
            //System.out.println(((com.github.javaparser.ast.body.VariableDeclarator)nodes.get(x)).getInitializer());
            if (((com.github.javaparser.ast.body.VariableDeclarator)nodes.get(x)).getName().toString().equals(name)){
              //System.out.println(((com.github.javaparser.ast.body.VariableDeclarator)nodes.get(x)).getInitializer());
              //not sure if possible
            }
          }
        }
      }
      else if(node instanceof com.github.javaparser.ast.body.ClassOrInterfaceDeclaration){
        NodeList members = ((com.github.javaparser.ast.body.ClassOrInterfaceDeclaration)node).getMembers();
        for (int x=0;x<members.size();x++){
          if(members.get(x) instanceof com.github.javaparser.ast.body.FieldDeclaration){
            NodeList var=((com.github.javaparser.ast.body.FieldDeclaration)members.get(x)).getVariables();
            for (int y=0;y<var.size();y++){
              if(((com.github.javaparser.ast.body.VariableDeclarator)var.get(y)).getName().toString().equals(name)){
                if(((com.github.javaparser.ast.body.VariableDeclarator)var.get(y)).getInitializer().isPresent()) {
                  if(bad.contains(((com.github.javaparser.ast.body.VariableDeclarator)var.get(y)).getInitializer().get().toString())) {
                    return true;
                  }
                }
              }
            }
          }
          //if(((com.github.javaparser.ast.body.Parameter)members.get(x)).getName().toString().equals(name)){

          //}
        }
      }
    }
    return false;
  }
  private ArrayList findViolation(Node section){
    ArrayList <Node> output=new ArrayList<>();
    BreadthFirstIterator bfi = new BreadthFirstIterator(section);
    while (bfi.hasNext()) {
      Node node = bfi.next();
      if (node instanceof com.github.javaparser.ast.expr.SimpleName){
        if(objects.contains(node.toString())){
          if(!checkSynchronized(node,node.toString())) {//not syncrhonized
            if(!checkUpForDec(node,node.toString())){
              output.add(node);
            }
          }
        }
      }
    }
    return output;
  }
  private boolean checkUpForDec(Node node,String name){
    if (node instanceof com.github.javaparser.ast.body.MethodDeclaration)return true;//bad
    Node old=node,old2=node;
    while(node.getParentNode().isPresent()){
      node=node.getParentNode().get();
      if(node instanceof com.github.javaparser.ast.body.MethodDeclaration){
        for (Node parameter:((com.github.javaparser.ast.body.MethodDeclaration)node).getParameters() ){
          if(((com.github.javaparser.ast.body.Parameter)parameter).getName().equals(name)){
            return true;
          }
        }

        return false;
      }
      else if (node instanceof com.github.javaparser.ast.stmt.BlockStmt){//itterate through deeper child nodes
        List childNodes=node.getChildNodes().get(0).getChildNodes();
        for (int x=0;x<childNodes.size();x++){
          if(childNodes.get(x).equals(old)||childNodes.get(x).equals(old2)){
            break;
          }
          else{
            if(childNodes.get(x) instanceof com.github.javaparser.ast.body.VariableDeclarator){
              for (int y=0;y<((List)childNodes.get(x)).size();y++){
                if (((Node)childNodes.get(x)).getChildNodes().get(y) instanceof com.github.javaparser.ast.expr.SimpleName){
                  if(((Node)childNodes.get(x)).getChildNodes().get(y).toString().equals(name)){
                    return true;
                  }
                }
              }
            }
          }
        }
      }
      else{
        List childNodes=node.getChildNodes();
        for(int x=0;x<childNodes.size();x++){
          if (!(childNodes.get(x) instanceof com.github.javaparser.ast.stmt.BlockStmt)){
            BreadthFirstIterator bfi = new BreadthFirstIterator((Node)childNodes.get(x));
            while (bfi.hasNext()) {
              Node n = bfi.next();
              if(childNodes.get(x) instanceof com.github.javaparser.ast.body.VariableDeclarator){
                for (int y=0;y<((Node)childNodes.get(x)).getChildNodes().size();y++){
                  if (((Node)childNodes.get(x)).getChildNodes().get(y) instanceof com.github.javaparser.ast.expr.SimpleName){
                    if(((Node)childNodes.get(x)).getChildNodes().get(y).toString().equals(name)){
                      return true;
                    }
                  }
                }
              }
            }
          }
        }
      }
      old2=old;
      old=node;
      node=node.getParentNode().get();
    }
    return false;
  }
  private boolean checkSynchronized(Node node, String name){

    if (node instanceof com.github.javaparser.ast.body.MethodDeclaration)return true;
    while(node.getParentNode().isPresent()){
      node=node.getParentNode().get();
      if(node instanceof com.github.javaparser.ast.stmt.SynchronizedStmt){
        for(Node n:node.getChildNodes()){
          if (n instanceof com.github.javaparser.ast.expr.Expression){
            if(n.toString().equals(name)){
              return true;
            }
          }
        }
      }
      else if (node instanceof com.github.javaparser.ast.body.MethodDeclaration){
        return false;
      }
    }
    return false;
  }
  private String syncObject(Node start){
    Node node=start;
    while(node.getParentNode().isPresent()){
      node=node.getParentNode().get();
      if (node instanceof com.github.javaparser.ast.stmt.SynchronizedStmt){
        return ((com.github.javaparser.ast.stmt.SynchronizedStmt)node).getExpression().toString();
      }
    }
    return null;
  }
  private boolean possibleSplit(Node start,String name){
    Node node=start;
    while(node.getParentNode().isPresent()){
      node=node.getParentNode().get();
      if (node instanceof com.github.javaparser.ast.stmt.BlockStmt){
        break;
      }
    }
    NodeList nodes=((com.github.javaparser.ast.stmt.BlockStmt)node).getStatements();

    for(int x=0;x<nodes.size();x++){
      if(nodes.get(x) instanceof com.github.javaparser.ast.stmt.SynchronizedStmt){
        //System.out.println(nodes.get(x));
        //System.out.println(start.getParentNode().get());
        if(!nodes.get(x).equals(start.getParentNode().get())){
          if(((com.github.javaparser.ast.stmt.SynchronizedStmt)nodes.get(x)).getExpression().toString().equals(name)){
            return true;
          }
        }
      }
    }

    return false;
  }
  private int checkInnerLoop(Node top){
    int total=0;
    BreadthFirstIterator bfi = new BreadthFirstIterator(top);
    while (bfi.hasNext()) {
      Node node = bfi.next();
      if (node instanceof com.github.javaparser.ast.stmt.WhileStmt || node instanceof com.github.javaparser.ast.stmt.DoStmt || node instanceof com.github.javaparser.ast.stmt.ForStmt || node instanceof com.github.javaparser.ast.stmt.ForEachStmt){
        total+=1;
      }
    }
    return total;
  }
  private int checkInnerLoopDepth(Node top){
    int total=0,x=0;
    List<Node> nodes=top.getChildNodes();
    while (x<nodes.size()) {
      Node node = nodes.get(x);
      x++;
      int n;
      if (node instanceof com.github.javaparser.ast.stmt.WhileStmt || node instanceof com.github.javaparser.ast.stmt.DoStmt || node instanceof com.github.javaparser.ast.stmt.ForStmt || node instanceof com.github.javaparser.ast.stmt.ForEachStmt){
        n=checkInnerLoopDepth(node)+1;
      }
      else{
        n=checkInnerLoopDepth(node);
      }
      if(n>total)total=n;
    }
    return total;
  }
  private int checkForCalls(Node start){
    Node node=start;
    int total=0;
    BreadthFirstIterator bfi = new BreadthFirstIterator(node);
    while (bfi.hasNext()) {
      node = bfi.next();
      if(node instanceof com.github.javaparser.ast.expr.MethodCallExpr){
        total++;
      }
    }
    return total;
  }
  private boolean checkupToLoop(Node start){
    Node node=start;
    while(node.getParentNode().isPresent()){
      node=node.getParentNode().get();
      if(node instanceof com.github.javaparser.ast.stmt.WhileStmt || node instanceof com.github.javaparser.ast.stmt.DoStmt || node instanceof com.github.javaparser.ast.stmt.ForStmt || node instanceof com.github.javaparser.ast.stmt.ForEachStmt){
        return true;//node in loop
      }
      else if(node instanceof com.github.javaparser.ast.modules.ModuleDeclaration){
        return false;//node not in loop
      }

    }
    return false;//impossible
  }
  public int[] begin(String name, ArrayList data,ArrayList locations,ArrayList methods,ArrayList structures,Set names,fileStructure B){
    objects=new ArrayList<>(names);
    System.out.println(name);
    int[] results = new int[7];
    for(int x=0;x<data.size();x++){
      System.out.println(x+"\t"+data.size());
      System.out.println(data.get(x));
      String location=locations.get(x).toString();
      String method=methods.get(x).toString();
      fileStructure structure=(fileStructure)structures.get(x);
      Node expanded=replaceSearch((Node)data.get(x),location,structure);
      base=B;
      int[] more=analyzeBlock((Node)data.get(x),expanded,location,method,data,name,structure, x);
      try{
        PrintWriter pw = new PrintWriter(new BufferedWriter(new FileWriter("outputRecords.txt", true)));
        pw.print("[");
        for(int y=0;y<results.length;y++){
          results[y]+=more[y];
          pw.print(more[y]+" ");
        }
        pw.println("]");
        pw.close();
      }
      catch(IOException e){
        System.out.println("ERROR");while(true){}
      }
    }
    return results;
  }
  public interface eval {
    Boolean check(Node p1);
  }
  private int depth(Node node){
    int depth=0;
    //System.out.println(node.getParentNode().get());
    while(node.getParentNode().isPresent()){
      node=node.getParentNode().get();
      depth++;
    }
    return depth;
  }
  private Node replaceSearch(Node original,String location,fileStructure structure){
    //System.out.println(original);
    Node big = original.clone();

    ArrayList<String> listN=new ArrayList<String>();
    ArrayList<String> listNames=new ArrayList<String>();
    ArrayList<Integer> listI=new ArrayList<Integer>();
    listN.add("empty");
    listI.add(-1);

    BreadthFirstIterator bfi = new BreadthFirstIterator(big);
    while (bfi.hasNext()) {
        Node node = bfi.next();
          //System.out.println(big.toString().length());
        if(node instanceof com.github.javaparser.ast.expr.MethodCallExpr){

          String FuncName=(((com.github.javaparser.ast.expr.MethodCallExpr)node).getName()).toString();
          if(((com.github.javaparser.ast.expr.MethodCallExpr)node).getScope().isPresent()){
            FuncName=((com.github.javaparser.ast.expr.MethodCallExpr)node).getScope().get()+"."+FuncName;
          }
          if(methodsNotFound.contains(FuncName)){}else{
            //System.out.println(FuncName+node+"\n");
            int index=0;
            List<Node> stats;
            try{
              stats=node.getParentNode().get().getParentNode().get().getChildNodes();
            }
            catch(java.util.NoSuchElementException e){
              continue;
            }
            for(index=0;index<stats.size();index++){
              if(stats.get(index).toString().equals(node.getParentNode().get().toString())){
                break;
              }
            }
            Node call = stats.get(index);
            String targetClass=null;

            try{
              if(node.getChildNodes().get(0).toString().split("\\.")[0].equals("this")){
                targetClass=location;
              }
              else if(node.getChildNodes().get(0).toString().length() - node.getChildNodes().get(0).toString().replace(".", "").length()==0){
                targetClass=location;
              }
              else{
                Node dec = findDecleration(node.getChildNodes().get(0).toString(),call,location);
                targetClass=dec.toString().split(" ")[0];
              }
              String[] parts=node.toString().split("\\.");
              Node found;
              if (parts.length==1){
                found=findMethod(node.toString().split("\\(")[0], findClass(targetClass+ext));
              }
              else{
                found=findMethod(node.toString().split("\\.")[1].split("\\(")[0], findClass(targetClass+ext));
              }
              List<Node> items=found.getChildNodes();
              for(int x=0;x<items.size();x++){
                if(items.get(x) instanceof com.github.javaparser.ast.stmt.BlockStmt){
                  found=items.get(x);
                  break;
                }
              }

              int depth=depth(node);

              for(int x=listI.size()-1;x>=0;x--){
                if(listI.get(x)>depth){
                  listN.remove(x);
                  listI.remove(x);
                }
              }
              if(!listN.contains(found.toString())){
                //((BlockStmt)node.getParentNode().get().getParentNode().get()).setStatement(index,(Statement)(found.clone()));
                ((BlockStmt)node.getParentNode().get().getParentNode().get()).addStatement((Statement)(found.clone()));
                if(!listNames.contains(FuncName)){
                  bfi = new BreadthFirstIterator(big);
                }
              }
              else{
                //remove decleration
              }
              listI.add(depth);
              listN.add(found.toString());
              listNames.add(FuncName);
            }
            catch(Exception e){
              //System.out.println(e);
              System.out.println("method "+FuncName+" not found\n");
              methodsNotFound.add(FuncName);
            }
          }
          //System.out.println(big);

          //node=replaceSearch(destination,targetClass);//merge into this node tree

        }
    }

    return big;
  }
  private int findAll(Node looking,eval E,String location,String method){
    //System.out.println(looking);
    int count=0;
    if (looking!=null){
      BreadthFirstIterator bfi = new BreadthFirstIterator(looking);
      while (bfi.hasNext()) {
          Node node = bfi.next();
          //System.out.println(node.getClass());
          if (E.check(node)) {
            //System.out.println(node.getClass());
            String[] type=node.getClass().toString().split(" ")[1].split("\\.");
            //System.out.println(type[type.length-1]);
            //System.out.println(node);
            //System.out.println('\n');
            count++;

            if(node instanceof com.github.javaparser.ast.expr.MethodCallExpr){
              /*DotPrinter dPrinter;
              dPrinter = new DotPrinter(true);
              try (FileWriter fileWriter = new FileWriter("images/"+node.toString().replace("()","")+".dot");
                  PrintWriter printWriter = new PrintWriter(fileWriter)) {
                  printWriter.print(dPrinter.output(node));
              }
              catch(Exception e){
                System.out.println(e);
              }*/
              String name=node.getChildNodes().get(1).toString();
              Node far=findMethodFull(method,findClass(location+ext));
              if (far!=null){
                //analyzeBlock(far,location,method);
              }
              else{
                //System.out.println(location);
                Node dec = findDecleration(node.getChildNodes().get(0).toString(),node,location);
                String targetClass=dec.toString().split(" ")[0];
                Node destination=findMethod(node.toString().split("\\.")[1].split("\\(")[0], findClass(targetClass+ext));
                findAll(destination,E,targetClass,node.toString().split("\\.")[1].split("\\(")[0]);

                /*analyzeBlock(
                findMethod(node.toString().split("\\.")[1].split("\\(")[0],
                findClass(targetClass, new File("input"))
                )
                ,location,method);*/

              }
            }
          }
      }
    }
    return count;
  }
  public Node findDecleration(String name, Node start,String location){
    //System.out.println("\nfindDecleration|"+start+"|"+name+"|"+location);
    try{
      Node up=start.getParentNode().get();
      //System.out.println("\nfindDecleration|"+start+"|"+name);
      if (up instanceof com.github.javaparser.ast.stmt.BlockStmt){
        List<Node> children=up.getChildNodes();
        Node child=null;
        int x;
        for (x=0;x<children.size();x++){
          child=children.get(x);
          if(child.equals(start)){
            //System.out.println("match|"+start+"|"+child);
            break;
          }
        }
        String ID="";
        BreadthFirstIterator bfi = new BreadthFirstIterator(start);
        while (bfi.hasNext()) {
          Node node = bfi.next();
          if (node instanceof com.github.javaparser.ast.expr.NameExpr){
            ID=node.toString();
          }
        }

        for(;x>=0;x--){
          child=children.get(x);
          //System.out.println(child);
          if(child instanceof com.github.javaparser.ast.stmt.ExpressionStmt){
            child=child.getChildNodes().get(0);
            if (child instanceof com.github.javaparser.ast.body.MethodDeclaration){
              //System.out.println("METH\t"+child);
            }
            else if (child instanceof com.github.javaparser.ast.expr.VariableDeclarationExpr){
              //System.out.println("VAR\t"+child);
              bfi = new BreadthFirstIterator(child);
              while (bfi.hasNext()) {
                Node node = bfi.next();
                if (node instanceof com.github.javaparser.ast.expr.SimpleName){
                  if(node.toString().equals(ID)){
                    //System.out.println(node.getParentNode().get().getParentNode().get());
                    //System.out.println("\t"+child);
                    return child;
                  }
                }
              }
            }
            else{
              //System.out.println("NONE");
            }
          }
        }
        return findDecleration(name,up,location);
      }
      /*else if(start instanceof com.github.javaparser.ast.body.MethodDeclaration){
        List<Node> children=start.getChildNodes();
        ListIterator<Node> C=children.listIterator();
        while(C.hasNext()){
          Node child=C.next();
          if (child instanceof com.github.javaparser.ast.body.Parameter){
            System.out.println();
          }
        }
        while(1==1){}
      }*/
      else{
        return findDecleration(name,start.getParentNode().get(),location);
      }
      //return null;
    }
    catch(Exception e){
      Node C = findClass(location+ext);
      BreadthFirstIterator bfi = new BreadthFirstIterator(C);
      while (bfi.hasNext()) {
        Node node = bfi.next();
        if(node instanceof com.github.javaparser.ast.expr.VariableDeclarationExpr){
          BreadthFirstIterator bfi2 = new BreadthFirstIterator(node);
          while (bfi.hasNext()) {
            Node node2 = bfi.next();
            if(node2 instanceof com.github.javaparser.ast.expr.SimpleName){
              if(node2.toString().equals(name))
                return node2;
            }
          }
        }
      }
    }
    return null;
  }
  public Node findClass(String name){
    return findClass(name,base);
  }
  public Node findClass(String name,fileStructure file){
    for(int x=0;x<file.fileNames.size();x++){
      if(file.fileNames.get(x).equals(name)){
        return file.javaFiles.get(x);
      }
    }
    for(int x=0;x<file.subFolders.size();x++){
      Node n=findClass(name,file.subFolders.get(x));
      if(n!=null){
        return n;
      }
    }
    return null;
  }
  public Node findMethod(String name, Node Class){
    //System.out.print(Class);
    BreadthFirstIterator bfi = new BreadthFirstIterator(Class);
    while (bfi.hasNext()) {
      //System.out.print("C_");
      Node node = bfi.next();
      if (node instanceof com.github.javaparser.ast.body.MethodDeclaration){
        //System.out.println(node.toString());


        BreadthFirstIterator bfi2 = new BreadthFirstIterator(node);
        while (bfi2.hasNext()) {
          Node node2 = bfi2.next();
          if (node2 instanceof com.github.javaparser.ast.expr.SimpleName){
            if(node2.toString().equals(name)){
              //System.out.println("|"+node2.toString()+"|"+name+"|"+(node2.toString().equals(name)));


              //check parameters
              //if(matchParameters(node,name)){

              //}

              return node;
            }
            break;
          }
        }
      }
    }
    return null;
  }
  public boolean matchParameters(){

    return false;
  }
  public Node findMethodFull(String name, Node Class){
    BreadthFirstIterator bfi = new BreadthFirstIterator(Class);
    //System.out.println("name:"+name);
    //System.out.println(Class);
    while (bfi.hasNext()) {
      Node node = bfi.next();
      if (node instanceof com.github.javaparser.ast.body.MethodDeclaration){
        if(node.toString().split("\n").equals(name)){
          return node;
        }
      }
    }
    return null;
  }
}
