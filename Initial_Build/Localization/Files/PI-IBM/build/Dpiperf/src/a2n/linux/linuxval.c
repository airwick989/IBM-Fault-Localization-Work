/*   Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
//**************************************************************************//
//                                                                          //
//                      Linux Kernel Symbols Validation                     //
//                      -------------------------------                     //
//                                                                          //
//**************************************************************************//
//
#include "a2nhdr.h"




//
// Externs (from a2n.c)
//
extern gv_t a2n_gv;                         // Globals



//
// MACRO to update the symbol pointer (along the harvested symbols)
// to the next symbols to be checked.
// The pointer is not moved if the symbol to which it is pointing
// contains aliases or contains other symbols.  That's because the
// next ksym could be one that matches the parent, an alias of a
// contained symbol of the parent.
//
#define NEXT_SYMBOL_UNLESS_SUBSYMBOLS(s)                \
   if (sn->aliases == NULL && sn->contained == NULL) {  \
      if (!using_symbol_array)                          \
         s = s->next;                                   \
      else {                                            \
         if (++scnt < lmn->mn->base_symcnt)             \
            s = sn_array[scnt];                         \
         else                                           \
            s = NULL;                                   \
      }                                                 \
   }


//
// MACRO to update the symbol pointer (along the harvested symbols)
// to the next symbols to be checked.
// The pointer is moved regardless of whether the symbol to which
// it is pointing contains aliases or not.
//
#define NEXT_SYMBOL(s)                        \
   if (!using_symbol_array)                   \
      s = s->next;                            \
   else {                                     \
      if (++scnt < lmn->mn->base_symcnt)      \
         s = sn_array[scnt];                  \
      else                                    \
         s = NULL;                            \
   }







//
// compare()
// *********
//
// Comparator function for qsort.
//
int compare(const void * e1, const void * e2)
{
   if (((struct ksym *)e1)->addr == ((struct ksym *)e2)->addr)
      return (0);     // e1 == e2
   else if (((struct ksym *)e1)->addr < ((struct ksym *)e2)->addr)
      return (-1);    // e1 < e2
   else
      return (1);     // e1 > e2
}


//
// names_match()
// *************
//
int names_match(char * ksym_name, char * sym_name)
{
   if (strncmp(ksym_name, sym_name, strlen(sym_name)) == 0)
      return (1);                           // Exact match

   //
   // Some symbols are prefixed by things like "GPLONLY_"
   // so if our symbol name is contained in the ksym then assume it's
   // one of those oddball ones.
   //
   if (strncmp(ksym_name, "GPLONLY_", 8) == 0) {
      if (strstr(ksym_name, sym_name) != NULL)
         return (1);                       // Close enough
   }

   //
   // Some symbols are enclosed by things like "__VERSIONED_SYMBOL()"
   // so if our symbol name is contained in the ksym then assume it's
   // one of those oddball ones.
   //
   if (strncmp(ksym_name, "__VERSIONED_SYMBOL", 18) == 0) {
      if (strstr(ksym_name, sym_name) != NULL)
         return (1);                       // Close enough
   }

   return (0);                             // Don't match
}


//
// ksym_matches_alias()
// ********************
//
// Takes given ksym and check if it matches any alias. All aliases have
// the same starting virtual address so we only need to calculate the
// address once and then compare the ksym name to all the aliases.
//
int ksym_matches_alias(struct ksym * ks, SYM_NODE * alias_list, LMOD_NODE * lmn)
{
   SYM_NODE * asn;
   uint64 addr;

   if (alias_list == NULL)
      return (0);                           // No aliases

   asn = alias_list;
   addr = asn->offset_start + lmn->start_addr;  // Doesn't change - it's an alias
   while (asn) {
      dbghv(("VKS: ksym[%d]=(%"_LZ64X")%s   alias_sym=(%"_LZ64X")%s\n",
             ks->index, ks->addr, ks->name, addr, asn->name));
      if (names_match(ks->name, asn->name))
         return (1);                        // Symbol addresses and names match
      else
         asn = asn->next;                   // No match, keep looking
   }

   return (0);
}


//
// ksym_matches_contained()
// ************************
//
// Takes given ksym and check if it matches any subsymbols. Subsymbols have
// different starting virtual address (than the parent symbol) so we need to
// calculate the address for each subsymbol and then compare the ksym name
// to the subsymbol.
//
int ksym_matches_contained(struct ksym * ks, SYM_NODE * contained_list, LMOD_NODE * lmn)
{
   SYM_NODE * csn;
   uint64 caddr;

   if (contained_list == NULL)
      return (0);                           // No contained symbols

   csn = contained_list;
   while (csn) {
      caddr = csn->offset_start + lmn->start_addr;
      dbghv(("VKS: ksym[%d]=(%"_LZ64X")%s   contained_sym=(%"_LZ64X")%s\n",
             ks->index, ks->addr, ks->name, caddr, csn->name));
      if (ks->addr == caddr && names_match(ks->name, csn->name))
         return (1);                        // Symbol addresses and names match
      else
         csn = csn->next;                   // No match, keep looking
   }

   return (0);
}


//
// ValidateKernelSymbols()
// ***********************
//
// Reads /proc/ksyms and makes sure all kernel symbols in that file
// match the ones we harvested (from the map or the image itself).
// If symbols don't match then the entire symbol node chain is
// deleted (we're just throwing away the symbols we gathered).
// If we have problems (like malloc errors, etc.) we leave symbols
// alone.
//
// NOTES:
// - argument is really an LMOD_NODE * cast as a void *
// - I put this function here because it's Linux specific, although
//   it's not a "harvester" function per se.
//
// If validation fails:
// - mn.symcnt set to 0
// - bit A2N_FLAGS_VALIDATION_FAILED set in mn.flags
// - entire symbol node chain is freed.  mn.symroot set to NULL
//
// If we run into problems (malloc, can't open files, etc):
// - bit A2N_FLAGS_VALIDATION_NOT_DONE set in mn.flags
//
void ValidateKernelSymbols(void * vlmn)
{
   char * ksyms_fn    = DEFAULT_KSYMS_FILENAME;
   char * kallsyms_fn = DEFAULT_KALLSYMS_FILENAME;
   int use_ksyms = 1;                       // 1=ksyms, 0=kallsyms

   FILE * fh = NULL;
   char line[512], sym[256];
   char * c;
   uint64 symaddr, symeaddr;
   uint64 start_addr = 0, end_addr = 0;

   struct ksym * ks = NULL;
   int i, scnt, using_symbol_array, kcnt = 0;
   int validation_failed = 1;               // Assume the worst

   LMOD_NODE * lmn;
   MOD_NODE * mn;
   SYM_NODE * sn;
   SYM_NODE ** sn_array;

   lmn = (LMOD_NODE *)vlmn;
   mn = lmn->mn;
   dbghv(("> ValidateKernelSymbols: lmn=%p  mn=%p\n", lmn, mn));
   //
   // Nothing to do if no symbols and/or no sections.
   //
   if (G(validate_symbols) == 0) {
      dbghv(("- ValidateKernelSymbols: validate_symbols = 0\n"));
      validation_failed = 0;
      goto TheEnd;                          // User doesn't want validation
   }
   if (mn->symcnt == 0) {
      dbghv(("- ValidateKernelSymbols: mn->symcnt = 0\n"));
      validation_failed = 0;
      goto TheEnd;                          // No symbols - nothing to do
   }

   //
   // Calculate bounds for address range to check for in ksyms.
   // Start at the module's starting virtual and end at the start of the symbol with
   // the highest offset.
   //
   sn_array = mn->symroot;
   start_addr = lmn->start_addr;      // Loaded module start address
#if defined(_PPC)
   end_addr = sn_array[mn->base_symcnt - 1]->offset_start + start_addr;
#else
   end_addr = lmn->length + start_addr;
#endif

   dbghv(("- ValidateKernelSymbols: start_addr=%"_LZ64X", end_addr=%"_LZ64X"\n", start_addr, end_addr));

   //
   // Allocate and clear the KS array.
   //
   ks = (struct ksym *)zmalloc(MAX_KSYMS * sizeof(struct ksym));
   if (ks == NULL) {
      errmsg(("*E* ValidateKernelSymbols: VALIDATION NOT DONE - unable to malloc() for reading /proc/[ksyms|kallsyms]\n"));

      msg_se("\n******************* KERNEL SYMBOL VALIDATION FAILED *******************\n");
      msg_se("  Unable to malloc() for reading /proc/[ksyms|kallsyms]. This is really bad!\n");
      msg_se("******************* KERNEL SYMBOL VALIDATION FAILED *******************\n\n");
      goto TheEnd;
   }
   memset(ks, 0, MAX_KSYMS * sizeof(struct ksym));

   //
   // Open the /proc/ksyms or /proc/kallsyms file
   //
   fh = fopen(ksyms_fn, "r");
   if (fh == NULL ) {
      warnmsg(("*W* ValidateKernelSymbols: unable to open '%s'. Trying '%s' ...\n", ksyms_fn, kallsyms_fn));
      fh = fopen(kallsyms_fn, "r");
      if (fh == NULL ) {
         warnmsg(("*W* ValidateKernelSymbols: unable to open file '%s'.\n", kallsyms_fn));
         errmsg(("*E* ValidateKernelSymbols: VALIDATION NOT DONE - unable to open /proc/[ksyms|kallsyms] file.\n"));
         msg_se("\n******************* KERNEL SYMBOL VALIDATION FAILED *******************\n");
         msg_se("  Unable to open %s or %s for reading. This is really bad!\n", ksyms_fn, kallsyms_fn);
         msg_se("******************* KERNEL SYMBOL VALIDATION FAILED *******************\n\n");
         goto TheEnd;
      }
      use_ksyms = 0;
   }

   //
   // Read ksyms file and only keep up to the first MAX_KSYMS symbols that
   // fall in the address range of the symbols we've gathered. Symbols are not
   // in any particular order.
   //
   kcnt = 0;
   while ((c = fgets(line, 512, fh)) != NULL) {
      if (kcnt == MAX_KSYMS)
         break;                             // Already have max

      if (use_ksyms) {
         // c01018be inflate_block                             <--- kernel
         // e09f50f5 snd_mixer_oss_release [snd_mixer_oss]     <--- module
         sscanf(line, "%"_L64x" %s\n", &ks[kcnt].addr, sym);
      }
      else {
         // c01018be t inflate_block                           <--- kernel
         // e09f50f5 t snd_mixer_oss_release [snd_mixer_oss]   <--- module
         sscanf(line, "%"_L64x" %*s %s\n", &ks[kcnt].addr, sym);
      }

      if (ks[kcnt].addr < start_addr  ||  ks[kcnt].addr > end_addr) {
         dbghv(("- %4d Rejecting %"_LZ64X" %s\n", kcnt, ks[kcnt].addr, sym));
         continue;                          // Not in addr range we want
      }
      dbghv(("- %4d Keeping %"_LZ64X" %s\n", kcnt, ks[kcnt].addr, sym));
      ks[kcnt].name = zstrdup(sym);
      if  (ks[kcnt].name == NULL) {
         errmsg(("*E* ValidateKernelSymbols: VALIDATION NOT DONE - unable to malloc() for ksym name #%d\n", kcnt));

         msg_se("\n******************* KERNEL SYMBOL VALIDATION FAILED *******************\n");
         msg_se("  Unable to malloc() for reading /proc/[ksyms|kallsyms] kernel symbol.\n");
         msg_se("  The symbol number is %d and the name is '%s'.\n", kcnt, sym);
         msg_se("******************* KERNEL SYMBOL VALIDATION FAILED *******************\n\n");
         goto TheEnd;
      }
      ks[kcnt].index = kcnt;
      kcnt++;
   }
   if (kcnt == 0) {
      errmsg(("*W* ValidateKernelSymbols: VALIDATION NOT DONE - 0 ksyms found in kernel address range.\n"));

      msg_se("\n******************* KERNEL SYMBOL VALIDATION FAILED *******************\n");
      msg_se("  There were no ksyms found in the address range for the kernel.\n");
      msg_se("  We looked from %"_L64X" to %"_L64X"\n", start_addr, end_addr);
      msg_se("******************* KERNEL SYMBOL VALIDATION FAILED *******************\n\n");
      goto TheEnd;
   }

   //
   // Sort the ksyms in ascending address order.
   //
   qsort((void *)ks, kcnt, sizeof(struct ksym), compare);

   //
   // Start validating ...
   // What I'm trying to do is to make sure every symbol we decided to keep
   // from /proc/ksyms or /proc/kallsyms exists (same address and same name) in
   // the set of kernel symbols gathered.
   //
   if (mn->flags & A2N_SYMBOL_ARRAY_BUILT)
      using_symbol_array = 1;
   else
      using_symbol_array = 0;

   scnt = 0;
   sn = sn_array[scnt];
   for (i = 0; i < kcnt; i++) {
      if (sn == NULL) {
         //
         // Looked thru all harvested symbols but still not finished looking
         // at ksyms. Kernels can't possibly match.
         //
         errmsg(("*E* ValidateKernelSymbols: *** KERNEL SYMBOL VALIDATION FAILED ***\n"));
         errmsg(("*** Could not validate all symbols in /proc/[ksyms|kallsyms] and '%s'\n", mn->hvname));

         msg_se("\n******************* KERNEL SYMBOL VALIDATION FAILED *******************\n");
         msg_se("  Looked through all harvested symbols but not finished looking at ksyms.\n");
         msg_se("  Kernels can't possibly match.\n");
         msg_se("******************* KERNEL SYMBOL VALIDATION FAILED *******************\n\n");
         goto TheEnd;
      }

      //
      // Toss symbol "Using_Versions" 'cause we'll never find it!
      //
      if (strcmp(ks[i].name, "Using_Versions") == 0)
         continue;

      //
      // March down the kernel symbols until we match or until we go past the
      // address of the ksym we're standing on.
      //
      while (sn) {
         symaddr = sn->offset_start + lmn->start_addr;
         symeaddr = sn->offset_end + lmn->start_addr;
         dbghv(("VKS: ksym[%d]=(%"_LZ64X")%s   sym=(%"_LZ64X" - %"_LZ64X")%s\n",
                i, ks[i].addr, ks[i].name, symaddr, symeaddr, sn->name));

         if (ks[i].addr >= symaddr && ks[i].addr <= symeaddr) {
            //
            // ksym is in the range for the current symbol ...
            // That means we either match it to something (parent, alias or
            // subsymbol) or we fail.
            //
            if (ks[i].addr == symaddr) {
               // ksym address matches the parent symbol. Now match on name ...
               if (names_match(ks[i].name, sn->name)) {
                  NEXT_SYMBOL_UNLESS_SUBSYMBOLS(sn);
                  break;                    // On to the next ksym
               }

               // Names didn't match so only other possibility is that the
               // ksym could be an alias of the parent.
               if (ksym_matches_alias(&ks[i], sn->aliases, lmn)) {
                  // ksym matches an alias. That's good.
                  // *** Don't move sn because it's possible that the next ksym
                  // *** could be a match on the parent symbol or another alias.
                  break;                    // On to the next ksym
               }

               // ****
               // Can't be anything else:
               // - ksym has same address as parent (or alias-less) symbol
               // - ksym name doesn't match parent
               // - If parent has aliases, ksym name doesn't match any alias
               // This ksym doesn't match anything.
               // ****
            }
            else {
               // ksym address doesn't match parent but is within the address
               // range for the parent.
               // - It can't be an alias because, otherwise, the address would have
               //   matched that of the parent, and that didn't happen
               // - If there are contained symbols then the ksym could be one of them ...
               if (ksym_matches_contained(&ks[i], sn->contained, lmn)) {
                  // ksym matches a subsymbol. That's good.
                  // *** Don't move sn because it's possible that the next ksym
                  // *** could be a match on the parent symbol, an alias or another
                  // *** contained symbol.
                  break;                    // On to the next ksym
               }

               // ****
               // Can't be anything else:
               // - ksym is in the address range of the parent symbol but is not the parent
               // - Parent either doesn't have subsymbols OR it does and the
               //   ksym doesn't match any of them.
               // This ksym doesn't match anything.
               // ****
            }

            //
            // OK, here's the deal. If we make it here it means that the ksym (ks[i])
            // is in the address range of the kernel symbol (sn) but either its
            // address doesn't match the symbol itself nor any aliases (if any) nor
            // is it contained in the kernel symbol.
            // In other words, we're done.
            //
            warnmsg(("*W* ValidateKernelSymbols: *** KERNEL SYMBOL VALIDATION FAILED ***\n"));
            warnmsg(("*** ksym '%s' does not match '%s' at %"_LZ64X" of '%s'\n",
                     ks[i].name, sn->name, ks[i].addr, mn->hvname));

            msg_se("\n******************* KERNEL SYMBOL VALIDATION FAILED *******************\n");
            msg_se("  ksym '%s' does not match '%s' at %"_L64X" of '%s'\n",
                   ks[i].name, sn->name, ks[i].addr, mn->hvname);
            msg_se("******************* KERNEL SYMBOL VALIDATION FAILED *******************\n\n");
            goto TheEnd;
         }
         else {
            //
            // ksym not in the address range of this symbol ...
            // Bail out if the symbol addr (harvested) is larger than the
            // ksym we're looking at. We're never going to match.
            //
            if (symaddr > ks[i].addr) {
               warnmsg(("*W* ValidateKernelSymbols: *** KERNEL SYMBOL VALIDATION FAILED ***\n"));
               warnmsg(("*** ksym '%s' at %"_LZ64X" does not match '%s' at %"_LZ64X" of '%s' (* already larger address *)\n",
                        ks[i].name, ks[i].addr, sn->name, symaddr, mn->hvname));

               msg_se("\n******************* KERNEL SYMBOL VALIDATION FAILED *******************\n");
               msg_se("  ksym '%s' at %"_L64X" does not match '%s' at %"_L64X" of '%s'\n",
                      ks[i].name, ks[i].addr, sn->name, symaddr, mn->hvname);
               msg_se("  Symbols already at larger address than ksyms.\n");
               msg_se("******************* KERNEL SYMBOL VALIDATION FAILED *******************\n\n");
               goto TheEnd;
            }

            //
            // Make sure we're not looking past the end of the largest symbol address we have.
            //
            if (symaddr > end_addr) {       // looking past the end. done
               errmsg(("*E* ValidateKernelSymbols: *** KERNEL SYMBOL VALIDATION FAILED ***\n"));
               errmsg(("*** ksym '%s' at %"_LZ64X" does not match any symbol from '%s'\n",
                       ks[i].name, ks[i].addr, mn->hvname));

               msg_se("\n******************* KERNEL SYMBOL VALIDATION FAILED *******************\n");
               msg_se("  ksym '%s' at %"_LZ64X" does not match any kernel symbol from '%s'\n",
                      ks[i].name, ks[i].addr, mn->hvname);
               msg_se("******************* KERNEL SYMBOL VALIDATION FAILED *******************\n\n");
               goto TheEnd;
            }
         }

         //
         // Address didn't match and we're not off the end of the symbol node chain.
         // Keep looking.
         //
         NEXT_SYMBOL(sn);
      } // while
   } // for i

   validation_failed = 0;                   // Validation didn't fail. I guessed wrong.

   //
   // We can get here one of 2 ways:
   // 1) Fall out of the for loop, which means we looked at every single
   //    ksym and were able to match up each one with one of the symbols we
   //    harvested.  That's goodness.
   // 2) Target of a goto, which means there was an error or that one of
   //    the ksyms didn't match a harvested symbol.  That's badness.
   //
TheEnd:
   if (validation_failed) {
      //
      // Validation failed 'cause ksyms and harvested symbols didn't match.
      // Keep the symbols around so we can see what we actually harvested
      // (if they use A2nDumpModules()).
      //
      mn->flags |= A2N_FLAGS_VALIDATION_FAILED;  // Module validation failed
   }

   //
   // Free up memory for the KS array
   //
   if (ks) {
      for (i = 0; i < kcnt; i++) {
         zfree(ks[i].name);                 // Free all the names first
      }
      zfree(ks);                            // Free the array last
   }

   //
   // Close /proc/ksyms file if open
   //
   if (fh != NULL)
      fclose(fh);

   //
   // Sayonara
   //
   dbghv(("< ValidateKernelSymbols: Done\n"));
   return;
} // ValidateKernelSymbols()
