import java.io.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.ArrayList;
import java.util.HashMap;

public class App{
  public static HashMap m =new HashMap<String, ArrayList<Segment>>();
  public static void main(String args[]){
    Segment files=new Segment("base");
    try{
      File folder = new File("input");
      File[] listOfFiles = folder.listFiles();
      for (File file : listOfFiles) {
        if (file.isFile()) {
          String text="";
          //System.out.println(file.getName());
          ArrayList<String> lines = new ArrayList<String>();
          BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream("input/"+file.getName())));
          String line;
          int count=0;
          while ((line = br.readLine()) != null) {
            count++;
            lines.add(line.strip());
          }
          Segment A = new Segment(file.getName());
          A.parent="base";
          A.x=0;
          A.parse(lines);
          files.parts.add(A);
          //System.out.println(text);
          //"(\\b(public|private|final|abstract|)\\b\\s)?class \\w*\\s*\\{"
          //"(\\b(public|private|final|abstract|)\\b\\s)?class \\w*\\s*\\{[\\n|\\s|\\w\\d]*"
          //Matcher m = Pattern.compile("(\\b(public|private|final|abstract|)\\b\\s)?class \\w*\\s*\\{[^\\{|\\}]*").matcher(text);
          //while (m1.find()) {
            //System.out.println(m1.start());
          //}
        }
      }
      //System.out.println(files);
      files.synchSearch(m);
      for (Object k: m.keySet()){
        System.out.println(k);
        System.out.println(m.get(k));
      }
    }
    catch(Exception e){
      System.out.println(e);
    }
  }
}

class Segment{
  public Segment(String name){
    line=name;
  }
  public Segment(){}
  String line;
  String parent;
  int type;
  static int x;
  ArrayList <Segment> parts=new ArrayList<Segment>();
  public void parse(ArrayList<String> lines){
    //line=lines.get(x);
    String l;
    while(x<lines.size()){
      l=lines.get(x);
      x++;
      //System.out.println(l);
      if (l.matches("[^\\{|\\}]*\\s*\\{")){
        Segment A = new Segment(l);
        A.parent=line;
        A.parse(lines);
        parts.add(A);
      }
      else if (l.matches("[^\\{|\\}]*\\s*\\}")){
        Segment A = new Segment(l);
        A.parent=line;
        parts.add(A);
        return;
      }
      else{
        Segment A = new Segment(l);
        A.parent=line;
        parts.add(A);
      }
    }

  }
  public String toString(){
    String out = line;
    for (int a=0;a<parts.size();a++){
      out+="\n"+parts.get(a).toString("\t");
    }
    return out;
  }
  public String toString(String space){
    String out = line;
    for (int a=0;a<parts.size();a++){
      out+="\n"+space+parts.get(a).toString(space+"\t");
    }
    return out;
  }
  public void synchSearch(HashMap m){
    if(line.strip().matches("\\bsynchronized\\b\\([\\.|\\w]*\\)\\{")){
      //System.out.println(line.strip().substring(line.strip().indexOf('(')+1,line.strip().lastIndexOf(')')));
      //System.out.println(toString("\t")+"\n");
      add(m,line.strip().substring(line.strip().indexOf('(')+1,line.strip().lastIndexOf(')')),this);
    }
    else if (line.strip().matches("((public)|(private)|(final)|(abstract))\\s*(synchronized)\\s+\\w+\\s+\\w+\\(.*\\)\\{")){
      String[] names=parent.strip().substring(0,parent.strip().indexOf('{')).split(" ");
      String upper=names[names.length-1];
      names=line.strip().substring(0,line.strip().indexOf('(')).split(" ");
      //System.out.println(upper+"."+names[names.length-1]);
      //System.out.println(toString("\t")+"\n");
      add(m,upper+"."+names[names.length-1],this);
    }
    for (int a=0;a<parts.size();a++){
      parts.get(a).synchSearch(m);
    }
  }
  private void add(HashMap m, String id, Segment item){
    if (!m.containsKey(id)){
      m.put(id,new ArrayList<Segment>());
    }
    ((ArrayList)m.get(id)).add(item);
  }
}
