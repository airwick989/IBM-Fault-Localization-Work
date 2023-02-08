/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2009
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef COMMON_H
   #define COMMON_H

   #include "encore.h"

char * getReturnAddress( char * dummy );
void   logCodeOffset(    char * dummy );
void   dumpStack(        int    depth );
void   DumpData(         char * p, uint32_t n );

void err_exit(    char * msg );
void writeStderr( char * msg );         // Allow hooking of stderr
void writeStdout( char * msg );         // Allow hooking of stdout

//======================================================================
// Global Common variables (persistent)
//======================================================================

   #define gc_mask_Malloc_Free         0x10000000
   #define gc_mask_Show_Write_Format   0x20000000

struct GC                               // Global Comon variables (Persistent)
{
   UINT64  memoryAllocations;           // Total Memory Allocations   (xMalloc)
   UINT64  memoryDeallocs;              // Total Memory Deallocations (xFree)
   UINT64  stringAllocations;           // Total String Allocations   (xStrDup)
   UINT64  stringDeallocs;              // Total String Deallocations (xFreeStr)
   UINT64  memoryRequested;             // Memory Requested by this application
   UINT64  memoryUsage;                 // Memory Usage     by this application
   UINT64  memoryFreed;                 // Memory Freed     by this application
   UINT64  memoryMaximum;               // Max Footprint    by this application
   UINT64  memoryUsageThreshold;        // Threshold for next memory usage report

   #define gc_verbose_stderr  1         // Optional messages to stderr
   #define gc_verbose_logmsg  2         // Optional messages to logmsg, if possible
   #define gc_verbose_maximum 3         // Optional messages to stderr and logmsg

   long    mask;                        // debug output mask
   int     verbose;                     // Level of verbosity, see ge_verbose_xxx

   int     delayedMsgBufSize;           // Size of delayed message buffer data
   int     delayedMsgBufLimit;          // Size of delayed message buffer
   char  * delayedMsgBuf;               // Delayed message buffer
   char  * allocMsg;                    // Temporary buffer for allocation identifier

   FILE  * msg;                         // Message file

   char  * myLoadAddress;               // Used to compute load addresses of routines

   int     posNeg;                      // Positive/Negative (+/-) helpTest response

   #define gc_mHelp_Help      1         // Normal Help only
   #define gc_mHelp_AllHelp   2         // All Help

   char    mHelp;                       // Help requested
   char    ShuttingDown;                // Shutdown in Progress
};
extern struct GC gc;

// Generic message functions/macros:
// *********************************
//
// msg_se()       Message to STDERR
// msg_so()       Message to STDOUT
// msg_log()      Message to log-msg
// msg_se_log()   Message to STDERR and log-msg
// msg_so_log()   Message to STDOUT and log-msg
// msg()          Message to log-msg if VERBMSG or to STDERR otherwise (= VMsg)
// verb_msg()     Message, *ONLY IF VERBOSE*, to log-msg file if VERBMSG or to STDERR otherwise (= OVMsg)
// OptVMsg()      Message, *ONLY IF VERBOSE*, to log-msg file if VERBMSG or to STDERR otherwise
// OptVMsgLog()   Message to log-msg file *AND* to STDERR if VERBOSE and not VERBMSG
// WriteFlush()   Write to any stream and flush it
//
// MVMsg()        msg() if mask matches
// VMsg           Same as msg()
// ErrVMsgLog     Same as msg_se_log()
// OVMsg          Same as verb_msg()

void   msg_se(     const char * format, ... );
void   msg_so(     const char * format, ... );
void   msg_log(    const char * format, ... );
void   msg_se_log( const char * format, ... );
void   msg_so_log( const char * format, ... );
void   msg(        const char * format, ... );
void   OptVMsg(    const char * format, ... );
void   OptVMsgLog( const char * format, ... );
void   WriteFlush( FILE * file, const char * format, ... );
void   DelayedVMsg(const char * format, ... );
void   writeDelayedMsgs();

   #define verb_msg(_m_)  \
   do {                   \
      if (gc.verbose)     \
         msg _m_;         \
   } while(0)

   #define MVMsg(x, y)    \
   do {                   \
      if (gc.mask & (x))  \
         msg y;           \
   } while(0)

   #define ErrVMsgLog   msg_se_log
   #define OVMsg        verb_msg
   #define MSG_BUF_SIZE 4096

//======================================================================
// Parameter testing with built-in help generation
//======================================================================

int helpTest(  char * parm, void * flag, char * keyword,                              char * description );
int helpTest2( char * parm, void * flag, char * keyword, char * alias,                char * description );
int helpTest3( char * parm, void * flag, char * keyword, char * alias, char * alias2, char * description );
int helpMsg(   char * parm,              char * keyword,                              char * description );

//======================================================================
// Hexadecimal Conversion Routine with upper truncation
//======================================================================

UINT64 strToUINT64( char *p );

//======================================================================
// Memory Allocation and Tracking
//======================================================================

void * jmalloc(size_t size);
void jfree(void * addr);
void * xMalloc(  char * msg, size_t size );
void * zMalloc(  char * msg, size_t size );
void * xRealloc( char * msg, size_t size, void * p, size_t sizeOld );
void   xFree(    void * p,   size_t size );
char * xStrdup(  char * msg, char * str );
void   xFreeStr( char * p );
void   logAllocStats();

//======================================================================
// Hash Tables
//======================================================================

typedef struct _bucket_t bucket_t;

struct _bucket_t
{
   bucket_t * nextInBucket;
   void     * key;
};

typedef struct
{
   char      * name;                    // Table name
   bucket_t ** buckv;                   // Bucket vector

   void      * ( * funcAlloc )();       // Element Alloc routine
   void        ( * funcFree  )( void * );   // Element Free  routine

   uint32_t    elems;                   // Number of entries in table
   uint32_t    bucks;                   // Number of buckets

   uint32_t    itemsize;                // Size of an item in a bucket
   uint32_t    reserved;                // Assure that it packs nicely

} hash_t;

typedef struct
{
   hash_t *   table;                    // Hash table  being traversed
   uint32_t   bucket;                   // Hash bucket being traversed
   bucket_t * next;                     // Next entry in bucket

} hash_iter_t;

void     hashInit(   hash_t * table,
                     int      bucks,
                     int      itemsize,
                     char   * name,
                     void   * ( * funcAlloc )(),
                     void     ( * funcFree  )( void * ) );
void   * hashLookup( hash_t * table, void * key );
void   * hashAdd(    hash_t * table, void * key );
void     hashChange( hash_t * table, void * keyOld, void * keyNew );
void     hashRemove( hash_t * table, void * key );
uint32_t hashCount(  hash_t * table );
char   * hashName(   hash_t * table );
void     hashStats(  hash_t * table );
void     hashFree(   hash_t * table );

void     hashIter(   hash_t * table, hash_iter_t * iter );
int      hashMore(                     hash_iter_t * iter );
void   * hashNext(                     hash_iter_t * iter );

#endif
