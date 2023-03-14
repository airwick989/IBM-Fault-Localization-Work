//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                      AIX Symbol Harvester header file                    //
//                      --------------------------------                    //
//                                                                          //
//**************************************************************************//
//
#ifndef _AIXSYMS_H_
   #define _AIXSYMS_H_

// #define __try     try
// #define __except  catch
// #define EXCEPTION_EXECUTE_HANDLER uint ui
// #define _exception_code()         ui

   #define __try     
   #define __except  if
   #define EXCEPTION_EXECUTE_HANDLER 0
   #define _exception_code()         0

   #define GetJvmmapSummary()
   #define IsBadReadPtr(x,y)         0

//
// Return codes from symbol gatherers.
// Must be negative values because 0 means no symbols
// and positive, non-zero means that many symbols.
//
   #define NO_ACCESS_TO_FILE        -1
   #define FILE_TYPE_NOT_SUPPORTED  -2
   #define ERROR_ACCESSING_FILE     -3
   #define INTERNAL_ERROR           -4
   #define NO_SYMBOLS_FOUND         -5
   #define DBGHELP_ERROR            -6

#include "symlib.h"

//
// CONTEXT Record
// **************
//
// Used internally by harvester
//
typedef struct context_rec
{
   void *handle;                            // Pointer to Module node
   uint64 load_addr;                        // Module virtual load address //STJ64
   void *mod_addr;                          // Where we load the module
   char *imgname;                           // Name of image (module)
   char *searchpath;                        // Symbol search path
   PFN_SYMBOL_CALLBACK symbol_callback;     // User symbol callback
   PFN_SECTION_CALLBACK section_callback;   // User section callback
   uint img_cs;                             // Disk image checksum
   uint img_ts;                             // Disk image timestamp
   int  symtype;                            // Symbol type (as best we know)
   uint hflags;                             // Flags for the harvester
} CONTEXT_REC;



//
// Structure to keep image sections
// - name points to a the section name. The name is at most 8 characters.
//   If less than 8 characters then the name is NULL terminated.  If
//   ff exactly 8 characters it is not NULL terminated.
//
   #define MAX_SECTIONS       64               // The most I've seen is about 20

typedef struct sec_info
{
   uint64 start_addr;                       // Section starting virtual address/offset //STJ64
   uint64 end_addr;                         // Section ending virtual address/offset   //STJ64
   void *loc_addr;                          // Where I have it loaded
   char *name;                              // Section name
   int  number;                             // Section number
   uint offset;                             // Section offset
   uint size;                               // Section size
   uint file_offset;                        // Offset from start of file
   uint flags;                              // Section flags
   int  symcnt;                             // Number of symbols
} SECTION_INFO;

//==============================================================
// XCOFF Structures
//==============================================================

   #define XCOFF32_SIGNATURE 0x01DF
   #define XCOFF64_SIGNATURE 0x01EF

typedef struct _XCOFF_FILE_HEADER
{
   unsigned short Signature;            // 0x01DF for XCOFF32
   unsigned short numSections;
   unsigned long  TimeDate;
   unsigned long  offSymbolTable;
   unsigned long  numSymbols;
   unsigned short sizeAuxHeader;
   unsigned short Flags;
} 
XCOFF_FILE_HEADER;

typedef struct _XCOFF64_FILE_HEADER
{
   unsigned short     Signature;        // 0x01EF for XCOFF64
   unsigned short     numSections;
   unsigned long      TimeDate;
   unsigned long long offSymbolTable;
   unsigned short     sizeAuxHeader;
   unsigned short     Flags;
   unsigned long      numSymbols;
} 
XCOFF64_FILE_HEADER;

//--------------------------------------------------------------

typedef struct _XCOFF_OPTIONAL_HEADER
{
   long  incomplete;
} 
XCOFF_OPTIONAL_HEADER;

typedef struct _XCOFF64_OPTIONAL_HEADER
{
   long  incomplete;
} 
XCOFF64_OPTIONAL_HEADER;

//--------------------------------------------------------------

typedef struct _XCOFF_SECTION_HEADER
{
   unsigned char  Name[8]; 
   unsigned long  PhysAddr;
   unsigned long  VirtAddr;
   unsigned long  Size;
   unsigned long  offRawData;
   unsigned long  offRelocEntries;
   unsigned long  offLineNumEntries;
   unsigned short numRelocEntries;
   unsigned short numLineNumEntries;
   unsigned long  Flags;
} 
XCOFF_SECTION_HEADER;

typedef struct _XCOFF64_SECTION_HEADER
{
   unsigned char      Name[8]; 
   unsigned long long PhysAddr;
   unsigned long long VirtAddr;
   unsigned long long Size;
   unsigned long long offRawData;
   unsigned long long offRelocEntries;
   unsigned long long offLineNumEntries;
   unsigned long      numRelocEntries;
   unsigned long      numLineNumEntries;
   unsigned long      Flags;
} 
XCOFF64_SECTION_HEADER;

   #define XCOFF_SECTION_CODE 0x00000020 // Section contains code

//--------------------------------------------------------------

typedef struct _XCOFF_SYMBOL
{
   unsigned long  Name;   // Overlaps Offset as char[8], if non-zero
   unsigned long  Offset; // Offset of name in string table
   unsigned long  Value;
   unsigned short SectionNumber;
   unsigned short Type;
   unsigned char  StorageClass;
   unsigned char  numAuxEntries;
} 
XCOFF_SYMBOL;

typedef struct _XCOFF64_SYMBOL
{
   unsigned long long Value;
   unsigned long  Offset; // Offset of name in string table
   unsigned short SectionNumber;
   unsigned short Type;
   unsigned char  StorageClass;
   unsigned char  numAuxEntries;
} 
XCOFF64_SYMBOL;

typedef struct _XCOFF_AUX_SYMBOL
{
   unsigned long  SectionLength;
   unsigned long  offTypeCheck;
   unsigned short numTypeCheck;
   unsigned char  AlignmentSymType;
   unsigned char  StorageMappingClass;
   unsigned char  reserved[6];
} 
XCOFF_AUX_SYMBOL;

typedef struct _XCOFF64_AUX_SYMBOL
{
   unsigned long  SectionLength;
   unsigned long  offTypeCheck;
   unsigned short numTypeCheck;
   unsigned char  AlignmentSymType;
   unsigned char  StorageMappingClass;
   unsigned long  SectionLengthHigh;
   unsigned char  pad;
   unsigned char  AuxType;
} 
XCOFF64_AUX_SYMBOL;

//==============================================================
// Harvester internal function prototypes (aixsyms.c)
//==============================================================

void          ValidateKernelSymbols(void *lmn);
static int    GetModuleType(XCOFF_FILE_HEADER *xfh);

static bool_t ModuleIsOkToUse(uint ts, uint chksum, size_t filesize);
static int    EnumerateSections(CONTEXT_REC *cr);
static int    TryToGetSymbols(CONTEXT_REC *cr, char *modname, int kernel);
static void   GatherCodeForSection(int s);
char         *GetCodePtrForSymbol(uint symoffset, int secnum);
int           FindSectionForAddress(uint64 addr); //STJ64
int           FindSectionForOffset(uint offset);
bool_t        SectionIsExecutable(int num);
char         *DemangleName(char *name);

int SendBackSymbol(char *symbol_name,                // Symbol name
                   uint symbol_offset,               // Symbol offset
                   uint symbol_size,                 // Symbol size
                   int symbol_type,                  // Symbol type (label, export, etc.)
                   int section,                      // Section to which the symbol belongs
                   char *code,                       // Code associated with this symbol
                   void *context);                   // Context record

static int SendBackSection(char *section_name,       // Section name
                           int section_number,       // Section number
                           uint64 start_addr,        // Section start address/offset //STJ64
                           uint section_offset,      // Section offset
                           uint section_size,        // Section size
                           uint section_flags,       // Section flags
                           void *loc_addr,           // Where I have it loaded
                           int executable,           // Executable section or not
                           void *context);           // Context record

// In aixxcoff.c
int HarvestXCOFFSymbols(XCOFF_FILE_HEADER *xfh, CONTEXT_REC *cr);

// In aixsyms.c
void DumpSectionHeaders(XCOFF_SECTION_HEADER *xsh, int numsec);
void DumpCoffSymbolsHeader(XCOFF_FILE_HEADER *xfh);
void DumpCoffSymbol(int cnt, char *name, XCOFF_SYMBOL *sym);
void DumpCode(uchar *s, int n);

int  GetSymlibSymbol( unsigned int pid,  // Process id
                      uint64       addr, // Address //STJ64
                      SYMDATA     *sd);  // Pointer to SYMDATA structure

void InitSymlibSymbols();

#endif // _AIXSYMS_H_
