//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                       Windows COFF Symbol Harvester                      //
//                       -----------------------------                      //
//                                                                          //
//**************************************************************************//
//
#include "a2nhdr.h"


//
// Externs (from a2n.c and windowsyms.c)
//
extern gv_t a2n_gv;                         // Globals
extern IMAGE_DOS_HEADER *dh;                // Image Dos Header (aka. mapped address)
extern SECTION_INFO sec[MAX_SECTIONS];      // Executable section information
extern WORD RequiredMachine;




//
// HarvestCoffSymbols()
// ********************
//
// Harvest COFF symbols from the COFF symbol table (either in the image
// or in a DBG file).
// As symbols are found a user callback function is called, once per
// symbol, to allow the user to manipulate/catalog the symbols.
//
// HarvestCOFFSymbols() returns when:
// - all symbols are enumerated, or
// - the callback function requests it to stop, or
// - an error is encountered
//
// Returns
//   0         No symbols harvested
//   non-zero  Number of symbols harvested
//
int HarvestCoffSymbols(IMAGE_COFF_SYMBOLS_HEADER *ich, CONTEXT_REC *cr)
{
   IMAGE_SYMBOL *sym, *symend;              // COFF Symbol Table start and end
   IMAGE_AUX_SYMBOL *auxsym;                // COFF auxiliary symbol
   char *strtbl;                            // Symbol string table
   char ShortNameBuf[9];                    // Holds short symbol name (8 bytes max)
   char *symname;                           // Symbol name (mangled/decorated)
   uint symsize;                            // Symbol size (if known)
   int symsec;                              // Symbol section
   int executable_section;                  // Symbol is in an executable section
   char *symcode = NULL;                    // Symbol code (or NULL if no code)
   uint symoffset;                          // Symbol offset (from image start)

   int i = 0;
   int rc = 0;
   int symcnt = 0;                          // Total symbols kept

   MOD_NODE *mn;


   dbghv(("> HarvestCoffSymbols: cr=0x%p, COFF_SYMBOLS_HEADER=0x%p\n", cr, ich));
   DumpCoffSymbolsHeader(ich);

   //
   // Calculate address of COFF Symbol Table and String Table.
   // The COFF symbol table (an array of IMAGE_SYMBOL structures) is at
   // offset coff_header.LvaToFirstSymbol from the beginning of the
   // COFF Header.
   // The string table is immediately following the COFF symbol table.
   //
   __try {
      sym = (IMAGE_SYMBOL *)PtrAdd(ich, (uintptr)(ich->LvaToFirstSymbol));
      symend = (IMAGE_SYMBOL *)(sym + ich->NumberOfSymbols);  // Pointer arithmetic here !!!
      strtbl = (char *)symend;
      dbghv(("- HarvestCoffSymbols: COFFSymbolTable=0x%p  SymbolTableEnd=0x%p\n", sym, symend));

      dbghv((" ************************ START COFF SYMBOLS *************************\n"));
      dbghv(("  Num    Value       Section    Type    StgClass   #AuxSyms   Name\n"));
      dbghv((" -----  ----------   -------   ------   --------   --------   ----------\n"));

      while (sym < symend) {
         i++;

         //
         // Get the name ...
         // Do this first so we can dump it if debug mode is on. Don't really
         // need to do it unless we decide to keep the symbol.
         //
         if (sym->N.Name.Short != 0) {
            // name <= 8 bytes. Pick it up ...
            symname = ShortNameBuf;
            memcpy(symname, &(sym->N), sizeof(sym->N));
            symname[8] = 0;                    // In case name is exactly 8 bytes
         }
         else {
            // name > 8 bytes. N.Name.Long is offset into string table
            symname = (char *)PtrAdd(strtbl, sym->N.Name.Long);
         }

         //
         // Remove leading underscore/at/dot character added by the compiler
         //
#if defined(_X86)
         if (*symname == LEAD_CHAR_US || *symname == LEAD_CHAR_AT) symname++;
#else
         if (*symname == LEAD_CHAR_DOT) symname++;
#endif
         DumpCoffSymbol(i, symname, sym);

         //
         // Decide whether or not to keep this symbol.
         // Want:
         // - Functions in *ANY* section
         // - Non-functions only in executable sections
         //
         // sym->Type:
         //     Not a Function        0x0000
         //     If it has an Aux record and it's of class static and the
         //     name is a section then ignore it. Otherwise save it.
         //
         // sym->Type:
         //     Function              0x0020
         //     They *MAY* have an Aux Format X record describing the function
         //     If no Aux record it looks like it's an extern or linked in
         //
         // sym-StorageClass:
         //     IMAGE_SYM_CLASS_EXTERNAL            0x0002
         //         if (SectionNumber == IMAGE_SYM_UNDEFINED)
         //            Value is size
         //         else
         //            Value is offset w/in section
         //     IMAGE_SYM_CLASS_STATIC              0x0003
         //         if (Value == 0)
         //            symbol is section name
         //         else
         //            Value is offset w/in section
         //     IMAGE_SYM_CLASS_LABEL               0x0006
         //     IMAGE_SYM_CLASS_FAR_EXTERNAL        0x0044  (68)
         //     IMAGE_SYM_CLASS_FUNCTION            0x0065  (101)
         //     IMAGE_SYM_CLASS_FILE                0x0067  (103)
         //         Source file.  Followed by Aux record(s) naming the file
         //     IMAGE_SYM_CLASS_SECTION             0x0068  (104)
         //     IMAGE_SYM_CLASS_WEAK_EXTERNAL       0x0069  (105)
         //
         symsec = sym->SectionNumber;
         executable_section = SectionIsExecutable(symsec);

         // (Function) *OR* (Non-function with no auxiliary record and executable section)
         if ( (sym->Type == 0x0020) ||      // Function ...
              (sym->Type == 0x0000 && sym->NumberOfAuxSymbols == 0 && executable_section) ) {
            //
            // If there's an auxiliary symbol then it contains the
            // actual size of the function.  I've only seen this for
            // static functions.  Publics don't have the AUX symbol
            // and thus I have to calculate the size.  But symbols are
            // not sorted so I have to wait until I'm done here to do it.
            //
            if (sym->NumberOfAuxSymbols != 0) {
               auxsym = (IMAGE_AUX_SYMBOL *)sym + 1; // Pointer arithmetic here !!!
               symsize = auxsym->Sym.Misc.TotalSize; // Symbol size
            }
            else {
               symsize = 0;                    // Calculate later
            }

            symoffset = (uint)sym->Value;

            //
            // If gathering code then calculate the local address
            // for this symbol's code.
            // ***************
            // Since it is possible that not all code is in executable
            // sections (for example, java code in jitc.dll and jvm.dll)
            // assume that if this is a function it must be executable code.
            // Gather code if haven't done so already (done under the covers
            // by GetCodePtrForSymbol).
            // ***************
            //
            sec[symsec - 1].symcnt++;
            if (G(gather_code)) {
               symcode = GetCodePtrForSymbol(symoffset, symsec);
               DumpCode((uchar *)symcode, symsize);
            }

            //
            // Demangle (Undecorate in MS speak) name
            //
            if (G(demangle_cpp_names))
               symname = DemangleName(symname);

            //
            // Send the symbol back to A2N
            //
            rc = SendBackSymbol(symname,               // Symbol name
                                symoffset,             // Symbol offset
                                symsize,               // Symbol size
                                SYMBOL_TYPE_FUNCTION,  // Symbol type (label, export, etc.)
                                symsec,                // Section to which the symbol belongs
                                symcode,               // Code associated with this symbol
                                (void *)cr);           // Context record
            if (rc) {
               dbghv(("- HarvestCoffSymbols: SendBackSymbol returned %d. Stopping.\n",rc));
               goto TheEnd;                    // Destination wants us to stop harvesting
            }

            symcnt++;                          // Another symbol added successfully
         }

         //
         // On to next symbol, skipping Auxiliary symbols
         //
         sym += sym->NumberOfAuxSymbols + 1;   // Pointer arithmetic here !!!
      } // while
   } // __try
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n**WARNING** ##### EXCEPTION 0x%X in HarvestCoffSymbols #####\n",_exception_code());
      msg_log("********* %d Symbols gathered up to now will be kept!\n", symcnt);
      goto TheEnd;
   }

   //
   // Done enumerating symbols
   //
TheEnd:
   dbghv((" ************************* END COFF SYMBOLS **************************\n"));
   if (symcnt > 0) {
      mn = (MOD_NODE *)cr->handle;
      mn->flags |= A2N_FLAGS_COFF_SYMBOLS;
   }
   dbghv(("< HarvestCoffSymbols: symcnt=%d\n", symcnt));
   return (symcnt);
}
