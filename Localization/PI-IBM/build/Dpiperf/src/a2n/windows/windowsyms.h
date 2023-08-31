//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                    Windows Symbol Harvester header file                  //
//                    ------------------------------------                  //
//                                                                          //
//**************************************************************************//
//
#ifndef _WINDOWSYMS_H_
#define _WINDOWSYMS_H_
                                            // PE structures are in winnt.h
#include <dbghelp.h>
#include "codeview.h"                       // CodeView structures
#include "pdb.h"                            // PDB structures are in pdb.h



typedef BOOL (__stdcall *SYMENUMLINES_FUNC) (HANDLE, ULONG64, PCSTR, PCSTR, PSYM_ENUMLINES_CALLBACK, PVOID);
typedef BOOL (__stdcall *SYMENUMSYMBOLS_FUNC) (HANDLE, ULONG64, PCSTR, PSYM_ENUMERATESYMBOLS_CALLBACK, PVOID);


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


//
// CONTEXT Record
// **************
//
// Used internally by harvester
//
typedef struct context_rec {
   uint64 load_addr;                        // Module virtual load address
   void *handle;                            // Pointer to Module node
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

typedef struct sec_info {
   uint64 start_addr;                       // Section starting virtual address/offset
   uint64 end_addr;                         // Section ending virtual address/offset
   void *loc_addr;                          // Where I have it loaded
   void *asec;                              // **** NOT USED ****
   char *name;                              // Section name
   int  number;                             // Section number
   uint offset;                             // Section offset
   uint size;                               // Section size
   uint rawsize;                            // Section size on disk
   uint file_offset;                        // Offset from start of file
   uint flags;                              // Section flags
   int  symcnt;                             // Number of symbols
#if defined(_64BIT)
   uint qwalign;                            // Maintain QWORD alignment
#endif
} SECTION_INFO;






//
// Harvester internal function prototypes (windowsyms.c)
// *****************************************************
//
void ValidateKernelSymbols(void *lmn);
static int GetModuleType(IMAGE_DOS_HEADER *dh);

static bool_t ModuleIsOkToUse(uint ts, uint chksum, size_t filesize);
static int EnumerateSections(CONTEXT_REC *cr);
static int TryToGetSymbols(CONTEXT_REC *cr, char *modname, int kernel);
static IMAGE_DEBUG_DIRECTORY *FindDebugDirectory(void);
static void GatherCodeForSection(int s);
char *GetCodePtrForSymbol(uint symoffset, int secnum);
int FindSectionForAddress(uint64 addr);
int FindSectionForOffset(uint offset);
bool_t SectionIsExecutable(int num);
char *DemangleName(char *name);
char *TsToDate(uint ts);

int SendBackSymbol(char *symbol_name,                // Symbol name
                   uint symbol_offset,               // Symbol offset
                   uint symbol_size,                 // Symbol size
                   int symbol_type,                  // Symbol type (label, export, etc.)
                   int section,                      // Section to which the symbol belongs
                   char *code,                       // Code associated with this symbol
                   void *context);                   // Context record
static int SendBackSection(char *section_name,       // Section name
                           int section_number,       // Section number
                           uint64 start_addr,        // Section start address/offset
                           uint section_offset,      // Section offset
                           uint section_size,        // Section size
                           uint section_flags,       // Section flags
                           void *loc_addr,           // Where I have it loaded
                           int executable,           // Executable section or not
                           void *context);           // Context record

// IMAGE_OPTIONAL_HEADER getters
ushort GetIohMagic(IMAGE_OPTIONAL_HEADER *ioh);
uint GetIohCheckSum(IMAGE_OPTIONAL_HEADER *ioh);
IMAGE_DATA_DIRECTORY *GetIohDataDirectory(IMAGE_OPTIONAL_HEADER *ioh);
uint GetIohNumberOfRvaAndSizes(IMAGE_OPTIONAL_HEADER *ioh);


// Prototype for symbol file (DBG, MAP, PDB) harvesters
typedef int (*PFN_FILE_HARVESTER)(char *fn, uint ts, CONTEXT_REC *cr);

// In windbg.c
int GetSymbolsFromDbgFile(char *dbgname, uint ts, CONTEXT_REC *cr);

// In winmap.c
int GetSymbolsFromMapFile(char *mapname, uint ts, CONTEXT_REC *cr);

// In winjmap.c
void GetJvmmapSummary(char *modname);
void DumpJvmmaps(FILE *fh);
void RemoveJvmmaps(void);
int GetSymbolsFromJmapFile(char *mapname, uint ts, CONTEXT_REC *cr);
int GetSymbolsFromJvmmapFile(CONTEXT_REC *cr);

// In winpdb.c
int GetSymbolsFromPdbFile(char *pdbname, uint ts, CONTEXT_REC *cr);

// In wincoff.c
int HarvestCoffSymbols(IMAGE_COFF_SYMBOLS_HEADER *ich, CONTEXT_REC *cr);

// In wincv.c
int HarvestCodeViewSymbols(IMAGE_CV_HEADER *cvh, CONTEXT_REC *cr);

// In winexport.c
int HarvestExportSymbols(IMAGE_NT_HEADERS *nth, CONTEXT_REC *cr);
IMAGE_EXPORT_DIRECTORY *FindExportDirectory(IMAGE_NT_HEADERS *nth, DWORD *rva_start,
                                            DWORD *rva_end, int   *delta);

// In winms.c
int GetSymbolsUsingMsEngine(CONTEXT_REC *cr);
void FixupLines(MOD_NODE *mn);

// In winhvdebug.c
void DumpNtHeader(IMAGE_NT_HEADERS *nth);
void DumpSectionHeaders(IMAGE_SECTION_HEADER *ish, int numsec);
void DumpDebugDirectories(IMAGE_DEBUG_DIRECTORY *idd, int debug_dir_cnt);
void DumpMiscDebugSection(IMAGE_DEBUG_MISC *idm);
void DumpExportDirectoryTable(IMAGE_EXPORT_DIRECTORY *ied);
void DumpCodeviewDebugHeader(IMAGE_CV_HEADER *cvh);
void DumpCodeviewDirectoryHeader(IMAGE_CV_DIR_HEADER *cvdh);
void DumpCodeViewDirEntry(IMAGE_CV_DIR_ENTRY *cvde, ULONG cvhrva);
void DumpCodeViewSymHash(IMAGE_CV_SYMHASH *cvsh);
void DumpCodeViewRecordType(CVSYM *sym, uint recoff);
void DumpCoffSymbolsHeader(IMAGE_COFF_SYMBOLS_HEADER *ich);
void DumpCoffSymbol(int cnt, char *name, IMAGE_SYMBOL *sym);
void DumpModuleInfo(IMAGEHLP_MODULE *im);
void DumpDbgFileHeader(IMAGE_SEPARATE_DEBUG_HEADER *sdh);
void DumpCode(uchar *s, int n);

#endif // _WINDOWSYMS_H_
