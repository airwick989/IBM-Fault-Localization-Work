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

#include "a2nhdr.h"


#define SUCCESS           0
#define FAILURE           1

//
// Calculate required amount to pad to 8-byte boundary
//
#define CALC_PAD(n, p)                 \
   if (n & 0x00000007)                 \
      p = 8 - (n & 0x00000007);        \
   else                                \
      p = 0;


//
// Externals (from a2n.c)
//
extern gv_t a2n_gv;                         // Globals
#if defined(_WINDOWS)
extern char dev_name[MAX_DEVICES][128];     // Valid sym links for DOS devices
extern char * drv_name[MAX_DEVICES];        // DOS drive letters
extern char * SystemRoot;                   // Windows System Directory
extern char * SystemDrive;                  // Windows System Drive
extern char * WinSysDir;                    // Windows System Directory (without drive letter)
#endif


//
// Internal data structures
//
// These control blocks contain the information saved in the MSI file
// for modules, sections and symbols.
//
typedef struct module_info {
   uint slen;                               // Length including all strings
   int  name_len;                           // Length of name (including terminating NULL)
   int  hvname_len;                         // Length of hvname (including terminating NULL)
   int  orgname_len;                        // Length of orgname (including terminating NULL)
   uint flags;                              // Node signature and flags
   uint ts;                                 // Timestamp (if available)
   uint chksum;                             // Checksum (if available)
   uint name_hash;                          // Hash value for name
   int  type;                               // Module type (of file from where symbols are harvested)
   int  symcnt;                             // Total number of symbols (no aliases/containes)
   int  seccnt;                             // Number of sections
   int  ne_seccnt;                          // Non-executable sections
   int  codesize;                           // Total amount of code harvested
   int  alignment_8byte;
} MOD_INFO;


typedef struct section_info {
   uint slen;                               // Length including all strings
   int  name_len;                           // Length of name (including terminating NULL)
   uint flags;                              // Node signature and flags
   int  alignment_8byte;
   uint64 start_addr;                       // Section starting virtual address/offset
   uint64 end_addr;                         // Section ending address/offset
   uint offset_start;                       // Starting offset
   uint offset_end;                         // Ending offset
   uint size;                               // Section size (in bytes)
   int  number;                             // Section number in module
   uint sec_flags;                          // Section flags
   int  codesize;                           // Size of code for section
} SEC_INFO;


typedef struct symbol_info {
   uint slen;                               // Length including all strings
   int  name_len;                           // Length of name (including terminating NULL)
   uint flags;                              // Node signature and flags
   int  codesize;                           // Size of attached code
   uint offset_start;                       // Offset from beginning of module
   uint offset_end;                         // Offset of end of symbol
   uint length;                             // Length (in bytes)
   uint type;                               // Symbol type
   uint sym_flags;                          // Symbol flags
   int  section;                            // Section number
   uint name_hash;                          // Hash value for name
   int  alignment_8byte;
} SYM_INFO;



//
// Globals ...
//
char * msi_base = NULL, * msi_end = NULL, * msi_cur = NULL, * os_str = NULL;
size_t msi_size = 0;
int msi_big_endian = -1;
int msi_word_size = -1;


//
// Internal prototypes
//
static int WriteModule(FILE * fh, MOD_NODE * mn);
static MOD_NODE * RecreateModule(void);
static int WriteSection(FILE * fh, SEC_NODE * cn);
static int RecreateSections(MOD_NODE * mn);
static SEC_NODE * InitialzeSectionNode(SEC_INFO * si, MOD_NODE * mn, int executable);
static int WriteSymbol(FILE * fh, SYM_NODE * sn);
static int InitialzeSymbolNode(SYM_INFO * si, SYM_NODE * sn, MOD_NODE * mn);
static void CalculateCodePtr(MOD_NODE * mn, SYM_NODE * sn);
static int RecreateSymbols(MOD_NODE * mn);
static int WriteHeader(FILE * fh);
static int ReadHeader(void);
static uint cvt_32bits(uint ul);
//#if defined(_64BIT)
//static uint64 cvt_64bits(uint64 ull);
//#endif



//***************************************************************************//
//                                                                           //
// * 8-bytes endianness/word size string                                     //
//                                                                           //
// * 4-bytes system pid                                                      //
//                                                                           //
// * 4-bytes number of string pairs                                          //
//                                                                           //
// * string pairs (see below)                                                //
//                                                                           //
// For each module the MSI (Module/Symbol Information) file looks like:      //
//                                                                           //
// * 1 MOD_INFO structure (aligned on an 8-byte boundary)                    //
//   - followed by the module name string                                    //
//   - followed by the module hvname string (if present)                     //
//                                                                           //
// * 0 or MOD_INFO.seccnt SEC_INFO structures, each followed by:             //
//   - section name string                                                   //
//   - SEC_INFO.codesize bytes of code                                       //
//   (**** Executable sections followed by non-executable sections ****)     //
//                                                                           //
// * 0 or MOD_INFO.symcnt SYM_INFO structures, each followed by:             //
//   - symbol name string                                                    //
//   - SYM_INFO.codesize bytes of code                                       //
//                                                                           //
//***************************************************************************//
//                                                                           //
// String pairs are used to save system-specific data in a free-form format. //
// Each pair consists of a "name" string and a "value" string. The number    //
// of name/value pairs is given at offset 12 of the file. Any string that    //
// is unknown to the parser should be ignored.                               //
//                                                                           //
//  Name       Value                           Description                   //
//  OS         "Windows" or "Linux"            OS on which file generated    //
//  psep       "\" on Windows, "/" on Linux)   path separator                //
//  spsep      ";" on Windows, ":" on Linux)   search path separator         //
//  SysRoot    system-specific                 Value of %SystemRoot          //
//  SysDrv     system-specific                 Value of %SystemDrive%        //
//  X:         drive letter followed by ":"    Drive                         //
//  dev        device to which drv is mapped   Device string                 //
//***************************************************************************//
//                                                                           //
//  8-byte endiannes/word size string                                        //
//  4-bytes system pid                                                       //
//  4-bytes number of string pairs                                           //
//  string pairs                                                             //
//                                                                           //
//  MOD_INFO        -----  Always present                                    //
//  module_name     -----  Always present                                    //
//                                                                           //
//    SEC_INFO       -+---  0 or (MOD_INFO.seccnt-ne_seccnt) of these        //
//    section_name       -+     (executable sections)                        //
//    <code>             -+                                                  //
//                                                                           //
//    SEC_INFO       -+---  0 or MOD_INFO.ne_seccnt of these                 //
//    section_name       -+     (non-executable sections)                    //
//    <code>             -+                                                  //
//                                                                           //
//    SYM_INFO        -+---  0 or MOD_INFO.base_symcnt of these              //
//    symbol_name        -+                                                  //
//    <code>             -+                                                  //
//                                                                           //
//***************************************************************************//



//
// CreateMsiFile()
// ***************
//
// Walks module/section/symbol chains and creates an msi file
//
int CreateMsiFile(char * filename)
{
   FILE * fh;
   MOD_NODE * mn;
   SEC_NODE * cn;
   int i, rc;


   dbgmsg(("> CreateMsiFile: fn='%s'\n", filename));
   //
   // Get rid of existing file, open the new one and seek to the beginning
   //
   remove(filename);
   fh = fopen(filename, "wb");
   if (fh == NULL) {
      errmsg(("*E* CreateMsiFile: unable to open '%s'\n", filename));
      return (A2N_CREATEMSIFILE_ERROR);
   }
   fseek(fh, 0, SEEK_SET);                  // Seek to 0

   //
   // Write source machine's endianness and word size
   //
   rc = WriteHeader(fh);
   if (rc == FAILURE) {
      errmsg(("*E* CreateMsiFile: unable to write MSI file header\n"));
      rc = A2N_CREATEMSIFILE_ERROR;
      goto TheEnd;
   }

   //
   // Traverse module cache and dump it to disk
   //
   mn = G(ModRoot);
   while (mn != NULL) {
      //
      // Write module header
      //
      dbgmsg(("- CreateMsiFile: writing %s\n", mn->name));
      rc = WriteModule(fh, mn);
      if (rc == FAILURE) {
         errmsg(("*E* CreateMsiFile: unable to write module\n"));
         rc = A2N_CREATEMSIFILE_ERROR;
         goto TheEnd;
      }

      //
      // Write sections and code (if any)
      //
      dbgmsg(("- CreateMsiFile: writing executable sections\n"));
      cn = mn->secroot;
      while (cn) {
         rc = WriteSection(fh, cn);
         if (rc == FAILURE) {
            errmsg(("*E* CreateMsiFile: unable to write section(s)\n"));
            rc = A2N_CREATEMSIFILE_ERROR;
            goto TheEnd;
         }
         cn = cn->next;
      }
      dbgmsg(("- CreateMsiFile: writing non-executable sections\n"));
      cn = mn->ne_secroot;
      while (cn) {
         rc = WriteSection(fh, cn);
         if (rc == FAILURE) {
            errmsg(("*E* CreateMsiFile: unable to write section(s)\n"));
            rc = A2N_CREATEMSIFILE_ERROR;
            goto TheEnd;
         }
         cn = cn->next;
      }

      //
      // Write symbols (if any)
      //
      dbgmsg(("- CreateMsiFile: writing symbols\n"));
      if (mn->flags & A2N_SYMBOL_ARRAY_BUILT) {
         for (i = 0; i < mn->base_symcnt; i++) {
            rc = WriteSymbol(fh, mn->symroot[i]);
            if (rc == FAILURE) {
               errmsg(("*E* CreateMsiFile: unable to write symbol(s)\n"));
               rc = A2N_CREATEMSIFILE_ERROR;
               goto TheEnd;
            }
         }
      }

      //
      // On to the next module ...
      //
      mn = mn->next;
   }

   rc = 0;                                  // Everything worked if we made it here

   //
   // Done
   //
TheEnd:
   fflush(fh);
   fclose(fh);
   dbgmsg(("< CreateMsiFile: done\n"));
   return (rc);
} // CreateMsiFile()


//
// ReadMsiFile()
// *************
//
// Reads an msi file and recreates the module/section/symbol structure
//
int ReadMsiFile(char * filename)
{
   MOD_NODE * mn;
   int rc;


   dbgmsg(("> ReadMsiFile\n"));
   G(kernel_not_seen) = 1;                  // until we see it ...

   //
   // Map the MSI file
   //
   msi_base = (char *)MapFile(filename, &msi_size);
   if (msi_base == NULL) {
      errmsg(("*E* ReadMsiFile: unable to map msi file.\n"));
      rc = A2N_READMSIFILE_ERROR;
      goto TheEnd;
   }
   if (msi_size > 0x7fffffff) {
      errmsg(("*E* ReadMsiFile: msi file too big (>=2GB).\n"));
      rc = A2N_READMSIFILE_ERROR;
      goto TheEnd;
   }
   msi_cur = msi_base;
   msi_end = msi_base + (uint)msi_size - 1;
   dbgmsg(("- ReadMsiFile: msi_base=%p  msi_size=%d  msi_end=%p\n", msi_base, msi_size, msi_end));

   //
   // Read source system's endiannes and word size
   //
   rc = ReadHeader();
   if (rc == -1) {
      errmsg(("*E* ReadMsiFile: error(s) reading MSI file header.\n"));
      rc = A2N_READMSIFILE_ERROR;
      goto TheEnd;
   }
   if (G(word_size) != msi_word_size) {
      errmsg(("*E* ReadMsiFile: MSI file word size does not match this machine's word size\n"));
      rc = A2N_READMSIFILE_ERROR;
      goto TheEnd;
   }
   if (msi_big_endian) {
      infomsg(("*I* ReadMsiFile: MSI file is big_endian\n"));
   }
   else {
      infomsg(("*I* ReadMsiFile: MSI file is little_endian\n"));
   }
   if (os_str) {
      infomsg(("*I* ReadMsiFile: MSI file was written on a %s machine\n", os_str));
   }

   //
   // Walk the file and recreate node structure
   //
   while (msi_cur < msi_end) {
      //
      // Add module node to module cache
      //
      mn = RecreateModule();
      if (mn == NULL) {
         errmsg(("*E* ReadMsiFile: unable to recreate MOD_NODE\n"));
         rc = A2N_READMSIFILE_ERROR;
         goto TheEnd;
      }
      if (mn->flags & A2N_FLAGS_KERNEL)
         G(kernel_not_seen) = 0;            // just saw it

      //
      // Add sections and code (if any) to the module
      //
      if (mn->seccnt) {
         rc = RecreateSections(mn);
         if (rc == FAILURE) {
            errmsg(("*E* ReadMsiFile: unable to recreate SEC_NODE(s)\n"));
            rc = A2N_READMSIFILE_ERROR;
            goto TheEnd;
         }
      }
      else {
         dbgmsg(("- ReadMsiFile: no sections\n"));
      }

      //
      // Add symbols (if any) to the module
      //
      if (mn->base_symcnt) {
         rc = RecreateSymbols(mn);
         if (rc == FAILURE) {
            errmsg(("*E* ReadMsiFile: unable to recreate SYM_NODE(s)\n"));
            rc = A2N_READMSIFILE_ERROR;
            goto TheEnd;
         }
      }
      else {
         dbgmsg(("- ReadMsiFile: no symbols\n"));
      }
   }

   rc = 0;                                  // Success if we make it this far

   //
   // Done
   //
TheEnd:
   if (msi_base)
      UnmapFile(msi_base);

   dbgmsg(("< ReadMsiFile: done\n"));
   return (rc);
} // ReadMsiFile()




//
// Strings stored in the header.
// Lengths include the terminating NULL character
//
#define OS_STR              "OS"            // Operating system
#define OS_STRLEN           3

#define WINDOWS_STR         "Windows"
#define WINDOWS_STRLEN      8

#define LINUX_STR           "Linux"
#define LINUX_STRLEN        6

#define PSEP_STR            "psep"          // Path separator
#define PSEP_STRLEN         5

#define SPSEP_STR           "spsep"         // Searchpath separator
#define SPSEP_STRLEN        6

#define SYSTEMROOT_STR      "SysRoot"       // Windows system directory
#define SYSTEMROOT_STRLEN   8

#define SYSTEMDRIVE_STR     "SysDrv"        // Windows system drive
#define SYSTEMDRIVE_STRLEN  7


//
// WriteHeader()
// *************
//
// 1) Write 8-byte string to identify the endianness and machine word
//    width of the machine where the MSI file is being created.
//    No matter how we write/read it:
//    - byte[0] is always "L" (little) or "B" (big) endian
//    - byte[1] is always "3" (32) or "6" (64) bits
//
// 2) Write 4-byte system pid (in machine's endiannes)
//
// 3) Write 4-byte number of string pairs (in machine's endiannes)
//
// 4) Write the correct number of string pairs
//
// 5) Pad to 8-byte boundary if necessary
//
//
// String pairs are used to save system-specific data in a free-form format.
// Each pair consists of a "name" string and a "value" string. The number
// of name/value pairs is given at offset 12 of the file. Any string that
// is unknown to the parser should be ignored.
//
//  Name       Value                           Description
//  -------    -----------------------------   --------------------------
//  OS         "Windows" or "Linux"            OS on which file generated
//  psep       "\" on Windows, "/" on Linux)   path separator
//  spsep      ";" on Windows, ":" on Linux)   search path separator
//  SysRoot    system-specific                 Value of %SystemRoot
//  SysDrv     system-specific                 Value of %SystemDrive%
//  X:         device to which X: is mapped    X is any letter
//
static int WriteHeader(FILE * fh)
{
   int pad;
   int num_str = 0, str_size = 0;
#if defined(_WINDOWS)
   int i, drv_cnt = 0;
#endif

   //
   // Write machines endiannes and word size
   //
#if defined(_PPC32)
   if (!FileWrite(fh, "B33BB33B", 8))       // Big endian 32 bits
#elif defined(_PPC64)
   if (!FileWrite(fh, "B66BB66B", 8))       // Big endian 64 bits
#elif defined (_S390X)
   if (!FileWrite(fh, "B66BB66B", 8))       // Big endian 64 bits
#elif defined (_S390)
   if (!FileWrite(fh, "B33BB33B", 8))       // Big endian 32 bits
#elif defined(_X86)
   if (!FileWrite(fh, "L33LL33L", 8))       // Little endian 32 bits
#elif defined (_IA64)
   if (!FileWrite(fh, "L66LL66L", 8))       // Little endian 64 bits
#else
   if (!FileWrite(fh, "????????", 8))       // Don't know (bad signature!)
#endif
      return (FAILURE);

   //
   // Write 4-byte system pid
   //
   if (!FileWrite(fh, (char *)&G(SystemPid), 4))     // write system pid
      return (FAILURE);

   //
   // Write 4-byte number of string pairs (in machine's endiannes)
   //
   num_str = 3;                             // OS, psep and spsep (always present)

#if defined(_WINDOWS)
   for (i = 0; i < 26; i++) {
      if (dev_name[i][0] != 0) {
         drv_cnt++;
      }
   }

   num_str += drv_cnt +                     // drive letter/device
              2;                            // SysRoot and SysDrv
#endif

   if (!FileWrite(fh, (char *)&num_str, 4))          // write number of strings
      return (FAILURE);

   //
   // Write the strings
   //
   if (!FileWrite(fh, OS_STR, OS_STRLEN))            // write OS
      return (FAILURE);

#if defined(_WINDOWS)
   if (!FileWrite(fh, WINDOWS_STR, WINDOWS_STRLEN))
      return (FAILURE);

   str_size += OS_STRLEN + WINDOWS_STRLEN;
#else
   if (!FileWrite(fh, LINUX_STR, LINUX_STRLEN))
      return (FAILURE);

   str_size += OS_STRLEN + LINUX_STRLEN;
#endif

   if (!FileWrite(fh, PSEP_STR, PSEP_STRLEN))        // write psep
      return (FAILURE);
   if (!FileWrite(fh, G(PathSeparator_str), 2))
      return (FAILURE);

   str_size += PSEP_STRLEN + 2;

   if (!FileWrite(fh, SPSEP_STR, SPSEP_STRLEN))      // write spsep
      return (FAILURE);
   if (!FileWrite(fh, G(SearchPathSeparator_str), 2))
      return (FAILURE);

   str_size += SPSEP_STRLEN + 2;

#if defined(_WINDOWS)
   if (!FileWrite(fh, SYSTEMROOT_STR, SYSTEMROOT_STRLEN))    // write SysRoot
      return (FAILURE);
   if (!FileWrite(fh, SystemRoot, strlen(SystemRoot)+1))
      return (FAILURE);

   str_size += SYSTEMROOT_STRLEN + (int)strlen(SystemRoot) + 1;

   if (!FileWrite(fh, SYSTEMDRIVE_STR, SYSTEMDRIVE_STRLEN))  // write SysDrv
      return (FAILURE);
   if (!FileWrite(fh, SystemDrive, strlen(SystemDrive)+1))
      return (FAILURE);

   str_size += SYSTEMDRIVE_STRLEN + (int)strlen(SystemDrive) + 1;

   for (i = 0; i < 26; i++) {
      if (dev_name[i][0] != 0) {
         if (!FileWrite(fh, drv_name[i], 3))         // write drive letter
            return (FAILURE);
         if (!FileWrite(fh, dev_name[i], strlen(dev_name[i])+1))  // write device name
            return (FAILURE);

         str_size += 3 + (int)strlen(dev_name[i]) + 1;
      }
   }
#endif

   CALC_PAD(str_size, pad);
   if (pad) {
      if (!FileWrite(fh, "########", pad))          // pad to 8-byte boundary if needed
         return (FAILURE);
   }

   return (SUCCESS);
}


//
// ReadHeader()
// ************
//
static int ReadHeader(void)
{
   int i, pad;
   int num_str = 0;
   int windows = 0;
   char * beg = msi_cur;
   uint header_size;
#if defined(_WINDOWS)
   int drv;
#endif

   dbgmsg(("> ReadHeader: c[0]=%c  c[1]=%c\n", msi_cur[0], msi_cur[1]));
   //
   // Pick up endianness
   //
   if (msi_cur[0] == 'B')
      msi_big_endian = 1;
   else if (msi_cur[0] == 'L')
      msi_big_endian = 0;
   else
      return (-1);                          // Incorrect endiannes indicator

   //
   // Pick up word size
   //
   if (msi_cur[1] == '3')
      msi_word_size = 32;
   else if (msi_cur[1] == '6')
      msi_word_size = 64;
   else
      return (-1);                          // Incorrect word size indicator

   dbgmsg(("- ReadHeader: machine is:  word_size: %d  big_endian: %d\n", G(word_size), G(big_endian)));
   dbgmsg(("- ReadHeader: msi_word_size: %d  msi_big_endian: %d\n", msi_word_size, msi_big_endian));
   msi_cur += 8;                            // advance file pointer

   //
   // Read 4-byte system pid
   //
   G(SystemPid) = cvt_32bits(*(uint *)msi_cur);
   dbgmsg(("- ReadHeader: SystemPid = %d\n", G(SystemPid)));
   msi_cur += 4;                            // advance file pointer

   //
   // Read 4-byte number of string pairs (in machine's endiannes)
   //
   num_str = cvt_32bits(*(int *)msi_cur);
   dbgmsg(("- ReadHeader: num_str = %d\n", num_str));
   msi_cur += 4;                            // advance file pointer

   if (num_str < 3) {
      dbgmsg(("< ReadHeader: there are %s strings. Expecting at least 3\n", num_str));
      return (-1);
   }

   //
   // Read the strings.  Skip the ones we don't understand.
   // We have defaults for everything so it's OK to skip.
   //
   for (i = 0; i < num_str; i++) {
      if (strcmp(msi_cur, OS_STR) == 0) {
         msi_cur += OS_STRLEN;

         os_str = msi_cur;
         msi_cur += strlen(msi_cur) + 1;
         if (strcmp(os_str, WINDOWS_STR) == 0)
            windows = 1;

         dbgmsg(("- ReadHeader: OS = '%s'\n", os_str));
         continue;
      }

      if (strcmp(msi_cur, PSEP_STR) == 0) {
         msi_cur += PSEP_STRLEN;

         G(PathSeparator_str) = zstrdup(msi_cur);
         msi_cur += strlen(msi_cur) + 1;

         dbgmsg(("- ReadHeader: PSEP = '%s'\n", G(PathSeparator_str)));
         continue;
      }

      if (strcmp(msi_cur, SPSEP_STR) == 0) {
         msi_cur += SPSEP_STRLEN;

         G(SearchPathSeparator_str) = zstrdup(msi_cur);
         msi_cur += strlen(msi_cur) + 1;

         dbgmsg(("- ReadHeader: SPSEP = '%s'\n", G(SearchPathSeparator_str)));
         continue;
      }

#if defined(_WINDOWS)
      if (strcmp(msi_cur, SYSTEMROOT_STR) == 0) {
         msi_cur += SYSTEMROOT_STRLEN;

         if (SystemRoot) zfree(SystemRoot);
         SystemRoot = zstrdup(msi_cur);
         msi_cur += strlen(msi_cur) + 1;

         if (WinSysDir) zfree(WinSysDir);
         WinSysDir = strlwr(zstrdup(SystemRoot));
         strcpy(WinSysDir, &WinSysDir[2]);  // Start at "\"
         strcat(WinSysDir, "\\");           // End it with a "\"

         dbgmsg(("- ReadHeader: SystemRoot = '%s'  WinSysDir = '%s'\n", SystemRoot, WinSysDir));
         continue;
      }

      if (strcmp(msi_cur, SYSTEMDRIVE_STR) == 0) {
         msi_cur += SYSTEMDRIVE_STRLEN;

         if (SystemDrive) zfree(SystemDrive);
         SystemDrive = zstrdup(msi_cur);
         msi_cur += strlen(msi_cur) + 1;

         dbgmsg(("- ReadHeader: SystemDrive = '%s'\n", SystemDrive));
         continue;
      }

      if (msi_cur[1] == ':') {
         drv = (int)msi_cur[0] - 0x41;      // Drive letter to number (0-25)
         msi_cur += strlen(msi_cur) + 1;

         strcpy(dev_name[drv], msi_cur);
         msi_cur += strlen(msi_cur) + 1;

         dbgmsg(("- ReadHeader: Drive %c: = '%s'\n", (drv+0x41), dev_name[drv]));
         continue;
      }
#endif

      //
      // Don't understand this one - skip it
      //
      dbgmsg(("- ReadHeader: Unknown string: '%s'", msi_cur));
      msi_cur += strlen(msi_cur) + 1;       // Skip name
      dbgmsg((" - '%s'\n",msi_cur));
      msi_cur += strlen(msi_cur) + 1;       // Skip value
   }

   //
   // Align to the next 8-byte boundary
   //
   header_size = (uint)PtrSub(msi_cur,beg);
   CALC_PAD(header_size, pad);
   msi_cur += pad;

   //
   // Save information for A2nGetMsiFileInformation()
   //
   G(msi_file_wordsize) = msi_word_size;    // Word size of machine where MSI file written
   G(msi_file_bigendian) = msi_big_endian;  // Endiannes of machine where MSI file written
   if (windows)
      G(msi_file_os) = MSI_FILE_WINDOWS;    // OS where MSI file written
   else
      G(msi_file_os) = MSI_FILE_LINUX;      // OS where MSI file written

   //
   // Done
   //
   dbgmsg(("< ReadHeader: done\n"));
   return (msi_big_endian);
}


//
// WriteModule()
// *************
//
// Writes module node info to msi file:
// - MOD_INFO structure
// - module name string
// - hvname string (if present)
// - orgname string (if present)
// - pad bytes to align on 8-byte boundary
//
// Returns 0 on success, non-zero on error
//
static int WriteModule(FILE * fh, MOD_NODE * mn)
{
   MOD_INFO mi;
   int pad;


   dbgmsg((">> WriteModule: %s\n", mn->name));
   memset(&mi, 0, sizeof(MOD_INFO));

   mi.name_len = (int)strlen(mn->name) + 1;
   if (mn->hvname == NULL)
      mi.hvname_len = 0;
   else
      mi.hvname_len = (int)strlen(mn->hvname) + 1;

   if (mn->orgname == NULL)
      mi.orgname_len = 0;
   else
      mi.orgname_len = (int)strlen(mn->orgname) + 1;

   mi.flags = mn->flags;
   mi.ts = mn->ts;
   mi.chksum = mn->chksum;
   mi.name_hash = mn->name_hash;
   mi.type = mn->type;
   mi.symcnt = mn->base_symcnt;;                    // No aliases/contained symbols saved
   mi.seccnt = mn->seccnt;
   mi.ne_seccnt = mn->ne_seccnt;
   mi.codesize = mn->codesize;

   mi.slen = sizeof(MOD_INFO) + mi.name_len + mi.hvname_len + mi.orgname_len;
   CALC_PAD(mi.slen, pad);
   mi.slen += pad;

   if (!FileWrite(fh, (char *)&mi, sizeof(mi)))     // write module_info
      return (FAILURE);

   if (!FileWrite(fh, mn->name, mi.name_len))       // write module name
      return (FAILURE);

   if (mi.hvname_len) {
      if (!FileWrite(fh, mn->hvname, mi.hvname_len)) // write hvname if present
         return (FAILURE);
   }

   if (mi.orgname_len) {
      if (!FileWrite(fh, mn->orgname, mi.orgname_len)) // write orgname if present
         return (FAILURE);
   }

   if (pad) {
      if (!FileWrite(fh, "########", pad))          // pad to 8-byte boundary if needed
         return (FAILURE);
   }

   fflush(fh);
   return (SUCCESS);
}


//
// RecreateModule()
// ****************
//
// Reads module_info from msi file an creates appropriate module node.
// Returns non-NULL on success, NULL on failure
//
static MOD_NODE * RecreateModule(void)
{
   MOD_NODE * mn;
   MOD_INFO * mi;
   char * name, * hvname, * orgname;
   int cblen, namelen, hvnamelen, orgnamelen, symcnt, seccnt, ne_seccnt, codesize;

   dbgmsg(("> RecreateModule:\n"));

   mn =  AllocModuleNode();
   if (mn == NULL) {
      errmsg(("*E* RecreateModule: unable to allocate MOD_NODE\n"));
      return (NULL);
   }

   mi = (MOD_INFO *)msi_cur;
   cblen = cvt_32bits(mi->slen);
   namelen = cvt_32bits(mi->name_len);
   hvnamelen = cvt_32bits(mi->hvname_len);
   orgnamelen = cvt_32bits(mi->orgname_len);
   symcnt = cvt_32bits(mi->symcnt);
   seccnt = cvt_32bits(mi->seccnt);
   ne_seccnt = cvt_32bits(mi->ne_seccnt);
   codesize = cvt_32bits(mi->codesize);
   msi_cur += cblen;

   dbgmsg(("- RecreateModule: symcnt=0x%08x (%d)   mi->symcnt=0x%08x (%d)\n",
           symcnt, symcnt, mi->symcnt, mi->symcnt));

   name = (char *)mi + sizeof(MOD_INFO);
   hvname = name + namelen;
   orgname = hvname + hvnamelen;

   dbgmsg(("- RecreateModule: name=%s  hvname=%s  orgname=%s\n", name, hvname, orgname));

   dbgmsg(("- RecreateModule: mi=%p (%d) %s  syms=%d  secs=%d  code=%d\n",
          mi, (char *)mi - msi_base, name, symcnt, seccnt, codesize));

   //
   // Initialize module node from saved module info
   //
   mn->ts = cvt_32bits(mi->ts);
   mn->chksum = cvt_32bits(mi->chksum);
   mn->name_hash = cvt_32bits(mi->name_hash);
   mn->flags = cvt_32bits(mi->flags);
   mn->flags &= ~A2N_FLAGS_REQUESTED;
   mn->type = cvt_32bits(mi->type);
   mn->symcnt = mn->base_symcnt = symcnt;
   mn->seccnt = seccnt;
   mn->ne_seccnt = ne_seccnt;
   mn->codesize = codesize;
   mn->strsize = namelen + hvnamelen + orgnamelen;

   mn->name = zstrdup(name);
   if (mn->name == NULL) {
      FreeModuleNode(mn);
      errmsg(("*E* RecreateModule: unable to allocate module name for %s.\n", name));
      return (NULL);
   }

   if (hvnamelen == 0)
      mn->hvname = NULL;
   else {
      mn->hvname = zstrdup(hvname);
      if (mn->hvname == NULL) {
         FreeModuleNode(mn);
         errmsg(("*E* RecreateModule: unable to allocate module hvname for %s.\n", name));
         return (NULL);
      }
   }

   if (orgnamelen == 0)
      mn->orgname = NULL;
   else {
      mn->orgname = zstrdup(orgname);
      if (mn->orgname == NULL) {
         FreeModuleNode(mn);
         errmsg(("*E* RecreateModule: unable to allocate module orgname for %s.\n", name));
         return (NULL);
      }
   }

   //
   // Insert the new node at the end of module list
   //
   G(ModCnt)++;
   mn->next = NULL;
   if (G(ModRoot) == NULL)
      G(ModRoot) = G(ModEnd) = mn;          // First node (also the last)
   else {
      G(ModEnd)->next = mn;                 // Add at the end
      G(ModEnd) = mn;                       // New end
   }

   //
   // Done
   //
   dbgmsg(("< RecreateModule: mn=%p, ModCnt=%d\n", mn, G(ModCnt)));
   return (mn);
}


//
// WriteSection()
// **************
//
// Writes section node info to msi file:
// - SEC_INFO structure
// - section name string
// - code (if any)
// - pad bytes to align on 8-byte boundary
//
// Returns 0 on success, non-zero on error
//
static int WriteSection(FILE * fh, SEC_NODE * cn)
{
   SEC_INFO si;
   int pad;


   dbgmsg((">> WriteSection: %s\n", cn->name));
   memset(&si, 0, sizeof(SEC_INFO));

   si.name_len = (int)strlen(cn->name) + 1;
   si.flags = cn->flags;
   si.start_addr = cn->start_addr;
   si.end_addr = cn->end_addr;
   si.offset_start = cn->offset_start;
   si.offset_end = cn->offset_end;
   si.size = cn->size;
   si.number = cn->number;
   si.sec_flags = cn->sec_flags;

   if (cn->code)
      si.codesize = cn->size;
   else
      si.codesize = 0;

   si.slen = sizeof(SEC_INFO) + si.name_len + si.codesize;
   CALC_PAD(si.slen, pad);
   si.slen += pad;

   if (!FileWrite(fh, (char *)&si, sizeof(si)))     // write section_info
      return (FAILURE);

   if (!FileWrite(fh, cn->name, si.name_len))       // write section name
      return (FAILURE);

   if (si.codesize) {
      dbgmsg(("- WriteSection: codesize=%d\n", si.codesize));
      if (!FileWrite(fh, (char *)cn->code, si.codesize))  // Write code (if any)
         return (FAILURE);
   }

   if (pad) {
      if (!FileWrite(fh, "********", pad))          // pad to 8-byte boundary if needed
         return (FAILURE);
   }

   fflush(fh);
   return (SUCCESS);
}


//
// RecreateSections()
// ******************
//
// Reads section_info from msi file an creates appropriate section nodes..
// Returns 0 on success, non-zero on error
//
static int RecreateSections(MOD_NODE * mn)
{
   SEC_NODE * cn;
   SEC_INFO * si;
   int i, cblen;
   int ex_seccnt, ne_seccnt;


   dbgmsg(("> RecreateSections:\n"));
   ne_seccnt = mn->ne_seccnt;               // Non-executable sections
   ex_seccnt = mn->seccnt - ne_seccnt;      // Executable sections

   //
   // Do executable sections (if any)
   //
   for (i = 0; i < ex_seccnt; i++) {
      si = (SEC_INFO *)msi_cur;
      cblen = cvt_32bits(si->slen);
      msi_cur += cblen;

      cn = InitialzeSectionNode(si, mn, 1);
      if (cn == NULL) {
         dbgmsg(("< RecreateSections: unable to initialize executable section node\n"));
         return (FAILURE);
      }
   }

   //
   // Do non-executable sections (if any)
   //
   for (i = 0; i < ne_seccnt; i++) {
      si = (SEC_INFO *)msi_cur;
      cblen = cvt_32bits(si->slen);
      msi_cur += cblen;

      cn = InitialzeSectionNode(si, mn, 0);
      if (cn == NULL) {
         dbgmsg(("< RecreateSections: unable to initialize non-executable section node\n"));
         return (FAILURE);
      }
   }

   //
   // Done
   //
   dbgmsg(("< RecreateSections\n"));
   return (SUCCESS);
}


//
// InitialzeSectionNode()
// **********************
//
// Create a section node from a section_info in the msi file.
// Returns SEC_NODE * on success, NULL on error
//
static SEC_NODE * InitialzeSectionNode(SEC_INFO * si, MOD_NODE * mn, int executable)
{
   SEC_NODE * cn;
   char * name;
   int namelen, codesize;

   namelen = cvt_32bits(si->name_len);
   codesize = cvt_32bits(si->codesize);

   name = (char *)si + sizeof(SEC_INFO);

   cn = AllocSectionNode();
   if (cn == NULL) {
      dbgmsg(("< RecreateSections: unable to allocate SEC_NODE\n"));
      return (NULL);
   }

   cn->flags = cvt_32bits(si->flags);
   cn->number = cvt_32bits(si->number);
   cn->sec_flags = cvt_32bits(si->sec_flags);
   cn->size = cvt_32bits(si->size);
   cn->start_addr = si->start_addr;
   cn->end_addr   = si->end_addr;
   cn->offset_start = cvt_32bits(si->offset_start);
   cn->offset_end = cvt_32bits(si->offset_end);

   dbgmsg(("- section %x at %p (%d) %s (EX=%d)\n", cn->number, si, (char *)si - msi_base, name, executable));
   mn->strsize += namelen;
   cn->name = zstrdup(name);
   if (cn->name == NULL) {
      FreeSectionNode(cn);
      dbgmsg(("*E* RecreateSections: unable to allocate section name.\n"));
      return (NULL);
   }

   if (codesize) {
      cn->code = CopyCode(((char *)si + sizeof(SEC_INFO) + namelen), codesize);
      dbgmsg(("- section %x codesize=%d\n", cn->number, codesize));
   }

   //
   // Link in the new node to the beginning of the correct section node
   // list (executable/non-executable) for the module that owns the section.
   //
   if (executable) {                        // Executable section
      cn->next = NULL;
      if (mn->secroot == NULL)
         mn->secroot = mn->secroot_end = cn;       // First node (also the last)
      else {
         mn->secroot_end->next = cn;               // Add at the end
         mn->secroot_end = cn;                     // New end
      }
   }
   else {                                   // Non-executable section
      cn->next = NULL;
      if (mn->ne_secroot == NULL)
         mn->ne_secroot = mn->ne_secroot_end = cn; // First node (also the last)
      else {
         mn->ne_secroot_end->next = cn;            // Add at the end
         mn->ne_secroot_end = cn;                  // New end
      }
   }

   return (cn);
}


//
// WriteSymbol()
// *************
//
// Writes symbol node info to msi file:
// - SYM_INFO structure
// - symbol name string
// - code (if any)
// - pad bytes to align on 8-byte boundary
//
// Returns 0 on success, non-zero on error
//
static int WriteSymbol(FILE * fh, SYM_NODE * sn)
{
   SYM_INFO si;
   int pad;

   dbgmsg((">> WriteSymbol: %s\n", sn->name));
   memset(&si, 0, sizeof(SYM_INFO));

   si.name_len = (int)strlen(sn->name) + 1;
   si.flags = sn->flags;
   si.offset_start = sn->offset_start;
   si.offset_end = sn->offset_end;
   si.length = sn->length;
   si.type = sn->type;
   si.sym_flags = sn->sym_flags;
   si.section = sn->section;
   si.name_hash = sn->name_hash;

   if ((sn->type == SYMBOL_TYPE_USER_SUPPLIED) && (sn->code != NULL))
      si.codesize = sn->length;             // Only for user-supplied symbols!
   else
      si.codesize = 0;

   si.slen = sizeof(SYM_INFO) + si.name_len + si.codesize;
   CALC_PAD(si.slen, pad);
   si.slen += pad;

   if (!FileWrite(fh, (char *)&si, sizeof(si)))     // write symbol_info
      return (FAILURE);

   if (!FileWrite(fh, sn->name, si.name_len))       // write symbol name
      return (FAILURE);

   if (si.codesize) {
      dbgmsg(("- WriteSymbol: codesize=%d\n",si.codesize));
      if (!FileWrite(fh, (char *)sn->code, si.codesize))  // Write code (if any)
         return (FAILURE);
   }

   if (pad) {
      if (!FileWrite(fh, "********", pad))          // pad to 8-byte boundary if needed
         return (FAILURE);
   }

   fflush(fh);
   return (SUCCESS);
}


//
// RecreateSymbols()
// *****************
//
// Reads symbol_info from msi file an creates appropriate symbol nodes..
// Returns 0 on success, non-zero on error
//
static int RecreateSymbols(MOD_NODE * mn)
{
   SYM_NODE * sym;                          // Array of symbol nodes
   SYM_NODE ** psym;                        // Array of pointers to symbol nodes
   SYM_INFO * si;
   int i, rc, cblen;


   dbgmsg(("> RecreateSymbols: mn=%p\n", mn));
   sym = (SYM_NODE *)zmalloc(mn->base_symcnt * sizeof(SYM_NODE));
   if (sym == NULL) {
      errmsg(("*E* RecreateSymbols: unable to malloc symbol node array for '%s'.\n", mn->name));
      return (1);
   }
   memset(sym, 0, (mn->base_symcnt * sizeof(SYM_NODE)));  // Clear the symbol array

   psym = (SYM_NODE **)zmalloc(mn->base_symcnt * sizeof(SYM_NODE *));
   if (sym == NULL) {
      errmsg(("*E* RecreateSymbols: unable to malloc symbol node pointer array for '%s'.\n", mn->name));
      return (1);
   }

   mn->symroot = psym;
   mn->flags |= A2N_SYMBOL_ARRAY_BUILT;

   for (i = 0; i < mn->base_symcnt; i++) {
      si = (SYM_INFO *)msi_cur;
      cblen = cvt_32bits(si->slen);
      msi_cur += cblen;

      dbgmsg(("- RecreateSymbols: symbol %d\n", i+1));
      psym[i] = &sym[i];
      rc = InitialzeSymbolNode(si, psym[i], mn);
      if (rc == FAILURE) {
         errmsg(("*E* RecreateSymbols: InitializeSymbolNode() error on symbol %d or %d\n", i+1, mn->base_symcnt));
         return (rc);
      }
   }

   //
   // Done
   //
   dbgmsg(("< RecreateSymbols:\n"));
   return (0);
}


//
// InitialzeSymbolNode()
// *********************
//
// Create a symbol node from a symbol_info in the msi file.
// Returns 0 on success, non-zero on error
//
static int InitialzeSymbolNode(SYM_INFO * si, SYM_NODE * sn, MOD_NODE * mn)
{
   char * name;
   int namelen, codesize;

   name = (char *)si + sizeof(SYM_INFO);
   dbgmsg(("> InitializeSymbol: si=%p (%d) %s\n", si, (char *)si - msi_base, name));
   namelen = cvt_32bits(si->name_len);
   codesize = cvt_32bits(si->codesize);

   sn->flags = cvt_32bits(si->flags);
   sn->alias_cnt = sn->sub_cnt = 0;
   sn->offset_start = cvt_32bits(si->offset_start);
   sn->offset_end = cvt_32bits(si->offset_end);
   sn->length = cvt_32bits(si->length);
   sn->type = cvt_32bits(si->type);
   sn->sym_flags = cvt_32bits(si->sym_flags);
   sn->section = cvt_32bits(si->section);
   sn->name_hash = cvt_32bits(si->name_hash);

   mn->strsize += namelen;
   sn->name = zstrdup(name);
   if (sn->name == NULL) {
      dbgmsg(("< InitializeSymbol: unable to allocate symbol name.\n"));
      return (FAILURE);
   }

   if (codesize) {                          // user-defined symbol
      sn->code = CopyCode(((char *)si + sizeof(SYM_INFO) + namelen), codesize);
      dbgmsg(("- symbol codesize=%d\n", codesize));
   }
   else {                                   // all others
      CalculateCodePtr(mn, sn);
   }

   dbgmsg(("< InitializeSymbol:\n"));
   return (SUCCESS);
}


//
// CalculateCodePtr()
// ******************
//
// Calculate local code pointer for given symbol.
//
static void CalculateCodePtr(MOD_NODE * mn, SYM_NODE * sn)
{
   SEC_NODE * cn;


   sn->code = NULL;                         // prepare for the worst

   if (mn->seccnt == 0)
      return;                               // there are no sections

   cn = mn->secroot;
   while (cn) {
      if (cn->number == sn->section) {      // symbol in this section
         if (cn->code == NULL)
            return;                         // no code in this section

         sn->code = (char *)cn->code + (sn->offset_start - cn->offset_start);
         return;
      }
      cn = cn->next;
   }

   return;                                  // couldn't find right section for symbol
}


//
// cvt_32bits()
// ************
//
// Returns 32-bit argument in correct endiannes.
// Needed when reading a MSI file created on a machine of different
// endiannes as the machine where the file is being read.
//
static uint cvt_32bits(uint ul)
{
   if (G(big_endian) == msi_big_endian)
      return (ul);                          // file and machine same endianness. done.
   else
      return ((ul << 24) |                  // reverse the bytes
              ((ul & 0x0000ff00) << 8) |
              ((ul & 0x00ff0000) >> 8) |
              (ul >> 24));
}


#if defined(_64BIT)
//
// cvt_64bits()
// ************
//
// Returns 64-bit argument in correct endiannes.
// Needed when reading a MSI file created on a machine of different
// endiannes as the machine where the file is being read.
//
//static uint64 cvt_64bits(uint64 ull)
//{
//   if (G(big_endian) == msi_big_endian)
//      return (ull);                         // file and machine same endianness. done.
//   else
//      return ((ull << 56) |
//              ((ull & 0x000000000000ff00) << 40) |
//              ((ull & 0x0000000000ff0000) << 24) |
//              ((ull & 0x00000000ff000000) << 8)  |
//              ((ull & 0x000000ff00000000) >> 8)  |
//              ((ull & 0x0000ff0000000000) >> 24) |
//              ((ull & 0x00ff000000000000) >> 40) |
//              (ull >> 56));
//}
#endif
