//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                      Windows Defaul Symbol Harvester                     //
//                      -------------------------------                     //
//                                                                          //
//**************************************************************************//
//
#include "a2nhdr.h"


//
// Externs (from a2n.c)
//
extern gv_t a2n_gv;                         // Globals
extern IMAGE_DOS_HEADER *dh;                // Image Dos Header (aka. mapped address)
extern SECTION_INFO sec[MAX_SECTIONS];      // Executable section information
extern int scnt;                            // Total number of sections
extern ULONG img_misc_debug_dir_ts;         // TimeDataStamp for MISC debug directory entry
extern SYMENUMLINES_FUNC  pSymEnumLines;    // SymEnumLines entry point
extern SYMENUMSYMBOLS_FUNC pSymEnumSymbols; // SymEnumSymbols entry point


//
// Globals
//
int ms_syms = 0;                            // #of symbols found by MS engine
int ms_syms_kept = 0;                       // The ones we actually kept
int ms_symtype = 0;                         // Symbol types returned by MS engine


SRCFILE_NODE *start_sfn = NULL, *end_sfn = NULL;
SRCLINE_NODE *start_sln = NULL, *end_sln = NULL;
int          strbytes = 0, l_cnt = 0, s_cnt = 0;


//
// AddLineNumberNode()
// *******************
//
BOOL CALLBACK AddLineNumberNode(PSRCCODEINFO LineInfo, PVOID nod_used)
{
   SRCLINE_NODE *sln;
   SRCFILE_NODE *sfn;
   uint  offset;


   //
   // Toss lines with 0 offset
   //
   offset = (uint)(LineInfo->Address - LineInfo->ModBase);
   if (offset == 0) {
      dbghv((" ***** %08X %6d %s\n",offset,LineInfo->LineNumber,LineInfo->FileName));
      return (TRUE);
   }

   //
   // Table up the source filename information
   //
   if ((end_sfn == NULL) ||
       (strcmp(end_sfn->fn, LineInfo->FileName) != 0)) {
      // First filename we see OR different filename from last line returned
      sfn = AllocSourceFileNode();
      if (sfn == NULL) {
         errmsg(("*E* AddLineNumberNode: unable to allocate SRCFILE_NODE.\n"));
         return (FALSE);                       // Stop enumerating
      }
      sfn->fn = strdup(LineInfo->FileName);
      if (sfn->fn == NULL) {
         FreeSourceFileNode(sfn);
         errmsg(("*E* AddLineNumberNode: unable to allocate memory for filename. fn=%s.\n", LineInfo->FileName));
         return (FALSE);
      }

      sfn->next = NULL;
      strbytes += (int)strlen(sfn->fn) + 1;
      s_cnt++;

      if (end_sfn == NULL) {
         // First file on the list
         start_sfn = end_sfn = sfn;
      }
      else {
         // Not first file. Chain it in
         end_sfn->next = sfn;
         end_sfn = sfn;
      }
   }

   //
   // Table up the source line number information
   //
   sln = AllocSourceLineNode();
   if (sln == NULL) {
      errmsg(("*E* AddLineNumberNode: unable to allocate SRCLINE_NODE.\n"));
      return (FALSE);                       // Stop enumerating
   }

   sln->sfn = end_sfn;
   sln->lineno = LineInfo->LineNumber;
   sln->offset_start = (uint)(LineInfo->Address - LineInfo->ModBase);

   sln->next = NULL;
   if (end_sln == NULL)
      start_sln = end_sln = sln;
   else {
      end_sln->next = sln;
      end_sln = sln;
   }

   l_cnt++;

   // Done
   dbghv((" %5d %08X %6d %s\n",l_cnt,sln->offset_start,sln->lineno,sln->sfn->fn));
   return(TRUE);                            // Keep going
}


//
// line_compare()
// **************
//
// Comparator function for qsort.
// Inputs are pointers to pointers to SRCLINE_NODEs.
//
static int line_compare(const void *left, const void *right)
{
   SRCLINE_NODE *l = *(SRCLINE_NODE **)left;
   SRCLINE_NODE *r = *(SRCLINE_NODE **)right;

   if (l->offset_start > r->offset_start)
      return (1);     // left > right
   else if (l->offset_start < r->offset_start)
      return (-1);    // left < right
   else
      return (0);     // left == right
}


//
// CreateSrcLineNumberArray()
// **************************
//
static void CreateSrcLineNumberArray(MOD_NODE *mn)
{
   SRCLINE_NODE *sln;
   int i;

   dbghv(("> CreateSrcLineNumberArray: mn=%p\n",mn));

   if (mn->li.linecnt == 0) {               // If there are no lines there's nothing to do
      dbghv(("< CreateSrcLineNumberArray: (module '%s' contains no lines)\n",mn->name));
      return;
   }

   // Allocate srcline pointer array
   dbghv(("- CreateSrcLineNumberArray: allocating line pointer array (%d elements, %d bytes)\n",
           mn->li.linecnt,(mn->li.linecnt * sizeof(SRCLINE_NODE *))));

   mn->li.srclines = (SRCLINE_NODE **)malloc(mn->li.linecnt * sizeof(SRCLINE_NODE *));
   if (mn->li.srclines == NULL) {
      errmsg(("*E* CreateSrcLineNumberArray: unable to malloc srclines array for '%s'. Discarding line information.\n",mn->name));
      return;
   }

   // Populate srcline pointer array
   dbghv(("- CreateSrcLineNumberArray: populating srcline pointer array. srclines=%p srclines_list=%p\n",mn->li.srclines,mn->li.srclines_list));
   i = 0;
   sln = mn->li.srclines_list;
   while (sln) {
//    dbgmsg(("----> %5d %08X %6d %s\n",i,sln->offset_start,sln->lineno,sln->sfn->fn));
      mn->li.srclines[i] = sln;
      sln = sln->next;
      i++;
   }

   // Sort the lines in ascending offset order.
   dbghv(("- CreateSrcLineNumberArray: sorting srcline pointer array. srclines=%p linecnt=%d\n",
          mn->li.srclines, mn->li.linecnt));
   qsort((void *)(mn->li.srclines), mn->li.linecnt, sizeof(SRCLINE_NODE *), line_compare);

   // Done
   dbghv(("< CreateSrcLineNumberArray: done\n"));
   return;
}


//
// CalculateLineOffsetEnd()
// ************************
//
// mn: module node
// ln: symbol node for label whose length we'd like to calculate
// nn: symbol node following ln
//
static void CalculateLineOffsetEnd(MOD_NODE *mn)
{
   SRCLINE_NODE **lines;
   SRCLINE_NODE  *csl, *nsl;
   int c, n;                                // Current and Next SNP indices
   int last_line;


   lines = mn->li.srclines;

   // Run thru all the lines and fix up as needed
   last_line = mn->li.linecnt - 1;          // SNP index of last line
   n = 1;
   for (c = 0; c < mn->li.linecnt; c++,n++) {
      csl = lines[c];
      if (c == 0)
         csl->section = GetSectionNodeByOffset(mn, csl->offset_start);

      if (c == last_line) {
         // Calculate the length to the end of the section.
         nsl = NULL;
         dbghv(("- CalculateLineOffsetEnd: last line\n"));
         if (csl->section == NULL) {
            csl->offset_end = csl->offset_start;
            warnmsg(("*W* CalculateLineOffsetEnd: line %d in %s not in any section.\n",csl->lineno,csl->sfn->fn));
         }
         else {
            csl->offset_end = csl->offset_start + (csl->section->offset_end - csl->offset_start - 1);
         }
      }
      else {
         nsl = lines[n];
         nsl->section = GetSectionNodeByOffset(mn, nsl->offset_start);
         if (csl->section != nsl->section) {        // Section changes
            // Calculate the length to the end of the section.
            dbghv(("- CalculateLineOffsetEnd: last line in section %d\n",csl->section->number));
            if (csl->section == NULL) {
               csl->offset_end = csl->offset_start;
               warnmsg(("*W* CalculateLineOffsetEnd: line %d in %s not in any section.\n",csl->lineno,csl->sfn->fn));
            }
            else {
               csl->offset_end = csl->offset_start + (csl->section->offset_end - csl->offset_start - 1);
            }
         }
         else {
            // Not last line AND in same section as the next one.
            // Calculate length to the next line.
            //
            if (nsl->offset_start == csl->offset_start)
               csl->offset_end = csl->offset_start;
            else
               csl->offset_end = csl->offset_start + (nsl->offset_start - csl->offset_start - 1);
         }
      }

      dbghv(("* CalculateLineOffsetEnd: line %d in %s goes from %x to %x\n",
             csl->lineno,csl->sfn->fn,csl->offset_start,csl->offset_end));
   }

   return;
}


//
// FixupLines()
// ************
//
// After lines are fixed up, put them into an array so we can do
// binary search when looking them up later.
//
void FixupLines(MOD_NODE *mn)
{
   SYM_NODE **syms;
   SRCLINE_NODE **lines;
   int s, l;


   dbghv(("> FixupLines: mn=%p\n",mn));

   if (mn->li.linecnt == 0) {               // If there are no lines there's nothing to do
      dbghv(("< FixupLines: (module '%s' contains no lines)\n",mn->name));
      return;
   }

   // Calculate end offsets for all line nodes
   CalculateLineOffsetEnd(mn);

   // Assign the correct set of lines to each symbol
   syms = mn->symroot;
   lines = mn->li.srclines;
   l = 0;
   for (s = 0; s < mn->base_symcnt; s++) {
      for ( ; l < mn->li.linecnt; l++) {
         if (lines[l]->offset_start >= syms[s]->offset_start &&
             lines[l]->offset_start <= syms[s]->offset_end) {
            // The starting offset for this line is within the current
            // symbol. Check if it's the first one or a subsequent one
            // and remember the indices into the line node pointer array.
            if (syms[s]->line_cnt == 0) {
               // First line for this symbol
               syms[s]->line_start = l;
               dbghv(("-*-*> FixupLines: (1) adding line %d (%x) in %s to %s (%X - %X)\n",
                      lines[l]->lineno, lines[l]->offset_start, lines[l]->sfn->fn, syms[s]->name,syms[s]->offset_start,syms[s]->offset_end));
            }
            else {
               // Not the first line for this symbol
               syms[s]->line_end = l;
               dbghv(("-*-*> FixupLines: (a) adding line %d (%x) in %s to %s (%X - %X)\n",
                      lines[l]->lineno, lines[l]->offset_start, lines[l]->sfn->fn, syms[s]->name,syms[s]->offset_start,syms[s]->offset_end));
            }

            if (lines[l]->offset_end > syms[s]->offset_end) {
               // Don't have a line ending offset past the end of the symbol
               dbghv(("-*-*> FixupLines: (f) adjusting line offset end from %X to %X\n",
                      lines[l]->offset_end, syms[s]->offset_end));
               lines[l]->offset_end = syms[s]->offset_end;
            }

            syms[s]->line_cnt++;
            continue;
         }

         if (lines[l]->offset_start < syms[s]->offset_start) {
            // Ignore line if offset less than current symbol. Just keep
            // going thru lines until we hit one that falls within the
            // current symbol.
            dbghv(("-*-*> FixupLines: (s) ignoring line %d (%x) in %s. Less than %s start offset (%X - %X)\n",
                   lines[l]->lineno, lines[l]->offset_start, lines[l]->sfn->fn, syms[s]->name, syms[s]->offset_start, syms[s]->offset_end));
            continue;
         }

         // Done with lines for this symbol
         break;
      }
   }

   // Done
   dbghv(("< FixupLines: done\n"));
   return;
}


//
// EnumSymbolsCallback()
// *********************
//
// Callback function invoked by the MS symbol harvester for each symbol.
// Returns: TRUE to allow harvester to continue
//          FALSE to force harvester to stop
//
BOOL CALLBACK EnumSymbolsCallback(PSYMBOL_INFO syminfo, ULONG size, PVOID cr)
{
   int rc = 0;
   int symsec;
   uint symoffset;
   char *symcode = NULL;
   uintptr uaddr;
   char *name;


   name = syminfo->Name;
   uaddr = (uintptr)syminfo->Address;
   ms_syms++;
   dbghv((" %4d  %"_PZP"   %6d   %s\n", ms_syms, uaddr, size, name));

   //
   // Find section to which this symbol belongs. If we need to gather
   // code for it then gather it now.
   // *****?????*****
   // Since we don't know what type of symbol MS is sending us
   // (ie. functions or some other type) assume we need code and
   // gather it if we haven't done so.  This would only apply to
   // symbols in non-executable sections because we always gather
   // code for executable sections.
   // *****?????*****
   //
   symoffset = (uint)(uaddr - (uintptr)(((CONTEXT_REC *)cr)->mod_addr));
   symsec = FindSectionForOffset(symoffset);
   if (symsec == 0) {
      dbghv((" mod_addr=%p  symoffset=%X  symsec=%d  *** INVALID SECTION - DISCARDED ***\n",((CONTEXT_REC *)cr)->mod_addr,symoffset,symsec));
      return (TRUE);                           // Continue harvesting
   }

   sec[symsec - 1].symcnt++;
   dbghv((" mod_addr=%p  symoffset=%X  symsec=%d\n",((CONTEXT_REC *)cr)->mod_addr,symoffset,symsec));

   if (G(gather_code)) {
#if defined(_32BIT)
      // Gather code for all sections
      symcode = GetCodePtrForSymbol(symoffset, symsec);
      DumpCode((uchar *)symcode, size);
#else
      // Only gather code for executable sections
      if (sec[symsec-1].flags & IMAGE_SCN_MEM_EXECUTE) {
         symcode = GetCodePtrForSymbol(symoffset, symsec);
         DumpCode((uchar *)symcode, size);
      }
#endif
   }

   if (G(demangle_cpp_names))
      name = DemangleName(name);

   rc = SendBackSymbol(name,                // Symbol name
                       symoffset,           // Symbol offset
                       size,                // Symbol size
                       ms_symtype,          // Symbol type (who knows what it is?)
                       symsec,              // Section to which the symbol belongs
                       symcode,             // Code associated with this symbol
                       cr);                 // Context record
   if (rc) {
      dbghv(("* EnumSymbolsCallback: SendBackSymbol returned %d. Stopping.\n",rc));
      return (FALSE);                       // Stop harvesting
   }

   ms_syms_kept++;                          // Another symbol added successfully
   return (TRUE);                           // Continue harvesting
}


//
// GetSymbolsUsingMsEngine()
// *************************
//
// Harvest symbols using the MS symbol engine (DbgHelp.dll).
// We do this when there's no symbol format we understand. We just
// let the MS symbol engine give us whatever symbols it can find.
//
// Returns: 0 if *ANY* symbols are found anywhere
//          non-zero on errors
//
int GetSymbolsUsingMsEngine(CONTEXT_REC *cr)
{
   HANDLE hProcess = 0;                     // We just need a dummy handle
   int rc;
   uintptr baseaddr = 0;                    // Addr where symbol engine loaded module
   IMAGEHLP_MODULE mi;
   DWORD symopt;
   int unload = 0, cleanup = 0;
   MOD_NODE *mn;
   int sym_file_mismatch = 0;               // 1 = symbol file doesn't match image
   uint di_ts, di_cs, lm_ts, lm_cs, sym_ts, sym_cs, imdde_ts;
   char *ext;


   dbghv(("> GetSymbolsUsingMsEngine: name='%s', searchpath='%s', load_addr=0x%p, cr=0x%p\n",
          cr->imgname, cr->searchpath, cr->load_addr, cr));

   //
   // Initialize MS symbol harvester engine
   //
   ms_syms = ms_syms_kept = ms_symtype = 0;
   start_sfn = end_sfn = NULL;
   start_sln = end_sln = NULL;
   strbytes = l_cnt = s_cnt = 0;

   if (!SymInitialize(hProcess, cr->searchpath, FALSE)) {
      errmsg(("*E* GetSymbolsUsingMsEngine: SymInitialize() error: %d\n",GetLastError()));
      rc = DBGHELP_ERROR;
      goto TheEnd;
   }
   cleanup = 1;                             // Do SymCleanup() when done

   //
   // Set some options.
   // I tried setting all the undefined bits but nothing made the
   // symbol engine return static symbols.
   // EMP: let it load unmatched PDBs. That way I can see that the PDB is
   //      being found and I can do the checking myself.
   //
   symopt = SymGetOptions();

   dbghv(("***** SymOptions in effect:\n"));
   if (symopt & SYMOPT_ALLOW_ABSOLUTE_SYMBOLS)
       dbghv(("  - SYMOPT_ALLOW_ABSOLUTE_SYMBOLS\n"));
   if (symopt & SYMOPT_AUTO_PUBLICS)
       dbghv(("  - SYMOPT_AUTO_PUBLICS\n"));
   if (symopt & SYMOPT_CASE_INSENSITIVE)
       dbghv(("  - SYMOPT_CASE_INSENSITIVE\n"));
   if (symopt & SYMOPT_DEBUG)
       dbghv(("  - SYMOPT_DEBUG\n"));
   if (symopt & SYMOPT_DEFERRED_LOADS)
       dbghv(("  - SYMOPT_DEFERRED_LOADS\n"));
   if (symopt & SYMOPT_EXACT_SYMBOLS)
       dbghv(("  - SYMOPT_EXACT_SYMBOLS\n"));
   if (symopt & SYMOPT_FAIL_CRITICAL_ERRORS)
       dbghv(("  - SYMOPT_FAIL_CRITICAL_ERRORS\n"));
// if (symopt & SYMOPT_FLAT_DIRECTORY)
//     dbghv(("  - SYMOPT_FLAT_DIRECTORY\n"));
   if (symopt & SYMOPT_IGNORE_CVREC)
       dbghv(("  - SYMOPT_IGNORE_CVREC\n"));
// if (symopt & SYMOPT_IGNORE_IMAGEDIR)
//     dbghv(("  - SYMOPT_IGNORE_IMAGEDIR\n"));
   if (symopt & SYMOPT_IGNORE_NT_SYMPATH)
       dbghv(("  - SYMOPT_IGNORE_NT_SYMPATH\n"));
   if (symopt & SYMOPT_INCLUDE_32BIT_MODULES)
       dbghv(("  - SYMOPT_INCLUDE_32BIT_MODULES\n"));
   if (symopt & SYMOPT_LOAD_ANYTHING)
       dbghv(("  - SYMOPT_LOAD_ANYTHING\n"));
   if (symopt & SYMOPT_LOAD_LINES)
       dbghv(("  - SYMOPT_LOAD_LINES\n"));
   if (symopt & SYMOPT_NO_CPP)
       dbghv(("  - SYMOPT_NO_CPP\n"));
   if (symopt & SYMOPT_NO_IMAGE_SEARCH)
       dbghv(("  - SYMOPT_NO_IMAGE_SEARCH\n"));
   if (symopt & SYMOPT_NO_PROMPTS)
       dbghv(("  - SYMOPT_NO_PROMPTS\n"));
   if (symopt & SYMOPT_NO_PUBLICS)
       dbghv(("  - SYMOPT_NO_PUBLICS\n"));
   if (symopt & SYMOPT_NO_UNQUALIFIED_LOADS)
       dbghv(("  - SYMOPT_NO_UNQUALIFIED_LOADS\n"));
// if (symopt & SYMOPT_OVERWRITE)
//     dbghv(("  - SYMOPT_OVERWRITE\n"));
   if (symopt & SYMOPT_PUBLICS_ONLY)
       dbghv(("  - SYMOPT_PUBLICS_ONLY\n"));
   if (symopt & SYMOPT_SECURE)
       dbghv(("  - SYMOPT_SECURE\n"));
   if (symopt & SYMOPT_UNDNAME)
       dbghv(("  - SYMOPT_UNDNAME \n"));

   symopt |= (SYMOPT_INCLUDE_32BIT_MODULES |  // Include 32-bit modules on Win64
              SYMOPT_IGNORE_CVREC);           // Ignore path info in CV record
   dbghv(("***** Adding SymOptions SYMOPT_INCLUDE_32BIT_MODULES and SYMOPT_IGNORE_CVREC.\n"));

   if (!G(demangle_cpp_names)) {
      symopt &= ~SYMOPT_UNDNAME;            // Don't undecorate names
      dbghv(("***** Removing SymOptions SYMOPT_UNDNAME.\n"));
   }

   if (G(lineno)) {
      symopt |= SYMOPT_LOAD_LINES;
      dbghv(("***** Adding SymOptions SYMOPT_LOAD_LINES.\n"));
   }
   else {
      symopt &= ~SYMOPT_LOAD_LINES;
      dbghv(("***** Removing SymOptions SYMOPT_LOAD_LINES.\n"));
   }

   symopt &= ~SYMOPT_DEFERRED_LOADS;        // Load symbols immediately (don't deferr)
   dbghv(("***** Removing SymOptions SYMOPT_DEFERRED_LOADS.\n"));

   SymSetOptions(symopt);

   //
   // Load the module (or dbg file or sym file or whatever)
   // We need to tell SymLoadModule the address of where we've mapped the image (dh),
   // not the address where the image was loaded (cr->load_addr) when it ran.
   //
#if defined(_32BIT)
   baseaddr = SymLoadModule(hProcess, NULL, cr->imgname, NULL, (uintptr)dh, 0);
#else
   baseaddr = SymLoadModule64(hProcess, NULL, cr->imgname, NULL, 0, 0);
#endif
   if (baseaddr == 0) {
      errmsg(("*E* GetSymbolsUsingMsEngine: SymLoadModule() error: %d\n",GetLastError()));
      rc = DBGHELP_ERROR;
      goto TheEnd;
   }
   unload = 1;                              // Do SymUnloadModule() when done

   cr->mod_addr = (void *)baseaddr;
   mn = (MOD_NODE *)cr->handle;

   //
   // Get info for symbol file associated with the executable
   //
   mi.SizeOfStruct = sizeof(IMAGEHLP_MODULE);
   if (!SymGetModuleInfo(hProcess, baseaddr, &mi)) {
      errmsg(("*E* GetSymbolsUsingMsEngine: SymGetModuleInfo() error: %d\n",GetLastError()));
      rc = DBGHELP_ERROR;
      goto TheEnd;
   }

   //
   // What kind of symbol file is loaded?
   //
   if (mi.LoadedImageName[0] == 0) {
      strcpy(mi.LoadedImageName, cr->imgname);
   }
   else {
      ext = GetExtensionFromFilename(mi.LoadedImageName);
      if (stricmp(ext, ".pdb") == 0)
         mn->type = MODULE_TYPE_PDB;
      else if (stricmp(ext, ".dbg") == 0)
         mn->type = MODULE_TYPE_DBG;
      else if (stricmp(ext, ".sym") == 0)
         mn->type = MODULE_TYPE_SYM;
      else if (stricmp(ext, ".map") == 0)
         mn->type = MODULE_TYPE_MAP_VC;
      else
         mn->type = MODULE_TYPE_PE;
   }

   //
   // Say what type of symbols DbgHelp is giving us
   //
   DumpModuleInfo(&mi);
   dbghv(("*I* GetSymbolsUsingMsEngine: getting "));
   switch(mi.SymType) {
      case SymCoff:
         ms_symtype = SYMBOL_TYPE_MS_COFF;
         dbghv(("COFF "));
         break;
      case SymCv:
         ms_symtype = SYMBOL_TYPE_MS_CODEVIEW;
         dbghv(("CODEVIEW "));
         break;
      case SymExport:
         ms_symtype = SYMBOL_TYPE_MS_EXPORT;
         dbghv(("EXPORT "));
         break;
      case SymPdb:
         ms_symtype = SYMBOL_TYPE_MS_PDB;
         if (mn->type == MODULE_TYPE_DBG)
            mn->type = MODULE_TYPE_DBGPDB;
         dbghv(("PDB "));
         break;
      case SymDia:
         ms_symtype = SYMBOL_TYPE_MS_DIA;
         if (mn->type == MODULE_TYPE_DBG)
            mn->type = MODULE_TYPE_DBGPDB;
         dbghv(("DIA "));
         break;
      case SymSym:
         ms_symtype = SYMBOL_TYPE_MS_SYM;
         dbghv(("SYM "));
         break;
      case SymNone:
         ms_symtype = SYMBOL_TYPE_NONE;
         dbghv(("NONE "));
         break;
      default:
         ms_symtype = SYMBOL_TYPE_MS_OTHER;
         dbghv(("type %d ",mi.SymType));
         break;
   }
   dbghv(("symbols from %s\n",mi.LoadedImageName));

   //
   // One last check before we attempt to enumerate ...
   //
   // **NOTE: Don't check for mi.NumSyms == 0. Even if it is 0
   //         go ahead and enumerate anyway!
   //
   if (ms_symtype == SYMBOL_TYPE_NONE)              // No symbols
      goto TheEnd;

   if (ms_symtype == SYMBOL_TYPE_MS_EXPORT &&       // Exports
       G(symbol_quality) == MODE_GOOD_SYMBOLS &&    // and user doesn't want them
       G(always_harvest) == 0) {                    // and we don't want them either
      dbghv(("- GetSymbolsUsingMsEngine: Only exports available and user doesn't want them.\n"));
      errmsg(("*I* GetSymbolsUsingMsEngine: Only exports available in '%s' and user doesn't want them.\n",mi.LoadedImageName));
      goto TheEnd;
   }

   //
   // If caller gave us a timestamp in the A2nAddmodule() call then check to
   // make sure the loaded symbols and the loaded module timestamps match.
   // If the caller did not give us a timestamp for the loaded module, and we
   // decided to use the image anyway, then see if the image timestamp/checksum
   // matches the symbols and use them if they do.
   //
   // These are not fail-safe checks because there is no straight-forward way
   // of making absolutely sure the symbols match the image. Only Microsoft
   // knows how to do that and they don't tell us.
   //
   // What we'll do is check if the checksums match and if that fails then
   // check if the timestamps match. If that fails too then the symbols
   // don't match.
   //
   // Here's some terminology:
   // * lm_ts/lm_cs
   //   - timestamp and checksum for loaded module (ie. given to us in the
   //     A2nAddModule call).
   // * di_ts/di_cs
   //   - timestamp and checksum for disk image (ie. the image we're looking
   //     at right now).
   // * sym_ts/sym_cs
   //   - timestamp and checksum for loaded symbols
   // * imdde_ts
   //   - timestamp of image's MISC debug directory entry
   //
   //                   Checksum Check
   //                   **************
   //
   //      lm_cs  di_cs  sym_cs   Result
   //      -----  -----  ------   ------------------------------------
   //        0              0     Can't tell. Check Timestamps
   //        0              X     Check di_cs/sym_cs (>A<)
   //        X              0     Can't tell. Check Timestamps
   //        X              X     ***** Module and symbols match *****
   //        X              Y     No match. Check Timestamps
   //        Y              X     No match. Check Timestamps
   //
   //  (>A<)        0       X     Can't tell. Check Timestamps
   //               X       X     ***** Module and symbols match *****
   //               X       Y     No match. Check Timestamps
   //               Y       X     No match. Check Timestamps
   //
   //
   //                   Timestamp Check
   //                   ***************
   //
   //      lm_ts  di_ts  sym_ts   Result
   //      -----  -----  ------   ------------------------------------
   //        0              0     Unable to check. Get symbols anyway
   //        0              X     Check di_ts/sym_ts (>B<)
   //        X              0     Unable to check. Get symbols anyway
   //        X              X     ***** Module and symbols match *****
   //        X              Y     Symbols don't match module
   //        Y              X     Symbols don't match module
   //
   //  (>B<)        0       X     Unable to check. Get symbols anyway
   //               X       X     ***** Module and symbols match *****
   //               X       Y     Symbols don't match module
   //               Y       X     Symbols don't match module
   //
   // NOTE:
   //   When checking timestamps it is also OK for the symbol file timestamp
   //   to match the image's MISC debug directory timestamp.
   //
   di_ts = cr->img_ts;
   di_cs = cr->img_cs;
   lm_ts = mn->ts;
   lm_cs = mn->chksum;
   sym_ts = mi.TimeDateStamp;
   sym_cs = mi.CheckSum;
   imdde_ts = img_misc_debug_dir_ts;

   if (sym_cs != 0  &&  (sym_cs == lm_cs  ||  sym_cs == di_cs)) {
      // Loaded module *OR* disk image checksum matches symbols checksum (and are non-zero)
      dbghv(("- GetSymbolsUsingMsEngine: module|image and symbols checksum matches. Symbols are good.\n"));
   }
   else if (sym_ts != 0  &&  (sym_ts == lm_ts  ||  sym_ts == di_ts  || sym_ts == imdde_ts)) {
      // Loaded module *OR* disk image timestamp matches symbols timestamp (and are non-zero)
      dbghv(("- GetSymbolsUsingMsEngine: module|image and symbols timestamp matches. Symbols are good.\n"));
   }
   else if (lm_ts == 0 || di_ts == 0 || sym_ts == 0) {
      // One or more of the timestamps is zero - Unable to validate
      warnmsg(("*W* GetSymbolsUsingMsEngine: Checksums don't match and one or more timestamps are zero.\n"));
      warnmsg(("    - symbols/module: '%s'/'%s'\n",mi.LoadedImageName, cr->imgname));
      warnmsg(("    - loaded module  cs=0x%08X ts=0x%08x => %s",lm_cs, lm_ts, TsToDate(lm_ts)));
      warnmsg(("    - disk image     cs=0x%08X ts=0x%08x => %s",di_cs, di_ts, TsToDate(di_ts)));
      warnmsg(("    - loaded symbols cs=0x%08X ts=0x%08x => %s",sym_cs, sym_ts, TsToDate(sym_ts)));
      if (G(validate_symbols)) {
         goto TheEnd;
      }
      else {
         warnmsg(("    - Symbol validation failed. ***** SYMBOLS MAY NOT BE VALID *****\n"));
         sym_file_mismatch = 1;          // symbol file doesn't match image
      }
   }
   else {
      // Timestamps non-zero and don't match - Symbols don't match image
      warnmsg(("*W* GetSymbolsUsingMsEngine: Checksum and timestamp mismatch\n"));
      warnmsg(("    - symbols/module: '%s'/'%s'\n",mi.LoadedImageName, cr->imgname));
      warnmsg(("    - loaded module  cs=0x%08X ts=0x%08x => %s",lm_cs, lm_ts, TsToDate(lm_ts)));
      warnmsg(("    - image_misc_dir_entry         ts=0x%08x => %s",imdde_ts, TsToDate(imdde_ts)));
      warnmsg(("    - disk image     cs=0x%08X ts=0x%08x => %s",di_cs, di_ts, TsToDate(di_ts)));
      warnmsg(("    - loaded symbols cs=0x%08X ts=0x%08x => %s",sym_cs, sym_ts, TsToDate(sym_ts)));
      if (G(validate_symbols)) {
         goto TheEnd;
      }
      else {
         warnmsg(("    - Symbols validation failed. ***** SYMBOLS MAY NOT BE VALID *****\n"));
         sym_file_mismatch = 1;          // symbol file doesn't match image
      }
   }

   //
   // OK looks like we want them. Enumerate symbols ...
   //
   if (pSymEnumSymbols == NULL) {
      errmsg(("*E* GetSymbolsUsingMsEngine: SymEnumSymbols() not supported. Unable to gather symbols.\n"));
   }
   else {
      dbghv((" ******************** MS SYMBOL ENGINE SYMBOLS ***********************\n"));
#if defined(_32BIT)
      dbghv((" Num      Addr       Size    Name\n"));
      dbghv((" ----  ----------   ------   ----------\n"));
#else
      dbghv((" Num         Addr          Size    Name\n"));
      dbghv((" ----  ----------------   ------   ----------\n"));
#endif

      ms_syms = ms_syms_kept = 0;              // Re-initialize
      SymEnumSymbols(hProcess, baseaddr, "*", EnumSymbolsCallback, cr);
      dbghv((" ******************** MS SYMBOL ENGINE SYMBOLS ***********************\n"));

      //
      // Remember where the symbols came from.
      // It is very possible the symbols come from the image itself if, for
      // example, the Symbol Engine gives us EXPORTS.
      //
      if (ms_syms_kept > 0) {
         mn->hvname = strdup(mi.LoadedImageName);           // Symbols came from here
         if (stricmp(mi.LoadedImageName, cr->imgname) != 0)
            mn->flags |= A2N_FLAGS_SYMBOLS_NOT_FROM_IMAGE;  // and it's not the on-disk image
         if (sym_file_mismatch)
            mn->flags |= A2N_FLAGS_VALIDATION_FAILED;       // Symbols may not match image
      }
   }

   //
   // Enumerate line numbers if needed
   //
   if (G(lineno)) {
      dbghv((" ******************** START ENUMERATING LINES ***********************\n"));
      dbghv(("  seq    ofst     line   filename\n"));
      dbghv((" ----- --------  ------  -----------------------------------------\n"));
      if (!pSymEnumLines(hProcess, baseaddr, NULL, NULL, AddLineNumberNode, NULL)) {
         errmsg(("*E* GetSymbolsUsingMsEngine: SymEnumLines() failed. rc = %d\n",GetLastError()));
      }
      dbghv((" ********************  END ENUMERATING LINES ***********************\n"));

      // Remember line information in the module node
      mn->li.srclines_list = start_sln;
      mn->li.srcfiles = start_sfn;
      mn->li.linecnt = l_cnt;
      mn->li.srccnt = s_cnt;
      mn->strsize += strbytes;

      // Create the line node pointer array and sort lines in ascending offset order
      CreateSrcLineNumberArray(mn);
   }
   if (mn->li.linecnt > 0)
      mn->flags &= ~A2N_FLAGS_NO_LINENO;

   //
   // Done
   //
TheEnd:
   if (unload)
      SymUnloadModule(hProcess, baseaddr);

   if (cleanup)
      SymCleanup(hProcess);

   dbghv(("< GetSymbolsUsingMsEngine: ms_syms_kept=%d  linecnt=%d\n",ms_syms_kept,l_cnt));
   return (ms_syms_kept);
}
