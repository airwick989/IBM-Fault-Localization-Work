//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                        Windows MAP Symbol Harvester                      //
//                        ----------------------------                      //
//                                                                          //
// Adapted from Chris Richardson's version for ATAS                         //
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


//
// Globals
//
uint   timestamp;                           // Map/Module timestamp
size_t load_addr;                           // Module load address

char *Timestamp_str =     " Timestamp is ";
char *LoadAddress_str =   " Preferred load address is ";
char *SectionTable_str =  " Start         Length     Name";
char *PublicSymbols_str = " Publics by Value";
char *EntryPoint_str =    " entry point at ";
char *StaticSymbols_str = " Static symbols";
char *ExportSymbols_str = "  ordinal    name";
char *LastLine_str =      " bytes saved through ";


//
// Internal prototypes
//
static int    hex2i(char c);
static uint   hex2ul(char *hexStr);
static size_t hex2size_t(char *hexString);

static size_t NextLine(char *source);
static size_t NextParagraph(char *source);
static uint   ReadTokenHexToUlong(char *fileptr,char *token);
static size_t ReadTokenHexToSize_t(char *fileptr,char *token);
static size_t GetNextWord(char *source, char *buf);
static size_t ReadTokenStr(char *fileptr,char *token,char *buf);

static int    CheckSectionTable(char *start, char *end);
static int    HarvestMapSymbols(char *start, char *end, CONTEXT_REC *cr);


struct _csec_info {
   int number;
   int size;
   int executable;
};



//
// GetSymbolsFromMapFile()
// ***********************
//
// Read a map file (produced by MS Visual C++) and extract symbols from it.
// Returns number of symbols found or 0 if no symbols or errors.
//
// Map file format on Win32:
// -------------------------
//          11111111112222222222333333333344444444445
// 12345678901234567890123456789012345678901234567890
//  a2n
//
//  Timestamp is 3ce3f6b0 (Thu May 16 13:13:04 2002)
//
//  Preferred load address is 10000000
//
//  Start         Length     Name                   Class
//  0001:00000000 0001a95aH .text                   CODE
//  0002:00000168 00000de4H .rdata                  DATA
//  ...
//  0003:0000dda8 00002ba4H .bss                    DATA
//
//   Address         Publics by Value              Rva+Base     Lib:Object
//
//  0001:00000000       _DllMain@12                10001000 f   a2n.obj
//  0001:0000090e       _A2nListControls           1000190e f   a2n.obj
//  ...
//
//  entry point at        0001:0000d9c7
//
//  Static symbols
//
//  0001:0000b59c       _WriteByteOrder            1000c59c f   saveres.obj
//  0001:0000b5ca       _ReadByteOrder             1000c5ca f   saveres.obj
//  ...
//
// Line numbers for winms.obj(d:\src\tools\a2n\winms.c) segment .text
//
//     67 0001:0003a4e0    68 0001:0003a531    71 0001:0003a541    72 0001:0003a551
//  ...
//    265 0001:0003ba10   266 0001:0003ba70   267 0001:0003ba80
//
// Line numbers for winmap.obj(d:\src\tools\a2n\winmap.c) segment .text
//  ...
//
// FIXUPS: c4ea 44 9 2a4 1f fffffd5a a 6 8 13 18 3c 32 10 24 1f 1b 16 12 fffffd20
//  ...
// FIXUPS: c24a 1e 1e 1f 1e 16 16 16 37 30 fffff082 17 21 19 c 12
//
//  Exports
//
//   ordinal    name
//
//         1    _A2nAddJittedMethod
//                exported name: A2nAddJittedMethod
//   ...
//        34    _GetHarvesterVersion
//                exported name: GetHarvesterVersion
// ------------------- End Of Map --------------------     (not a line in the map!)
//
//
// Map file format on Win64:
// -------------------------
//          11111111112222222222333333333344444444445
// 12345678901234567890123456789012345678901234567890
//  a2n
//
//  Timestamp is 3ce3f6b0 (Thu May 16 13:13:04 2002)
//
//  Preferred load address is 0000000010000000
//
//  Start         Length     Name                   Class
//  0001:00000000 0001a95aH .text                   CODE
//  0002:00000168 00000de4H .rdata                  DATA
//  ...
//  0003:0000dda8 00002ba4H .bss                    DATA
//
//   Address         Publics by Value              Rva+Base     Lib:Object
//
//  0001:00000000       .DllMain                   0000000010002000 f   a2n.obj
//  0001:00002060       .A2nListControls           0000000010004060 f   a2n.obj
//  ...
//
//  entry point at        0001:0000d9c7
//
//  Static symbols
//
//  0001:00023380       .WriteByteOrder            0000000010025380 f   saveres.obj
//  0001:00023440       .ReadByteOrder             0000000010025440 f   saveres.obj
//  ...
//
// Line numbers for winms.obj(d:\src\tools\a2n\winms.c) segment .text
//
//     67 0001:0003a4e0    68 0001:0003a531    71 0001:0003a541    72 0001:0003a551
//  ...
//    265 0001:0003ba10   266 0001:0003ba70   267 0001:0003ba80
//
// Line numbers for winmap.obj(d:\src\tools\a2n\winmap.c) segment .text
//  ...
//
//  Exports
//
//   ordinal    name
//
//         1    A2nAddJittedMethod
//  ...
//        34    GetHarvesterVersion
// ------------------- End Of Map --------------------     (not a line in the map!)
//
//
// Assumptions:
// - Line 1 contains one word and that is the name (without extension) of the
//   image file for which this map was generated.
// - Line 3 contains the timestamp when the image was generated.
// - Line 5 contains the preferred load address of the image.  It is used to
//   calculate symbol offsets.
// - The Section Table must *NOT* contain more sections (segments) than there
//   are sections in the image.
// - The length of CODE sections must match the lengths of the sections
//   in the image.
// - Only symbols marked as "f" (functions) are harvested.
// - There can be many "Line Number" sections. They're all ignored.
// - There can be many FIXUP sections (Win32 only). They're all ignored.
// - Exports are ignored.
// - The order of the map file sections is as follows:
//   * image_name
//   * Timestamp is ...
//   * Preferred load address is ...
//   * Start         Length     Name                <- Section Table
//   * Address         Publics by Value             <- Publics
//   * entry point at ...                           <- Image entry point
//   * Static symbols                               <- Statics
//   * Line numbers for ...                         <- Line Numbers (Optional, many of these)
//   * FIXUPS: ...                                  <- Fixups (Optional, many of these)
//   * Exports                                      <- Exports (Optional)
//
int GetSymbolsFromMapFile(char *mapname, uint ts, CONTEXT_REC *cr)
{
   char *pSectionTable;                     // Section table
   char *pPublicSymbols;                    // Publics by value
   char *pEntryPoint;                       // entry point
   char *pStaticSymbols;                    // Static symbols
   char *pExportSymbols;                    // Exports (if present)
   char *pLastLine;                         // last line of map file
   char *pFileEnd;                          // EOF

   char modname[MAX_FILENAME_LEN];          // Name of module whose map this is
   char *map_base;                          // Address where we map the map
   size_t mapsize;                          // Size of map (in bytes)

   int  symcnt = 0;                         // Total symbols found
   int sym_file_mismatch = 0;               // 1 = symbol file doesn't match image

   char fn[MAX_FILENAME_LEN];               // Map filename (without path)
   char *c;

   MOD_NODE *mn;


   dbghv(("> GetSymbolsFromMapFile: mapfn='%s', ts=%X, cr=%p\n",mapname,ts,cr));
   //
   // Map the map
   //
   map_base = (char *)MapFile(mapname, &mapsize);
   if (map_base == NULL) {
      errmsg(("*E* GetSymbolsFromMapFile: unable to map file '%s'.\n",mapname));
      goto TheEnd;
   }

   //
   // Line 1: Read module name (1st word in map)
   // Line 3: Read and convert timestamp
   // Line 5: Read and convert load address
   //
   ReadTokenStr(map_base, "", modname);
   timestamp = ReadTokenHexToUlong(map_base, Timestamp_str);
   load_addr = ReadTokenHexToSize_t(map_base, LoadAddress_str);

   //
   // Find start of map sections we care about
   //
   pSectionTable  = strstr(map_base, SectionTable_str);   // Find Section Table
   pPublicSymbols = strstr(map_base, PublicSymbols_str);  // Find Publics
   pEntryPoint    = strstr(map_base, EntryPoint_str);     // Find Entry Point
   pStaticSymbols = strstr(map_base, StaticSymbols_str);  // Find Statics
   pExportSymbols = strstr(map_base, ExportSymbols_str);  // Find Exports
   pLastLine      = strstr(map_base, LastLine_str);       // Find Last line
   pFileEnd = map_base + strlen(map_base);
   if (pLastLine == NULL) pLastLine = pFileEnd;

   dbghv(("- GetSymbolsFromMapFile: modname        = %s\n", modname));
   dbghv(("- GetSymbolsFromMapFile: timestamp      = %X => %s", timestamp, TsToDate(timestamp)));
   dbghv(("- GetSymbolsFromMapFile: load_addr      = %p\n", load_addr));
   dbghv(("- GetSymbolsFromMapFile: pSectionTable  = %p\n", pSectionTable));
   dbghv(("- GetSymbolsFromMapFile: pPublicSymbols = %p\n", pPublicSymbols));
   dbghv(("- GetSymbolsFromMapFile: pEntryPoint    = %p\n", pEntryPoint));
   dbghv(("- GetSymbolsFromMapFile: pStaticSymbols = %p\n", pStaticSymbols));
   dbghv(("- GetSymbolsFromMapFile: pLastLine      = %p\n", pLastLine));
   dbghv(("- GetSymbolsFromMapFile: pFileEnd       = %p\n", pFileEnd));
   dbghv(("- GetSymbolsFromMapFile: mapsize        = %d\n", mapsize));

   //
   // Make sure this map file is for the module we want.
   // Peel off the map name (the "thing" after the last "\") and
   // compare it to the module name listed in the map.
   //
   c = GetFilenameFromPath(mapname);
   strcpy(fn, c);                           // Only the map file name (no path)
   c = GetExtensionFromFilename(fn);
   *c = 0;
   if (COMPARE_NAME(modname,fn) != 0) {
      errmsg(("*E* GetSymbolsFromMapFile: Image name not same as image name in map: %s vs %s\n",fn,modname));
      goto TheEnd;
   }

   //
   // Make sure map timestamp matches module timestamp.
   //
   if (timestamp == 0) {
      errmsg(("*E* GetSymbolsFromMapFile: timestamp not found. Unsupported map file format.\n"));
      goto TheEnd;
   }
   if (ts == 0) {
      warnmsg(("*W* GetSymbolsFromMapFile: '%s' TS is 0. Unable to check if symbols match the image.\n",cr->imgname));
      sym_file_mismatch = 1;                // symbol file doesn't match image
   }
   else {
      if (ts != timestamp) {
         warnmsg(("*W* GetSymbolsFromMapFile: '%s'/'%s' timestamp mismatch: %08x vs %08x\n",
                 mapname, cr->imgname, timestamp, ts));
//       warnmsg(("    - ts 0x%08x is: %s",timestamp, ctime((time_t *)&timestamp)));
//       warnmsg(("    - ts 0x%08x is: %s",ts, ctime((time_t *)&ts)));
         if (G(validate_symbols)) {
//          errmsg(("    - Symbol validation failed. Symbols discarded.\n"));
            goto TheEnd;
         }
         else {
            warnmsg(("    - Symbols validation failed. ***** SYMBOLS MAY NOT BE VALID *****\n"));
//          warnmsg(("    - User requested no validation - using symbols anyway.\n"));
            sym_file_mismatch = 1;          // symbol file doesn't match image
         }
      }
   }

   //
   // Make sure there's a section table and read it.
   // Check that there are <= sections as there are sections in module.
   //
   if (pSectionTable == NULL) {
      errmsg(("*E* GetSymbolsFromMapFile: Section Table not found. Unsupported map file format.\n"));
      goto TheEnd;
   }

   if (CheckSectionTable(pSectionTable+NextParagraph(pSectionTable), pPublicSymbols) != 0) {
      errmsg(("*E* GetSymbolsFromMapFile: Section Data from map does not match image.\n"));
      goto TheEnd;
   }

   //
   // If there are public symbols gather them.
   //
   if (pPublicSymbols != 0) {
      dbghv(("---------- Public Symbols ----------\n"));
      symcnt += HarvestMapSymbols(pPublicSymbols+NextParagraph(pPublicSymbols), pEntryPoint, cr);
   }
   else {
      dbghv(("- GetSymbolsFromMapFile: No publics ...\n"));
   }

   //
   // If there are static symbols gather them.
   //
   if (pStaticSymbols != 0) {
      dbghv(("---------- Static Symbols ----------\n"));
      symcnt += HarvestMapSymbols(pStaticSymbols+NextParagraph(pStaticSymbols), pLastLine, cr);
   }
   else {
      dbghv(("- GetSymbolsFromMapFile: No statics ...\n"));
   }

   //
   // Done
   //
TheEnd:
   if (map_base != NULL)
      UnmapFile((void *)map_base);

   if (symcnt != 0) {
      mn = (MOD_NODE *)cr->handle;
      mn->type = MODULE_TYPE_MAP_VC;                // Set new module type
      if (sym_file_mismatch)
         mn->flags |= A2N_FLAGS_VALIDATION_FAILED;  // Symbols may not match image
   }

   dbghv(("< GetSymbolsFromMapFile: found %d symbols\n",symcnt));
   return (symcnt);
}


//
// CheckSectionTable()
// *******************
//
static int CheckSectionTable(char *start, char *end)
{
   char *cursor;
   int seg = 1, secnum = 0;
   uint seclen = 0;
   char tok_so[32];
   char tok_len[32];
   char tok_name[32];
   char tok_type[32];

#define CSI_SIZE   64
   struct _csec_info csi[CSI_SIZE];
   int csec_cnt = 0;
   int i;


   dbghv(("> CheckSectionTable: start = %p  end = %p\n", start, end));

   memset(csi, 0, (sizeof(struct _csec_info) * CSI_SIZE));

   cursor = start;
   while (*cursor != CR && *cursor != LF && cursor < end) {
      //
      // Read section start address (in Seg:Off form)
      // The lines look like this:
      //
      //  0001:00000000 0001bf1aH .text                   CODE
      //  0002:00000000 00000168H .idata$5                DATA
      //  0003:00000004 00000004H .CRT$XCZ                DATA
      //  ---- -------- --------- --------                ----
      //  seg   offset   length     name                  type
      //
      // Segment is not really a section, but a "segment" of the image
      // that is loaded into memory.  Sections may be combined into a
      // segment so we should never have more segments than sections.
      //
      cursor += GetNextWord(cursor, tok_so);   // Read seg:off token
      seg = hex2ul(tok_so);	

      cursor += GetNextWord(cursor, tok_len);  // Read segment length ...
      seclen = hex2ul(tok_len);                // ... and convert to UL

      cursor += GetNextWord(cursor, tok_name); // Read segment name
      cursor += GetNextWord(cursor, tok_type); // Read segment type

      dbghv(("- CheckSectionTable:  %s  %s  %-8s   %s\n", tok_so,tok_len,tok_name,tok_type));

      //
      // Make sure we don't have more sections in the map than there
      // are sections in the module.
      //
      if (seg > scnt) { // number of sections!
         errmsg(("*E* CheckSectionTable: Seg %d is larger than image number of sections (%d)\n", seg, scnt));
         return (1);
      }

      //
      // Remember data about this segment
      //
      csi[seg].number = seg;
      csi[seg].size += seclen;
      if (strcmp(tok_type,"CODE") == 0)
         csi[seg].executable++;             // Executable segment

      cursor += NextLine(cursor);           // Skip to next line
   }

   //
   // Now check that all code sections are the same size as in the image
   // Don't bother with the name check. We already know the timestamp matches
   // so we're just making sure this map is who it says it is.
   //
   for (i = 0; i < CSI_SIZE; i++) {
      //
      // Check against what we read from image
      //
      if (csi[i].executable == 0)
         continue;

      csec_cnt++;
      seg    = csi[i].number;
      secnum = seg - 1;
      seclen = csi[i].size;

      dbghv(("\n- CheckSectionTable: Map segment %d is executable. Length = %X  Segments = %d\n", seg, seclen, csi[i].executable));
      dbghv(("- CheckSectionTable: Image section data: flags=%08X, name=%s, size=%X\n",
              sec[secnum].flags, sec[secnum].name, sec[secnum].size));

      if ((sec[secnum].flags & IMAGE_SCN_MEM_EXECUTE) == 0) {
         errmsg(("*E* CheckSectionTable: Section %d not executable in image\n",seg));
         return (1);
      }

      if (seclen != sec[secnum].size) {
         errmsg(("*E* CheckSectionTable: Section %d size different from image: 0x%X vs 0x%X\n",
                 seg, seclen, sec[secnum].size));
         return (1);
      }

      // Looks OK to me ...
      dbghv(("- CheckSectionTable: Map and Image agree on this segment. Continuing ...\n"));
   }

   if (csec_cnt == 0) {
      errmsg(("*E* CheckSectionTable: There are no executable (CODE) sections listed. Quitting.\n"));
      return (1);
   }

   //
   // Done
   //
   dbghv(("< CheckSectionTable: Done.\n"));
   return (0);
}


//
// HarvestMapSymbols()
// *******************
//
// Parses either the publics or statics sections and gathers
// approriate symbols from them.
//
static int HarvestMapSymbols(char *start, char *end, CONTEXT_REC *cr)
{
   char *cursor;
   char tok_so[32];                         // Seg:Offset address
   char tok_sym[2048];                      // Symbol name
   char tok_addr[32];                       // Rva + Base
   char tok_f[128];                         // "f" or obj name
   uint seg = 1;

   int  rc;
   int  symcnt = 0;                         // number of symbols found
   char *sym_name;                          // symbol name
   uint sym_offset;                         // symbol offset
   char *sym_code = NULL;                   // Symbol code (or NULL if no code)

   dbghv(("> HarvestMapSymbols: start = %p  end = %p  cr = %p\n", start, end, cr));

   cursor = start;
   while (*cursor != CR && cursor < end) {
      //
      // Tokenize line
      //
      //     tok_so       tok_sym     tok_addr          tok_f
      //  -------------  -----------  ----------------  -----
      //  0001:00000000  _DllMain@12  10001000            f    a2n.obj   <-- Win32
      //  0001:00000000  .DllMain     0000000010001000    f    a2n.obj   <-- Win64
      //
      cursor += GetNextWord(cursor, tok_so);    // Seg:Offset address
      seg = hex2ul(tok_so);	
      cursor += GetNextWord(cursor, tok_sym);   // Symbol name
      cursor += GetNextWord(cursor, tok_addr);  // Rva + Base
      cursor += GetNextWord(cursor, tok_f);     // "f" or obj name

      //
      // Only keep symbols that are functions
      // *** Now we keep everything, as long as the segment is non-zero
      //     and smaller than the total number of sections in the image.
      //     Win64 maps have some <linker-defined> symbols that are in
      //     segment 0 and that does not sit well with this code!
      //
//1   if (strcmp(tok_f,"f") == 0) {
      if (seg >= 1) {
         sym_name = tok_sym;
         sym_offset = (uint)(hex2size_t(tok_addr) - load_addr); // Calculate offset

         //
         // Remove leading underscore/at/dot character added by compiler
         //
#if defined(_X86)
         if (*sym_name == LEAD_CHAR_US || *sym_name == LEAD_CHAR_AT) sym_name++;
#else
         if (*sym_name == LEAD_CHAR_DOT) sym_name++;
#endif
         dbghv(("** Keep -> 0x%p (seg: %d) %s\n",sym_offset,seg,sym_name));

         //
         // Demangle (Undecorate in MS speak) name
         //
         if (G(demangle_cpp_names))
            sym_name = DemangleName(sym_name);

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
         sec[seg - 1].symcnt++;
         if (G(gather_code)) {
            sym_code = GetCodePtrForSymbol(sym_offset, seg);
            DumpCode((uchar *)sym_code, 4); // Don't know size so dump first 4 bytes
         }

         //
         // Send the symbol back to A2N
         //
         rc = SendBackSymbol(sym_name,              // Symbol name
                             sym_offset,            // Symbol offset
                             0,                     // Symbol size
                             SYMBOL_TYPE_FUNCTION,  // Symbol type (label, export, etc.)
                             seg,                   // Section to which the symbol belongs
                             sym_code,              // Code associated with this symbol
                             (void *)cr);           // Context record
         if (rc) {
            dbghv(("< HarvestMapSymbols: SendBackSymbol returned %d. Stopping.\n",rc));
            return (symcnt);                // Destination wants us to stop harvesting
         }

         symcnt++;                          // Another symbol added successfully
      }
      else {
         dbghv(("** Discard -> %s %s %s  *** invalid segment ***\n",tok_so,tok_sym,tok_addr));
      }
//1   }
//1   else {
//1      dbghv(("** Discard -> %s\n",tok_sym));
//1   }

      cursor += NextLine(cursor);           // On to the next line
   }

   //
   // Done
   //
   dbghv(("< HarvestMapSymbols: found %d symbols\n", symcnt));
   return (symcnt);
}


//
// NextLine()
// **********
//
static size_t NextLine(char *source)
{
   char *str = source;

   while (*str != CR && *str != LF) str++;
   if (*str == CR) str++;
   if (*str == LF) str++;

   return (str - source);
}


//
// NextParagraph()
// ***************
//
static size_t NextParagraph(char *source)
{
   char *str = source;

   while (*str != CR && *str != LF) str++;
   while (*str == CR || *str == LF) str++;

   return (str - source);
}


//
// ReadTokenStr()
// **************
//
static size_t ReadTokenStr(char *source, char *token, char *buf)
{
   char *cursor;

   if ((cursor = strstr(source,token))) {
      cursor += strlen(token);
      return (GetNextWord(cursor,buf));
   }
   return (0);
}


//
// GetNextWord()
// *************
//
static size_t GetNextWord(char *source, char *buf)
{
   char *str = source;

   while (*str == ' ' || *str == 0x09) str++;   // Skip tabs and spaces
   while (!isspace(*str)) *buf++ = *str++;
   *buf = 0;

   return (str - source);
}


//
// ReadTokenHexToUlong()
// *********************
//
static uint ReadTokenHexToUlong(char *source, char *token)
{
   char *cursor;

   if ((cursor = strstr(source,token))) {
      cursor += strlen(token);
      return (hex2ul(cursor));
   }
   return (0);
}


//
// ReadTokenHexToSize_t()
// *********************
//
static size_t ReadTokenHexToSize_t(char *source, char *token)
{
   char *cursor;

   if ((cursor = strstr(source,token))) {
      cursor += strlen(token);
      return (hex2size_t(cursor));
   }
   return (0);
}


//
// hex2size_t()
// ************
//
static size_t hex2size_t(char *hex)
{
   size_t st = 0;
   uint   i;
   char   *c = hex;

   for (i = 0; i < (sizeof(size_t) * 2); i++, c++) {
      if (!isxdigit(*c)) break;
      st = st * 0x10 + hex2i(*c);
   }
   return (st);
}


//
// hex2ul()
// ********
//
static uint hex2ul(char *hex)
{
   uint32 i, ul = 0;
   char *c = hex;

   for (i = 0; i < 8; i++, c++) {
      if (!isxdigit(*c)) break;
      ul = ul * 0x10 + hex2i(*c);
   }
   return (ul);
}


//
// hex2i()
// *******
//
static int hex2i(char c)
{
   if (!isxdigit(c))             return -1;
   if (('0' <= c) && (c <= '9')) return (c - '0');
   if (('a' <= c) && (c <= 'f')) return (c - 'a' + 0xa);

   return (c - 'A' + 0xa);
}
