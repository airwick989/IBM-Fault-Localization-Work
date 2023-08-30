//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                       Windows DBG Symbol Harvester                       //
//                       ----------------------------                       //
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
extern ULONG img_misc_debug_dir_ts;         // TimeDataStamp for MISC debug directory entry





//
// GetSymbolsFromDbgFile()
// ***********************
//
// Harvest COFF/CV symbols from DBG file.
// As symbols are found a user callback function is called, once per
// symbol, to allow the user to manipulate/catalog the symbols.
// GetSymbolsFromDbgFile() returns when:
// - all symbols are enumerated, or
// - the callback function requests it to stop, or
// - an error is encountered
//
int GetSymbolsFromDbgFile(char *dbgfn, uint ts, CONTEXT_REC *cr)
{
   IMAGE_SEPARATE_DEBUG_HEADER *sdh = NULL; // DBG file header
   IMAGE_DEBUG_DIRECTORY *idd = NULL;       // Image debug directory
   DWORD debug_dir_rva;                     // Debug directory RVA
   int debug_dir_cnt;                       // Number of debug directory entries

   IMAGE_COFF_SYMBOLS_HEADER *ich = NULL;   // File address of COFF Symbols header
   size_t filesize;

   IMAGE_DEBUG_MISC *idm = NULL;            // MISC debug section
   IMAGE_CV_HEADER *cvh = NULL;             // CODEVIEW debug section
   void *idots = NULL;                      // OMAP_TO_SRC debug information
   void *idofs = NULL;                      // OMAP_FROM_SRC debug information

   ULONG coff_debug_dir_ts = 0;             // TimeDataStamp for COFF debug dir entry
   ULONG cv_debug_dir_ts = 0;               // TimeDataStamp for CODEVIEW debug dir entry


   int i;
   int symcnt = 0;                          // Number of symbols found
   int sym_file_mismatch = 0;               // 1 = symbol file doesn't match image

   MOD_NODE *mn;


   dbghv(("> GetSymbolsFromDbgFile: dbgfn=%s, ts=0x%08x, cr=0x%p\n",dbgfn,ts,cr));

   mn = (MOD_NODE *)cr->handle;

   //
   // Make sure DBG file exists and we can read it
   //
   if (!FileIsReadable(dbgfn)) {
      errmsg(("*E* GetSymbolsFromDbgFile: unable to access '%s'\n",dbgfn));
      goto TheEnd;
   }

   //
   // Map file so we can look at the header
   //
   sdh = (IMAGE_SEPARATE_DEBUG_HEADER *)MapFile(dbgfn, &filesize);
   if (sdh == NULL) {
      errmsg(("*E* GetSymbolsFromDbgFile: unable to map file '%s'.\n",dbgfn));
      goto TheEnd;
   }
   if (IsBadReadPtr((void *)sdh, sizeof(IMAGE_SEPARATE_DEBUG_HEADER))) {
      errmsg(("*E* GetSymbolsFromDbgFile: IMAGE_SEPARATE_DEBUG_HEADER is not readable.\n"));
      goto TheEnd;
   }
   DumpDbgFileHeader(sdh);

   //
   // Make sure this is a valid DBG file ...
   // - must have correct signature
   // - timestamp must match the image timestamp (if we have it)
   //   or the timestamp of the MISC debug directory entry.
   // - target machine must match
   // - must have the debug directory
   //
   if (sdh->Signature != IMAGE_SEPARATE_DEBUG_SIGNATURE) {
      errmsg(("*E* GetSymbolsFromDbgFile: Invalid SEPARATE_DEBUG_HEADER signature: 0x%04X\n",sdh->Signature));
      goto TheEnd;
   }

   if (ts == 0 && img_misc_debug_dir_ts == 0) {
      warnmsg(("*W* GetSymbolsFromDbgFile: '%s' TS and MISC_DEBUG_DIR_ENTRY TS are 0. Unable to check if symbols match the image.\n",cr->imgname));
      sym_file_mismatch = 1;                // symbol file doesn't match image
   }
   else {
      if (ts != sdh->TimeDateStamp  &&  img_misc_debug_dir_ts != sdh->TimeDateStamp)  {
         warnmsg(("*E* GetSymbolsFromDbgFile: '%s'/'%s' timestamp mismatch: dbg=%08x/img=%08x/img_misc_dbg_dir=%08x\n",
                 dbgfn, cr->imgname, sdh->TimeDateStamp, ts, img_misc_debug_dir_ts));
         if (G(validate_symbols)) {
            goto TheEnd;
         }
         else {
            warnmsg(("    - Symbols validation failed. ***** SYMBOLS MAY NOT BE VALID *****\n"));
            sym_file_mismatch = 1;          // symbol file doesn't match image
         }
      }
   }

#if defined(_32BIT)
   if (sdh->Machine != IMAGE_FILE_MACHINE_I386) {
      errmsg(("*E* GetSymbolsFromDbgFile: sdh->Machine incorrect. Expected 0x%04x Got 0x%04x\n",
               IMAGE_FILE_MACHINE_I386, sdh->Machine));
      goto TheEnd;
   }
#else
   if (sdh->Machine != IMAGE_FILE_MACHINE_IA64 &&
       sdh->Machine != IMAGE_FILE_MACHINE_I386) {
      errmsg(("*E* GetSymbolsFromDbgFile: sdh->Machine incorrect. Expected 0x%04x or 0x%04x, Got 0x%04x\n",
               IMAGE_FILE_MACHINE_IA64, IMAGE_FILE_MACHINE_I386, sdh->Machine));
      goto TheEnd;
   }
#endif



   if (sdh->DebugDirectorySize == 0) {
      errmsg(("*E* GetSymbolsFromDbgFile: DBG file does not have debug directories\n"));
      goto TheEnd;
   }

   //
   // Calculate location and number of debug directories.
   // Contrary to what Microsoft says in the description of
   // IMAGE_SEPARATE_DEBUG_HEADER (in winnt.h) the layout of the DBG
   // file is:
   // - IMAGE_SEPARATE_DEBUG_HEADER
   // - Followed by sdh->NumberOfSections IMAGE_SECTION_HEADERs
   // - Followed by sdh->ExportedNamesSize bytes of exported names strings
   // - Followed by the IMAGE_DEBUG_DIRECTORYs
   // - Followed by the sections pointed to by the DEBUG_DIRECTORYs
   //
   debug_dir_cnt = sdh->DebugDirectorySize / sizeof(IMAGE_DEBUG_DIRECTORY);
   debug_dir_rva = sizeof(IMAGE_SEPARATE_DEBUG_HEADER) +
                   (sdh->NumberOfSections * sizeof(IMAGE_SECTION_HEADER)) +
                   sdh->ExportedNamesSize;
   idd = (IMAGE_DEBUG_DIRECTORY *)PtrAdd(sdh, debug_dir_rva);
   if (IsBadReadPtr((void *)idd, (sizeof(IMAGE_DEBUG_DIRECTORY) * debug_dir_cnt))) {
      errmsg(("*E* GetSymbolsFromDbgFile: IMAGE_DEBUG_DIRECTORYs for %d entries are not readable.\n",debug_dir_cnt));
      goto TheEnd;
   }
   dbghv(("- GetSymbolsFromDbgFile: dbgdir_rva: 0x%08X dbgdir_cnt: %d idd: 0x%p\n",debug_dir_rva, debug_dir_cnt, idd));
   DumpDebugDirectories(idd, debug_dir_cnt);

   //
   // Check all debug directories looking for ones we care about
   //
   for (i = 0; i < debug_dir_cnt; i++) {
      if (idd[i].Type == IMAGE_DEBUG_TYPE_COFF) {
         // Have COFF debug symbols. Remember it.
         ich = (IMAGE_COFF_SYMBOLS_HEADER *)PtrAdd(sdh, idd[i].PointerToRawData);
         coff_debug_dir_ts = idd[i].TimeDateStamp;
      }
      if (idd[i].Type == IMAGE_DEBUG_TYPE_OMAP_TO_SRC) {
         // Have OMAP_TO_SRC information. Remember it.
         idots = (void *)PtrAdd(sdh, idd[i].PointerToRawData);
         dbghv(("- GetSymbolsFromDbgFile: OMAP_TO_SRC debug section: %p\n",idots));
      }
      if (idd[i].Type == IMAGE_DEBUG_TYPE_OMAP_FROM_SRC) {
         // Have OMAP_FROM_SRC information. Remember it.
         idofs = (void *)PtrAdd(sdh, idd[i].PointerToRawData);
         dbghv(("- GetSymbolsFromDbgFile: OMAP_FROM_SRC debug section: %p\n",idofs));
      }
      if (idd[i].Type == IMAGE_DEBUG_TYPE_MISC) {
         // Have MISC debug directory.  Remember it.
         idm = (IMAGE_DEBUG_MISC *)PtrAdd(sdh, idd[i].PointerToRawData);
         DumpMiscDebugSection(idm);
      }
      if (idd[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
         // Have CODEVIEW debug directory.  Remember it.
         cvh = (IMAGE_CV_HEADER *)PtrAdd(sdh, idd[i].PointerToRawData);
         cv_debug_dir_ts = idd[i].TimeDateStamp;
      }
   }

   //
   // Have MISC debug directory.
   //
   if (idm != NULL) {
      if (idm->DataType == IMAGE_DEBUG_MISC_EXENAME) {
         //
         // Make sure name listed in the MISC debug section is the same
         // as the DBG filename. If we have the image name then check
         // against that too.
         // idm->Data and is a NULL terminated string which may be an
         // entire pathname. It should be the DBG file name.
         //
         dbghv(("- GetSymbolsFromDbgFile: misc->Data: %s\n",idm->Data));
      }
   }

   //
   // If the DBG file contains COFF debug symbols then go read them ...
   //
   if (ich != NULL) {
      cr->mod_addr = (void *)cr->load_addr;
      cr->symtype = (int)SYMBOL_TYPE_FUNCTION;

      dbghv(("- GetSymbolsFromDbgFile: attempting to get COFF symbols from %s\n",dbgfn));
      symcnt = HarvestCoffSymbols(ich, cr);
      if (symcnt == 0) {
         dbghv(("- GetSymbolsFromDbgFile: no COFF symbols or error.\n"));
      }
      else {
         mn->type = MODULE_TYPE_DBG;        // Set new module type
      }
      goto TheEnd;
   }

   //
   // If the DBG file contains CodeView debug symbols then go read them ...
   //
   if (cvh != NULL) {
      cr->mod_addr = (void *)cr->load_addr;
      cr->symtype = (int)SYMBOL_TYPE_FUNCTION;

      dbghv(("- GetSymbolsFromDbgFile: attempting to get CV symbols from %s\n",dbgfn));
      symcnt = HarvestCodeViewSymbols(cvh, cr);
      if (symcnt <= 0) {
         dbghv(("- GetSymbolsFromDbgFile: no CV symbols or error.\n"));
      }
      else {
         mn->type = MODULE_TYPE_DBG;        // Set new module type
      }
      goto TheEnd;
   }

   //
   // Done
   //
TheEnd:
   if (sdh != NULL)
      UnmapFile(sdh);

   if (symcnt > 0 && sym_file_mismatch)
      mn->flags |= A2N_FLAGS_VALIDATION_FAILED;  // Symbols may not match image

   dbghv(("< GetSymbolsFromDbgFile: symcnt=%d\n", symcnt));
   return (symcnt);
}
