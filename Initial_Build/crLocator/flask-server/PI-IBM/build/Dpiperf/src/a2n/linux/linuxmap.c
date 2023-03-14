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
//                      Linux MAP File Symbol Harvester                     //
//                      -------------------------------                     //
//                                                                          //
//**************************************************************************//
//
#include "a2nhdr.h"




//
// Externs (from a2n.c)
//
extern gv_t a2n_gv;                         // Globals






//
// GetMapType()
// ************
//
// Reads a .MAP file and tries to figure out what kind of map file it is.
//
int GetMapType(char * filename)
{
   FILE * fh = NULL;
   char * c;
   char line[512], cvalue[128], cvalue2[128];
   uint64 uvalue;
   int rc = MODULE_TYPE_INVALID;            // Still being pessimistic


   dbghv(("> GetMapType: fn='%s'\n", filename));
   //
   // Look inside.
   //
   fh = fopen(filename, "r");
   if (fh == NULL ) {
      errmsg(("*E* GetMapType: unable to open file '%s'\n", filename));
      goto TheEnd;
   }

   c = fgets(line, 512, fh);                // Read first line
   if (c == NULL) {
      errmsg(("*E* GetMapType: unable to read 1st line\n"));
      goto TheEnd;
   }

   if (line[0] == 0x0a) {                   // Blank line ...
      c = fgets(line, 512, fh);             // Read another one
      if (c == NULL) {
         errmsg(("*E* GetMapType: unable to read 2nd line\n"));
         goto TheEnd;
      }
   }

   dbghv(("- GetMapType: Here's what we're looking at: '%s'\n", line));

   //
   // Check for a map file generated using "nm -an"
   // I've seen a couple of different ways their first couple of lines look:
   //
   // 1) "xxxxxxxx c symbol_name"
   //    Where "xxxxxxxx" is an address, "c" is one of the nm types,
   //    and symbol_name is just that.
   //
   // 2) "xxxxxxxx c"
   //     Where "xxxxxxxx" is an address, "c" is one of the nm types,
   //     usually a "?", and there is no symbol name.
   //
   // 3) "         c symbol_name"
   //    Where "c" is one of the nm types and symbol_name is just that.
   //
   rc = sscanf(line, "%"_L64x" %s %s\n", &uvalue, cvalue, cvalue2);
   if (rc == 3) {
      // type 1
      dbghv(("- GetMapType: (MODULE_TYPE_MAP_NM, type 1: xss)\n"));
      rc = MODULE_TYPE_MAP_NM;             // (.MAP) Linux map file generated via "nm -an"
      goto TheEnd;
   }
   rc = sscanf(line, "%"_L64x" %s\n", &uvalue, cvalue);
   if (rc == 2) {
      // type 2
      dbghv(("- GetMapType: (MODULE_TYPE_MAP_NM, type 2: xs)\n"));
      rc = MODULE_TYPE_MAP_NM;             // (.MAP) Linux map file generated via "nm -an"
      goto TheEnd;
   }
   rc = sscanf(line, "%s %s\n", cvalue, cvalue2);
   if (rc == 2) {
      // type 3
      dbghv(("- GetMapType: (MODULE_TYPE_MAP_NM, type 3: ss)\n"));
      rc = MODULE_TYPE_MAP_NM;             // (.MAP) Linux map file generated via "nm -an"
      goto TheEnd;
   }

   //
   // Check for a map file generated using "readelf -s"
   // I've seen a couple of different way their first couple of line look:
   // - "Symbol table '.symtab' contains ### entries:"
   // - "Symbol table '.dynsym' contains ### entries:"
   //
   rc = sscanf(line, "Symbol table '%s' contains %"_L64d" entries:\n", cvalue, &uvalue);
   dbghv(("*** Symbol table '%s' contains %"_L64d" entries:\n", cvalue, uvalue));
   if ((rc == 2) && ((stricmp(cvalue, ".symtab") == 0) ||
                     (stricmp(cvalue, ".dynsym") == 0))) {
      dbghv(("- GetMapType: (MODULE_TYPE_MAP_READELF)\n"));
      rc = MODULE_TYPE_MAP_READELF;        // (.MAP) Linux map file generated via "readelf -s"
      goto TheEnd;
   }

   //
   // Check for a map file generated using "objdump -t"
   // The first line in the file is blank.
   // The second line looks like:
   //   "module_name:     file format elf32-i386"
   //   Where "module_name is the name of the module.
   //
   // For example, for "libsu.so" the second lines looks like:
   //   "libsu.so:     file format elf32-i386"
   //
   rc = sscanf(line, "%s: file format %s\n", cvalue, cvalue2);
   if (rc == 2) {
      dbghv(("- GetMapType: (MODULE_TYPE_MAP_OBJDUMP)\n"));
      rc = MODULE_TYPE_MAP_OBJDUMP;        // (.MAP) Linux map file generated via "objdump -t"
      goto TheEnd;
   }

   //
   // If we fall thru then it was something we didn't understand.
   //
TheEnd:
   if (fh != NULL)
      fclose(fh);

   dbghv(("< GetMapType: rc=%d\n", rc));
   return (rc);
}


//
// GetSymbolsFromMap_nm()
// **********************
//
// Read a map file (in "nm -an" format) and extract symbols from it.
// Returns number of symbols found or 0 to indicate error.
//
// "nm" map file format:
// ---------------------
//   addr type name
//
// Where:
// - addr is the symbol address/offset in hex.
// - type is the 1-character symbol type
// - name is the symbol name
//
// Example:
//  00003e14 T AddLoadedModuleNode
//  c010022a t gdt_descr
//
// Assumptions:
// - Map file is generated the same way the kernel map file (System.map)
//   is generated.
// - Every line contains an address, a type and a symbol name
// - Base address for the module is the first t/T symbol
// - Base address > 0x08000000 are assumed to be for non-relocateble modules
// - Symbol types are:
//   - B/b: Global/Local symbol in uninitialized data section
//   - D/d: Global/Local symbol in initialized data section
//   - R/r: Global/Local symbol in read-only data section
//   - T/f: Global/Local symbol in text section
//   - A:   Absolute symbol (section names usually)
//
int GetSymbolsFromMap_nm(CONTEXT_REC * cr, char * mapname)
{
   FILE * fd = NULL;
   int rc;
   int NumberOfSymbols = 0;                 // Total number of symbols

   uint64 addr, prev_addr;
   uint64 base_addr = 0;                    // Module base address

   char type[2], prev_type;
   char name[512], prev_name[512];
   uint32 prev_offset, prev_size;
   int symtype;
   int addr_is_offset;

   char * symtype_p,
        * symtype_function = "Fnc",
        * symtype_label = "Lbl";


   dbghv(("> GetSymbolsFromMap_mn: fn='%s', cr=%p\n", mapname, cr));
   //
   // Open the file
   //
   fd = fopen(mapname, "r");
   if (fd == NULL ) {
      errmsg(("*E* GetSymbolsFromMap_mn: unable to open file '%s'.\n", mapname));
      goto TheEnd;                          // No symbols found
   }

   //
   // Look for the first T or t symbol.  That's considered to be
   // the start address.
   // dbghv(("*** %x %s %s\n",base_addr,type,name));
   //
   rc = fscanf(fd, "%"_L64x" %s %s\n", &base_addr, type,name);
   dbghv(("*** %"_L64x" %s %s\n", base_addr, type,name));
   while (rc != EOF  &&  (type[0] != 'T' && type[0] != 't')) {
      rc = fscanf(fd, "%"_L64x" %s %s\n", &base_addr, type,name);
      dbghv(("*** %"_L64x" %s %s\n", base_addr, type,name));
   }
   if (rc == EOF) {
      dbghv(("< GetSymbolsFromMap_nm: unable to find first 'T' symbol\n"));
      goto TheEnd;                          // No symbols found
   }

   //
   // Take a guess as to what type of module this map is for.
   // Linux loads executables at 0x08048000, while shared objects
   // are not loaded at fixed addresses.
   // * Addresses in the map for an executable are actual virtual
   //   addressess (ie. the linker assumes the executable will be
   //   loaded at 0x08048000).
   // * Addresses in the map for a .so/reloacatable module are offsets
   //   (ie. the linker assumes the module will be relocated).
   //
   if (base_addr >= EXEC_IMAGE_BASE)
      addr_is_offset = 0;                   // executable: addr is virtual address
   else
      addr_is_offset = 1;                   // .so/relocatable: addr is offset

   //
   // base_addr contains the base address for the module.
   // Now look for the rest of the T/t and D/d symbols ...
   //
   prev_addr = base_addr;
   strcpy(prev_name,name);
   prev_type = type[0];
   rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);
   dbghv(("*** %"_L64X" %s %s\n", addr, type, name));
   while (rc != EOF) {
      if (prev_type != 'b' || prev_type != 'B') {         // D, R, T or A (ie. not B)
         if (prev_size == 0) {
            symtype_p = symtype_label;
            symtype = SYMBOL_TYPE_LABEL;
         }
         else {
            symtype_p = symtype_function;
            symtype = SYMBOL_TYPE_FUNCTION;
         }

         if (addr_is_offset)
            prev_offset = (uint32)prev_addr;
         else
            prev_offset = (uint32)(prev_addr - base_addr);

         prev_size = (uint32)(addr - prev_addr);

         //
         // Call callback function with each symbol.
         // Callback function returns 0 if it wants us to continue, non-zero
         // if it wants us to stop.
         //
         dbghv(("%s   0x%08x  0x%08x  %d  %d  %s\n", symtype_p, prev_offset, prev_size, symtype, 1, prev_name));
         rc = SendBackSymbol(prev_name,     // Symbol name
                             prev_offset,   // Symbol offset
                             prev_size,     // Symbol size
                             symtype,       // Symbol type
                             1,             // Section (don't know any better)
                             NULL,          // No code
                             cr);           // Context record
         if (rc) {
            dbghv(("< GetSymbolsFromMap_nm: SymbolCallback returned %d.  Stopping\n", rc));
            goto TheEnd;                    // Error. Stop enumerating and return
                                            // number of symbols found so far.
         }

         NumberOfSymbols++;
      }

      prev_addr = addr;
      strcpy(prev_name,name);
      prev_type = type[0];
      rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type, name);
      dbghv(("*** %"_L64x" %s %s\n", addr, type, name));
   }

   //
   // Close file if open
   //
TheEnd:
   if (fd != NULL)
      fclose(fd);

   return (NumberOfSymbols);
}


//
// GetSymbolsFromMap_objdump()
// ***************************
//
// Read a map file (in "objdump -t" format) and extract symbols from it.
// Returns number of symbols found: 0 means none or error.
//
int GetSymbolsFromMap_objdump(CONTEXT_REC * cr, char * mapname)
{
   dbghv(("> GetSymbolsFromMap_objdump: fn='%s', cr=%p\n", mapname, cr));
   dbghv(("< GetSymbolsFromMap_objdump: rc=%d\n", 0));
   return (0);
}


//
// GetSymbolsFromMap_readelf()
// ***************************
//
// Read a map file (in "readelf -s" format) and extract symbols from it.
// Returns number of symbols found: 0 means none or error.
//
int GetSymbolsFromMap_readelf(CONTEXT_REC * cr, char * mapname)
{
   dbghv(("> GetSymbolsFromMap_readelf: fn='%s', cr=%p\n", mapname, cr));
   dbghv(("< GetSymbolsFromMap_readelf: rc=%d\n", 0));
   return (0);
}


#if defined(_X86) || defined(_IA64) || defined(_X86_64)
//
// GetKernelSymbolsFromMap()
// *************************
//
// Read kernel map file (System.map) and extract symbols from it.
// Returns number of symbols found: 0 means none or error.
// Assumptions:
// * Map is sorted in ascending address order.
// * 1st symbol we care about is "startup_32". It is the kernel base address.
// * Symbols between "startup_32" and "stext_lock" are assumed to be in the
//   ".text" section.
// * Symbol "stext_lock" is the one and only symbol in the ".text.lock" section.
// * Symbols after "stext_lock" are assumed to be in the ".text.init" section.
//
//
// System.map file format:
// -----------------------
//   ...
//   c0100000 t startup_32              <-- first real symbol (assumed to be base addr)
//   ...                                <-- ".text" section
//   c02353d0 T stext_lock              <-- ".text.lock" section
//   ...                                <-- non-text section symbols
//   c02c6000 A __init_begin            <-- start of ".text.init" section
//   ...                                <-- ".text.init" section
//   c03133d8 A _end                    <-- last line
//
int GetKernelSymbolsFromMap(CONTEXT_REC * cr, char * mapname)
{
   int rc;
   int look_for_init_section = 0;
   int NumberOfSymbols = 0;                 // Total number of symbols
   int section_number = 1;

   FILE * fd;

   uint64 addr, prev_addr;
   uint64 base_addr;                        // Kernel base address
   uint64 last_section_base_addr;           // Base address of last executable section (guess)
   uint64 end_addr;                         // End address of last executable section (guess)

   char type[2], prev_type;
   char name[1024], prev_name[1024];

   char * FirstRealSymbol = "startup_32",
        * section_name1 = ".text",
        * section_name2 = ".text.lock",
        * section_name3 = ".text.init";

   uint32 prev_offset, prev_size;
   int symtype;

   char * symtype_p,
        * symtype_function = "Fnc",
        * symtype_label = "Lbl";


   dbghv(("> GetKernelSymbolsFromMap: fn='%s', cr=%p\n", mapname, cr));
   dbghv(("\n\n***** Map file \"%s\"\n", mapname));

   //
   // Open the file
   //
   fd = fopen(mapname, "r");
   if (fd == NULL ) {
      errmsg(("*E* GetKernelSymbolsFromMap: unable to open file '%s'.\n", mapname));
      return (NumberOfSymbols);             // No symbols found
   }

   //
   // Look for the "startup_32" or "_start" symbol
   // dbghv(("*** %x %s %s\n",base_addr,type,name));
   //
   rc = fscanf(fd, "%"_L64x" %s %s\n", &base_addr, type,name);
   while (rc != EOF  &&  strcmp(name, FirstRealSymbol) != 0) {
      rc = fscanf(fd, "%"_L64x" %s %s\n", &base_addr, type,name);
   }
   if (rc == EOF) {
      errmsg(("*E* GetKernelSymbolsFromMap: unable to find symbol '%s'.\n", FirstRealSymbol));
      fclose(fd);
      return (NumberOfSymbols);             // No symbols found
   }

   //
   // "startup_32"/"_start" is the first symbol.
   // base_addr contains the base address for the kernel.
   // Now look for the first "t" or "T" symbol whose address is different
   // than the kernel base address.
   //
   rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);
   while (rc != EOF  &&  addr == base_addr  &&  (type[0] != 't' || type[0] != 'T')) {
      rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type, name);
   }
   if (rc == EOF) {
      errmsg(("*E* GetKernelSymbolsFromMap: unable to find symbol after '%s'.\n", FirstRealSymbol));
      fclose(fd);
      return (NumberOfSymbols);             // No symbols found
   }

   dbghv(("Lbl   0x%08x  0x%08x  %d  %d  %s\n", 0, (uint32)(addr - base_addr), STT_FUNC, section_number, FirstRealSymbol));
   rc = SendBackSymbol(FirstRealSymbol,      // Symbol name
                       0,                    // Symbol offset
                       (addr - base_addr),   // Symbol size
                       SYMBOL_TYPE_FUNCTION, // Symbol type
                       section_number,       // Section (don't know any better)
                       NULL,                 // No code
                       cr);                  // Context record
   if (rc) {
      dbghv(("< GetKernelSymbolsFromMap: SymbolCallback returned %d.  Stopping\n", rc));
      return (NumberOfSymbols);       // Error. Stop enumerating and return
                                      // number of symbols found so far.
   }
   NumberOfSymbols++;

   //
   // Now look for the rest of the symbols ...
   // Grab everything except for symbols in uninitialized data section (ie. B/b)
   //
   prev_addr = addr;
   strcpy(prev_name,name);
   prev_type = type[0];
   rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type, name);
   while (rc != EOF) {
      if (prev_type != 'b' || prev_type != 'B') {         // D, R, T or A (ie. not B)
         if (prev_size == 0) {
            symtype_p = symtype_label;
            symtype = SYMBOL_TYPE_LABEL;
         }
         else {
            symtype_p = symtype_function;
            symtype = SYMBOL_TYPE_FUNCTION;
         }
         prev_offset = (uint32)(prev_addr - base_addr);
         prev_size = (uint32)(addr - prev_addr);

         //
         // Try to guess at what the different executable sections are ...
         // * Anything up to, but not including, "stext_lock" is considered
         //   to be the ".text" section.
         // * "stext_lock" is considered to be the ".text.lock" section.
         // * Anything after "stext_lock" is considered to be in the
         //   ".text.init" section.
         //
         if (strcmp(prev_name, "stext_lock") == 0) {
            dbghv(("%s   0x%"_LZ64x"  0x%08x  offset:0x%08x\n", section_name1, base_addr,
                                                                (uint32)(prev_addr-base_addr), 0));
            SendBackSection(section_name1,                // Section name
                            section_number,               // Section number
                            NULL,                         // BFD asection pointer
                            base_addr,                    // Section start address/offset
                            0,                            // Section offset
                            (prev_addr - base_addr - 1),  // Section size
                            0,                            // Section flags
                            NULL,                         // Where I have it loaded
                            1,                            // Executable section
                            cr);                          // Context record

            dbghv(("%s   0x%"_LZ64x"  0x%08x  offset:0x%08x\n", section_name2, prev_addr,
                                                                prev_size, (uint32)(prev_addr - base_addr)));
            SendBackSection(section_name2,                // Section name
                            ++section_number,             // Section number
                            NULL,                         // BFD asection pointer
                            prev_addr,                    // Section start address/offset
                            (prev_addr - base_addr),      // Section offset
                            prev_size,                    // Section size
                            0,                            // Section flags
                            NULL,                         // Where I have it loaded
                            1,                            // Executable section
                            cr);                          // Context record

            dbghv(("%s   0x%08x  0x%08x  %d  %d  %s\n", symtype_p, prev_offset,
                                                        prev_size, symtype, section_number, prev_name));
            rc = SendBackSymbol(prev_name,       // Symbol name
                                prev_offset,     // Symbol offset
                                prev_size,       // Symbol size
                                symtype,         // Symbol type (
                                section_number,  // Section (don't know any better)
                                NULL,            // No code
                                cr);             // Context record
            if (rc) {
               dbghv(("< GetKernelSymbolsFromMap: SymbolCallback returned %d.  Stopping\n", rc));
               return (NumberOfSymbols);       // Error. Stop enumerating and return
                                               // number of symbols found so far.
            }

            section_number++;
            look_for_init_section = 1;
         }
         else {
            //
            // Call callback function with each symbol.
            // Callback function returns 0 if it wants us to continue, non-zero
            // if it wants us to stop.
            //
            dbghv(("%s   0x%08x  0x%08x  %d  %d  %s\n", symtype_p, prev_offset,
                                                        prev_size, symtype, section_number, prev_name));
            rc = SendBackSymbol(prev_name,      // Symbol name
                                prev_offset,    // Symbol offset
                                prev_size,      // Symbol size
                                symtype,        // Symbol type
                                section_number, // Section (don't know any better)
                                NULL,           // No code
                                cr);            // Context record
            if (rc) {
               dbghv(("< GetKernelSymbolsFromMap: SymbolCallback returned %d.  Stopping\n", rc));
               return (NumberOfSymbols);       // Error. Stop enumerating and return
                                               // number of symbols found so far.
            }
         }

         NumberOfSymbols++;
         end_addr = prev_addr + prev_size;
      }

      prev_addr = addr;
      strcpy(prev_name, name);
      prev_type = type[0];
      rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);

      if (look_for_init_section  &&  (strcmp(name, "__init_begin") == 0)) {
         last_section_base_addr = addr;
         look_for_init_section = 0;
      }
   }

   //
   // Send back the last section ...
   //
   dbghv(("%s   0x%"_LZ64x"  0x%08x  offset:0x%08x\n", section_name3, last_section_base_addr,
                                                       (uint32)(end_addr - last_section_base_addr),
                                                       (uint32)(last_section_base_addr - base_addr)));
   SendBackSection(section_name3,                        // Section name
                   section_number,                       // Section number
                   NULL,                                 // BFD asection pointer
                   last_section_base_addr,               // Section start address/offset
                   (uint32)(last_section_base_addr - base_addr), // Section offset
                   (uint32)(end_addr - last_section_base_addr),  // Section size
                   0,                                    // Section flags
                   NULL,                                 // Where I have it loaded
                   1,                                    // Executable section
                   cr);                                  // Context record

   //
   // Done
   //
   fclose(fd);
   dbghv(("< GetKernelSymbolsFromMap: Done enumerating. Found %d symbols.\n", NumberOfSymbols));
   dbghv(("*************** Found %d symbols\n", NumberOfSymbols));
   return (NumberOfSymbols);
}
#endif  //_X86 || defined(_IA64) || defined(_X86_64)


#if defined(_PPC)
//
// GetKernelSymbolsFromMap()
// *************************
//
// Read kernel map file (System.map) and extract symbols from it.
// Returns number of symbols found: 0 means none or error.
// Assumptions:
// * Map is sorted in ascending address order.
// * 1st symbol we care about is "_start". It is the kernel base address.
//
//
//   c0000000 T _start                  <-- first real symbol (assumed to be base addr)
//   ...
//   c01b40b0 t exit_shmem_fs           <-- start of ".text.exit" section
//   c01b4fc8 A _etext                  <-- end of .text section. Includes ".text.exit" section
//   ...
//   c0249000 A __init_begin            <-- start of ".text.init"
//   ...
//            A __init_end
//   c028c000 A __pmac_begin            <-- start of ".text.pmac"
//   ...
//            A __pmac_end
//   c0293000 A __prep_begin            <-- start of ".text.prep"
//   ...
//            A __prep_end
//   c029c000 A __chrp_begin            <-- start of ".text.chrp"
//   ...
//            A __chrp_end
//   c029e000 A __openfirmware_begin    <-- start of ".text.openfirmware"
//   ...
//            A __openfirmware_end
//   ...
//   c037e3f8 A _end                    <-- last line
//
int GetKernelSymbolsFromMap(CONTEXT_REC * cr, char * mapname)
{
#define SECTION_CNT  6
   int rc, i;
   int NumberOfSymbols = 0;                 // Total number of symbols
   int section_number = 1;

   FILE * fd;

   uint64 addr, prev_addr;
   uint64 base_addr;                        // Kernel base address
   uint64 end_addr;                         // End address of last executable section (guess)

   char type[2], prev_type;
   char name[1024], prev_name[1024];

   char * FirstRealSymbol_1 = "_start";
   char * FirstRealSymbol_2 = ".__start";
   char * sym1_name;

   char * section_names[SECTION_CNT] = {
                                          ".text",
                                          ".text.init",
                                          ".text.pmac",
                                          ".text.prep",
                                          ".text.chrp",
                                          ".text.openfirmware"
                                       };

   uint64 section_start_addr[SECTION_CNT] = {0, 0, 0, 0, 0, 0},
          section_end_addr[SECTION_CNT] = {0, 0, 0, 0, 0, 0};

   uint32 prev_offset, prev_size;
   int symtype;

   char * symtype_p,
        * symtype_function = "Fnc",
        * symtype_label = "Lbl";


   dbghv(("> GetKernelSymbolsFromMap: fn='%s', cr=%p\n", mapname, cr));
   dbghv(("\n\n***** Map file \"%s\"\n", mapname));

   //
   // Open the file
   //
   fd = fopen(mapname, "r");
   if (fd == NULL ) {
      errmsg(("*E* GetKernelSymbolsFromMap: unable to open file '%s'.\n", mapname));
      return (NumberOfSymbols);             // No symbols found
   }

   //
   // Look for the "_start" symbol
   // dbghv(("*** %x %s %s\n",base_addr,type,name));
   //
   rc = fscanf(fd, "%"_L64x" %s %s\n", &base_addr, type, name);
   while (rc != EOF  &&  strcmp(name, FirstRealSymbol_1) != 0  &&  strcmp(name, FirstRealSymbol_2) != 0) {
      rc = fscanf(fd, "%"_L64x" %s %s\n", &base_addr, type,name);
   }
   if (rc == EOF) {
      errmsg(("*E* GetKernelSymbolsFromMap: unable to find symbols '%s' or '%s'.\n", FirstRealSymbol_1, FirstRealSymbol_2));
      fclose(fd);
      return (NumberOfSymbols);             // No symbols found
   }
   section_start_addr[0] = base_addr;
   sym1_name = zstrdup(name);

   //
   // "_start" is the first symbol.
   // base_addr contains the base address for the kernel.
   // Now look for the first "t" or "T" symbol whose address is different
   // than the kernel base address.
   //
   rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);
   while (rc != EOF  &&  addr == base_addr  &&  (type[0] != 't' || type[0] != 'T')) {
      rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);
   }
   if (rc == EOF) {
      errmsg(("*E* GetKernelSymbolsFromMap: unable to find symbol after '%s'.\n", name));
      fclose(fd);
      zfree(sym1_name);
      return (NumberOfSymbols);             // No symbols found
   }

   dbghv(("Lbl   0x%08x  0x%08x  %d  %d  %s\n", 0, (uint32)(addr - base_addr), STT_FUNC, section_number, sym1_name));
   rc = SendBackSymbol(sym1_name,            // Symbol name
                       0,                    // Symbol offset
                       (addr - base_addr),   // Symbol size
                       SYMBOL_TYPE_FUNCTION, // Symbol type
                       section_number,       // Section (don't know any better)
                       NULL,                 // No code
                       cr);                  // Context record
   if (rc) {
      dbghv(("< GetKernelSymbolsFromMap: SymbolCallback returned %d.  Stopping\n", rc));
      return (NumberOfSymbols);       // Error. Stop enumerating and return
                                      // number of symbols found so far.
   }
   NumberOfSymbols++;
   zfree(sym1_name);

   //
   // Now look for the rest of the T/t and D/d symbols ...
   // Grab everything except for symbols in uninitialized data section (ie. B/b)
   //
   prev_addr = addr;
   strcpy(prev_name,name);
   prev_type = type[0];
   rc = fscanf(fd,"%"_L64x" %s %s\n", &addr, type, name);
   while (rc != EOF) {
      if (prev_type != 'b' || prev_type != 'B') {         // D, R, T or A (ie. not B)
         if (prev_size == 0) {
            symtype_p = symtype_label;
            symtype = SYMBOL_TYPE_LABEL;
         }
         else {
            symtype_p = symtype_function;
            symtype = SYMBOL_TYPE_FUNCTION;
         }
         prev_offset = (uint32)(prev_addr - base_addr);
         prev_size = (uint32)(addr - prev_addr);

         //
         // Call callback function with each symbol.
         // Callback function returns 0 if it wants us to continue, non-zero
         // if it wants us to stop.
         //
         dbghv(("%s   0x%08x  0x%08x  %d  %d  %s\n", symtype_p, prev_offset,
                                                     prev_size, symtype, section_number, prev_name));
         rc = SendBackSymbol(prev_name,      // Symbol name
                             prev_offset,    // Symbol offset
                             prev_size,      // Symbol size
                             symtype,        // Symbol type
                             section_number, // Section (don't know any better)
                             NULL,           // No code
                             cr);            // Context record
         if (rc) {
            dbghv(("< GetKernelSymbolsFromMap: SymbolCallback returned %d.  Stopping\n", rc));
            return (NumberOfSymbols);       // Error. Stop enumerating and return
                                            // number of symbols found so far.
         }

         NumberOfSymbols++;
         end_addr = prev_addr + prev_size;
      }

      prev_addr = addr;
      strcpy(prev_name,name);
      prev_type = type[0];
      rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);

      if (type[0] == 'A') {
         for (i = 0; i < SECTION_CNT; i++) {
            if      (strcmp(name, "_etext") == 0)
               section_end_addr[0] = addr;

            else if (strcmp(name, "__init_begin") == 0) {
               section_start_addr[1] = addr;
               section_number++;
            }
            else if (strcmp(name, "__init_end") == 0)
               section_end_addr[1] = addr;

            else if (strcmp(name, "__pmac_begin") == 0) {
               section_start_addr[2] = addr;
               section_number++;
            }
            else if (strcmp(name, "__pmac_end") == 0)
               section_end_addr[2] = addr;

            else if (strcmp(name, "__prep_begin") == 0) {
               section_start_addr[3] = addr;
               section_number++;
            }
            else if (strcmp(name, "__prep_end") == 0) {
               section_end_addr[3] = addr;
               section_number++;
            }

            else if (strcmp(name, "__chrp_begin") == 0) {
               section_start_addr[4] = addr;
               section_number++;
            }
            else if (strcmp(name, "__chrp_end") == 0)
               section_end_addr[4] = addr;

            else if (strcmp(name, "__openfirmware_begin") == 0) {
               section_start_addr[5] = addr;
               section_number++;
            }
            else if (strcmp(name, "__openfirmware_end") == 0)
               section_end_addr[5] = addr;
         }
      }
   }

   //
   // Send back the sections ...
   //
   for (i = 0; i < SECTION_CNT; i++) {
      if (section_start_addr[i] != 0  &&  section_end_addr[i] != 0) {
         dbghv(("%s   0x%"_LZ64x"  0x%"_LZ64x"  offset:0x%08x\n", section_names[i], section_start_addr[i],
                                                       section_end_addr[i],
                                                       (uint32)(section_start_addr[i] - section_start_addr[0])));
         SendBackSection(section_names[i],                                  // Section name
                         i+1,                                               // Section number
                         NULL,                                              // BFD asection pointer
                         section_start_addr[i],                             // Section start address/offset
                         (uint32)(section_start_addr[i] - section_start_addr[0]),   // Section offset
                         (uint32)(section_end_addr[i] - section_start_addr[i] + 1), // Section size
                         0,                                                 // Section flags
                         NULL,                                              // Where I have it loaded
                         1,                                                 // Executable section
                         cr);                                               // Context record
      }
      else {
         errmsg(("*E* GetKernelSymbolsFromMap: symbol name(s) for section '%s' not found\n", section_names[i]));
      }
   }

   //
   // Done
   //
   fclose(fd);
   dbghv(("< GetKernelSymbolsFromMap: Done enumerating. Found %d symbols.\n", NumberOfSymbols));
   dbghv(("*************** Found %d symbols\n", NumberOfSymbols));
   return (NumberOfSymbols);
}
#endif  //_PPC


#if defined(_S390)
//
// GetKernelSymbolsFromMap()
// *************************
//
// Read kernel map file (System.map) and extract symbols from it.
// Returns number of symbols found: 0 means none or error.
// Assumptions:
// * Map is sorted in ascending address order.
// * 1st symbol we care about is "_text". It is the kernel base address.
//
//
//   00000000 A _text                   <-- first real symbol (assumed to be base addr)
//   ...
//   001b52f8 t exit_shmem_fs           <-- start of ".text.exit" section
//   ...
//   001f2968 A _etext                  <-- end of .text section. Includes ".text.exit" section
//   ...
//   0029a000 A __init_begin            <-- start of ".text.init"
//   ...
//   002a6000 A __setup_start           <-- start of .setup.init section
//   ...
//   002a6128 A __initcall_start        <-- start of .initcall.init
//   ...
//   002a7000 A __init_end              <-- start of .data.cacheline_aligned
//   ...
//   002b2000 A __bss_start             <-- start of .bss
//   ...
//   0032b160 A _end                    <-- last line
//
int GetKernelSymbolsFromMap(CONTEXT_REC * cr, char * mapname)
{
#define SECTION_CNT  2                      // 2 text section we can identify
   int rc, i;
   int NumberOfSymbols = 0;                 // Total number of symbols
   int section_number = 1;

   FILE * fd;

   uint64 addr, prev_addr;
   uint64 base_addr;                        // Kernel base address
   uint64 end_addr;                         // End address of last executable section (guess)

   char type[2], prev_type;
   char name[1024], prev_name[1024];

   char * FirstRealSymbol = "_text";

   char * section_names[SECTION_CNT] = {".text", ".text.init"};

   uint64 section_start_addr[SECTION_CNT] = {0, 0},
          section_end_addr[SECTION_CNT] = {0, 0};

   uint32 prev_offset, prev_size;
   int symtype;

   char * symtype_p,
        * symtype_function = "Fnc",
        * symtype_label = "Lbl";


   dbghv(("> GetKernelSymbolsFromMap: fn='%s', cr=%p\n", mapname, cr));
   dbghv(("\n\n***** Map file \"%s\"\n", mapname));

   //
   // Open the file
   //
   fd = fopen(mapname, "r");
   if (fd == NULL ) {
      errmsg(("*E* GetKernelSymbolsFromMap: unable to open file '%s'.\n", mapname));
      return (NumberOfSymbols);             // No symbols found
   }

   //
   // Look for the "_text" symbol
   //
   rc = fscanf(fd, "%"_L64x" %s %s\n", &base_addr, type,name);
   while (rc != EOF  &&  strcmp(name, FirstRealSymbol) != 0) {
      rc = fscanf(fd, "%"_L64x" %s %s\n", &base_addr, type,name);
   }
   if (rc == EOF) {
      errmsg(("*E* GetKernelSymbolsFromMap: unable to find symbol '%s'.\n", FirstRealSymbol));
      fclose(fd);
      return (NumberOfSymbols);             // No symbols found
   }
   section_start_addr[0] = base_addr;       // Start of .text section

   //
   // "_text" is the first symbol.
   // base_addr contains the base address for the kernel.
   // Now look for the first "t" or "T" symbol whose address is different
   // than the kernel base address.
   //
   rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);
   while (rc != EOF  &&  addr == base_addr  &&  (type[0] != 't' || type[0] != 'T')) {
      rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);
   }
   if (rc == EOF) {
      errmsg(("*E* GetKernelSymbolsFromMap: unable to find symbol after '%s'.\n", FirstRealSymbol));
      fclose(fd);
      return (NumberOfSymbols);             // No symbols found
   }

   dbghv(("Lbl   0x%08x  0x%08x  %d  %d  %s\n", 0, (uint32)(addr - base_addr), STT_FUNC, section_number, FirstRealSymbol));
   rc = SendBackSymbol(FirstRealSymbol,      // Symbol name
                       0,                    // Symbol offset
                       (addr - base_addr),   // Symbol size
                       SYMBOL_TYPE_FUNCTION, // Symbol type
                       section_number,       // Section (don't know any better)
                       NULL,                 // No code
                       cr);                  // Context record
   if (rc) {
      dbghv(("< GetKernelSymbolsFromMap: SymbolCallback returned %d.  Stopping\n", rc));
      return (NumberOfSymbols);       // Error. Stop enumerating and return
                                      // number of symbols found so far.
   }
   NumberOfSymbols++;

   //
   // Now look for the rest of the symbols ...
   // Grab everything except for symbols in uninitialized data section (ie. B/b)
   //
   prev_addr = addr;
   strcpy(prev_name, name);
   prev_type = type[0];
   rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);
   while (rc != EOF) {
      if (prev_type != 'b' || prev_type != 'B') {         // D, R, T or A (ie. not B)
         if (prev_size == 0) {
            symtype_p = symtype_label;
            symtype = SYMBOL_TYPE_LABEL;
         }
         else {
            symtype_p = symtype_function;
            symtype = SYMBOL_TYPE_FUNCTION;
         }
         prev_offset = (uint32)(prev_addr - base_addr);
         prev_size = (uint32)(addr - prev_addr);

         //
         // Call callback function with each symbol.
         // Callback function returns 0 if it wants us to continue, non-zero
         // if it wants us to stop.
         //
         dbghv(("%s   0x%08x  0x%08x  %d  %d  %s\n", symtype_p, prev_offset,
                                                     prev_size, symtype, section_number, prev_name));
         rc = SendBackSymbol(prev_name,      // Symbol name
                             prev_offset,    // Symbol offset
                             prev_size,      // Symbol size
                             symtype,        // Symbol type
                             section_number, // Section (don't know any better)
                             NULL,           // No code
                             cr);            // Context record
         if (rc) {
            dbghv(("< GetKernelSymbolsFromMap: SymbolCallback returned %d.  Stopping\n", rc));
            return (NumberOfSymbols);       // Error. Stop enumerating and return
                                            // number of symbols found so far.
         }

         NumberOfSymbols++;
         end_addr = prev_addr + prev_size;
      }

      prev_addr = addr;
      strcpy(prev_name, name);
      prev_type = type[0];
      rc = fscanf(fd, "%"_L64x" %s %s\n", &addr, type,name);

      //
      // Try to determime start/end addresses of text sections.
      // These are approximate because we can't be 100% sure what they
      // are by reading the map - not all section start/end symbols
      // appear in the map.
      //
      if (type[0] == 'A') {
         for (i = 0; i < SECTION_CNT; i++) {
            if      (strcmp(name, "_etext") == 0) {
               section_end_addr[0] = addr;
            }
            else if (strcmp(name, "__init_begin") == 0) {
               section_start_addr[1] = addr;
               section_number++;
            }
            else if (strcmp(name, "__init_end") == 0) {
               section_end_addr[1] = addr;
            }
         }
      }
   }

   //
   // Send back the sections ...
   // Section 0 usually starts at address 0 so only check the ending address
   // to see whether it's 0 or not.  Assume that if the ending address is set
   // that we also set the starting address.
   //
   for (i = 0; i < SECTION_CNT; i++) {
      if (section_end_addr[i] != 0) {
         dbghv(("%s   0x"_LZ64x"  0x"_LZ64x"  offset:0x%08x\n", section_names[i], section_start_addr[i],
                                                                section_end_addr[i],
                                                                (uint32)(section_start_addr[i] - section_start_addr[0])));
         SendBackSection(section_names[i],                                  // Section name
                         i+1,                                               // Section number
                         NULL,                                              // BFD asection pointer
                         section_start_addr[i],                             // Section start address/offset
                         (uint32)(section_start_addr[i] - section_start_addr[0]),   // Section offset
                         (uint32)(section_end_addr[i] - section_start_addr[i] + 1), // Section size
                         0,                                                 // Section flags
                         NULL,                                              // Where I have it loaded
                         1,                                                 // Executable section
                         cr);                                               // Context record
      }
      else {
         errmsg(("*E* GetKernelSymbolsFromMap: symbol name(s) for section '%s' not found\n", section_names[i]));
      }
   }

   //
   // Done
   //
   fclose(fd);
   dbghv(("< GetKernelSymbolsFromMap: Done enumerating. Found %d symbols.\n", NumberOfSymbols));
   dbghv(("*************** Found %d symbols\n", NumberOfSymbols));
   return (NumberOfSymbols);
}
#endif  // _S390


//
// get_kernel_module_name()
// ************************
//
char * get_kernel_module_name(char * line)
{
   char * start_bracket;
   char * end_bracket;

   end_bracket = strrchr(line, ']');
   if (end_bracket) {
      start_bracket = strchr(line, '[');
      return (start_bracket);
   }
   else {
      return (G(KernelFilename));
   }
}


//
// skip_module()
// *************
//
// Returns NULL on EOF. Otherwise line contains last line read.
//
char * skip_module(FILE * fh, char * line, char * name)
{
   char * c;
   char * fname;

   dbghv(("> skip_module: %s\n", name));
   while ((c = fgets(line, 512, fh)) != NULL) {
      line[strlen(line)-1] = 0;
      fname = get_kernel_module_name(line);
      if (strcmp(fname, name) != 0)
         break;
   }
   dbghv(("< skip_module: %s end. c = %p\n", c));
   return (c);
}


//
// GetKernelModuleSymbolsFromKallsyms()
// ************************************
//
int GetKernelModuleSymbolsFromKallsyms(CONTEXT_REC * cr)
{
   int rc;
   int NumberOfSymbols = 0;
   int TotalNumberOfSymbols = 0;

   FILE * fh;
   char line[512];
   char * c;
   int64_t last_offset;
   int64_t cur_offset;

   char sym_name[512];
   char sym_type[8];
   uint64_t sym_addr;

   uint64_t keep_start, keep_end, keep_size;
   uint sym_offset;
   char cur_mod[128];
   char * nxt_mod = NULL;

   MOD_NODE * mn;

   dbghv(("> GetKernelModuleSymbolsFromKallsyms: fn='%s', cr=%p\n", G(KallsymsFilename), cr));
   dbghv(("\n\n***** kallsyms file \"%s\"\n", G(KallsymsFilename)));

   G(kernel_modules_harvested) = 1;
   fh = fopen(G(KallsymsFilename), "r");
   if (fh == NULL) {
      errmsg(("*E* GetKernelModuleSymbolsFromKallsyms: unable to open file '%s'.\n", G(KallsymsFilename)));
      return (TotalNumberOfSymbols);        // No symbols found
   }

   last_offset = G(kallsyms_file_offset) + G(kallsyms_file_size);
   fseeko(fh, (int64_t)G(kallsyms_file_offset), SEEK_SET);
   dbgmsg(("- GetKernelModuleSymbolsFromKallsyms: fn=%s  offset=%"_L64d"  size=%"_L64d"  last_offset=%"_L64d"\n",
           G(KallsymsFilename), G(kallsyms_file_offset), G(kallsyms_file_size), last_offset));


   cur_mod[0] = '\0';
   while ((c = fgets(line, 512, fh)) != NULL) {
      if (G(kallsyms_file_size)) {
         cur_offset = ftello(fh);
         if (cur_offset > last_offset) {
            dbghv(("- GetKernelModuleSymbolsFromKallsyms: *** just went past requested size. At %"_L64d", end %"_L64d" ***\n", cur_offset, last_offset));
            break;
         }
      }
      // c01018be t inflate_block                           <--- kernel
      // e09f50f5 t snd_mixer_oss_release [snd_mixer_oss]   <--- module
      sscanf(line, "%"_L64x" %s %s\n", &sym_addr, sym_type, sym_name);
      sym_type[0] = tolower(sym_type[0]);
      if (sym_type[0] == 'a' || sym_type[0] == 'u' || sym_type[0] == '?')
         continue;
      line[strlen(line)-1] = 0;

      nxt_mod = get_kernel_module_name(line);
      if (nxt_mod == G(KernelFilename))
         continue;

      if (strcmp(nxt_mod, cur_mod) != 0) {
         // changed modules
         if (cur_mod[0] != '\0') {
            // finish off the previous one
            SendBackSection(".ks-text",                   // Section name
                            1,                            // Section number
                            NULL,                         // BFD asection pointer
                            keep_start,                   // Section start address/offset
                            0,                            // Section offset
                            keep_size,                    // Section size
                            0,                            // Section flags
                            NULL,                         // Where I have it loaded
                            1,                            // Executable section
                            cr);                          // Context record
            dbghv(("- GetKernelModuleSymbolsFromKallsyms: *** finished off '%s' %d symbols\n", cur_mod, NumberOfSymbols));
            FixupSymbols(mn);
            TotalNumberOfSymbols += NumberOfSymbols;
            mn->hvname = zstrdup(G(KallsymsFilename));
            mn->flags |= A2N_FLAGS_SYMBOLS_NOT_FROM_IMAGE;
            mn->flags |= A2N_FLAGS_SYMBOLS_HARVESTED;
#if defined(_64BIT)
            mn->flags |= A2N_FLAGS_64BIT_MODULE;
#else
            mn->flags |= A2N_FLAGS_32BIT_MODULE;
#endif
         }

         // start a new one
         NumberOfSymbols = 0;
         dbghv(("- GetKernelModuleSymbolsFromKallsyms: *** starting '%s'\n", nxt_mod));
         mn = GetModuleNodeFromModuleFilename(nxt_mod, G(ModRoot));
         if (mn == NULL) {
            dbghv(("- GetKernelModuleSymbolsFromKallsyms: no mn for '%s'\n", nxt_mod));
            skip_module(fh, line, nxt_mod);
            continue;
         }

         strcpy(cur_mod, nxt_mod); // switch
         cr->handle = mn;

         keep_start = mn->last_lmn->start_addr;
         keep_size = mn->last_lmn->length;
         keep_end = keep_start + keep_size - 1;
         dbghv(("%s  %"_L64x" - %"_L64x" (%"_L64x")\n", cur_mod, keep_start, keep_end, keep_size));

         if (sym_addr >= keep_start && sym_addr <= keep_end) {
            sym_offset = (uint)(sym_addr - keep_start);
            dbghv(("Fnc   0x%08x  0x%"_L64x"  %s\n", sym_offset, sym_addr, sym_name));
            rc = SendBackSymbol(sym_name,             // Symbol name
                                sym_offset,           // Symbol offset
                                0,                    // Symbol size - figure it out later
                                SYMBOL_TYPE_FUNCTION, // Symbol type
                                1,                    // Section (don't know any better)
                                NULL,                 // No code
                                cr);                  // Context record
            if (rc) {
               dbghv(("< GetKernelModuleSymbolsFromKallsyms: SymbolCallback returned %d.  Stopping\n", rc));
               return (TotalNumberOfSymbols);  // Error. Stop enumerating and return
                                               // number of symbols found so far.
            }
            NumberOfSymbols++;
         }
      }
      else {
         // same module
         if (sym_addr >= keep_start && sym_addr <= keep_end) {
            sym_offset = (uint)(sym_addr - keep_start);
            dbghv(("Fnc   0x%08x  0x%"_L64x"  %s\n", sym_offset, sym_addr, sym_name));
            rc = SendBackSymbol(sym_name,             // Symbol name
                                sym_offset,           // Symbol offset
                                0,                    // Symbol size - figure it out later
                                SYMBOL_TYPE_FUNCTION, // Symbol type
                                1,                    // Section (don't know any better)
                                NULL,                 // No code
                                cr);                  // Context record
            if (rc) {
               dbghv(("< GetKernelModuleSymbolsFromKallsyms: SymbolCallback returned %d.  Stopping\n", rc));
               return (TotalNumberOfSymbols);  // Error. Stop enumerating and return
                                               // number of symbols found so far.
            }
            NumberOfSymbols++;
         }
      }
   }

   // take care of the last module we were working on
   if (cur_mod[0] != '\0' && mn != NULL) {
      // finish off the previous one
      SendBackSection(".ks-text",                   // Section name
                      1,                            // Section number
                      NULL,                         // BFD asection pointer
                      keep_start,                   // Section start address/offset
                      0,                            // Section offset
                      keep_size,                    // Section size
                      0,                            // Section flags
                      NULL,                         // Where I have it loaded
                      1,                            // Executable section
                      cr);                          // Context record
      dbghv(("- GetKernelModuleSymbolsFromKallsyms: *** finished off '%s' %d symbols\n", cur_mod, NumberOfSymbols));
      FixupSymbols(mn);
      TotalNumberOfSymbols += NumberOfSymbols;
      mn->hvname = zstrdup(G(KallsymsFilename));
      mn->flags |= A2N_FLAGS_SYMBOLS_NOT_FROM_IMAGE;
      mn->flags |= A2N_FLAGS_SYMBOLS_HARVESTED;
#if defined(_64BIT)
      mn->flags |= A2N_FLAGS_64BIT_MODULE;
#else
      mn->flags |= A2N_FLAGS_32BIT_MODULE;
#endif
   }

   // Done
   fclose(fh);
   dbghv(("< GetKernelModuleSymbolsFromKallsyms: Done enumerating. Found %d symbols.\n", TotalNumberOfSymbols));
   dbghv(("*************** Found %d symbols\n", TotalNumberOfSymbols));
   return (TotalNumberOfSymbols);
}


//
// GetKernelSymbolsFromKallsyms()
// ******************************
//
int GetKernelSymbolsFromKallsyms(CONTEXT_REC * cr)
{
   int rc;
   int NumberOfSymbols = 0;

   FILE * fh;
   char line[512];
   char * c;
   int64_t last_offset;
   int64_t cur_offset;

   char sym_name[512];
   uint64_t sym_addr;

   uint64_t kernel_start;
   uint sym_offset;
   char * cur_mod = NULL;

   MOD_NODE * mn = cr->handle;

   dbghv(("> GetKernelSymbolsFromKallsyms: fn='%s', cr=%p\n", G(KallsymsFilename), cr));
   dbghv(("\n\n***** kallsyms file \"%s\"\n", G(KallsymsFilename)));

   fh = fopen(G(KallsymsFilename), "r");
   if (fh == NULL) {
      errmsg(("*E* GetKernelSymbolsFromKallsyms: unable to open file '%s'.\n", G(KallsymsFilename)));
      return (NumberOfSymbols);             // No symbols found
   }

   last_offset = G(kallsyms_file_offset) + G(kallsyms_file_size);
   fseeko(fh, (int64_t)G(kallsyms_file_offset), SEEK_SET);
   dbgmsg(("- GetKernelSymbolsFromKallsyms: fn=%s  offset=%"_L64d"  size=%"_L64d"  last_offset=%"_L64d"\n",
           G(KallsymsFilename), G(kallsyms_file_offset), G(kallsyms_file_size), last_offset));

   kernel_start = mn->last_lmn->start_addr;
   dbghv(("%s  %"_L64x" (%"_L64x")\n", mn->name, kernel_start, mn->last_lmn->length));

   while ((c = fgets(line, 512, fh)) != NULL) {
      if (G(kallsyms_file_size)) {
         cur_offset = ftello(fh);
         if (cur_offset > last_offset) {
            dbghv(("- GetKernelSymbolsFromKallsyms: *** just went past requested size. At %"_L64d", end %"_L64d" ***\n", cur_offset, last_offset));
            break;
         }
      }
      // c01018be t inflate_block                           <--- kernel
      // e09f50f5 t snd_mixer_oss_release [snd_mixer_oss]   <--- module
      sscanf(line, "%"_L64x" %*s %s\n", &sym_addr, sym_name);
      line[strlen(line)-1] = 0;
      if (sym_addr >> 32 == 0)
    	  continue;
      cur_mod = get_kernel_module_name(line);
      if (cur_mod != G(KernelFilename)) {
    	  mn->last_lmn->length = sym_addr - mn->last_lmn->start_addr;
          break;
      }
      if(!kernel_start) {
    	  kernel_start = sym_addr;
    	  mn->last_lmn->start_addr = sym_addr;
      }

      sym_offset = (uint)(sym_addr - kernel_start);
      dbghv(("Fnc   0x%08x  0x%"_L64x"  %s\n", sym_offset, sym_addr, sym_name));
      rc = SendBackSymbol(sym_name,             // Symbol name
                          sym_offset,           // Symbol offset
                          0,                    // Symbol size - figure it out later
                          SYMBOL_TYPE_FUNCTION, // Symbol type
                          1,                    // Section (don't know any better)
                          NULL,                 // No code
                          cr);                  // Context record
      if (rc) {
         dbghv(("< GetKernelSymbolsFromKallsyms: SymbolCallback returned %d.  Stopping\n", rc));
         return (NumberOfSymbols);          // Error. Stop enumerating and return
                                            // number of symbols found so far.
      }
      NumberOfSymbols++;
   }

   // Done. Finish it up
   SendBackSection(".ks-text",                   // Section name
                   1,                            // Section number
                   NULL,                         // BFD asection pointer
                   kernel_start,                 // Section start address/offset
                   0,                            // Section offset
                   mn->last_lmn->length,         // Section size
                   0,                            // Section flags
                   NULL,                         // Where I have it loaded
                   1,                            // Executable section
                   cr);                          // Context record
   dbghv(("- GetKernelSymbolsFromKallsyms: *** finished off '%s' %d symbols\n", mn->name, NumberOfSymbols));
   mn->hvname = zstrdup(G(KallsymsFilename));
   mn->flags |= A2N_FLAGS_SYMBOLS_NOT_FROM_IMAGE;
   mn->flags |= A2N_FLAGS_SYMBOLS_HARVESTED;
   mn->flags |= A2N_FLAGS_VALIDATION_NOT_DONE;
#if defined(_64BIT)
   mn->flags |= A2N_FLAGS_64BIT_MODULE;
#else
   mn->flags |= A2N_FLAGS_32BIT_MODULE;
#endif
   G(validate_symbols) = 0;
   dbghv(("- GetKernelSymbolsFromKallsyms: *** forcing kernel symbol validation off! ***\n"));
   mn->type = MODULE_TYPE_KALLSYMS_VMLINUX;

   // Done
   fclose(fh);
   dbghv(("< GetKernelSymbolsFromKallsyms: Done enumerating. Found %d symbols.\n", NumberOfSymbols));
   dbghv(("*************** Found %d symbols\n", NumberOfSymbols));
   return (NumberOfSymbols);
}
