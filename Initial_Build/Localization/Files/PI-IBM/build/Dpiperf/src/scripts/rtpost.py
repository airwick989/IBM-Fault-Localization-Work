import sys

mods = dict()
unresolved = []
jitsyms = []
ppid = 0

class JitSymbol(object):
   def __init__(self, low, high, name):
      self.low = low
      self.high = high
      self.name = name

class Symbol(object):
   def __init__(self, symId, name):
      self.symbolId = symId
      self.name = name
      self.count = 0
      
class Module(object):
   
   def __init__(self, modId, name):
      self.moduleId = modId
      self.name = name
      self.syms = dict()
      self.count = 0
      
   def addSymbol(self, symId, name):
      if (self.syms.has_key(symId)):
         self.syms[symId].name = name
      else:
         sym = Symbol(symId, name)
         self.syms[symId] = sym
   
   def sample(self, symId):
      self.count += 1
      if (self.syms.has_key(symId)):
         self.syms[symId].count += 1
      else:
         sym = Symbol(symId, str(symId))
         sym.count = 1
         self.syms[symId] = sym
         
   def report(self):
      count = 0;
      symreport = ""
      symList = self.syms.values()
      symList.sort(key=lambda x: x.count, reverse=True)
      for sym in symList:
         if (sym.count > 0):
            symreport += "\tSYM\t" + str(sym.count) + "\t" + sym.name + "\n"
            count += sym.count;
      strr = "MOD\t" + str(count) + "\t" + self.name + "\n"
      strr += symreport
      return strr;
         
         
def handleModule(line):
   # "mte.moduleName <pid> <moduleId> <name>\n"
   tokens = line.split(" ")
   pid = tokens[1]
   ppid = int(pid, 16)
   modId = tokens[2].strip()
   name = tokens[3].strip()
   if (mods.has_key(modId)):
      print ("module already added: " + modId)
      mods[modId].name = name
   else:
      module = Module(modId, name)
      mods[modId] = module
      
def handleSymbol(line):
   # "mte.symbolName <pid> <moduleId> <symbolId> <name>\n"
   tokens = line.split(" ")
   pid = tokens[1]
   modId = tokens[2].strip()
   symId = tokens[3].strip()
   name = tokens[4].strip()
   if (not mods.has_key(modId)):
      module = Module(modId, "none")
      mods[modId] = module
   mods[modId].addSymbol(symId, name)
   
def handleSample(line):
   # "sample <timeStamp> <pid> <tid> <moduleId> <symbolId> <offset>\n"
   tokens = line.split(" ")
   ts = tokens[1]
   pid = tokens[2]
   tid = tokens[3]
   modId = tokens[4].strip()
   symId = tokens[5].strip()
   off = tokens[6]
   
   # if we don't know the module Id then it is unresolved
   if (modId == "0"):
      unresolved.append(int(off, 16))
      return
      
   if (not mods.has_key(modId)):
      module = Module(modId, "none")
      mods[modId] = module
   mods[modId].sample(symId)

def parseDataFile(fname):
   file = open(fname)
   lines = file.readlines()
   for line in lines:
      if line.startswith("mte.moduleName"):
         handleModule(line)
      elif line.startswith("mte.symbolName"):
         handleSymbol(line)
      elif line.startswith("sample"):
         handleSample(line)
   file.close()
         
def parseVLog(fname):
   file = open(fname)
   lines = file.readlines()
   mod = Module("JITTED", "JITTED")
   for i in range(2, len(lines)):
      line = lines[i]
      if (not line.startswith("+")):
         continue
      tokens = line.split(" ")
      if (tokens[1].startswith("(profiled")):
          name = tokens[3].strip()
          addrange = tokens[5]
      else:
          name = tokens[2].strip()
          addrange = tokens[4]
      #print addrange
      addrs = addrange.split("-")
      low = int(addrs[0], 16)
      high = int(addrs[1], 16)
      jitsyms.append(JitSymbol(low, high, name))
      mod.addSymbol(name, name)
   
   jitsyms.sort(key=lambda x: x.low, reverse=False)
   for off in unresolved:
      for jsym in jitsyms:
         if ((off > jsym.low) and (off < jsym.high)):
            mod.sample(jsym.name)
            break
            
   mods["JITTED"] = mod
   file.close()

def report():
   modList = mods.values()
   modList.sort(key=lambda x: x.count, reverse=True)
   for mod in modList:
      sys.stdout.write (mod.report())
      print ("\n")
   print ("unresolved samples: %d" % len(unresolved))

def main():
   parseDataFile("tprof.data")
   if (len(sys.argv) > 1):
      parseVLog(sys.argv[1])
   report()
   return 0

if __name__ == '__main__':
   main()

