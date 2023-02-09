/*
 *   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2010
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

#include "common.h"

#if defined(_LINUX)
// We use functions from libc in our memory wrappers
void * __libc_malloc(size_t size);
void * __libc_free(void * inp);
#endif

struct GC gc = {0};

///============================================================================
/// Debugging routines
///
/// These routines are used only while debugging.  They assume the normal
/// Windows calling conventions to return or log the current return address to
/// help determine the caller of a routine in case of failure.  The callstack
/// can be walked and memory can be dumped.
///============================================================================

char * getReturnAddress( char * dummy )
{
   char ** p = (char **)&dummy;

   return( p[-1] );
}

void   logCodeOffset( char * dummy )
{
   char ** p = (char **)&dummy;

   OptVMsgLog( "CODE:%p\n", (char *)( p[-1] - gc.myLoadAddress ) );
}

void   dumpStack( int depth )           // Dump up to depth stack entries
{
#if defined(WALK_CALLSTACK)
   #if defined(_WINDOWS)
      #if !defined(_AMD64)

   char ** pcs = (char **)&depth;

   pcs--;                               // Backup to point to return address
   pcs--;                               // Backup to point to frame pointer

   OptVMsgLog( "\nWalk Callstack: addr = %p\n\n", gc.myLoadAddress );

   if ( pcs )
   {
      do
      {
         if ( 0 == pcs[1] )             // Invalid return address
         {
            break;
         }
         OptVMsgLog( "bp = %p, ", pcs[0] );
         OptVMsgLog( "ip = %p\n", pcs[1] );

         if ( pcs >= (char **)pcs[0] )  // Frame pointer no longer increasing
         {
            break;
         }
         pcs = (char **)pcs[0];
      }
      while ( --depth );
   }

      #endif
   #endif
#endif
}

// Generic dump of n bytes at p with Ascii

void copyAscii( char * dst, char * src, int len )
{
   int i;

   for ( i = 0; i < len; i++ )
   {
      if ( src[i] < 0x20 || src[i] >= 0x7F )
      {
         dst[i] = '.';
      }
      else
      {
         dst[i] = src[i];
      }
   }
}

void DumpData( char * p, uint32_t n )
{
   int  i = 0;
   int  j = 0;
   char buf[80];

   while ( n > 0 )
   {
      if ( n > 3 )
      {
         OptVMsgLog( "%08X ", *((uint32_t *)p) );

         copyAscii( buf+j, p, 4 );
         j += 4;

         p += 4;
         n -= 4;
      }
      else
      {
         copyAscii( buf+j, p, n );
         j += n;

         while ( n > 0 )
         {
            OptVMsgLog( "%02X ", *p );

            p += 1;
            n -= 1;
         }
      }

      if ( 8 == ++i || 0 == n )
      {
         buf[j] = 0;

         i = 0;
         j = 0;
         OptVMsgLog( "%s\n", buf );
      }
   }
   OptVMsgLog( "\n" );
}

//----------------------------------------------------------------------
// Message routines
//----------------------------------------------------------------------

// DelayedMsg()
// ************
//
// Adds the message in msgBuf to the delayedMsgBuf which will
// be written to log-msg after it is opened

void DelayedMsg( char * msgBuf )
{
   int      n = (int)strlen( msgBuf );
   char   * p;

   if ( n > 0 )
   {
      if ( ( n + 1 + gc.delayedMsgBufSize ) > gc.delayedMsgBufLimit )
      {
         gc.delayedMsgBufLimit += MSG_BUF_SIZE;   // Enough room for buf

         p = (char *)jmalloc(gc.delayedMsgBufLimit);   // Can not use xMalloc/xFree for DelayedMsg

         if ( 0 == p )
         {
            fprintf(stderr,"Allocation error in DelayedMsg\n");
            err_exit(0);
         }

         if ( gc.delayedMsgBuf )
         {
            strcpy( p, gc.delayedMsgBuf );

            jfree( gc.delayedMsgBuf );   // Can not use xMalloc/xFree for DelayedMsg
         }
         gc.delayedMsgBuf = p;
      }
      strcpy( gc.delayedMsgBuf + gc.delayedMsgBufSize, msgBuf );

      gc.delayedMsgBufSize += n;
   }
}

void writeDelayedMsgs()
{
   if ( gc.delayedMsgBuf )              // Write delayed messages to log-msg
   {
      WriteFlush( gc.msg, "%s", gc.delayedMsgBuf );

      jfree( gc.delayedMsgBuf );        // Can not use xMalloc/xFree for DelayedMsg

      gc.delayedMsgBuf      = 0;
      gc.delayedMsgBufSize  = 0;
      gc.delayedMsgBufLimit = 0;
   }
}

//
// msg_se()
// ********
//
// Message to STDERR
//
void msg_se(const char * format, ...)
{
   char     msgBuf[MSG_BUF_SIZE];
   va_list  vap;

   va_start(vap, format);
   vsprintf( msgBuf, format, vap );
   va_end(vap);

   writeStderr(msgBuf);
}


//
// msg_so()
// ********
//
// Message to STDOUT
//
void msg_so(const char * format, ...)
{
   char     msgBuf[MSG_BUF_SIZE];
   va_list  vap;

   va_start(vap, format);
   vsprintf( msgBuf, format, vap );
   va_end(vap);

   writeStdout(msgBuf);
}


//
// msg_log()
// *********
//
// Message to log-msg
//
void msg_log(const char * format, ...)
{
   char     msgBuf[MSG_BUF_SIZE];
   va_list  vap;

   va_start(vap, format);
   vsprintf( msgBuf, format, vap );
   va_end(vap);

   if ( gc.msg )
   {
      fprintf(gc.msg, "%s", msgBuf);
      fflush(gc.msg);
   }
   else
   {
      DelayedMsg( msgBuf );
   }
}


//
// msg_se_log()
// ************
//
// Message to STDERR and log-msg
//
void msg_se_log(const char * format, ...)
{
   char     msgBuf[MSG_BUF_SIZE];
   va_list  vap;

   va_start(vap, format);
   vsprintf( msgBuf, format, vap );
   va_end(vap);

   writeStderr(msgBuf);

   if ( gc.msg != stderr )
   {
      if ( gc.msg )
      {
         fprintf(gc.msg, "%s", msgBuf);
         fflush(gc.msg);
      }
      else
      {
         DelayedMsg( msgBuf );
      }
   }
}


//
// msg_so_log()
// ************
//
// Message to STDOUT and log-msg
//
void msg_so_log(const char * format, ...)
{
   char     msgBuf[MSG_BUF_SIZE];
   va_list  vap;

   va_start(vap, format);
   vsprintf( msgBuf, format, vap );
   va_end(vap);

   writeStdout(msgBuf);

   if ( gc.msg )
   {
      fprintf(gc.msg, "%s", msgBuf);
      fflush(gc.msg);
   }
   else
   {
      DelayedMsg( msgBuf );
   }
}


//
// msg()
// *****
//
// Message to log-msg if VERBMSG or to STDERR otherwise
//
void msg(const char * format, ...)
{
   va_list  vap;

   va_start(vap, format);

   if (gc.msg && (gc.verbose & gc_verbose_logmsg))
   {
      vfprintf(gc.msg, format, vap);
      fflush(gc.msg);
   }
   else
   {
      vfprintf(stderr, format, vap);
   }

   va_end(vap);
   return;
}


//
// OptVMsg()
// *********
//
// Message to log-msg file if VERBMSG and to STDERR if VERBOSE
//
void OptVMsg( const char *format, ... )
{
   va_list vap;

   if ( gc.verbose )
   {
      if ( gc_verbose_logmsg & gc.verbose )
      {
         va_start( vap, format );

         if ( gc.msg )
         {
            vfprintf( gc.msg, format, vap );
            fflush(gc.msg);
         }
         else
         {
            char msgBuf[MSG_BUF_SIZE];

            vsprintf( msgBuf, format, vap );
            DelayedMsg( msgBuf );
         }
         va_end( vap );
      }
      if ( gc_verbose_stderr & gc.verbose )
      {
         va_start( vap,    format );
         vfprintf( stderr, format, vap );
         va_end( vap );
      }
   }
}


//
// OptVMsgLog()
// ************
//
// Message to log-msg file *AND* to STDERR if VERBOSE and not VERBMSG
//
void OptVMsgLog( const char *format, ... )   // OptVMsg + write to log
{
   va_list vap;

   va_start( vap, format );

   if ( gc.msg )
   {
      vfprintf( gc.msg, format, vap );
      fflush(gc.msg);
   }
   else
   {
      char msgBuf[MSG_BUF_SIZE];

      vsprintf( msgBuf, format, vap );
      DelayedMsg( msgBuf );
   }

   va_end( vap );

   if ( ( gc_verbose_stderr & gc.verbose ) && gc.msg != stderr )
   {
      va_start( vap, format );
      vfprintf( stderr, format, vap );
      va_end( vap );
   }
}


//
// WriteFlush()
// ************
//
// Write to any stream and flush it
//
void WriteFlush( FILE *file, const char *format, ... )
{
   va_list vap;

   if ( file != NULL )
   {
      MVMsg(gc_mask_Show_Write_Format, ("%s", format));

      va_start( vap, format );
      vfprintf( file, format, vap );
      va_end( vap );

      fflush(file);
   }
}

// Hexadecimal conversion routine with upper truncation

UINT64 strToUINT64( char *p )
{
   UINT64 n = 0;
   char   c;

   if ( p )
   {
      while ( 0 != ( c = *p++ ) )
      {
         n <<= 4;

         if ( c >= '0' && c <= '9' )
         {
            n += c - '0';
         }
         else if ( c >= 'a' && c <= 'f' )
         {
            n += 10 + c - 'a';
         }
         else if ( c >= 'A' && c <= 'F' )
         {
            n += 10 + c - 'A';
         }
      }
   }
   return( n );
}

///============================================================================
/// Memory management wrappers
///
/// This set of wrappers records all memory allocations and deallocations.
/// The routines gather statistics and optionally log all operations with
/// annotations.  The logAllocStats routine writes these statistics to the log
/// file.  The compallocs Java program can be used to read the annotated logs
/// and search for memory leaks.
///
/// xMalloc():        [equivalent to malloc()]
/// ----------
///   Performs all of the allocations.  If there is an allocation error,
///   the msg parameter determines whether the failure is fatal or not.  If it
///   is specified, msg is immediately reported via err_exit(), greatly
///   simplifying the code for the caller.  Otherwise, zero is returned to the
///   calling routine.  Usually, msg is specified as zero only if the allocation
///   is performed under a lock, so that the error can be returned to the caller
///   for proper cleanup before using err_exit().
///
///   If there is no allocation error, msg is used as the annotation for the log
///   file, unless gc.allocMsg is specified as a one-time annotation override.
///   Typically, msg reports the routine that is requesting a memory allocation,
///   while gc.allocMsg reports what is being allocated.  For example, xStrdup
///   sets gc.allocMsg to the string being duplicated, then passes its own msg
///   parameter to xMalloc.  Thus, the log will report the actual string being
///   allocated, unless there is an error.
///
/// zMalloc():        [equivalent to calloc(size_of_block,1)]
/// ----------
///   Allocates, using xMalloc(), zero-filled memory.
///
/// xFree():          [equivalent to free()]
/// --------
///   Deallocates memory allocated with xMalloc()/zMalloc(). Attempts to use the
///   size parameter to keep an accurate account of total memory usage.
///
/// xRealloc():       [equivalent to realloc()]
/// -----------
///   Change the size of a memory block allocated with xMalloc()/zMalloc()
///   and copies data from the former to the new memory block.
///   Use xFree() to deallocate.
///
/// xStrdup():        [equivalent to strdup()]
/// ----------
///   Duplicate a string. Use xFreeStr() to deallocate.
///
/// xFreeStr():       [equivalent to free()]
/// -----------
///   Deallocate strings allocated with xStrdup().
///============================================================================


//
// JProf malloc() and free() wrappers
// **********************************
//
// Using these wrappers allows us to chose which memory allocation
// scheme we want to use. For example, on Linux, we like to use libc's
// malloc()/free(), not some other wrapper.
//
// * If you allocate with jmalloc() then deallocate with jfree().
// * Don't mix and match with malloc()/free().
// * Use free() [*NOT* jfree()] when system services allocate
//   memory which must be deallocated by the caller.
//   - For example, on Linux, the backtrace_symbols() API returns a
//     memory block that must be deallocated by the caller. In that
//     case use free(), *NOT* jfree().
//

//
// jmalloc()
// *********
//
void * jmalloc(size_t size)
{
#if defined(_LINUX)
	return (__libc_malloc(size));
#else
	return (malloc(size));
#endif
}


//
// jfree()
// *******
//
void jfree(void * addr)
{
#if defined(_LINUX)
	__libc_free(addr);
#else
	free(addr);
#endif
	return;
}


//
// xMalloc()
// *********
// malloc() with record keeping
//
void * xMalloc(char * msg, size_t size)
{
   void * p;

   // FIX ME:  Handle cases of annotations in EBCDIC

   gc.memoryAllocations++;
   p = jmalloc(size);

   if (p) {
      if (gc.memoryUsage > gc.memoryMaximum) {
         gc.memoryMaximum = gc.memoryUsage;
         if (gc.memoryMaximum >= gc.memoryUsageThreshold) {
            gc.memoryUsageThreshold = ((gc.memoryMaximum >> 23) + 1) << 23;
            OptVMsgLog("Maximum bytes allocated >= %d MB\n", gc.memoryMaximum >> 20);
         }
      }

      if (gc.mask & gc_mask_Malloc_Free) {
         OptVMsgLog("Alloc: %6d:%p %s\n",
                    size, p,
                    gc.allocMsg ? gc.allocMsg : (msg ? msg : ""));

         gc.allocMsg = 0;
      }
      gc.memoryRequested += size;       // Add the amount of memory requested
      gc.memoryUsage += ( ( size + 0x0000000F ) & 0xFFFFFFF8 );   // Add the actual amount of memory consumed
   }
   else {
      if (msg) {
         ErrVMsgLog( "%s\n", msg );
         err_exit( "Allocation Error" );
      }
   }
   return (p);
}


//
// zMalloc()
// *********
// malloc() and set to zero with record keeping
//
void * zMalloc(char * msg, size_t size)
{
   void * p;

   p = xMalloc(msg, size);
   if (p) {
      memset(p, 0, size);
   }
   return (p);
}


//
// xRealloc()
// **********
// Reallocate p from oldSize to size and copy data
//
void * xRealloc(char * msg, size_t size, void * p, size_t sizeOld)
{
   char * q;

   if (size == sizeOld && p) {         // Do nothing if size is not changing
      q = p;
   }
   else {
      q = xMalloc(msg, size);
      if (p) {                         // Copy nothing if this is the initial alloc
         if (size > sizeOld) {
            size = sizeOld;
         }
         memcpy(q, p, size);
         xFree(p, sizeOld);
      }
   }
   return (q);
}


//
// xFree()
// *******
// free() memory alloced by xMalloc or zMalloc with record keeping
//
void xFree(void * p, size_t size)
{
   if (p) {
      MVMsg(gc_mask_Malloc_Free, ("Free:  %6d:%p\n", size, p));
      if (size) {
         if (gc.mask & gc_mask_Malloc_Free) {
            memset(p, 0x33, size);      // Mark this as dead with 333...
         }
         gc.memoryRequested -= size;    // Reduce memory usage, if specified
         size                = ((size + 0x0000000F) & 0xFFFFFFF8);   // Actual amount of memory freed
         gc.memoryUsage     -= size;    // Reduce memory usage, including overhead even if not specified
         gc.memoryFreed     += size;
      }
      jfree(p);
      gc.memoryDeallocs++;
   }
   return;
}


//
// xStrdup()
// *********
// strdup() with record keeping
//
char * xStrdup(char * msg, char * str)
{
   char * p;

   // FIX ME:  Handle EBCDIC cases

   gc.stringAllocations++;
   gc.allocMsg = str;
   p = xMalloc(msg, strlen(str)+1);

   if (p) {
      strcpy(p, str);
   }
   return(p);
}


//
// xFreeStr()
// **********
// free() string memory alloced by xMalloc or xStrdup with record keeping
//
void xFreeStr(char * p)
{
   if (p) {
      MVMsg(gc_mask_Malloc_Free, ("String:%s\n", p));
      xFree(p, strlen(p)+1);
      gc.stringDeallocs++;
   }
   return;
}


void logAllocStats()
{
   OptVMsgLog( "\n# of xMalloc:   %"_P64"d\n# of xFree:     %"_P64"d\n# of xStrDup:   %"_P64"d\n# of xFreeStr:  %"_P64"d\nMem Requested:  %"_P64"d\nMemory In Use:  %"_P64"d\nMemory Freed:   %"_P64"d\nMax Footprint:  %"_P64"d\n",
               gc.memoryAllocations,
               gc.memoryDeallocs,
               gc.stringAllocations,
               gc.stringDeallocs,
               gc.memoryRequested,
               gc.memoryUsage,
               gc.memoryFreed,
               gc.memoryMaximum );
}

///============================================================================
/// Generic Hashing Routines:
///
/// The generic hashing routines perform no locking.  It is the responsibility of
/// the object specific wrappers in hash.c to enforce serialization, if needed.
///
/// The hashXxxx routines assume that all hash bucket entries begin with the
/// fields defined by the bucket_t structure.  That is, the first two fields
/// are a pointer to the next bucket entry, followed by the key.  The key is
/// defined as (void *) to ensure that the field packing is correct on both 32
/// and 64 bit systems.  This common mapping of the start of every control
/// block which is stored in a hash table eliminates the need to to allocate
/// separate bucket entries to point to each control block.
///
/// The insert_xxxx routines are expected to call hashInit to initialize the
/// hash table descriptor and allocate the bucket table, as needed.  The
/// lookup_xxxx routines will simply return 0 if it has not be initialized.
/// hashAdd should only be called by the insert_xxxx routines and only after
/// hashInit.
///
/// Hash tables with multiple keys can be supported by using the hashXxxx
/// routines for the primary key, with the object specific wrappers handling
/// any secondary keys.  Optional element allocation and free routines can be
/// specified at hashInit time, if special processing is required.
///
/// hashChange is used to re-hash an entry if its key has changed, possibly
/// moving it to a different bucket.  hashRemove removes the entry from the
/// hash table, but should only be used with external serialization.  hashFree
/// discards the entire hash table and all of its entries, returning the hash
/// table descriptor to an uninitialized state.  hashStats can be called at
/// any time to report the current state of the hash table.  It is
/// automatically called by hashFree before anything is freed.
///
/// hashIter creates an iterator that can be used to walk every entry in the
/// hash table, using the hashMore and hashNext routines.
///
/// Use of a leading nextInBucket pointer makes it possible to use the pointer
/// to each list of bucket as a virtual bucket entry.  For example, this code
/// can be used to traverse the entries in a bucket:
///
///    bucket_t * bp;
///    bp = (bucket_t *)&table->buckv[index];
///
///    while ( 0 != ( bp = bp->nextInBucket ) )
///    {
///    }
///
/// Efficient removal is possible with the following code:
///
///    bucket_t ** pp;                  // Previous bucket pointer
///    pp = &(table->buckv[index]);
///
///    while ( 0 != ( bp = *pp ) )
///    {
///       if ( key == bp->key )
///       {
///          *pp = bp->nextInBucket;     // Remove item from hash bucket
///
///          table->elems--;
///          break;
///       }
///       pp = &(bp->nextInBucket);
///    }
///
///============================================================================

//----------------------------------------------------------------------
// Initialize hash table
//----------------------------------------------------------------------

void hashInit( hash_t * table,
               int      bucks,
               int      itemsize,
               char   * name,
               void   * ( * funcAlloc )(),
               void     ( * funcFree  )( void * ) )
{
   if ( 0 == table->bucks )             // Not already initialized
   {
      table->elems     = 0;
      table->buckv     = zMalloc( "hashInit", bucks * sizeof(bucket_t *));
      table->itemsize  = itemsize;
      table->name      = name;
      table->bucks     = bucks;
      table->funcAlloc = funcAlloc;
      table->funcFree  = funcFree;

      OptVMsgLog( "hashInit: buckets = %8d, itemsize = %8d, name = %s\n",
                  bucks, itemsize, name );
   }
}

//----------------------------------------------------------------------
// If item found in table, return pointer, otherwise return NULL
//----------------------------------------------------------------------

void * hashLookup( hash_t * table, void * key )
{
   int        index;
   bucket_t * bp;

   if ( table->bucks )
   {
      index = PtrToUint32(key) % table->bucks;
      bp    = (bucket_t *)&table->buckv[index];

      while ( 0 != ( bp = bp->nextInBucket ) )
      {
         if ( key == bp->key )
         {
            return( bp );
         }
      }
   }
   return( NULL );
}

//----------------------------------------------------------------------
// Add an item to the hash table, no LOCK
//----------------------------------------------------------------------

void * hashAdd( hash_t * table, void * key )
{
   int        ind;
   bucket_t * bp;

   if ( 0 == table->bucks )
   {
      msg_se_log( "Hash table not initialized" );
      bp = 0;
   }
   else
   {
      ind = PtrToUint32(key) % table->bucks;

      if ( table->funcAlloc )
      {
         bp  = (bucket_t *)( *table->funcAlloc )();
      }
      else
      {
         bp  = (bucket_t *)zMalloc( 0, table->itemsize );
      }

      if ( bp )
      {
         bp->nextInBucket  = table->buckv[ind];   // Pointer to old head of list
         bp->key           = key;

         table->elems++;
         table->buckv[ind] = bp;        // New item is new head of list
      }
   }
   return(bp);
}

//----------------------------------------------------------------------
// Change the key of an item in the hash table, no LOCK
//----------------------------------------------------------------------

void hashChange( hash_t * table, void * keyOld, void * keyNew )
{
   bucket_t  * bp;
   bucket_t ** pp;                      // Previous bucket pointer
   int         indexOld;
   int         indexNew;

   if ( table->bucks )
   {
      indexOld = PtrToUint32( keyOld ) % table->bucks;
      indexNew = PtrToUint32( keyNew ) % table->bucks;

      if ( indexOld == indexNew )
      {
         bp    = (bucket_t *)&table->buckv[indexOld];

         while ( 0 != ( bp = bp->nextInBucket ) )
         {
            if ( keyOld == bp->key )
            {
               bp->key = keyNew;
               break;
            }
         }
      }
      else
      {
         pp = &(table->buckv[indexOld]);

         while ( 0 != ( bp = *pp ) )
         {
            if ( keyOld == bp->key )
            {
               *pp = bp->nextInBucket;  // Remove item from old bucket

               bp->key                = keyNew;
               bp->nextInBucket       = table->buckv[indexNew];
               table->buckv[indexNew] = bp;   // Place item in new bucket

               break;
            }
            pp = &(bp->nextInBucket);
         }
      }
   }
}

//----------------------------------------------------------------------
// Remove an item from the hash table, but return its pointer, no LOCK
//----------------------------------------------------------------------

void hashRemove( hash_t * table, void * key )
{
   int         index;
   bucket_t ** pp;
   bucket_t *  bp;

   if ( table->bucks )
   {
      index = PtrToUint32(key) % table->bucks;
      pp    = &(table->buckv[index]);

      while ( 0 != ( bp = *pp ) )
      {
         if ( key == bp->key )
         {
            *pp = bp->nextInBucket;     // Remove item from hash bucket

            if ( table->funcFree )
            {
               (*table->funcFree)(bp);
            }
            else
            {
               xFree( bp, table->itemsize );
            }
            table->elems--;
            break;
         }
         pp = &(bp->nextInBucket);
      }
   }
}

//----------------------------------------------------------------------
// Free an entire hash table
//----------------------------------------------------------------------

void hashFree( hash_t * table )
{
   uint32_t    i;
   bucket_t  * bp;
   bucket_t  * pp;

   uint32_t    bucks;
   bucket_t ** buckv;

   if ( table->bucks )
   {
      OptVMsgLog( "\nhashFree: name = %s\n", table->name );

      hashStats( table );

      bucks = table->bucks;
      buckv = table->buckv;

      table->bucks    = 0;
      table->buckv    = 0;
      table->elems    = 0;

      for ( i = 0; i < bucks; i++ )
      {
         bp = buckv[i];

         while ( bp )
         {
            pp = bp;
            bp = bp->nextInBucket;

            if ( table->funcFree )
            {
               (*table->funcFree)(pp);
            }
            else
            {
               xFree( pp, table->itemsize );
            }
         }
      }
      table->itemsize = 0;

      xFree( buckv, bucks * sizeof(bucket_t *) );

      MVMsg( gc_mask_Malloc_Free, ( "\nhashFree Complete: name = %s\n", table->name ) );
   }
}

//----------------------------------------------------------------------
// Return the number of entries in the hash table
//----------------------------------------------------------------------

uint32_t hashCount( hash_t * table )
{
   return( table->elems );
}

//----------------------------------------------------------------------
// Return the name of the hash table
//----------------------------------------------------------------------

char * hashName( hash_t * table )
{
   return( table->name );
}

//----------------------------------------------------------------------
// Initialize an iterator for this hash table
//----------------------------------------------------------------------

void hashIter( hash_t * table, hash_iter_t * iter )
{
   iter->table  = table;
   iter->bucket = 0;
   iter->next   = table->buckv[0];
}

//----------------------------------------------------------------------
// Return true if a next item exists in the hash table being traversed
//----------------------------------------------------------------------

int hashMore( hash_iter_t * iter )
{
   if ( iter->next )
   {
      return(1);
   }
   while ( ( iter->bucket + 1 ) < iter->table->bucks )
   {
      iter->bucket++;

      iter->next = iter->table->buckv[ iter->bucket ];

      if ( iter->next )
      {
         return(1);
      }
   }
   return(0);
}

//----------------------------------------------------------------------
// Return the next item in the hash table being traversed
//----------------------------------------------------------------------

void * hashNext( hash_iter_t * iter )
{
   bucket_t * next = 0;

   if ( hashMore( iter ) )
   {
      next       = iter->next;
      iter->next = next->nextInBucket;
   }
   return( next );
}

//----------------------------------------------------------------------
// Write hashing statistics to gc.msg
//----------------------------------------------------------------------

void hashStats( hash_t * table )
{
   bucket_t * bp;
   uint32_t   i;
   uint32_t   n;
   uint32_t   empty   = 0;
   uint32_t   largest = 0;

   if ( table->bucks )
   {
      OptVMsgLog( "\nStatistics for %s\n\n", table->name );

      OptVMsgLog( "Total Elements: %d\n", table->elems );
      OptVMsgLog( "Total Buckets:  %d\n", table->bucks );
      OptVMsgLog( "Element Size:   %d\n", table->itemsize );

      if ( table->elems )
      {
         for ( i = 0; i < table->bucks; i++ )
         {
            bp = table->buckv[i];

            if ( bp )
            {
               n = 0;

               while ( bp )
               {
                  n++;
                  bp = bp->nextInBucket;
               }
               if ( n > largest )
               {
                  largest = n;
               }
            }
            else
            {
               empty++;
            }
         }
      }
      OptVMsgLog( "Empty Buckets:  %d\n", empty );
      OptVMsgLog( "Largest Bucket: %d\n", largest );
   }
}

///============================================================================
/// Parameter Checking with Automatic Help Generation
///
/// The helpXxxx routines are designed to ensure that parameter parsing and
/// help text always stay in synch.  All of the routines require the pointer
/// to the parameter string being validated, parm, plus the keyword being
/// tested and its description for the help listing.
///
/// These routines are expected to be used as the comparison for each valid
/// keyword in a cascaded IF structure.  Except for helpMsg, all of the
/// routines return true if parm matches the keyword, allowing special action
/// to be taken.  Otherwise, false is returned and control passes to the next
/// keyword in the cascade.
///
/// The helpTest routines include an extra parameter that specifies a pointer
/// to a flag field to be set if the option is true, simplifying actions taken
/// when the keyword is matched.  The helpTest2 and helpTest3 routines allow
/// aliases to be specified for the keyword.
///
/// This design allows the same cascaded IF to be used to generate the help
/// listing by simply specifying 0 for parm.  When each helpXxxx sees this 0,
/// it simply writes the description of the keyword to standard error and
/// returns false, allowing the same thing to happen for the next helpXxxx
/// routine in the cascade.  The result is a complete help listing.
///
/// The helpMsg routine always returns false, allowing extra messages to be
/// inserted into the help listing.  If the first character of the description
/// is '?', this help entry will only be displayed when the AllHelp flag is
/// set.
///
/// The gc.posNeg field must be set by the calling routine to indicate whether
/// a prefix of "ALL", "NO", "+" or "-" had been stripped from parm in its
/// initial parsing.
///============================================================================

// Insert a message in the help output, this will always fall thru during parsing

int helpMsg( char * parm,
             char * keyword,
             char * description )
{
   char msgBuf[MSG_BUF_SIZE];

   if ( 0 == parm || 0 == *parm )
   {
      if ( '?' == *description )
      {
         if ( 0 == ( gc.mHelp & gc_mHelp_AllHelp ) )
         {
            return( 0 );
         }
         description++;                 // Skip the '?'
      }

      if ( keyword )
      {
         sprintf( msgBuf, "%s\n\t%s\n", keyword, description );
      }
      else
      {
         sprintf( msgBuf, "%s\n", description );
      }

      writeStderr(msgBuf);
   }
   return( 0 );
}

// helpTest - parameter testing with built-in help generation
//
// Compare the parameter with the keyword with description
// If parm == null, add keyword/description pair to help and return 0
// If parm != null, compare parm to keyword and report:
//                  0 = no match, +1/-1 = positive/negative (+/-) match,
// If flag != null, store true(1) or false(0) into target flag

int helpTest( char * parm,
              void * flag,
              char * keyword,
              char * description )
{
   int    result = 1;                   // Preset positive match
   int    n;

   if ( parm && *parm )
   {
      n = (int)strlen( keyword );

      if ( '=' == keyword[n-1] )        // Compare as prefix
      {
         if ( strncmp( parm, keyword, n ) )
         {
            result = 0;                 // No match
         }
      }
      else                              // Compare as complete string
      {
         if ( strcmp( parm, keyword ) )
         {
            result = 0;                 // No match
         }
      }

      if ( result )                     // Positive or negative match
      {
         OptVMsgLog( " Parsed: %c%d = %s\n",
                     ("- +")[ gc.posNeg + 1 ],
                     result,
                     parm );

         if ( flag )                    // Target flag found
         {
            *(char *)flag = ( gc.posNeg >= 0 ) ? 1 : 0;   // Set true/false into flag
         }
      }
   }
   else                                 // Display description of parameter
   {
      result = helpMsg( 0, keyword, description );   // Allow test to fall thru to next case
   }
   return( result );
}

// Call helpTest for both a keyword and its alias

int helpTest2( char *parm,
               void *flag,
               char *keyword,
               char *alias,
               char *description )
{
   int    result;
   char   keybuf[1024];

   if ( parm && *parm )
   {
      result = helpTest( parm, flag, keyword, 0 );

      if ( 0 == result )
      {
         result = helpTest( parm, flag, alias, 0 );
      }
   }
   else
   {
      strcat( strcat( strcpy( keybuf, keyword ), " | " ), alias );

      result = helpMsg( 0, keybuf, description );
   }
   return( result );
}

// Call helpTest for a keyword and two aliases

int helpTest3( char *parm,
               void *flag,
               char *keyword,
               char *alias,
               char *alias2,
               char *description )
{
   int    result;
   char   keybuf[1024];

   if ( parm && *parm )
   {
      result = helpTest( parm, flag, keyword, 0 );

      if ( 0 == result )
      {
         result = helpTest( parm, flag, alias, 0 );

         if ( 0 == result )
         {
            result = helpTest( parm, flag, alias2, 0 );
         }
      }
   }
   else
   {
      strcat( strcat( strcat( strcat( strcpy( keybuf, keyword ), " | " ), alias ), " | " ), alias2 );

      result = helpMsg( 0, keybuf, description );
   }
   return( result );
}
