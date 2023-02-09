//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                     Windows EXPORTS Symbol Harvester                     //
//                     --------------------------------                     //
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
extern int scnt;                            // Total number of sections




//
// FindExportDirectory()
// *********************
//
// rva_start, rva_end and delta are only set if return is non-NULL.
//
IMAGE_EXPORT_DIRECTORY *FindExportDirectory(IMAGE_NT_HEADERS *nth,
                                            DWORD *rva_start,
                                            DWORD *rva_end,
                                            int   *delta)
{
   IMAGE_EXPORT_DIRECTORY *ied = NULL;      // Image Export Directory
   IMAGE_DATA_DIRECTORY *ioh_DataDirectory;

   DWORD ied_rva_start;                     // Export Directory Starting RVA
   DWORD ied_rva_end;                       // Export Directory Ending RVA
   int   ied_delta;                         // Delta from start of container section
   int   ied_sec;                           // Section containing IED


   dbghv(("> FindExportDirectory: IMAGE_NT_HEADER=0x%p\n",nth));


   ioh_DataDirectory = GetIohDataDirectory(&(nth->OptionalHeader));
   ied_rva_start = ioh_DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
   ied_rva_end =   ied_rva_start + ioh_DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
   dbghv(("- FindExportDirectory: ied_rva_start=%p, ied_rva_end=%p\n",ied_rva_start, ied_rva_end));

   //
   // Find Export Directory.
   // It is usually within some section and we need to find the offset
   // within the containing section as well.
   //
   ied_sec = FindSectionForOffset((uint)ied_rva_start);
   if (ied_sec == 0) {
      dbghv(("< FindExportDirectory: Can't find IMAGE_EXPORT_DIRECTORY.\n"));
      return (NULL);
   }

   ied_delta = sec[ied_sec-1].offset - sec[ied_sec-1].file_offset;   // RVA - PtrToRawData
   dbghv(("- FindExportDirectory: ied at +0x%X into section %d\n",ied_delta,ied_sec));

   ied = (IMAGE_EXPORT_DIRECTORY *)PtrAdd(dh, (ied_rva_start - ied_delta));
   if (IsBadReadPtr((void *)ied, sizeof(IMAGE_EXPORT_DIRECTORY))) {
      errmsg(("*E* FindExportDirectory: IMAGE_EXPORT_DIRECTORY is not readable.\n"));
      return (NULL);
   }

   //
   // Done
   //
   if (rva_start) {
      *rva_start = ied_rva_start;           // Export Directory Starting RVA
   }
   if (rva_end) {
      *rva_end = ied_rva_end;               // Export Directory Ending RVA
   }
   if (delta) {
      *delta = ied_delta;                   // Delta from start of container section
   }

   dbghv(("< FindExportDirectory: ied=%p\n", ied));
   return (ied);
}


//
// HarvestExportSymbols()
// **********************
//
// Harvest Exported symbols from the Export Section (if one is present).
// As symbols are found a user callback function is called, once per
// symbol, to allow the user to manipulate/catalog the symbols.
//
// HarvestExportSymbols() returns when:
// - all symbols are enumerated, or
// - the callback function requests it to stop, or
// - an error is encountered
//
// Returns
//   0         No symbols harvested
//   non-zero  Number of symbols harvested
//
int HarvestExportSymbols(IMAGE_NT_HEADERS *nth, CONTEXT_REC *cr)
{
   IMAGE_EXPORT_DIRECTORY *ied = NULL;      // Image Export Directory
   DWORD ied_rva_start;                     // Export Directory Starting RVA
   DWORD ied_rva_end;                       // Export Directory Ending RVA
   int   ied_delta;                         // Delta from start of container section

   DWORD *functions;                        // Export Address Table
   DWORD *names;                            // Export Name Table
   SHORT *ordinals;                         // Export Ordinal Table

   char *symname;                           // Symbol name (mangled/decorated)
   char *symfwd;                            // Fowarded symbol name (if any)
   int symtype;                             // Symbol type (EXPORT or FORWARDER)
   int symsec;                              // Symbol section
   char *symcode = NULL;                    // Symbol code (or NULL if no code)
   DWORD symoffset;                         // Symbol offset (from image start)
   int symcnt = 0;                          // Total symbols kept

   int i;
   int rc;

   MOD_NODE *mn;



   dbghv(("> HarvestExportSymbols: cr=0x%p, IMAGE_NT_HEADER=0x%p\n", cr, nth));

   //
   // Find export directory
   //
   ied = FindExportDirectory(nth, &ied_rva_start, &ied_rva_end, &ied_delta);
   if (ied == NULL) {
      dbghv(("< HarvestExportSymbols: IMAGE_EXPORT_DIRECTORY not found or not readable. Nothing to do.\n"));
      return (0);
   }
   DumpExportDirectoryTable(ied);

   //
   // Make sure there are functions exported
   //
   if (ied->NumberOfFunctions == 0) {
      dbghv(("< HarvestExportSymbols: ied.NumberOfFunction == 0.  Nothing to do.\n"));
      return (0);
   }

   //
   // Determine location of:
   // - Export Address Table  (functions)
   // - Export Name Table     (names)
   // - Export Ordinal Table  (ordinals)
   //
   functions = (DWORD *)PtrAdd((ied->AddressOfFunctions - ied_delta), dh);
   names = (DWORD *)PtrAdd((ied->AddressOfNames - ied_delta), dh);
   ordinals = (SHORT *)PtrAdd((ied->AddressOfNameOrdinals - ied_delta), dh);

   dbghv((" addr_table: 0x%X, name_tbl: 0x%X, ord_tbl: 0x%X\n",
          functions, names, ordinals));

   dbghv((" ************************ START EXPORTED SYMBOLS *********************\n"));
   dbghv(("  Num    Value       Section    Type    Name\n"));
   dbghv((" -----  ----------   -------   ------   ----------\n"));

   __try {
      //
      // Harvest all available names (unless they don't have an associated RVA)
      // If the symbol RVA falls outside the Export section then it's an exported
      // symbol.  If the RVA falls within the Export section then it's a forwarder
      // symbol and the RVA is a pointer to the DLL.EntryPoint name of the
      // forwarded symbol.
      // We don't harvest forwarders because they have no backing code in this
      // image - the do in the target image.
      //
      for (i = 0; i < (int)ied->NumberOfNames; i++) {
         symoffset = functions[ordinals[i]];   // symbol RVA
         if (symoffset == 0)
            continue;                          // Skip over gaps in exported functions

         symname = (char *)PtrAdd((names[i] - ied_delta), dh);
         symsec = FindSectionForOffset((uint)symoffset);

         if (symoffset < ied_rva_start || symoffset > ied_rva_end) {
            // EXPORT.
            symtype = SYMBOL_TYPE_EXPORT;
            dbghv((" %5d  0x%08X     %2d      0x%04X   %s\n",
                   symcnt, symoffset, symsec, symtype, symname));
         }
         else {
            // FORWARDER.  We don't harvest these.
            symtype = SYMBOL_TYPE_FORWARDER;
            symfwd = (char *)PtrAdd((symoffset - ied_delta), dh);
            dbghv((" %5d  0x%08X     %2d      0x%04X   %s -> %s",
                   symcnt, 0, symsec, symtype, symname, symfwd));
         }

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
         if (symtype == SYMBOL_TYPE_EXPORT) {
            symcnt++;                          // Another symbol added successfully

            sec[symsec - 1].symcnt++;
            if (G(gather_code)) {
               symcode = GetCodePtrForSymbol(symoffset, symsec);
               DumpCode((uchar *)symcode, 8);
            }

            //
            // Demangle (Undecorate in MS speak) name
            //
            if (G(demangle_cpp_names))
               symname = DemangleName(symname);

            //
            // Send the symbol back to A2N
            //
            rc = SendBackSymbol(symname,            // Symbol name
                                (uint)symoffset,    // Symbol offset
                                0,                  // Symbol size
                                symtype,            // Symbol type (label, export, etc.)
                                symsec,             // Section to which the symbol belongs
                                symcode,            // Code associated with this symbol
                                (void *)cr);        // Context record
            if (rc) {
               dbghv(("- HarvestExportSymbols: SendBackSymbol returned %d. Stopping.\n",rc));
               goto TheEnd;                    // Destination wants us to stop harvesting
            }
         }
      }
   } // __try
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n**WARNING** ##### EXCEPTION 0x%X in HarvestExportSymbols #####\n",_exception_code());
      msg_log("********* %d Symbols gathered up to now will be kept!\n", symcnt);
      goto TheEnd;
   }

   //
   // Done enumerating symbols
   //
TheEnd:
   dbghv((" ************************* END EXPORTED SYMBOLS **********************\n"));

   if (symcnt > 0) {
      mn = (MOD_NODE *)cr->handle;
   }

   dbghv(("< HarvestExportSymbols: symcnt=%d\n", symcnt));
   return (symcnt);
}
