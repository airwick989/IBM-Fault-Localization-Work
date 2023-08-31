import sys

# -------------------------- Function Definition --------------------------

def checkIfAllZeros(input):
    allZeros = 1
    for i in input:
        if i != 0: allZeros = 0
    return allZeros

def printHelp():
    print 'Usage: analyze.pi [Library Threshold Percentage] [Symbols Threshold Percentage] [filenames]'
    print '- Library Threshold Percentage: the minimum percentage above which a library needs to be on a separate record (not aggregated)'
    print '- Symbols Threshold Percentage: the minimum percentage above which a symbol needs to be on a separate record (not aggregated)'
    print '- Filenames: Path to each PI output file that needs to be compared'

# Find Java's PID that is used in the current PI output
def findPID(pidMap):
    pid = -1
    for name,currentPid in pidMap.iteritems():
        # print name
        if 'java' in name:
            if pid != -1: raise Exception('More than one Java process are there, please specify which one to analyze')
            pid = currentPid
    if pid == -1: raise Exception('Java process not found')
    return pid

# Process PI output file to get 2 maps:
# libraryLoad: {library --> load}
# librarySymbolLoad: {library --> {symbol --> load}}
def processFile(file):
    # Storing information
    pidMap = {} # Process Name --> PID
    libraryLoad = {} # Percentage of load taken by each library
    librarySymbolLoads = {} # Library --> Symbol --> load

    # Variables to keep state
    pid = -1 # Java PID
    isProcess = 0 # Whether we are currently parsing the right PID
    maxLoad = -1
    phase = 0 # What is being parsed (PIDs, modules, or symbols)
    counter = 0
    previousLine = ''
    currentLibrary = ''

    for r in file:
        counter += 1
        r = r.strip()
        if not r: continue
        while '  ' in r: r = r.replace('  ', ' ')
        # print r
        if ')) Process' == r and previousLine == '================================':
            phase = 1
            previousLine = r
            continue
        elif ')) Process_Module' == r and previousLine == '================================':
            phase = 2
            pid = findPID(pidMap)
            previousLine = r
            continue
        elif ')) Process_Module_Symbol' == r and previousLine == '================================': phase = 3

        previousLine = r

        if phase == 1:
            r = r.split(' ')
            if len(r) > 1:
                pidMap[r[3]] = r[1]
        
        if phase == 2:
            r = r.split(' ')
            if str(pid) in r:
                isProcess = 1
                maxLoad = float(r[2])
                continue
            if isProcess and 'PID' == r[0]: isProcess = 0
            if isProcess == 1:
                library = r[3].split('/')[-1]
                libraryLoad[library] = float(r[2])/maxLoad
        
        if phase == 3:
            # print counter
            r = r.split(' ')
            if str(pid) in r:
                isProcess = 1
                maxLoad = float(r[2])
                continue
            if isProcess and 'PID' == r[0]: 
                isProcess = 0
                break
            if isProcess:
                if r[0] == 'MOD':
                    currentLibrary = r[3].split('/')[-1]
                    librarySymbolLoads[currentLibrary] = {}
                    continue
                if r[3] in librarySymbolLoads:
                    raise Exception(r[3] + ' symbol is appearing in multiple libraries (if this is okay, remove the exception from code)')
                librarySymbolLoads[currentLibrary][r[3]] = float(r[2])/maxLoad
                
    return libraryLoad, librarySymbolLoads

# Print table: | Library | Symbol | Load Percentage |
def printLibrarySymbolTable(librarySymbolLoads, threshold):
    print '| Library | Symbol | Load Percentage |'
    print '| --- |  --- |  --- |'
    for library,symbols in librarySymbolLoads.iteritems():
        for symbol,percentage in symbols.iteritems():
            if percentage > threshold:
                symbol = symbol.replace('::','::<br>')
                print '| ' + library + ' | ' + symbol + ' | ' + str(percentage * 100)[0:5] + '% |'

# Print table: | Library | Load Percentage |
def printLibraryTable(libraryLoads, threshold):
    print '| Library | Load Percentage |'
    print '| --- | --- |'
    for library in libraryLoads:
        if libraryLoads[library] > threshold:
            print '| ' + library + ' | ' + str(libraryLoads[library] * 100)[0:5] + '% |'

# Get set of all qualified symbol names, that is: library <delimeter> symbol
def getAllQualSymbolNames(librarySymbolLoads, delimeter = ':::'):
    symbolSet = set()
    for library,symbols in librarySymbolLoads.iteritems():
        for symbol in symbols: symbolSet.add(library + delimeter + symbol)
    return symbolSet

# Tables = list of input tables
# Tables Names = list of input table names, respectively to the order in the tables array
# Input Table 1: | Library | Symbol | Load Percentage1 |
# Input Table 2: | Library | Symbol | Load Percentage2 |
# Printed Result: | Table | Symbol | Load Percentage1 | Load Percentage2 |
# Threshold percentage above which records should be recorded (0 < threshold percentage < 1)
def mergeLibrarySymbolTables(tables, tableNames, thresholdPercentage):
    toPrint = '| Library | Symbol | '
    for name in tableNames: toPrint += name + ' | '
    toPrint = toPrint[:-1] + '\n| --- |  --- | '
    for name in tableNames: toPrint += '--- | '
    print toPrint[:-1]
    toPrint = ''

    insignificants = {} # All symbols with percentages less than threshold have their percentages 
                        # aggregated in an 'insignificant' by libraries
                        # insignificants = {library --> list of (total load of insignificants)})
                        # Each record of the list corresponds to a record in the tables list (ordered respectively)

    allSymbols = set()
    for table in tables: allSymbols |= getAllQualSymbolNames(table)
    allSymbols = list(allSymbols)
    allSymbols.sort()

    for qualSymbolName in allSymbols:
        # print qualSymbolName
        library = qualSymbolName.split(':::')[0] # Assuming default delimiter used for getAllQualSymbolNames
        symbol = qualSymbolName.split(':::')[1] # Assuming default delimiter used for getAllQualSymbolNames

        # Get percentages
        percentages = []
        printable = 0
        for table in tables:
            percentage = 0.0
            if library in table and symbol in table[library]: percentage = table[library][symbol] * 100
            if percentage > thresholdPercentage: printable = 1 # at least one value is above threshold
            percentages.append(percentage)
        
        if printable:
            toPrint = '| ' + library + ' | ' + symbol + ' | '
            for percentage in percentages: toPrint += str(percentage)[0:5] + '% | '
            print toPrint[:-1]
        
        else:
            for i in range(len(tables)):
                if library not in insignificants: insignificants[library] = [0] * len(tables)
                insignificants[library][i] += percentages[i]
    
    # Print the insignificants
    for library, percentages in insignificants.iteritems():
        toPrint = '| ' + library + ' | (rest of symbols) | '
        if checkIfAllZeros(percentages): continue # skip libraries with all values = 0
        for percentage in percentages: toPrint += str(percentage)[0:5] + '% | '
        print toPrint[:-1]
    

def mergeLibraryTables(tables, tableNames, thresholdPercentage):
    toPrint = '| Library | '
    for name in tableNames: toPrint += name + ' | '
    toPrint = toPrint[:-1] + '\n| --- | '
    for name in tableNames: toPrint += '--- | '
    print toPrint[:-1]
    toPrint = ''

    allLibraries = set()
    for table in tables: allLibraries |= set(table.keys())
    allLibraries = list(allLibraries)
    allLibraries.sort()

    for library in allLibraries:
        # Get percentages
        percentages = []
        printable = 0
        for table in tables:
            percentage = 0.0
            if library in table: percentage = table[library] * 100
            if percentage > thresholdPercentage: printable = 1 # at least one value is above threshold
            percentages.append(percentage)
        
        if printable:
            toPrint = '| ' + library + ' | '
            for percentage in percentages: toPrint += str(percentage)[0:5] + '% | '
            print toPrint[:-1]

# -------------------------- Main Code --------------------------

try:
    libraryThresholdPercentage = float(sys.argv[1])
    symbolThresholdPercentage = float(sys.argv[2])
except:
    printHelp()
    raise Exception('Wrong arguments')

if len(sys.argv) < 4: 
    printHelp()
    raise Exception('Wrong arguments: at least 4 arguments must be provided')

# All lists have the same order. So the first file has its name in the first record of libraryLoadsNames,
# first libraryLoad map in the first record of libraryLoads and first libraryLoadsSymbols map in the 
# first record of libraryLoadsSymbols
libraryLoads = []
libraryLoadsSymbols = []
names = []
for i in range(3, len(sys.argv)):
    try:
        file = open(sys.argv[i])
    except:
        raise Exception('File path not valid for file: ' + sys.argv[i])
    currentLibraryLoad, currentLibraryLoadsSymbol = processFile(file)
    libraryLoads.append(currentLibraryLoad)
    libraryLoadsSymbols.append(currentLibraryLoadsSymbol)
    filename = sys.argv[i].split('/')[-1]
    names.append(filename)

print "--------------------------- Library Loads ---------------------------\n"
mergeLibraryTables(libraryLoads, names, libraryThresholdPercentage)
print "\n--------------------------- Library Symbol Loads ---------------------------\n"
mergeLibrarySymbolTables(libraryLoadsSymbols, names, symbolThresholdPercentage)