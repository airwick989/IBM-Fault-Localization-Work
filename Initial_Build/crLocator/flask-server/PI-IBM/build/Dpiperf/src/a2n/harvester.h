/*
 * IBM Performance Inspector
 * Copyright (c) International Business Machines Corp., 1996 - 2009
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _HARVESTER_H_
#define _HARVESTER_H_

//
// Harvester version number
// ************************
//
#define HV_MAJOR      3
#define HV_MINOR      1
#define HV_REV        19

#define H_VERSION_NUMBER   (HV_MAJOR << 24) | (HV_MINOR << 16) | HV_REV




//
// SYMBOL Record
// *************
//
// Data associated with a symbol.
// Returned by harvester to the symbol callback function.
//
typedef struct symbol_rec {
    char * name;                            // Symbol name
    void * code;                            // Code associated with symbol
    void * handle;                          // Module node
    int  type;                              // Symbol type
    uint offset;                            // Offset from start of module
    uint length;                            // Symbol size
    int section;                            // Section where it resides
} SYMBOL_REC;


//
// SECTION Record
// **************
//
// Data associated with an executable section
// Returned by harvester to the section callback function.
//
typedef struct section_rec {
   uint64 start_addr;                       // Section starting virtual address/offset
   char * name;                             // Section name
   void * loc_addr;                         // Where I have it loaded
   void * handle;                           // Module node
   void * asec;                             // BFD asection pointer
   int  number;                             // Section number
   int  executable;                         // 1= executable section, 0= non-executable
   uint offset;                             // Section offset
   uint size;                               // Section size
   uint flags;                              // Section flags
} SECTION_REC;



//
// Callback function prototypes
// ****************************
// Possible return values:
// 0:        continue enumerating symbols/sections
// non-zero: stop enumerating symbols/sections
//
//int SymbolCallBack(SYMBOL_REC *sr);
//int SectionCallBack(SECTION_REC *sr);
typedef int (* PFN_SYMBOL_CALLBACK)(SYMBOL_REC *);
typedef int (* PFN_SECTION_CALLBACK)(SECTION_REC *);



//
// GetHarvesterVersion()
// *********************
//
// Returns Linux harverster version.
// Version is returned as an unsigned 32-bit quantity interpreted
// as follows:
//
//               3      2 2      1 1
//               1      4 3      6 5              0
//              +--------+--------+----------------+
//              | Major  |  Minor |    Revision    |
//              +--------+--------+----------------+
//               (0-255)  (0-255)     (0-65535)
//
// Returns
//    non-zero: Version information
//
ApiExport uint ApiLinkage GetHarvesterVersion(void);


//
// GetSymbolsForModule()
// *********************
//
// Harvester interface.
// Given and executable module name (fully qualified) and an optional
// search path for locating symbol files associated with the module,
// this function will attempt to harvest symbols either from the
// executable itself or from an associated "symbol" file.
//
// Returns: 0 if *ANY* symbols are found anywhere
//          non-zero on errors
//
int GetSymbolsForModule(void * handle,            // Really a pointer to a Module node
                        char * modname,           // Fully qualified Module name
                        char * searchpath,        // Search path
                        uint64 relocaddr,         // Relocation address
                        void * symbol_callback,   // Call-back function
                        void * section_callback); // Call-back function


//
// ValidateKernelSymbols()
// ***********************
//
// Reads /proc/ksyms and makes sure all kernel symbols in that file
// match the ones we harvested (from the map or the image itself).
// If symbols don't match then the entire symbol node chain is
// deleted (we're just throwing away the symbols we gathered).
// If we have problems (like malloc errors, etc.) we leave symbols
// alone.
//
// NOTE: argument is really an LMOD_NODE * cast as a void *
//
// If validation fails:
// - mn.symcnt set to 0
// - bit A2N_FLAGS_VALIDATION_FAILED set in mn.flags
// - entire symbol node chain is freed.  mn.symroot set to NULL
//
// If we run into problems (malloc, can't open files, etc):
// - bit A2N_FLAGS_VALIDATION_NOT_DONE set in mn.flags
//
void ValidateKernelSymbols(void * lmn);
#endif
