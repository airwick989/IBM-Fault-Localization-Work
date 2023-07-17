package com.mycompany.app;

import com.github.javaparser.ast.Node;

import java.util.List;


class Tools {
  public static void methodPrintout(String indent, fileStructure B){
    System.out.println(indent+B.name);
    for(fileStructure f : B.subFolders){
      methodPrintout(indent+"\t",f);
    }
    for(Node n : B.javaFiles){
      printFileTree(n,indent+"\t");
    }
  }
  public static void printFileTree(Node n, String indent ){
    List<com.github.javaparser.ast.body.ClassOrInterfaceDeclaration> classes = n.findAll(com.github.javaparser.ast.body.ClassOrInterfaceDeclaration.class);
    for(com.github.javaparser.ast.body.ClassOrInterfaceDeclaration node : classes){
      System.out.println(indent+(node.getName()));
      List<com.github.javaparser.ast.body.ConstructorDeclaration> methods2 = n.findAll(com.github.javaparser.ast.body.ConstructorDeclaration.class);
      for(com.github.javaparser.ast.body.ConstructorDeclaration method : methods2){
        System.out.println("\t"+indent+method.getName());
      }
      List<com.github.javaparser.ast.body.MethodDeclaration> methods = n.findAll(com.github.javaparser.ast.body.MethodDeclaration.class);
      for(com.github.javaparser.ast.body.MethodDeclaration method : methods){
        System.out.println("\t"+indent+method.getName());
      }
    }
  }
  public static void methodPrintoutInline(String trace, fileStructure B){
    for(fileStructure f : B.subFolders){
      methodPrintoutInline(trace+B.name+"/",f);
    }
    for(Node n : B.javaFiles){
      printFileTreeInline(n,trace+B.name+"/");
    }
  }
  public static void printFileTreeInline(Node n, String trace ){
    List<com.github.javaparser.ast.body.ClassOrInterfaceDeclaration> classes = n.findAll(com.github.javaparser.ast.body.ClassOrInterfaceDeclaration.class);
    for(com.github.javaparser.ast.body.ClassOrInterfaceDeclaration node : classes){
      String className=node.getName()+"";
      List<com.github.javaparser.ast.body.ConstructorDeclaration> methods2 = n.findAll(com.github.javaparser.ast.body.ConstructorDeclaration.class);
      for(com.github.javaparser.ast.body.ConstructorDeclaration method : methods2){
        System.out.println(trace+className+"/"+method.getName());
      }
      List<com.github.javaparser.ast.body.MethodDeclaration> methods = n.findAll(com.github.javaparser.ast.body.MethodDeclaration.class);
      for(com.github.javaparser.ast.body.MethodDeclaration method : methods){
        System.out.println(trace+className+"/"+method.getName());
      }
    }
  }
}
