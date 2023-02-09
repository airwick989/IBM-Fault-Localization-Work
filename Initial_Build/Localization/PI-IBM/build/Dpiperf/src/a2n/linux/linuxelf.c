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
//                        Linux ELF32 Symbol Harvester                      //
//                        ----------------------------                      //
//                                                                          //
//**************************************************************************//
//
#include "a2nhdr.h"


#define SEGMENT_TYPE_EXECUTE(flag)    ((flag) & PF_X)
#define SEGMENT_TYPE_WRITE(flag)      ((flag) & PF_W)
#define SEGMENT_TYPE_READ(flag)       ((flag) & PF_R)



//
// Externs (from a2n.c)
//
extern gv_t a2n_gv;                         // a2n.c


//
// Demangler interface (see cplus-dem.c)
//
char * cplus_demangle(const char * mangled, int options);

#define DMGL_PARAMS     (1 << 0)            // Include function args
#define DMGL_ANSI       (1 << 1)            // Include const, volatile, etc
#define DMGL_GNU        (1 << 9)            // gnu-style demangling





//
// GetSymbolsFromElfExecutable()
// *****************************
//
// Harvest symbols from non-stripped ELF32 executables.
// As symbols are found a user callback function is called, once per
// symbol, to allow the user to manipulate/catalog the symbols.
// GetSymbolsFromExecutable() returns when:
// - all symbols are enumerated, or
// - the callback function requests it to stop, or
// - an error is encountered
// For relocatable modules (ex. shared objects) the load address is not used -
// the symbols are in terms of an offset from the text section.
// For non-relocatable modules (ex. executables) the load address is used
// to calculate a symbol offset since the symbols are in terms of a virtual
// address.
//
//
int GetSymbolsFromElfExecutable(CONTEXT_REC * cr, char * modname, int module_type, bfd * abfd)
{
   Elf32_Ehdr * eh;                         // Elf Header
   Elf32_Phdr * ph_table, * ph;             // Program Header
   Elf32_Shdr * sh_table, * sh;             // Section Headers
   Elf32_Shdr * sym_sh = NULL;              // Symbol table section header
   Elf32_Shdr * dynsym_sh = NULL;           // Dynamic symbol table section header
   Elf32_Sym  * sym_table, * sym;           // Symbol Table/Section

   MOD_NODE * mn;

   int i, rc, symtype, symbind;
   int NumberOfSymbols = 0;                 // Number of symbols in executable
   int NumberOfSymbolsKept = 0;             // Number of symbols we actually kept
   int NumberOfDynamicSymbols = 0;          // Number of dynamic section symbols
   int it_is_the_kernel = 0;                // Just what it says

   char * shstrtab;                         // String table base for Sections
   char * strtab;                           // String section base for SYMTAB section
   char * symname, * demangled_name = NULL; // Symbol name
   uint symoffset;                          // Symbol offset
   unsigned char * code_ptr;                // Code associated with symbol

   char * secname;
   int have_debug_line = 0, have_debug_info = 0;

   uintptr base_addr = UINTPTR_MAX;

   SECTION_INFO e_sec[MAX_EXEC_SECTIONS];   // Executable section information
   int escnt = 0;                           // Number of executable sections
   int plt_index = -1;                      // Index to .plt section

   SECTION_INFO a_sec[MAX_ALLOC_SECTIONS];  // Non-executable section information
   int ascnt = 0;                           // Number of non-executable sections

   int symtab_section_cnt = 0;              // Number of SYMTAB sections


   dbghv(("> GetSymbolsFromElfExecutable: fn='%s', cr=%p, module_type=%d, abfd=%p\n", modname, cr, module_type, abfd));
   //
   // Set beginning of the Program Header table and Section Header table
   //
   mn = (MOD_NODE *)cr->handle;
   eh = (Elf32_Ehdr *)cr->mod_addr;                       // Elf header is beginning of file
   ph_table = (Elf32_Phdr *)PtrAdd(eh, eh->e_phoff);      // Program headers table
   sh_table = (Elf32_Shdr *)PtrAdd(eh, eh->e_shoff);      // Section headers table

   mn->li.abfd = abfd;
   mn->li.asym = NULL;
   mn->li.asymcnt = 0;
   mn->flags |= A2N_FLAGS_32BIT_MODULE;                   // ELF (32-bit) module

   if (mn->flags & A2N_FLAGS_KERNEL)
      it_is_the_kernel = 1;

   //
   // Enumerate the program headers.
   // Remember the lowest virtual address of all the PT_LOAD headers.
   // That will be used when calculating symbol offsets later on ...
   //
   dbghv(("\n  ****************  Program Headers [%d]  **************\n",eh->e_phnum));
   dbghv(("##    Type       Offset   VirtAddr   PhysAdd    FileSize   MemSize     Flags      Align\n"));
   dbghv(("-- ---------- ---------- ---------- ---------- ---------- ---------- ---------- ----------\n"));

   ph = ph_table;
   for (i = 0; i < eh->e_phnum; i++) {
      dbghv(("%02d 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
             i, ph->p_type, ph->p_offset, ph->p_vaddr, ph->p_paddr,
             ph->p_filesz, ph->p_memsz, ph->p_flags, ph->p_align));

      if (ph->p_type == PT_LOAD && ph->p_vaddr < base_addr) {
         base_addr = ph->p_vaddr;
      }
      ph++;
   }

   dbghv(("*** Lowest PT_LOAD virtual = %p\n", base_addr));
   if (base_addr == UINTPTR_MAX) {
      base_addr = (uintptr)cr->load_addr;
      dbghv(("*** Lowest PT_LOAD virtual changed to %p\n", cr->load_addr));
   }
   dbghv(("  *****************************************************\n\n"));

   //
   // Enumerate all section (headers) looking for executable sections
   // (sections that contain code) and for *THE* symbol table (the section
   // that contains debug symbols).
   //
   sh = sh_table + eh->e_shstrndx;                   // Section header for SHSTRTAB (pointer addition!)
   shstrtab = (char *)PtrAdd(eh, sh->sh_offset);     // Start of string table

   dbghv(("  ******************* Sections ********************\n"));
   dbghv(("      type      flags      link       info        addr     f_offset   m_offset     size     addralign   entsize\n"));
   for (i = 0, sh = sh_table;  i < eh->e_shnum;  i++, sh++) {
      secname = (char *)PtrAdd(shstrtab, sh->sh_name);
      //
      // Executable section?
      // If so remember it.  Only symbols in these sections will be harvested.
      // Check for SHF_EXECINSTR (executable machine instructions) in sh_flags.
      //
      if (sh->sh_flags & SHF_EXECINSTR) {
         if (escnt == MAX_EXEC_SECTIONS) {
            errmsg(("*E* GetSymbolsFromElfExecutable: ***** INTERNAL ERROR: more than %d executable sections!\n", MAX_EXEC_SECTIONS));
            return (0);                     // No symbols
         }
         e_sec[escnt].number = i;
         e_sec[escnt].name = secname;

         e_sec[escnt].offset = PtrToUint32(PtrSub(Ptr32ToPtr(sh->sh_addr), base_addr));

         if (module_type == MODULE_TYPE_ELF_EXEC) {
            // Executables are images and sections do not require fixups
            e_sec[escnt].start_addr = sh->sh_addr;
         }
         else { // MODULE_TYPE_ELF_REL
            // Relocatables may require fixups. Sections start relative to the load address.
            e_sec[escnt].start_addr = cr->load_addr + e_sec[escnt].offset;
         }
         e_sec[escnt].size = sh->sh_size;
         e_sec[escnt].end_addr    = e_sec[escnt].start_addr + sh->sh_size - 1;
         e_sec[escnt].file_offset = sh->sh_offset;
         e_sec[escnt].flags = sh->sh_flags;
         e_sec[escnt].type = sh->sh_type;

         e_sec[escnt].loc_addr = NULL;      // Until we decide if we need code

         dbghv(("%2x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x  *X* '%s'\n",
                i, sh->sh_type, sh->sh_flags, sh->sh_link, sh->sh_info,
                sh->sh_addr, sh->sh_offset, e_sec[escnt].offset,
                sh->sh_size, sh->sh_addralign, sh->sh_entsize, secname));

         //
         // Remember index to .plt section
         //
         if (strcmp(e_sec[escnt].name, ".plt") == 0) {
            plt_index = escnt;
            dbghv((".plt section is number %d, e_sec_index = %d\n", i, plt_index));
         }

         escnt++;
         continue;
      }

      //
      // Non-executable and allocated section?
      // If so remember it.  These are sections that contain no executable
      // code but are part of the loaded image (thus the allocated name).
      // Since they're not executable there should not be symbols for them
      // and, in fact, we won't gather symbols for them even if they're
      // present.  We only gather symbols for executable sections.
      //
      if (sh->sh_flags & SHF_ALLOC) {
         if (ascnt == MAX_ALLOC_SECTIONS) {
            errmsg(("*E* GetSymbolsFromElfExecutable: ***** INTERNAL ERROR: more than %d executable sections!\n", MAX_ALLOC_SECTIONS));
            return (0);                     // No symbols
         }
         a_sec[ascnt].number = i;
         a_sec[ascnt].name = secname;
         a_sec[ascnt].offset = PtrToUint32(PtrSub(Ptr32ToPtr(sh->sh_addr), base_addr));

         if (strcmp(secname, ".dynsym") == 0) {
            dynsym_sh = sh;
            NumberOfDynamicSymbols = sh->sh_size / sizeof(Elf32_Sym);
         }

         if (module_type == MODULE_TYPE_ELF_EXEC) {
            // Executables are images and sections do not require fixups
            a_sec[ascnt].start_addr = sh->sh_addr;
         }
         else { // MODULE_TYPE_ELF_REL
            // Relocatables may require fixups. Sections start relative to the load address.
            a_sec[ascnt].start_addr = cr->load_addr + a_sec[ascnt].offset;
         }
         a_sec[ascnt].size = sh->sh_size;
         a_sec[ascnt].end_addr    = a_sec[ascnt].start_addr + sh->sh_size - 1;
         a_sec[ascnt].file_offset = sh->sh_offset;
         a_sec[ascnt].flags = sh->sh_flags;
         a_sec[ascnt].type = sh->sh_type;

         a_sec[ascnt].loc_addr = NULL;      // Never code for these!

         dbghv(("%2x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x  *A* '%s'\n",
                i, sh->sh_type, sh->sh_flags, sh->sh_link, sh->sh_info,
                sh->sh_addr, sh->sh_offset, a_sec[ascnt].offset,
                sh->sh_size, sh->sh_addralign, sh->sh_entsize, secname));

         ascnt++;
         continue;
      }

      //
      // Symbol Table?
      // If we find it we have a non-stripped executable with symbols.
      // If we don't find it we have a stripped executable without symbols.
      // ***** Currently non-stripped executables have one and only one
      // ***** Symbol Table.
      //
      if (sh->sh_type == SHT_SYMTAB) {
         if (symtab_section_cnt > 0) {
            errmsg(("*E* GetSymbolsFromElfExecutable: ***** INTERNAL ERROR: more than 1 SYMTAB sections!\n"));
            return (0);                     // No symbols
         }
         NumberOfSymbols = sh->sh_size / sizeof(Elf32_Sym);
         sym_sh = sh;                       // Remember section header
         dbghv(("%2x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x  *S* '%s' symcnt=%d\n",
                i, sh->sh_type, sh->sh_flags, sh->sh_link, sh->sh_info,
                sh->sh_addr, sh->sh_offset, 0xffffffff, sh->sh_size,
                sh->sh_addralign, sh->sh_entsize, secname, NumberOfSymbols));
         continue;                          // Assumes there is only 1 symbol table in executable!
      }

      //
      // Some other section type
      //
      if (stricmp(secname, ".debug_line") == 0) {
         have_debug_line = 1;
      }
      if (stricmp(secname, ".debug_info") == 0) {
         have_debug_info = 1;
      }

      dbghv(("%2x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x      '%s'\n",
             i, sh->sh_type, sh->sh_flags, sh->sh_link, sh->sh_info,
             sh->sh_addr, sh->sh_offset, 0xffffffff, sh->sh_size,
             sh->sh_addralign, sh->sh_entsize, secname));
   }
   dbghv(("  *************************************************\n\n"));

   //
   // If we're being asked to keep the code around then copy the
   // executable sections so we can keep them around after we free the module.
   // If not, then just indicate we don't have any code.
   // As we do this send back each section.
   //
   dbghv(("  ************** Executable Sections **************\n"));
   dbghv(("      type       flags     s_offset   s_addr     e_addr     size       l_addr    name\n"));
   for (i = 0; i < escnt; i++) {
      if (G(gather_code) && e_sec[i].type != SHT_NOBITS) {
         e_sec[i].loc_addr = zmalloc(e_sec[i].size);
         if (e_sec[i].loc_addr == NULL) {
            errmsg(("*E* GetSymbolsFromElfExecutable: Unable to malloc(%d) for code for section %s\n", e_sec[i].size, e_sec[i].name));
         }
      }

      dbghv(("%2x 0x%08x 0x%08x 0x%08x 0x%"_LZ64x" 0x%"_LZ64x" 0x%08x %p  %s\n",
             e_sec[i].number, e_sec[i].type, e_sec[i].flags,
             e_sec[i].offset, e_sec[i].start_addr, e_sec[i].end_addr,
             e_sec[i].size, e_sec[i].loc_addr, e_sec[i].name));

      if (e_sec[i].loc_addr != NULL) {
         dbghv(("- GetSymbolsFromElfExecutable: Saving %d bytes of code for section %s\n", e_sec[i].size, e_sec[i].name));
         memcpy(e_sec[i].loc_addr, (void *)PtrAdd(eh, e_sec[i].file_offset), e_sec[i].size);
      }

      e_sec[i].sec = NULL;

      dbghv(("- GetSymbolsFromElfExecutable: asction for %s = %p\n", e_sec[i].name, e_sec[i].sec));

      //
      // Call the callback function with each executable section.
      // Don't care whether user accepts of not.
      //
      SendBackSection(e_sec[i].name,        // Section name
                      e_sec[i].number,      // Section number
                      e_sec[i].sec,         // BFD asection pointer
                      e_sec[i].start_addr,  // Section start address/offset
                      e_sec[i].offset,      // Section offset
                      e_sec[i].size,        // Section size
                      e_sec[i].flags,       // Section flags
                      e_sec[i].loc_addr,    // Where I have it loaded
                      1,                    // Executable section
                      cr);                  // Context record

      //
      // If there is a .plt section then create a symbol for it.
      // Don't care whether it works or not.
      //
      if (i == plt_index) {
         dbghv(("- GetSymbolsFromElfExecutable: Adding pseudo symbol for .plt\n"));
         mn->flags |= A2N_FLAGS_PLT_SYMBOLS_ONLY;  // Only symbol is PLT (so far)
         SendBackSymbol(A2N_SYMBOL_PLT_STR,   // Symbol name
                        e_sec[i].offset,      // Symbol offset
                        e_sec[i].size,        // Symbol size
                        SYMBOL_TYPE_ELF_PLT,  // Symbol type
                        e_sec[i].number,      // Section to which the symbol belongs
                        e_sec[i].loc_addr,    // Code associated with this symbol
                        cr);                  // Context record
      }
   }

   dbghv(("  ************ Non-executable Sections ************\n"));
   dbghv(("      type       flags     s_offset   s_addr     e_addr     size       l_addr    name\n"));
   for (i = 0; i < ascnt; i++) {
      dbghv(("%2x 0x%08x 0x%08x 0x%08x 0x%08x 0x%"_LZ64x" 0x%"_LZ64x" 0x%08x  %s\n",
             a_sec[i].number, a_sec[i].type, a_sec[i].flags,
             a_sec[i].offset, a_sec[i].start_addr, a_sec[i].end_addr,
             a_sec[i].size, a_sec[i].loc_addr, a_sec[i].name));


      a_sec[i].sec = NULL;

      dbghv(("- GetSymbolsFromElfExecutable: asction for %s = %p\n", a_sec[i].name, a_sec[i].sec));

      //
      // Call the callback function with each executable section.
      // Don't care whether user accepts of not.
      //
      SendBackSection(a_sec[i].name,        // Section name
                      a_sec[i].number,      // Section number
                      a_sec[i].sec,         // BFD asection pointer
                      a_sec[i].start_addr,  // Section start address/offset
                      a_sec[i].offset,      // Section offset
                      a_sec[i].size,        // Section size
                      a_sec[i].flags,       // Section flags
                      a_sec[i].loc_addr,    // Where I have it loaded
                      0,                    // Non-executable section
                      cr);                  // Context record
   }
   dbghv(("  *************************************************\n\n"));

   //
   // If vdso then there won't be any symbols. For this one we need to use
   // the symbols in the .dynsym section, if there is one
   //
   if ((mn->flags & A2N_FLAGS_VDSO) && dynsym_sh != NULL) {
      sym_table = (Elf32_Sym *)PtrAdd(eh, dynsym_sh->sh_offset);  // Beginning of symbol table
      sh = sh_table + dynsym_sh->sh_link;                         // DYNSYM string section (pointer addition!)
      strtab = (char *)PtrAdd(eh, sh->sh_offset);                 // Start of string section
      NumberOfSymbols = NumberOfDynamicSymbols;
      goto EnumerateSymbols;
   }

   //
   // If it wasn't [vdso] and there there are no symbols (whether because there
   // was no Symbol Table section or because there were 0 symbols) then we're done.
   //
   if (NumberOfSymbols == 0) {
      dbghv(("< GetSymbolsFromElfExecutable: file '%s' contains no symbols.\n", modname));
      return (0);                           // No symbols
   }

   //
   // Now enumerate the symbols ...
   // sym_sh is pointing to the section header of the SYMTAB section ...
   //
   sym_table = (Elf32_Sym *)PtrAdd(eh, sym_sh->sh_offset);  // Beginning of symbol table
   sh = sh_table + sym_sh->sh_link;                         // SYMTAB string section (pointer addition!)
   strtab = (char *)PtrAdd(eh, sh->sh_offset);              // Start of string section

EnumerateSymbols:

   dbghv(("*S*  st_name     st_value    st_size     st_info  st_other  st_shndx   name\n"));

   for (i = 0, sym = sym_table;  i < NumberOfSymbols;  i++, sym++) {

      // Invalid section index
      if (sym->st_shndx == 0) {
         DumpElfSymbol("NULL", "***Section index == 0***", sym);
         continue;
      }

      symname = (char *)PtrAdd(strtab, sym->st_name);      // Grab symbol name (NULL str if st_name == 0)
      symtype = ELF32_ST_TYPE(sym->st_info);    // Isolate symbol type
      symbind = ELF32_ST_BIND(sym->st_info);    // Isolate symbol binding

      //
      // Labels:    (STT_NOTYPE && (STB_LOCAL || STB_GLOBAL))
      // Functions: (STT_FUNC && (STB_LOCAL || STB_GLOBAL || STB_WEAK))
      // Objects;   (STT_OBJECT && (STB_LOCAL || STB_GLOBAL))
      //
      // NOTE:
      //  If it is the kernel then collect objects as well as functions and labels.
      //  Need to do that because objects appear in KSYSMs and if we don't collect
      //  them now then kernel symbol validation will fail.
      //  All these kernel special cases are here only to make validation work.
      //
      //    BIND        TYPE      sym->st_info   What it means
      //  ----------  ----------  ------------   -------------------------------------
      //  STB_LOCAL   STT_NOTYPE      0x00       Local (static) Label
      //  STB_LOCAL   STT_OBJECT      0x01       Local (static) Object
      //  STB_LOCAL   STT_FUNC        0x02       Local (static) Function
      //  STB_GLOBAL  STT_NOTYPE      0x10       Global Label
      //  STB_GLOBAL  STT_OBJECT      0x11       Global Object
      //  STB_GLOBAL  STT_FUNC        0x12       Global Function
      //  STB_WEAK    STT_NOTYPE      0x20       Weak Label (?? does it make sense ??)
      //  STB_WEAK    STT_FUNC        0x22       Weak Function
      //
      if (symbind != STB_LOCAL  &&  symbind != STB_GLOBAL  &&  symbind != STB_WEAK) {
         DumpElfSymbol(symname, "***Not function or label (BIND != LOCAL/GLOBAL/WEAK)***", sym);
         continue;                          // BIND attribute not what we want
      }

      if (it_is_the_kernel) {
         // Grab functions, labels and objects
         if (symtype != STT_FUNC  &&  symtype != STT_NOTYPE  &&  symtype != STT_OBJECT) {
            DumpElfSymbol(symname, "***Not function, label, or object (TYPE != FUNC/NOTYPE/OBJECT)***", sym);
            continue;                          // TYPE attribute not what we want
         }

         // Probably absolute symbol but value too small
         if (sym->st_value < base_addr) {
            DumpElfSymbol(symname, "***Kernel and absolute symbol < base_addr*****", sym);
            continue;
         }

         // Grab symbols from any section
      }
      else {
         // Grab functions and labels only
         if (symtype != STT_FUNC  &&  symtype != STT_NOTYPE) {
            DumpElfSymbol(symname, "***Not function or label (TYPE != FUNC/NOTYPE)***", sym);
            continue;                          // TYPE attribute not what we want
         }

         // Symbol in undefined/reserved section
         if (sym->st_shndx == SHN_UNDEF  ||  sym->st_shndx > eh->e_shnum) {
            DumpElfSymbol(symname, "***Undefined(0)/Reserved(>0xff00) Section***", sym);
            continue;                          // Undefined/Reserved section
         }

         // Throw away symbols for non-executable sections.
         // A section is executable if:
         //    ((sh->sh_type == SHT_PROGBITS) && (sh->sh_flags & SHF_EXECINSTR))
         sh = sh_table + sym->st_shndx;     // Section to which this symbol belongs (pointer addition!)
         if ((sh->sh_type != SHT_PROGBITS) || !(sh->sh_flags & SHF_EXECINSTR)) {
            DumpElfSymbol(symname, "**Non-executable section**", sym);
            continue;                          // Don't want this one
         }
      }

#if (defined(_PPC) && defined(_32BIT)) || defined(_S390)
      //
      // These platforms have some symbols that are repeated over and over.
      // In general they are contained in other symbols (functions) so I'm
      // not filtering them out for now.
      //
      // * Letext
      //   This is usually a label to some number of NOPs.  It appears to be
      //   used to force alignment (32-bit) for function entry points.
      // * .LT##_0 (where ## are numbers)
      //   These seem to be some sort of branch or offset table, usually located
      //   immediately following the function prologue code.  Prologue code branches
      //   around it and code later on references it by offset.
      // * .L# (where # is a number)
      // * L## (where # is a number and thing between them is a  black smily face)
      //   These contain initialized data at some offset from the label.  I don't
      //   know what they are for.
#endif

      //
      // It's a keeper!
      //
      // Basically what we have left are either Labels or Functions which are
      // either Local, Global or Weak, and are not generated by gcc.
      // For the kernel they can be in any section (ie. executable or not).
      // For all other modules they can only be in executable sections.
      //
      // The symbol address we return is really an offset.
      // For executable (ie. non-relocatable) modules, st_value is given in
      // terms of a virtual address so we need to convert that to an offset.
      // For relocatable modules, st_value is given as an offset so we
      // just use it as-is.
      //
      NumberOfSymbolsKept++;                // Keping another one
      DumpElfSymbol(symname, NULL, sym);

      symoffset = PtrToUint32(PtrSub(sym->st_value, base_addr));

      if (sym->st_size == 0)
         symtype = SYMBOL_TYPE_LABEL;
      else {
         if (symbind == STB_WEAK)
            symtype = SYMBOL_TYPE_FUNCTION_WEAK;
         else if (symtype == STT_OBJECT)
            symtype = SYMBOL_TYPE_OBJECT;
         else
            symtype = SYMBOL_TYPE_FUNCTION;
      }

      //
      // Calculate where the code for this symbol starts (if needed)
      //
      code_ptr = GetCodePtrForSymbol((char *)Uint32ToPtr(symoffset), &e_sec[0], escnt);
      PrintBytes(code_ptr, (int)sym->st_size);   // Only if debugging harvester

      //
      // Demangle symbol name if asked to do so.
      // ***** Doesn't seem to work on IA64 so don't do it there *****
      //
#if !defined(_IA64)
      if (G(demangle_cpp_names)) {
         demangled_name = cplus_demangle(symname, (DMGL_PARAMS | DMGL_ANSI));
         dbghv(("DEMANGLE: %s -> %s\n", symname, demangled_name));
         if (demangled_name)
            symname = demangled_name;
      }
#endif

      //
      // Call the callback function with each symbol we've decided to keep.
      // Callback function returns 0 if it wants us to continue, non-zero
      // if it wants us to stop.
      //
      rc = SendBackSymbol(symname,          // Symbol name
                          symoffset,        // Symbol offset
                          sym->st_size,     // Symbol size
                          symtype,          // Symbol type
                          sym->st_shndx,    // Section to which the symbol belongs
                          code_ptr,         // Code associated with this symbol
                          cr);              // Context record

      if (demangled_name)
         free(demangled_name);

      if (rc) {
         dbghv(("< GetSymbolsFromElfExecutable: SendBackSymbol returned %d. Stopping\n", rc));
         return (NumberOfSymbolsKept);      // Error. Stop enumerating and return
                                            // number of symbols found so far.
      }
   }

   //
   // Done enumerating symbols
   //
   dbghv(("< GetSymbolsFromElfExecutable: Done enumerating. Found %d symbols.\n", NumberOfSymbolsKept));

   if (NumberOfSymbolsKept != 0)
      mn->flags &= ~A2N_FLAGS_PLT_SYMBOLS_ONLY;  // There are symbols besides the PLT

   return (NumberOfSymbolsKept);
}


//
// DumpElfSymbol()
// ***************
//
void DumpElfSymbol(char * name, char * comment, Elf32_Sym * sym)
{
   if (!G(debug))
      return;

   if (comment == NULL) {                   // Good symbols
       if (sym->st_size == 0) {
           dbghv(("Lbl  "));
       }
       else {
           dbghv(("Fnc  "));
       }

       dbghv(("0x%08x  0x%08x  0x%08x  0x%02x     0x%02x      0x%04x     %s\n",
              sym->st_name, sym->st_value, sym->st_size, sym->st_info, sym->st_other, sym->st_shndx, name));
   }
   else {                                   // Bad symbols
       dbghv(("     0x%08x  0x%08x  0x%08x  0x%02x     0x%02x      0x%04x     %s   %s\n",
              sym->st_name, sym->st_value, sym->st_size, sym->st_info, sym->st_other, sym->st_shndx, name, comment));
   }
   return;
}
