//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                     Windows CodeView Symbol Harvester                    //
//                     ---------------------------------                    //
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




//
// HarvestCodeViewSymbols()
// ************************
//
// Harvest CodeView symbols from the CodeView symbol table (either in the image
// or in a DBG file).
// As symbols are found a user callback function is called, once per
// symbol, to allow the user to manipulate/catalog the symbols.
//
// HarvestCodeViewSymbols() returns when:
// - all symbols are enumerated, or
// - the callback function requests it to stop, or
// - an error is encountered
//
// Returns
//   -1        NB10 signature (CV symbols in PDB file)
//   -2        Signature other than NB09 or NB11
//   0         No symbols harvested
//   >0        Number of symbols harvested
//
int HarvestCodeViewSymbols(IMAGE_CV_HEADER *cvh, CONTEXT_REC *cr)
{
   IMAGE_CV_DIR_HEADER *cvdh = NULL;        // CV Subsection Directory Header
   IMAGE_CV_DIR_ENTRY *cvde = NULL;         // CV Directory Entry array
   IMAGE_CV_SYMHASH  *cvsh;                 // CV Symbol Hash table
   CVSYM             *tsym, *sym, *symstart, *symend;     // Generic symbol pointers
   uint recoff;                             // Record offset within CV section

   int  sstModule_cnt = 0;                  // Number of sstModule sections
   int  sstAlignSym_cnt = 0;                // Number of sstAlignSym sections
   CVSYM **SymTbl = NULL;                   // Array of pointers to sstAlignSym sections

   char *symcode = NULL;                    // Symbol code (or NULL if no code)
   uint symoffset;                          // symbol offset
   char symname[512];
   char *pname;
   int ix;
   int namelen;
   ULONG cvhrva;                            // File offset of CV Header

   int i = 0;
   int rc = 0;
   int symcnt = 0;                          // Total symbols kept

   MOD_NODE *mn;


   cvhrva = (ULONG)PtrSub(cvh, dh);
   dbghv(("> HarvestCodeViewSymbols: cr=0x%p, CV_HEADER=0x%p (%X)\n", cr, cvh, cvhrva));
   DumpCodeviewDebugHeader(cvh);

   __try {
      //
      // Check to see if we know how to read this type of CV symbols.
      // We can read:
      // - NB09 and NB11: CodeView debug symbol information
      // - NB10: CodeView symbols in a PDB file
      //
      if (strncmp(cvh->Signature, "NB10", 4) == 0) {  //Win32 PDB signature
         dbghv(("< HarvestCodeViewSymbols: NB10 (PDB) signature.  Nothing done for now.\n"));
         return (-1);                          // PDB file
      }
      if (strncmp(cvh->Signature, "RSDS", 4) == 0) {  //Win64 PDB signature
         dbghv(("< HarvestCodeViewSymbols: RSDS (PDB) signature.  Nothing done for now.\n"));
         return (-1);                          // PDB file
      }

      if ( (strncmp(cvh->Signature, "NB09", 4) != 0) &&
           (strncmp(cvh->Signature, "NB11", 4) != 0) ) {
         dbghv(("< HarvestCodeViewSymbols: unknown CV signature %c%c%c%c.  Nothing done.\n",
                cvh->Signature[0], cvh->Signature[1], cvh->Signature[2], cvh->Signature[3]));
         return (-2);                          // Unknown CV format
      }

      //
      // Calculate address of CV Directory Header and the beginning of the
      // Directoty Entry array.
      //
      cvdh = (IMAGE_CV_DIR_HEADER *)PtrAdd(cvh, (uintptr)(cvh->filepos));
      cvde = (IMAGE_CV_DIR_ENTRY *)PtrAdd(cvdh, (uintptr)(cvdh->cbDirHeader));
      dbghv(("CV_DIR_HEADER=%p (%X)   CV_DIR_ENTRY=%p (%X)\n",cvdh, (ULONG)PtrSub(cvdh, dh), cvde, (ULONG)PtrSub(cvde, dh)));

      DumpCodeviewDirectoryHeader(cvdh);

      if (G(debug) & DEBUG_MODE_HARVESTER) {
         dbghv(("\nCV SUBSECTION DIRECTORY\n"));
         dbghv(("***********************\n"));
         dbghv(("\nsSec  iMod  CVHofset  fileofst   sSecLen\n"));
         dbghv(("----  ----  -------- (--------)  --------\n"));
      }

      //
      // Run thru the directory and:
      // * Count number of sstModule sections
      // * As soon as we hit the first sstAlignSym:
      //   - malloc space to hold pointers to each sstAlignSym section
      //   - for each sstAlignSym section we come across save its address
      // * When we hit the sstGlobalPub section gather symbols that
      //   fall in executable sections (segments).
      // * When we hit the sstStaticSyms gather symbols for functions the
      //   fall in executable sections (segments).
      //
      // Assumptions (for NB09 and NB11):
      // * All sstModule sections are first
      // * All sstAlignSym sections follow (mixed in with sstSrcModule sections)
      // * One sstGlobalPub section follows
      // * One sstStaticSym section follows
      //
      for (i = 0; i < (int)cvdh->cDir; i++) {
         DumpCodeViewDirEntry(cvde, cvhrva);

         if (cvde->SubSection == sstModule) {
            //
            // sstModule
            //
            sstModule_cnt++;
         }
         else if (cvde->SubSection == sstAlignSym) {
            //
            // sstAlignSym
            //
            // ##### If we want we can always dump all symbols in each sstAlignSym
            // ##### section.  That will give us all the labels (if we want them).
            //
            sstAlignSym_cnt++;
            if (sstAlignSym_cnt == 1) {
               dbghv(("-- calloc(%d, %d) for SymTbl\n",(sstModule_cnt + 1), sizeof(CVSYM *)));
               SymTbl = (CVSYM **)calloc((sstModule_cnt + 1), sizeof(CVSYM *));
               if (SymTbl == NULL) {
                  errmsg(("*E* HarvestCodeViewSymbols: Unable to malloc for SymTbl array.\n"));
                  goto TheEnd;                 // No symbols
               }
            }

            //
            // Remember the address of this sstAlignSym section
            //
            SymTbl[cvde->iMod] = (CVSYM *)PtrAdd(cvh, cvde->lfo);
            dbghv(("-- SymTbl[%04X] = %p  (%X)\n",cvde->iMod, SymTbl[cvde->iMod], cvde->lfo));
         }
         if (cvde->SubSection == sstGlobalPub) {
            //
            // sstGlobalPub
            //
            cvsh = (IMAGE_CV_SYMHASH *)PtrAdd(cvh, cvde->lfo);
            DumpCodeViewSymHash(cvsh);

            symstart = (CVSYM *)PtrAdd(cvh, (cvde->lfo + sizeof(IMAGE_CV_SYMHASH)));
            symend = (CVSYM *)PtrAdd(symstart, cvsh->cbSymbol);
            dbghv(("symstart=%p (%X) symend=%p (%X)\n",symstart, (ULONG)PtrSub(symstart, dh), symend, (ULONG)PtrSub(symend, dh)));
            sym = symstart;

            //
            // About the only things we find here are S_PUB32 and S_ALIGN records.
            // Symbol offsets are offsets from the beginning of the segment.
            // Symbols have leading character decoration.
            //
            while (sym < symend) {
               recoff = PtrToUint32((CVSYM *)PtrSub(sym, symstart));
//             DumpCodeViewRecordType(sym, recoff);

               if (sym->rectyp == S_PUB32) {
                  namelen = (int)(((PUBSYM32 *)sym)->name[0]);
                  if (namelen == 0) {
                     symname[0] = 0;
                  }
                  else {
#if defined(_X86)
                     if (((PUBSYM32 *)sym)->name[1] == LEAD_CHAR_US || ((PUBSYM32 *)sym)->name[1] == LEAD_CHAR_AT) {
#else
                     if (((PUBSYM32 *)sym)->name[1] == LEAD_CHAR_DOT) {
#endif
                        ix = 2;
                        namelen--;
                     }
                     else {
                        ix = 1;
                     }

                     memcpy(symname, &(((PUBSYM32 *)sym)->name[ix]), namelen);
                     symname[namelen] = 0;
                  }

                  if (SectionIsExecutable(((PUBSYM32 *)sym)->seg)) {
                     //
                     // Symbol in executable section - get it.
                     //
                     dbghv(("\n## S_PUB32  (%06X)  seg=%04X off=%08X name=%s\n",
                             recoff, ((PUBSYM32 *)sym)->seg, ((PUBSYM32 *)sym)->off, symname));

                     //
                     // Demangle (Undecorate in MS speak) name
                     //
                     if (G(demangle_cpp_names))
                        pname = DemangleName(symname);
                     else
                        pname = symname;

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
                     sec[((PUBSYM32 *)sym)->seg - 1].symcnt++;
                     symoffset = ((PUBSYM32 *)sym)->off + sec[((PUBSYM32 *)sym)->seg - 1].offset; // Calculate absolute offset

                     if (G(gather_code)) {
                        symcode = GetCodePtrForSymbol(symoffset, ((PUBSYM32 *)sym)->seg);
                        DumpCode((uchar *)symcode, 4);  // Don't know size so dump first 4 bytes
                     }

                     //
                     // Send the symbol back to A2N
                     //
                     rc = SendBackSymbol(pname,                  // Symbol name
                                         symoffset,              // Symbol offset
                                         0,                      // Symbol size
                                         SYMBOL_TYPE_FUNCTION,   // Symbol type (label, export, etc.)
                                         ((PUBSYM32 *)sym)->seg, // Section to which the symbol belongs
                                         symcode,                // Code associated with this symbol
                                         (void *)cr);            // Context record
                     if (rc) {
                        dbghv(("< HarvestCodeViewSymbols: SendBackSymbol returned %d. Stopping.\n",rc));
                        goto TheEnd;                    // Destination wants us to stop harvesting
                     }

                     symcnt++;
                  }
                  else {
                     dbghv(("\n** S_PUB32  (%06X)  seg=%04X off=%08X name=%s\n",
                            recoff, ((PUBSYM32 *)sym)->seg, ((PUBSYM32 *)sym)->off, symname));
                  }
               }
               else if (sym->rectyp == S_LDATA32)
                  dbghv(("\n** S_LDATA32  (%06X)\n",recoff));
               else if (sym->rectyp ==  S_GDATA32)
                  dbghv(("\n** S_GDATA32  (%06X)\n",recoff));
               else if (sym->rectyp == S_GPROC32)
                  dbghv(("\n** GPROC32  (%06X)\n",recoff));
               else if (sym->rectyp == S_LPROC32)
                  dbghv(("\n** LPROC32  (%06X)\n",recoff));
               else if (sym->rectyp == S_LABEL32)
                  dbghv(("\n** S_LABEL32  (%06X)\n",recoff));
               else if (sym->rectyp == S_ALIGN) {
                  dbghv(("\n** S_ALIGN  (%06X)\n",recoff));
                  if (*(uint *)(&((SALIGN *)sym)->pad[0]) == 0xffffffff) {
                     break;                    // End of symbols
                  }
               }
               else {
                  dbghv(("\n** rectyp = %04X  (%X)\n",sym->rectyp,recoff));
               }

               sym = (CVSYM *)PtrAdd(sym, (sym->reclen + sizeof(unsigned short)));
            }
            dbghv(("***** Done with sstGlobalPub *****\n"));
         }
         else if (cvde->SubSection == sstStaticSym) {
            //
            // sstStaticSym
            //
            cvsh = (IMAGE_CV_SYMHASH *)PtrAdd(cvh, cvde->lfo);
            DumpCodeViewSymHash(cvsh);

            symstart = (CVSYM *)PtrAdd(cvh, (cvde->lfo + sizeof(IMAGE_CV_SYMHASH)));
            symend = (CVSYM *)PtrAdd(symstart, cvsh->cbSymbol);
            dbghv(("symstart=%p (%X) symend=%p (%X)\n",symstart, (ULONG)PtrSub(symstart, dh), symend, (ULONG)PtrSub(symend, dh)));
            sym = symstart;

            //
            // About the only things we find here are S_PROCREF, S_DATAREF and S_ALIGN records.
            // The only ones we care about are S_PROCREF records.
            // Symbol offsets are offsets from the beginning of the segment.
            // Symbols don't have leading character decoration.
            //
            while (sym < symend) {
               recoff = PtrToUint32((CVSYM *)PtrSub(sym, symstart));
//             DumpCodeViewRecordType(sym, recoff);

               if (sym->rectyp == S_PROCREF) {
                  dbghv(("\n** S_PROCREF  (%06X)  offsym=%08X imod=%04X\n",
                          recoff, ((REFSYM *)sym)->offsym, ((REFSYM *)sym)->imod));

                  tsym = (CVSYM *)PtrAdd(SymTbl[((REFSYM *)sym)->imod], ((REFSYM *)sym)->offsym);
//                DumpCodeViewRecordType(tsym, ((REFSYM *)sym)->offsym);

                  if (tsym->rectyp == S_LPROC32) {
                     if (SectionIsExecutable(((PROCSYM32 *)tsym)->seg)) {
                        namelen = (int)(((PROCSYM32 *)tsym)->name[0]);
                        if (namelen == 0) {
                           symname[0] = 0;
                        }
                        else {
                           memcpy(symname, &(((PROCSYM32 *)tsym)->name[1]), namelen);
                           symname[namelen] = 0;
                        }
                        dbghv(("## S_LPROC32  (%06X)  seg=%04X off=%08X name=%s\n",
                                recoff, ((PROCSYM32 *)tsym)->seg, ((PROCSYM32 *)tsym)->off, symname));

                        //
                        // Demangle (Undecorate in MS speak) name
                        //
                        if (G(demangle_cpp_names))
                           pname = DemangleName(symname);
                        else
                           pname = symname;

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
                        sec[((PROCSYM32 *)tsym)->seg - 1].symcnt++;
                        symoffset = ((PROCSYM32 *)tsym)->off + sec[((PROCSYM32 *)tsym)->seg - 1].offset; // Calculate absolute offset

                        if (G(gather_code)) {
                           symcode = GetCodePtrForSymbol(symoffset, ((PROCSYM32 *)tsym)->seg);
                           DumpCode((uchar *)symcode, 4);  // Don't know size so dump first 4 bytes
                        }

                        //
                        // Send the symbol back to A2N
                        //
                        rc = SendBackSymbol(pname,                    // Symbol name
                                            symoffset,                // Symbol offset
                                            ((PROCSYM32 *)tsym)->len, // Symbol size
                                            SYMBOL_TYPE_FUNCTION,     // Symbol type (label, export, etc.)
                                            ((PROCSYM32 *)tsym)->seg, // Section to which the symbol belongs
                                            symcode,                  // Code associated with this symbol
                                            (void *)cr);              // Context record
                        if (rc) {
                           dbghv(("- HarvestCodeViewSymbols: SendBackSymbol returned %d. Stopping.\n",rc));
                           goto TheEnd;                    // Destination wants us to stop harvesting
                        }

                        symcnt++;
                     }
                  }
               }
               else if (sym->rectyp == S_DATAREF)
                  dbghv(("\n** S_DATAREF  (%06X)\n",recoff));
               else if (sym->rectyp == S_ALIGN) {
                  dbghv(("\n** S_ALIGN  (%06X)\n",recoff));
                  if (*(uint *)(&((SALIGN *)sym)->pad[0]) == 0xffffffff) {
                     break;                    // End of symbols
                  }
               }
               else {
                  dbghv(("\n** rectyp = %04X  (%06X)\n",sym->rectyp,recoff));
               }

               sym = (CVSYM *)PtrAdd(sym, (sym->reclen + sizeof(unsigned short)));
            }
            dbghv(("***** Done with sstStaticSym *****\n"));
         }

         cvde++;           // Pointer arithmetic !!!
      }
   } // __try
   __except (EXCEPTION_EXECUTE_HANDLER) {
      // Handle access violations
      msg_log("\n**WARNING** ##### EXCEPTION 0x%X in HarvestCodeViewSymbols #####\n",_exception_code());
      msg_log("********* %d Symbols gathered up to now will be kept!\n", symcnt);
      goto TheEnd;
   }

   //
   // Done enumerating symbols
   //
TheEnd:
   if (SymTbl)
      free(SymTbl);

   if (symcnt > 0) {
      mn = (MOD_NODE *)cr->handle;
      mn->flags |= A2N_FLAGS_CV_SYMBOLS;
   }

   dbghv(("< HarvestCodeViewSymbols: symcnt=%d\n", symcnt));
   return (symcnt);
}


//
// GetPdbFileNameFromCvHeader()
// ****************************
//
// Caller MUST free() returned string.
//
char *GetPdbFileNameFromCvHeader(IMAGE_CV_HEADER *cvh)
{
   char *fn;


   if (strncmp(cvh->Signature, "RSDS", 4) == 0) {  // VC++ 7 PDB signature
      fn = GetFilenameFromPath((char *)((IMAGE_CV_RSDS_HEADER *)cvh)->Data);
   }
   else if (strncmp(cvh->Signature, "NB10", 4) == 0) {  // VC++ 6 PDB signature
      fn = GetFilenameFromPath((char *)((IMAGE_CV_NB10_HEADER *)cvh)->Data);
   }
   else {
      dbghv(("< GetPdbFileNameFromCvHeader: unknown CV signature %c%c%c%c.  Nothing done.\n",
             cvh->Signature[0], cvh->Signature[1], cvh->Signature[2], cvh->Signature[3]));
      return (NULL);                        // Unknown CV signature

   }

   return (strdup(fn));
}


//
// GetPdbFilePathFromCvHeader()
// ****************************
//
// Caller MUST free() returned string.
//
char *GetPdbFilePathFromCvHeader(IMAGE_CV_HEADER *cvh)
{
   char *fn, *pn;
   char pdb_id[64];


   if (strncmp(cvh->Signature, "RSDS", 4) == 0) {  // VC++ 7 PDB signature
      uchar *id = (uchar *)((IMAGE_CV_RSDS_HEADER *)cvh)->id;
      fn = GetFilenameFromPath((char *)((IMAGE_CV_RSDS_HEADER *)cvh)->Data);
      sprintf(pdb_id,"%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%d",
                     *(ULONG *)id, *(USHORT *)(id+4), *(USHORT *)(id+6),
                     id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15],
                     ((IMAGE_CV_RSDS_HEADER *)cvh)->PDBFileRevision);
   }
   else if (strncmp(cvh->Signature, "NB10", 4) == 0) {  // VC++ 6 PDB signature
      fn = GetFilenameFromPath((char *)((IMAGE_CV_NB10_HEADER *)cvh)->Data);
      sprintf(pdb_id,"%08X%d",
                     ((IMAGE_CV_NB10_HEADER *)cvh)->TimeDateStamp,
                     ((IMAGE_CV_NB10_HEADER *)cvh)->PDBFileRevision);
   }
   else {
      dbghv(("< GetPdbFilePathFromCvHeader: unknown CV signature %c%c%c%c.  Nothing done.\n",
             cvh->Signature[0], cvh->Signature[1], cvh->Signature[2], cvh->Signature[3]));
      return (NULL);                        // Unknown CV signature
   }


   //
   // Path looks like this:  pdbfilename.pdb\pdb_id
   //
   pn = calloc((strlen(fn) + strlen(pdb_id) + 2), 1);
   if (pn) {
      strcpy(pn, fn);
      strcat(pn, "\\");
      strcat(pn, pdb_id);
   }

   return (pn);
}
