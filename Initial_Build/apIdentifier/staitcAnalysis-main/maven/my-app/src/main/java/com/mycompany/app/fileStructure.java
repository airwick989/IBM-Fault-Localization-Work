package com.mycompany.app;
import java.util.List;
import java.util.ArrayList;
import com.github.javaparser.ast.Node;

class fileStructure {
  String name;
  fileStructure parent;
  List<fileStructure> subFolders;
  List<Node> javaFiles;
  List<String> fileNames;
  public fileStructure(){
    name="";
    parent =null;
    subFolders=new ArrayList();
    javaFiles=new ArrayList();
    fileNames=new ArrayList();
  }
  public int countSize(){
    int total=javaFiles.size();
    for(int x=0;x<subFolders.size();x++){
      total+=subFolders.get(x).countSize();
    }
    return total;
  }
}
