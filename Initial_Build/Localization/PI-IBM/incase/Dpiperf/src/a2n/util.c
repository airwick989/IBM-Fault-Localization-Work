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

#if defined(_LINUX)
// We use functions from libc in our memory wrappers
void * __libc_malloc(size_t size);
void __libc_free(void * buf);
void * __libc_calloc(size_t num_elements, size_t size);
void * __libc_realloc(void * buf, size_t size);
#endif


extern gv_t a2n_gv;                         // Globals


//
// Memory allocation/deallocation wrappers
// ***************************************
//
// Using these wrappers allows us to chose which memory allocation
// scheme we want to use. For example, on Linux, we like to use libc's
// malloc()/free(), not some other wrapper.
//
// * If you allocate with zmalloc/zcalloc/zrealloc/zstrdup() then
//   deallocate with zfree().
// * Don't mix and match with malloc()/free().
// * Use free() [*NOT* zfree()] when system services, or non-A2N APIs,
//   allocate memory which must be deallocated by the caller.
//

//
// zmalloc()
// *********
//
__INLINE void * zmalloc(size_t size)
{
#if defined(_LINUX)
   return (__libc_malloc(size));
#else
   return (malloc(size));
#endif
}


//
// zcalloc()
// *********
//
#if defined(_WINDOWS)
void * zcalloc(size_t num_elements, size_t size)
#else
__INLINE void * zcalloc(size_t num_elements, size_t size)
#endif
{
#if defined(_LINUX)
   return (__libc_calloc(num_elements, size));
#else
   return (calloc(num_elements, size));
#endif
}


//
// zrealloc()
// **********
//
#if defined(_WINDOWS)
void * zrealloc(void * buf, size_t size)
#else
__INLINE void * zrealloc(void * buf, size_t size)
#endif
{
#if defined(_LINUX)
   return (__libc_realloc(buf, size));
#else
   return (realloc(buf, size));
#endif
}


//
// zstrdup()
// *********
//
__INLINE char * zstrdup(const char * str)
{
#if defined(_LINUX)
   char * p = zmalloc(strlen(str)+1);
	if (p) {
	   strcpy(p, str);
	}
	return (p);
#else
   return (strdup(str));
#endif
}


//
// zfree()
// *******
//
__INLINE void zfree(void * buf)
{
#if defined(_LINUX)
   __libc_free(buf);
#else
   free(buf);
#endif
   return;
}


//
// MapFile()
// *********
//
// Maps a file into memory and returns the pointer to where the file
// is mapped.  If "filesize" is not NULL then the size of the file
// is saved in filesize.
//
// Returns non-NULL if file mapped successfully.
//         NULL on errors.
//
void * MapFile(char * filename, size_t * filesize)
{
   void * map_addr;

#if defined(_LINUX) || defined(_ZOS)

   int rc;
   int fd;
   struct stat s;
   size_t file_size;
   size_t bytes_read;


   dbgmsg(("> MapFile: fn='%s', filesize_ptr=%p\n", filename, filesize));

   // Open the file (read only)
   fd = open(filename, O_RDONLY);
   if (fd < 0) {
      errmsg(("*E* MapFile: open() failed for '%s'. errno=%d\n", filename, errno));
      return (NULL);
   }

   // Get some info about this file
   rc = fstat(fd, &s);
   if (rc < 0) {
      errmsg(("*E* MapFile: fstat() failed for '%s'. errno=%d\n", filename, errno));
      close(fd);
      return (NULL);
   }
   file_size = s.st_size;

   // Allocate buffer to read the file
   map_addr = (void *)zmalloc(file_size);
   if (map_addr == NULL) {
      errmsg(("*E* MapFile: malloc(%d bytes) for '%s' failed.\n", file_size, filename));
      close(fd);
      return (NULL);
   }

   // Read the file
   bytes_read = read(fd, map_addr, file_size);
   if (bytes_read != file_size) {
      errmsg(("*E* MapFile: failed to read entire file '%s'. wanted=%d, got=%d\n", filename, file_size, bytes_read));
      close(fd);
      UnmapFile(map_addr);
      return (NULL);
   }

   // Done
   close(fd);
   if (filesize != NULL)
      *filesize = (unsigned long)file_size;
   dbgmsg(("< MapFile: map_addr=%p  filesize=0x%X (%d)\n", map_addr, file_size, file_size));
   return (map_addr);

#elif defined(_WINDOWS)

   HANDLE fh;
   HANDLE mh;
   uint file_size;


   dbgmsg(("> MapFile: fn='%s', filesize_ptr=%p\n", filename, filesize));

   // Open file
   fh = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
   if (fh == INVALID_HANDLE_VALUE) {
      errmsg(("*E* MapFile: CreateFile() failed for '%s'. rc=%d\n", filename, GetLastError()));
      return (NULL);
   }

   // Create a file mapping
   mh = CreateFileMapping(fh, NULL, PAGE_READONLY, 0, 0, NULL);
   if (mh == NULL) {
      errmsg(("*E* MapFile: CreateFileMapping) failed for '%s'. rc=%d\n", filename, GetLastError()));
      CloseHandle(fh);
      return (NULL);
   }
   file_size = GetFileSize(fh, NULL);       // Only the low order 32 bits

   // Map the file read-only
   map_addr = MapViewOfFile(mh, FILE_MAP_READ, 0, 0, 0);

   // Done
   CloseHandle(fh);
   CloseHandle(mh);
   if (filesize != NULL)
      *filesize = (size_t)file_size;
   dbgmsg(("< MapFile: map_addr=%p  filesize=0x%X (%d)\n", map_addr, file_size, file_size));
   return (map_addr);

#elif defined(_AIX)

   int rc;
   struct stat s;
   size_t file_size;
   size_t bytes_read;
   FILE  * Datafile;

   dbgmsg(("> MapFile: fn='%s', filesize_ptr=%p\n", filename, filesize));

   // Open the file (read only)
   if ((Datafile = fopen( filename, "rb")) == 0) {
      errmsg(("*E* MapFile: open() failed for '%s'. errno=%d\n", filename, 999));
      return (NULL);
   }

   // Get some info about this file
   rc = fstat( fileno(Datafile), &s);
   if (rc < 0) {
      errmsg(("*E* MapFile: fstat() failed for '%s'. errno=%d\n", filename, errno));
      fclose(Datafile);
      return (NULL);
   }
   file_size = s.st_size;

   // Allocate buffer to read the file
   map_addr = (void *)zmalloc(file_size);
   if (map_addr == NULL) {
      errmsg(("*E* MapFile: malloc(%d bytes) for '%s' failed.\n", file_size, filename));
      fclose(Datafile);
      return (NULL);
   }

   // Read the file
   bytes_read = fread(map_addr, 1, file_size, Datafile);
   if (bytes_read != file_size) {
      errmsg(("*E* MapFile: failed to read entire file '%s'. wanted=%d, got=%d\n", filename, file_size, bytes_read));
      fclose(Datafile);
      UnmapFile(map_addr);
      return (NULL);
   }

   // Done
   fclose(Datafile);
   if (filesize != NULL)
      *filesize = (unsigned long)file_size;
   dbgmsg(("< MapFile: map_addr=%p  filesize=0x%X (%d)\n", map_addr, file_size, file_size));
   return (map_addr);

#else
   return (NULL);
#endif
}


//
// UnmapFile()
// ***********
//
// Unmaps a file from memory.
//
void UnmapFile(void * mapped_addr)
{
   dbgmsg(("> UnmapFile: mapped_addr=%p\n", mapped_addr));

   // Don't unmap if address is NULL
   if (mapped_addr == NULL) {
      dbgmsg(("< UnmapFile: Nothing done - map_addr is NULL\n"));
      return;                               // NULL address. Nothing to do
   }

   // OK, go ahead an unmap
#if defined(_LINUX) || defined(_ZOS) || defined(_AIX)
   zfree(mapped_addr);
#elif defined(_WINDOWS)
   UnmapViewOfFile(mapped_addr);
#endif

   // Done
   dbgmsg(("< UnmapFile: OK\n"));
   return;
}


//
// FileIsReadable()
// ****************
//
// Returns TRUE if file exists and can be read.
// Returns FALSE otherwise.
//
bool FileIsReadable(char * filename)
{
#if defined(_WINDOWS)
   if (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES)
      return (TRUE);                        // File exists. Should have read permission
   else
      return (FALSE);                       // File does not exist so we can't read it

#elif defined(_LINUX) || defined(_ZOS) || defined(_AIX)
   if (access(filename, F_OK | R_OK) == 0)
      return (TRUE);                        // File exists and have read permission
   else
      return (FALSE);                       // File does not exist or no read permission

#else
   return (FALSE);
#endif
}


//
// FileExists()
// ************
//
// Returns TRUE if file exists and can be read.
// Returns FALSE otherwise.
//
bool FileExists(char * filename)
{
#if defined(_WINDOWS)
   if (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES)
      return (TRUE);                        // File exists
   else
      return (FALSE);                       // File does not exist

#elif defined(_LINUX) || defined(_ZOS) || defined(_AIX)
   if (access(filename, F_OK) == 0)
      return (TRUE);                        // File exists
   else
      return (FALSE);                       // File does not exist

#else
   return (FALSE);
#endif
}


//
// GetCurrentWorkingDirectory()
// ****************************
//
// Returns current working directory or NULL if errors.
//
// Caller *MUST* deallocate memory using zfree().
//
char * GetCurrentWorkingDirectory(void)
{
   char * c = zmalloc(MAX_PATH_LEN);

#if defined(_LINUX) || defined(_ZOS) || defined(_AIX)
   if (getcwd(c, MAX_PATH_LEN) == NULL) {
      zfree(c);
      c = NULL;
   }
#elif defined(_WINDOWS)
   GetCurrentDirectory(MAX_PATH_LEN, c);
#endif

   return (c);
}


//
// GetFilenameFromPath()
// *********************
//
// Returns pointer to beginning of filename in a fully qualified path.
// Returns NULL if path ends in / (linux) or \ (windows)
//
// Caller *MUST* make a copy of the returned name.  It is not
// guaranteed to persist.
//
char * GetFilenameFromPath(char * path)
{
#if defined(_LINUX) || defined(_ZOS) || defined(_AIX)
   char * p = zstrdup(path);
   char * n = basename(path);
   zfree(p);
#elif defined(_WINDOWS)
   char * n = strrchr(path, G(PathSeparator));
   if (n == NULL)
      n = path;                             // No "slash" in path.  Must be name already
   else {
      if (*++n == '\0')
         n = NULL;
   }
#else
   n = NULL;
#endif

   return (n);
}


//
// GetExtensionFromFilename()
// **************************
//
// Returns pointer to the "." (dot) in filename or NULL if no extension.
// Filename can be just a filename or a path - doesn't matter.
//
char * GetExtensionFromFilename(char * filename)
{
   char * c = strrchr(filename, '.');
   return (c);
}


//
// FindFileAlongSearchPath()
// *************************
//
// Find a file along the directories listed in the search path
// and return the fully qualified filename.
//
// Returns:
// - Success: Pointer to fully qualified filename string.
//            User *MUST* free storage for string.
// - Failure: NULL
//
char * FindFileAlongSearchPath(char * fn, char * searchpath)
{
   char path[1024], lfn[MAX_PATH_LEN];
   char * p;


   if (fn == NULL  ||  searchpath == NULL) {
      dbgmsg((">< FindFileAlongSearchPath: fn or searchpath NULL\n"));
      return (NULL);                        // Invalid fn or searchpath
   }

   p = searchpath;
   if (strlen(p) > 1024) {
      dbgmsg((">< FindFileAlongSearchPath: value of searchpath '%s' is too long\n", searchpath));
      return (NULL);                        // searchpath too long (for us)
   }

   strcpy(path, p);                         // Make local copy - strtok mucks with it

   // Search for file in each component of search path
   p = strtok(path, G(SearchPathSeparator_str));    // Prime strtok
   while(p) {
      strcpy(lfn, p);
      if (p[strlen(p) - 1] != G(PathSeparator))
         strcat(lfn, G(PathSeparator_str));
      strcat(lfn, fn);

      if (FileExists(lfn)) {
         p = zstrdup(lfn);
         return (p);                                // Found it
      }

      p = strtok(NULL, G(SearchPathSeparator_str)); // Grab the next PATH component
   }

   return (NULL);                           // Nothing found anywhere
}


#if defined(LINUX) || defined(_ZOS)
//
// strlwr()
// ********
//
// Lowercase a string.
//
char * strlwr(char * s)
{
   char * c = s;

   while (*c != '\0') {
      *c = (char)tolower(*c);
      c++;
   }

   return (s);
}
#endif


//
// istrstr()
// *********
//
// Case insensitive strstr().
//
char * istrstr(const char * haystack, const char * needle)
{
   char * cs, * ct, * p;

   cs = strlwr(zstrdup(haystack));
   ct = strlwr(zstrdup(needle));
   p  = strstr(cs, ct);
   if (p) {
      p = (char *)PtrAdd(haystack, PtrSub(p, ct));
   }
   zfree(cs);
   zfree(ct);

   return (p);
}


//
// hash_string()
// *************
//
// Generate hash value for a string.
//
uint hash_string(char * s)
{
   uint hv;

   if (s == NULL)
      return (0);

   for (hv = 0; *s != '\0'; s++) {
      hv = *s + 31 * hv;
   }
   return (hv);
}


//
// hash_module_name()
// ******************
//
// Generate hash value for a module name string.
// String treated as case insensitive on Windows and case sensitive
// on Linux.
//
uint hash_module_name(char * s)
{
   uint hv;

   if (s == NULL)
      return (0);

   for (hv = 0; *s != '\0'; s++) {
#if defined(_LINUX) || defined(_ZOS) || defined(_AIX)
      hv = *s + 31 * hv;                    // Use as-is on Linux
#else
      hv = tolower(*s) + 31 * hv;           // Force lowercase on Windows
#endif
   }
   return (hv);
}


//
// msg_se()
// ********
//
// Write messages to stderr and flush the stream.
//
void msg_se(char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stderr, format, argptr);
   fflush(stderr);
   va_end(argptr);
   return;
}


//
// msg_so()
// ********
//
// Write messages to stdout and flush the stream.
//
void msg_so(char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stdout, format, argptr);
   fflush(stdout);
   va_end(argptr);
   return;
}


//
// msg_log()
// *********
//
// Write messages to A2n error log file and flush the stream.
//
void NoExport msg_log(char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(G(msgfh), format, argptr);
   fflush(G(msgfh));
   va_end(argptr);
   return;
}


//
// FilePrint()
// ***********
//
// Write messages to given file and flush the stream.
//
void FilePrint(FILE * fh, char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(fh, format, argptr);
   fflush(fh);
   va_end(argptr);
   return;
}


//
// FileWrite()
// ***********
//
// Binary write to file.
// Returns #bytes_written on success, 0 on error.
//
size_t FileWrite(FILE * fh, char * src, size_t cnt)
{
   size_t items_written;

   items_written = fwrite((void *)src, cnt, 1, fh);
   return (items_written);
}
