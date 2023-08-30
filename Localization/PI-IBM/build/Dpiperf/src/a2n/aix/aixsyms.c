//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                       AIX Symbol Harvester Mainline                      //
//                       -----------------------------                      //
//                                                                          //
// REQUIRED functions:                                                      //
//  - GetHarvesterVersion()                                                 //
//  - GetSymbolsForModule()                                                 //
//  - ValidateKernelSymbols()                                               //
//**************************************************************************//
//
//
// A2N handles the following types of "symbol" files:
// **************************************************
//
// XCOFF:    Extension of COFF. Documented. Well understood by A2N.
//           Exists either in the executable image or in a seprate file.
//           A2N harvests this format. Produces the highest quality symbols.
//
// Compiler/Linker flags required:
// *******************************
//                     Compiler                Linker
//                     ----------    ----------------------------------------
// XCOFF               ???           ???
//
//
// What the various DEBUG sections contain:
// ****************************************
// Link flags: -pdb:NONE -debugtype:COFF
//   * COFF symbols in image
//     - MISC section should contain the image name
//   * COFF symbols in DBG file (if REBASE used to strip symbols)
//     - MISC section should contain the name of the DBG fle
//
//****************************************************************************
//
#include "a2nhdr.h"
#include "symlib.h"

//
// Externs (from a2n.c)
//
extern gv_t a2n_gv;                     // Globals


//
// Globals
//
XCOFF_FILE_HEADER *xfh;                 // Mapped file header

SECTION_INFO sec[MAX_SECTIONS];         // Executable section information
int scnt = 0;                           // Total number of sections
int xcnt = 0;                           // Number of executable sections

ULONG img_misc_debug_dir_ts = 0;        // TimeDataStamp for MISC debug directory entry

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
   return((uint)H_VERSION_NUMBER);
}




//
// ValidateKernelSymbols()
// ***********************
//
// Not needed in Windows but need to have it.
//
void ValidateKernelSymbols(void *lmn)
{
   return;
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
// Symbol harvesting rules for other modules:
// ******************************************
//
// 1) Try on-disk executable (given by module name)
//    - If it contains code, and code is requested, harvest it. Code is
//      only harvested from the on-disk executable.
//    - If it contains symbols harvest them.
//    - If it does not contain symbols go to step 2.
//
// 2) Give up.
//
int GetSymbolsForModule(void *handle,   // Really a pointer to a Module node
                        char *modname,  // Fully qualified Module name
                        char *searchpath,   // Search path
                        uint64 loadaddr,   // Load virtual address //STJ64
                        void *symbol_callback,   // Call-back function
                        void *section_callback)   // Call-back function
{
   CONTEXT_REC cr;
   int i, rc;
   MOD_NODE *mn;

// XCOFF_FILE_HEADER   *xfh;                // XCOFF File Header

   size_t filesize;                     // File size of image

   char idir[MAX_PATH_LEN];             // Image path (without filename)
   char ifn[MAX_FILENAME_LEN];          // Image filename (without path, without extension)
   char iext[16];                       // Image extension (without dot)
   char *c, *pc, *pname;

   int sec_cnt;                         // Total number of sections
   int executable_section;              // 1=section is executable

   dbghv(("> GetSymbolsForModule: handle=0x%p, name='%s', searchpath='%s' (%d)\n",
          handle, modname, searchpath, strlen(searchpath)));
   dbghv(("                       loadaddr=0x%p, symcallbak=0x%p, seccallback=0x%p\n",
          loadaddr, symbol_callback, section_callback));

   //
   // Context saved here and used by the real callback function
   //
   cr.handle     = handle;
   cr.load_addr  = loadaddr;
   cr.imgname    = modname;
   cr.searchpath = searchpath;
   cr.symtype    = 0;
   cr.hflags     = 0;

#if defined(_64BIT)
#pragma warning(disable : 4055)
// Win64 compiler bitches about casting the function pointers to the right types
#endif

   cr.symbol_callback  = (PFN_SYMBOL_CALLBACK)symbol_callback;
   cr.section_callback = (PFN_SECTION_CALLBACK)section_callback;

#if defined(_64BIT)
#pragma warning(default : 4055)
#endif

   mn = (MOD_NODE *)handle;

   //
   // Globals initialization
   //
   scnt = xcnt = 0;

   //
   // Make sure module exists and we can read it.
   // This is an absolute requirement!  We have to be able to read the image.
   //
   if (!FileIsReadable(modname))
   {
      errmsg(("*E* GetSymbolsForModule: unable to access '%s'\n",modname));
      rc = A2N_FILE_NOT_FOUND_OR_CANT_READ;
      goto TheEnd;
   }

   //
   // Map file so we can look at the header
   //
   xfh = (XCOFF_FILE_HEADER *)MapFile(modname, &filesize);

   if ( xfh == NULL )
   {
      errmsg(("*E* GetSymbolsForModule: unable to map file '%s'.\n",modname));
      rc = A2N_FILE_NOT_FOUND_OR_CANT_READ;
      goto TheEnd;
   }
   if (IsBadReadPtr((void *)xfh, sizeof(XCOFF_FILE_HEADER)))
   {
      errmsg(("*E* GetSymbolsForModule: XCOFF_FILE_HEADER for %s is not readable.\n",modname));
      rc = A2N_FILE_NOT_FOUND_OR_CANT_READ;
      goto TheEnd;
   }
   cr.mod_addr = (void *)xfh;

   //
   // Determine image type.
   //

   mn->type = ( xfh->Signature == XCOFF32_SIGNATURE )
              ? MODULE_TYPE_XCOFF32 : MODULE_TYPE_XCOFF64;

   //
   // Save the timestamp and checksum of the on-disk image in the context record
   //
   cr.img_ts = xfh->TimeDate;
   cr.img_cs = 0;

   //
   // Enumerate the image sections and gather code (for executable
   // sections only) if required. We want to gather code even if the
   // symbols don't come from the on-disk image.
   //
   sec_cnt = EnumerateSections(&cr);
   if (sec_cnt == 0)
   {
      errmsg(("*E* GetSymbolsForModule: module has no sections!\n"));
      rc = A2N_INVALID_MODULE_TYPE;
      goto TheEnd;
   }

   //
   // If the image contains XCOFF debug symbols then go read them ...
   //
   cr.symtype = (int)SYMBOL_TYPE_FUNCTION;

   dbghv(("- GetSymbolsForModule: attempting to get XCOFF symbols from image\n"));

   rc = HarvestXCOFFSymbols( xfh, &cr );

   if ( rc == 0 )
   {
      dbghv(("- GetSymbolsForModule: no XCOFF symbols or error.\n"));
      rc = A2N_NO_SYMBOLS;
   }
   else
   {
      rc = 0;
      mn->hvname = strdup(modname);     // Symbols came from here
   }

   TheEnd:
   //
   // Send back all the sections.
   // Waited until now because it is possible to harvest code *AFTER*
   // the sections are enumerated initially and we want to make sure
   // we send back sections with all the code we've gathered.
   // We may or may not have any symbols.
   //

   for (i = 0; i < scnt; i++)
   {
      if ( sec[i].flags & XCOFF_SECTION_CODE )
      {
         executable_section = 1;
      }
      else
      {
         executable_section = 0;
      }

      SendBackSection( sec[i].name,     // Section name
                       sec[i].number,   // Section number
                       sec[i].start_addr,   // Section start address/offset
                       sec[i].offset,   // Section offset
                       sec[i].size,     // Section size
                       sec[i].flags,    // Section flags
                       sec[i].loc_addr, // Where I have it loaded
                       executable_section,   // Executable section or not
                       &cr);            // Context record

   }

   UnmapFile( xfh );                    // Unmap image

   dbghv(("< GetSymbolsForModule rc=%d\n",rc));
   return(rc);
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
int SendBackSymbol( char *symbol_name,  // Symbol name
                    uint symbol_offset, // Symbol offset
                    uint symbol_size,   // Symbol size
                    int  symbol_type,   // Symbol type (label, export, etc.)
                    int  section,       // Section to which the symbol belongs
                    char *code,         // Code associated with this symbol
                    void *context)      // Context record
{
   SYMBOL_REC sr;
   CONTEXT_REC *cr = (CONTEXT_REC *)context;
   PFN_SYMBOL_CALLBACK UserSymbolCallBack = cr->symbol_callback;
   int rc = 0;

   if (UserSymbolCallBack != NULL)
   {
      sr.name    = symbol_name;
      sr.offset  = symbol_offset;
      sr.length  = symbol_size;
      sr.type    = symbol_type;
      sr.section = section;
      sr.code    = code;
      sr.handle  = cr->handle;
      rc = UserSymbolCallBack(&sr);     // Call user callback function
   }
   return(rc);
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
static int SendBackSection(char *section_name,   // Section name
                           int section_number,   // Section number
                           uint64 start_addr,   // Section start address/offset //STJ64
                           uint section_offset,   // Section offset
                           uint section_size,   // Section size
                           uint section_flags,   // Section flags
                           void *loc_addr,   // Where I have it loaded
                           int executable,   // Executable section or not
                           void *context)   // Context record
{
   SECTION_REC sr;
   CONTEXT_REC *cr = (CONTEXT_REC *)context;
   PFN_SECTION_CALLBACK UserSectionCallBack = cr->section_callback;
   char temp_name[9];
   int rc = 0;

   if (UserSectionCallBack != NULL)
   {
      if (section_name[7] != '\0')
      {
         //
         // Handle section names that are exactly 8 characters
         //
         memcpy(temp_name, section_name, 8);
         temp_name[8] = 0;              // Section name was exactly 8 bytes
         sr.name = temp_name;
      }
      else
      {
         sr.name = section_name;
      }

      sr.number     = section_number;
      sr.executable = executable;
      sr.start_addr = start_addr;
      sr.offset     = section_offset;
      sr.size       = section_size;
      sr.flags      = section_flags;
      sr.loc_addr   = loc_addr;
      sr.handle     = cr->handle;
      rc = UserSectionCallBack(&sr);    // Call user callback function
   }
   return(rc);
}




//
// EnumerateSections()
// *******************
//
// Builds the global "sec" array and sets globals "scnt" and "xcnt"
//
// Returns: number of sections if successfull
//          0 otherwise
//
static int EnumerateSections(CONTEXT_REC *cr)
{
//   XCOFF_FILE_HEADER    *xfh;               // XCOFF file header
   XCOFF_SECTION_HEADER *xsh;           // XCOFF section headers
   int i;


   dbghv(("> EnumerateSections: xfh=0x%p, cr=0x%p\n",xfh,cr));
   //
   // Set up addressability to the various headers
   //
//   xfh = (XCOFF_FILE_HEADER *)cr->mod_addr; // XCOFF file header
   xsh = (XCOFF_SECTION_HEADER *)( (char *)xfh   // Immediately after Aus Header
                                   + sizeof(XCOFF_FILE_HEADER)
                                   + xfh->sizeAuxHeader );

   //
   // Make sure we can handle all the sections in this image
   //
   if ( xfh->numSections > MAX_SECTIONS)
   {
      errmsg(("*E* EnumerateSections: ***** INTERNAL ERROR: %d > section table dimemsion of %d!\n",
              xfh->numSections, MAX_SECTIONS));
      return(0);                        // No sections
   }

   //
   // Make sure the SECTION_HEADERs are accessible for all sections
   //
   if (IsBadReadPtr((void *)xsh, (sizeof(XCOFF_SECTION_HEADER) * xfh->numSections)))
   {
      errmsg(("*E* EnumerateSections: XCOFF_SECTION_HEADERs for %d sections is not readable.\n",xfh->numSections));
      scnt = 0;                         // Force 0 sections!
      return(0);
   }
   DumpSectionHeaders( xsh, xfh->numSections );

   //
   // Now enumerate the sections
   // - Section numbers start at 1
   // - Section name may or may not be NULL terminated
   // - Section name is limited to at most 8 characters
   //
   scnt = xcnt = 0;                     // Nothing yet
   for (i = 0; i < xfh->numSections; i++)
   {
      scnt++;                           // One more section

      //
      // Build section array entry
      //
      __try
      {
         sec[i].name        = (char *)&(xsh[i].Name);
         sec[i].number      = i + 1;
         sec[i].start_addr  = cr->load_addr + xsh[i].VirtAddr;   //STJ64
         sec[i].offset      = xsh[i].VirtAddr;
         sec[i].size        = xsh[i].Size;
         sec[i].end_addr    = sec[i].start_addr + sec[i].size - 1;   //STJ64
         sec[i].file_offset = xsh[i].offRawData;
         sec[i].flags       = xsh[i].Flags;
         sec[i].loc_addr    = NULL;
         sec[i].symcnt      = 0;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         // Handle access violations
         msg_log("\n**ERROR** ##### EXCEPTION 0x%X in EnumerateSections sec=%d#####\n",_exception_code(),i);
         scnt = 0;                      // Force 0 sections!
         return(0);
      }

      if ( xsh[i].Flags & XCOFF_SECTION_CODE )
      {
         //
         // Executable section.
         // These are sections that contain executable code. As such, we
         // always gather code if requested to do so.
         //
         GatherCodeForSection(scnt);
         xcnt++;                        // One more executable section
      }
      else
      {
         //
         // Non-executable section.
         // These are sections that *MAY* contain code. There are times
         // (like in the java jvm.dll and jitc.dll, for example) where
         // sections not marked executable *DO* contain code. Don't know why
         // they do this but they do it.
         // If we come across a symbol in one of them we will save the
         // code later.
         //
      }
   }

   //
   // Done
   //
   dbghv(("< EnumerateSections: scnt=%d  xcnt=%d\n",scnt,xcnt));
   return(scnt);                        // Total number of sections
}                                       // EnumerateSections()




//
// GatherCodeForSection()
// **********************
//
// Given a section number gather code (actually whatever data the
// section contains).
//
static void GatherCodeForSection(int secnum)
{
   void *sec_data;                      // Pointer to section data
   int   xsecnum = secnum - 1;

   dbghv(("> GatherCodeForSection(%d)\n",secnum));
   if (!G(gather_code))
   {
      dbghv(("< GatherCodeForSection: not gathering code.\n"));
      return;                           // They don't want code
   }

   if (sec[xsecnum].loc_addr != NULL)
   {
      dbghv(("< GatherCodeForSection: have code already.\n"));
      return;                           // Already have code
   }

   sec[xsecnum].loc_addr = malloc(sec[xsecnum].size);
   dbghv(("- GatherCodeForSection: malloc(%d (0x%08X)) bytes for code at 0x%p (NULL is bad)\n",
          sec[xsecnum].size, sec[xsecnum].size, sec[xsecnum].loc_addr));

   if (sec[xsecnum].loc_addr == NULL)
   {
      G(gather_code) = 0;               // Don't try anymore
      dbghv(("- GatherCodeForSection: ***** Code gathering has been turned off *****\n"));
   }
   else
   {
      __try {
         sec_data = (void *)PtrAdd(xfh, sec[xsecnum].file_offset);
         dbghv(("- GatherCodeForSection: saving %d bytes of code from 0x%p to 0x%p for section %d\n",
                sec[xsecnum].size, sec_data, sec[xsecnum].loc_addr, secnum));
         memcpy(sec[xsecnum].loc_addr, sec_data, sec[xsecnum].size);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         // Handle access violations
         msg_log("\n**ERROR** ##### EXCEPTION 0x%X in GatherCodeForSection secnum=%d #####\n",_exception_code(),secnum);
         return;
      }
   }

   dbghv(("< GatherCodeForSection: done\n"));
   return;
}




//
// FindSectionForAddress()
// ***********************
//
// Returns a section number or 0 if address isn't in any section.
//
int FindSectionForAddress( uint64 addr )   //STJ64
{
   int i;

   if (addr == NULL)
      return(0);                        // Address can't be zero

   for (i = 0; i < scnt; i++)
   {
      if (addr >= sec[i].start_addr && addr <= sec[i].end_addr)
         return(i+1);
   }
   return(0);
}




//
// FindSectionForOffset()
// **********************
//
// Returns a section number or 0 if address isn't in any section.
//
int FindSectionForOffset(uint offset)
{
   int xsecnum;
   uint sec_end;

   for (xsecnum = 0; xsecnum < scnt; xsecnum++)
   {
      sec_end = sec[xsecnum].offset + sec[xsecnum].size;
      if (offset >= sec[xsecnum].offset && offset <= sec_end)
         return(xsecnum+1);
   }
   return(0);
}




//
// SectionIsExecutable()
// *********************
//
// Returns TRUE if section number is for an executable section
// and FALSE otherwise.
//
bool_t SectionIsExecutable(int secnum)
{
   if (secnum <= 0  ||  secnum > scnt)
   {
      return(FALSE);
   }
   else if (sec[secnum-1].flags & XCOFF_SECTION_CODE)
   {
      return(TRUE);
   }
   return(FALSE);
}




char UndecoratedName[1024];             // Global and hardcoded!
//
// DemangleName()
// **************
//
// Attempts to demangle (or undecorate) a symbol name.
// Returns pointer to undecorated name (if it was decorated)
// or to the original name (if it wasn't decorated) or if
// there was an error.
// UnDecorateSymbolName() doesn't do a real good job of removing
// calling convention decoration, nor of removing the leading
// underscore/dot added by the compiler/linker.
//
char *DemangleName(char *name)
{
// uint udlen;
//
// if (name[0] == '?')
// {
//    if (G(demangle_complete))
//    {
//       udlen = UnDecorateSymbolName(name, UndecoratedName,
//                                    sizeof(UndecoratedName),
//                                    (UNDNAME_COMPLETE | UNDNAME_32_BIT_DECODE));
//    }
//    else
//    {
//       udlen = UnDecorateSymbolName(name, UndecoratedName,
//                                    sizeof(UndecoratedName),
//                                    (UNDNAME_NAME_ONLY | UNDNAME_32_BIT_DECODE));
//    }
//
//    if (udlen != 0)
//    {
//       dbghv(("- DemangleName: undecorated=%s\n",UndecoratedName));
//       return(UndecoratedName);          // Undecorated name
//    }
//    else
//    {
//       dbghv(("- DemangleName: UnDecorateSymbolName() failed. rc=%d\n",GetLastError()));
//    }
// }

   return(name);                        // Not decorated or error
}




//
// GetModuleType()
// ***************
//
// Determines the type of a given module.
// Returns the module type (or MODULE_TYPE_INVALID if it can't determine type).
//
static int GetModuleType( XCOFF_FILE_HEADER *xfh )
{
   int rc;

   dbghv(("> GetModuleType: xfh=0x%p\n",xfh));

   if ( xfh->Signature == XCOFF32_SIGNATURE)   // 0x1DF
   {
      rc = MODULE_TYPE_XCOFF32;
      dbghv(("- GetModuleType: XCOFF32 signature found\n"));
   }
   else if ( xfh->Signature == XCOFF64_SIGNATURE)   // 0x1EF
   {
      rc = MODULE_TYPE_XCOFF64;
      dbghv(("- GetModuleType: XCOFF64 signature found\n"));
   }
   else
   {
      rc = MODULE_TYPE_INVALID;
      errmsg(("*E* GetModuleType: unknown module signature 0x%04X\n", xfh->Signature));
   }

   //
   // Done
   //
   dbghv(("< GetModuleType: rc=%d\n",rc));
   return(rc);
}




//
// GetCodePtrForSymbol()
// *********************
//
char *GetCodePtrForSymbol(uint symoffset, int secnum)
{
   char *cp;
   int   xsecnum = secnum - 1;

   if (sec[xsecnum].symcnt == 1)
      GatherCodeForSection(secnum);     // Gather code on 1st hit

   if (sec[xsecnum].loc_addr == NULL)
      return(NULL);                     // No code

   cp = (char *)PtrAdd(sec[xsecnum].loc_addr, (symoffset - sec[xsecnum].offset));
   dbghv(("code_ptr = 0x%p  [%p + (0x%08x - %p)], sec#=%d\n",
          cp,(char *)sec[xsecnum].loc_addr,symoffset,sec[xsecnum].offset,secnum));

   return(cp);
}

//
// GetSymlibSymbol()
// *****************
//

int GetSymlibSymbol( unsigned int pid,  // Process id
                     uint64       addr, // Address //STJ64
                     SYMDATA     *sd)   // Pointer to SYMDATA structure
{
   int                rc      = 0;
   uint64_t           ull;
   symentry          *se;

   dbghv(("\n>> GetSymlibSymbol: **ENTRY** pid=0x%08X, addr=%"_LZ64X", sd=%p\n",pid,addr,sd));
   rc = A2N_NO_SYMBOL_FOUND;

   se = syml_binsym_lookup( addr,
                            (uint64)pid,
                            &ull,
                            LKUP_FUNCTION );
   if ( se )
   {
      rc = A2N_SUCCESS;
      //
      // Process Information (set by A2nGetSymbol)
      //
//    sd->owning_pid_name = se->binary; // Name of "owning" process
//    sd->owning_pid = pid;             // Process id of "owning" process
      //
      // Module Information
      //
      sd->mod_name   = se->binary;      // Name of module
      sd->mod_addr   = ull;             // Module starting (load) virtual address //STJ64
      sd->mod_length = 0;               // Module length (in bytes)
      //
      // Symbol Information
      //
      sd->sym_name   = se->name;        // Symbol name
      sd->sym_addr   = se->addr + ull;  // Symbol starting virtual address //STJ64
      sd->sym_length = se->size;        // Symbol length (in bytes)
      //
      // Symbol Code Information
      //
      if ( sizeof(symentry) < 64 )      //STJ/LS
      {
         sd->code    = 0;               // Code stream at given address
      }
      else
      {
         sd->code    = (char *)se->instructions;   // Code stream at given address

         sd->code    = sd->code + ( addr - sd->sym_addr );   // Adjust to match A2nGetSymbol
      }

//    dbghv(("\tpid_name   = %s\n",sd->owning_pid_name));
//    dbghv(("\towning_pid = %X\n",sd->owning_pid));
//    dbghv(("\tmod_name   = %s\n",sd->mod_name));
//    dbghv(("\tmod_addr   = %X\n",sd->mod_addr));
//    dbghv(("\tmod_length = %X\n",sd->mod_length));
//    dbghv(("\tsym_name   = %s\n",sd->sym_name));
//    dbghv(("\tsym_addr   = %X\n",sd->sym_addr));
//    dbghv(("\tsym_length = %X\n",sd->sym_length));
//    dbghv(("\tcode       = %X\n",sd->code));
   }
   dbghv(("<< GetSymLibSymbol: rc=%d se=%p sym='%s' mod='%s'\n",rc, se, sd->sym_name, sd->mod_name));
   return( rc );
}

void InitSymlibSymbols()
{
   static int fLoaded = 0;
   int        rc;

   dbghv(("\n>> InitSymlibSymbols: **ENTRY**\n"));

   if ( 0 == fLoaded )
   {
      fprintf(stderr, " Reading symbols from gensyms.out\n\n" );

      rc = syml_symbols_from_file( "gensyms.out" );

//    fprintf(stderr, "syml_symbols_from_file(gensyms.out): rc = %d\n", rc );

      dbghv(("- syml_symbols_from_file(), rc = %d\n",rc));

      fLoaded = 1;
   }
   dbghv(("\n>> InitSymlibSymbols: **EXIT**\n"));
}
