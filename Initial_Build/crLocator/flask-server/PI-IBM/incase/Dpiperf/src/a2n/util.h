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

#ifndef _UTIL_H_
#define _UTIL_H_


// TODO: Clean this crap up
#define __INLINE


//
// Constants
//
#define MAX_FILENAME_LEN              256             // Max size of a filename
#define MAX_PATH_LEN                 1024             // Max size of a path


//
// Prototypes
//
__INLINE void * zmalloc(size_t size);
__INLINE char * zstrdup(const char * str);
__INLINE void zfree(void * buf);
#if defined(_WINDOWS)
void * zcalloc(size_t num_elements, size_t size);
void * zrealloc(void * buf, size_t size);
#else
__INLINE void * zcalloc(size_t num_elements, size_t size);
__INLINE void * zrealloc(void * buf, size_t size);
#endif

void * MapFile(char * filename, size_t * filesize);      // Map a file
void UnmapFile(void * mapped_addr);                      // Unmap a file

uint hash_string(char * s);                              // Calculate hash value for a string
uint hash_module_name(char * s);                         // Calculate hash value for a module name

char * GetCurrentWorkingDirectory(void);                 // Get current working directory
char * GetFilenameFromPath(char * path);                 // Get filename from pathname
char * GetExtensionFromFilename(char * filename);        // Get extension from filename
char * FindFileAlongSearchPath(char * fn, char * var);   // Find file along given environment var

char * istrstr(const char * haystack, const char * needle); // Case insensitive strstr()
bool FileIsReadable(char * filename);                  // Check if file exists and is readable
bool FileExists(char * filename);                      // Check if file exists
void FilePrint(FILE * fh, char * format, ...);           // Print to file (like fprintf)
size_t FileWrite(FILE * fh, char * src, size_t cnt);     // Write to file (like fwrite)
#define writef   FilePrint

void msg_se(char * format, ...);                         // Write msg to stderr
void msg_so(char * format, ...);                         // Write msg to stdout
void NoExport msg_log(char * format, ...);               // Write msg to A2N log file
#define msg    msg_so



//
// Macros to write error/warning/informational messages
//
#define errmsg(_x_)                                \
   do {                                            \
      if (MSG_ERROR <= G(display_error_messages))  \
         msg_log _x_ ;                             \
   } while (0)

#define warnmsg(_x_)                               \
   do {                                            \
      if (MSG_WARN <= G(display_error_messages))   \
         msg_log _x_ ;                             \
   } while (0)

#define infomsg(_x_)                               \
   do {                                            \
      if (MSG_INFO <= G(display_error_messages))   \
         msg_log _x_ ;                             \
   } while (0)


//
// Debug macros
// Only defined for DEBUG versions!
//
// dbgmsg:
//   Should be for all function entry/exits
// dbgapi:
//   Should be for exported API entry/exits
// dbghv:
//   Should only be used in the harvester
//
#if defined(DEBUG)
   #define dbgmsg(_x_)                            \
      do {                                        \
         if (G(debug) & DEBUG_MODE_INTERNAL)      \
            msg_log _x_ ;                         \
      } while (0)

   #define dbgapi(_x_)                                          \
      do {                                                      \
         if (G(debug) & DEBUG_MODE_API)                         \
            msg_log _x_ ;                                       \
      } while (0)

   #define dbghv(_x_)                             \
      do {                                        \
         if (G(debug) & DEBUG_MODE_HARVESTER)     \
            msg_log _x_ ;                         \
      } while (0)
#else
   #define dbgmsg(_x_)
   #define dbgapi(_x_)
   #define dbghv(_x_)
#endif



//
// Platform specific library functions
//
#if defined(_LINUX) || defined(_ZOS) || defined(_AIX)
   #define stricmp   strcasecmp
   #define strnicmp  strncasecmp
#endif
#if defined(_WINDOWS)
   #define sleep    Sleep
   #define getpid() GetCurrentProcessId()
#endif

#endif //_UTIL_H_
