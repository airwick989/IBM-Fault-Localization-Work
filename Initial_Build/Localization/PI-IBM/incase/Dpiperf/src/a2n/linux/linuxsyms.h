/*   Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
//**************************************************************************//
//                                                                          //
//                     Linux Symbol Harvester header file                   //
//                     ----------------------------------                   //
//                                                                          //
//**************************************************************************//
//
#ifndef _LINUXSYMS_H_
#define _LINUXSYMS_H_

#include <elf.h>


#ifndef EI_OSABI
#define EI_OSABI       7
#endif

#ifndef EI_ABIVERSION
#define EI_ABIVERSION  8
#endif

#ifndef EM_S390_OLD
#define EM_S390_OLD    0xA390
#endif



//
// CONTEXT Record
// **************
//
// Used internally by harvester
//
typedef struct context_rec {
   uint64 load_addr;                        // Module virtual load address
   void * handle;                           // Pointer to Module node
   void * mod_addr;                         // Where we load the module
   PFN_SYMBOL_CALLBACK symbol_callback;     // User symbol callback
   PFN_SECTION_CALLBACK section_callback;   // User section callback
   int  symtype;                            // Symbol type (as best we know)
   uint hflags;                             // Flags for the harvester
} CONTEXT_REC;



//
// Structure to keep track of executable sections
//
#define MAX_EXEC_SECTIONS      16           // Max number of executable sections
#define MAX_ALLOC_SECTIONS     64           // Max number of non-executable, allocated sections

typedef struct sec_info {
   uint64 start_addr;                       // Section starting virtual address/offset
   uint64 end_addr;                         // Section ending virtual address/offset
   char * name;                             // Section name
   void * loc_addr;                         // Where I have it loaded
   asection * sec;                          // BFD asection pointer
   int  number;                             // Section number
   uint offset;                             // Section offset
   uint size;                               // Section size
   uint flags;                              // Section flags
   uint type;                               // Section type
   uint file_offset;                        // Offset from start of file
} SECTION_INFO;



//
// Structure used when validating kernel symbols
//
#define MAX_KSYMS  1500

struct ksym {                               // Symbol in /proc/ksyms
   uint64 addr;
   char * name;
   int index;
};




//
// Harvester internal function prototypes
// **************************************
//
int GetModuleType(char * filename, bfd ** abfd, int kernel);
int TryToGetSymbols(CONTEXT_REC * cr, char * modname, int kernel);
int SendBackSymbol(char * symbol_name,      // Symbol name
                   uint symbol_offset,      // Symbol offset
                   uint symbol_size,        // Symbol size
                   int symbol_type,         // Symbol type (label, export, etc.)
                   int section,             // Section to which the symbol belongs
                   char * code,             // Code associated with this symbol
                   void * context);         // Context record
int SendBackSection(char * section_name,    // Section name
                    int section_number,     // Section number
                    void * asec,            // BFD asection pointer
                    uint64 start_addr,      // Section start address/offset
                    uint section_offset,    // Section offset
                    uint section_size,      // Section size
                    uint section_flags,     // Section flags
                    void * loc_addr,        // Where I have it loaded
                    int executable,         // Executable section or not
                    void * context);        // Context record
void PrintBytes(unsigned char * s, int n);
char *GetCodePtrForSymbol(char * sym_addr, SECTION_INFO * si, int num_secs);
void AddKernelModules(void);
void DumpElfHeader(Elf64_Ehdr * eh, char * modname);

// In linuxelf.c
int GetSymbolsFromElfExecutable(CONTEXT_REC * cr, char * modname, int module_type, bfd * abfd);
int GetSymbolsFromElf64Executable(CONTEXT_REC * cr, char * modname, int module_type, bfd * abfd);
void DumpElfSymbol(char * name, char * comment, Elf32_Sym * sym);
void DumpElf64Symbol(char * name, char * comment, Elf64_Sym * sym);

// In linuxmap.c
int GetMapType(char * filename);
int GetKernelSymbolsFromMap(CONTEXT_REC * cr, char * mapname);
int GetSymbolsFromMap_nm(CONTEXT_REC * cr, char * mapname);
int GetSymbolsFromMap_objdump(CONTEXT_REC * cr, char * mapname);
int GetSymbolsFromMap_readelf(CONTEXT_REC * cr, char * mapname);
int GetKernelSymbolsFromKallsyms(CONTEXT_REC * cr);
int GetKernelModuleSymbolsFromKallsyms(CONTEXT_REC * cr);

// In linuxval.c
int compare(const void * e1, const void * e2);

#endif
