//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                     Windows Symbol Harvester Debug Code                  //
//                     -----------------------------------                  //
//                                                                          //
//**************************************************************************//
//
#include "a2nhdr.h"


//
// Externs (from a2n.c)
//
extern gv_t a2n_gv;                         // Globals


//
// Stuff needed when dumping debug information
//
#ifdef DEBUG

// Names of the data directories
char *ImageDirectoryNames[] = {"EXPORT",
                               "IMPORT",
                               "RESOURCE",
                               "EXCEPTION",
                               "SECURITY",
                               "BASERELOC",
                               "DEBUG",
                               "ARCHITECTURE",
                               "GLOBALPTR",
                               "TLS",
                               "LOAD_CONFIG",
                               "BOUND_IMPORT",
                               "IAT",
                               "DELAY_IMPORT",
                               "COM_DESCRIPTOR",
                               "Unused"};

typedef struct {
   DWORD   flag;
   char   *name;
} DWORD_FLAG_DESCRIPTIONS;

// These are commented out in WinNT.h
#define IMAGE_SCN_TYPE_REG        0x00000000  // Reserved.
#define IMAGE_SCN_TYPE_DSECT      0x00000001  // Reserved.
#define IMAGE_SCN_TYPE_NOLOAD     0x00000002  // Reserved.
#define IMAGE_SCN_TYPE_GROUP      0x00000004  // Reserved.
#define IMAGE_SCN_TYPE_COPY       0x00000010  // Reserved.
#define IMAGE_SCN_TYPE_OVER       0x00000400  // Reserved.
#define IMAGE_SCN_MEM_PROTECTED   0x00004000
#define IMAGE_SCN_MEM_SYSHEAP     0x00010000

// Bitfield values and names for the IMAGE_SECTION_HEADER flags
DWORD_FLAG_DESCRIPTIONS SectionFlags[] = {
   {IMAGE_SCN_TYPE_DSECT             , "DSECT"             },
   {IMAGE_SCN_TYPE_NOLOAD            , "NOLOAD"            },
   {IMAGE_SCN_TYPE_GROUP             , "GROUP"             },
   {IMAGE_SCN_TYPE_NO_PAD            , "NO_PAD"            },
   {IMAGE_SCN_TYPE_COPY              , "COPY"              },
   {IMAGE_SCN_CNT_CODE               , "CODE"              },
   {IMAGE_SCN_CNT_INITIALIZED_DATA   , "INITIALIZED_DATA"  },
   {IMAGE_SCN_CNT_UNINITIALIZED_DATA , "UNINITIALIZED_DATA"},
   {IMAGE_SCN_LNK_OTHER              , "LNK_OTHER"         },
   {IMAGE_SCN_LNK_INFO               , "LNK_INFO"          },
   {IMAGE_SCN_TYPE_OVER              , "TYPE_OVER"         },
   {IMAGE_SCN_LNK_REMOVE             , "LNK_REMOVE"        },
   {IMAGE_SCN_LNK_COMDAT             , "LNK_COMDAT"        },
   {IMAGE_SCN_NO_DEFER_SPEC_EXC      , "NO_DEFER_SPEC_EXC" },
   {IMAGE_SCN_MEM_FARDATA            , "FARDATA"           },
   {IMAGE_SCN_MEM_SYSHEAP            , "SYSHEAP"           },
   {IMAGE_SCN_MEM_PURGEABLE          , "PURGEABLE"         },
   {IMAGE_SCN_MEM_16BIT              , "16BIT"             },
   {IMAGE_SCN_MEM_LOCKED             , "LOCKED"            },
   {IMAGE_SCN_MEM_PRELOAD            , "PRELOAD"           },
   {IMAGE_SCN_LNK_NRELOC_OVFL        , "LNK_NRELOC_OVFL"   },
   {IMAGE_SCN_MEM_DISCARDABLE        , "DISCARDABLE"       },
   {IMAGE_SCN_MEM_NOT_CACHED         , "NOT_CACHED"        },
   {IMAGE_SCN_MEM_NOT_PAGED          , "NOT_PAGED"         },
   {IMAGE_SCN_MEM_SHARED             , "SHARED"            },
   {IMAGE_SCN_MEM_EXECUTE            , "EXECUTE"           },
   {IMAGE_SCN_MEM_READ               , "READ"              },
   {IMAGE_SCN_MEM_WRITE              , "WRITE"             },
};

#define NUMBER_SECTION_FLAGS \
    (sizeof(SectionFlags) / sizeof(DWORD_FLAG_DESCRIPTIONS))


char *DebugFormats[] = {"UNKNOWN",       // IMAGE_DEBUG_TYPE_UNKNOWN          0
                        "COFF",          // IMAGE_DEBUG_TYPE_COFF             1
                        "CODEVIEW",      // IMAGE_DEBUG_TYPE_CODEVIEW         2
                        "FPO",           // IMAGE_DEBUG_TYPE_FPO              3
                        "MISC",          // IMAGE_DEBUG_TYPE_MISC             4
                        "EXCEPTION",     // IMAGE_DEBUG_TYPE_EXCEPTION        5
                        "FIXUP",         // IMAGE_DEBUG_TYPE_FIXUP            6
                        "OMAP_TO_SRC",   // IMAGE_DEBUG_TYPE_OMAP_TO_SRC      7
                        "OMAP_FROM_SRC", // IMAGE_DEBUG_TYPE_OMAP_FROM_SRC    8
                        "BORLAND",       // IMAGE_DEBUG_TYPE_BORLAND          9
                        "RESERVED10",    // IMAGE_DEBUG_TYPE_RESERVED10       10
                        "CLSID"};        // IMAGE_DEBUG_TYPE_CLSID            11
#endif //DEBUG





//
// DumpNtHeader()
// **************
//
void DumpNtHeader(IMAGE_NT_HEADERS *nth)
{
#ifdef DEBUG
   IMAGE_FILE_HEADER *ifh;                  // Image file header
   IMAGE_OPTIONAL_HEADER *ioh;              // Image optional header
   uint ioh_NumberOfRvaAndSizes;
   IMAGE_DATA_DIRECTORY *ioh_DataDirectory;
   int i;


   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         ifh = &(nth->FileHeader);
         ioh = &(nth->OptionalHeader);
         //
         // Dump NT Header signature
         //
         msg_log("\nImageSignature: 0x%08x\n\n",nth->Signature);

         //
         // Dump File Header fields we care about
         //
         msg_log("\nFILE HEADER VALUES:\n");
         msg_log("*******************\n");
         msg_log("              Machine: 0x%04X ",ifh->Machine);
            if (ifh->Machine == IMAGE_FILE_MACHINE_I386)
               msg_log("(x86 compatible)\n");
            else if (ifh->Machine == IMAGE_FILE_MACHINE_IA64)
               msg_log("(IA64)\n");
            else if (ifh->Machine == IMAGE_FILE_MACHINE_AMD64)
               msg_log("(AMD64)\n");
            else
               msg_log("(????)\n");
         msg_log("     NumberOfSections: 0x%04X (%d)\n",ifh->NumberOfSections,ifh->NumberOfSections);
         msg_log("        TimeDateStamp: 0x%08X => %s",ifh->TimeDateStamp,TsToDate(ifh->TimeDateStamp));
         msg_log("  COFFSymbolTable RVA: 0x%08X\n",ifh->PointerToSymbolTable);
         msg_log("      NumberOfSymbols: 0x%08X (%d)\n",ifh->NumberOfSymbols,ifh->NumberOfSymbols);
         msg_log("SizeOfOptionalHeaders: 0x%04X\n",ifh->SizeOfOptionalHeader);
         msg_log("      Characteristics: 0x%04X\n",ifh->Characteristics);
            if (ifh->Characteristics & IMAGE_FILE_RELOCS_STRIPPED)
               msg_log("                       0x%04X Relocation info stripped\n",IMAGE_FILE_RELOCS_STRIPPED);
            if (ifh->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE)
               msg_log("                       0x%04X Executable\n",IMAGE_FILE_EXECUTABLE_IMAGE);
            if (ifh->Characteristics & IMAGE_FILE_LINE_NUMS_STRIPPED)
               msg_log("                       0x%04X Line numbers stripped\n",IMAGE_FILE_LINE_NUMS_STRIPPED);
            if (ifh->Characteristics & IMAGE_FILE_LOCAL_SYMS_STRIPPED)
               msg_log("                       0x%04X Local symbols stripped\n",IMAGE_FILE_LOCAL_SYMS_STRIPPED);
            if (ifh->Characteristics & IMAGE_FILE_AGGRESIVE_WS_TRIM)
               msg_log("                       0x%04X Agressively trim working set\n",IMAGE_FILE_AGGRESIVE_WS_TRIM);
            if (ifh->Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE)
               msg_log("                       0x%04X Large address aware\n",IMAGE_FILE_LARGE_ADDRESS_AWARE);
            if (ifh->Characteristics & IMAGE_FILE_BYTES_REVERSED_LO)
               msg_log("                       0x%04X Bytes of machine word are reversed\n",IMAGE_FILE_BYTES_REVERSED_LO);
            if (ifh->Characteristics & IMAGE_FILE_32BIT_MACHINE)
               msg_log("                       0x%04X 32 bit word machine\n",IMAGE_FILE_32BIT_MACHINE);
            if (ifh->Characteristics & IMAGE_FILE_DEBUG_STRIPPED)
               msg_log("                       0x%04X Debug information stripped to DBG/PDB file\n",IMAGE_FILE_DEBUG_STRIPPED);
            if (ifh->Characteristics & IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP)
               msg_log("                       0x%04X If on removable media copy and run from swap file\n",IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP);
            if (ifh->Characteristics & IMAGE_FILE_NET_RUN_FROM_SWAP)
               msg_log("                       0x%04X If on network copy and run from swap file\n",IMAGE_FILE_NET_RUN_FROM_SWAP);
            if (ifh->Characteristics & IMAGE_FILE_SYSTEM)
               msg_log("                       0x%04X System file\n",IMAGE_FILE_SYSTEM);
            if (ifh->Characteristics & IMAGE_FILE_DLL)
               msg_log("                       0x%04X DLL\n",IMAGE_FILE_DLL);
            if (ifh->Characteristics & IMAGE_FILE_UP_SYSTEM_ONLY)
               msg_log("                       0x%04X File should only be run on a UP machine\n",IMAGE_FILE_UP_SYSTEM_ONLY);
            if (ifh->Characteristics & IMAGE_FILE_BYTES_REVERSED_HI)
               msg_log("                       0x%04X Bytes of machine word are reversed\n",IMAGE_FILE_BYTES_REVERSED_HI);

         //
         // If there is an Optional Header dump fields we care about
         //
         if (ifh->SizeOfOptionalHeader == 0) {
            return;                               // Images *MUST* have an Optional Header
         }

         msg_log("\nOPTIONAL HEADER VALUES:\n");
         msg_log("***********************\n");
         msg_log("                  Magic: 0x%X ",ioh->Magic);
            if (ioh->Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
               msg_log("(PE32)\n");
            else if (ioh->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
               msg_log("(PE32+)\n");
            else
               msg_log("(????)\n");
         msg_log("     MajorLinkerVersion: 0x%02X\n",ioh->MajorLinkerVersion);
         msg_log("     MinorLinkerVersion: 0x%02X\n",ioh->MinorLinkerVersion);
         msg_log("             SizeOfCode: 0x%08X\n",ioh->SizeOfCode);
         msg_log("  SizeOfInitializedData: 0x%08X\n",ioh->SizeOfInitializedData);
         msg_log("SizeOfUninitializedData: 0x%08X\n",ioh->SizeOfUninitializedData);
         msg_log("    AddressOfEntryPoint: 0x%08X\n",ioh->AddressOfEntryPoint);
         msg_log("             BaseOfCode: 0x%08X\n",ioh->BaseOfCode);

         if (ioh->Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            msg_log("             BaseOfData: 0x%08X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->BaseOfData);
            msg_log("              ImageBase: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER32 *)ioh)->ImageBase);
            msg_log("       SectionAlignment: 0x%X\n",  ((IMAGE_OPTIONAL_HEADER32 *)ioh)->SectionAlignment);
            msg_log("          FileAlignment: 0x%X\n",  ((IMAGE_OPTIONAL_HEADER32 *)ioh)->FileAlignment);
            msg_log("         MajorOSVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->MajorOperatingSystemVersion);
            msg_log("         MinorOSVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->MinorOperatingSystemVersion);
            msg_log("      MajorImageVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->MajorImageVersion);
            msg_log("      MinorImageVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->MinorImageVersion);
            msg_log("  MajorSubsystemVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->MajorSubsystemVersion);
            msg_log("  MinorSubsystemVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->MinorSubsystemVersion);
            msg_log("      Win32VersionValue: 0x%08X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->Win32VersionValue);
            msg_log("            SizeOfImage: 0x%08X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->SizeOfImage);
            msg_log("          SizeOfHeaders: 0x%08X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->SizeOfHeaders);
            msg_log("               CheckSum: 0x%08X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->CheckSum);
            msg_log("              Subsystem: 0x%04X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->Subsystem);
            msg_log("     DllCharacteristics: 0x%04X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->DllCharacteristics);
            msg_log("     SizeOfStackReserve: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER32 *)ioh)->SizeOfStackReserve);
            msg_log("      SizeOfStackCommit: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER32 *)ioh)->SizeOfStackCommit);
            msg_log("      SizeOfHeapReserve: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER32 *)ioh)->SizeOfHeapReserve);
            msg_log("       SizeOfHeapCommit: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER32 *)ioh)->SizeOfHeapCommit);
            msg_log("            LoaderFlags: 0x%08X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->LoaderFlags);
            msg_log("    NumberOfRvaAndSizes: 0x%08X\n",((IMAGE_OPTIONAL_HEADER32 *)ioh)->NumberOfRvaAndSizes);
         }
         else {
            msg_log("              ImageBase: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER64 *)ioh)->ImageBase);
            msg_log("       SectionAlignment: 0x%X\n",  ((IMAGE_OPTIONAL_HEADER64 *)ioh)->SectionAlignment);
            msg_log("          FileAlignment: 0x%X\n",  ((IMAGE_OPTIONAL_HEADER64 *)ioh)->FileAlignment);
            msg_log("         MajorOSVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->MajorOperatingSystemVersion);
            msg_log("         MinorOSVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->MinorOperatingSystemVersion);
            msg_log("      MajorImageVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->MajorImageVersion);
            msg_log("      MinorImageVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->MinorImageVersion);
            msg_log("  MajorSubsystemVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->MajorSubsystemVersion);
            msg_log("  MinorSubsystemVersion: 0x%04X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->MinorSubsystemVersion);
            msg_log("      Win32VersionValue: 0x%08X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->Win32VersionValue);
            msg_log("            SizeOfImage: 0x%08X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->SizeOfImage);
            msg_log("          SizeOfHeaders: 0x%08X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->SizeOfHeaders);
            msg_log("               CheckSum: 0x%08X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->CheckSum);
            msg_log("              Subsystem: 0x%04X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->Subsystem);
            msg_log("     DllCharacteristics: 0x%04X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->DllCharacteristics);
            msg_log("     SizeOfStackReserve: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER64 *)ioh)->SizeOfStackReserve);
            msg_log("      SizeOfStackCommit: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER64 *)ioh)->SizeOfStackCommit);
            msg_log("      SizeOfHeapReserve: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER64 *)ioh)->SizeOfHeapReserve);
            msg_log("       SizeOfHeapCommit: 0x%p\n",  ((IMAGE_OPTIONAL_HEADER64 *)ioh)->SizeOfHeapCommit);
            msg_log("            LoaderFlags: 0x%08X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->LoaderFlags);
            msg_log("    NumberOfRvaAndSizes: 0x%08X\n",((IMAGE_OPTIONAL_HEADER64 *)ioh)->NumberOfRvaAndSizes);
         }

         ioh_NumberOfRvaAndSizes = GetIohNumberOfRvaAndSizes(ioh);
         ioh_DataDirectory = GetIohDataDirectory(ioh);

         msg_log("\nDATA DIRECTORIES:\n");
         msg_log("*****************\n");
         msg_log("       Name          RVA         Size\n");
         msg_log("  --------------  ----------  ----------\n");
         for (i = 0; i < (int)ioh_NumberOfRvaAndSizes; i++) {
            msg_log("  %-14s  0x%08X  0x%08X\n", ImageDirectoryNames[i],
                    ioh_DataDirectory[i].VirtualAddress, ioh_DataDirectory[i].Size);
         }
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpNtHeader #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpSectionHeaders()
// ********************
//
void DumpSectionHeaders(IMAGE_SECTION_HEADER *ish, int numsec)
{
#ifdef DEBUG
   int i, j;
   char SectNameBuf[9];                     // Section names limited to 8 chars

   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("\nSECTION HEADERS:\n");
         msg_log("****************\n");
         msg_log(" ## Name     VirtSize   VirtAddr   RawDataSz  RawDataRVA Flags\n");
         msg_log(" -- -------- ---------- ---------- ---------- ---------- ----------\n");

         for (i = 0; i < numsec; i++) {
            memcpy(SectNameBuf, &(ish[i].Name), IMAGE_SIZEOF_SHORT_NAME);
            SectNameBuf[8] = 0;                // In case name is exactly 8 bytes
            msg_log(" %02d %-8s 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X\n",
                    (i+1), SectNameBuf, ish[i].Misc.VirtualSize,
                    ish[i].VirtualAddress, ish[i].SizeOfRawData,
                    ish[i].PointerToRawData, ish[i].Characteristics);
            msg_log("    ");

            for (j = 0; j < NUMBER_SECTION_FLAGS; j++) {
               if (ish[i].Characteristics & SectionFlags[j].flag)
                   msg_log("%s ",SectionFlags[j].name);
            }
            msg_log("\n\n");
         }
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpSectionHeaders #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpExportDirectoryTable()
// **************************
//
void DumpExportDirectoryTable(IMAGE_EXPORT_DIRECTORY *ied)
{
#ifdef DEBUG
   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("\nEXPORT DIRECTORY TABLE:\n");
         msg_log("***********************\n");
         msg_log("        Characteristics: 0x%08X\n",ied->Characteristics);
         msg_log("          TimeDateStamp: 0x%08X\n",ied->TimeDateStamp);
         msg_log("           MajorVersion: 0x%04X\n",ied->MajorVersion);
         msg_log("           MinorVersion: 0x%04X\n",ied->MinorVersion);
         msg_log("                   Name: 0x%08X\n",ied->Name);
         msg_log("                   Base: 0x%08X\n",ied->Base);
         msg_log("      NumberOfFunctions: 0x%08X\n",ied->NumberOfFunctions);
         msg_log("          NumberOfNames: 0x%08X\n",ied->NumberOfNames);
         msg_log("     AddressOfFunctions: 0x%08X\n",ied->AddressOfFunctions);
         msg_log("         AddressOfNames: 0x%08X\n",ied->AddressOfNames);
         msg_log("  AddressOfNameOrdinals: 0x%08X\n",ied->AddressOfNameOrdinals);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpExportDirectoryTable #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpDebugDirectories()
// **********************
//
void DumpDebugDirectories(IMAGE_DEBUG_DIRECTORY *idd, int debug_dir_cnt)
{
#ifdef DEBUG
   int i;

   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("\nDEBUG_DIRECTORIES:\n");
         msg_log("******************\n");
         msg_log(" Type          Chars      TS         Ma:Mi Size    RawDataVa  RawDataRVA\n");
         msg_log(" ------------- ---------- ---------- ----- ------- ---------- ----------\n");

         for (i = 0; i < debug_dir_cnt; i++) {
            msg_log(" %-13s 0x%08X 0x%08X %02d:%02d %7d 0x%08X 0x%08X\n",
                    ((idd[i].Type <= IMAGE_DEBUG_TYPE_CLSID) ? DebugFormats[idd[i].Type] : "?????"),
                    idd[i].Characteristics, idd[i].TimeDateStamp,
                    idd[i].MajorVersion, idd[i].MinorVersion, idd[i].SizeOfData,
                    idd[i].AddressOfRawData, idd[i].PointerToRawData);
         }
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpDebugDirectories #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpMiscDebugSection()
// **********************
//
void DumpMiscDebugSection(IMAGE_DEBUG_MISC *idm)
{
#ifdef DEBUG
   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("\nMISC DEBUG SECTION:\n");
         msg_log("*******************\n");
         msg_log("   DataType              0x%X\n",idm->DataType);
         msg_log("   Length                0x%X\n",idm->Length);
         msg_log("   Unicode               %1d\n",idm->Unicode);
         msg_log("   Data                  %s\n",idm->Data);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpMiscDebugSection #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpCodeviewDebugHeader()
// *************************
//
void DumpCodeviewDebugHeader(IMAGE_CV_HEADER *cvh)
{
#ifdef DEBUG
   char cvsig[5];

   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         memcpy(cvsig, cvh, 4);
         cvsig[4] = 0;
         msg_log("\nCODEVIEW DEBUG HEADER:\n");
         msg_log("**********************\n");
         msg_log("   Signature             %s\n",cvsig);
         if (strcmp(cvsig,"NB10") == 0) { // PDB file (VC++ 6)
            msg_log("   FilePos               0x%08X\n",((IMAGE_CV_NB10_HEADER *)cvh)->filepos);
            msg_log("   TimeDateStamp         0x%08X => %s",((IMAGE_CV_NB10_HEADER *)cvh)->TimeDateStamp,TsToDate(((IMAGE_CV_NB10_HEADER *)cvh)->TimeDateStamp));
            msg_log("   PDBFileRevision       %d\n",((IMAGE_CV_NB10_HEADER *)cvh)->PDBFileRevision);
            msg_log("   PDBFilePath           %s\n",((IMAGE_CV_NB10_HEADER *)cvh)->Data);
         }
         else if (strcmp(cvsig,"RSDS") == 0) { // PDB file (VC++ 7)
            uchar *id = (uchar *)((IMAGE_CV_RSDS_HEADER *)cvh)->id;
            msg_log("   id                    {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
                                               *(ULONG *)id, *(USHORT *)(id+4), *(USHORT *)(id+6),
                                               id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15]);
            msg_log("   PDBFileRevision       %d\n",((IMAGE_CV_RSDS_HEADER *)cvh)->PDBFileRevision);
            msg_log("   PDBFilePath           %s\n",((IMAGE_CV_RSDS_HEADER *)cvh)->Data);
         }
         else {
            msg_log("   FilePos (of DirHdr)   0x%08X\n",((IMAGE_CV_HEADER *)cvh)->filepos);
         }
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpCodeviewDebugHeader #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpCodeviewDirectoryHeader()
// *****************************
//
void DumpCodeviewDirectoryHeader(IMAGE_CV_DIR_HEADER *cvdh)
{
#ifdef DEBUG
   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("\nCODEVIEW DIRECTORY HEADER:\n");
         msg_log("**************************\n");
         msg_log("   DirHeaderLen          %04X\n",cvdh->cbDirHeader);
         msg_log("   DirEntrySize          %04X\n",cvdh->cbDirEntry);
         msg_log("   DirEntryCount         %08X\n",cvdh->cDir);
         msg_log("   NextDirOffset         %08X\n",cvdh->lfoNextDir);
         msg_log("   StatusFlags           %08X\n",cvdh->flags);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpCodeviewDirectoryHeader #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpCodeViewDirEntry()
// **********************
//
void DumpCodeViewDirEntry(IMAGE_CV_DIR_ENTRY *cvde, ULONG cvhrva)
{
#ifdef DEBUG
   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("%04X  %04X  %08X (%08X)  %08X  ",
                 cvde->SubSection, cvde->iMod, cvde->lfo, (cvhrva + cvde->lfo), cvde->cb);

         switch (cvde->SubSection) {
            case sstModule:
               msg_log("sstModule\n");
               break;
            case sstTypes:
               msg_log("sstTypes\n");
               break;
            case sstPublic:
               msg_log("sstPublic\n");
               break;
            case sstPublicSym:
               msg_log("sstPublicSym\n");
               break;
            case sstSymbols:
               msg_log("sstSymbols\n");
               break;
            case sstAlignSym:
               msg_log("sstAlignSym\n");
               break;
            case sstSrcLnSeg:
               msg_log("sstSrcLnSeg\n");
               break;
            case sstSrcModule:
               msg_log("sstSrcModule\n");
               break;
            case sstLibraries:
               msg_log("sstLibraries\n");
               break;
            case sstGlobalSym:
               msg_log("sstGlobalSym\n");
               break;
            case sstGlobalPub:
               msg_log("sstGlobalPub\n");
               break;
            case sstGlobalTypes:
               msg_log("sstGlobalTypes\n");
               break;
            case sstMPC:
               msg_log("sstMPC\n");
               break;
            case sstSegMap:
               msg_log("sstSegMap\n");
               break;
            case sstSegName:
               msg_log("sstSegName\n");
               break;
            case sstPreComp:
               msg_log("sstPreComp\n");
               break;
            case sstPreCompMap:
               msg_log("sstPreCompMap\n");
               break;
            case sstOffsetMap16:
               msg_log("sstOffsetMap16\n");
               break;
            case sstOffsetMap32:
               msg_log("sstOffsetMap32\n");
               break;
            case sstFileIndex:
               msg_log("sstFileIndex\n");
               break;
            case sstStaticSym:
               msg_log("sstStaticSym\n");
               break;
            default:
               msg_log("*Unknown Section Type*\n");
               break;
         }
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpCodeViewDirEntry #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpCodeViewSymHash()
// *********************
//
void DumpCodeViewSymHash(IMAGE_CV_SYMHASH *cvsh)
{
#ifdef DEBUG
   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("CV_SYMHASH: symhash=%04X addrhash=%04X cbSymbol=%08X cbHSym=%08X cbHAddr=%08X\n",
                 cvsh->symhash, cvsh->addrhash, cvsh->cbSymbol ,cvsh->cbHSym ,cvsh->cbHAddr);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpCodeViewSymHash #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpCodeViewRecordType()
// ************************
//
void DumpCodeViewRecordType(CVSYM *sym, uint recoff)
{
#ifdef DEBUG
   int namelen;
   char symname[256];

   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         if (sym->rectyp == S_PUB32 ||
             sym->rectyp == S_LDATA32 ||
             sym->rectyp ==  S_GDATA32) {

            if (sym->rectyp == S_LDATA32)
               msg_log("\n** S_LDATA32  (%X)\n",recoff);
            else if (sym->rectyp ==  S_GDATA32)
               msg_log("\n** S_GDATA32  (%X)\n",recoff);
            else
               msg_log("\n** S_PUB32  (%X)\n",recoff);

            msg_log("PUBSYM32.off           %08X\n",((PUBSYM32 *)sym)->off);
            msg_log("PUBSYM32.seg           %04X\n",((PUBSYM32 *)sym)->seg);
            msg_log("PUBSYM32.typind        %04X\n",((PUBSYM32 *)sym)->typind);
            namelen = (int)(((PUBSYM32 *)sym)->name[0]);
            if (namelen == 0) {
               msg_log("PUBSYM32.name          *NULL*\n");
            }
            else {
               memcpy(symname, &(((PUBSYM32 *)sym)->name[1]), namelen);
               symname[namelen] = 0;
               msg_log("PUBSYM32.name          %s\n",symname);
            }
         }
         else if (sym->rectyp == S_GPROC32 ||
                  sym->rectyp ==  S_LPROC32) {

            if (sym->rectyp == S_GPROC32)
               msg_log("\n** GPROC32  (%X)\n",recoff);
            else if (sym->rectyp == S_LPROC32)
               msg_log("\n** LPROC32  (%X)\n",recoff);

            msg_log("PROCSYM32.pParent       %08X\n",((PROCSYM32 *)sym)->pParent);
            msg_log("PROCSYM32.pEnd          %08X\n",((PROCSYM32 *)sym)->pEnd);
            msg_log("PROCSYM32.pNext         %08X\n",((PROCSYM32 *)sym)->pNext);
            msg_log("PROCSYM32.len           %08X\n",((PROCSYM32 *)sym)->len);
            msg_log("PROCSYM32.DbgStart      %08X\n",((PROCSYM32 *)sym)->DbgStart);
            msg_log("PROCSYM32.DbgEnd        %08X\n",((PROCSYM32 *)sym)->DbgEnd);
            msg_log("PROCSYM32.typind        %08X\n",((PROCSYM32 *)sym)->typind);
            msg_log("PROCSYM32.off           %08X\n",((PROCSYM32 *)sym)->off);
            msg_log("PROCSYM32.seg           %04X\n",((PROCSYM32 *)sym)->seg);
            msg_log("PROCSYM32.flags         %04X\n",((PROCSYM32 *)sym)->flags);
            namelen = (int)(((PROCSYM32 *)sym)->name[0]);
            memcpy(symname, &(((PROCSYM32 *)sym)->name[1]), namelen);
            symname[namelen] = 0;
            msg_log("PROCSYM32.name          %s\n",symname);
         }
         else if (sym->rectyp == S_LABEL32) {
            msg_log("\n** S_LABEL32  (%X)\n",recoff);

            msg_log("LABELSYM32.off           %08X\n",((LABELSYM32 *)sym)->off);
            msg_log("LABELSYM32.seg           %04X\n",((LABELSYM32 *)sym)->seg);
            msg_log("LABELSYM32.flags         %04X\n",((LABELSYM32 *)sym)->flags);
            namelen = (int)(((LABELSYM32 *)sym)->name[0]);
            memcpy(symname, &(((LABELSYM32 *)sym)->name[1]), namelen);
            symname[namelen] = 0;
            msg_log("LABELSYM32.name          %s\n",symname);
         }
         else if (sym->rectyp == S_ALIGN) {
            msg_log("\n** S_ALIGN  (%X)\n",recoff);

            msg_log("SALIGN.reclen        %04X\n",((SALIGN *)sym)->reclen);
            msg_log("SALIGN.pad           %08X\n",((SALIGN *)sym)->pad);
         }
         else if (sym->rectyp == S_COMPILE) {
            msg_log("\n** S_COMPILE  (%X)\n",recoff);

            msg_log("CFLAGSYM.reclen        %04X\n",((CFLAGSYM *)sym)->reclen);
            msg_log("CFLAGSYM.machine       %02X\n",((CFLAGSYM *)sym)->machine_and_flags.machine);
            msg_log("CFLAGSYM.language      %02X\n",((CFLAGSYM *)sym)->machine_and_flags.language);
   //       msg_log("CFLAGSYM.flags         %04X\n",((CFLAGSYM *)sym)->machine_and_flags & 0xFFFF0000);
            namelen = (int)(((CFLAGSYM *)sym)->ver[0]);
            memcpy(symname, &(((CFLAGSYM *)sym)->ver[1]), namelen);
            symname[namelen] = 0;
            msg_log("CFLAGSYM.ver           %s\n",symname);
         }
         else if (sym->rectyp == S_LINKER) {
            msg_log("\n** S_LINKER  (%X)\n",recoff);

            msg_log("LINKERSYM.reclen        %04X\n",((LINKERSYM *)sym)->reclen);
            msg_log("LINKERSYM.ul1           %08X\n",((LINKERSYM *)sym)->ul1);
            msg_log("LINKERSYM.ul2           %08X\n",((LINKERSYM *)sym)->ul2);
            msg_log("LINKERSYM.ul3           %08X\n",((LINKERSYM *)sym)->ul3);
            msg_log("LINKERSYM.ul4           %08X\n",((LINKERSYM *)sym)->ul4);
            msg_log("LINKERSYM.us1           %04X\n",((LINKERSYM *)sym)->us1);
            namelen = (int)(((LINKERSYM *)sym)->ver[0]);
            memcpy(symname, &(((LINKERSYM *)sym)->ver[1]), namelen);
            symname[namelen] = 0;
            msg_log("LINKERSYM.ver           %s\n",symname);
         }
         else if (sym->rectyp == S_OBJNAME) {
            msg_log("\n** S_OBJNAME  (%X)\n",recoff);

            msg_log("OBJNAMESYM.reclen        %04X\n",((OBJNAMESYM *)sym)->reclen);
            msg_log("OBJNAMESYM.signature     %04X\n",((OBJNAMESYM *)sym)->signature);
            namelen = (int)(((OBJNAMESYM *)sym)->name[0]);
            memcpy(symname, &(((OBJNAMESYM *)sym)->name[1]), namelen);
            symname[namelen] = 0;
            msg_log("OBJNAMESYM.ver           %s\n",symname);
         }
         else if (sym->rectyp == S_SSEARCH) {
            msg_log("\n** S_SSEARCH  (%X)\n",recoff);

            msg_log("SEARCHSYM.reclen        %04X\n",((SEARCHSYM *)sym)->reclen);
            msg_log("SEARCHSYM.startsym      %08X\n",((SEARCHSYM *)sym)->startsym);
            msg_log("SEARCHSYM.seg           %04X\n",((SEARCHSYM *)sym)->seg);
         }
         else if (sym->rectyp == S_NULL) {
            msg_log("\n** S_NULL   filler: %04X  recoff(%X)\n", sym->reclen,recoff);
         }
         else if (sym->rectyp == S_END) {
            msg_log("\n** S_END   reclen: %04X  recoff(%X)\n", sym->reclen,recoff);
         }
         else if (sym->rectyp == S_THUNK32) {
            msg_log("\n** S_THUNK32  (%X)\n",recoff);

            msg_log("THUNKSYM32.pParent       %08X\n",((THUNKSYM32 *)sym)->pParent);
            msg_log("THUNKSYM32.pEnd          %08X\n",((THUNKSYM32 *)sym)->pEnd);
            msg_log("THUNKSYM32.pNext         %08X\n",((THUNKSYM32 *)sym)->pNext);
            msg_log("THUNKSYM32.off           %08X\n",((THUNKSYM32 *)sym)->off);
            msg_log("THUNKSYM32.seg           %04X\n",((THUNKSYM32 *)sym)->seg);
            msg_log("THUNKSYM32.len           %04X\n",((THUNKSYM32 *)sym)->len);
            msg_log("THUNKSYM32.ord           %02X\n",((THUNKSYM32 *)sym)->ord);
            namelen = (int)(((THUNKSYM32 *)sym)->name[0]);
            memcpy(symname, &(((THUNKSYM32 *)sym)->name[1]), namelen);
            symname[namelen] = 0;
            msg_log("THUNKSYM32.name          %s\n",symname);
            if (((THUNKSYM32 *)sym)->ord == 0) {
               msg_log("THUNKSYM32.no_variant_data\n");
            }
            else {
               msg_log("THUNKSYM32.variant_data_present\n");
            }
         }
         else if (sym->rectyp == S_BPREL32) {
            msg_log("\n** S_BPREL32  (%X)\n",recoff);

            msg_log("BPRELSYM32.typind        %08X\n",((BPRELSYM32 *)sym)->typind);
            msg_log("BPRELSYM32.off           %08X\n",((BPRELSYM32 *)sym)->off);
            namelen = (int)(((BPRELSYM32 *)sym)->name[0]);
            memcpy(symname, &(((BPRELSYM32 *)sym)->name[1]), namelen);
            symname[namelen] = 0;
            msg_log("BPRELSYM32          %s\n",symname);
         }
         else if (sym->rectyp == S_UDT) {
            msg_log("\n** S_UDT  (%X)\n",recoff);

            msg_log("UDTSYM.typind        %04X\n",((UDTSYM *)sym)->typind);
            namelen = (int)(((UDTSYM *)sym)->name[0]);
            memcpy(symname, &(((UDTSYM *)sym)->name[1]), namelen);
            symname[namelen] = 0;
            msg_log("UDTSYM.name          %s\n",symname);
         }
         else if (sym->rectyp == S_PROCREF || sym->rectyp == S_DATAREF) {
            if (sym->rectyp == S_PROCREF)
               msg_log("\n** S_PROCREF  (%X)\n",recoff);
            else
               msg_log("\n** S_DATAREF  (%X)\n",recoff);

            msg_log("REFSYM.checksum      %08X\n",((REFSYM *)sym)->checksum);
            msg_log("REFSYM.offsym        %08X\n",((REFSYM *)sym)->offsym);
            msg_log("REFSYM.imod          %04X\n",((REFSYM *)sym)->imod);
         }
         else {
            msg_log("\n** rectyp = %04X  (%X)\n",sym->rectyp,recoff);
         }
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpCodeViewRecordType #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpCoffSmbolsHeader()
// **********************
//
void DumpCoffSymbolsHeader(IMAGE_COFF_SYMBOLS_HEADER *ich)
{
#ifdef DEBUG
   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("\nCOFF_SYMBOLS_HEADER:\n");
         msg_log("********************\n");
         msg_log("   NumberOfSymbols       0x%X (%d)\n",ich->NumberOfSymbols,ich->NumberOfSymbols);
         msg_log("   LvaToFirstSymbol      0x%X\n",ich->LvaToFirstSymbol);
         msg_log("   NumberOfLinenumbers   %d\n",ich->NumberOfLinenumbers);
         msg_log("   LvaToFirstLinenumber  0x%X\n",ich->LvaToFirstLinenumber);
         msg_log("   RvaToFirstByteOfCode  0x%X\n",ich->RvaToFirstByteOfCode);
         msg_log("   RvaToLastByteOfCode   0x%X\n",ich->RvaToLastByteOfCode);
         msg_log("   RvaToFirstByteOfData  0x%X\n",ich->RvaToFirstByteOfData);
         msg_log("   RvaToLastByteOfData   0x%X\n",ich->RvaToLastByteOfData);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpCoffSymbolsHeader #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpCoffSymbol()
// ****************
//
void DumpCoffSymbol(int cnt, char *name, IMAGE_SYMBOL *sym)
{
#ifdef DEBUG
   IMAGE_AUX_SYMBOL *auxsym;                // An Auxiliary symbol
   char FileNameBuf[MAX_PATH_LEN + MAX_FILENAME_LEN]; // Holds file name

   int i, j;


   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log(" %5d  0x%08X     %2d      0x%04X      %d          %2d      %s\n",
                 cnt, sym->Value, sym->SectionNumber, sym->Type,
                 sym->StorageClass, sym->NumberOfAuxSymbols, name);

         if (sym->NumberOfAuxSymbols != 0) {
            auxsym = (IMAGE_AUX_SYMBOL *)sym + 1; // Pointer arithmetic here !!!
            if (sym->Type == 0x0020) {
               msg_log(" ****  TagIndex: %d  TotalSize: %d  PtrToLineNumber: 0x%08x  PtrToNextFunc: 0x%08x\n",
                       auxsym->Sym.TagIndex, auxsym->Sym.Misc.TotalSize,
                       auxsym->Sym.FcnAry.Function.PointerToLinenumber,
                       auxsym->Sym.FcnAry.Function.PointerToNextFunction);
            }
            if (sym->StorageClass == IMAGE_SYM_CLASS_FILE) {
               for (i = 0, j = 0; i < sym->NumberOfAuxSymbols; i++, j += IMAGE_SIZEOF_SYMBOL) {
                  memcpy(&FileNameBuf[j], &(auxsym->File.Name), IMAGE_SIZEOF_SYMBOL);
                  auxsym++;                    // Pointer arithmetic here !!!
               }
               FileNameBuf[j] = 0;             // In case name is exactly 18 bytes
               msg_log(" ****  Filename: %s\n", FileNameBuf);
            }
         }
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpCoffSymbol #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpModuleInfo()
// ****************
//
void DumpModuleInfo(IMAGEHLP_MODULE *im)
{
#ifdef DEBUG
   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("\nIMAGEHLP_MODULE:\n");
         msg_log("****************\n");
         msg_log("   SizeOfStruct   %d\n",im->SizeOfStruct);
         msg_log("   BaseOfImage    0x%p\n",im->BaseOfImage);
         msg_log("   ImageSize      0x%X  (%d)\n",im->ImageSize,im->ImageSize);
         msg_log("   TimeDateStamp  0x%08X => %s",im->TimeDateStamp,TsToDate(im->TimeDateStamp));
         msg_log("   CheckSum       0x%08X\n",im->CheckSum);
         msg_log("   NumSyms        %d\n",im->NumSyms);
         msg_log("   SymType        %d (",im->SymType);
         switch(im->SymType) {
            case SymCoff:
               msg_log("SymCoff)\n");  break;
            case SymCv:
               msg_log("SymCv)\n");  break;
            case SymDeferred:
               msg_log("SymDeferred)\n");  break;
            case SymDia:
               msg_log("SymDia)\n");  break;
            case SymExport:
               msg_log("SymExport)\n");  break;
            case SymNone:
               msg_log("SymNone)\n");  break;
            case SymPdb:
               msg_log("SymPdb)\n");  break;
            case SymSym:
               msg_log("SymSym)\n");  break;
            default:
               msg_log("?????)\n");  break;
         }
         msg_log("   ModuleName       %s\n",im->ModuleName);
         msg_log("   ImageName        %s\n",im->ImageName);
         if (im->LoadedImageName[0] == 0)
            msg_log("   LoadedImageName  ***** NO LOADED IMAGE *****\n");
         else
            msg_log("   LoadedImageName  %s\n",im->LoadedImageName);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpModuleInfo #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpDbgFileHeader()
// *******************
//
void DumpDbgFileHeader(IMAGE_SEPARATE_DEBUG_HEADER *sdh)
{
#ifdef DEBUG
   char *c = (char *)&sdh->Signature;
   char *d = c + 1;

   __try {
      if (G(debug) & DEBUG_MODE_HARVESTER) {
         msg_log("\nIMAGE_SEPARATE_DEBUG_HEADER:\n");
         msg_log("****************************\n");
         msg_log("    Signature            0x%04X (%c%c)\n",sdh->Signature, *c, *d);
         msg_log("    Flags                0x%04X\n",sdh->Flags);
         msg_log("    Machine              0x%04X\n",sdh->Machine);
         msg_log("    Characteristics      0x%04X\n",sdh->Characteristics);
         msg_log("    TimeDateStamp        0x%08X => %s",sdh->TimeDateStamp,TsToDate(sdh->TimeDateStamp));
         msg_log("    CheckSum             0x%08X\n",sdh->CheckSum);
         msg_log("    ImageBase            0x%08X\n",sdh->ImageBase);
         msg_log("    SizeOfImage          0x%08X\n",sdh->SizeOfImage);
         msg_log("    NumberOfSections     0x%08X\n",sdh->NumberOfSections);
         msg_log("    ExportedNamesSize    0x%08X\n",sdh->ExportedNamesSize);
         msg_log("    DebugDirectorySize   0x%08X\n",sdh->DebugDirectorySize);
         msg_log("    SectionAlignment     0x%08X\n",sdh->SectionAlignment);
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n");
      msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpModuleInfo #####\n",_exception_code());
   }
#endif
   return;
}


//
// DumpCode()
// **********
//
void DumpCode(uchar *s, int n)
{
#ifdef DEBUG
   int i, j, bytes, iterations;

   if (G(debug) & DEBUG_MODE_HARVESTER) {
      msg_log("* DumpCode: from=0x%p  for=%d (128 max!)\n",s,n);

      if (s == NULL  ||  n == 0)
         return;                            // NULL code pointer or 0 length

      if (n > 128)
         n = 128;                           // Dump 128 bytes max

      iterations = (n + 15) / 16;
      msg_log("code=");
      __try {
         for (i = 0; i < iterations; i++,n-=16) {
            if (i != 0)
               msg_log("     ");
            bytes = (n < 16) ? n : 16;
            for (j = 0; j < bytes; j++,s++) {
               msg_log("%02x ",*s);
            }
            msg_log("\n");
         }
      }
      __except (EXCEPTION_EXECUTE_HANDLER) {
         // Handle access violations
         msg_log("\n");
         msg_log("**WARNING** ##### EXCEPTION 0x%X in DumpCode at %p #####\n",_exception_code(),s);
      }
   }
#endif
   return;
}
