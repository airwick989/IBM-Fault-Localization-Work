//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                        Windows PDB Symbol Harvester                      //
//                        ----------------------------                      //
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
// Internal prototypes
//
static int HarvestPdbSymbols(IMAGE_CV_HEADER *cvh, CONTEXT_REC *cr);





//
// GetSymbolsFromPdbFile()
// ***********************
//
// Read a PDB file (produced by MS Visual C++) and extract symbols from it.
// Returns number of symbols found or 0 if no symbols or errors.
//
int GetSymbolsFromPdbFile(char *pdbname, uint ts, CONTEXT_REC *cr)
{
   int symcnt;

   dbghv(("> GetSymbolsFromPdbFile: fn='%s' ts=%X  cr=%p\n",pdbname,ts,cr));

   // MS engine expects an image name, not a PDB file name
   symcnt = GetSymbolsUsingMsEngine(cr);

   if (symcnt != 0)
      ((MOD_NODE *)cr->handle)->type = MODULE_TYPE_PDB;     // Set new module type

   dbghv(("< GetSymbolsFromPdbFile: symcnt=%d\n", symcnt));
   return (symcnt);
}


//
// HarvestPdbSymbols()
// *******************
//
// Harvest CodeView symbols from a PDB file.
// As symbols are found a user callback function is called, once per
// symbol, to allow the user to manipulate/catalog the symbols.
//
// HarvestPdbSymbols() returns when:
// - all symbols are enumerated, or
// - the callback function requests it to stop, or
// - an error is encountered
//
// Returns
//   0         No symbols harvested
//   non-zero  Number of symbols harvested
//
static int HarvestPdbSymbols(IMAGE_CV_HEADER *cvh, CONTEXT_REC *cr)
{
   int symcnt = 0;

   dbghv(("> HarvestPdbSymbols: cr=0x%p, CV_HEADER=0x%p\n", cr, cvh));
   errmsg(("*E* HarvestPdbSymbols: ***** YOU SHOULH NOT BE HERE! *****\n"));
   dbghv(("< HarvestPdbSymbols: symcnt=%d\n", symcnt));
   return (symcnt);
}
