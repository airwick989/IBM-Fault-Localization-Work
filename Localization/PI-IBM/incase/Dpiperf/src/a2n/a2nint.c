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

// Globals
#if defined(_WINDOWS)
   #include <psapi.h>
   CRITICAL_SECTION harvester_cs;
#endif

#if defined(_LINUX)
   #include <dirent.h>
   pthread_mutex_t harvester_mutex;
#endif


//
// Externs (declared in a2n.c)
// ***************************
//
extern gv_t a2n_gv;                         // Globals
extern SYM_NODE ** snp;                     // SN pointer array
extern SYM_NODE ** dsnp;                    // SN pointer array for checking dups
extern int kernel_symbols_choice_made;
// A2nGetSymbolEx global stuff
extern PID_NODE * ex_pn;
extern PID_NODE * ex_opn;
extern unsigned int ex_pid;



//****************************************************************************//
// My naming convention for node pointers is as follows:                      //
// - pn    Pid Node                                                           //
// - opn   Owning Pid Node                                                    //
// - lmn   Loaded Module Node                                                 //
// - mn    Module Node                                                        //
// - jmn   Jitted Method Node                                                 //
// - sn    Symbol Node                                                        //
// - cn    Section Node (sometimes also use secn)                             //
//****************************************************************************//



//##############################################################################
//###    CODE  CODE  CODE  CODE  CODE  CODE  CODE  CODE  CODE  CODE  CODE    ###
//##############################################################################


//
// make_kernel_symbols_choice()
// ****************************
//
// Take a stab at where to get kernel and kernel modules symbols from.
// It's all good if the user has already made the choice and we assume
// they know what they're doing. Otherwise, we make a choice and it is
// what it is.
//
// #####
// ##### Right now we do this the first time we're looking for a loaded
// ##### module (ie. the first A2nGetSymbol()) or the first time we go
// ##### harvest symbols after loading a module (immediate gather mode).
// ##### We should probably do it anytime an API changes one of the
// ##### choices, but we don't.
// #####
//
// For kernel symbols:
// -------------------
// * If user indicates no preference then we choose as follows:
//   - If /proc/kallsyms is readable set use_kallayms
//   - If /boot/System.map is readable set use_kernel_map
//   - If /usr/src/linux/vmlinux is readable set use_kernel_image
// * If user indicates preference(s) then OK
//
// For kernel modules (device driver) symbols:
// -------------------------------------------
// * If user indicates no preference then:
//   - If we chose/or were told to use kallsyms for kernel symbols, and the
//     /proc/modules file is readable, then set use_modules
// * If user indicates preference(s) then OK
//
void make_kernel_symbols_choice(void)
{
   dbgmsg(("> make_kernel_symbols_choice: start\n"));

#if defined(_LINUX)
   // If no choice made for where to get kernel symbols, make it now
   if (G(use_kernel_image) == 0 && G(use_kernel_map) == 0 && G(use_kallsyms) == 0) {
      if (FileIsReadable(G(KallsymsFilename))) {
         dbgmsg(("- make_kernel_symbols_choice: *** no kernel choice and kallsyms '%s' exists. Try it. ***\n",
                 G(KallsymsFilename)));
         G(use_kallsyms) = 1;
      }
      if (FileIsReadable(G(KernelMapName))) {
         dbgmsg(("- make_kernel_symbols_choice: *** no kernel choice and map '%s' exists. Try it. ***\n",
                 G(KernelMapName)));
         G(use_kernel_map) = 1;
      }
      if (FileIsReadable(G(KernelImageName))) {
         dbgmsg(("- make_kernel_symbols_choice: *** no kernel choice and image '%s' exists. Try it. ***\n",
                 G(KernelImageName)));
         G(use_kernel_image) = 1;
      }
      if (G(use_kernel_image) == 0 && G(use_kernel_map) == 0 && G(use_kallsyms) == 0) {
         dbgmsg(("< make_kernel_symbols_choice: *** no kernel choice and no A2N choice. Not good! ***\n"));
      }
   }
   else {
      dbgmsg(("- make_kernel_symbols_choice: *** kernel choice(s): kallsyms=%d, map=%d, image=%d ***\n",
                 G(use_kallsyms), G(use_kernel_map), G(use_kernel_image)));
   }

   // If no choice made for where to get kernel module symbols, make it now
   if (G(use_modules) == 0) {
      if (G(use_kallsyms)) {
         if (FileIsReadable(G(ModulesFilename))) {
            dbgmsg(("- make_kernel_symbols_choice: *** no modules choice and modules '%s' exists. Try it. ***\n",
                    G(ModulesFilename)));
            G(use_modules) = 1;
         }
      }
      else {
         dbgmsg(("- make_kernel_symbols_choice: *** no modules choice and no A2N choice. No modules. ***\n"));
      }
   }
   else {
      dbgmsg(("- make_kernel_symbols_choice: *** user choice for modules is '%s' ***\n", G(ModulesFilename)));
   }

   // If we haven't added the kernel modules, do it now
   if (G(use_modules) && !G(kernel_modules_added))
      AddKernelModules();
#endif

   kernel_symbols_choice_made = 1;
   dbgmsg(("< make_kernel_symbols_choice: done\n"));
   return;
}


//
// check_if_good_filename()
// ************************
//
int check_if_good_filename(char * fn)
{
   if (fn == NULL  ||  *fn == '\0') {       // Make sure filename is not NULL
      errmsg(("*E* check_if_good_filename: invalid file name - NULL\n"));
      return (A2N_INVALID_PATH);
   }
   if (!FileIsReadable(fn)) {               // Make sure file exists and we have permission to read it
      errmsg(("*E* check_if_good_filename: %s not found or not accessible.\n", fn));
      return (A2N_FILE_NOT_FOUND_OR_CANT_READ);   // File not found
   }
   return (0);
}


//
// verify_kernel_image_name()
// **************************
//
int verify_kernel_image_name(char * fn)
{
   int rc = 0;

#if defined (_LINUX)
   dbgapi(("\n>> verify_kernel_image_name: fn='%s'\n", fn));

   rc = check_if_good_filename(fn);
   if (rc == 0) {
      zfree(G(KernelImageName));
      G(KernelImageName) = zstrdup(fn);
      if (G(KernelImageName) == NULL) {
         errmsg(("*E* verify_kernel_image_name: unable to allocate memory for image name. Quitting.\n"));
         exit (A2N_MALLOC_ERROR);
      }
      G(use_kernel_image) = 1;
   }

   dbgapi(("<< verify_kernel_image_name: rc = %d\n", rc));
#endif

   return (rc);
}


//
// verify_kernel_map_name()
// ************************
//
int verify_kernel_map_name(char * fn)
{
   int rc = 0;

#if defined (_LINUX)
   dbgmsg(("\n>> verify_kernel_map_name: fn='%s'\n", fn));

   rc = check_if_good_filename(fn);
   if (rc == 0) {
      zfree(G(KernelMapName));
      G(KernelMapName) = zstrdup(fn);
      if (G(KernelMapName) == NULL) {
         errmsg(("*E* verify_kernel_map_name: unable to allocate memory for map file name. Quitting.\n"));
         exit (A2N_MALLOC_ERROR);
      }
      G(use_kernel_map) = 1;
   }

   dbgmsg(("<< verify_kernel_map_name: rc = %d\n", rc));
#endif

   return (rc);
}


//
// check_if_file_usable()
// **********************
//
int check_if_file_usable(char * fn, int64_t offset, int64_t size)
{
#if defined(_LINUX)
   struct stat s;

   if (stat(fn, &s) < 0) {
      errmsg(("*E* check_if_file_usable: stat() failed for '%s'. errno=%d\n", fn, errno));
      return (A2N_FILE_NOT_RIGHT);
   }
   if (offset && s.st_size < offset) {
      errmsg(("*E* check_if_file_usable: given offset (%"_L64d") exceeds file size (%"_L64d") for '%s'\n", offset, s.st_size, fn));
      return (A2N_FILE_NOT_RIGHT);
   }
   if (size && s.st_size < size) {
      errmsg(("*E* check_if_file_usable: given size (%"_L64d") exceeds file size (%"_L64d") for '%s'\n", size, s.st_size, fn));
      return (A2N_FILE_NOT_RIGHT);
   }
#endif

   return (0);
}


//
// verify_kallsyms_file_info()
// ***************************
//
// size = 0 implies entire file, or to EOF from offset.
//
int verify_kallsyms_file_info(char * fn, int64_t offset, int64_t size)
{
   int rc = 0;

#if defined (_LINUX)
   dbgmsg(("\n>> verify_kallsyms_file_info: fn='%s' offset=%"_L64d" size=%"_L64d"\n", fn, offset, size));

   rc = check_if_good_filename(fn);
   if (rc != 0) {
      dbgmsg(("<< verify_kallsyms_file_info: rc = %d\n", rc));
      return (rc);
   }
   rc = check_if_file_usable(fn, offset, size);
   if (rc != 0) {
      dbgmsg(("<< verify_kallsyms_file_info: rc = %d\n", rc));
      return (rc);
   }

   // OK, remember kallsyms name
   zfree(G(KallsymsFilename));
   G(KallsymsFilename) = zstrdup(fn);
   if (G(KallsymsFilename) == NULL) {
      errmsg(("*E* verify_kallsyms_file_info: unable to allocate memory for map file name. Quitting.\n"));
      exit (A2N_MALLOC_ERROR);
   }
   G(kallsyms_file_offset) = offset;
   G(kallsyms_file_size) = size;
   G(use_kallsyms) = 1;

   dbgmsg(("<< verify_kallsyms_file_info: rc = %d\n", rc));
#endif

   return (rc);
}


//
// verify_modules_file_info()
// **************************
//
int verify_modules_file_info(char * fn, int64_t offset, int64_t size)
{
   int rc = 0;

#if defined (_LINUX)
   dbgmsg(("\n>> verify_modules_file_info: fn='%s' offset=%"_L64d" size=%"_L64d"\n", fn, offset, size));

   rc = check_if_good_filename(fn);
   if (rc != 0) {
      dbgmsg(("<< verify_modules_file_info: rc = %d\n", rc));
      return (rc);
   }
   rc = check_if_file_usable(fn, offset, size);
   if (rc != 0) {
      dbgmsg(("<< verify_kallsyms_file_info: rc = %d\n", rc));
      return (rc);
   }

   // OK, remember modules file name
   zfree(G(ModulesFilename));
   G(ModulesFilename) = zstrdup(fn);
   if (G(ModulesFilename) == NULL) {
      errmsg(("*E* verify_modules_file_info: unable to allocate memory for map file name. Quitting.\n"));
      exit (A2N_MALLOC_ERROR);
   }
   G(modules_file_offset) = offset;
   G(modules_file_size) = size;
   G(use_modules) = 1;

   dbgmsg(("<< verify_modules_file_info: rc = %d\n", rc));
#endif

   return (rc);
}


//
// ChangeBlanksToSomethingElse()
// *****************************
//
void ChangeBlanksToSomethingElse(char * s)
{
   for ( ; *s != '\0'; s++) {
      if (*s == ' ')
         *s = G(blank_sub);
   }
   return;
}


//
// hash_pid()
// **********
//
// Calculate hash value for a pid
//
uint hash_pid(uint pid)
{
   return (((pid + 13) * 31) % PID_HASH_TABLE_SIZE);
}


//
// pid_hash_add()
// **************
//
// Add PID_NODE to pid node hash table and pid node list
//
void pid_hash_add(PID_NODE * pn)
{
   uint hv = hash_pid(pn->pid);

   // Add node to pid hash table
   if (G(PidTable[hv]) != NULL)
      pn->hash_next = G(PidTable[hv]);      // Not first node on list
   else
      pn->hash_next = NULL;                 // First and only node

   G(PidTable[hv]) = pn;

   // Add node to pid list
   pn->next = NULL;
   if (G(PidRoot) == NULL)
      G(PidRoot) = G(PidEnd) = pn;          // First node (also the last)
   else {
      G(PidEnd)->next = pn;                 // Add at the end
      G(PidEnd) = pn;                       // New end
   }

   // Done
   return;
}

#if defined(_WINDOWS) || defined(_LINUX)
//
// enter_critical_section()
// ************************
//
void enter_critical_section(void)
{
   dbgmsg((">> enter_critical_section() on tid=%d\n", GET_TID()));
#if defined(_WINDOWS)
   EnterCriticalSection(&harvester_cs);
#elif defined(_LINUX)
   pthread_mutex_lock(&harvester_mutex);
#endif
   dbgmsg(("<< enter_critical_section() on tid=%d\n", GET_TID()));
   return;
}


//
// exit_critical_section()
// ***********************
//
void exit_critical_section(void)
{
   dbgmsg((">> exit_critical_section() on tid=%d\n", GET_TID()));
#if defined(_WINDOWS)
   LeaveCriticalSection(&harvester_cs);
#elif defined(_LINUX)
   pthread_mutex_unlock(&harvester_mutex);
#endif
   dbgmsg(("<< exit_critical_section() on pid=%d\n", GET_TID()));
   return;
}
#endif

//
// GetProcessNode()
// ****************
//
// Find pid node for given pid.
// Returns pointer to pid node or NULL if node does not exist.
//
PID_NODE * GetProcessNode(uint pid)
{
   PID_NODE * pn;

   dbgmsg(("> GetProcessNode: 0x%x\n", pid));
   pn = G(PidTable[hash_pid(pid)]);
   while (pn) {
      if (pn->pid == pid)
         break;                             // Found it
      pn = pn->hash_next;                   // No match. Keep going
   }
   if (pn) {
      dbgmsg(("< GetProcessNode: 0x%x (pn=%p, pn->lastsym=%p)\n", pid, pn, pn->lastsym));
   } else {
      dbgmsg(("< GetProcessNode: 0x%x (pn=%p)\n", pid, pn));
   }

   return (pn);                             // Will be NULL if node not found.
}


//
// CreateProcessNode()
// *******************
//
// Create a pid node and add it to the pid list.  NULL process name.
// Process name will be set, later on, to the name of the first module
// loaded, which is usually the actual executable itself.
// Process nodes created this way are "root" processes: not cloned
// and not forked - the process node is the start of a parent/child chain.
// Returns pointer to added node or NULL if unable to add.
//
PID_NODE * CreateProcessNode(uint pid)
{
   PID_NODE * pn;


   dbgmsg(("> CreateProcessNode: %d (0x%x)\n", pid,pid));

   pn = AllocProcessNode();                 // Allocate and initialize node.
   if (pn == NULL) {
      errmsg(("*E* CreateProcessNode: unable to allocate pid node. pid=0x%x.\n", pid));
      return (NULL);
   }

   pn->pid = pid;                           // Initialize node
   pid_hash_add(pn);                        // Add it to the pid hash and pid list

   //
   // Remember the pid node for the System process if not already set.
   // We need to know this because any modules associated with the kernel
   // need to be added to all processes - the kernel can run in the context
   // of any process.
   // This is to cover the case where the caller does not explicitly set
   // the system's pid by calling A2nSetSystemPid().
   //
   if (G(SystemPidNode) == NULL  &&  pid == G(SystemPid))
      G(SystemPidNode) = pn;

   // Done
   G(PidCnt)++;
   dbgmsg(("< CreateProcessNode: (0x%x) (pn=%p, total pids=%d)\n", pid, pn, G(PidCnt)));
   return (pn);
}


//
// CloneProcessNode()
// ******************
//
// Create a new pid node and add it to the pid list.
// The new node is linked to the cloner's pid node and takes on the
// cloner's modules and name.
// Each cloned pid node points back to its cloner.  All processes
// cloned by any one process are linked together.
// Should only be called after a call to GetProcessNode() returns NULL -
// otherwise we can wind up with multiple pid nodes for the same process.
// Returns pointer to added node or NULL if unable to add.
//
PID_NODE * CloneProcessNode(uint ppid, uint cpid)
{
#if defined(_WINDOWS)
   errmsg(("*E* CloneProcessNode: Not supported on Windows!\n"));
   return (NULL);

#else

   PID_NODE * ppn, * cpn;


   dbgmsg(("> CloneProcessNode: 0x%x->0x%x\n", ppid, cpid));

   if (ppid == cpid) {                      // Make sure parent/child pids different
      errmsg(("*E* CloneProcessNode: cloner == cloned\n"));
      return (NULL);
   }

   ppn = GetProcessNode(ppid);              // Find parent's pid node
   if (ppn == NULL) {
      errmsg(("*E* CloneProcessNode: unable to find cloner's pid node.\n"));
      return (NULL);
   }

   cpn = GetProcessNode(cpid);              // Find child's pid node
   if (cpn != NULL) {
      warnmsg(("*W* CloneProcessNode: 0x%0x cloning 0x%0x which already exists and will be lost.\n", ppid, cpid));
      cpn = NULL;
   }

   cpn = AllocProcessNode();                // Allocate child's process node
   if (cpn == NULL) {
      errmsg(("*E* CloneProcessNode: unable to allocate pid node for pid=0x%x.\n", cpid));
      return (NULL);
   }

   cpn->pid = cpid;                         // Initialize node
   cpn->cloner = ppn;                       // My parent's pid node
   cpn->name = zstrdup(ppn->name);          // Take on the parent's name until changed.
   if (cpn->name == NULL) {
      warnmsg(("*W* CloneProcessNode: unable to allocate cloned pid name. Setting name to \"%s\"\n", cpid, G(UnknownName)));
      cpn->name = G(UnknownName);
   }
   dbgmsg(("- CloneProcessNode: set pid 0x%04x name to '%s'\n", cpid, cpn->name));

   // Add child to all appropriate lists
   pid_hash_add(cpn);                       // Add it to the pid hash and pid list

   cpn->cnext = NULL;
   if (ppn->cloned == NULL)
      ppn->cloned = ppn->cend = cpn;        // First node (also the last)
   else {
      ppn->cend->cnext = cpn;               // Add child at the end
      ppn->cend = cpn;                      // New end
   }
   ppn->cloned_cnt++;                       // One more clone

   //
   // Keep pointer to process that originated entire clone chain:
   // - If cloner is not a clone then he's the root cloner for the
   //   entire clone chain.
   // - If cloner is a clone then we need to point to his root cloner
   //   because we both originated from him.
   //
   dbgmsg(("- CloneProcessNode: ppn->rootcloner=%p (NULL => cloner is not a clone)\n", ppn->rootcloner));
   if (ppn->rootcloner == NULL)
      cpn->rootcloner = ppn;                // Cloner is not a clone
   else
      cpn->rootcloner = ppn->rootcloner;    // Cloner is a clone

   // Done
   G(PidCnt)++;
   dbgmsg(("< CloneProcessNode: 0x%x->0x%x  (pn=%p, total pids=%d)\n", ppid, cpid, cpn, G(PidCnt)));
   return (cpn);
#endif
}


//
// ForkProcessNode()
// ******************
//
// Create a new pid node and add it to the pid list.
// The new node is linked to the cloner's pid node and takes on the
// forker's modules.  The name will be set to that of the forker.
// Returns pointer to added node or NULL if unable to add.
//
PID_NODE * ForkProcessNode(uint ppid, uint cpid)
{
#if defined(_WINDOWS)
   errmsg(("*E* ForkProcessNode: Not supported on Windows!\n"));
   return (NULL);

#else

   PID_NODE * ppn, * cpn;
   LMOD_NODE * ilmn = NULL;                  // Inherited loaded modules chain

   dbgmsg(("> ForkProcessNode: 0x%x->0x%x\n", ppid, cpid));

   if (ppid == cpid) {                      // Make sure parent/child pids different
      errmsg(("*E* ForkProcessNode: forker == forked\n"));
      return (NULL);
   }

   ppn = GetProcessNode(ppid);              // Find parent's pid node
   if (ppn == NULL) {
#if defined(_AIX)

      if (A2nSetProcessName(ppid, "FORKER") != A2N_SUCCESS) {
         return (NULL);
      }
      ppn = GetProcessNode(ppid);           // Get the pid node just created

#else

      // Can't find the parent so pretend the child has no parent and create it
      cpn = GetProcessNode(cpid);           // Find child's pid node
      if (cpn != NULL) {
         // Code used to throw away the child's data because it appeared
         // that the child pid was being reused. Now we just assume the
         // fork hook is out of order and if the child exists we just
         // keep it. We don't fix the the child node at all - it stays
         // the way it is.
         warnmsg(("*W* ForkProcessNode: 0x%0x forking 0x%0x which already exists and will be kept.\n", ppid, cpid));
         return (cpn);
      }

      cpn = CreateProcessNode(cpid);
      if (cpn == NULL) {                    // Unable to add child PID node
         errmsg(("*E* ForkProcessNode: unable to create process node for child pid 0x%04x\n", cpid));
         return (NULL);
      }
      cpn->name = G(UnknownName);
      errmsg(("*I* ForkProcessNode: cpid 0x%04x created without parent. ppid 0x%04x not found.\n", cpid, ppid ));
      dbgmsg(("< ForkProcessNode: cpid 0x%04x created without parent. ppid 0x%04x not found. Total pids=%d\n", cpid, ppid, G(PidCnt)));
      return (cpn);

#endif
   }

   cpn = GetProcessNode(cpid);              // Find child's pid node
   if (cpn != NULL) {
      // Code used to throw away the child's data because it appeared
      // that the child pid was being reused. Now we just assume the
      // fork hook is out of order and if the child exists we just
      // keep it. We don't fix the the child node at all - it stays
      // the way it is.
      warnmsg(("*W* ForkProcessNode: 0x%0x forking 0x%0x which already exists and will be kept.\n", ppid, cpid));
      return (cpn);
   }

   cpn = AllocProcessNode();                // Allocate child's process node
   if (cpn == NULL) {
      errmsg(("*E* ForkProcessNode: unable to allocate pid node for pid=0x%x.\n", cpid));
      return (NULL);
   }

   cpn->pid = cpid;                         // Initialize node
   cpn->forker = ppn;                       // My parent's pid node
   cpn->name = zstrdup(ppn->name);          // Take on the parent's name until changed.
   if (cpn->name == NULL) {
      warnmsg(("*W* ForkProcessNode: unable to allocate forked pid name. Setting name to \"%s\"\n", cpid, G(UnknownName)));
      cpn->name = G(UnknownName);
   }
   dbgmsg(("- ForkProcessNode: set pid 0x%04x name to '%s'\n", cpid, cpn->name));

   //
   // Inherit the forker's module list as it currently exists.
   // Just copy the forker's module list to the forked process.
   // Never inherit the system pid's modules.
   //
   if (ppn->pid == G(SystemPid)) {
      dbgmsg(("- ForkProcessNode: forker is System. Don't inherit anything.\n"));
   }
   else if (PROCESS_IS_CLONE(ppn)) {        // Forker is a clone and not System
      dbgmsg(("- ForkProcessNode: forker is clone - inherit root cloner's (0x%x) modules\n", ppn->rootcloner->pid));
      ilmn = ppn->rootcloner->lmroot;       // Inherit root cloner's modules
      cpn->imcnt = ppn->rootcloner->lmcnt;  // Inherited modules (incl. jitted methods)
      cpn->ijmcnt = ppn->rootcloner->jmcnt; // Inherited jitted methods
   }
   else {                                   // Forker is not a clone and not System
      dbgmsg(("- ForkProcessNode: forker not a clone - inherit forker's modules\n"));
      ilmn = ppn->lmroot;                   // Inherit its modules
      cpn->imcnt = ppn->lmcnt;              // Inherited modules (incl. jitted methods)
      cpn->ijmcnt = ppn->jmcnt;             // Inherited jitted methods
   }

   if (ilmn == NULL) {
      dbgmsg(("- ForkProcessNode: There are no modules to inherit!\n"));
      cpn->lmroot = NULL;                   // No inherited modules
      cpn->lmcnt = cpn->imcnt = cpn->jmcnt = cpn->ijmcnt = 0;
   }
   else {
      dbgmsg(("- ForkProcessNode: (Inheriting %d modules. Copying module list ...)\n", cpn->imcnt));
      cpn->lmcnt = cpn->imcnt;              // Start off with inherited modules
      cpn->jmcnt = cpn->ijmcnt;             // Start off with inherited jitted methods
      cpn->lmroot = CopyLoadedModuleList(ilmn);
      if (cpn->lmroot == NULL) {            // Error copying list
         FreeProcessNode(cpn);
         errmsg(("*E* ForkProcessNode: unable to copy forked pid's (0x%x) inherited module list\n", cpid));
         return (NULL);
      }
   }

   // Add child to all appropriate lists
   pid_hash_add(cpn);                       // Add it to the pid hash and pid list

   cpn->fnext = NULL;
   if (ppn->forked == NULL)
      ppn->forked = ppn->fend = cpn;        // First node (also the last)
   else {
      ppn->fend->fnext = cpn;               // Add child at the end
      ppn->fend = cpn;                      // New end
   }
   ppn->forked_cnt++;                       // One more fork

   // Done
   G(PidCnt)++;
   dbgmsg(("< ForkProcessNode: 0x%x->0x%x  (pn=%p, total pids=%d)\n", ppid, cpid, cpn, G(PidCnt)));
   return (cpn);
#endif
}


//
// CreateNewProcess()
// ******************
//
// Create a new pid node and add it to the pid list.
// The new node is not a fork nor a clone and does not inherit any modules.
// If a process with the same PID alredy exists it is deleted and a
// new one created.
// Returns pointer to added node or NULL if unable to add.
//
PID_NODE * CreateNewProcess(uint pid)
{
#if !defined(_WINDOWS)
   errmsg(("*E* CreateNewProcess: Not supported on this platform\n"));
   return (NULL);

#else

   PID_NODE * pn, * npn, * hpn;
   LASTSYM_NODE * lsn;


   dbgmsg(("> CreateNewProcess: 0x%x\n", pid));
   pn = GetProcessNode(pid);
   if (pn == NULL) {
      // Not an existing process. Create a new one
      pn = CreateProcessNode(pid);
      if (pn == NULL) {
         errmsg(("*E* CreateNewProcess: unable to create process node for 0x%04x\n", pid));
         return (NULL);
      }
   }
   else {
      // Existing process being replaced. Save data we need and re-use the node
      errmsg(("*I* CreateNewProcess: 0x%04x-%s being replaced.\n", pid, pn->name));

      npn = AllocProcessNode();             // Allocate and initialize new node
      if (npn == NULL) {
         errmsg(("*E* CreateNewProcess: unable to allocate pid node. pid=0x%x.\n", pid));
         return (NULL);
      }

      dbgmsg(("- CreateNewProcess: replaced process information\n"));
      dbgmsg(("                    pn=%p, name=%s, lmroot=%p, lmreq_cnt=%d\n",
              pn, pn->name, pn->lmroot, pn->lmreq_cnt));
      dbgmsg(("                    lmcnt=%d, jmcnt=%d, imcnt=%d, ijmcnt=%d\n",
              pn->lmcnt, pn->jmcnt, pn->imcnt, pn->ijmcnt));
      dbgmsg(("                    Being copied to npn=%p\n", npn));

      // Copy from existing to new node
      npn->hash_next  = NULL;
      npn->lmroot     = pn->lmroot;
      npn->name       = zstrdup(pn->name);
      npn->rootcloner = npn->cloner = npn->cnext = npn->cloned = npn->cend = NULL;
      npn->forker     = npn->fnext = npn->forked = npn->fend = NULL;
      npn->cloned_cnt = npn->forked_cnt = 0;
      npn->pid        = pn->pid;
      npn->lmcnt      = pn->lmcnt;
      npn->jmcnt      = pn->jmcnt;
      npn->imcnt      = pn->imcnt;
      npn->ijmcnt     = pn->ijmcnt;
      npn->flags      = pn->flags;
      npn->lmerr_cnt  = pn->lmerr_cnt;
      npn->kernel_req = pn->kernel_req;
      npn->lmreq_cnt  = pn->lmreq_cnt;
      memcpy(npn->lastsym, pn->lastsym, sizeof(LASTSYM_NODE)); // Copy original's LASTSYM mode
      npn->lastsym->valid = 0;

      // Save the new node at the beginning of the re-used list
      G(ReusedPidCnt)++;
      if (G(ReusedPidNode) == NULL) {
         G(ReusedPidNode) = npn;
         npn->next = NULL;
      }
      else {
         npn->next = G(ReusedPidNode);
         G(ReusedPidNode) = npn;
      }
      dbgmsg(("- CreateNewProcess: npn=%p added to Reused Pid List. ReusedPidCnt=%d\n", npn, G(ReusedPidCnt)));

      // Re-initialize the existing PID node so we can re-use it
      npn = pn->next;
      hpn = pn->hash_next;
      lsn = pn->lastsym;
      if (pn->name != G(UnknownName))
         zfree(pn->name);                   // free the original name
      memset(pn, 0, sizeof(PID_NODE));      // Re-initialize pid node
      pn->pid = pid;
      pn->next = npn;
      pn->hash_next = hpn;
      pn->lastsym = lsn;
      pn->lastsym->valid = 0;
      pn->flags = A2N_FLAGS_PID_NODE_SIGNATURE | A2N_FLAGS_PID_REUSED;
      dbgmsg(("- CreateNewProcess: pn=%p re-initialized\n", pn));
   }
   pn->name = G(UnknownName);

   dbgmsg(("< CreateNewProcess: pn=%p (NULL is bad)\n", pn));
   return (pn);
#endif
}


//
// GetParentProcessNode()
// **********************
//
// Returns the process node of the parent process.
// The "parent process" is the process that forked/cloned the child process.
//
PID_NODE * GetParentProcessNode(PID_NODE * pn)
{
   if (pn->cloner != NULL)
      return (pn->cloner);                  // CLONE: parent is the cloner
   else if (pn->forker != NULL)
      return (pn->forker);                  // FORK: parent is the forker
   else
      return (pn);                          // INITIAL: parent is itself
}


//
// GetOwningProcessNode()
// **********************
//
// Returns the process node of the owning process (for clones)
// or itself (for forks).
//
PID_NODE * GetOwningProcessNode(PID_NODE * pn)
{
   if (pn->cloner != NULL)
      return (pn->rootcloner);              // CLONE: owner is root cloner
   else
      return (pn);                          // NOT CLONE: owner is self
}


#if defined(_LINUX)
//
// find_vdso_in_maps()
// *******************
//
static int find_vdso_in_maps(void * * addr)
{
   FILE * map_file;                        // kernel /proc file handle
   char map_path[MAX_PATH_LEN];            // kernel /proc file path

   void * low, * high;                     // low and high module addresses
   char perms[5];                          // perms string (4 chars)
   void * offset;                          // module offset
   char dev[6];                            // device numbers (5 chars)
   unsigned int inode;                     // module inode number
   char pathname[MAX_PATH_LEN];            // module's full name
   char line[MAXLINE];                     // line buffer

   int vdso_size = 0;

   *addr = NULL;

   // Open the map file for the process
   sprintf(map_path, "/proc/self/maps");
   map_file = fopen (map_path, "r");
   if (!map_file) {
      dbgmsg(("-- find_vdso_in_maps: Unable to open maps\n"));
      return (vdso_size);
   }

   // Process map file entries
   while (fgets(line, MAXLINE, map_file) != NULL) {
      pathname[0] = 0;
      sscanf(line, "%p-%p %s %p %s %u %s", &low, &high, perms, &offset, dev, &inode, pathname);
      if (strcmp(pathname, VDSO_SEGMENT_NAME) == 0) {
         dbgmsg(("-- find_vdso_in_maps: found %s at %p-%p\n", VDSO_SEGMENT_NAME, low, high));
         vdso_size = PtrToInt32(PtrSub(high, low));
         *addr = low;
         break;
      }
   }

   fclose(map_file);
   return (vdso_size);
}


//
// find_a2n_path()
// ***************
//
static char * find_a2n_path(void)
{
   char * p;
   char * b;
   char path[MAX_PATH_LEN];

   p = getenv("LD_LIBRARY_PATH");
   if (p == NULL)
      goto TheEnd;

   b = FindFileAlongSearchPath("liba2n.so", p);
   if (b == NULL)
      goto TheEnd;

   p = strrchr(b, G(PathSeparator));
   if (p == NULL || *++p == '\0')
      goto TheEnd;

   *p = 0;
   return (b);

TheEnd:
   return (NULL);
}


//
// create_fake_vdso_module()
// *************************
//
// When called from A2nLoadMap() we're reading /proc/self/maps so we know
// exactly where the vdso segment is.
// When called from A2nLoadModule() we're reading MTE data and we know
// where the vdso segment *WAS* for whatever PID we happen to be processing.
// Since it doesn't really matter which vdso we save (if there isn't one already)
// we'll save our own (whatever process is calling A2N). That means we have to
// trudge thru /proc/self/maps and find [vdso]. We know to do this because
// addr and size are 0.
//
static char * create_fake_vdso_module(void * addr, int size)
{
   int fd, cnt;
   char * buf;
   char pathname[MAX_PATH_LEN];

   dbgmsg((">> create_fake_vdso_module: at %p for %d bytes\n", addr, size));
   if (addr == NULL && size == 0) {
      // Have to find [vdso] in /proc/self/maps
      size = find_vdso_in_maps(&addr);
      if (size == 0) {
         msg_log("*W* Can't find %s in /proc/maps. Will not continue processing.\n", VDSO_SEGMENT_NAME);
         return (NULL);
      }
   }

   // Find path where liba2n.so lives
   buf = find_a2n_path();
   if (!buf) {
      msg_log("*W* Can't find liba2n.so. Will not continue processing %s.\n", VDSO_SEGMENT_NAME);
      return (NULL);
   }

   strcpy(pathname, buf);
   strcat(pathname, FAKE_VDSO_MODULE_NAME);
   zfree(buf);
   if (FileIsReadable(pathname)) {
      dbgmsg(("<< create_fake_vdso_module: %s already exists\n", pathname));
      return (zstrdup(pathname));
   }

   dbgmsg(("-- create_fake_vdso_module: %s does not exist. Create it\n", pathname));
   buf = zmalloc(size);
   if (!buf) {
      msg_log("*E* Unable to malloc() %d bytes for reading %s segment.\n", size, VDSO_SEGMENT_NAME);
      return (NULL);
   }

   memcpy(buf, addr, size);
   fd = open(pathname, O_CREAT|O_WRONLY, S_IRWXU);
   if (fd == -1) {
      msg_log("*E* Unable to open %s for writing. open() rc = %d\n", pathname, errno);
      zfree(buf);
      return (NULL);
   }

   cnt = write(fd, buf, size);
   if (cnt != -1) {
      msg_log("*I* Wrote %s\n", pathname);
      zfree(buf);
      return (zstrdup(pathname));
   }

   msg_log("*E* Unable to write %s. write() rc = %d\n", pathname, errno);
   zfree(buf);
   return (NULL);
}
#endif


//
// AddLoadedModuleNode()
// *********************
//
// Add module node to module node list for the process and to the
// module cache if not already there.
// Returns pointer to added node or NULL if unable to add.
//
LMOD_NODE * AddLoadedModuleNode(PID_NODE * pn,
                                char * name,
                                uint64 base_addr,
                                uint length,
                                uint ts,
                                uint chksum,
                                int jitted_method)
{
   PID_NODE * opn;
   LMOD_NODE * lmn;
   char * buf = NULL;


   dbgmsg(("> AddLoadedModuleNode: pn=%p, name=%s, addr=%"_LZ64X", len=0x%x, ts=0x%x, chksum=0x%x, jm=%d\n",
           pn, name, base_addr, length, ts, chksum, jitted_method));
   //
   // Find correct pid node to add module to ...
   // For a cloned process we add modules to the root cloner only, so we
   // need to find the start of the cloned process chain (the process
   // we're trying to add a module to could have been cloned by a clone
   // of a clone of a clone ...)
   // For forked/primordial process we add modules to them.
   //
   opn = GetOwningProcessNode(pn);
   if (opn == NULL) {
      errmsg(("*E* AddLoadedModuleNode: unable to find owning process node for 0x%x\n", pn->pid));
      return (NULL);
   }

   dbgmsg(("- AddLoadedModuleNode: pn->lmcnt=%d, pn->jmcnt=%d, opn=%p, opn->lmcnt=%d, opn->jmcnt=%d\n",
           pn->lmcnt, pn->jmcnt, opn, opn->lmcnt, opn->jmcnt));

#if defined(_LINUX)
   if (strcmp(name, VDSO_SEGMENT_NAME) == 0) {
      buf = create_fake_vdso_module(NULL, 0);
      if (buf) {
         name = buf;
         dbgmsg(("- AddLoadedModuleNode: %s and changed name to %s\n",VDSO_SEGMENT_NAME, name));
      }
      else {
         dbgmsg(("- AddLoadedModuleNode: % but unable to change name\n", VDSO_SEGMENT_NAME));
      }
   }
#endif

#if defined(_LINUX) || defined(_ZOS)
   //
   // Is this loaded module the same as the last one loaded for this pid?
   // If it is, then I assume it's not really a module but another loadable
   // segment of the previous module.
   //
   if (!jitted_method) {
      lmn = opn->lmroot;                       // Last module loaded on this pid
      while (lmn != NULL) {
         if ( lmn->type != LMOD_TYPE_JITTED_CODE &&    // LMN not for jitted code
              !(lmn->flags & A2N_FLAGS_INHERITED) ) {  // LMN not inherited
            // There are other loaded modules, and the last one is not a jitted method, so check ...
            if (strcmp(name, lmn->mn->orgname) == 0) {
               int i;
               // For some reason we like to load the same things over and over. Go figure.
               if (lmn->start_addr == base_addr && lmn->length == length) {
                  if (PROCESS_IS_CLONE(pn)) {
                     // don't make too much noise for clones
                     dbgmsg(("- AddLoadedModuleNode: adding identical segment to clone 0x%04x (%s) at 0x%"_LZ64X" for 0x%x\n",
                             pn->pid, name, base_addr, length));
                  }
                  else {
                     errmsg(("*W* AddLoadedModuleNode: adding identical segment to 0x%04x (%s) at 0x%"_LZ64X" for 0x%x\n",
                             pn->pid, name, base_addr, length));
                  }
                  zfree(buf);
                  return (lmn);
               }
               // check if segment already exists
               for (i =0; i < lmn->seg_cnt; i++) {
                  if (lmn->seg_start_addr[i] == base_addr &&
                        lmn->seg_end_addr[i] == base_addr + length - 1){
                     zfree(buf);
                     return (lmn);
                  }
               }
               // this is a new segment
               // Given names are equal so must be another segment on this module
               if (lmn->seg_cnt > MAX_LOADABLE_SEGS) {
                  // Already have MAX_LOADABLE_SEGS loaded. Make another LOADED_MODULE
                  // and start adding segments to it. Should work just as well.
                  dbgmsg(("- AddLoadedModuleNode: more than %d segments loaded for %s. Making new LM\n", MAX_LOADABLE_SEGS, name));
                  lmn = lmn->next;
                  continue;
               }
               // check if not in range
               if ((lmn->start_addr + lmn->total_len) > base_addr &&
                     lmn->start_addr >= base_addr) {
                  lmn = lmn->next;
                  continue;
               }
               // add the segment
               lmn->seg_cnt++;
               i = lmn->seg_cnt - 2;
               lmn->seg_start_addr[i] = base_addr;
               lmn->seg_end_addr[i]   = base_addr + length - 1;
               lmn->seg_length[i] = length;
               lmn->total_len = PtrToUint32(PtrSub(lmn->seg_end_addr[i], lmn->start_addr)) + 1;
         #ifdef EMPTEMP
               dbgmsg(("  lmn->next              = %p\n", lmn->next));
               dbgmsg(("  lmn->mn                = %p\n", lmn->mn));
               dbgmsg(("  lmn->jmn               = %p\n", lmn->jmn));
               dbgmsg(("  lmn->start_addr        = %p\n", lmn->start_addr));
               dbgmsg(("  lmn->end_addr          = %p\n", lmn->end_addr));
               dbgmsg(("  lmn->length            = %08X\n", lmn->length));
               for (i = 0; i <= lmn->seg_cnt - 2; i++) {
                  dbgmsg(("  lmn->seg_start_addr[%d] = %p\n", i, lmn->seg_start_addr[i]));
                  dbgmsg(("  lmn->seg_end_addr[%d]   = %p\n", i, lmn->seg_end_addr[i]));
                  dbgmsg(("  lmn->seg_length[%d]     = %08X\n", i, lmn->seg_length[i]));
               }
               dbgmsg(("  lmn->total_len         = %08X\n", lmn->total_len));
               dbgmsg(("  lmn->seg_cnt           = %08X\n", lmn->seg_cnt));
         #endif
               dbgmsg(("< AddLoadedModuleNode: Adding another segment to %s\n", name));
               zfree(buf);
               return (lmn);
            }
            dbgmsg(("- AddLoadedModuleNode: Last module loaded not same. Assumming new one.\n"));
         }
         lmn = lmn->next;
      }
   }

AddNew:
#endif

   // Allocate new node
   lmn = AllocLoadedModuleNode();
   if (lmn == NULL) {
      errmsg(("*E* AddLoadedModuleNode: unable to allocate LMOD_NODE. pid=0x%x, modname=%s.\n", pn->pid, name));
      zfree(buf);
      return (NULL);
   }

   // Initialize node. Anything not explicitly initialized is 0/NULL.
   lmn->start_addr = base_addr;
   lmn->end_addr   = base_addr + length - 1;
   lmn->length = length;
   lmn->total_len = length;
   lmn->pid = opn->pid;

   if (jitted_method) {
      lmn->type = LMOD_TYPE_JITTED_CODE;
   }
   else {
      // Real module: initialize and add module to module cache if necessary.
      lmn->seg_cnt = 1;
      if (ts == TS_ANON)
         lmn->type = LMOD_TYPE_ANON;
      else
         lmn->type = LMOD_TYPE_MODULE;

      AddModuleNodeToCache(lmn, name, ts, chksum);
      if (lmn->mn == NULL) {
         FreeLoadedModuleNode(lmn);
         errmsg(("*E* AddLoadedModuleNode: unable to add '%s' to module cache.\n", name));
         zfree(buf);
         return (NULL);
      }
   }

   // Invalidate last symbol returned on pid, in case what we're adding overlaps
   opn->lastsym->valid = 0;

   //
   // Link in the new node to the beginning of the loaded module node
   // list for the process that owns the module.
   //
   if (opn->lmroot != NULL)
      lmn->next = opn->lmroot;              // Not first node on list
   else
      lmn->next = NULL;                     // First and only node

   opn->lmroot = lmn;

   //
   // Invalidate LastSymbol node. This will force a full lookup
   // the next time a symbol is requested for this pid.
   //
   pn->lastsym->valid = 0;

   // Done
   opn->lmcnt++;
   if (jitted_method)
      opn->jmcnt++;

#ifdef EMPTEMP
   {
      int i;
      dbgmsg(("  lmn->next              = %p\n", lmn->next));
      dbgmsg(("  lmn->mn                = %p\n", lmn->mn));
      dbgmsg(("  lmn->jmn               = %p\n", lmn->jmn));
      dbgmsg(("  lmn->start_addr        = %p\n", lmn->start_addr));
      dbgmsg(("  lmn->end_addr          = %p\n", lmn->end_addr));
      dbgmsg(("  lmn->length            = %08X\n", lmn->length));
      for (i = 0; i <= lmn->seg_cnt - 2; i++) {
         dbgmsg(("  lmn->seg_start_addr[%d] = %p\n", i, lmn->seg_start_addr[i]));
         dbgmsg(("  lmn->seg_end_addr[%d]   = %p\n", i, lmn->seg_end_addr[i]));
         dbgmsg(("  lmn->seg_length[%d]     = %08X\n", i, lmn->seg_length[i]));
      }
      dbgmsg(("  lmn->total_len         = %08X\n", lmn->total_len));
      dbgmsg(("  lmn->seg_cnt           = %08X\n", lmn->seg_cnt));
   }
#endif
   zfree(buf);
   dbgmsg(("< AddLoadedModuleNode: (lmn=%p, lmcnt=%d, jmcnt=%d)\n", lmn, opn->lmcnt, opn->jmcnt));
   return (lmn);
}


//
// GetMatchingLoadedModuleNode()
// *****************************
//
// Determine whether or not a loaded module is already in the loaded
// module chain for the process.
// Returns node ptr if module already in chain or NULL if not.
//
LMOD_NODE * GetMatchingLoadedModuleNode(PID_NODE * pn,
                                        char * name,
                                        uint64 base_addr,
                                        uint length)
{
   LMOD_NODE * lmn;
   uint hv;


   if (pn == NULL) {
      errmsg(("*E* GetMatchingLoadedModuleNode: **BUG** NULL pn!)\n"));
      return (NULL);                        // Not good
   }

   //
   // Look for the module (name, base_addr and length all must match)
   // in loaded module chain. We either find it or we don't.
   //
   hv = hash_module_name(name);
   lmn = pn->lmroot;
   while (lmn) {
      if (lmn->type == LMOD_TYPE_MODULE || lmn->type == LMOD_TYPE_ANON) {  // Only look at modules
         if (base_addr == lmn->start_addr  &&    // Same base address
             lmn->length == length  &&           // and same length
             lmn->mn->name_hash == hv  &&        // and same name hash value
             COMPARE_NAME(name, lmn->mn->name) == 0) {  // and names are the same
            dbgmsg((">< GetMatchingLoadedModuleNode: '%s' already loaded for 0x%04x. lmn=%p\n", name, pn->pid, lmn));
            return (lmn);                   // Everything matches
         }
#if defined(_LINUX) || defined(_ZOS)
         // If >1 segments, look in all of them
         if (lmn->seg_cnt > 1) {
            int i;
            for (i = 0; i <= lmn->seg_cnt - 2; i++) {
               if (base_addr == lmn->seg_start_addr[i]  &&    // Same base address
                   lmn->seg_length[i] == length  &&           // and same length
                   lmn->mn->name_hash == hv  &&               // and same name hash value
                   COMPARE_NAME(name, lmn->mn->name) == 0) {  // and names are the same
                  dbgmsg((">< GetMatchingLoadedModuleNode: '%s' already loaded for 0x%04x. lmn=%p, seg=%d\n",
                          name, pn->pid, lmn, i + 2));
                  return (lmn);                   // Everything matches
               }
            }

         }
#endif
      }
      lmn = lmn->next;
   }

   dbgmsg((">< GetMatchingLoadedModuleNode: '%s' not yet loaded for 0x%04x\n", name, pn->pid));
   return (NULL);                           // Didn't find it.
}


//
// GetLoadedModuleName()
// *********************
//
char * GetLoadedModuleName(LMOD_NODE * lmn)
{
   if (lmn->mn != NULL)
      return (lmn->mn->name);
   else if (lmn->jmn != NULL)
      return (lmn->jmn->name);
   else
      return ("mn == jmn == NULL");
}


//
// GetLoadedModuleNode()
// *********************
//
// Find the module node associated with an address in a process.
// What this is really doing is finding out if "addr" falls in the
// address range of any module loaded by this process.
// Returns pointer to mod node or NULL if unable to find.
//
LMOD_NODE * GetLoadedModuleNode(PID_NODE * pn, uint64 addr)
{
   PID_NODE * opn;
   LMOD_NODE * lmn;


   dbgmsg(("> GetLoadedModuleNode: pn=%p, addr=%"_LZ64X"\n", pn, addr));

   if (pn == ex_pn) {
      opn = ex_opn;
   }
   else {
      if (pn == NULL) {                     // NULL pn. Not good
         errmsg(("*E* GetLoadedModuleNode: **BUG** NULL pn!)\n"));
         return (NULL);
      }

      opn = GetOwningProcessNode(pn);       // Find owning process
      if (opn == NULL) {
         errmsg(("*E* GetLoadedModuleNode: unable to find owning process node for 0x%x\n", pn->pid));
         return (NULL);
      }
   }

   // To populate kernel modules before we actually start looking
   if (!kernel_symbols_choice_made)
      make_kernel_symbols_choice();

   //
   // Look thru the loaded module list for the module/jitted method
   // containing the given address. We can fail if there have been no
   // modules/jitted methods added to or the address is in the kernel.
   //
   opn->lmreq_cnt++;
   if (opn != pn)
      pn->lmreq_cnt++;                      // Track by pid also (mainly for clones) #### TS_FIX ####

   lmn = opn->lmroot;
   while (lmn) {
      if (addr >= lmn->start_addr  &&  addr <= lmn->end_addr) {
         dbgmsg(("< GetLoadedModuleNode: %"_LZ64X" found in [%p]'%s'\n", addr, lmn, GetLoadedModuleName(lmn)));
         return (lmn);                      // Got it. addr falls in this module
      }
#if defined(_LINUX) || defined(_ZOS)
      // If >1 segments, look in all of them
      if (lmn->seg_cnt > 1) {
         int i;
         for (i = 0; i <= lmn->seg_cnt - 2; i++) {
            if (addr >= lmn->seg_start_addr[i]  &&  addr <= lmn->seg_end_addr[i]) {
               dbgmsg(("< GetLoadedModuleNode: %"_LZ64X" found in segment %d of '%s'\n",
                       addr, i+2, GetLoadedModuleName(lmn)));
               return (lmn);                      // Got it. addr falls in this module
            }
         }

      }
#endif
      lmn = lmn->next;
   }
   opn->lmerr_cnt++;
   if (opn != pn)
      pn->lmerr_cnt++;                      // Track by pid also (mainly for clones) #### TS_FIX ####

   dbgmsg(("- GetLoadedModuleNode: loaded module not found. Trying kernel ...\n"));

   //
   // Not a regular module and not jitted code. The only thing
   // left is to check the kernel if we know which pid it is.
   // If we were already looking in the kernel's loaded module
   // list then don't bother doing it again.
   //
   if (opn->pid != G(SystemPid)  &&  G(SystemPidNode) != NULL) {
      G(SystemPidNode)->lmreq_cnt++;
      lmn = G(SystemPidNode)->lmroot;
      while (lmn) {
         if (addr >= lmn->start_addr  &&  addr <= lmn->end_addr) {
            dbgmsg(("< GetLoadedModuleNode: %"_LZ64X" found in [%p]'%s'\n", addr, lmn, GetLoadedModuleName(lmn)));
            return (lmn);                   // Got it. addr falls in this module
         }
#if defined(_LINUX) || defined(_ZOS)
         // If >1 segments, look in all of them
         if (lmn->seg_cnt > 1) {
            int i;
            for (i = 0; i <= lmn->seg_cnt - 2; i++) {
               if (addr >= lmn->seg_start_addr[i]  &&  addr <= lmn->seg_end_addr[i]) {
                  dbgmsg(("< GetLoadedModuleNode: %"_LZ64X" found in segment %d of '%s'\n",
                          addr, i+2, GetLoadedModuleName(lmn) ));
                  return (lmn);                      // Got it. addr falls in this module
               }
            }

         }
#endif
         lmn = lmn->next;
      }
      G(SystemPidNode)->lmerr_cnt++;
   }

   dbgmsg(("< GetLoadedModuleNode: **NOMODULE** (%"_LZ64X" not in any module for 0x%x (or root 0x%x))\n", addr, pn->pid, opn->pid));
   return (NULL);
}


//
// GetLoadedModuleNodeForModule()
// ******************************
//
// Given a pid node and a module node, find the first loaded module
// that points to the given module node.
//
LMOD_NODE * GetLoadedModuleNodeForModule(PID_NODE * pn, MOD_NODE * mn)
{
   PID_NODE * opn;
   LMOD_NODE * lmn;


   dbgmsg(("> GetLoadedModuleNodeForModule: pn=%p, mn=%p\n", pn, mn));

   if (pn == NULL  ||  mn == NULL) {
      dbgmsg(("< GetLoadedModuleNodeForModule: **BUG** NULL pn and/or mn!)\n"));
      return (NULL);                        // Not good
   }

   opn = GetOwningProcessNode(pn);          // Find owning process
   if (opn == NULL) {
      errmsg(("*E* GetLoadedModuleNodeForModule: unable to find owning process node for 0x%x\n", pn->pid));
      return (NULL);
   }

   // Look for the first loaded module pointing to the given module node.
   lmn = opn->lmroot;
   while (lmn) {
      if (lmn->mn == mn) {
         dbgmsg(("< GetLoadedModuleNodeForModule: (Done. lmn=%p)\n", lmn));
         return (lmn);                      // Got it.
      }
      lmn = lmn->next;
   }

   // Done
   dbgmsg(("< GetLoadedModuleNodeForModule: (No lmn points to mn=%p)\n", mn));
   return (NULL);
}


//
// AddModuleNodeToCache()
// **********************
//
// Add module node to module cache.
// Sets lmn->mn on success, leaves NULL on error.
//
void AddModuleNodeToCache(LMOD_NODE * lmn,
                          char * name,
                          uint ts,
                          uint chksum)
{
   MOD_NODE * mn;
   uint hv;
   char * fixed_name[MAX_PATH_LEN];


   dbgmsg(("> AddModuleNodeToCache: '%s', lmn=%"_PP", ts=0x%x, cs=0x%x\n", name, lmn, ts, chksum));

   if (name == NULL  ||  *name == '\0') {   // Module has no name
      lmn->mn = NULL;
      errmsg(("*E* AddModuleNodeToCache: module name is NULL.\n"));
      return;
   }

   FixupModuleName(lmn, name, (char *)fixed_name);

   // Check to see if module already exists in the cache.
   hv = hash_module_name((char *)fixed_name);
   mn = GetModuleNodeFromCache((char *)fixed_name, hv, ts, chksum);
   if (mn != NULL) {
      lmn->mn = mn;
      mn->last_lmn = lmn;
      dbgmsg(("< AddModuleNodeToCache: (found '%s' in cache. mn=%p)\n", fixed_name, mn));
      return;
   }

   // Not in cache already. Allocate new node
   mn = AllocModuleNode();
   if (mn == NULL) {
      lmn->mn = NULL;
      errmsg(("*E* AddModuleNodeToCache: unable to allocate MOD_NODE for '%s'\n", name));
      return;
   }

   // Initialize node. Anything not explicitly initialized is 0/NULL.
   if (ts == TS_ANON) {
      mn->type = MODULE_TYPE_ANON;
      ts = 0;
   }
   else if (ts == TS_VSYSCALL32) {
      mn->type = MODULE_TYPE_VSYSCALL32;
      ts = 0;
   }
   else if (ts == TS_VSYSCALL64) {
      mn->type = MODULE_TYPE_VSYSCALL64;
      ts = 0;
   }
   else if (ts == TS_VDSO) {
      mn->type = MODULE_TYPE_VDSO;
      mn->flags |= A2N_FLAGS_VDSO;
      ts = 0;
   }
   mn->ts = ts;                            // Module timestamp
   mn->chksum = chksum;                    // Module checksum

   mn->orgname = zstrdup(name);            // Original name (from A2nAddModule)
   if (mn->orgname == NULL) {
      FreeModuleNode(mn);
      lmn->mn = NULL;
      errmsg(("*E* AddModuleNodeToCache: unable to allocate original module name for %s.\n", name));
      return;
   }

   if (strcmp(name, (char *)fixed_name) == 0)
      mn->name = mn->orgname;                // Original and fixed names are the same
   else {
      mn->name = zstrdup((char *)fixed_name); // Original and fixed names are different
      if (mn->name == NULL) {
         FreeModuleNode(mn);
         lmn->mn = NULL;
         errmsg(("*E* AddModuleNodeToCache: unable to allocate fixed module name for %s.\n", name));
         return;
      }
   }

#if defined(_WINDOWS)
   GetJvmmapSummary(mn->name);
#endif

   mn->strsize += (int)strlen(name) + 1;
   mn->name_hash = hv;
   lmn->mn = mn;
   mn->last_lmn = lmn;

   //
   // If we haven't seen the kernel yet then check to see if this is it
   // and mark the module node as such.
   //
   if (G(kernel_not_seen)) {
      if (strcmp(mn->name, G(KernelFilename)) == 0) {
         mn->flags |= A2N_FLAGS_KERNEL;     // Mark module as being the kernel
         lmn->flags |= A2N_FLAGS_KERNEL;    // Mark loaded module as being the kernel
         G(kernel_not_seen) = 0;            // Just saw it
         dbgmsg(("- AddModuleNodeToCache: '%s' marked as being the kernel\n", mn->name));
      }
   }

   // Insert the new node at the requested end of the list
   if (G(add_module_cache_head)) {
      mn->next = G(ModRoot);
      if (G(ModRoot) == NULL)
         G(ModRoot) = G(ModEnd) = mn;
      else
         G(ModRoot) = mn;
   }
   else {
      mn->next = NULL;
      if (G(ModRoot) == NULL)
         G(ModRoot) = G(ModEnd) = mn;
      else {
         G(ModEnd)->next = mn;
         G(ModEnd) = mn;
      }
   }

   //
   // Gather symbols if user wants "immediate gather"
   // Of course we won't try again if we've already tried and failed
   // or if we've already gathered symbols for this module.
   //
   if (G(immediate_gather)  &&  !(mn->flags & A2N_FLAGS_SYMBOLS_HARVESTED)
                            &&  mn->type != MODULE_TYPE_ANON
                            &&  mn->type != MODULE_TYPE_VSYSCALL32
                            &&  mn->type != MODULE_TYPE_VSYSCALL64) {
      dbgmsg(("- AddModuleNodeToCache: Need to harvest symbols ...\n"));
      HarvestSymbols(mn, lmn);
   }

   // Done
   G(ModCnt)++;
   dbgmsg(("< AddModuleNodeToCache: (mn=%p, total modules=%d)\n", mn, G(ModCnt)));
   return;
}


//
// GetModuleNodeFromCache()
// ************************
//
// Find the module node in cache.
// Returns pointer to node or NULL if unable to find.
//
MOD_NODE * GetModuleNodeFromCache(char * name,
                                  uint hv,
                                  uint ts,
                                  uint chksum)
{
   MOD_NODE * mn;


   dbgmsg(("> GetModuleNodeFromCache: '%s'. hv=%x, ts=%x, cs=%x, mods_in_cache=%d\n", name, hv, ts, chksum, G(ModCnt)));

   // Find module cache node for this module in the cache
   mn = G(ModRoot);
   while (mn) {
      if (ModulesAreTheSame(mn, name, hv, ts, chksum))
         break;                             // Found it!
      else
         mn = mn->next;                     // No match, keep trying ...
   }

   dbgmsg(("< GetModuleNodeFromCache: (mn=%p)\n", mn));
   return (mn);
}


//
// ModulesAreTheSame()
// *******************
//
// Applies whatever comparisson rules are needed to make sure
// a given set of parameters describe a module precisely.
// "mn" describes a module in the module cache and the rest of
// the arguments describe a module the caller is looking for.
// Returns TRUE(1) if modules are the same or FALSE(0) if they're not.
//
// Note: if ts == -1 then only a name match is done.
//
int ModulesAreTheSame(MOD_NODE * mn,
                      char * name,
                      uint hv,
                      uint ts,
                      uint chksum)
{
   if (hv != mn->name_hash)
      return (0);                           // Can't be the same module if hash
                                            // values don't match.

   if (COMPARE_NAME(name, mn->name) != 0)
      return (0);                           // Names don't match

   if (ts == -1)
      return (1);                           // They only wanted a name match

   if (mn->ts != 0  &&  mn->ts != ts)
      return (0);                           // Non-zero TS and doesn't match

   if (mn->chksum != 0  &&  mn->chksum != chksum)
      return (0);                           // Non-zero CS and doesn't match

   // Good match: name, length, timestamp and checksum all matched
   return (1);
}


//
// ModuleFilenameMatches()
// ***********************
//
// Compares the given filename to the given module's filename.
// Returns TRUE (1) if they match or FALSE (0) if they don't.
//
int ModuleFilenameMatches(MOD_NODE * mn, char * filename)
{
   char * modname;


   dbgmsg(("> ModuleFilenameMatches: mn->name='%s'  fn='%s'\n", mn->name, filename));

   // Find module cache node for this module in the cache
   modname = GetFilenameFromPath(mn->name);
   if (COMPARE_NAME(modname, filename) == 0) {
      dbgmsg(("< ModuleFilenameMatches: match!\n"));
      return (1);                        // Match
   }
   else {
      dbgmsg(("< ModuleFilenameMatches: no match!\n"));
      return (0);                        // No match
   }
}


//
// GetModuleNodeFromModuleFilename()
// *********************************
//
// Find the 1st module node in cache, if any, that matches the given filename.
// If there are more than one module with the same filename (but different paths)
// then it's too bad - we'll grab the first one we come across.
// Returns pointer to node or NULL if unable to find.
//
MOD_NODE * GetModuleNodeFromModuleFilename(char * filename, MOD_NODE * start_mn)
{
   MOD_NODE * mn;
   char * modname;


   dbgmsg(("> GetModuleNodeFromModuleFilename: '%s' start_mn=%p. Modules currently in cache=%d\n", filename, start_mn, G(ModCnt)));

   // Find module cache node for this module in the cache
   mn = start_mn;
   while (mn) {
      modname = GetFilenameFromPath(mn->name);

      if (COMPARE_NAME(modname, filename) == 0) {
         dbgmsg(("- GetModuleNodeFromModuleFilename: module '%s' found in '%s'.\n", filename, mn->name));
         break;                             // Found it!
      }
      else
         mn = mn->next;                     // No match, keep trying ...
   }

   dbgmsg(("< GetModuleNodeFromModuleFilename: (mn=%p)\n", mn));
   return (mn);
}


//
// CopyLoadedModuleList()
// **********************
//
// Copies the module list pointed to by from_mn.
// This is done when processes are forked and we need to snapshot
// the module list of the forker process.
// The caller needs to figure out the correct module list to copy -
// that is, this function does not deal with fork/clone issues - it
// just copies a module list.
// Returns the pointer to the head of the module list or NULL on errors.
//
LMOD_NODE * CopyLoadedModuleList(LMOD_NODE * root)
{
   LMOD_NODE * newroot = NULL;              // Root of new module list
   LMOD_NODE * curfrom;                     // Current module node in from chain
   LMOD_NODE * curnew;                      // New module node
   LMOD_NODE * prevnew = NULL;              // Previous module node in new chain
   int copied;


   dbgmsg(("> CopyLoadedModuleList: %p\n", root));
   copied = 0;
   curfrom = root;                          // Starting point in "from" chain.
                                            // May or may not be NULL.
   while (curfrom) {
      // Copy loaded module node and add to chain
      curnew = AllocLoadedModuleNode();
      if (curnew == NULL) {
         FreeLoadedModuleList(newroot);     // Doesn't matter if it fails
         errmsg(("*E* CopyLoadedModuleList: unable to allocate MOD_NODE.\n"));
         return (NULL);
      }

      memcpy(curnew, curfrom, sizeof(LMOD_NODE));  // Copy node
      curnew->flags |= A2N_FLAGS_INHERITED;        // Mark it as inherited
      curnew->flags &= ~A2N_FLAGS_REQUESTED;       // Mark it as not requested
      curnew->req_cnt = 0;                         // Not requested on new pid yet

      // Link loaded module node into chain and continue ...
      if (newroot == NULL)
         newroot = curnew;                  // First module in new (copy) list
      else
         prevnew->next = curnew;            // Not first. Link it in.

      prevnew = curnew;                     // Remember prev in new chain
      curfrom = curfrom->next;              // Advance current in from chain
      copied++;
   }

   dbgmsg(("< CopyLoadedModuleList: (lmn=%p - OK if non-NULL, copied=%d)\n", newroot, copied));
   return (newroot);
}


//
// FreeLoadedModuleList()
// **********************
//
// Frees memory used by given module list.
// Used to clean up on failures which require that we free up memory.
// Since this is used on error paths it'll just blast ahead and clean
// up as much as it can.
//
void FreeLoadedModuleList(LMOD_NODE * root)
{
   LMOD_NODE * cur, * next;
   char * lmname;


   dbgmsg(("> FreeLoadedModuleList: root=%p\n", root));
   cur = root;                              // Starting point.
   while (cur) {
      next = cur->next;

      if (cur->type == LMOD_TYPE_MODULE || cur->type == LMOD_TYPE_ANON)
         lmname = cur->mn->name;
      else
         lmname = cur->jmn->name;

      dbgmsg(("- FreeLoadedModuleList: freeing LM node at %p for %s\n", cur, lmname));

      FreeLoadedModuleNode(cur);
      cur = next;
   }

   dbgmsg(("< FreeLoadedModuleList: (OK)\n"));
   return;
}


// Crud we need in order to read and load MTE data
#if defined(_WINDOWS)

BOOL is_process_wow64(void);    // in initterm.c

//
// read_mte_data_and_send_to_a2n()
// *******************************
//
static int read_mte_data_and_send_to_a2n(FILE * fd, char * buffer)
{
   char * name;
   char * p;
   UINT32 pid, size, ts, cs;
   UINT64 load_addr;
   int cnt = 0;

   // File contains one line per module.
   // Each line looks like this: pid load_addr size timestamp checksum name
   // - All numbers are in hex
   // - pid, size, timestamp and checksum are UINT32
   // - load_addr is UINT64
   // - name is string. It may contain blanks so need to use to EOL
   // - ex: 00000714 0000000077CA0000 00180000 4A5BDB3B 00148A78 C:\Windows\SysWOW64\ntdll.dll
   //   col 0123456789111111111122222222223333333333444444444455555555556666666666...
   //                 012345678901234567890123456789012345678901234567890123456789...

   dbgmsg(("> read_mte_data_and_send_to_a2n: start\n"));

   while (fgets(buffer, 4096, fd) != NULL) {
      dbgmsg(("- line='%s'\n", buffer));
      if ((int)strlen(buffer) < 54) {
         dbgmsg(("- SMALL line. Done, I guess.\n"));
         break;
      }
      buffer[52] = '\0';          // Blank before module name
      sscanf(buffer, "%08X %016I64X %08X %08X %08X", &pid, &load_addr, &size, &ts, &cs);
      name = (char *)PtrAdd(buffer, 53);
      p = strchr(name, 0x0D);     // CR
      *p = '\0';
      dbgmsg(("- sending: pid=%d addr=0x%016I64X len=%d ts=0x%08X cs=0x%08X name='%s'\n",
              pid, load_addr, size, ts, cs, name));

      A2nAddModule(pid, load_addr, size, name, ts, cs, NULL);
      cnt++;
   }

   dbgmsg(("< read_mte_data_and_send_to_a2n: end. Processed %d lines\n", cnt));
   return (cnt);
}


//
// running_on_64bit_os()
// *********************
//
// Returns:
// - TRUE:  current process is a WOW64 process, which means we're running
//          on a 64-bit OS.
// - FALSE: current process is NOT WOW64 process, which means it must be
//          running native on 32-bit OS.
//
static BOOL running_on_64bit_os(void)
{
#if defined(_64BIT)
   return (TRUE);
#else
   if (is_process_wow64())
      return (TRUE);                        // WOW64 process: 32-bit app on 64-bit OS
   else
      return (FALSE);                       // Not WOW64 - must be native 32-bit on 32-bit OS
#endif
}


//
// run_program()
// *************
//
static int run_program(char * cmdline)
{
   int rc = 0;
   PROCESS_INFORMATION pi;
   STARTUPINFO si;

   dbgmsg(("> run_program(%s)\n", cmdline));

   ZeroMemory(&si, sizeof(si));
   si.cb = sizeof(si);
   ZeroMemory(&pi, sizeof(pi));
   if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
      rc = GetLastError();
      errmsg(("*E* run_program: unable to create \"%s\". rc = %d", cmdline, rc));
      if (rc == ERROR_FILE_NOT_FOUND)
         errmsg((" (ERROR_FILE_NOT_FOUND).\n"));
      else
         errmsg((".\n", cmdline, rc));
   }
   else {
      // Wait for command to complete
      WaitForSingleObject(pi.hProcess, INFINITE);
      // Close process and thread handles
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
      rc = 0;
   }

   dbgmsg(("< run_program(%s). rc = %d\n", cmdline, rc));
   return (rc);
}
#endif


#if defined(_LINUX)
//
// load_process_modules()
// **********************
//
//       address
//    start     end    perm  offset  dev   inode      pathname
//   -------- -------- ---- -------- ----- -----      -------------------------
//   081ef000-0829e000 rw-p 00000000 00:00 0          [heap]
//   ad4fa000-ad50e000 rw-p 00000000 00:00 0
//   ad50e000-ad560000 r-xp 00000000 08:06 10075      /Dpiperf/lib/libjprof.so
//   ad560000-ad561000 r--p 00051000 08:06 10075      /Dpiperf/lib/libjprof.so
//   ad561000-ad562000 rw-p 00052000 08:06 10075      /Dpiperf/lib/libjprof.so
//   ad562000-ad575000 rw-p 00000000 00:00 0
//   ad575000-ad576000 ---p 00000000 00:00 0
//   b7814000-b7832000 r-xp 00000000 08:06 97         /lib/ld-2.10.1.so
//   b7832000-b7833000 r--p 0001d000 08:06 97         /lib/ld-2.10.1.so
//   b7833000-b7834000 rw-p 0001e000 08:06 97         /lib/ld-2.10.1.so
//   bf92a000-bf93f000 rw-p 00000000 00:00 0          [stack]
//   ffffe000-fffff000 r-xp 00000000 00:00 0          [vdso]
//
// * inode is non-zero if region mapped from a file
// * dev is major:minor device number (hex) if region mapped from a file
// * offset within file if region mapped from a file
//
// Keep all regions mapped from a file
// - non-zero inode and non-zero device
// Keep all regions with a pathname (real or virtual)
// Everything else is considered an anonymous region
//
int load_process_modules(uint32_t pid, int loadAnon)
{
   int rc = 0;
   FILE * map_file;                        // kernel /proc file handle
   char map_path[MAX_PATH_LEN];            // kernel /proc file path

   void * low, * high;                     // low and high module addresses
   char perms[5];                          // perms string (4 chars)
   void * offset;                          // module offset
   char dev[6];                            // device numbers (5 chars)
   unsigned int inode;                     // module inode number
   char pathname[MAX_PATH_LEN];            // module's full name
   char line[MAXLINE];                     // line buffer

   int vdso_size;
   char * buf;

   dbgmsg((">> load_process_modules(%d/0x%x)\n", pid, pid));

   // Open the map file for the process
   sprintf(map_path, "/proc/%u/maps", pid);
   map_file = fopen (map_path, "r");
   if (!map_file) {
      rc = A2N_FILEOPEN_ERROR;
      goto TheEnd;
   }

   // Process map file entries
   while (fgets(line, MAXLINE, map_file) != NULL) {
      pathname[0] = 0;
      sscanf(line, "%p-%p %s %p %s %u %s", &low, &high, perms, &offset, dev, &inode, pathname);

      if (strcmp(pathname, VDSO_SEGMENT_NAME) == 0) {
         // vdso segment
         dbgmsg(("-- load_process_modules: found %s at %p-%p\n", VDSO_SEGMENT_NAME, low, high));
         rc = 0;
         // it is not legal to directly read other process vdso content from current thread 
         // in create_fake_vdso_module by memcpy, correctly solution is using ptrace   
         //     if(ptrace(PTRACE_ATTACH, pid, NULL, NULL))
         //     {      perror("PTRACE_ATTACH"); 	      return(1); }
         //     loop read   //     ptrace(PTRACE_PEEKDATA, pid, (void *)addr1, NULL)) == -1)
         //     ptrace(PTRACE_DETACH, pid, NULL, NULL);
         // Current workaournd is only read current thread's [vdso]
         if (pid != getpid()) {
            goto TheEnd;
         }

         vdso_size = PtrToInt32(PtrSub(high, low));
         buf = create_fake_vdso_module(low, vdso_size);
         if (buf) {
            rc = A2nAddModule(pid, PtrToUint64(low), vdso_size, buf, TS_VDSO, 0, NULL);
            zfree(buf);
         }
      }
      else if (inode != 0 || (pathname[0] != '\0' && pathname[0] != ' ')) {
         // Named region
         // - Mapped from a file (non-zero inode (and pathname))
         // - Virtual named (zero inode and [pathname])
         rc = A2nAddModule(pid, PtrToUint64(low), (int)((uintptr)high-(uintptr)low)+1, pathname, 0, 0, NULL);
      }
      else if (loadAnon) {
         // Everything else is considered an anonymous region
         rc = A2nAddModule(pid, PtrToUint64(low), (int)((uintptr)high-(uintptr)low)+1, "", 0, 0, NULL);
      }

      if (rc)
         goto TheEnd;
   }

TheEnd:;
   fclose(map_file);
   dbgmsg(("<< load_process_modules(%d/0x%x) rc = %d\n", pid, pid, rc));
   return (rc);
}
#endif


//
// LoadMap()
// *********
//
// pid == -1 implies load modules for all pids
//
int LoadMap(uint pid, int loadAnon)
{
   int rc = 0;

#if defined(_WINDOWS)

   char data32_fn[256], data64_fn[256];
   char a2nlm32_cmd[256], a2nlm64_cmd[256];
   int have_32bit_mte = 0, have_64bit_mte = 0;
   char * buffer;
   FILE * data_fd = NULL;


   dbgmsg(("> LoadMap(%d/0x%x): starting\n", pid, pid));

   // Build output file names
   if (G(output_path)) {
      if (G(append_pid_to_fn)) {
         sprintf(data32_fn, "%s%slog-mte32bit_%d", G(output_path), DEFAULT_PATH_SEP_STR, (UINT32)GetCurrentProcessId());
         sprintf(data64_fn, "%s%slog-mte64bit_%d", G(output_path), DEFAULT_PATH_SEP_STR, (UINT32)GetCurrentProcessId());
      }
      else {
         sprintf(data32_fn, "%s%slog-mte32bit", G(output_path), DEFAULT_PATH_SEP_STR, (UINT32)GetCurrentProcessId());
         sprintf(data64_fn, "%s%slog-mte64bit", G(output_path), DEFAULT_PATH_SEP_STR, (UINT32)GetCurrentProcessId());
      }
   }
   else {
      if (G(append_pid_to_fn)) {
         sprintf(data32_fn, "log-mte32bit_%d", (UINT32)GetCurrentProcessId());
         sprintf(data64_fn, "log-mte64bit_%d", (UINT32)GetCurrentProcessId());
      }
      else {
         strcpy(data32_fn, "log-mte32bit");
         strcpy(data64_fn, "log-mte64bit");
      }
   }

   // Build commands to go get loaded modules
   sprintf(a2nlm32_cmd, "a2nlm32.exe %d %s", pid, data32_fn);
   sprintf(a2nlm64_cmd, "a2nlm64.exe %d %s", pid, data64_fn);

   // Erase output files (in case they already exist)
   remove(data32_fn);
   remove(data64_fn);

   dbgmsg(("- MTE files: %s and %s\n", data32_fn, data64_fn));

   //
   // Get MTE data and write it to a temporary file
   //
   // Valid OS/A2N combinations, and what MTE data is written:
   //
   //    OS   |   A2N    |  Written
   // --------+----------+-------------
   //  32 Bit |  32 Bit  | Native MTE (32 bit)
   //         |          | - a2nlm32.exe
   //         |          |
   //  64 Bit |  32 Bit  | Native MTE (64 bit)  and  32-bit MTE
   //         |          | - a2nlm64.exe             a2nlm32.exe
   //         |          |
   //  64 Bit |  64 Bit  | Native MTE (64 bit)  and  32-bit MTE
   //         |          | - a2nlm64.exe             a2nlm32.exe
   //
   // a2nlm64.exe
   // - 64-bit process
   // - Writes the native 64-bit MTE data
   //   * Only used when running on a 64-bit OS when running
   //     with either a 32-bit or a 64-bit A2N.
   // a2nlm32.exe
   // - 32-bit process
   // - Writes the native 32-bit MTE data
   //   * When running natively on a 32-bit OS
   //   * When running a 32-bit version of A2N on a 64-bit OS
   //
   // Both programs collect MTE (loaded module) data and write it
   // to a temporary file.
   //
#if defined(_64BIT)
   //    OS   |   A2N    |  Written
   // --------+----------+-------------
   //  64 Bit |  64 Bit  | Native MTE (64 bit)  and  32-bit MTE
   //         |          | - a2nlm64.exe             a2nlm32.exe
   //
   rc = run_program(a2nlm64_cmd);           // Get native 64-bit MTE data
   if (rc)
      errmsg(("*E* LoadMap: unable to collect 64-bit MTE data. run_program() rc = %d.\n", rc));
   else
      have_64bit_mte = 1;

   rc = run_program(a2nlm32_cmd);           // Get 32-bit MTE data
   if (rc)
      errmsg(("*E* LoadMap: unable to collect 32-bit MTE data. run_program() rc = %d.\n", rc));
   else
      have_32bit_mte = 1;
#else
   if (running_on_64bit_os()) {
      //    OS   |   A2N    |  Written
      // --------+----------+-------------
      //  64 Bit |  32 Bit  | Native MTE (64 bit)  and  32-bit MTE
      //         |          | - a2nlm64.exe             a2nlm32.exe
      rc = run_program(a2nlm64_cmd);        // Get native 64-bit MTE data
      if (rc)
         errmsg(("*E* LoadMap: unable to collect 64-bit MTE data. run_program() rc = %d.\n", rc));
      else
         have_64bit_mte = 1;

      rc = run_program(a2nlm32_cmd);        // Get 32-bit MTE data
      if (rc)
         errmsg(("*E* LoadMap: unable to collect 32-bit MTE data. run_program() rc = %d.\n", rc));
      else
         have_32bit_mte = 1;
   }
   else {
      //    OS   |   A2N    |  Written
      // --------+----------+-------------
      //  32 Bit |  32 Bit  | Native MTE (32 bit)
      //         |          | - a2nlm32.exe
      //
      rc = run_program(a2nlm32_cmd);        // Get native 32-bit MTE data
      if (rc)
         errmsg(("*E* LoadMap: unable to collect 32-bit MTE data. run_program() rc = %d.\n", rc));
      else
         have_32bit_mte = 1;
   }
#endif

   // Buffer used for reading MTE data files
   buffer = zmalloc(4096);
   if (buffer == NULL) {
      errmsg(("*E* LoadMap: unable to allocate temp buffer. Quitting.\n"));
      return (-1);
   }

   // Read MTE data file(s) and send the data to A2N ...
   // There will always be 32-bit MTE data, either because we're running
   // 32-bit natively or because we're on a 64-bit OS and always get
   // the 32-bit loaded modules.
   //
   // If on a 64-bit system then process the 64-bit (native) MTE first
   // because it contains all the system stuff plus the 64-bit components
   // of 32-bit applications.
   if (have_64bit_mte) {
      data_fd = fopen(data64_fn, "rb");
      if (data_fd == NULL) {
         infomsg(("*I* LoadMap(%d): can't open %s. Assuming no 64-bit MTE data.\n", pid, data64_fn));
      }
      else {
         infomsg(("*I* LoadMap(%d): Begin processing %s ...\n", pid, data64_fn));
         rc = read_mte_data_and_send_to_a2n(data_fd, buffer);
         infomsg(("*I* LoadMap(%d): Read %d 64-bit MTE data records.\n", pid, rc));
         fclose(data_fd);
      }
   }

   if (have_32bit_mte) {
      data_fd = fopen(data32_fn, "rb");
      if (data_fd == NULL) {
         infomsg(("*I* LoadMap(%d): can't open %s. Assuming no 32-bit MTE data.\n", pid, data32_fn));
      }
      else {
         infomsg(("*I* LoadMap(%d): Begin processing %s ...\n", pid, data32_fn));
         rc = read_mte_data_and_send_to_a2n(data_fd, buffer);
         infomsg(("*I* LoadMap(%d): Read %d 32-bit MTE data records.\n", pid, rc));
         fclose(data_fd);
      }
   }

   zfree(buffer);
   rc = 0;

#else

   struct dirent * * de;
   uint32_t locpid;
   int n;

   dbgmsg(("> LoadMap(%d/0x%x): starting\n", pid, pid));

   if (pid != -1) {
      // Load modules for a single process
      rc = load_process_modules(pid, loadAnon);
      if (rc) {
         msg_log("*E* LoadMap(): load_process_mopdules(%d/0x%x) failed. rc = %d.\n", pid, pid, rc);
      }
      goto TheEnd;
   }

   // Load modules for all processes
   n = scandir("/proc", &de, 0, alphasort);
   if (n < 0) {
      msg_log("*E* LoadMap() failed. Unable to access /proc directory.\n");
      rc = A2N_FILE_NOT_RIGHT;
      goto TheEnd;
   }

   // Process directory entries that are numbers
   while(n--) {
      locpid = atoi(de[n]->d_name);
      if (locpid) {                         // it's a number
         rc = load_process_modules(locpid, loadAnon);
         if (rc) {
            msg_log("*E* LoadMap(): load_process_mopdules(%d/0x%x) failed. rc = %d.\n", locpid, locpid, rc);
         }
      }
      free(de[n]);
   }
   free(de);

TheEnd:

#endif

   dbgmsg(("< LoadMap(%d/0x%x): done. rc = %d\n", pid, pid, rc));
   return (rc);
}


//
// is_wow64_module()
// *****************
//
int is_wow64_module(char * modname)
{
#if defined(_WINDOWS)
   char * lc, * p;

   lc = zstrdup(modname);
   if (lc == NULL)
      return (0);

   _strlwr(lc);
   p = strstr(lc, "\\syswow64");
   zfree(lc);
   if (p)
      return (1);
   else
#endif
      return (0);
}


//
// display_nosymbols_reason()
// **************************
//
void display_nosymbols_reason(MOD_NODE * mn, int rc)
{
   msg_log("*W* No symbols harvested for '%s' - rc = %d.\n", mn->name, rc);

#if defined(_WINDOWS)
   switch (mn->nosym_rc) {
      case NOSYM_REASON_IMAGE_NO_ACCESS:
         msg_log("    - Unable to access on-disk image file.\n");
         break;
      case NOSYM_REASON_IMAGE_CANT_MAP:
         msg_log("    - Unable to memory-map on-disk image file.\n");
         break;
      case NOSYM_REASON_DOS_HEADER_NOT_READABLE:
         msg_log("    - Unable to read on-disk image DOS_HEADER.\n");
         break;
      case NOSYM_REASON_IMAGE_NOT_PE:
         msg_log("    - On-disk image type is not PE.\n");
         break;
      case NOSYM_REASON_NOT_OK_TO_USE:
         msg_log("    - On-disk image is not OK to use.\n");
         break;
      case NOSYM_REASON_IMAGE_HAS_NO_SECTIONS:
         msg_log("    - On-disk image has no sections.\n");
         break;
      case NOSYM_REASON_EXPORTS_WANTED_BUT_NO_EXPORT_DIRECTORY:
         msg_log("    - Only wanted EXPORTS but there is no EXPORT_DIRECTORY.\n");
         break;
      case NOSYM_REASON_EXPORTS_WANTED_BUT_NO_EXPORTS:
         msg_log("    - Only wanted EXPORTS but there are no EXPORTed symbols.\n");
         break;
      case NOSYM_REASON_NO_SYMBOLS_AND_EXPORTS_NOT_WANTED:
         msg_log("    - No symbols found (in-image/symbol files). No check for EXPORTS - not wanted.\n");
         break;
      case NOSYM_REASON_NO_SYMBOLS_EXPORTS_WANTED_BUT_NO_EXPORTS:
         msg_log("    - No symbols found (in-image/symbol files) and no EXPORTS found.\n");
         break;
      case NOSYM_REASON_MAP_CANT_MAP:
         msg_log("    - Unable to memory-map MAP file.\n");
         break;
      case NOSYM_REASON_IMAGE_NAME_MAP_MISMATCH:
         msg_log("    - On-disk image name not same as image name in MAP file.\n");
         break;
      case NOSYM_REASON_MAP_TS_ZERO:
         msg_log("    - MAP file timestamp not found. Unsupported MAP file format.\n");
         break;
      case NOSYM_REASON_MAP_IMAGE_TS_MISMATCH:
         msg_log("    - On-disk image and MAP file timstamp mismatch.\n");
         break;
      case NOSYM_REASON_MAP_NO_SECTION_TABLE:
         msg_log("    - MAP file Section Table not found. Unsupported MAP file format.\n");
         break;
      case NOSYM_REASON_MAP_SECTION_DATA_MISMATCH:
         msg_log("    - MAP file Section Data does not match on-disk image.\n");
         break;
      case NOSYM_REASON_DBGHELP_SYMINITIALIZE_ERROR:
         msg_log("    - DbgHelp SymInitialize() error.\n");
         break;
      case NOSYM_REASON_DBGHELP_SYMLOADMODULE64_ERROR:
         msg_log("    - DbgHelp SymLoadModule64() error.\n");
         break;
      case NOSYM_REASON_DBGHELP_SYMGETMODULEINFO64_ERROR:
         msg_log("    - DbgHelp SymGetModuleInfo64() error.\n");
         break;
      case NOSYM_REASON_NO_SYMBOLS:
         msg_log("    - DbgHelp claims there are no symbols.\n");
         break;
      case NOSYM_REASON_ONLY_EXPORTS_BUT_NOT_WANTED:
         msg_log("    - Only EXPORTS available but user doesn't want them.\n");
         break;
      case NOSYM_REASON_MDD_TS_AND_PDBSIG_MISMATCH:
         msg_log("    - On-disk image MISC_DEBUG_DIR timestamp does not match loaded PDB signature.\n");
         break;
      case NOSYM_REASON_MISMATCH_BUT_NO_CVH_NOR_MDD:
         msg_log("    - Symbols mismatch and no CV_DEBUG_HEADER nor MISC_DEBUG_DIR to double-check.\n");
         break;
      case NOSYM_REASON_NB10_CVH_PDB_SIGNATURE_MISMATCH:
         msg_log("    - NB10 signature/age in on-disk image CV Header don't match those of loaded PDB.\n");
         break;
      case NOSYM_REASON_RSDS_CVH_PDB_SIGNATURE_MISMATCH:
         msg_log("    - RSDS signature/age in on-disk image CV Header don't match those of loaded PDB.\n");
         break;
      case NOSYM_REASON_UNKNOWN_PDB_FORMAT:
         msg_log("    - Not NB10/RSDS PDB signature. Unable to verify. Assume on-disk image and loaded PDB don't match.\n");
         break;
      case NOSYM_REASON_UNKNOWN_SYMBOL_FORMAT:
         msg_log("    - Unknown symbol format/information.\n");
         break;
      case NOSYM_REASON_DBGHELP_MISSING_SYMENUMSYMBOLS:
         msg_log("    - DbgHelp SymEnumSymbols() API not exported. Unable to harvest symbols.\n");
         break;
      default:
         break;
   }
#endif

   return;
}


//
// HarvestSymbols()
// ****************
//
// Calls harvester, performs validation and fixes up symbols as required.
// Should be called *ONLY* is harvesting is needed.
// Will either harvest/validate symbols or it won't.
//
void HarvestSymbols(MOD_NODE * mn, LMOD_NODE * lmn)
{
   int rc;
   char * symbol_search_path;


   dbgmsg(("> HarvestSymbols: mn=%p\n", mn));
   if (mn == NULL || lmn == NULL) {         // mn/lmn should not be NULL
      errmsg(("*E* HarvestSymbols: **BUG** mn/lmn NULL!\n"));
      return;
   }

   // Serialize use of the SNP/DSNP arrays.
   enter_critical_section();

   // Check if we already harvested for this module (on an A2nGetSymbol() on
   // a different thread) while we were blocked waiting our turn to harvest.
   if (mn->flags & A2N_FLAGS_SYMBOLS_HARVESTED) {
      dbgmsg(("< HarvestSymbols: Harvesting for '%s' already done while blocked\n", mn->name));
      goto TheEnd;
   }

   // To cover the immediate gather case
   if (!kernel_symbols_choice_made)
      make_kernel_symbols_choice();

   // If SysWow64 (32-bit emulation) module then flip the SysWow64 component
   // of the SymbolSearchPath to put \Symbols\SysWow64 ahead of \Symbols.
   if (is_wow64_module(mn->name))
      symbol_search_path = G(SymbolSearchPathWow64);
   else
      symbol_search_path = G(SymbolSearchPath);

#if defined(_WINDOWS64)
// Win64 compiler complains about casting the function pointers to void *
#pragma warning(disable : 4054)
#endif

   rc = GetSymbolsForModule((void *)mn,
                            mn->name,
                            symbol_search_path,
                            lmn->start_addr,
                            (void *)SymbolCallBack,
                            (void *)SectionCallBack);

#if defined(_WINDOWS64)
#pragma warning(default : 4054)
#endif

   // Don't do any fixup for kernel modules (linux-only) 'cause they've
   // already been fixed up.
   if (mn->type != MODULE_TYPE_KALLSYMS_MODULE) {
      mn->flags |= A2N_FLAGS_SYMBOLS_HARVESTED; // Don't do this again
      FixupSymbols(mn);                         // Fixup symbols
   }

   if (rc != 0) {                           // Harvester error. No symbols
      display_nosymbols_reason(mn, rc);
//    if (mn->flags & A2N_FLAGS_KERNEL) {
//       msg_se("*W* A2N: No symbols harvested for '%s' - rc=%d\n", mn->name, rc);
//    }
//    warnmsg(("*W* HarvestSymbols: No symbols harvested for '%s' - rc=%d\n", mn->name, rc));
      goto TheEnd;
   }

   if (mn->symcnt == 0) {                   // No harvester error but no  symbols
      if (mn->flags & A2N_FLAGS_KERNEL) {
         msg_se("*W* A2N: No symbols found for module '%s'\n", mn->name);
      }
      warnmsg(("*W* HarvestSymbols: No symbols found for module '%s'\n", mn->name));
      goto TheEnd;
   }

#if defined(_WINDOWS)
   FixupLines(mn);
#endif

   if (G(rename_duplicate_symbols))
      RenameDuplicateSymbols(mn);

   if (mn->flags & A2N_FLAGS_KERNEL) {
      //
      // Kernel. Validate symbols. Validation, if requested, either works or it doesn't.
      // If it does fail, there are messages written to stderr and to a2n.err.
      //
      ValidateKernelSymbols(lmn);
      if (mn->flags & A2N_FLAGS_VALIDATION_FAILED) {
         dbgmsg(("< HarvestSymbols: Symbols not valid for '%s' - VALIDATION FAILED\n", mn->name));
         if (G(quit_on_validation_error) == MODE_ON) {
            msg_se("*E* A2N: Symbols not valid for '%s' - VALIDATION FAILED\n", mn->name);
            msg_se("*E* A2N: Processing ended. Kernel symbol validation failed.\n");
            dbgmsg(("< HarvestSymbols: Processing ended. Kernel symbol validation failed and requested to stop.\n"));
            exit (-1);
         }
         goto TheEnd;
      }
   }

   if (G(collapse_ibm_mmi) == 1)
      CollapseMMIRange(mn);

   if (G(range_desc_root) != NULL)
      CollapseRangeIfApplicable(mn);

   if (COMPARE_NAME(mn->name, mn->hvname) == 0) {
      if (mn->flags & A2N_FLAGS_EXPORT_SYMBOLS) {
         errmsg(("*I* HarvestSymbols: %d *EXPORT* symbols found for '%s'\n", mn->symcnt, mn->name));
      } else {
         errmsg(("*I* HarvestSymbols: %d symbols found for '%s'\n", mn->symcnt, mn->name));
      }
   } else {
      errmsg(("*I* HarvestSymbols: %d symbols found for '%s' from '%s'\n", mn->symcnt, mn->name, mn->hvname));
   }
   mn->strsize += (int)strlen(mn->hvname) + 1;

   dbgmsg(("< HarvestSymbols: got some symbols ...\n"));

TheEnd:
   exit_critical_section();
   return;
}


//
// AddJittedMethodNode()
// *********************
//
// Add jitted method node to a loaded module.
// Jitted method nodes are not linked - there is only one per
// loaded module (for jitted methods).
//
JM_NODE * AddJittedMethodNode(LMOD_NODE * lmn,
                              char * name,
                              uint length,
                              char * code,
                              int jln_type,
                              int jln_cnt,
                              void * jln)
{
   JM_NODE * jmn;
   uint jln_size;
   int i;
   JTLINENUMBER * jtln;


   dbgmsg(("> AddJittedMethodNode: lmn=%p, name=%s, len=0x%x, code=%p, jln_type=%d, jln_cnt=0x%x, jln=%p\n",
            lmn, name, length, code, jln_type, jln_cnt, jln));

   if (lmn == NULL) {                       // lmn must not be NULL
      errmsg(("*E* AddJittedMethodNode: **BUG** NULL LMOD_NODE pointer\n"));
      return (NULL);
   }
   if (name == NULL  ||  *name == '\0') {   // name must not be NULL
      errmsg(("*E* AddJittedMethodNode: NULL method name\n"));
      return (NULL);
   }

   jmn = AllocJittedMethodNode();           // Allocate jitted method node
   if (jmn == NULL) {
      errmsg(("*E* AddJittedMethodNode: unable to allocate JM_NODE for '%s'\n", name));
      return (NULL);
   }

   // Initialize node. Anything not explicitly initialized is 0/NULL.
   jmn->length = length;

   jmn->name = zstrdup(name);
   if (jmn->name == NULL) {
      FreeJittedMethodNode(jmn);
      errmsg(("*E* AddJittedMethodNode: unable to allocate jitted method name for '%s'\n", name));
      return (NULL);
   }

   if (code != NULL) {
      jmn->code = CopyCode(code, length);
      if (jmn->code == NULL) {
         FreeJittedMethodNode(jmn);
         errmsg(("*E* AddJittedMethodNode: unable to copy %d bytes of code.\n", length));
         return (NULL);
      }
   }

   if (jln != NULL) {
      jln_size = jln_cnt * sizeof(JTLINENUMBER);

      // Line number info already in the correct format, so just copy
      jmn->jln =(JTLINENUMBER *)CopyCode((char *)jln, jln_size);
      if (jmn->jln == NULL) {
         FreeJittedMethodNode(jmn);
         errmsg(("*E* AddJittedMethodNode: unable to copy %d bytes of JTLINUMBER information.\n", jln_size));
         return (NULL);
      }
   }

   // Hang jitted method node off the loaded module node.
   lmn->jmn = jmn;

   // Add jitted method node to the beginning of the jitted method pool
   if (G(JmPoolRoot) != NULL)
      jmn->next = G(JmPoolRoot);
   else
      jmn->next = NULL;

   G(JmPoolRoot) = jmn;

   // Done
   G(JmPoolCnt)++;

   dbgmsg(("< AddJittedMethodNode: done OK\n"));
   return(jmn);
}


//
// CollapseRangeInModule()
// ***********************
//
// Returns NULL on errors.
//
RANGE_NODE * CollapseRangeInModule(MOD_NODE * mn,
                                   char * symbol_start,
                                   char * symbol_end,
                                   char * range_name)
{
   int i, slen, rlen, scnt = 0;
   char * c;
   SYM_NODE * sn_start, * sn_end;
   RANGE_NODE * rn = NULL;


   dbgmsg(("> CollapseRangeInModule: mn=%p  sym_start='%s'  sym_end='%s' range_name='%s'\n",
            mn, symbol_start, symbol_end, range_name));
   //
   // Find the symbol nodes for the starting and ending symbol.
   // Make sure we can actually collapse the symbols.
   //
   sn_start = GetSymbolNodeFromName(mn, symbol_start);
   if (sn_start == NULL) {
      infomsg(("*I* CollapseRangeInModule: '%s' not found in '%s'\n", symbol_start, mn->name));
      goto TheEnd;
   }

   sn_end = GetSymbolNodeFromName(mn, symbol_end);
   if (sn_end == NULL) {
      infomsg(("*I* CollapseRangeInModule: '%s' not found in '%s'\n", symbol_end, mn->name));
      goto TheEnd;
   }

   if (sn_end->index < sn_start->index) {
      infomsg(("*I* CollapseRangeInModule: '%s' has smaller offset than '%s'\n", symbol_end, symbol_start));
      goto TheEnd;
   }

   if (sn_start->section != sn_end->section) {
      infomsg(("*E* CollapseRangeInModule: '%s'/%d and '%s'/%d span sections.\n",
               symbol_start, sn_start->section, symbol_end, sn_end->section));
      goto TheEnd;
   }

   for (i = sn_start->index; i <= sn_end->index; i++) {
      if (mn->symroot[i]->rn != NULL) {
         errmsg(("*E* CollapseRangeInModule: '%s' already part of range '%s'. Can't be in 2 ranges.\n",
                 mn->symroot[i]->name, mn->symroot[i]->rn->name));
         goto TheEnd;
      }
   }

   // Allocate and initialize the RANGE_NODE to describe the new range.
   rn = AllocRangeNode();
   if (rn == NULL) {
      errmsg(("*E* CollapseRangeInModule: Unable to allocate RANGE_NODE for range '%s'.\n", range_name));
      goto TheEnd;
   }

   rn->name = zstrdup(range_name);
   rlen = (int)strlen(range_name);
   rn->code = sn_start->code;
   rn->offset_start = sn_start->offset_start;
   rn->offset_end = sn_end->offset_end;
   rn->length =  rn->offset_end - rn->offset_start + 1;
   rn->section = sn_start->section;
   rn->name_hash = hash_string(range_name);

   //
   // Change name of every symbol between symbol_start and symbol_end (included)
   // to whatever name they want.
   //
   scnt = 0;
   dbgmsg(("- CollapseRangeInModule: creating symbol range '%s' ...\n", range_name));
   for (i = sn_start->index; i <= sn_end->index; i++) {
      dbgmsg(("- CollapseRangeInModule: adding '%s' ofs=%08X ofe=%08X len=%08X\n",
              mn->symroot[i]->name, mn->symroot[i]->offset_start,
              mn->symroot[i]->offset_end, mn->symroot[i]->length));
      rn->ref_cnt++;
      mn->symroot[i]->rn = rn;
      mn->symroot[i]->flags |= A2N_FLAGS_SYMBOL_COLLAPSED;
      scnt++;

      slen = (int)strlen(mn->symroot[i]->name);
      c = zmalloc(slen + rlen + 2);
      if (c != NULL) {
         // Rename symbol if we have memory
         strcpy(c, range_name);
         strcat(c, ".");
         strcat(c, mn->symroot[i]->name);
         zfree(mn->symroot[i]->name);
         mn->symroot[i]->name = c;
         mn->strsize += rlen + 2;
      }
   }
   dbgmsg(("- CollapseRangeInModule: %d symbols collapsed\n"
           "  Range name         = %s\n"
           "  Range offset_start = %08X\n"
           "  Range offset_end   = %08X\n"
           "  Range length       = %08X\n"
           "  Range code         = %08X\n"
           "  Range section      = %d\n",
           scnt, rn->name, rn->offset_start, rn->offset_end, rn->length, rn->code, rn->section));

TheEnd:
   dbgmsg(("< CollapseRangeInModule: rn=%p (NULL is bad)\n", rn));
   return (rn);
}


//
// AddRangeDescNode()
// ******************
//
void AddRangeDescNode(char * fn,
                      char * module_fn,
                      char * symbol_start,
                      char * symbol_end,
                      char * range_name)
{
   RDESC_NODE * rd;


   rd = AllocRangeDescNode();
   if (rd == NULL) {
      errmsg(("*E* AddRangeDescNode: unable to malloc for RD_NODE. Quitting.\n"));
      exit (-1);
   }

   if (G(range_desc_root) == NULL)
      G(range_desc_root) = rd;
   else {
      rd->next = G(range_desc_root);
      G(range_desc_root) = rd;
   }

   rd->filename = fn;
   rd->mod_name = module_fn;
   rd->sym_start = symbol_start;
   rd->sym_end = symbol_end;
   rd->range_name = range_name;

   return;
}


//
// CollapseSymbolRange()
// *********************
//
// Collapses all symbols between symbol_start and symbol_end into a single
// symbol name. The symbol name can be given or, if missing, it is assume
// to be symbol_start.
//
// 'module_fn' is the filename (just the name, no path) of the desired
// module. The symbols will be collapsed in all instances of the named
// module.
//
int CollapseSymbolRange(char * module_fn,
                        char * symbol_start,
                        char * symbol_end,
                        char * symbol_new)
{
   int mcnt = 0;
   MOD_NODE * mn;
   RANGE_NODE * rn;


   dbgmsg(("> CollapseSymbolRange: module='%s'  sym_start='%s'  sym_end='%s'  sym_new='%s'\n",
            module_fn, symbol_start, symbol_end, symbol_new));

   // Find the 1st module node or quit
   mn = GetModuleNodeFromModuleFilename(module_fn, G(ModRoot));
   if (mn == NULL) {
      infomsg(("*E* CollapseSymbolRange: No instances of '%s' found. Queueing request\n", module_fn));
      AddRangeDescNode(zstrdup("QUEUED"), zstrdup(module_fn),
                              zstrdup(symbol_start), zstrdup(symbol_end),
                              zstrdup(symbol_new));
      return (0);
   }

   // Collapse symbols into single range in all modules found
   while (mn) {
      // If we haven't harvested symbols yet then do it now
      if (!(mn->flags & A2N_FLAGS_SYMBOLS_HARVESTED)) {
         dbgmsg(("- CollapseSymbolRange: Need to harvest symbols ...\n"));
         HarvestSymbols(mn, mn->last_lmn);
      }

      //
      // If symbol count is zero it means we failed to get any symbols
      // so we're not going to find any symbol for any address.
      //
      if (mn->symcnt == 0) {
         infomsg(("*I* CollapseSymbolRange: module '%s' contains no symbols\n", mn->name));
         mn = GetModuleNodeFromModuleFilename(module_fn, mn->next);
         continue;
      }

      //
      // If symbol validation failed make it look like there are no symbols
      // 'cause we don't use the symbols we got, unless we were told not
      // to validate, in which case they were willing to take any symbol -
      // good or not.
      //
      if (mn->flags & A2N_FLAGS_VALIDATION_FAILED  &&  G(validate_symbols)) {
         infomsg(("*I* CollapseSymbolRange: '%s' symbol validation failed - symbols not valid\n", mn->name));
         mn = GetModuleNodeFromModuleFilename(module_fn, mn->next);
         continue;
      }

      //
      // Find the symbol nodes for the starting and ending symbol.
      // Make sure we can actually collapse the symbols.
      //
      rn = CollapseRangeInModule(mn, symbol_start, symbol_end, symbol_new);
      if (rn == NULL) {
         dbgmsg(("- CollapseSymbolRange: Error - could not make range.\n"));
         mn = GetModuleNodeFromModuleFilename(module_fn, mn->next);
         continue;
      }

      // On to next module ...
      mcnt++;
      mn = GetModuleNodeFromModuleFilename(module_fn, mn->next);
   }

   // Done
   dbgmsg(("< CollapseSymbolRange: symbols collapsed in %d modules\n", mcnt));

   // Queue for later anyway
   AddRangeDescNode(zstrdup("QUEUED"), zstrdup(module_fn),
                           zstrdup(symbol_start), zstrdup(symbol_end),
                           zstrdup(symbol_new));

   if (mcnt == 0)
      return (A2N_COLLAPSE_RANGE_ERROR);
   else
      return (0);
}


//
// CollapseMMIRange()
// ******************
//
// Collapses IBM JVM Interpreter into a single symbol.
//
void CollapseMMIRange(MOD_NODE * mn)
{
#if !defined(_X86)
   return;

#else
   int i;
   RANGE_NODE * rn;
   char * s1  = MMI_RANGE_SYMSTART;
   char * s1a = MMI_RANGE_SYMSTART_ALT;
   char * s2  = MMI_RANGE_SYMEND;

   dbgmsg(("> CollapseMMIRange: mn->name=%s\n", mn->name));

   if (!ModuleFilenameMatches(mn, MMI_RANGE_MODNAME)) {
      dbgmsg(("< CollapseMMIRange: Not %s. Nothing to do.\n", MMI_RANGE_MODNAME));
      return;
   }

   // Set correct starting position of symbol name strings
   s1 += MMI_RANGE_SYM_POS;
   s1a += MMI_RANGE_SYM_POS;
   s2 += MMI_RANGE_SYM_POS;

   // Do it
   for (i = 0; i < MMI_RANGE_SYM_TRIES; i++) {
      rn = CollapseRangeInModule(mn, s1, s2, G(collapsed_mmi_range_name));
      if (rn != NULL)
         break;
      rn = CollapseRangeInModule(mn, s1a, s2, G(collapsed_mmi_range_name));
      if (rn != NULL)
         break;
      s1++;  s1a++;  s2++;
   }

   // Done
   dbgmsg(("< CollapseMMIRange: rn=%p (NULL=failed)\n",rn));
   return;
#endif
}


//
// CollapseRangeIfApplicable()
// ***************************
//
void CollapseRangeIfApplicable(MOD_NODE * mn)
{
   RDESC_NODE * rd;
   RANGE_NODE * rn;

   dbgmsg(("> CollapseRangeIfApplicable: mn=%p\n", mn));

   rd = G(range_desc_root);
   while (rd) {
      if (ModuleFilenameMatches(mn, rd->mod_name)) {
         dbgmsg(("- CollapseRangeIfApplicable: (%s) and trying %s ...\n", rd->mod_name, mn->name));
         rn = CollapseRangeInModule(mn, rd->sym_start, rd->sym_end, rd->range_name);
      }
      rd = rd->next;
   }

   dbgmsg(("< CollapseRangeIfApplicable: done\n"));
}


//
// ReadRangeDescriptorFile()
// *************************
//
int ReadRangeDescriptorFile(char * fn)
{
   FILE * fh;
   char m[64], s1[128], s2[128], n[64];
   char line[1024];
   int linecnt = 1;
   char * f, * c;

   dbgmsg(("> ReadRangeDescriptorFile: fn=%s\n", fn));

   if ((fh = fopen(fn, "rb")) == NULL) {
      errmsg(("*E* ReadRangeDescriptorFile: Unable to open '%s' for reading.\n", fn));
      return (A2N_FILEOPEN_ERROR);
   }

   f = zstrdup(fn);

   *s2 = *n = 0;
   c = fgets(line, 1024, fh);
   while (c != NULL) {
      if (*line != 0x0D) {
         sscanf(line, "%s %s %s %s", m, s1, s2, n);
         dbgmsg(("- ReadRangeDescriptorFile: (%d) %s %s %s %s\n", linecnt, m, s1, s2, n));

         if (*s2 == 0) {
            errmsg(("*E* ReadRangeDescriptorFile: Line %d format incorrect. Ignoring it.\n", linecnt));
         }
         else {
            if (*n == 0)
               AddRangeDescNode(f, zstrdup(m), zstrdup(s1), zstrdup(s2), zstrdup(s1));
            else
               AddRangeDescNode(f, zstrdup(m), zstrdup(s1), zstrdup(s2), zstrdup(n));
         }
      }

      linecnt++;
      *s2 = *n = 0;
      c = fgets(line, 1024, fh);
   }

   if (fh != NULL)
      fclose(fh);

   if (G(range_desc_root) == NULL) {
      warnmsg(("*W* ReadRangeDescriptorFile: No valid range descriptors found in %s.\n", fn));
      return (A2N_INVALID_RANGE_FILE);
   }
   else {
      dbgmsg(("< ReadRangeDescriptorFile: done\n"));
      return (0);
   }
}


//
// SymbolCallBack()
// ****************
//
// This function is called by the harvester once per symbol.
// Returns zero to allow harvester to continue enumerating symbols.
// Returns non-zero to cause harvester to stop enumerating symbols.
//
int SymbolCallBack(SYMBOL_REC * sr)
{
   SYM_NODE * sn;


   // Filter symbol some more if necessary and then add it
   sn = AddSymbolNode((MOD_NODE *)sr->handle,
                      sr->name,
                      sr->offset,
                      sr->length,
                      sr->section,
                      sr->type,
                      sr->code);
   if (sn == NULL)
      return (-1);                          // Stop enumerating
   else
      return (0);                           // Continue enumerating
}


//
// AddSymbolNode()
// ***************
//
// Add symbol node to a module cache node.
// Symbol nodes are linked in ascending start address order.
// Returns pointer to added node or NULL if unable to add.
//
SYM_NODE * AddSymbolNode(MOD_NODE * mn,
                         char * name,
                         uint offset,
                         uint length,
                         int section,
                         int type,
                         char * code)
{
   SYM_NODE * sn;


   dbgmsg(("> AddSymbolNode: mn=%p, name=%s, offset=%x, len=0x%x, sec=%d, type=%d, code=%p\n",
            mn, name, offset, length, section, type, code));

   if (name == NULL  ||  *name == '\0') {   // name must not be NULL
      errmsg(("*E* AddSymbolNode: NULL symbol name\n"));
      return (NULL);
   }

   sn = AllocSymbolNode();                  // Allocate node
   if (sn == NULL) {
      errmsg(("*E* AddSymbolNode: unable to allocate sym node. mod=%s  sym=%s.\n", mn->name, name));
      return (NULL);
   }

   // Initialize node. Anything not explicitly initialized is 0/NULL.
   //
   sn->offset_start = offset;
   sn->length = length;
   if (length)
      sn->offset_end = offset + length - 1;
   sn->section = section;
   sn->type = type;

#if defined(_WINDOWS)
   // Exports and "other" symbols are considered low quality
   if (type == SYMBOL_TYPE_MS_EXPORT ||
       type == SYMBOL_TYPE_MS_OTHER ||
       type == SYMBOL_TYPE_EXPORT) {
      sn->flags |= A2N_FLAGS_SYMBOL_LOW_QUALITY;
   }

   //
   // Take a guess at symbols that are really labels.
   // You see this kind of thing in the jvm where there are
   // labels like "$$$0004" which are aliases for other labels
   // with "nicer" names.
   //
   if (name[0] == '$' && name[1] == '$' && name[2] == '$')
      sn->flags |= SYMBOL_FLAGS_GUESS_LABEL;
#endif

   sn->name_hash = hash_string(name);
   sn->name = zstrdup(name);
   if (sn->name == NULL) {
      FreeSymbolNode(sn);
      errmsg(("*E* AddSymbolNode: unable to allocate sym name. symname=%s.\n", name));
      return (NULL);
   }
   mn->strsize += (int)strlen(name) + 1;
   if (code != NULL && type == SYMBOL_TYPE_USER_SUPPLIED) {
      //
      // User-supplied symbol with code: need to make copy of code and
      // hang it off symbol node.
      //
      sn->code = CopyCode(code, length);
      if (sn->code == NULL) {
         FreeSymbolNode(sn);
         errmsg(("*E* AddSymbolNode: unable to malloc %d bytes for code.\n", length));
         return (NULL);
      }
      mn->codesize += length;
   }
   else {
      //
      // Harvested symbol: if there is code it was harvested and it's hanging
      // off the appropriate section node.
      //
      sn->code = code;
   }

   //
   // Stick the pointer to the symbol node in the next available SNP array
   // element. All checking on this symbol is deferred until after all the
   // symbols have been harvested.
   //
   snp[mn->symcnt] = sn;
   if (G(rename_duplicate_symbols)) {
      dsnp[mn->symcnt] = sn;
   }
   mn->symcnt++;

   //
   // Done with this symbol.
   // If we've already filled the SNP array then stop enumerating and just
   // keep whatever symbols we have so far.
   //
   if (mn->symcnt == G(snp_elements)) {
      warnmsg(("*W* AddSymbolNode: SNP overflow in %s. Only the first %d symbols will be kept.\n", mn->name, G(snp_elements)));
      return (NULL);
      //##### We could realloc() the SNP array and keep going!
      //##### Maybe grow it by another 5000 elements?
   }

   // Done
   dbgmsg(("< AddSymbolNode: (OK - sn=%p, symcnt=%d)\n", sn, mn->symcnt));
   return(sn);
}


//
// FindInSymbolCache()
// *******************
//
// Find requested symbol in symbol cache.
// Returns SYM_NODE pointer (if found) or NULL (if not found).
//
SYM_NODE * FindInSymbolCache(MOD_NODE * mn, uint offset)
{
   int i;

   dbgmsg(("> FindInSymbolCache: cnt=%d\n", mn->cachecnt));

   for (i = 0; i < mn->cachecnt; i++) {
      if (offset >= mn->symcache[i]->offset_start  &&  offset <= mn->symcache[i]->offset_end) {
         dbgmsg(("< FindInSymbolCache: hit in %d iterations\n", i+1));
         return (mn->symcache[i]);          // hit
      }
   }

   dbgmsg(("< FindInSymbolCache: missed\n"));
   return (NULL);                           // miss
}


//
// AddToSymbolCache()
// ******************
//
// Add symbol to front of symbol cache.
//
void AddToSymbolCache(MOD_NODE * mn, SYM_NODE * sn)
{
   int i;

   dbgmsg(("> AddToSymbolCache: cnt=%d\n", mn->cachecnt));

   if (mn->cachecnt < SYMCACHE_SIZE) {
      i = mn->cachecnt;
      mn->cachecnt++;                       // Count must stay <= SYMCACHE_SIZE
   }
   else {
      i = SYMCACHE_SIZE - 1;
   }

   while (i > 0) {
      mn->symcache[i] = mn->symcache[i-1];  // Slide everyone down one
      i--;
   }

   mn->symcache[0] = sn;                    // Add new symbol node in front

   dbgmsg(("< AddToSymbolCache: cnt=%d\n", mn->cachecnt));

   return;
}


//
// GetSymbolNode()
// ***************
//
// Get symbol node given an address.  The address must fall in the range
// of the symbol.
// Returns:
// * pointer to sym node (if we get a hit)
// * NULL (if address does not fall in address range of any symbol).
// * -1 (if symbol validation failed and symbols are no good)
//
SYM_NODE * GetSymbolNode(LMOD_NODE * lmn, uint64 addr)
{
   MOD_NODE * mn;
   SYM_NODE * sn;
   uint64 req_offset;                       // offset of "addr" within module
   int iter;


   dbgmsg(("> GetSymbolNode: lmn=%p, addr=%"_LZ64X"\n", lmn, addr));

   if (lmn == NULL) {                       // lmn pointer must not be NULL
      errmsg(("*E* GetSymbolNode: **BUG** NULL lmn pointer! (addr=0x%"_LZ64X"\n", addr));
      return (NULL);
   }

   mn = lmn->mn;
   if (mn == NULL) {                        // Module node pointer must not be NULL
      errmsg(("*E* GetSymbolNode: **BUG** NULL mn pointer! (addr=0x%"_LZ64X"\n", addr));
      return (NULL);
   }

   // If anon segment or fake vsyscall page there are no symbols
   if (mn->type == MODULE_TYPE_ANON ||
       mn->type == MODULE_TYPE_VSYSCALL32 ||
       mn->type == MODULE_TYPE_VSYSCALL64) {
      dbgmsg(("< GetSymbolNode: module '%s' contains no symbols\n", mn->name));
      return (NULL);
   }

   // If we haven't harvested symbols yet then do it now
   if (!(mn->flags & A2N_FLAGS_SYMBOLS_HARVESTED)) {
      dbgmsg(("- GetSymbolNode: Need to harvest symbols ...\n"));
      HarvestSymbols(mn, lmn);
   }

   //
   // If symbol count is zero it means we failed to get any symbols
   // so we're not going to find any symbol for any address.
   //
   if (mn->symcnt == 0) {
      dbgmsg(("< GetSymbolNode: module '%s' contains no symbols\n", mn->name));
      return (NULL);
   }

   //
   // If symbol validation failed make it look like there are no symbols
   // 'cause we don't use the symbols we got, unless we were told not
   // to validate, in which case they were willing to take any symbol -
   // good or not.
   //
   if (mn->flags & A2N_FLAGS_VALIDATION_FAILED  &&  G(validate_symbols)) {
      dbgmsg(("< GetSymbolNode: '%s' symbol validation failed - symbols not valid\n", mn->name));
      return ((SYM_NODE *)-1);
   }

   //
   // At this point we've harvested symbols, there are symbols and they're
   // symbols we trust (except if symbol validation wasn't done, in which
   // case we look the other way and hope the symbols we got are OK).
   // Now try and find the symbol where the given address falls ...
   //
   // First check to see if it's one of the last 4 we've returned
   // from this module ...
   //

   req_offset = addr - lmn->start_addr;
   dbgmsg(("- GetSymbolNode: looking for symbol at offset 0x%0X\n",req_offset));
   if (mn->cachecnt > 0) {
      // We never do this in MT mode because we never add to the symbol cache.
      sn = FindInSymbolCache(mn, (uint)req_offset);
      if (sn != NULL) {
         mn->symcachehits++;
         if (sn->rn != NULL) {
            dbgmsg(("< GetSymbolNode: <found_in_cache> (%"_LZ64X" is '%s' [range '%s'] in '%s')\n", addr, sn->name, sn->rn->name, mn->name));
         }
         else {
            dbgmsg(("< GetSymbolNode: <found_in_cache> (%"_LZ64X" is '%s' in '%s')\n", addr, sn->name, mn->name));
         }
         if (G(lineno) && sn->cn == NULL)
            sn->cn = GetSectionNodeByNumber(mn, sn->section);

         return (sn);                          // req in the range for this symbol.
      }
   }

   //
   // Empty symbol cache or wasn't in the cache.
   // Look for it and add it to the cache if we find it.
   //
   if (mn->flags & A2N_SYMBOL_ARRAY_BUILT) {
      // Look for the symbol by doing a binary search.
      SYM_NODE **psym = lmn->mn->symroot;
      int low = 0,  high = lmn->mn->base_symcnt - 1,  middle;

      iter = 0;
      while (low <= high) {
         iter++;
         middle = (low + high) / 2;         // Calculate middle index
         dbgmsg(("- GetSymbolNode: <bsearch iter=%d> middle=%d %p is '%s'\n", iter, middle, psym[middle]->offset_start, psym[middle]->name));
         if (req_offset < psym[middle]->offset_start) {
            high = middle - 1;              // Must be in lower half
         }
         else if (req_offset <= psym[middle]->offset_end) {
            if (G(st_mode))
               AddToSymbolCache(mn, psym[middle]);  // Only do if single-threaded

            if (psym[middle]->rn != NULL)
               dbgmsg(("< GetSymbolNode: <bsearch iter=%d> (%"_LZ64X" is '%s' [range '%s'] in '%s')\n", iter, addr, psym[middle]->name, psym[middle]->rn->name, mn->name));
            else
               dbgmsg(("< GetSymbolNode: <bsearch iter=%d> (%"_LZ64X" is '%s' in '%s')\n", iter, addr, psym[middle]->name, mn->name));

            if (G(lineno) && psym[middle]->cn == NULL)
               psym[middle]->cn = GetSectionNodeByNumber(mn, psym[middle]->section);

            return (psym[middle]);          // Offset in the range for this symbol.
         }
         else {
            low = middle + 1;               // Must be in upper half
         }
      }
   }

   // If we make it here we didn't find a symbol
   dbgmsg(("< GetSymbolNode: (can't find symbol at %"_LZ64X" in '%s')\n", addr, mn->name));
   return (NULL);                           // Ran off the end and no match
}


//
// GetSymbolNodeFromName()
// ***********************
//
// Get symbol node given a symbol name.
// Returns pointer to sym node (if we get a hit) or NULL (if name not found).
//
SYM_NODE * GetSymbolNodeFromName(MOD_NODE * mn, char * name)
{
   uint hv;
   int i;


   dbgmsg(("> GetSymbolNodeFromName: mn=%p, name=%s\n", mn, name));

   if (mn == NULL) {                        // Module node pointer must not be NULL
      errmsg(("*E* GetSymbolNodeFromName: **BUG** NULL mn pointer!\n"));
      return (NULL);
   }

   // Now try and find the symbol node for the given symbol name
   hv = hash_string(name);
   if (mn->flags & A2N_SYMBOL_ARRAY_BUILT) {
      for (i = 0; i < mn->base_symcnt; i++) {
         if (mn->symroot[i]->name_hash == hv  &&  strcmp(mn->symroot[i]->name,name) == 0) {
            dbgmsg(("< GetSymbolNodeFromName: (sn=%p for '%s')\n", mn->symroot[i], mn->symroot[i]->name));
            return (mn->symroot[i]);        // Names are equal. This is the one
         }
      }
   }

   // Done.  If we make it here we didn't find what we were looking for
   dbgmsg(("< GetSymbolNodeFromName: (can't find symbol '%s' in module '%s'\n", name, mn->name));
   return (NULL);                           // Ran off the end and didn't find the symbol
}


//
// GetSubsymbolNode()
// ******************
//
// Get sub-symbol node (contained symbol) given an address and a symbol node
// with a list of contained symbols.
//
// Returns:
//   non-NULL: address falls in one of the contained symbols.
//       NULL: address falls in the symbol itself (ie. between the
//             start of the symbol and the start of the first contained
//             symbol).
//
SYM_NODE * GetSubsymbolNode(LMOD_NODE * lmn, SYM_NODE * sn, uint64 addr)
{
   uint req_offset;                         // Offset corresponding to addr
   SYM_NODE * cs;


   dbgmsg(("> GetSubsymbolNode: lmn=%p, sn=%p, addr=%"_LZ64X"\n", lmn, sn, addr));

   if (lmn == NULL || sn == NULL) {         // lmn and mn must not be NULL
      errmsg(("*E* GetSubsymbolNode: **BUG** NULL lmn/sn pointer!\n"));
      return (NULL);
   }

   // Now try and find the sub-symbol where the given address falls (if any).
   req_offset = (uint)(addr - lmn->start_addr);

   if (req_offset >= sn->offset_start && req_offset < sn->contained->offset_start) {
      dbgmsg(("< GetSubsymbolNode: (addr falls before 1st contained symbol)\n"));
      return (NULL);                        // addr falls before the first label.
   }

   cs = sn->contained;                      // Start with 1st contained symbol
   while (cs->next) {
      if (req_offset >= cs->offset_start && req_offset < cs->next->offset_start)
         break;                             // addr falls in cs (ie. between cs and cs->next)

      cs = cs->next;
   }

   // If we fall out then addr fell in the last contained symbol.
   dbgmsg(("< GetSubsymbolNode: cs=%p ('%s')\n", cs, cs->name));
   return (cs);
}


//
// GetLineNumber()
// ***************
//
void GetLineNumber(LMOD_NODE * lmn,
                   SYM_NODE * sn,
                   uint64 addr,
                   char ** srcfn,
                   int * srcline)
{
#if !defined(_WINDOWS) && !defined(_LINUX)
   // This stuff only works on Windows and Linux (for now)
   dbghv(("< GetLineNumber: Not supported\n"));
   return;

#else

#if defined(_WINDOWS)
   SRCLINE_NODE ** lines;
#endif
   uint offset;                             // offset of "addr" within module
   int iter;
   int cur, low, high, middle;
   JM_NODE * jmn;
   JTLINENUMBER * jln_array;


   dbghv(("> GetLineNumber: addr=%"_LZ64X" lmn=%p sn=%p\n", addr, lmn, sn));

   // Assume no line number information available
   *srcfn = NULL;
   *srcline = 0;


   //
   // ANONYMOUS section ...
   //
   if (lmn->type == LMOD_TYPE_ANON) {
      dbghv(("< GetLineNumber: Anonymous sections don't have line numbers\n"));
      return;
   }

   //
   // JITTED method ...
   // We don't use the input symbol node.
   //
   if (lmn->type == LMOD_TYPE_JITTED_CODE) {
      jmn = lmn->jmn;
      jln_array = jmn->jln;

      dbghv(("- GetLineNumber: sym=%s jln=%p\n", jmn->name, jln_array));

      if (jln_array == NULL) {
         dbghv(("< GetLineNumber: No line number information available\n"));
         return;
      }

      offset = (uint)(addr - lmn->start_addr);
      dbghv(("- GetLineNumber: looking for line at offset 0x%0X\n", offset));

      *srcfn = jmn->name;                   // filename is the method name

      if (jmn->jln_cnt >= 6) {
         // Method with 6 or more lines. Look up doing a binary search
         iter = 0;
         low = 0;
         high = jmn->jln_cnt - 1;
         while (low <= high) {
            iter++;
            middle = (low + high) / 2;      // Calculate middle index
            dbghv(("- GetLineNumber: <bsearch iter=%d> middle=%d  offset=0x%08X  line=%d\n", iter, middle, jln_array[middle].offset, jln_array[middle].lineno));
            if (offset < Uint64ToUint32(jln_array[middle].offset)) {
               high = middle - 1;           // Must be in upper half
            }
            else if (offset < Uint64ToUint32(jln_array[middle+1].offset)) {
               *srcline = jln_array[middle].lineno;
               dbghv(("< GetLineNumber: addr 0x%"_L64X" is line %d in %s\n", addr, *srcline, *srcfn));
               return;
            }
            else {
               low = middle + 1;            // Must be in lower half
            }
         }

         // If we make it here we didn't find a line
         *srcline = 0;
         dbghv(("< GetLineNumber: **BUG** BINARY SEARCH - No line number info for 0x%"_L64X"\n", addr));
         return;
      }
      else {
         // Method with 1-5 lines. Check doing a linear search
         high = jmn->jln_cnt - 1;

         for (cur = 0; cur < high; cur++) {
            if (offset >= jln_array[cur].offset && offset < jln_array[cur+1].offset) {
               *srcline = jln_array[cur].lineno;
               dbghv(("< GetLineNumber: addr 0x%"_L64X" is line %d in %s\n", addr, *srcline, *srcfn));
               return;
            }
         }

         // If we make it here then it has to be the very last line
         *srcline = jln_array[high].lineno;
         dbghv(("< GetLineNumber: addr 0x%"_L64X" is line %d in %s\n", addr, *srcline, *srcfn));
         return;
      }
   }

   //
   // Only thing left is a MODULE ...
   //
#if defined(_LINUX)
   dbghv(("< GetLineNumber: No line number info for %"_LZ64X"\n", addr));
   return;
#endif

#if defined(_WINDOWS)
   offset = 0;
   dbghv(("- GetLineNumber: sym=%s\n", sn->name));
   if (lmn->mn->li.linecnt != 0  &&  sn->line_cnt != 0) {
      // Both the module and the symbol have lines
      offset = (uint)((char *)addr - (char *)lmn->start_addr);
      dbghv(("- GetLineNumber: looking for line at offset 0x%0X\n", offset));

      // Look for the line by doing a binary search.
      lines = lmn->mn->li.srclines;
      low = sn->line_start;
      high = sn->line_end;
      iter = 0;
      while (low <= high) {
         iter++;
         middle = (low + high) / 2;         // Calculate middle index
         dbghv(("- GetLineNumber: <bsearch iter=%d> middle=%d  %p - %p\n", iter, middle, lines[middle]->offset_start, lines[middle]->offset_end));
         if (offset < lines[middle]->offset_start) {
            high = middle - 1;              // Must be in lower half
         }
         else if (offset <= lines[middle]->offset_end) {
            *srcfn = lines[middle]->sfn->fn;
            *srcline = lines[middle]->lineno;
            dbghv(("< GetLineNumber: 0x%0X is line %d in %s\n", offset, *srcline, *srcfn));
            return;                         // Offset in the range for this line
         }
         else {
            low = middle + 1;               // Must be in upper half
         }
      }
   }

   // If we make it here we didn't find a line
   *srcfn = NULL;
   *srcline = 0;
   dbghv(("< GetLineNumber: No line number info for 0x%0X\n", offset));
   return;
#endif
#endif
}


//
// GetLineNumbersForSymbol()
// *************************
//
LINEDATA * GetLineNumbersForSymbol(LMOD_NODE * lmn, SYM_NODE * sn)
{
#if !defined(_WINDOWS) && !defined(_LINUX)
   // This stuff only works on Windows and Linux (for now)
   dbghv(("<< GetLineNumbersForSymbol: ld = NULL\n"));
   return (NULL);

#else

#if defined(_WINDOWS)
   SRCLINE_NODE ** lines;
#endif

   LINEDATA * ld = NULL;
   int ld_size;
   JM_NODE * jmn;
   JTLINENUMBER * jln_array;
   int jln_end;
   int i, j;


   //
   // ANONYMOUS section ...
   //
   if (lmn->type == LMOD_TYPE_ANON) {
      dbghv(("< GetLineNumbersForSymbol: Anonymous sections don't have line numbers\n"));
      return (NULL);
   }

   //
   // JITTED method ...
   // We don't use the input symbol node.
   //
   if (lmn->type == LMOD_TYPE_JITTED_CODE) {
      jmn = lmn->jmn;
      jln_array = jmn->jln;

      if (jln_array == NULL) {
         dbghv(("< GetLineNumbersForSymbol: No line number information available for jitted method\n"));
         return (NULL);
      }

      ld_size = sizeof(LINEDATA) + ((jmn->jln_cnt-1) * sizeof(SIZEOF_LINE_INFO));
      dbghv(("* GetLineNumbersForSymbol: %d lines. malloc(%d) bytes for line number array\n", jmn->jln_cnt, ld_size));
      ld = (LINEDATA *)A2nAlloc(ld_size);
      if (ld == NULL) {
         errmsg(("*E* GetLineNumbersForSymbol: Unable to malloc() for line number array.\n"));
         return (NULL);
      }

      jln_end = jmn->jln_cnt - 1;
      ld->cnt = jmn->jln_cnt;
      for (i = 0, j = 0; i <= jln_end; i++, j++) {
         ld->line[j].lineno = jln_array[i].lineno;
         ld->line[j].offset_start = Uint64ToUint32(jln_array[i].offset);
         if (i == jln_end)
            ld->line[j].offset_end = Uint64ToUint32((((jmn->length - jln_array[i].offset) - 1) + ld->line[j].offset_start));
         else
            ld->line[j].offset_end = Uint64ToUint32((jln_array[i+1].offset - 1));
      }

      dbghv(("<< GetLineNumbersForSymbol: line_cnt = %d, ld = %p\n", jmn->jln_cnt, ld));
      return (ld);
   }

   //
   // Only thing left is a MODULE ...
   //
#if defined(_LINUX)
   dbghv(("** GetLineNumbersForSymbol: ld = NULL\n"));
   return (NULL);
#endif // _LINUX

#if defined(_WINDOWS)
   dbgmsg((">> GetLineNumbersForSymbol: lmn=%p, sn=%p\n", lmn, sn));

   if (sn->line_cnt == 0) {
      dbgmsg(("<< GetLineNumbersForSymbol: No line numbers for this symbol\n"));
      return (NULL);
   }

   lines = lmn->mn->li.srclines;

   ld_size = sizeof(LINEDATA) + ((sn->line_cnt-1) * SIZEOF_LINE_INFO);
   dbghv(("* GetLineNumbersForSymbol: %d lines. malloc(%d) bytes for line number array\n", sn->line_cnt, ld_size));
   ld = (LINEDATA *)A2nAlloc(ld_size);
   if (ld == NULL) {
      errmsg(("*E* GetLineNumbersForSymbol: Unable to malloc() for line number array.\n"));
      return (NULL);
   }

   ld->cnt = sn->line_cnt;
   for (i = sn->line_start, j = 0; i <= sn->line_end; i++, j++) {
      ld->line[j].lineno = lines[i]->lineno;
      ld->line[j].offset_start = lines[i]->offset_start - sn->offset_start;
      ld->line[j].offset_end = lines[i]->offset_end - sn->offset_start;
   }

   dbgmsg(("<< GetLineNumbersForSymbol: line_cnt = %d, ld = %p\n", sn->line_cnt, ld));
   return (ld);
#endif
#endif
}


//
// sym_compare()
// *************
//
// Comparator function for qsort.
// Inputs are pointers to pointers to SYM_NODEs.
// Sorts in symbol offset order.
//
static int sym_compare(const void * left, const void * right)
{
   SYM_NODE * l = *(SYM_NODE **)left;
   SYM_NODE * r = *(SYM_NODE **)right;

   if (l->offset_start > r->offset_start)
      return (1);     // left > right
   else if (l->offset_start < r->offset_start)
      return (-1);    // left < right
   else
      return (0);     // left == right
}


//
// FixupSymbols()
// **************
//
// Make a pass thru the entire symbol node list and fix up labels.
// We need to do that as a second pass because:
// - The harvester returns symbols as it finds them, thus we don't
//   necessarily get functions before labels within those functions.
// - The harvester returns labels with a length of zero, so we
//   have to wait until we have all the symbols before we go back and
//   calculate label lengths and move them to the label chain under
//   the symbol node.
//
// After symbols are fixed up, put them into an array so we can do
// binary search when looking them up later.
//
// * cs: is the "current" symbol node - the one we're working on.
// * ps: is the "previous" symbol node - one before the "current" one.
//       NULL if cs == mn->symroot.
// * ns: is the "next" symbol node - the one after the "current" one.
//       NULL if cs points to last node in list.
//
void FixupSymbols(MOD_NODE * mn)
{
   SYM_NODE * ps, * cs, * ns;               // Previous, Current and Next symbol nodes
   int p, c, n;                             // Previous, Current and Next SNP indices
   SYM_NODE ** sym_array;
   int acnt = 0, ccnt = 0, i, j;            // no aliased/contained symbols yet
   int last_symbol;


   dbgmsg(("> FixupSymbols: mn=%p\n", mn));

   if (mn == NULL) {                        // mn must not be NULL
      errmsg(("*E* FixupSymbols: **BUG** NULL mn pointer!\n"));
      return;
   }

   if (mn->symcnt == 0) {                   // If there are no symbols there's nothing to do
      dbgmsg(("< FixupSymbols: (module '%s' contains no symbols)\n", mn->name));
      return;
   }

#if defined(_LINUX) || defined(_ZOS)
   //
   // Go thru fixup even if the symbols came from a map because there can be duplicate
   // (ie. aliased) symbols that need to be moved. The harvester has already calculated
   // lengths so we should not be doing that here.
   //
#endif

   // Sort the symbols in ascending offset order.
   qsort((void *)snp, mn->symcnt, sizeof(SYM_NODE *), sym_compare);

   // Run thru all the symbols and fix up as needed
   last_symbol = mn->symcnt - 1;            // SNP index of last symbol
   p = -1;  ps = NULL;                      // previous symbol
   c = 0;                                   // current symbol
   n = 1;                                   // next symbol

   while (1) {                              // We exit with explicit break!
      cs = snp[c];
      if (c == last_symbol || n > last_symbol) {
         //
         // This is the last symbol OR symbols following the current symbol have
         // been aliased to it or move to it's contained list. If needed, calculate
         // the length. Either way we're done.
         //
         if (cs->length == 0) {
            cs->length = CalculateLabelLength(mn, cs, NULL);
            if (cs->length)
               cs->offset_end = cs->offset_start + cs->length - 1;
         }
         break;                             // ***** This is the only way out *****
      }

      ns = snp[n];
      if (ns->offset_start == cs->offset_start) {
         //
         // Next symbol has the same offset as the current symbol.
         // Move one of them, without fixing it up, to the aliases chain:
         // * Same types:
         //   - Move one under the other (don't favor either)
         // * Label followed by any function:
         //   - Move the label under function (favor functions)
         // * Weak function followed by function:
         //   - Move weak function under "strong" function (favor "strong" functions)
         //
#if defined(_LINUX) || defined(_ZOS)
         //
         // ************************ LINUX ************************
         //    cs->type       ns->type      move   in English
         //  -------------  -------------  ------  ---------------------------------
         //* LABEL          FUNCTION       CsToNs  Move label under function
         //* LABEL          WEAK_FUNCTION  CsToNs  Move label under weak function
         //  LABEL          LABEL          NsToCs  Move one label under the other
         //  FUNCTION       FUNCTION       NsToCs  Move one function under the other
         //  FUNCTION       WEAK_FUNCTION  NsToCs  Move weak function under function
         //  FUNCTION       LABEL          NsToCs  Move label under function
         //* WEAK_FUNCTION  FUNCTION       CsToNs  Move weak function under function
         //  WEAK_FUNCTION  WEAK_FUNCTION  NsToCs  Move one weak function under the other
         //  WEAK_FUNCTION  LABEL          NsToCs  Move label under weak function
         //
         // Note:
         //   Whether we favor weak function or "strong" functions doesn't seem to
         //   make a difference.  I tried to objdump libc.so and objdump labels some
         //   function with weak function names and some with "strong" functio names.
         //   I can't see a pattern on how it (objdump) chooses a name, but that
         //   doesn't mean there isn't one!
         //
         if ( (cs->type == SYMBOL_TYPE_LABEL && ns->type == SYMBOL_TYPE_FUNCTION) ||
              (cs->type == SYMBOL_TYPE_LABEL && ns->type == SYMBOL_TYPE_FUNCTION_WEAK) ||
              (cs->type == SYMBOL_TYPE_FUNCTION_WEAK && ns->type == SYMBOL_TYPE_FUNCTION) )
         {
            AliasCurrentToNext(cs, ns, mn);      // Move 1st (current) under 2nd (next)
            acnt++;
            snp[c] = NULL;                       // Not there anymore
            c = n;                               // Next becomes current
            n++;                                 // Next is next
         }
         else {
            AliasNextToCurrent(ns, cs, mn);      // Same types or 1st one is "stronger".
                                                 // Move 2nd one under the 1st.
            acnt++;
            snp[n] = NULL;                       // Not there anymore
            n++;                                 // Move next
         }

#else
         //
         // ************************ WINDOWS ************************
         //    cs->type       ns->type      move   in English
         //  -------------  -------------  ------  ---------------------------------
         //  LABEL          FUNCTION       CsToNs  Move label under function
         //  LABEL          LABEL          NsToCs  Move one label under the other
         //  FUNCTION       FUNCTION       NsToCs  Move one function under the other
         //  FUNCTION       LABEL          NsToCs  Move label under function
         //
         // Notes:
         //   1) The COFF format (as implemented by Microsoft) does not differentiate
         //      between functions and labels - Everything appears to be a function.
         //   2) Don't alias anything to any symbol whose name is "__" (two underscores
         //      back to back) or any import stub (name starts with "_imp__"),
         //
         if (cs->type == SYMBOL_TYPE_LABEL && ns->type == SYMBOL_TYPE_FUNCTION) {
            AliasCurrentToNext(cs, ns, mn);      // Move 1st (current) under 2nd (next)
            acnt++;
            snp[c] = NULL;                       // Not there anymore
            c = n;                               // Next becomes current
            n++;                                 // Next is next
         }
         else {
            if (strcmp(cs->name, "__") == 0 ||             // "__"
                strnicmp(cs->name, "_imp__", 6) == 0 ||    // import stub
                cs->type == SYMBOL_TYPE_PE_ILT) {          // <ilt>
               // Never alias anything to "__" or import stub or "<ilt>"
               AliasCurrentToNext(cs, ns, mn);      // Move 1st (current) under 2nd (next)
               acnt++;
               snp[c] = NULL;                       // Not there anymore
               c = n;                               // Next becomes current
               n++;                                 // Next is next
            }
            else {
               AliasNextToCurrent(ns, cs, mn);      // Same types or 1st one is "stronger".
                                                    // Move 2nd one under the 1st.
               acnt++;
               snp[n] = NULL;                       // Not there anymore
               n++;                                 // Move next
            }
         }
#endif

         continue;                          // the while loop
      } // of cs and ns have same offset

      if (cs->length == 0) {
         //
         // Symbol length is 0 so we need to calculate the length.
         // COFF symbols we harvest don't all have a length so the
         // harvester sends length=0 and that's our hint to calculate
         // the actual length here.
         //
         cs->length = CalculateLabelLength(mn, cs, ns);
         if (cs->length)
            cs->offset_end = cs->offset_start + cs->length - 1;
      }

#if defined(_LINUX) || defined(_ZOS)
      if (p >= 0) {
         // There is a valid previous symbol (one to the left of current symbol).
         ps = snp[p];
         if (ps->type != SYMBOL_TYPE_LABEL  &&  cs->type == SYMBOL_TYPE_LABEL) {
            //
            // ps is a function and cs is a label.
            // Let's check whether or not the label is contained in the function.
            //
            if (cs->offset_start < ps->offset_end) {
               //
               // cs is contained within ps so move the cs node to the
               // "contained" chain under ps.
               //
               MoveToContained(cs, ps, mn); // cs will be contained by ps
               ccnt++;
               snp[c] = NULL;                       // Not there anymore
               c = n;                               // Next becomes current
               n++;                                 // Next is next
               continue;
            }
         }
      }
#endif

      // Different offsets OR already fixed up length if we had to
      p = c;                                // What was current is now previous
      c = n;                                // What was next is now current
      n++;                                  // New next is next-next
   } // while


   //*********************************************************************
   //*****                 Common Linux/Windows code                 *****
   //*********************************************************************


   //
   // At this point in time we've completed fixing up symbols:
   // - All base labels/symbols have lengths
   // - Symbols have been alised/contained as needed
   // - Symbol node pointer list is sorted in ascending symbol offset order
   //
   // What we're going to do next is to create a permanent SNP array for this
   // module and copy all the base symbol pointers to it.
   //
   // Determine actual number of non-aliased and non-contained symbols
   // and allocate the symbol pointer array to hold them.
   //
   mn->base_symcnt = mn->symcnt - acnt - ccnt;   // total - aliased - contained
   dbgmsg(("- FixupSymbols: allocating symbol pointer array (%d elements, %d bytes)\n",
           mn->base_symcnt, (mn->base_symcnt * sizeof(SYM_NODE *))));
   sym_array = (SYM_NODE **)zmalloc(mn->base_symcnt * sizeof(SYM_NODE *));
   if (sym_array == NULL) {
      errmsg(("*E* FixupSymbols: unable to malloc snp array for '%s'. Discarding symbols.\n", mn->name));
      mn->symcnt = mn->base_symcnt = 0;
      mn->symroot = NULL;
      return;
   }
   //***** As an alternative in the case where the malloc() fails, we could just
   //***** form a linked list of symbol nodes and use them as they are.  It would
   //***** mean linear searches for symbols in that particular module, but at least
   //***** we'd keep the symbols around. Just a thought.  The mechanics for that
   //***** are in place by using the A2N_SYMBL_ARRAY_BUILT flag in the module node.

   //
   // Copy base (non-aliased/non-contained) symbols to symbol node pointer array.
   // Symbols are already in ascending address/offset order so all I need
   // to do is copy.
   //
   j = 0;
   for (i = 0; i < mn->symcnt; i++) {
      if (snp[i] == NULL)
         continue;                          // ignore aliases/contained
      else {
         sym_array[j] = snp[i];
         sym_array[j]->index = j;           // Symbol index within snp array
         j++;
      }
   }
   mn->symroot = sym_array;                 // Set root to point to symbol array
   mn->flags |= A2N_SYMBOL_ARRAY_BUILT;

   // Done
   dbgmsg(("< FixupSymbols: done\n"));
   return;
}


//
// CalculateLabelLength()
// **********************
//
// mn: module node
// ln: symbol node for label whose length we'd like to calculate
// nn: symbol node following ln
//
// Returns either the calculated length or zero (0) on errors.
//
uint CalculateLabelLength(MOD_NODE * mn, SYM_NODE * ln, SYM_NODE * nn)
{
   SEC_NODE * secn;
   uint length;


   dbgmsg(("> CalculateLabelLength: mn=%p, ln=%p, nn=%p\n", mn, ln, nn));
   if (nn == NULL  ||  ln->section != nn->section) {   // Last symbol OR Section changes
      //
      // Either it's the very last symbol OR it's the last symbol in
      // a particular section.  Either way calculate the length to
      // the end of the section.
      //
      dbgmsg(("- CalculateLabelLength: sym='%s'  section=%d\n", ln->name, ln->section));
      secn = GetSectionNodeByNumber(mn, ln->section);
      if (secn == NULL) {
         length = 0;
         warnmsg(("*W* CalculateLabelLength: section 0x%x not found for module '%s'. Label '%s' not fixed.\n", ln->section, mn->name, ln->name));
      }
      else {
         length = secn->offset_end - ln->offset_start + 1;
      }
   }
   else {
      //
      // Not last symbol AND in same section as the next one.
      // Calculate length to the next symbol.
      //
      dbgmsg(("- CalculateLabelLength: sym='%s'  section=%d\n", ln->name, ln->section));
      length = nn->offset_start - ln->offset_start;
   }

   dbgmsg(("< CalculateLabelLength: length=0x%x (%d)\n", length, length));
   return (length);
}


//
// AliasNextToCurrent()
// ********************
//
// Moves a symbol node (ns) and its alias chain (if any) to the
// end of another symbol node (cs) alias chain.
// Either cs or ns or both can currently have an alias chain.
//
// Start with:                       End with:
// -----------                       ---------
//
//      cs         ns                     cs
//      |          |                      |
//      V          V                      V
//  ... +-----+    +-----+            ... +-----+
//      |     |    |     |                |     |
//      | S1  |    | S2  |                | S1  |
//      |     |    |     |                | A   |
//      +-----+    +-----+                +-*---+
//                                          |
//                                          V
//                                    ns -> +-----+
//                                          |     |
//                                          | S2  |
//                                          |     |
//                                          +-----+
//
void AliasNextToCurrent(SYM_NODE * ns, SYM_NODE * cs, MOD_NODE * mn)
{
   if (ns == NULL || cs == NULL) {
      errmsg(("*E* AliasNextToCurrent: **BUG** one or more input node ptrs is NULL. Quitting.\n"));
      return;
   }

   dbgmsg(("- AliasNextToCurrent: '%s' being aliased to '%s'\n", ns->name, cs->name));

   ns->next = ns->aliases;                  // Move aliased node aliases (if any) out to next
   ns->aliases = NULL;                      // Aliased node no longer has alias chain

   if (cs->aliases == NULL)
      cs->aliases = ns;                     // No previous aliases. Start new chain
   else
      cs->alias_end->next = ns;             // Previous aliases. Add new one at the end

   if (ns->alias_end == NULL)
      cs->alias_end = ns;                   // New alias has no chain. It is our end
   else
      cs->alias_end = ns->alias_end;        // New alias has chain. Its end is our end

   ns->alias_end = NULL;
   cs->alias_cnt += ns->alias_cnt + 1;      // Take over aliased node aliases (if any) and aliased node (+1)
   ns->alias_cnt = 0;                       // ditto
   ns->flags |= A2N_FLAGS_SYMBOL_ALIASED;   // ALiased node is now an alias itself

   return;
}


//
// AliasCurrentToNext()
// ********************
//
// Moves a symbol node (cs) and its alias chain (if any) to the
// end of another symbol node (ns) alias chain.
// Either cs or ns or both can currently have an alias chain.
//
// Start with:                       End with:
// -----------                       ---------
//
//      cs         ns                     ns
//      |          |                      |
//      V          V                      V
//  ... +-----+    +-----+            ... +-----+
//      |     |    |     |                |     |
//      | S1  |    | S2  |                | S2  |
//      |     |    |     |                | A   |
//      +-----+    +-----+                +-*---+
//                                          |
//                                          V
//                                    cs -> +-----+
//                                          |     |
//                                          | S1  |
//                                          |     |
//                                          +-----+
//
void AliasCurrentToNext(SYM_NODE * cs, SYM_NODE * ns, MOD_NODE * mn)
{
   if (cs == NULL || ns == NULL || mn == NULL) {
      errmsg(("*E* AliasCurrentToNext: **BUG** one or more input node ptrs is NULL. Quitting.\n"));
      return;
   }

   dbgmsg(("- AliasCurrentToNext: '%s' being aliased to '%s'\n", cs->name, ns->name));

   cs->next = cs->aliases;                  // Move aliased node aliases (if any) out to next
   cs->aliases = NULL;                      // Aliased node no longer has alias chain

   if (ns->aliases == NULL)
      ns->aliases = cs;                     // No previous aliases. Start new chain
   else
      ns->alias_end->next = cs;             // Previous aliases. Add new one at the end

   if (cs->alias_end == NULL)
      ns->alias_end = cs;                   // New alias has no chain. It is our end
   else
      ns->alias_end = cs->alias_end;        // New alias has chain. Its end is our end

   cs->alias_end = NULL;
   ns->alias_cnt += cs->alias_cnt + 1;      // Take over aliased node aliases (if any) and aliased node (+1)
   cs->alias_cnt = 0;                       // ditto
   cs->flags |= A2N_FLAGS_SYMBOL_ALIASED;   // ALiased node is now an alias itself

   return;
}


#if defined(_LINUX) || defined(_ZOS)
//
// MoveToContained()
// *****************
//
// Moves a symbol node (cs) to the end of the contained symbol
// node chain of another symbol node (ps).  The symbol is moved
// to the end of the chain in order to maintain them in ascending
// offset order.
//
void MoveToContained(SYM_NODE * cs, SYM_NODE * ps, MOD_NODE * mn)
{
   if (cs == NULL || ps == NULL) {
      errmsg(("*E* MoveToContained: **BUG** one or more input node ptrs is NULL. Quitting.\n"));
      return;
   }

   dbgmsg(("- MoveToContained: '%s' now contained in '%s'\n", cs->name, ps->name));

   if (ps->contained == NULL) {
      cs->next = NULL;                      // This is the first (and last) contained
      ps->contained = ps->contained_end = cs;
   }
   else {
      ps->contained_end->next = cs;         // Put contained at end of list
      ps->contained_end = cs;
   }

   ps->sub_cnt++;
   cs->flags |= A2N_FLAGS_SYMBOL_CONTAINED;

   return;
}
#endif // _LINUX


//
// sym_name_compare()
// ******************
//
// Comparator function for qsort.
// Inputs are pointers to pointers to SYM_NODEs.
// Sorts in alphabetical order.
//
static int sym_name_compare(const void * left, const void * right)
{
   SYM_NODE * l = *(SYM_NODE **)left;
   SYM_NODE * r = *(SYM_NODE **)right;

   return ((int)strcmp(l->name, r->name));
}


//
// RenameRangeOfSymbols()
// **********************
//
// Takes a range of symbols with the same name and renames them all
// so they have unique names.
//
// If there is no line number information for the symbols:
// - Append "_**DUP**_0xXXXXXXXX" to the name, where 0xXXXXXXXX is the
//   starting offset of the symbol within the module.
//
// If there is line number information for the symbols:
// - Append "_**DUP**_sourcefilename_linenumber" to the name.
//

#define DUP_TAG         "_**DUP**_"
#define DUP_TAG_LEN     9
#define DUP_OFFSET_LEN  10     // 0xXXXXXXXX
#define DUP_LINENO_LEN  5

void RenameRangeOfSymbols(MOD_NODE * mn, int start, int end)
{
   int i, namelen;
   char * tname;
   char temp[64];
#if defined(_WINDOWS)
   SRCLINE_NODE ** lines;
#endif


   dbgmsg(("> RenameRangeOfSymbols: start = %d  end = %d\n", start, end));

#if defined(_WINDOWS)
   for (i = start; i <= end; i++) {
      namelen = (int)strlen(dsnp[i]->name);
      if (dsnp[i]->line_cnt == 0) {
         // No line number info. Append "_**DUP**_0xXXXXXXXX"
         tname = zmalloc(namelen + DUP_TAG_LEN + DUP_OFFSET_LEN + 1);
         strcpy(tname, dsnp[i]->name);
         strcat(tname, "_**DUP**_");
         sprintf(temp, "0x%08X", dsnp[i]->offset_start);
         strcat(tname, temp);
         dbgmsg(("- RenameRangeOfSymbols: renaming '%s' to '%s'\n", dsnp[i]->name, tname));
         zfree(dsnp[i]->name);
         dsnp[i]->name = tname;
      }
      else {
         // There is line nunber information. Append "_**DUP**_sourcefilename_linenumber"
         lines = mn->li.srclines;
         tname = zmalloc(namelen + DUP_TAG_LEN + strlen(lines[dsnp[i]->line_start]->sfn->fn) + 1 + DUP_LINENO_LEN + 1);
         strcpy(tname, dsnp[i]->name);
         strcat(tname, "_**DUP**_");
         strcat(tname, lines[dsnp[i]->line_start]->sfn->fn);
         strcat(tname, "_");
         sprintf(temp, "%05d", lines[dsnp[i]->line_start]->lineno);
         strcat(tname, temp);
         dbgmsg(("- RenameRangeOfSymbols: renaming '%s' to '%s'\n", dsnp[i]->name, tname));
         zfree(dsnp[i]->name);
         dsnp[i]->name = tname;
      }
   }
#else
   for (i = start; i <= end; i++) {
      namelen = (int)strlen(dsnp[i]->name);
      // Allways append "_**DUP**_0xXXXXXXXX"
      tname = zmalloc(namelen + DUP_TAG_LEN + DUP_OFFSET_LEN + 1);
      strcpy(tname, dsnp[i]->name);
      strcat(tname, "_**DUP**_");
      sprintf(temp, "0x%08X", dsnp[i]->offset_start);
      strcat(tname, temp);
      dbgmsg(("- RenameRangeOfSymbols: renaming '%s' to '%s'\n", dsnp[i]->name, tname));
      zfree(dsnp[i]->name);
      dsnp[i]->name = tname;
   }
#endif

   dbgmsg(("< RenameRangeOfSymbols: done\n"));
   return;
}


//
// RenameDuplicateSymbols()
// ************************
//
// Checks all symbols to see if any of them have the same name. If so,
// all symbols with the same name are renamed to something unique so
// we can tell them apart.
//
void RenameDuplicateSymbols(MOD_NODE * mn)
{
   int c, n, x;                             // Current, Next and eXtra SNP indices


   dbgmsg(("> RenameDuplicateSymbols: mn = %p\n", mn));

   if (!G(rename_duplicate_symbols)) {
      dbgmsg(("< RenameDuplicateSymbols: don't need to do this\n"));
      return;
   }

   if (mn == NULL) {                        // mn must not be NULL
      errmsg(("*E* RenameDuplicateSymbolss: **BUG** NULL mn pointer!\n"));
      return;
   }

   if (mn->symcnt == 0) {                   // If there are no symbols there's nothing to do
      dbgmsg(("< RenameDuplicateSymbols: (module '%s' contains no symbols)\n", mn->name));
      return;
   }

   //
   // Sort the symbols in ascending name order so we can check for
   // duplicate names quickly. Accoring to the Microsoft documentation the
   // SYMOPT_UNDNAME option has no effect on local (static) or global symbols
   // because they're always stored in undecorated form. What that means is
   // that each instance of an overloaded method appears with the same name as
   // every other instance. You go figure why they did that.
   // Do only if sn->type SYMBOL_TYPE_MS_PDB or SYMBOL_TYPE_MS_DIA
   //
   qsort((void *)dsnp, mn->symcnt, sizeof(SYM_NODE *), sym_name_compare);
   for (c = 0, n = 1; n < mn->symcnt; c++, n++) {
      if (strcmp(dsnp[c]->name, dsnp[n]->name) != 0)
         continue;                       // Current and next not equal. Keep going

      // Current and next are equal. Find all the ones that are equal to current.
      // We know at least next is equal.
      for (x = n + 1; x < mn->symcnt; x++) {
         if (strcmp(dsnp[c]->name, dsnp[x]->name) == 0)
            n++;
         else
            break;
      }

      // Rename the entire group of duplicates. It's at least 2 but
      // could be more.
      RenameRangeOfSymbols(mn, c, n);
      c = n;                             // Start next search after this range
                                         // c will be incremented automatically
      n++;
   }

   dbgmsg(("< RenameDuplicateSymbols: done\n"));
   return;
}


//
// SectionCallBack()
// *****************
//
// This function called by the harvester once per executable section.
// Returns zero to allow harvester to continue enumerating sections.
// Returns non-zero to cause harvester to stop enumerating sections.
//
int SectionCallBack(SECTION_REC * sr)
{
   SEC_NODE * secn;


   // Filter section some more if necessary and then add it
   secn = AddSectionNode((MOD_NODE *)sr->handle,
                         sr->number,
                         sr->asec,
                         sr->executable,
                         sr->start_addr,
                         sr->offset,
                         sr->size,
                         sr->flags,
                         sr->name,
                         sr->loc_addr);
   if (secn == NULL)
      return (-1);                          // Error. Stop enumerating
   else
      return (0);                           // Continue enumerating
}


//
// AddSectionNode()
// ****************
//
// Add section node to module node.
// Returns pointer to sec node or NULL if unable to add.
//
SEC_NODE * AddSectionNode(MOD_NODE * mn,    // Module handle (node)
                         int number,        // Section number
                         void * asec,       // BFD asection pointer
                         int executable,    // Executable section or not
                         uint64 addr,       // Section base address/offset
                         uint offset,       // Section offset
                         uint length,       // Section length
                         uint flags,        // Section flags
                         char * name,       // Section name (ex: .text, .code, etc)
                         void * loc_addr)   // Where we have it loaded/or NULL
{
   SEC_NODE * sn;
   char default_section_name[128];
   char alt_section_name[128];

   dbgmsg(("> AddSectionNode: mn:%p, #=%d, vaddr=%"_LZ64X", offset=%x, len=%x, flags=%x, locaddr=%p, name=%s\n",
           mn, number, addr, offset, length, flags, loc_addr, name));

   // Allocate node
   sn = AllocSectionNode();
   if (sn == NULL) {
      errmsg(("*E* AddSectionNode: unable to allocate SEC_NODE. mod=%s  sym=%s.\n", mn->name, name));
      return (NULL);
   }

   // Initialize node. Anything not explicitly initialized is 0/NULL.
   sn->number = number;
   sn->sec_flags = flags;
   sn->size = length;
   sn->start_addr = addr;
   sn->end_addr = addr + length - 1;
   sn->offset_start = offset;
   sn->offset_end = offset + length - 1;
   sn->asec = asec;
   sn->code = loc_addr;
   if (loc_addr)
      mn->codesize += length;

   if (name == NULL  ||  *name == '\0') {
      sprintf(default_section_name, "no_name_%d", number);
      name = default_section_name;
   }

   strcpy(alt_section_name, "<<");
   strcat(alt_section_name, name);
   strcat(alt_section_name, ">>");

   sn->name = zstrdup(name);
   if (sn->name == NULL) {
      FreeSectionNode(sn);
      errmsg(("*E* AddSectionNode: unable to allocate section name. secname=%s.\n", name));
      return (NULL);
   }
   mn->strsize += (int)strlen(name) + 1;

   sn->alt_name = zstrdup(alt_section_name);
   if (sn->alt_name == NULL) {
      FreeSectionNode(sn);
      errmsg(("*E* AddSectionNode: unable to allocate section alt_name. secname=%s.\n", name));
      return (NULL);
   }
   mn->strsize += (int)strlen(alt_section_name) + 1;

   //
   // Link in the new node to the beginning of the correct section node
   // list (executable/non-executable) for the module that owns the section.
   //
   if (executable) {                        // Executable section
      sn->next = NULL;
      if (mn->secroot == NULL)
         mn->secroot = mn->secroot_end = sn;       // First node (also the last)
      else {
         mn->secroot_end->next = sn;               // Add at the end
         mn->secroot_end = sn;                     // New end
      }
   }
   else {                                   // Non-executable section
      mn->ne_seccnt++;

      sn->next = NULL;
      if (mn->ne_secroot == NULL)
         mn->ne_secroot = mn->ne_secroot_end = sn; // First node (also the last)
      else {
         mn->ne_secroot_end->next = sn;            // Add at the end
         mn->ne_secroot_end = sn;                  // New end
      }
   }

   // Done
   mn->seccnt++;
   dbgmsg(("< AddSectionNode: (sn=%p.  Section count=%d)\n", sn, mn->seccnt));
   return(sn);
}


//
// GetSectionNode()
// ****************
//
// Get section node given a loaded module and an address.
// Returns pointer to sec node or NULL if error or address does not
// fall in any section.
//
SEC_NODE * GetSectionNode(LMOD_NODE * lmn, uint64 addr)
{
   SEC_NODE * sn;
   uint req_offset;
   int i;


   dbgmsg(("> GetSectionNode: lmn:%p, addr=%"_LZ64X"\n", lmn, addr));

   if (lmn == NULL) {                       // Module node pointer must not be NULL
      errmsg(("*E* GetSectionNode: **BUG** NULL lmn pointer!\n"));
      return (NULL);
   }

   if (lmn->mn->seccnt == 0) {              // There are no sections
      dbgmsg(("< GetSectionNode: (section count = 0)\n"));
      return (NULL);
   }

   // Now try and find the section where the given address falls.
   req_offset = (uint)(addr - lmn->start_addr);
   for (i = 0; i < 2; i++) {
      if (i == 0)
         sn = lmn->mn->secroot;             // Check executable sections
      else
         sn = lmn->mn->ne_secroot;          // Check non-executable sections

      while (sn) {
         if (req_offset >= sn->offset_start  &&  req_offset <= sn->offset_end) {
            dbgmsg(("< GetSectionNode: (secn=%p - %"_LZ64X" in section '%s' in module '%s')\n", sn, addr, sn->name, lmn->mn->name));
            return (sn);                    // Address in the range for this section
         }
         sn = sn->next;                     // Keep trying ...
      }
   }

   dbgmsg(("< GetSectionNode: (sn=NULL - can't find %"_LZ64X" in any section)\n", addr));
   return (NULL);
}


//
// GetSectionNodeByOffset()
// ************************
//
// Get section node given an offset within the module.
//
SEC_NODE * GetSectionNodeByOffset(MOD_NODE * mn, uint offset)
{
   SEC_NODE * cn;
   int i;


   if (mn->seccnt == 0) {                   // There are no sections
      dbgmsg(("* GetSectionNodeByOffset: (section count = 0)\n"));
      return (NULL);
   }

   // Now try and find the section where the given offset falls.
   for (i = 0; i < 2; i++) {
      if (i == 0)
         cn = mn->secroot;                  // Check executable sections
      else
         cn = mn->ne_secroot;               // Check non-executable sections

      while (cn) {
         if (offset >= cn->offset_start  &&  offset <= cn->offset_end) {
            return (cn);                    // Offset in the range for this section
         }
         cn = cn->next;                     // Keep trying ...
      }
   }

   dbgmsg(("* GetSectionNodeByOffset: (cn=NULL - can't find %x in any section)\n", offset));
   return (NULL);
}


//
// GetSectionNodeByNumber()
// ************************
//
// Get section node given a module node and a section number.
// Returns pointer to sec node or NULL if error or given section
// number does not exist.
//
SEC_NODE * GetSectionNodeByNumber(MOD_NODE * mn, int number)
{
   SEC_NODE * sn;
   int i;

   dbgmsg(("> GetSectionNodeByNumber: mn:%p, number=0x%x\n", mn, number));

   if (mn == NULL) {                        // mn pointer must not be NULL
      errmsg(("*E* GetSectionNodeByNumber: **BUG** NULL mn pointer!\n"));
      return (NULL);
   }

   if (mn->seccnt == 0) {                   // There are no sections
      dbgmsg(("< GetSectionNodeByNumber: (section count = 0)\n"));
      return (NULL);
   }

   // Now try and find the requested section.
   for (i = 0; i < 2; i++) {
      if (i == 0)
         sn = mn->secroot;                  // Check executable sections
      else
         sn = mn->ne_secroot;               // Check non-executable sections

      while (sn) {
         if (number == sn->number) {
            dbgmsg(("< GetSectionNodeByNumber: (sn=%p for sec_num=0x%x)\n", sn, sn->number));
            return (sn);                    // Found requested section
         }
         sn = sn->next;                     // Keep trying ...
      }
   }

   dbgmsg(("< GetSectionNodeByNumber: (sn=NULL - can't find section 0x%x)\n", number));
   return (NULL);
}


//
// GetSectionNodeByName()
// **********************
//
// Get section node given a module node and a section name.
// Returns pointer to sec node or NULL if error or given section
// number does not exist.
//
SEC_NODE * GetSectionNodeByName(MOD_NODE * mn, char * name)
{
   SEC_NODE * sn;
   int i;

   dbgmsg(("> GetSectionNodeByName: mn:%p, name=%s\n", mn, name));

   if (mn == NULL) {                        // mn pointer must not be NULL
      errmsg(("*E* GetSectionNodeByName: **BUG** NULL mn pointer!\n"));
      return (NULL);
   }

   if (mn->seccnt == 0) {                   // There are no sections
      dbgmsg(("< GetSectionNodeByName: (section count = 0)\n"));
      return (NULL);
   }

   // Now try and find the requested section.
   for (i = 0; i < 2; i++) {
      if (i == 0)
         sn = mn->secroot;                  // Check executable sections
      else
         sn = mn->ne_secroot;               // Check non-executable sections

      while (sn) {
         if (strcmp(name, sn->name) == 0) {
            dbgmsg(("< GetSectionNodeByName: (sn=%p for sec '%s')\n", sn,name));
            return (sn);                    // Found requested section
         }
         sn = sn->next;                     // Keep trying ...
      }
   }

   dbgmsg(("< GetSectionNodeByName: (sn=NULL - can't find section '%s')\n", name));
   return (NULL);
}


//
// CopyCode()
// **********
//
// Makes local copy of user supplied code.
//
// Returns:
//    non-NULL: buffer where code has been copied to
//        NULL: code was not copied - malloc error.
//
char * CopyCode(char * from, uint length)
{
   char * to;

   to = (char *)zmalloc(length);
   if (to == NULL) {
      dbgmsg((">< CopyCode: unable to malloc %d bytes\n", length));
   }
   else {
      memcpy(to, from, length);
      dbgmsg((">< CopyCode: copying %d bytes from: %p to: %p\n", length, from, to));
   }

   return (to);
}


//
// AllocProcessNode()
// ******************
//
PID_NODE * AllocProcessNode(void)
{
   PID_NODE * pn = (PID_NODE *)zcalloc(1, ((sizeof(PID_NODE) + 7) & 0xFFFFFFF8));
   if (pn != NULL) {
      pn->lastsym = AllocLastSymbolNode();
      if (pn->lastsym != NULL) {
         pn->flags = A2N_FLAGS_PID_NODE_SIGNATURE;
         pn->pid = (uint)-1;
      }
      else {
         FreeProcessNode(pn);
         pn = NULL;
      }
   }
   return (pn);
}

//
// FreeProcessNode()
// *****************
//
void FreeProcessNode(PID_NODE * pn)
{
   if (pn != NULL) {
      FreeLastSymbolNode(pn->lastsym);
      if (pn->name != G(UnknownName))
         zfree(pn->name);

      zfree(pn);
   }
   return;
}


//
// AllocLoadedModuleNode()
// ***********************
//
LMOD_NODE * AllocLoadedModuleNode(void)
{
   LMOD_NODE * lmn = (LMOD_NODE *)zcalloc(1, ((sizeof(LMOD_NODE) + 7) & 0xFFFFFFF8));
   if (lmn != NULL) {
      lmn->flags = A2N_FLAGS_LMOD_NODE_SIGNATURE;
      lmn->type = LMOD_TYPE_INVALID;
   }
   return (lmn);
}

//
// FreeLoadedModuleNode()
// **********************
//
void FreeLoadedModuleNode(LMOD_NODE * lmn)
{
   if (lmn != NULL) {
      if (lmn->type == LMOD_TYPE_JITTED_CODE)
         FreeJittedMethodNode(lmn->jmn);

      zfree(lmn);
   }
   return;
}


//
// AllocModuleNode()
// ****************
//
MOD_NODE * AllocModuleNode(void)
{
   MOD_NODE * mn = (MOD_NODE *)zcalloc(1, ((sizeof(MOD_NODE) + 7) & 0xFFFFFFF8));
   if (mn != NULL) {
      mn->flags = A2N_FLAGS_MOD_NODE_SIGNATURE | A2N_FLAGS_NO_LINENO;
      mn->type = MODULE_TYPE_UNKNOWN;
   }
   return (mn);
}

//
// FreeModuleNode()
// ***************
//
void FreeModuleNode(MOD_NODE * mn)
{
   if (mn != NULL) {
      zfree(mn->name);
      if (mn->name != mn->hvname)
         zfree(mn->hvname);

      zfree(mn);
   }
   return;
}


//
// AllocJittedMethodNode()
// ***********************
//
JM_NODE * AllocJittedMethodNode(void)
{
   JM_NODE * jmn = (JM_NODE *)zcalloc(1, ((sizeof(JM_NODE) + 7) & 0xFFFFFFF8));
   if (jmn != NULL) {
      jmn->flags = A2N_FLAGS_JM_NODE_SIGNATURE;
   }
   return (jmn);
}

//
// FreeJittedMethodNode()
// **********************
//
void FreeJittedMethodNode(JM_NODE * jmn)
{
   if (jmn != NULL) {
      zfree(jmn->name);
      if (jmn->code != NULL)
         zfree(jmn->code);
      if (jmn->jln != NULL)
         zfree(jmn->jln);

      zfree(jmn);
   }
   return;
}


//
// AllocSectionNode()
// ******************
//
SEC_NODE * AllocSectionNode(void)
{
   SEC_NODE *sn = (SEC_NODE *)zcalloc(1, ((sizeof(SEC_NODE) + 7) & 0xFFFFFFF8));
   if (sn != NULL) {
      sn->flags = A2N_FLAGS_SEC_NODE_SIGNATURE;
   }
   return (sn);
}

//
// FreeSectionNode()
// *****************
//
void FreeSectionNode(SEC_NODE * sn)
{
   if (sn != NULL) {
      zfree(sn->name);
      zfree(sn->code);
      zfree(sn);
   }
   return;
}


//
// AllocSymbolNode()
// *****************
//
SYM_NODE * AllocSymbolNode(void)
{
   SYM_NODE * sn = (SYM_NODE *)zcalloc(1, ((sizeof(SYM_NODE) + 7) & 0xFFFFFFF8));
   if (sn != NULL) {
      sn->flags = A2N_FLAGS_SYM_NODE_SIGNATURE;
   }
   return (sn);
}

//
// FreeSymbolNode()
// ****************
//
void FreeSymbolNode(SYM_NODE * sn)
{
   if (sn != NULL) {
      if (sn->type == SYMBOL_TYPE_USER_SUPPLIED)
         zfree(sn->code);
      if (sn->rn != NULL)
         zfree(sn->rn);

      zfree(sn->name);
      zfree(sn);
   }
   return;
}


//
// AllocLastSymbolNode()
// *********************
//
LASTSYM_NODE * AllocLastSymbolNode(void)
{
   return ((LASTSYM_NODE *)zcalloc(1, ((sizeof(LASTSYM_NODE) + 7) & 0xFFFFFFF8)));
}

//
// FreeLastSymbolNode()
// ********************
//
void FreeLastSymbolNode(LASTSYM_NODE * lsn)
{
   if (lsn != NULL)
      zfree(lsn);

   return;
}


//
// AllocRangeNode()
// ****************
//
RANGE_NODE * AllocRangeNode(void)
{
   return ((RANGE_NODE *)zcalloc(1, ((sizeof(RANGE_NODE) + 7) & 0xFFFFFFF8)));
}

//
// FreeLastSymbolNode()
// ********************
//
void FreeRangeNode(RANGE_NODE * rn)
{
   if (rn != NULL) {
      rn->ref_cnt--;
      if (rn->ref_cnt == 0) {
         zfree(rn->name);
         zfree(rn);
      }
   }

   return;
}


//
// AllocRangeDescNode()
// ********************
//
RDESC_NODE * AllocRangeDescNode(void)
{
   return ((RDESC_NODE *)zcalloc(1, ((sizeof(RDESC_NODE) + 7) & 0xFFFFFFF8)));
}


//
// FreeRangeDescNodeList()
// ***********************
//
void FreeRangeDescNodeList(void)
{
   RDESC_NODE * rn = G(range_desc_root);
   RDESC_NODE * tn;

   while (rn) {
      tn = rn;
      rn = rn->next;
      zfree(tn);
   }

   G(range_desc_root) = NULL;
   return;
}


//
// AllocSourceLineNode()
// *********************
//
SRCLINE_NODE * AllocSourceLineNode(void)
{
   return ((SRCLINE_NODE *)zcalloc(1, ((sizeof(SRCLINE_NODE) + 7) & 0xFFFFFFF8)));
}


//
// FreeSourceLineNode()
// ********************
//
void FreeSourceLineNode(SRCLINE_NODE * sln)
{
   if (sln != NULL)
      zfree(sln);

   return;
}


//
// AllocSourceFileNode()
// *********************
//
SRCFILE_NODE * AllocSourceFileNode(void)
{
   return ((SRCFILE_NODE *)zcalloc(1, ((sizeof(SRCFILE_NODE) + 7) & 0xFFFFFFF8)));
}


//
// FreeSourceFileNode()
// ********************
//
void FreeSourceFileNode(SRCFILE_NODE * sfn)
{
   if (sfn != NULL) {
      zfree(sfn->fn);
      zfree(sfn);
   }

   return;
}
