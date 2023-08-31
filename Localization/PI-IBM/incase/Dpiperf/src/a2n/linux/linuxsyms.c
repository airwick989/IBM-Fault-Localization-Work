/*   Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2009
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
//                           Linux Symbol Harvester                         //
//                           ----------------------                         //
//                                                                          //
// REQUIRED functions:                                                      //
//  - GetHarvesterVersion()                                                 //
//  - GetSymbolsForModule()                                                 //
//                                                                          //
// OPTIONAL functions:                                                      //
//  - ValidateKernelSymbols()                                               //
//**************************************************************************//
//
#include "a2nhdr.h"


#define SEGMENT_TYPE_EXECUTE(flag)    ((flag) & PF_X)
#define SEGMENT_TYPE_WRITE(flag)      ((flag) & PF_W)
#define SEGMENT_TYPE_READ(flag)       ((flag) & PF_R)


// kernel types
#define NOT_KERNEL      0
#define KERNEL_IMAGE    1
#define KERNEL_MAP      2


// TryToGetSymbols() return codes
#define NO_ACCESS_TO_FILE         -1
#define FILE_TYPE_NOT_SUPPORTED   -2
#define ERROR_ACCESSING_FILE      -3
#define INTERNAL_ERROR            -4
#define EXE_FORMAT_NOT_SUPPORTED  -5


//
// Externs (from a2n.c)
//
extern gv_t a2n_gv;                         // Globals





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
uint GetHarvesterVersion(void)
{
   return ((uint)H_VERSION_NUMBER);
}


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
//
// Symbol harvesting rules for the Linux kernel:
// *********************************************
// (for the latest rules see make_kernel_symbols_choice() in a2nint.c)
//
// Search order is as follows:
//
// 1) If use_kernel_image (either told or chosen by us) try that.
//    - If it does not contain symbols go to step 2.
//    - If it contains symbols harvest them.
//      - Will attempt validation.
//      - If it contains code, and code is requested, harvest it.
// 2) If use_kallsyms (either told or chosen by us) try that.
//    - If it does not contain symbols go to step 3.
//    - If it contains symbols harvest them.
//      - No validation needed.
//      - No code saved.
// 3) If use_kernel_map (either told or chosen by us) try that.
//    - If it does not contain symbols go to step 4.
//    - If it contains symbols harvest them.
//      - Will attempt validation.
//      - No code saved.
// 4) Give up.
//
// Symbol harvesting rules for other modules:
// ******************************************
//
// 1) Try on-disk executable (given by module name)
//    - If it contains code, and code is requested, harvest it. Code is
//      only harvested from the on-disk executable.
//    - If it contains symbols harvest them.
//    - If it does not contain symbols go to step 2.
//
// 2) Try a "map" file (the same name as the requested module with
//    ".map" appended to it) in same directory as executable module.
//    - If symbols are found harvest them.
//    - If no symbols are found then go to step 3.
//
// 3) For each directory in "searchpath" (if any):
//    a) Try /directory/modulename.map
//       - If file contains symbols then harvest them.
//    If no symbols are found in any directory/file, go to step 4.
//
// 4) Give up.
//
int GetSymbolsForModule(void * handle,           // Really a pointer to a Module node
                        char * modname,          // Fully qualified Module name
                        char * searchpath,       // Search path
                        uint64 loadaddr,         // Load virtual address
                        void * symbol_callback,  // Call-back function
                        void * section_callback) // Call-back function
{
   CONTEXT_REC cr;
   int rc;
   MOD_NODE * mn;
   char * c, * pc, * pname;
   char module_name_without_path[MAX_FILENAME_LEN];
   char sname[MAX_PATH_LEN];
   char local_searchpath[MAX_PATH_LEN];
   char sp_sep[2], p_sep[2];


   dbgmsg(("> GetSymbolsForModule: handle=%p, name='%s', searchpath='%s' (%d), loadaddr=%p, symcallbak=%p, seccallback=%p\n",
           handle, modname, searchpath, strlen(searchpath), loadaddr, symbol_callback, section_callback));
   //
   // Context saved here and used by the real callback function
   //
   cr.handle = handle;
   cr.load_addr = loadaddr;
   cr.symtype = 0;
   cr.hflags = 0;
   cr.symbol_callback = symbol_callback;
   cr.section_callback = section_callback;

   mn = (MOD_NODE *)handle;

   //
   // If this is the kernel then follow the special kernel rules:
   // 1- If KernelImageName is set try that
   // 2- If KallsymsFilename is set try that
   // 3- If KernelMapName is set try that
   // 4- Give up
   //
   if (mn->flags & A2N_FLAGS_KERNEL) {
      // (1) If they want us to try the image then try it
      if (G(ok_to_use_kernel_image)) {
         if (G(use_kernel_image) && G(KernelImageName) != NULL) {
            pname = G(KernelImageName);
            rc = TryToGetSymbols(&cr, G(KernelImageName), KERNEL_IMAGE);
            if (rc > 0) {
               dbgmsg(("- GetSymbolsForModule: %d kernel symbols found in '%s'\n", rc, pname));
               goto TheEnd;                    // Had symbols. Done.
            }
         }
      }

      // (2) If they want us to try kallsyms then try it
      if (G(ok_to_use_kallsyms)) {
         if (G(use_kallsyms) && G(KallsymsFilename) != NULL) {
            pname = G(KallsymsFilename);
            rc = GetKernelSymbolsFromKallsyms(&cr);
            if (rc > 0) {
               dbgmsg(("< GetSymbolsForModule: %d kernel symbols found in %s.\n", rc, pname));
               goto TheEnd;                       // Had symbols
            }
         }
      }

      // (3) If they want us to try the map then try it
      if (G(ok_to_use_kernel_map)) {
         if (G(use_kernel_map) && G(KernelMapName) != NULL) {
            pname = G(KernelMapName);
            rc = TryToGetSymbols(&cr, G(KernelMapName), KERNEL_MAP);
            if (rc > 0) {
               dbgmsg(("- GetSymbolsForModule: %d kernel symbols found in '%s'\n", rc, pname));
               goto TheEnd;                    // Had symbols. Done.
            }
         }
      }

      // (4) Give up
      dbgmsg(("< GetSymbolsForModule: kernel and no symbols found\n"));
      return (NO_ACCESS_TO_FILE);
   }

   //
   // Not the kernel. Is it a kernel module?
   //
   if (mn->type == MODULE_TYPE_KALLSYMS_MODULE) {
      if (G(kernel_modules_harvested)) {
         dbgmsg(("< GetSymbolsForModule: kernel module and already harvested.\n"));
         return (0);
      }
      if (G(ok_to_use_modules)) {
         if (G(use_modules)) {
            rc = GetKernelModuleSymbolsFromKallsyms(&cr);
            dbgmsg(("< GetSymbolsForModule: %d kernel module symbols found.\n", rc));
            if (rc > 0)
               return (0);                  // Had symbols. Doesn't necessarily mean
                                            // we got symbols for the kernel module
                                            // that got us here though.
            else
               return (NO_ACCESS_TO_FILE);  // No symbols in kallsyms, couldn't read
                                            // the file, the file doesn't exist, etc.
         }
         else {
            dbgmsg(("< GetSymbolsForModule: kernel module but not OK to harvest.\n"));
            return (NO_ACCESS_TO_FILE);
         }
      }
   }

   //
   // Not the kernel and not a kernel module.
   // Peel off the module name (the "thing" after the last "/").
   //
   c = GetFilenameFromPath(modname);
   strcpy(module_name_without_path, c);     // Only the module name (no path)

   //
   // Try the on-disk module as given by A2nAddModule()
   //
   pname = modname;
   rc = TryToGetSymbols(&cr, modname, NOT_KERNEL);
   if (rc > 0)
      goto TheEnd;                          // This guy had symbols
   if (rc == EXE_FORMAT_NOT_SUPPORTED)
      goto TheEnd;                    // Don't even try anything else

   //
   // OK, now try the map on the same directory
   //
   pname = sname;
   strcpy(sname, modname);                  // Copy of module name ...
   strcat(sname, ".map");                   // ... with ".map" appended

   rc = TryToGetSymbols(&cr, sname, NOT_KERNEL);
   if (rc > 0)
      goto TheEnd;

   //
   // If there is no search path then we're done - no place else left
   // to look for symbols.
   //
   if (searchpath == NULL  ||  *searchpath == '\0') {
      dbgmsg(("< GetSymbolsForModule: no symbols in executable/map and NULL searchpath\n"));
      return (NO_ACCESS_TO_FILE);           // NULL searchpath: done.
   }

   //
   // Make local copy of searchpath (strtok mucks with it) and build
   // strings for the SearchPathSeparator and PathSeparator characters.
   //
   strcpy(local_searchpath, searchpath);
   sp_sep[0] = G(SearchPathSeparator);  sp_sep[1] = 0;
   p_sep[0] = G(PathSeparator);  p_sep[1] = 0;

   //
   // Now repeat above for each component of searchpath.
   // Look for map files *ONLY*
   //
   pc = strtok(local_searchpath, sp_sep);   // Prime strtok
   while(pc) {
      strcpy(sname, pc);
      strcat(sname, p_sep);
      strcat(sname, module_name_without_path);
      strcat(sname, ".map");                // Try module.map
      rc = TryToGetSymbols(&cr, sname, 0);
      if (rc > 0)
         goto TheEnd;

      pc = strtok(NULL, sp_sep);            // Grab the next search path component
   }

   //
   // If we fall thru to here we did not find symbols anywhere
   // for this module.
   //
   dbgmsg(("< GetSymbolsForModule: no symbols in executable/map anywhere\n"));
   return (NO_ACCESS_TO_FILE);              // Nothing found anywhere


   //
   // Only way to get here is by having found symbols either in the
   // module itself, its map, or copies of either in any of the
   // directories in the search path.
   //
TheEnd:
   mn->hvname = zstrdup(pname);              // Save actual module name
   if (strcmp(mn->hvname, mn->name) != 0)
      mn->flags |= A2N_FLAGS_SYMBOLS_NOT_FROM_IMAGE;  // Symbols not obtained from on-disk image

   dbgmsg(("< GetSymbolsForModule: %d symbols found in '%s'\n", rc, pname));
   return (0);
}


//
// SendBackSymbol()
// ****************
//
// Actual callback function invoked by the harvester for each symbol.
// Can be used to "filter" or pre-process symbols before actually
// handing them back to the requestor.
// Returns: 0 to allow harvester to continue
//          non-zero to force harvester to stop
//
int SendBackSymbol(char * symbol_name,      // Symbol name
                   uint symbol_offset,      // Symbol offset
                   uint symbol_size,        // Symbol size
                   int symbol_type,         // Symbol type (label, export, etc.)
                   int section,             // Section to which the symbol belongs
                   char * code,             // Code associated with this symbol
                   void * context)          // Context record
{
   SYMBOL_REC sr;
   CONTEXT_REC * cr = (CONTEXT_REC *)context;
   PFN_SYMBOL_CALLBACK UserSymbolCallBack = cr->symbol_callback;
   int rc = 0;

   if (UserSymbolCallBack != NULL) {
      sr.name = symbol_name;
      sr.offset = symbol_offset;
      sr.length = symbol_size;
      sr.type = symbol_type;
      sr.section = section;
      sr.code = code;
      sr.handle = cr->handle;
      rc = UserSymbolCallBack(&sr);            // Call user callback function
   }
   return (rc);
}


//
// SendBackSection()
// *****************
//
// Actual callback function invoked by the harvester for each section.
// Can be used to "filter" or pre-process sections before actually
// handing them back to the requestor.
// Returns: 0 to allow harvester to continue
//          non-zero to force harvester to stop
//
int SendBackSection(char * section_name,     // Section name
                    int section_number,      // Section number
                    void * asec,             // BFD section pointer
                    uint64 start_addr,       // Section start address/offset
                    uint section_offset,     // Section offset
                    uint section_size,       // Section size
                    uint section_flags,      // Section flags
                    void * loc_addr,         // Where I have it loaded
                    int executable,          // Executable section or not
                    void * context)          // Context record
{
   SECTION_REC sr;
   CONTEXT_REC * cr = (CONTEXT_REC *)context;
   PFN_SECTION_CALLBACK UserSectionCallBack = cr->section_callback;
   int rc = 0;

   if (UserSectionCallBack != NULL) {
      sr.name = section_name;
      sr.number = section_number;
      sr.asec = asec;
      sr.executable = executable;
      sr.start_addr = start_addr;
      sr.offset = section_offset;
      sr.size = section_size;
      sr.flags = section_flags;
      sr.loc_addr = loc_addr;
      sr.handle = cr->handle;
      rc = UserSectionCallBack(&sr);           // Call user callback function
   }
   return (rc);
}


//
// TryToGetSymbols()
// *****************
//
// Attempts to harvest symbols from the given file (which could be
// an ELF executable or a MAP file).
// Possible outcomes are:
// * 0      File exists but does not contain symbols
// * > 0    File exists and contains symbols
// * -1     Unable to access file (does not exist or no read permission)
// * -2     Not a file type we support
// * -3     File exists and we understand it but had problems accessing
//
int TryToGetSymbols(CONTEXT_REC * cr, char * modname, int kernel)
{
   int module_type, symcnt;
   void * mapped_addr;
   size_t filesize;
   MOD_NODE * mn;
   bfd * abfd;


   dbgmsg(("> TryToGetSymbols: name=%s, kernel=%d\n", modname, kernel));
   //
   // Make sure file exists and we can read it
   //
   if (!FileIsReadable(modname)) {
      dbgmsg(("< TryToGetSymbols: access() failed for '%s'. errno=%d\n", modname, errno));
      return (NO_ACCESS_TO_FILE);        // No access to the file
   }

   //
   // Figure out if module type is one we support
   //
   module_type = GetModuleType(modname, &abfd, kernel);
   if (module_type == MODULE_TYPE_INVALID) {
      dbgmsg(("< TryToGetSymbols: (MODULE_TYPE_INVALID)\n"));
      return (FILE_TYPE_NOT_SUPPORTED);     // Not a file type we support
   }

   //
   // Set module type in module node
   //
   mn = (MOD_NODE *)cr->handle;
   mn->type = module_type;                  // Save module type

   //
   // Handle executables ...
   //
   if (module_type == MODULE_TYPE_ELF_REL || module_type == MODULE_TYPE_ELF_EXEC ||
       module_type == MODULE_TYPE_ELF64_REL || module_type == MODULE_TYPE_ELF64_EXEC) {
      //
      // ELF executable. Work on it ...
      // Map the module, get symbols and unmap the file
      //
      mapped_addr = MapFile(modname, &filesize);
      if (mapped_addr == NULL) {
         dbgmsg(("< TryToGetSymbols: (MapFile() returned NULL)\n"));
         return (ERROR_ACCESSING_FILE);     // Error accessing file
      }
      cr->mod_addr = mapped_addr;
      if (module_type == MODULE_TYPE_ELF_REL || module_type == MODULE_TYPE_ELF_EXEC)
         symcnt = GetSymbolsFromElfExecutable(cr, modname, module_type, abfd);
      else
         symcnt = GetSymbolsFromElf64Executable(cr, modname, module_type, abfd);
      UnmapFile(mapped_addr);

      dbgmsg(("< TryToGetSymbols: %d symbols found in executable\n", symcnt));
      return (symcnt);
   }

   //
   // Handle maps ...
   //
   if (module_type == MODULE_TYPE_MAP_NM) {
      if (kernel == NOT_KERNEL)
         symcnt = GetSymbolsFromMap_nm(cr, modname);
      else
         symcnt = GetKernelSymbolsFromMap(cr, modname);
   } else if (module_type == MODULE_TYPE_MAP_OBJDUMP) {
      symcnt = GetSymbolsFromMap_objdump(cr, modname);
   } else if (module_type == MODULE_TYPE_MAP_READELF) {
      symcnt = GetSymbolsFromMap_readelf(cr, modname);
   } else {
      errmsg(("*E* TryToGetSymbols: ***** INTERNAL ERROR: unknown map type for '%s' *****\n", modname));
      return (INTERNAL_ERROR);              // Error accessing file
   }

   dbgmsg(("< TryToGetSymbols: %d symbols found in map\n", symcnt));
   return (symcnt);
}


//
// GetModuleType()
// ***************
//
// Maps an executable and determines the type (unless the type can be
// determined by the file extension).
// Returns the module type (or MODULE_TYPE_INVALID if it can't determine type).
//
int GetModuleType(char * filename, bfd ** abfd, int kernel)
{
   char * extension;
   int rc;
   size_t bytes_read;
   FILE * fh = NULL;
   Elf64_Ehdr eh;                           // Elf Header (64-bit one is bigger)


   dbgmsg(("> GetModuleType: fn='%s'\n", filename));
   *abfd = NULL;

   //
   // Check if kernel map (which may or may not have a .map extension).
   // Let GetMapType() sort out if it's a map I understand.
   //
   if (kernel == KERNEL_MAP) {
      rc = GetMapType(filename);
      goto TheEnd;
   }

   //
   // Get extension and set the type if we can tell what the file is
   // just by the extension.
   // Linux only supports .map or non-stripped executales.
   //
   extension = GetExtensionFromFilename(filename);
   if (extension != NULL) {
      if (stricmp(extension, ".map") == 0) {
         //
         // Some kind of map. Let GetMapType() sort it out.
         //
         rc = GetMapType(filename);
         goto TheEnd;
      }
      else if (stricmp(extension, ".nm") == 0) {
         dbghv(("- GetModuleType: (MODULE_TYPE_MAP_NM)\n"));
         rc = MODULE_TYPE_MAP_NM;           // (.MAP) Linux map file generated via "nm -a"
         goto TheEnd;
      }
      else if (stricmp(extension, ".objdump") == 0) {
         dbghv(("- GetModuleType: (MODULE_TYPE_MAP_OBJDUMP)\n"));
         rc = MODULE_TYPE_MAP_OBJDUMP;      // (.MAP) Linux map file generated via "objdump -t"
         goto TheEnd;
      }
      else if (stricmp(extension, ".readelf") == 0) {
         dbghv(("- GetModuleType: (MODULE_TYPE_MAP_READELF)\n"));
         rc = MODULE_TYPE_MAP_READELF;      // (.MAP) Linux map file generated via "readelf -s"
         goto TheEnd;
      }
   }

   //
   // Can't determine by extension. Look inside.
   //
   fh = fopen(filename, "r");
   if (fh == NULL ) {
      rc = MODULE_TYPE_INVALID;
      errmsg(("*E* GetModuleType: unable to open file '%s'.\n", filename));
      goto TheEnd;
   }

   //
   // OK, read the header in ...
   //
   bytes_read = fread((void *)&eh, sizeof(char), sizeof(eh), fh);
   if (bytes_read < sizeof(eh)) {
      rc = MODULE_TYPE_INVALID;
      errmsg(("*E* GetModuleType: module size too small.\n"));
      goto TheEnd;
   }
   DumpElfHeader(&eh, filename);

   //
   // See if it's ELF or not.  Check signature ...
   //
   if (eh.e_ident[EI_MAG0] != ELFMAG0 ||
       eh.e_ident[EI_MAG1] != ELFMAG1 ||
       eh.e_ident[EI_MAG2] != ELFMAG2 ||
       eh.e_ident[EI_MAG3] != ELFMAG3) {
      rc = MODULE_TYPE_INVALID;
      dbgmsg(("- GetModuleType: Invalid ELF signature. Expected: 0x%02x%02x%02x%02x  Found: 0x%02x%02x%02x%02x\n",
              ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3,
              eh.e_ident[EI_MAG0], eh.e_ident[EI_MAG1], eh.e_ident[EI_MAG2], eh.e_ident[EI_MAG3]));
      goto TheEnd;
   }

   //
   // Good signature. Make sure machine size and architecture matches ...
   //
   // e_machine     eh.e_ident[EI_CLASS]
   //  ---------     --------------------
   //  EM_386        ELFCLASS32                x86 32 bit
   //  EM_IA_64      ELFCLASS64                ia64 64 bit
   //  EM_PPC        ELFCLASS32                ppc 32 bit
   //  EM_PPC64      ELFCLASS64                ppc 64 bit
   //  EM_S390       ELFCLASS32                s390 32 bit
   //  EM_S390_OLD   ELFCLASS32                s390 32 bit
   //  EM_????       ELFCLASS64                s390 64 bit
   //
   // Don't care about machine anymore.  All we do is make sure the
   // image is either Executable or Relocatable and what size (32/64 bits).
   // - EXECUTABLEs are load images (ie. not relocatable)
   // - RELOCATABLEs are just that
   //
   if (eh.e_type == ET_REL || eh.e_type == ET_DYN) {
      if (eh.e_ident[EI_CLASS] == ELFCLASS32) {
         dbghv(("- GetModuleType: (MODULE_TYPE_ELF_REL)\n"));
         rc = MODULE_TYPE_ELF_REL;
      }
      else {
         dbghv(("- GetModuleType: (MODULE_TYPE_ELF64_REL)\n"));
         rc = MODULE_TYPE_ELF64_REL;
      }
   }
   else if (eh.e_type == ET_EXEC) {
      if (eh.e_ident[EI_CLASS] == ELFCLASS32) {
         dbghv(("- GetModuleType: (MODULE_TYPE_ELF_EXEC)\n"));
         rc = MODULE_TYPE_ELF_EXEC;
      }
      else {
         dbghv(("- GetModuleType: (MODULE_TYPE_ELF64_EXEC)\n"));
         rc = MODULE_TYPE_ELF64_EXEC;
      }
   }
   else {
      dbghv(("- GetModuleType: Module (%d) not executable or relocatable. (MODULE_TYPE_INVALID)\n", eh.e_type));
      rc = MODULE_TYPE_INVALID;
   }

   //
   // Done
   //
TheEnd:
   if (fh != NULL)
      fclose(fh);

   dbgmsg(("< GetModuleType: rc=%d\n", rc));
   return (rc);
}


//
// PrintBytes()
// ************
//
void PrintBytes(unsigned char * s, int n)
{
   int i, j, bytes, iterations;

   if ((G(debug) & DEBUG_MODE_HARVESTER) == 0)
      return;
   if (s == NULL  ||  n == 0  ||  n > 128)
      return;

   iterations = (n + 15) / 16;
   dbghv(("code="));
   for (i = 0; i < iterations; i++, n -= 16) {
      if (i != 0)
         dbghv(("     "));
      bytes = (n < 16) ? n : 16;
      for (j = 0; j < bytes; j++,s++) {
         dbghv(("%02x ", *s));
      }
      dbghv(("\n"));
   }
   return;
}


//
// GetCodePtrForSymbol()
// *********************
//
char * GetCodePtrForSymbol(char * sym_addr, SECTION_INFO * si, int num_secs)
{
   int i;
   char * cp = NULL;                        // code pointer
   char * end_offset;

   if (!G(gather_code))
      return (cp);                          // Not gathering code

   //
   // Look for this symbol's address in all executable sections until
   // we find which section it falls in.  Then calculate the offset
   // into the section and add that to the base address of the section.
   //
   dbghv(("* GetCodePtrForSymbol: sym_addr = %p num_secs = %d\n", sym_addr, num_secs));
   for (i = 0; i < num_secs; i++) {
      end_offset = (char *)Uint32ToPtr((si[i].offset + si[i].size - 1));
      dbghv(("- GetCodePtrForSymbol: comparing %p to %p and %p\n",
              sym_addr,si[i].offset, end_offset));
      if (sym_addr >= (char *)Uint32ToPtr(si[i].offset)  &&  sym_addr <= end_offset) {
         cp = (char *)PtrAdd(si[i].loc_addr, (sym_addr - si[i].offset));
         dbghv(("code_ptr(%p) = %p  + (0x%08x - 0x%08x)\n", cp, si[i].loc_addr, sym_addr, si[i].offset));
         break;
      }
   }

   return (cp);
}


//
// AddKernelModules()
// ******************
//
void AddKernelModules(void)
{
   FILE * fh = NULL;
   char line[512];
   char tbuf[80];
   char * c;
   int64_t last_offset;
   int64_t cur_offset;

   char mod_name[80];
   int  mod_size;
   uint64_t mod_addr;

   LMOD_NODE * lmn;


   dbgmsg(("> AddKernelModules: start\n"));

   if (!G(use_modules)) {
      dbgmsg(("< AddKernelModules: use_modules not set. Quitting.\n"));
      return;
   }
   if (G(kernel_modules_added)) {
      dbgmsg(("< AddKernelModules: Already did it. Quitting.\n"));
      return;
   }
   if (G(SystemPidNode) == NULL) {
      dbgmsg(("< AddKernelModules: SystemPidNode is NULL. Quitting.\n"));
      return;
   }
   if (G(ModulesFilename) == NULL) {
      dbgmsg(("< AddKernelModules: ModulesFilename is NULL. Quitting.\n"));
      return;
   }

   fh = fopen(G(ModulesFilename), "r");
   if (fh == NULL ) {
      errmsg(("*E* AddKernelModules: Unable to open file '%s'.\n", G(ModulesFilename)));
      return;
   }

   last_offset = G(modules_file_offset) + G(modules_file_size);
   fseeko(fh, (int64_t)G(modules_file_offset), SEEK_SET);
   dbgmsg(("- AddKernelModules: fn=%s  offset=%"_L64d"  size=%"_L64d"  last_offset=%"_L64d"\n",
           G(ModulesFilename), G(modules_file_offset), G(modules_file_size), last_offset));

   while ((c = fgets(line, 512, fh)) != NULL) {
      dbghv(("- AddKernelModules:  read: %s\n", line));
      if (G(modules_file_size)) {
         cur_offset = ftello(fh);
         if (cur_offset > last_offset) {
            dbghv(("- AddKernelModules: *** just went past requested size. At %"_L64d", end %"_L64d" ***\n", cur_offset, last_offset));
            break;
         }
      }
      // iptable_filter 2753 1 - Live 0xe101b000
      // ip_tables 16833 3 ipt_REJECT,ipt_state,iptable_filter, Live 0xe10a6000
      sscanf(line, "%s %d %*d %*s %*s %"_L64x"\n", tbuf, &mod_size, &mod_addr);
      strcpy(mod_name, "[");
      strcat(mod_name, tbuf);
      strcat(mod_name, "]");
      dbgmsg(("- AddKernelModules: ***** read %s %d %"_L64x" *****\n", tbuf, mod_size, mod_addr));
      lmn = AddLoadedModuleNode(G(SystemPidNode), mod_name, mod_addr, mod_size, 0, 0, 0);
      if (lmn)
         lmn->mn->type = MODULE_TYPE_KALLSYMS_MODULE;
   }

   fclose(fh);
   G(kernel_modules_added) = 1;
   dbgmsg(("< AddKernelModules: end\n"));
   return;
}


//
// DumpElfHeader()
// ***************
//
void DumpElfHeader(Elf64_Ehdr * eh, char * modname)
{
   if (!G(debug))
      return;

   if (eh->e_ident[EI_CLASS] == ELFCLASS32) {
      Elf32_Ehdr * eh32 = (Elf32_Ehdr *)eh;
      dbghv(("\n\n***** ELF32 Header for \"%s\"\n", modname));
      dbghv(("  >>> Loaded at: %p\n",eh32));
      dbghv(("      Signature: 0x%02x%02x%02x%02x\n", eh32->e_ident[EI_MAG0], eh32->e_ident[EI_MAG1],
                                                      eh32->e_ident[EI_MAG2], eh32->e_ident[EI_MAG3]));
      dbghv(("     File Class: 0x%02x\n", eh32->e_ident[EI_CLASS]));
      dbghv(("   DataEncoding: 0x%02x\n", eh32->e_ident[EI_DATA]));
      dbghv(("     HdrVersion: 0x%02x\n", eh32->e_ident[EI_VERSION]));
      dbghv(("      OS/ABI Id: 0x%02x\n", eh32->e_ident[EI_OSABI]));
      dbghv(("     ABIVersion: 0x%02x\n", eh32->e_ident[EI_ABIVERSION]));
      dbghv(("           Type: 0x%04x\n", eh32->e_type));
      dbghv(("        Machine: 0x%04x\n", eh32->e_machine));
      dbghv(("        Version: 0x%08x\n", eh32->e_version));
      dbghv(("     EntryPoint: 0x%08x\n", eh32->e_entry));
      dbghv(("    ProgramHdrs: 0x%08x\n", eh32->e_phoff));
      dbghv(("    SectionHdrs: 0x%08x\n", eh32->e_shoff));
      dbghv(("          Flags: 0x%08x\n", eh32->e_flags));
      dbghv(("     ElfHdrSize: 0x%04x\n", eh32->e_ehsize));
      dbghv(("    ProgHdrSize: 0x%04x\n", eh32->e_phentsize));
      dbghv(("     ProgHdrCnt: 0x%04x\n", eh32->e_phnum));
      dbghv(("    SectHdrSize: 0x%04x\n", eh32->e_shentsize));
      dbghv(("     SectHrdCnt: 0x%04x\n", eh32->e_shnum));
      dbghv((" StringTblIndex: 0x%04x\n", eh32->e_shstrndx));
   }
   else {
      dbghv(("\n\n***** ELF64 Header for \"%s\"\n", modname));
      dbghv(("  >>> Loaded at: %p\n",eh));
      dbghv(("      Signature: 0x%02x%02x%02x%02x\n", eh->e_ident[EI_MAG0], eh->e_ident[EI_MAG1],
                                                      eh->e_ident[EI_MAG2], eh->e_ident[EI_MAG3]));
      dbghv(("     File Class: 0x%02x\n", eh->e_ident[EI_CLASS]));
      dbghv(("   DataEncoding: 0x%02x\n", eh->e_ident[EI_DATA]));
      dbghv(("     HdrVersion: 0x%02x\n", eh->e_ident[EI_VERSION]));
      dbghv(("      OS/ABI Id: 0x%02x\n", eh->e_ident[EI_OSABI]));
      dbghv(("     ABIVersion: 0x%02x\n", eh->e_ident[EI_ABIVERSION]));
      dbghv(("           Type: 0x%04x\n", eh->e_type));
      dbghv(("        Machine: 0x%04x\n", eh->e_machine));
      dbghv(("        Version: 0x%08x\n", eh->e_version));
      dbghv(("     EntryPoint: 0x%016x\n", eh->e_entry));
      dbghv(("    ProgramHdrs: 0x%016x\n", eh->e_phoff));
      dbghv(("    SectionHdrs: 0x%016x\n", eh->e_shoff));
      dbghv(("          Flags: 0x%08x\n", eh->e_flags));
      dbghv(("     ElfHdrSize: 0x%04x\n", eh->e_ehsize));
      dbghv(("    ProgHdrSize: 0x%04x\n", eh->e_phentsize));
      dbghv(("     ProgHdrCnt: 0x%04x\n", eh->e_phnum));
      dbghv(("    SectHdrSize: 0x%04x\n", eh->e_shentsize));
      dbghv(("     SectHrdCnt: 0x%04x\n", eh->e_shnum));
      dbghv((" StringTblIndex: 0x%04x\n", eh->e_shstrndx));
   }
   return;
}
