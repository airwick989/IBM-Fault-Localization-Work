//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//                      Windows JVMMAP Symbol Harvester                     //
//                      -------------------------------                     //
//                                                                          //
//**************************************************************************//
//
#include "a2nhdr.h"
#include "ft_map_md.h"


//
// Externs
//
extern gv_t a2n_gv;                         // in a2n,c
extern IMAGE_DOS_HEADER *dh;                // Image Dos Header (aka. mapped address)


//
// What an entry in a jvmmap file looks like
//
typedef struct _jvmmap_entry_node  JVMMAP_ENTRY_NODE;
struct _jvmmap_entry_node {
   JVMMAP_ENTRY_NODE *next;                 // Next one of these. Last is NULL
   char *name;                              // Name of a map in jvmmap
   uint img_base;                           // Image load address
   uint timestamp;                          // Timestamp of original map file
   uint num_syms;                           // number of symbols
   uint hdr_offset;                         // offset of file header for this map
};


//
// A list of these is built as we find jvmmaps, in case there are
// more than one. List is anchored off G(jvmmap_data).
//
typedef struct _jvmmap_file_node  JVMMAP_FILE_NODE;
struct _jvmmap_file_node {
   JVMMAP_FILE_NODE *next;                  // Next one of these. Last is NULL
   char *filename;                          // Name of the jvmmap (without path)
   char *path;                              // Path to the jvmmap (without name)
   char *uc_filename;                       // Fully qualified name of uncompressed jvmmap
   JVMMAP_ENTRY_NODE *entries;              // List of entries for this jvmmap
   int entry_cnt;                           // Number of entries in this jvmmap
   int remove_map;                          // 1=We uncompressed so remove
};


//
// To be appended to uncompressed jvmmaps to make names unique so we
// don't have to keep uncompressing file everytime we need symbols
// from it.
//
char *tfsuf[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};





//
// AllocJvmmapEntryNode()
// **********************
//
JVMMAP_ENTRY_NODE *AllocJvmmapEntryNode(void)
{
   return ((JVMMAP_ENTRY_NODE *)calloc(sizeof(JVMMAP_ENTRY_NODE),1));
}


//
// AllocJvmmapFileNode()
// **********************
//
JVMMAP_FILE_NODE *AllocJvmmapFileNode(void)
{
   return ((JVMMAP_FILE_NODE *)calloc(sizeof(JVMMAP_FILE_NODE),1));
}


//
// FreeJvmmapFileNode()
// ********************
//
// Always frees the last (first in the list) jvmmap file node.
//
void FreeJvmmapFileNode(void)
{
   JVMMAP_FILE_NODE *jfn = (JVMMAP_FILE_NODE *)G(jvmmap_data);
   JVMMAP_ENTRY_NODE *jen, *ten;

   dbghv(("> FreeJvmmapFileNode\n"));

   // First free all the ENTRY nodes
   jen = jfn->entries;
   while (jen) {
      ten = jen;
      jen = jen->next;
      free(ten->name);
      free(ten);
   }

   // Then free the FILE node
   G(jvmmap_data) = (void *)jfn->next;
   free(jfn->path);
   if (jfn->remove_map)
      remove(jfn->uc_filename);
   free(jfn->uc_filename);
   free(jfn);

   dbghv(("< FreeJvmmapEntryNodeList\n"));
   return;
}


//
// FindMatchingJvmmapFileNode()
// ****************************
//
// Input is something like:
//    E:\java14\jre\bin\classic\jvm.dll
//    E:\java14\jre\bin\java.exe
//    C:\winnt\system32\ntdll.dll
//
// What I want to do is strip off the filename and check if the
// path is contained in any of the paths for the jvmmaps we've found.
// If so, check the filename to see if the jvmmap contains an entry
// for it and whether we can use it or not.
//
JVMMAP_FILE_NODE *FindMatchingJvmmapFileNode(char *modname)
{
   JVMMAP_FILE_NODE *jfn;
   char *s, *p;


   dbghv(("> FindMatchingJvmmapFileNode: modname=%s\n",modname));
   s = strrchr(modname, '\\');              // Find last "\" (the one before the name)
   *s = '\0';                               // Zap "\" with a null. modname is now a path

   //
   // Check if path for modname is contained in the pathname for
   // any of the jvmmaps we've looked at.
   //
   jfn = (JVMMAP_FILE_NODE *)G(jvmmap_data);
   while (jfn) {
      dbghv(("- FindMatchingJvmmapFileNode: is '%s' in '%s'?\n",jfn->path,modname));
      p = istrstr(modname, jfn->path);      // Is jfn->path contained in path?
      if (p) {
         dbghv(("- FindMatchingJvmmapFileNode: Match.\n"));
         break;
      }
      jfn = jfn->next;
   }

   dbghv(("< FindMatchingJvmmapFileNode: jfn=%p (NULL=no match)\n",jfn));
   if (jfn) {
      dbghv(("                      jvmmap: %s\\%s\n",jfn->path,jfn->filename));
      dbghv(("                   uc_jvmmap: %s\n",jfn->uc_filename));
   }
   *s = '\\';                               // Zap "\" back into modname
   return (jfn);
}


//
// FindMatchingJvmmapEntryNode()
// *****************************
//
JVMMAP_ENTRY_NODE *FindMatchingJvmmapEntryNode(JVMMAP_FILE_NODE *jfn, char *modname)
{
   JVMMAP_ENTRY_NODE *jen;
   char *name;


   dbghv(("> FindMatchingJvmmapEntryNode: jfn=%p, modname=%s\n",jfn,modname));

   //
   // Check if there is an entry in the jvmmap for this module
   //
   name = GetFilenameFromPath(modname);
   jen = jfn->entries;
   while (jen) {
      if (COMPARE_NAME(name,jen->name) == 0)
         break;
      else
         jen = jen->next;
   }

   dbghv(("< FindMatchingJvmmapEntryNode: jen=%p (NULL=no match)\n",jen));
   return (jen);
}


//
// AddJvmmapFileNode()
// *******************
//
JVMMAP_FILE_NODE *AddJvmmapFileNode(char *map_name, char *uc_name, int remove_map)
{
   JVMMAP_FILE_NODE *jfn;
   char *s;


   jfn = AllocJvmmapFileNode();
   if (jfn == NULL) {
      errmsg(("*E* AddJvmmapFileNode: **OUT OF MEMORY** Can't allocate JVMMAP_FILE_NODE. Quitting.\n"));
      exit (-1);
   }

   if (G(jvmmap_data) == NULL)
      G(jvmmap_data) = (void *)jfn;
   else {
      jfn->next = (JVMMAP_FILE_NODE *)G(jvmmap_data);
      G(jvmmap_data) = (void *)jfn;
   }

   jfn->filename = G(jvmmap_fn);
   s = strrchr(map_name, '\\');             // Find last "\" (the one before the name)
   *s = '\0';                               // Zap it to make a path
   jfn->path = strdup(map_name);
   *s = '\\';                               // Zap it back to "\"
   jfn->entries = NULL;
   jfn->entry_cnt = 0;
   jfn->uc_filename = strdup(uc_name);
   jfn->remove_map = remove_map;

   return(jfn);
}


//
// AddJvmmapEntryNode()
// ********************
//
void AddJvmmapEntryNode(JVMMAP_FILE_NODE *jfn, char *map_name, int map_offset, ftMapFileHeader *map_hdr)
{
   JVMMAP_ENTRY_NODE *jen;


   jen = AllocJvmmapEntryNode();
   if (jen == NULL) {
      errmsg(("*E* AddJvmmapEntryNode: **OUT OF MEMORY** Can't allocate JVMMAP_NODE. Quitting.\n"));
      exit (-1);
   }

   if (jfn->entries == NULL)
      jfn->entries = (void *)jen;
   else {
      jen->next = jfn->entries;
      jfn->entries = (void *)jen;
   }

   jfn->entry_cnt++;

   jen->name = map_name;
   jen->img_base = map_hdr->loadAddress;
   jen->timestamp = map_hdr->timestamp;
   jen->num_syms = map_hdr->numSyms;
   jen->hdr_offset = map_offset;

   dbghv(("%16s  %08X  %6d  %08X  %7d\n",
          jen->name, jen->img_base, jen->num_syms, jen->timestamp, jen->hdr_offset));

   return;
}


//
// GetJvmmapSummary()
// ******************
//
// Find a jvmmap file (uncompressed or compressed) and extract summary
// information from it. If the file is compressed attempt to uncompress it.
//
//
// We need to try real hard to find the jvmmap on the first try because we
// don't want to be in a postion where part(s) of the jvm load, we don't find
// the jvmmap, we start taking ticks in those parts and can't resolve symbols.
// So, we're going to look for the jvmmap in the directory where the module
// is and in one directory up, unless the module's directory ends in "\bin"
// in which case we'll just quit.
// This all assumes a well defined directory structure for the IBM Sov JVMs,
// and that the jvmmap is always in .....\jre\bin.
//
void GetJvmmapSummary(char *modname)
{
   FILE *fh = NULL;
   char sname[MAX_PATH_LEN];                   // Next symbol file name
   char buf[(2 * MAX_PATH_LEN)];
   char map_name[1024];
   ftMapFileHeader map_hdr;
   char *hdr = (char *)&map_hdr;
   char *s1 = NULL, *s2 = NULL, *s3 = NULL;
   char *cd = NULL;                            // Current directory
   char *uc_name;
   int map_offset = 0;
   int remove_map = 0;
   JVMMAP_FILE_NODE *jfn;


   dbghv(("> GetJvmmapSummary: modname=%s starting ...\n",modname));

   strcpy(sname, modname);                  // module name

   s1 = strrchr(sname, '\\');               // Find last "\" (the one before the name)
   if (s1 == NULL) {
      dbghv(("< GetJvmmapSummary: '%s' does not seem to be fully qualified. Done.\n",sname));
      return;
   }
   s1[0] = '\0';
   s2 = strrchr(sname, '\\');               // Find "\" one back from the last one
   if (s2 != NULL) {
      s2[0] = '\0';
      s3 = strrchr(sname, '\\');            // Find "\" one back from the last one
      s2[0] = '\\';
   }
   s1[0] = '\\';
   s1[1] = '\0';                            // Zap 1st char of name with a null
   strcat(sname, G(jvmmap_fn));             // jvmmap filename

   if (!FileIsReadable(sname)) {
      dbghv(("- GetJvmmapSummary: '%s' does not exist.\n",sname));
      if (s2 != NULL) {
         s1[0] = '\0';
         if (stricmp(s2,"\\bin") == 0) {
            dbghv(("- GetJvmmapSummary: path ends in \\bin\n"));
            // Path ends in \bin and didn't find jvm map. Done.
            dbghv(("< GetJvmmapSummary: Done looking.\n"));
            return;
         }

         // Path didn't end in \bin. Look one directory up if we can
         if (s3 != NULL) {
            s2[1] = '\0';
            strcat(sname, G(jvmmap_fn));    // jvmmap filename
            if (!FileIsReadable(sname)) {
               dbghv(("< GetJvmmapSummary: '%s' does not exist. Done.\n",sname));
               return;
            }
         }
         // Nowhere else to look.
      }
      else {
         dbghv(("< GetJvmmapSummary: Done looking.\n"));
         return;
      }
   }

   dbghv(("- GetJvmmapSummary: '%s' exists. Checking if already have.\n",sname));
   jfn = FindMatchingJvmmapFileNode(modname);
   if (jfn) {
      dbghv(("< GetJvmmapSummary: '%s' already processed. Done.\n",sname));
      return;                            // Already have this jvmmap
   }

   dbghv(("- GetJvmmapSummary: trying '%s' ...\n",sname));
   if ((fh = fopen(sname, "rb")) == NULL) {
      dbghv(("< GetJvmmapSummary: Can't open '%s' for reading. Quitting.\n",sname));
      return;
   }

   if (fread(buf, 1, 8, fh) != 8) {
      fclose(fh);
      dbghv(("< GetJvmmapSummary: '%s' is not a valid jvmmap. Quitting.\n",sname));
      return;
   }
   fseek(fh, 0, SEEK_SET);                  // Go back to the beginning

   if (buf[0] == 'P' && buf[1] == 'K' && buf[2] == 0x03 && buf[3] == 0x04) {
      // compressed file. uncompress it.
      cd = GetCurrentWorkingDirectory();
      strcat(cd, "\\");
      strcat(cd, DEFAULT_UC_JVMMAP_NAME);

      dbghv(("- GetJvmmapSummary: %s is compressed. Attempting to unzip ...\n",sname));
      fclose(fh);
      strcpy(buf,"unzip -o ");
      strcat(buf, "\"");                 // In case of blanks in path
      strcat(buf, sname);
      strcat(buf, "\" > NUL 2> NUL");
      msg_log("%s\n",buf);
      system(buf);
      if ((fh = fopen(cd, "rb")) == NULL) {
         errmsg(("*W* GetJvmmapSummary: unzip \"%s\" failed. Trying jar ...\n",sname));
         strcpy(buf,"jar xvf ");
         strcat(buf, "\"");
         strcat(buf, sname);
         strcat(buf, "\" > NUL 2> NUL");
         msg_log("%s\n",buf);
         system(buf);
         if ((fh = fopen(cd, "rb")) == NULL) {
            errmsg(("*W* GetJvmmapSummary: jar \"%s\" failed ...\n",sname));
            errmsg(("*E* jar and unzip failed. Can't uncompress jvmmap file. File %s not found. Quitting.\n",cd));
            free(cd);
            return;
         }
      }

      //
      // Since we always uncompress to the current directory
      // there will only ever be one of these, regardless of how
      // many jvmmaps we process
      //
      remove_map = 1;                       // Erase jvmmap on exit
      strcat(cd, tfsuf[G(jvmmap_cnt)]);     // Make name unique
      remove(cd);                           // Remove existing one
      uc_name = cd;
   }
   else {
      uc_name = sname;
   }

   // Add new jvmmap file node
   jfn = AddJvmmapFileNode(sname, uc_name, remove_map);

   dbghv(("- GetJvmmapSummary: enumerating maps ...\n"));
   dbghv(("Image Name        ImgBase   SymCnt  TimeDate  FOffset\n"
          "----------------  --------  ------  --------  -------\n"));

   while (fread(hdr, 1, sizeof(ftMapFileHeader), fh) == sizeof(ftMapFileHeader)) {
      if (strcmp(map_hdr.magic, "JVMMAPS") != 0) {
         dbghv(("- GetJvmmapSummary: Invalid map entry. Quitting this jvmmap.\n"));
         errmsg(("*E* GetJvmmapSummary: Invalid map entry in %s. Quitting this jvmmap.\n",sname));
         fclose(fh);
         FreeJvmmapFileNode();
         free(cd);
         return;
      }

      fseek(fh, (map_offset + map_hdr.nameOffset), SEEK_SET);
      fread(map_name, 1, 1024, fh);

      // Remember this map
      AddJvmmapEntryNode(jfn, strdup(map_name), map_offset, &map_hdr);

      // On to the next one
      map_offset += map_hdr.next;
      fseek(fh, map_offset, SEEK_SET);
   }

   //
   // Done
   //
   if (fh != NULL)
      fclose(fh);

   if (remove_map) {
      // We uncompressed this one. Need to rename the uncompressed
      // map, in case we have others.
//    s = GetFilenameFromPath(cd);          // Unique name
//    dbghv(("- GetJvmmapSummary: renaming jvmmap.X to %s\n",s));
//    rename(DEFAULT_UC_JVMMAP_NAME, s);
      s1 = GetFilenameFromPath(cd);         // Unique name
      dbghv(("- GetJvmmapSummary: renaming jvmmap.X to %s\n",s1));
      rename(DEFAULT_UC_JVMMAP_NAME, s1);
   }

   G(jvmmap_cnt)++;
   free(cd);

   dbghv(("< GetJvmmapSummary: done ...\n"));
   return;
}


//
// TimestampsAreOk()
// *****************
//
// Checks the image timestamp (MTE data) against the symbol file timestamp.
// Symbol file can be a jvmmap, jmap, map, dbg or pdb file.
//
// If the given timestamps match then that's easy and we're done.
// If the image and symbols timestamps don't match, and the image is
// a DLL, then check the Export Directory timestamp in the image. If
// it matches the symbols timestamp then we accept it as a match.
// The reason I'm doing this is that if DLLs are rebased (with EDITBIN or
// REBASE) the original timestamp is replaced, but the Export Directory
// timestamp is left intact. Pain in the ass but what are you going to do.
//
// Returns TRUE (1) if timestamps match, FALSE (0) if they don't.
//
int TimestampsAreOk(uint img_ts, uint sym_ts)
{
   IMAGE_NT_HEADERS *nth;
   IMAGE_FILE_HEADER *ifh;                  // Image file header
   IMAGE_EXPORT_DIRECTORY *ied;


   dbghv(("> TimestampsAreOk: img_ts=0x%08X  sym_ts=0x%08X\n",img_ts,sym_ts));

   if (img_ts == sym_ts) {
      dbghv(("< TimestampsAreOk: timestamps match.\n"));
      return (1);
   }
   dbghv(("- TimestampsAreOk: timestamps don't match. Checking export directory\n"));

   //
   // Image and jvmmap/jmap timestamps don't match.
   // If the image is a DLL then check the timestamp in the
   // Export Directory to see if it matches the jvmmap timestamp.
   // We'll consider that a match.
   //
   nth = (IMAGE_NT_HEADERS *)PtrAdd(dh, dh->e_lfanew);
   if (IsBadReadPtr((void *)dh, sizeof(IMAGE_DOS_HEADER))) {
      errmsg(("*E* TimestampsAreOk: IMAGE_NT_HEADERS is not readable. No match.\n"));
      return (0);
   }

   ifh = &(nth->FileHeader);                // Image file header
   if ((ifh->Characteristics & IMAGE_FILE_DLL) == 0) {
      // Not a DLL so don't bother
      dbghv(("< TimestampsAreOk: Image is not a DLL. No match.\n"));
      return (0);
   }

   ied = FindExportDirectory(nth, NULL, NULL, NULL);
   if (ied == NULL) {
      // Can't check so no match
      dbghv(("< TimestampsAreOk: IMAGE_EXPORT_DIRECTORY not found or not readable. No match.\n"));
      return (0);
   }

   dbghv(("- TimestampsAreOk: IMAGE_EXPORT_DIRECTORY timestamp = 0x%08X\n",ied->TimeDateStamp));
   if (ied->TimeDateStamp == sym_ts) {
      dbghv(("< TimestampsAreOk: EXPORT_DIRECTORY timestamp matches jvmmap/jmap timestamp.\n"));
      return (1);
   }

   dbghv(("< TimestampsAreOk: Neither the image nor Export Directory timestamp matches jvmmap timestamp.\n"));
   return (0);
}


//
// GetSymbolsFromJmapFile()
// ************************
//
// Read a map file (produced by printmap.exe or my xjmap.exe) and extract
// symbols from it.  The maps are the result of extracting the symbol
// information stored in the jvmmap compressed maps file.
// jvmmap is an attemp by the java folks to complicate life for anybody
// needing symbols. They just couldn't save the real maps, they had to
// invent some half-ass map format which throws away a lot of the good
// information from the real maps.
// Returns number of symbols found or 0 if no symbols or errors.
//
// JMap file format on Win32 and Win64:
// ------------------------------------
//          11111111112222222222333333333344444444445
// 12345678901234567890123456789012345678901234567890
//  jvm
//
//  Timestamp is 3e7e62a4
//
//  Preferred load address is 10000000
//
//  Rva      Name
//
// 00001000  _getUserAssertDefault
// 00001010  _getSystemAssertDefault
// 00001020  _getAssertClasses
//  ...
// 00155098  _psiMultiProcessorMode
// ------------------- End Of Map -------------------- (not a line in the map!)
//
// Assumptions:
// - Line 1 contains one word and that is the name (without extension) of the
//   image file for which this map was generated.
// - Line 3 contains the timestamp when the image was generated.
// - Line 5 contains the preferred load address of the image.
// - Line 9 and above contain the symbols
//
int GetSymbolsFromJmapFile(char *mapname, uint ts, CONTEXT_REC *cr)
{
   FILE *fh = NULL;
   char *c;
   char line[1024];
   int rc;

   char fn[64];
   char modname[MAX_FILENAME_LEN];          // Name of module whose map this is
   uint timestamp;                          // module timestamp

   char sym_name[1024];                     // symbol name
   int  sym_sec;                            // section to which symbol belongs
   uint sym_offset;                         // symbol offset
   char *sym_code = NULL;                   // Symbol code (or NULL if no code)
   int symcnt = 0;
   int sym_file_mismatch = 0;

   MOD_NODE *mn;


   dbghv(("> GetSymbolsFromJmapFile: mapfn='%s', cr=%p\n",mapname,cr));
   mn = (MOD_NODE *)cr->handle;

   fh = fopen(mapname, "r");
   if (fh == NULL ) {
      errmsg(("*E* GetSymbolsFromJmapFile: unable to open file '%s'\n",mapname));
      goto TheEnd;
   }

   // Line 1: image name
   dbghv(("- GetSymbolsFromJmapFile: reading line 1: map name\n"));
   c = fgets(line, 512, fh);
   if (c == NULL || line[0] == 0x0D) {
      errmsg(("*E* GetSymbolsFromJmapFile: Invalid jmap file - no image name.\n"));
      goto TheEnd;
   }
   modname[0] = 0;
   sscanf(line,"%s",modname);
   if (modname[0] == 0) {
      errmsg(("*E* GetSymbolsFromJmapFile: Invalid jmap file - no image name.\n"));
      goto TheEnd;
   }
   c = GetFilenameFromPath(mapname);
   strcpy(fn, c);                           // Only the map file name (no path)
   c = GetExtensionFromFilename(fn);
   *c = 0;
   if (COMPARE_NAME(modname,fn) != 0) {
      errmsg(("*E* GetSymbolsFromJmapFile: Image name not same as image name in jmap: %s vs %s\n",fn,modname));
      goto TheEnd;
   }

   // Line 2: blank
   dbghv(("- GetSymbolsFromJmapFile: reading line 2: blank\n"));
   c = fgets(line, 512, fh);

   // Line 3: timestamp
   dbghv(("- GetSymbolsFromJmapFile: reading line 3: timestamp\n"));
   c = fgets(line, 512, fh);
   if (c == NULL || line[0] == 0x0D) {
      errmsg(("*E* GetSymbolsFromJmapFile: Invalid jmap file - no timestamp\n"));
      goto TheEnd;
   }
   timestamp = 0;
   sscanf(line," Timestamp is %lx",&timestamp);
   if (timestamp == 0) {
      errmsg(("*E* GetSymbolsFromJmapFile: Invalid jmap file - no timestamp\n"));
      goto TheEnd;
   }
   dbghv(("- GetSymbolsFromJmapFile: map timestamp %x\n",timestamp));
   if (ts == 0) {
      warnmsg(("*W* GetSymbolsFromJmapFile: '%s' TS is 0. Unable to check if symbols match the image.\n",cr->imgname));
      sym_file_mismatch = 1;                // symbol file doesn't match image
   }
   else {
      if (!TimestampsAreOk(ts, timestamp)) {
         warnmsg(("*W* GetSymbolsFromJmapFile: '%s'/'%s' timestamp mismatch: %08x vs %08x\n",
                 mapname, cr->imgname, timestamp, ts));
         if (G(validate_symbols)) {
            goto TheEnd;
         }
         else {
            warnmsg(("    - Symbols validation failed. ***** SYMBOLS MAY NOT BE VALID *****\n"));
            sym_file_mismatch = 1;          // symbol file doesn't match image
         }
      }
   }

   dbghv(("- GetSymbolsFromJmapFile: reading lines 4 - 8: ignored\n"));
   c = fgets(line, 512, fh);                // Line 4: blank
   c = fgets(line, 512, fh);                // Line 5: load address
   c = fgets(line, 512, fh);                // Line 6: blank
   c = fgets(line, 512, fh);                // Line 7: rva  name
   c = fgets(line, 512, fh);                // Line 8: blank
   if (c == NULL || line[0] == 0x0D) {
      errmsg(("*E* GetSymbolsFromJmapFile: Invalid jmap file - not enough lines.\n"));
      goto TheEnd;
   }

   // Line 9 and above:  rva    symbol
   dbghv(("- GetSymbolsFromJmapFile: reading lines 9 - end: offset/symbol\n"));
   rc = fscanf(fh,"%lx %s\n",&sym_offset,sym_name);
   while (rc != EOF) {
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
      sym_sec = FindSectionForOffset(sym_offset);
      if (G(gather_code)) {
         sym_code = GetCodePtrForSymbol(sym_offset, sym_sec);
         DumpCode((uchar *)sym_code, 4); // Don't know size so dump first 4 bytes
      }

      c = sym_name;
#if defined(_X86)
      if (*c == LEAD_CHAR_US || *c == LEAD_CHAR_AT) c++;
#else
      if (*c == LEAD_CHAR_DOT) c++;
#endif
      // Demangle (Undecorate in MS speak) name
      if (G(demangle_cpp_names))
         c = DemangleName(c);

      dbghv(("- ***** name=%s off=0x%X sec=%d code=%p\n",
             c, sym_offset, sym_sec, sym_code));

      // Send the symbol back to A2N
      rc = SendBackSymbol(c,                     // Symbol name
                          sym_offset,            // Symbol offset
                          0,                     // Symbol size
                          SYMBOL_TYPE_FUNCTION,  // Symbol type (label, export, etc.)
                          sym_sec,               // Section to which the symbol belongs
                          sym_code,              // Code associated with this symbol
                          (void *)cr);           // Context record
      if (rc) {
         dbghv(("< GetSymbolsFromJmapFile: SendBackSymbol returned %d. Stopping.\n",rc));
         return (symcnt);                // Destination wants us to stop harvesting
      }

      symcnt++;                          // Another symbol added successfully

      // Read the next line
      rc = fscanf(fh,"%lx %s\n",&sym_offset,sym_name);
   }

TheEnd:
   if (fh != NULL)
      fclose(fh);

   if (symcnt != 0) {
      mn = (MOD_NODE *)cr->handle;
      mn->type = MODULE_TYPE_MAP_JVMX;              // Set new module type
      if (sym_file_mismatch)
         mn->flags |= A2N_FLAGS_VALIDATION_FAILED;  // Symbols may not match image
   }

   dbghv(("< GetSymbolsFromJmapFile: found %d symbols\n",symcnt));
   return (symcnt);
}


//
// GetSymbolsFromJvmmapFile()
// **************************
//
// Read an uncompressed jvmmap file and extract symbols for the given
// module from it.
// Returns number of symbols found or 0 if no symbols or errors.
//
int GetSymbolsFromJvmmapFile(CONTEXT_REC *cr)
{
   FILE *fh = NULL;

   char *modname;
   ftMapFileHeader map_hdr;
   char *hdr = (char *)&map_hdr;
   ftMapFileSymbol *symbols = NULL;
   char *strings = NULL;

   int i, rc;
   MOD_NODE *mn = NULL;
   JVMMAP_FILE_NODE *jfn = NULL;
   JVMMAP_ENTRY_NODE *jen;
   char fq_jvmmap_name[MAX_PATH_LEN];

   char *sym_name;                          // symbol name
   int  sym_sec;                            // section to which symbol belongs
   uint sym_offset;                         // symbol offset
   char *sym_code = NULL;                   // Symbol code (or NULL if no code)
   int symcnt = 0;
   int sym_file_mismatch = 0;


   dbghv(("> GetSymbolsFromJvmmapFile: cr = %p\n",cr));

   if (G(jvmmap_data) == NULL) {
      dbghv(("- GetSymbolsFromJvmmapFile: there are no jvmmap files.\n"));
      goto TheEnd;
   }

   // Check if module's path matches any of the jvmmaps we've seen
   jfn = FindMatchingJvmmapFileNode(cr->imgname);
   if (jfn == NULL) {
      dbghv(("- GetSymbolsFromJvmmapFile: %s not part of any jvmmap file's path.\n",cr->imgname));
      goto TheEnd;
   }

   // Check if jvmmap contains this module
   jen = FindMatchingJvmmapEntryNode(jfn, cr->imgname);
   if (jen == NULL) {
      dbghv(("- GetSymbolsFromJvmmapFile: no jvmmap contains symbols for '%s'\n",cr->imgname));
      goto TheEnd;
   }

   // Now make sure timestamps and checksums match
   mn = (MOD_NODE *)cr->handle;
   modname = GetFilenameFromPath(cr->imgname);

   if (mn->ts == 0) {
      warnmsg(("*W* GetSymbolsFromJvmmapFile: '%s' TS is 0. Unable to check if symbols match the image.\n",cr->imgname));
      sym_file_mismatch = 1;                // symbol file doesn't match image
   }
   else {
      if (!TimestampsAreOk(mn->ts, jen->timestamp)) {
         warnmsg(("*E* GetSymbolsFromJvmmapFile: jvmmap(%s)/'%s' timestamp mismatch: %08x vs %08x\n",
                 modname, cr->imgname, jen->timestamp, mn->ts));
         if (G(validate_symbols)) {
            goto TheEnd;
         }
         else {
            warnmsg(("    - Symbols validation failed. ***** SYMBOLS MAY NOT BE VALID *****\n"));
            sym_file_mismatch = 1;          // symbol file doesn't match image
         }
      }
   }

   // Looks like we want to keep the symbols. Open the uncompressed jvmmap file ...
   dbghv(("- GetSymbolsFromJvmmapFile: looks like we're going for it ...\n"));
   if ((fh = fopen(jfn->uc_filename, "rb")) == NULL) {
      errmsg(("*E* GetSymbolsFromJvmmapFile: Unable to open '%s' for reading.\n",jfn->uc_filename));
      goto TheEnd;
   }

   // Get the symbols
   dbghv(("- GetSymbolsFromJvmmapFile: reading map header ...\n"));
   fseek(fh, jen->hdr_offset, SEEK_SET);
   if (fread(hdr, 1, sizeof(ftMapFileHeader), fh) != sizeof(ftMapFileHeader)) {
      errmsg(("*E* GetSymbolsFromJvmmapFile: can't read jvmmap header for '%s'\n",modname));
      goto TheEnd;
   }
   strings = (char *)malloc(map_hdr.stringsLen);
   if (strings == NULL) {
      errmsg(("*E* GetSymbolsFromJvmmapFile: **OUT OF MEMORY** Can't allocate %d bytes for strings\n", map_hdr.stringsLen));
      exit (-1);
   }

   dbghv(("- GetSymbolsFromJvmmapFile: reading string table ...\n"));
   fseek(fh, (jen->hdr_offset + map_hdr.strOffset), SEEK_SET);
   if (fread(strings, 1, map_hdr.stringsLen, fh) != map_hdr.stringsLen) {
      errmsg(("*E* GetSymbolsFromJvmmapFile: Can't read strings from '%s'\n", jfn->uc_filename));
      goto TheEnd;
   }
   fseek(fh, (jen->hdr_offset + map_hdr.symOffset), SEEK_SET);
   symbols = (ftMapFileSymbol *)malloc(map_hdr.symSize);
   if (symbols == NULL) {
      errmsg(("*E* GetSymbolsFromJvmmapFile: **OUT OF MEMORY** Can't allocate %d bytes for symbols\n", map_hdr.symSize));
      exit (-1);
   }

   dbghv(("- GetSymbolsFromJvmmapFile: reading symbol descriptors ...\n"));
   if (fread(symbols, 1, map_hdr.symSize, fh) != map_hdr.symSize) {
      errmsg(("*E* GetSymbolsFromJvmmapFile: Can't read symbols from '%s'\n", jfn->uc_filename));
      goto TheEnd;
   }

   dbghv(("- GetSymbolsFromJvmmapFile: enumerating symbols ...\n"));
   for (i = 0; i < (int)map_hdr.numSyms; i++) {
      sym_offset = symbols[i].address;                            // sym_offset
      sym_name = (char *)PtrAdd(strings, symbols[i].nameOffset);  // sym_name

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
      sym_sec = FindSectionForOffset(sym_offset);
      if (G(gather_code)) {
         sym_code = GetCodePtrForSymbol(sym_offset, sym_sec);
         DumpCode((uchar *)sym_code, 4); // Don't know size so dump first 4 bytes
      }

#if defined(_X86)
      if (*sym_name == LEAD_CHAR_US || *sym_name == LEAD_CHAR_AT) sym_name++;
#else
      if (*sym_name == LEAD_CHAR_DOT) sym_name++;
#endif

      // Demangle (Undecorate in MS speak) name
      if (G(demangle_cpp_names))
         sym_name = DemangleName(sym_name);

      dbghv(("***** name=%s off=0x%X sec=%d code=%p\n",
             sym_name, sym_offset, sym_sec, sym_code));

      // Send the symbol back to A2N
      rc = SendBackSymbol(sym_name,              // Symbol name
                          sym_offset,            // Symbol offset
                          0,                     // Symbol size
                          SYMBOL_TYPE_FUNCTION,  // Symbol type (this is a guess)
                          sym_sec,               // Section to which the symbol belongs
                          sym_code,              // Code associated with this symbol
                          (void *)cr);           // Context record
      if (rc) {
         dbghv(("- GetSymbolsFromJvmmapFile: SendBackSymbol returned %d. Stopping.\n",rc));
         goto TheEnd;                    // Destination wants us to stop harvesting
      }

      symcnt++;                          // Another symbol added successfully
   }

TheEnd:
   if (fh != NULL)
      fclose(fh);

   if (strings)
     free(strings);

   if (symbols)
     free(symbols);

   if (symcnt != 0) {
      strcpy(fq_jvmmap_name, jfn->path);
      strcat(fq_jvmmap_name, "\\");
      strcat(fq_jvmmap_name, jfn->filename);

      mn->type = MODULE_TYPE_MAP_JVMC;                // Set new module type
      mn->hvname = strdup(fq_jvmmap_name);            // Symbols came from here
      mn->flags |= A2N_FLAGS_SYMBOLS_NOT_FROM_IMAGE;  // and it's not the on-disk image
      if (sym_file_mismatch)
         mn->flags |= A2N_FLAGS_VALIDATION_FAILED;    // Symbols may not match image
   }

   dbghv(("< GetSymbolsFromJvmmapFile: found %d symbols\n",symcnt));
   return (symcnt);
}


//
// RemoveJvmmaps()
// ***************
//
void RemoveJvmmaps(void)
{
   JVMMAP_FILE_NODE *jfn = (JVMMAP_FILE_NODE *)G(jvmmap_data);

   dbghv(("\n> RemoveJvmmaps: start\n"));

   while (jfn) {
      if (jfn->remove_map) {
         dbghv(("- RemoveJvmmaps: removing %s\n",jfn->uc_filename));
         remove(jfn->uc_filename);
      }
      jfn = jfn->next;
   }

   dbghv(("< RemoveJvmmaps: done\n"));
   return;
}


//
// DumpJvmmaps()
// *************
//
void DumpJvmmaps(FILE *fh)
{
   JVMMAP_FILE_NODE *jfn = (JVMMAP_FILE_NODE *)G(jvmmap_data);
   JVMMAP_ENTRY_NODE *jen;

   if (jfn == NULL)
      return;

   FilePrint(fh,"**************** jvmmap file(s) summary ****************\n");

   while (jfn) {
      FilePrint(fh,"*****    name: %s%s%s\n",jfn->path,G(PathSeparator_str),jfn->filename);
      FilePrint(fh,"***** uc_name: %s\n",jfn->uc_filename);
      FilePrint(fh,"************** Contents **************\n");
      FilePrint(fh,"Image Name        ImgBase   SymCnt  TimeDate  FOffset  Date\n"
                   "----------------  --------  ------  --------  -------  ------------------------\n");
      jen = jfn->entries;
      while (jen) {
         FilePrint(fh,"%16s  %08X  %6d  %08X  %7d  %s",
                   jen->name, jen->img_base, jen->num_syms, jen->timestamp,
                   jen->hdr_offset, TsToDate(jen->timestamp));
         jen = jen->next;
      }
      FilePrint(fh,"**************************************\n");
      jfn = jfn->next;
   }

   FilePrint(fh,"********************************************************\n");
   return;
}
