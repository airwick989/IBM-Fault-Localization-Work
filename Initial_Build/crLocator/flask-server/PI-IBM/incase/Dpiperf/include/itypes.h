/*
 * IBM Performance Inspector
 * Copyright (c) International Business Machines Corp., 1996 - 2009
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * itypes.h
 * ********
 *
 * This file defines integer data types common to all platforms.
 * The following data types are defined:
 *
 * Boolean types:
 * --------------
 *    BOOL       bool       bool_t
 *
 * 8-bit integer types:                   Max/Min values:
 * --------------------                   ---------------
 *   CHAR       char(*)    char_t   (#)   CHAR_MAX       CHAR_MIN
 *   UCHAR      uchar      uchar_t        UCHAR_MAX      UCHAR_MIN
 *   INT8       int8       int8_t         INT8_MAX       INT8_MIN
 *   UINT8      uint8      uint8_t        UINT8_MAX      UINT8_MIN
 *   BYTE       byte       byte_t         BYTE_MAX       BYTE_MIN
 *
 *   (#) On AIX, CHAR_MAX is defined as unsigned (thus 0xFF not 0x7F)
 *
 * 16-bit integer types:                  Max/Min values:
 * ---------------------                  ---------------
 *   SHORT      short(*)   short_t        SHORT_MAX      SHORT_MIN
 *   USHORT     ushort     ushort_t       USHORT_MAX     USHORT_MIN
 *   INT16      int16      int16_t        INT16_MAX      INT16_MIN
 *   UINT16     uint16     uint16_t       UINT16_MAX     UINT16_MIN
 *   DBLBYTE    dblbyte    dblbyte_t      DBLBYTE_MAX    DBLBYTE_MIN
 *
 * 32-bit integer types:                  Max/Min values:
 * ---------------------                  ---------------
 *   INT        int(*)     int_t          INT_MAX        INT_MIN
 *   UINT       uint       uint_t         UINT_MAX       UINT_MIN
 *   INT32      int32      int32_t        INT32_MAX      INT32_MIN
 *   UINT32     uint32     uint32_t       UINT32_MAX     UINT32_MIN
 *   LONG32     long32     long32_t       LONG32_MAX     LONG32_MIN
 *   ULONG32    ulong32    ulong32_t      ULONG32_MAX    ULONG32_MIN
 *
 * 64-bit integer types:                  Max/Min values:
 * ---------------------                  ---------------
 *   INT64      int64      int64_t        INT64_MAX      INT64_MIN
 *   UINT64     uint64     uint64_t       UINT64_MAX     UINT64_MIN
 *   LONG64     long64     long64_t       LONG64_MAX     LONG64_MIN
 *   ULONG64    ulong64    ulong64_t      ULONG64_MAX    ULONG64_MIN
 *
 *   In addition, macro _INT64_AVAILABLE is defined when 64-bit data
 *   types are defined. It is possible that on certain compilers
 *   the options required to provide 64-bit data types are not used
 *   when compiling.
 *
 * 32 or 64-bit integer types:            Max/Min values:
 * ---------------------------            ---------------
 *   LONG       long(*)    long_t         LONG_MAX       LONG_MIN
 *   ULONG      ulong      ulong_t        ULONG_MAX      ULONG_MIN
 *
 * These types (LONG/ULONG) vary by platform/compiler combination.
 * - Windows: always 32 bits
 * - Linux:   32 or 64 bits depending on 32/64-bit compiler
 * - AIX:     32 or 64 bits depending on -q32 or -q64 compiler option
 * - MVS:     32 or 64 bits depending on 32/64-bit compiler
 *
 * Pointer-precision integer types:       Max values:
 * --------------------------------       -----------
 *   INTPTR     intptr     intptr_t       INTPTR_MAX
 *   UINTPTR    uintptr    uintptr_t      UINTPTR_MAX
 *
 * Good 'ol size_t (signed and unsigned)  Max values:
 * -------------------------------------  -----------
 *   SIZE_T     sizet      size_t         SIZET_MAX
 *   SSIZE_T    ssizet     ssize_t        SSIZET_MAX
 *
 * Printf integer format conversion
 * --------------------------------
 *   _P64  and _L64        Correct prefix for 64-bit numbers
 *   _P64d and _L64d       Decimal, 64-bit
 *   _P64i and _L64i       Decimal, 64-bit
 *   _P64o and _L64o       Octal,   64-bit
 *   _P64u and _L64u       Unsigned integer, 64-bit
 *   _P64x and _L64x       Lowercase hexadecimal, 64-bit
 *   _P64X and _L64X       Uppercase hexadecimal, 64-bit
 *   _Pp                   Lowercase, no leading zeros, pointer
 *   _PZp                  Lowercase, leading zeros, pointer
 *   _PP                   Uppercase, no leading zeros, pointer
 *   _PZP                  Uppercase, leading zeros, pointer
 *   _S                    size_t, no leading zeros, you specify format type
 *   _Sd                   size_t, no leading zeros, decimal
 *   _Si                   size_t, no leading zeros, decimal
 *   _So                   size_t, no leading zeros, octal
 *   _Su                   size_t, no leading zeros, unsigned integer
 *   _Sx                   size_t, no leading zeros, lowercase hexadecimal
 *   _SX                   size_t, no leading zeros, uppercase hexadecimal
 *   _SZ                   size_t, leading zeros, you specify format type
 *   _SZd                  size_t, leading zeros, decimal
 *   _SZi                  size_t, leading zeros, decimal
 *   _SZo                  size_t, leading zeros, octal
 *   _SZu                  size_t, leading zeros, unsigned integer
 *   _SZx                  size_t, leading zeros, lowercase hexadecimal
 *   _SZX                  size_t, leading zeros, uppercase hexadecimal
 *
 *****************************************************************************
 *                          Change History
 *                          **************
 *
 * Vers  When       Why
 * ----  --------  --------------------------------------------------
 * 1.00  2002/03/02  itypes.h is born
 * 1.01  2002/03/11  added auto OS/Arch/Width detection
 * 1.02  2002/03/12  added PtrAdd() and PtrSub()
 * 1.03  2002/03/16  - split off platform detection
 *                   - added boolean types
 * 1.04  2002/05/22  - added Ptr<>Integral type conversion
 *                   - MAX/MIN values
 *                   - added beginnings of MVS/S390 support
 * 1.05  2002/08/27  Fixed typedefs for LONG/ULONG/long_t
 *                   on 64-bit Linux. LONGs are 8 bytes.
 * 1.06  2003/03/05  Added Ptr32ToPtr() and Ptr64ToPtr()
 * 1.07  2003/07/30  Added printf format conversion types
 * 1.08  2003/10/01  Pruned some, added z/OS
 * 1.09  2003/10/14  Futzed with z/OS some more
 * 1.10  2004/09/03  Added zero-extended 64-bit printf
 *                   format conversion types.
 * 1.11  2004/10/07  Added PTR32 for _ZOS
 * 1.12  2005/10/01  Ptr32ToPtr()/Ptr64ToPtr if not defined
 * 1.13  2006/05/09  Added ifdefs for Linux kernel module
 * 1.14  2008/05/21  Windows-only changes:
 *                   - Include windef.h
 *                   - Don't typedef BOOL
 * 1.15  2008/05/23  Driver defines __KERNEL__ so windows.h not included
 * 1.16  2009/01/22  Added size_t (_S) printf format specifiers
 *****************************************************************************
 */
#ifndef _H_ITYPES_
#define _H_ITYPES_

#define ITYPES_H_VERSION   (1.16)

#include <stdbool.h>

/* Platform detection, except for Linux kernel module */
#if !defined(_LINUX) || !defined(__KERNEL__)
   #include <platform.h>
#endif

#if defined(_WINDOWS)
   #if !defined(__KERNEL__)
      #include <windows.h>
   #endif
   #include <windef.h>
   #include <basetsd.h>
   #include <limits.h>
#elif defined(_LINUX)
   #if !defined(__KERNEL__)
      #include <stdint.h>
      #include <limits.h>
      #include <inttypes.h>
   #endif
#elif defined(_AIX)
   #include <inttypes.h>
   #include <sys/types.h>
#elif defined(_ZOS)
   #include <inttypes.h>
   #include <sys/types.h>
#elif defined(_OS2)
   /* Nothing yet */
#else
   #error ***** ITYPES.H: unknown platform/compiler combination *****
#endif


#if !defined(BOOL)
#define BOOL bool
#endif


/*
 * Some useful constants
 * *********************
 */
#if !defined(NULL)
   #define NULL  ((void *)0)
#endif

#if !defined(FALSE)
   #define FALSE  0
#endif

#if !defined(TRUE)
   #define TRUE  1
#endif


/*
 * For describing function arguments
 * *********************************
 *
 * For example:
 *
 *  void ExFunc(IN INT8 byte, OUT INT * type, INOUT INT * result, OPTIONAL ...);
 */
#if !defined(IN)
   #define IN
#endif

#if !defined(OUT)
   #define OUT
#endif

#if !defined(INOUT)
   #define INOUT
#endif

#if !defined(OPTIONAL)
   #define OPTIONAL
#endif


/*
 * 8-bit integer types
 * *******************
 *
 *   CHAR                  char_t
 *   UCHAR      uchar      uchar_t
 *   BYTE       byte       byte_t
 *   INT8       int8       int8_t
 *   UINT8      uint8      uint8_t
 */
#if defined(_AIX)
   typedef signed char         CHAR;
   typedef unsigned char       UCHAR;
   typedef signed char         INT8;
   typedef unsigned char       UINT8;
   typedef unsigned char       BYTE;

   typedef unsigned char       uint8;
   typedef unsigned char       byte;

   typedef signed char         char_t;
   typedef unsigned char       byte_t;

   #define UCHAR_MIN           (0)
   #define BYTE_MAX            UCHAR_MAX
   #define BYTE_MIN            (0)
   #define UINT8_MIN           (0)
#endif

#if defined(_LINUX)
   typedef signed char         CHAR;
#ifndef STAP_ITRACE // DEFS
   typedef unsigned char       UCHAR;
#endif // STAP_ITRACE DEFS
   typedef signed char         INT8;
   typedef unsigned char       UINT8;
   typedef unsigned char       BYTE;

   typedef unsigned char       uchar;
   typedef signed char         int8;
   typedef unsigned char       uint8;
   typedef unsigned char       byte;

   typedef signed char         char_t;
   typedef unsigned char       uchar_t;
   typedef unsigned char       byte_t;

   #if defined(_X86_64) || defined(CONFIG_X86_64)
      #if !defined(__KERNEL__)
         typedef signed char         s8;
         typedef unsigned char       u8;
      #endif
   #endif

   #define UCHAR_MIN           (0)
   #define BYTE_MAX            UCHAR_MAX
   #define BYTE_MIN            (0)
   #define UINT8_MIN           (0)
#endif

#if defined(_WINDOWS)
   typedef signed char         INT8;
   typedef unsigned char       UINT8;

   typedef unsigned char       uchar;
   typedef signed char         int8;
   typedef unsigned char       uint8;
   typedef unsigned char       byte;

   typedef signed char         char_t;
   typedef unsigned char       uchar_t;
   typedef signed char         int8_t;
   typedef unsigned char       uint8_t;
   typedef unsigned char       byte_t;

   #define INT8_MAX            _I8_MAX
   #define INT8_MIN            _I8_MIN
   #define UINT8_MAX           _UI8_MAX
   #define UINT8_MIN           (0ui8)
   #define UCHAR_MIN           (0ui8)
   #define BYTE_MAX            _UI8_MAX
   #define BYTE_MIN            (0ui8)
#endif

#if defined(_ZOS)
   typedef signed char         CHAR;
   typedef unsigned char       UCHAR;
   typedef signed char         INT8;
   typedef unsigned char       UINT8;
   typedef unsigned char       BYTE;

   typedef unsigned char       uchar;
   typedef signed char         int8;
   typedef unsigned char       uint8;
   typedef unsigned char       byte;

   typedef signed char         char_t;
   typedef unsigned char       uchar_t;
// typedef signed char         int8_t;
   typedef unsigned char       byte_t;

   #define INT8_MAX            SCHAR_MAX
   #define INT8_MIN            SCHAR_MIN
   #define UINT8_MAX           UCHAR_MAX
   #define UINT8_MIN           (0)
   #define UCHAR_MIN           (0)
   #define BYTE_MAX            UCHAR_MAX
   #define BYTE_MIN            (0)
#endif


/*
 * 16-bit integer types
 * ********************
 *
 *   SHORT                 short_t
 *   USHORT     ushort     ushort_t
 *   INT16      int16      int16_t
 *   UINT16     uint16     uint16_t
 *   DBLBYTE     dblbyte     dblbyte_t
 */
#if defined(_AIX)
   typedef signed short        SHORT;
   typedef unsigned short      USHORT;
   typedef signed short        INT16;
   typedef unsigned short      UINT16;
   typedef unsigned short      DBLBYTE;

   typedef unsigned short      uint16;
   typedef unsigned short      dblbyte;

   typedef signed short        short_t;
   typedef unsigned short      dblbyte_t;

   #define SHORT_MAX           SHRT_MAX
   #define SHORT_MIN           SHRT_MIN
   #define USHORT_MAX          USHRT_MAX
   #define USHORT_MIN          (0)
   #define UINT16_MIN          (0)
   #define DBLBYTE_MAX         USHRT_MAX
   #define DBLBYTE_MIN         (0)
#endif

#if defined(_LINUX)
   typedef signed short        SHORT;
   typedef unsigned short      USHORT;
   typedef signed short        INT16;
   typedef unsigned short      UINT16;
   typedef unsigned short      DBLBYTE;

   typedef signed short        int16;
   typedef unsigned short      uint16;
   typedef unsigned short      dblbyte;

   typedef signed short        short_t;
   typedef unsigned short      ushort_t;
   typedef unsigned short      dblbyte_t;

   #if defined(_X86_64) || defined(CONFIG_X86_64)
      #if !defined(__KERNEL__)
         typedef signed short        s16;
         typedef unsigned short      u16;
      #endif
   #endif

   #if !defined(SHORT_MAX)
   #define SHORT_MAX           SHRT_MAX
   #endif
   #if !defined(SHORT_MIN)
   #define SHORT_MIN           SHRT_MIN
   #endif
   #if !defined(USHORT_MAX)
   #define USHORT_MAX          USHRT_MAX
   #endif
   #if !defined(USHORT_MIN)
   #define USHORT_MIN          (0)
   #endif
   #define UINT16_MIN          (0)
   #define DBLBYTE_MAX         USHRT_MAX
   #define DBLBYTE_MIN         (0)
#endif

#if defined(_WINDOWS)
   typedef signed short        INT16;
   typedef unsigned short      UINT16;
   typedef unsigned short      DBLBYTE;

   typedef unsigned short      ushort;
   typedef signed short        int16;
   typedef unsigned short      uint16;
   typedef unsigned short      dblbyte;

   typedef signed short        short_t;
   typedef unsigned short      ushort_t;
   typedef signed short        int16_t;
   typedef unsigned short      uint16_t;
   typedef unsigned short      dblbyte_t;

   #define INT16_MAX           _I16_MAX
   #define INT16_MIN           _I16_MIN
   #define UINT16_MAX          _UI16_MAX
   #define UINT16_MIN          (0ui16)
   #define SHORT_MAX           _I16_MAX
   #define SHORT_MIN           _I16_MIN
   #define USHORT_MAX          _UI16_MAX
   #define USHORT_MIN          (0ui16)
   #define DBLBYTE_MAX         _UI16_MAX
   #define DBLBYTE_MIN         (0ui16)
#endif

#if defined(_ZOS)
   typedef signed short        SHORT;
   typedef unsigned short      USHORT;
   typedef signed short        INT16;
   typedef unsigned short      UINT16;
   typedef unsigned short      DBLBYTE;

   typedef signed short        int16;
   typedef unsigned short      uint16;
   typedef unsigned short      dblbyte;

// typedef unsigned short      ushort;
   typedef signed short        short_t;
   typedef unsigned short      ushort_t;
   typedef unsigned short      dblbyte_t;

   #define SHORT_MAX           SHRT_MAX
   #define SHORT_MIN           SHRT_MIN
   #define USHORT_MAX          USHRT_MAX
   #define USHORT_MIN          (0)
   #define INT16_MAX           SHRT_MAX
   #define INT16_MIN           SHRT_MIN
   #define UINT16_MAX          USHRT_MAX
   #define UINT16_MIN          (0)
   #define DBLBYTE_MAX         USHRT_MAX
   #define DBLBYTE_MIN         (0)
#endif


/*
 * 32-bit integer types
 * ********************
 *
 *   INT                   int_t
 *   UINT       uint       uint_t
 *   INT32      int32      int32_t
 *   UINT32     uint32     uint32_t
 *   LONG32     long32     long32_t
 *   ULONG32    ulong32    ulong32_t
 */
#if defined(_AIX)
   typedef signed int          INT;
   typedef unsigned int        UINT;
   typedef signed int          INT32;
   typedef unsigned int        UINT32;
   typedef signed int          LONG32;
   typedef unsigned int        ULONG32;

   typedef unsigned int        uint32;
   typedef signed int          long32;
   typedef unsigned int        ulong32;

   typedef signed int          int_t;
   typedef signed int          long32_t;
   typedef unsigned int        ulong32_t;

   #define UINT_MIN            (0)
   #define UINT32_MIN          (0)
   #define LONG32_MAX          INT_MAX
   #define LONG32_MIN          INT_MIN
   #define ULONG32_MAX         UINT_MAX
   #define ULONG32_MIN         (0)
#endif

#if defined(_LINUX)
   typedef signed int          INT;
   typedef unsigned int        UINT;
   typedef signed int          INT32;
#ifndef STAP_ITRACE // DEFS
#ifndef UINT32
   typedef unsigned int        UINT32;
#endif
#endif // STAP_ITRACE DEFS
   typedef signed int          LONG32;
   typedef unsigned int        ULONG32;

   typedef signed int          int32;
   typedef unsigned int        uint32;
   typedef signed int          long32;
   typedef unsigned int        ulong32;

   typedef signed int          int_t;
   typedef unsigned int        uint_t;
   typedef signed int          long32_t;
   typedef unsigned int        ulong32_t;

   #if defined(_X86_64) || defined(CONFIG_X86_64)
      #if !defined(__KERNEL__)
         typedef signed int          s32;
         typedef unsigned int        u32;
      #endif
   #endif

   #define UINT_MIN            (0)
   #define UINT32_MIN          (0)
   #define LONG32_MAX          INT_MAX
   #define LONG32_MIN          INT_MIN
   #define ULONG32_MAX         UINT_MAX
   #define ULONG32_MIN         (0)
#endif

#if defined(_WINDOWS)
   typedef signed int          INT;
   typedef unsigned int        UINT;
   typedef signed int          INT32;
   typedef unsigned int        UINT32;

   typedef unsigned int        uint;
   typedef signed int          int32;
   typedef unsigned int        uint32;
   typedef signed int          long32;
   typedef signed int          ulong32;

   typedef signed int          int_t;
   typedef unsigned int        uint_t;
   typedef signed int          int32_t;
   typedef unsigned int        uint32_t;
   typedef signed int          long32_t;
   typedef unsigned int        ulong32_t;

   #define UINT_MIN            (0i32)
   #define INT32_MAX           _I32_MAX
   #define INT32_MIN           _I32_MIN
   #define UINT32_MAX          _UI32_MAX
   #define UINT32_MIN          (0ui32)
   #define LONG32_MAX          _I32_MAX
   #define LONG32_MIN          _I32_MAX
   #define ULONG32_MAX         _UI32_MAX
   #define ULONG32_MIN         (0)
#endif

#if defined(_ZOS)
   typedef signed int          INT;
   typedef unsigned int        UINT;
   typedef signed int          INT32;
   typedef unsigned int        UINT32;
   typedef signed int          LONG32;
   typedef unsigned int        ULONG32;

   typedef signed int          int32;
   typedef unsigned int        uint32;
   typedef unsigned int        uint;
   typedef signed int          long32;
   typedef unsigned int        ulong32;

   typedef signed int          int_t;
   typedef signed int          long32_t;
   typedef unsigned int        uint_t;
   typedef unsigned int        ulong32_t;

   #define UINT_MIN            (0)
   #define INT32_MAX           INT_MAX
   #define INT32_MIN           INT_MIN
   #define UINT32_MAX          UINT_MAX
   #define UINT32_MIN          (0)
   #define LONG32_MAX          INT_MAX
   #define LONG32_MIN          INT_MIN
   #define ULONG32_MAX         UINT_MAX
   #define ULONG32_MIN         (0)
#endif


/*
 * 32/64-bit integer types
 * ***********************
 *
 *   LONG       long       long_t
 *   ULONG      ulong      ulong_t
 */
#if defined(_AIX)
   typedef signed long         LONG;
   typedef unsigned long       ULONG;
   typedef signed long         long_t;

   #define ULONG_MIN           (0)
#endif

#if defined(_LINUX)
   typedef signed long         LONG;
   typedef unsigned long       ULONG;
   typedef signed long         long_t;
   typedef unsigned long       ulong_t;

   #define ULONG_MIN           (0)
#endif

#if defined(_WINDOWS)
   typedef signed int          long_t;
   typedef unsigned int        ulong;
   typedef unsigned int        ulong_t;

   #define ULONG_MIN           (0ui32)
#endif

#if defined(_ZOS)
   typedef signed long         LONG;
   typedef signed long         long_t;
   typedef unsigned long       ULONG;
   typedef unsigned long       ulong;
   typedef unsigned long       ulong_t;

   #define ULONG_MIN           (0)
#endif


/*
 * 64-bit integer types
 * ********************
 *
 *   INT64      int64      int64_t
 *   UINT64     uint64     uint64_t
 *   LONG64     long64     long64_t
 *   ULONG64    ulong64    ulong64_t
 */
#if defined(_AIX)
   #define _INT64_AVAILABLE

   typedef int64_t             INT64;
   typedef uint64_t            UINT64;
   typedef int64_t             LONG64;
   typedef uint64_t            ULONG64;

   typedef uint64_t            uint64;
   typedef int64_t             long64;
   typedef uint64_t            ulong64;

   typedef int64_t             long64_t;
   typedef uint64_t            ulong64_t;

   #define LONG64_MAX          INT64_MAX
   #define ULONG64_MAX         UINT64_MAX
   #define UINT64_MIN          (0)
   #define LONG64_MIN          INT64_MIN
   #define ULONG64_MIN         (0)
#endif

#if defined(_LINUX)
   #define _INT64_AVAILABLE

   typedef int64_t             INT64;
#ifndef STAP_ITRACE // DEFS
#ifndef UINT64
   typedef uint64_t            UINT64;
#endif
#endif // STAP_ITRACE DEFS
   typedef int64_t             LONG64;
   typedef uint64_t            ULONG64;

   typedef int64_t             int64;
   typedef uint64_t            uint64;
   typedef int64_t             long64;
   typedef uint64_t            ulong64;

   typedef int64_t             long64_t;
   typedef uint64_t            ulong64_t;

   #if defined(_X86_64) || defined(CONFIG_X86_64)
      #if !defined(__KERNEL__)
         typedef signed long long    s64;
         typedef unsigned long long  u64;
     #endif
   #endif

   #define UINT64_MIN          (0)
   #define LONG64_MAX          INT64_MAX
   #define LONG64_MIN          INT64_MIN
   #define ULONG64_MAX         UINT64_MAX
   #define ULONG64_MIN         (0)
#endif

#if defined(_WINDOWS)
   #define _INT64_AVAILABLE

   typedef signed __int64      INT64;
   typedef unsigned __int64    UINT64;
   typedef signed __int64      LONG64;
   typedef unsigned __int64    ULONG64;

   typedef signed __int64      int64;
   typedef unsigned __int64    uint64;
   typedef signed __int64      long64;
   typedef unsigned __int64    ulong64;

   typedef signed __int64      int64_t;
   typedef unsigned __int64    uint64_t;
   typedef signed __int64      long64_t;
   typedef unsigned __int64    ulong64_t;

   #define INT64_MAX           _I64_MAX
   #define INT64_MIN           _I64_MIN
   #define UINT64_MAX          _UI64_MAX
   #define UINT64_MIN          (0ui64)
   #define LONG64_MAX          _I64_MAX
   #define LONG64_MIN          _I64_MIN
   #define ULONG64_MAX         _UI64_MAX
   #define ULONG64_MIN         (0ui64)
#endif

#if defined(_ZOS)
   /*
    * Must use -W "langlvl(EXTENDED)" or "langlvl(LONGLONG)"
    * in order to have and be able to use 64-bit data types
    * on 31-bit compilers (< V1R5). If either of those options
    * is not used then the 64-bit data types will not be defined.
    */
   #if defined(_LP64) || defined(__LL)
      #define _INT64_AVAILABLE

      typedef int64_t             INT64;
      typedef uint64_t            UINT64;
      typedef int64_t             LONG64;
      typedef uint64_t            ULONG64;

      typedef int64_t             int64;
      typedef uint64_t            uint64;
      typedef int64_t             long64;
      typedef uint64_t            ulong64;

      typedef int64_t             long64_t;
      typedef uint64_t            ulong64_t;
   #endif

   #if defined(_LP64)
      #define INT64_MAX           LONG_MAX
      #define INT64_MIN           LONG_MIN
      #define UINT64_MAX          ULONG_MAX
      #define UINT64_MIN          (0)
      #define LONG64_MAX          LONG_MAX
      #define LONG64_MIN          LONG_MIN
      #define ULONG64_MAX         ULONG_MAX
      #define ULONG64_MIN         (0)
   #elif defined(__LL)
      #define INT64_MAX           LONGLONG_MAX
      #define INT64_MIN           LONGLONG_MIN
      #define UINT64_MAX          ULONGLONG_MAX
      #define UINT64_MIN          (0)
      #define LONG64_MAX          LONGLONG_MAX
      #define LONG64_MIN          LONGLONG_MIN
      #define ULONG64_MAX         ULONGLONG_MAX
      #define ULONG64_MIN         (0)
   #endif
#endif


/*
 * Pointer-precision integer types
 * *******************************
 *
 * As the pointer precision changes (32 or 64 bits depending on the platform)
 * these data types reflect the precision accordingly. They are guaranteed to
 * be the same size as a pointer: if a pointer is 64 bits these types are 64 bits,
 * if a pointer is 32 bits these types are 32 bits.
 * Therefore, it is safe to cast a pointer to one of these types when performing
 * pointer arithmetic.
 *
 *   INTPTR     intptr     intptr_t
 *   UINTPTR    uintptr    uintptr_t
 *   PTR32 (z/OZ only)
 */
#if defined(_AIX)
   typedef intptr_t            INTPTR;
   typedef uintptr_t           UINTPTR;
   typedef intptr_t            intptr;
   typedef uintptr_t           uintptr;
#endif

#if defined(_LINUX)
   typedef long int            INTPTR;
   typedef unsigned long int   UINTPTR;

   #if !defined(__intptr_t_defined) && !defined(__KERNEL__)
   typedef INTPTR             intptr_t;
   typedef UINTPTR            uintptr_t;
   #endif

   typedef INTPTR             intptr;
   typedef UINTPTR            uintptr;
#endif

#if defined(_WINDOWS)
   #if defined(_64BIT)
      typedef __int64             INTPTR;
      typedef unsigned __int64    UINTPTR;
      typedef __int64             intptr;
      typedef unsigned __int64    uintptr;
      typedef __int64             intptr_t;
      typedef unsigned __int64    uintptr_t;
   #elif defined(_32BIT)
      typedef signed int          INTPTR;
      typedef unsigned int        UINTPTR;
      typedef signed int          intptr;
      typedef unsigned int        uintptr;
      typedef signed int          intptr_t;
      typedef unsigned int        uintptr_t;
   #else
      #error ***** ITYPES.H: Windows but not IA64 nor X86 ****
   #endif

   #define INTPTR_MAX             MAXINT_PTR
   #define UINTPTR_MAX            MAXUINT_PTR
#endif

#if defined(_ZOS)
   typedef intptr_t            INTPTR;
   typedef uintptr_t           UINTPTR;
   typedef intptr_t            intptr;
   typedef uintptr_t           uintptr;

   #if defined(_LP64)
      #define INTPTR_MAX          LONG_MAX
      #define UINTPTR_MAX         ULONG_MAX
      #define PTR32               __ptr32
   #elif defined(__LL)
      #define INTPTR_MAX          LONGLONG_MAX
      #define UINTPTR_MAX         ULONGLONG_MAX
      #define PTR32
   #else
      #define INTPTR_MAX          INT_MAX
      #define UINTPTR_MAX         UINT_MAX
      #define PTR32
   #endif
#endif


/*
 * size_t/ssize_t
 * **************
 *
 *   SIZE_T     sizet      size_t
 *   SSIZE_T    ssizet     ssize_t
 */
#if defined(_AIX)
   typedef uintptr_t           SIZE_T;
   typedef uintptr_t           sizet;
   typedef intptr_t            SSIZE_T;
   typedef intptr_t            ssizet;

   #define SIZET_MAX           UINTPTR_MAX
   #define SSIZET_MAX          INTPTR_MAX
#endif

#if defined(_LINUX) && !defined(__KERNEL__)
   typedef uintptr_t           SIZE_T;
   typedef uintptr_t           sizet;
   typedef intptr_t            SSIZE_T;
   typedef intptr_t            ssizet;

   #define SIZET_MAX           UINTPTR_MAX
   #define SSIZET_MAX          INTPTR_MAX
#endif

#if defined(_WINDOWS)
   typedef SSIZE_T             ssize_t;
   typedef SIZE_T              sizet;
   typedef SSIZE_T             ssizet;

   #define SIZET_MAX           MAXUINT_PTR
   #define SSIZET_MAX          MAXINT_PTR
#endif

#if defined(_ZOS)
   typedef size_t              SIZE_T;
   typedef size_t              sizet;
   typedef ssize_t             SSIZE_T;
   typedef ssize_t             ssizet;

   #define SIZET_MAX           UINTPTR_MAX
   #define SSIZET_MAX          INTPTR_MAX
#endif


/*
 * Miscellaneous other data types
 * ******************************
 */
#if defined(_WINDOWS)
   /* Most efficient type on platform */
   #if defined(_64BIT)
      typedef int64_t         intfast_t;
      typedef uint64_t        uintfast_t;
   #else
      typedef int32_t         intfast_t;
      typedef uint32_t        uintfast_t;
   #endif

   /* Memory address, byte addressable */
   typedef char *          caddr_t;
   typedef char *          cptr_t;

   /* Half a pointer. Unsigned. */
   #if defined(_64BIT)
      typedef uint32_t     halfptr_t;
   #else
      typedef ushort_t     halfptr_t;
   #endif

   /* Hardware types. byte_t already defined above. */
   typedef ushort_t        word_t;
   typedef uint32_t        dword_t;
   typedef uint64_t        qword_t;

   /* Fixed-size pointers. Unsigned. */
   #if defined(_64BIT)
      typedef void *          ptr64_t;
      typedef char *          cptr64_t;
      typedef uint32_t        ptr32_t;
      typedef uint32_t        cptr32_t;
   #else
      typedef uint64_t        ptr64_t;
      typedef uint64_t        cptr64_t;
      typedef void *          ptr32_t;
      typedef char *          cptr32_t;
   #endif
#endif


/*
 * Pointer arithmetic macros
 * *************************
 *
 * - p     cannot be an expression - it must be a pointer variable.
 * - v     can be an expression or a constant - anything that can be added.
 *
 * For example, given:
 *   void * pvoid;
 *   int  * pint;
 *
 *   pint = (int *)PtrAdd(pvoid, sizeof(int));
 *      // Adds 4 (sizeof(int)) to the value of pvoid and casts the result
 *      // back to (int *)
 *
 *   pvoid = (void *)PtrSub(pint, sizeof(char));
 *      // Subtracts 1 (sizeof(char) from the value of pint and casts the
 *      // result back to (void *)
 */
#define PtrAdd(p,v)     ( ((uintptr)(p)) + (uintptr)(v) )  /* Add v to p */
#define PtrSub(p,v)     ( ((uintptr)(p)) - (uintptr)(v) )  /* Subtract v from p */


/*
 * Pointer-to-Integral types conversion macros
 * *******************************************
 *
 * - p     must be a pointer-type variable
 *
 *                  on 32-bit machine            on 64-bit machine
 *                  -------------------------    -----------------------
 * PtrToInt32(p)    Cast ptr to int32            Truncate ptr to int32
 * PtrToLong32(p)   Cast ptr to long32           Truncate ptr to long32
 * PtrToUint32(p)   Cast ptr to uint32           Truncate ptr to uint32
 * PtrToUlong32(p)  Cast ptr to ulong32          Truncate ptr to ulong32
 *
 * PtrToInt64(p)    Sign-extend ptr to int64     Cast ptr to int64
 * PtrToLong64(p)   Sign-extend ptr to long64    Cast ptr to long64
 * PtrToUint64(p)   Zero-extend ptr to uint64    Cast ptr to uint64
 * PtrToUlong64(p)  Zero-extend ptr to ulong64   Cast ptr to ulong64
 */
#define PtrToInt32(p)   (   (int32) ((intptr)(p)) )
#define PtrToLong32(p)  (  (long32) ((intptr)(p)) )
#define PtrToUint32(p)  (  (uint32) ((uintptr)(p)) )
#define PtrToUlong32(p) ( (ulong32) ((uintptr)(p)) )
#define PtrToInt64(p)   (   (int64) ((intptr)(p)) )
#define PtrToLong64(p)  (  (long64) ((intptr)(p)) )
#define PtrToUint64(p)  (  (uint64) ((uintptr)(p)) )
#define PtrToUlong64(p) ( (ulong64) ((uintptr)(p)) )

/*
 * Integral-to-Pointer types conversion macros
 * *******************************************
 *
 * - i     must be an integral-type variable
 *
 *                  on 32-bit machine            on 64-bit machine
 *                  -------------------------    -----------------------
 * Int32ToPtr(i)    Cast int32 to void *         Sign-extend int32 to ptr
 * Long32ToPtr(i)   Cast long32 to void *        Sign-extend long32 to ptr
 * Uint32ToPtr(i)   Cast uint32 to void *        Zero-extend uint32 to ptr
 * Ulong32ToPtr(i)  Cast ulong32 to void *       Zero-extend ulong32 to ptr
 *
 * Int64ToPtr(i)    Truncate int64 to ptr        Cast int64 to void *
 * Long64ToPtr(i)   Truncate long64 to ptr       Cast long64 to void *
 * Uint64ToPtr(i)   Truncate uint64 to ptr       Cast uint64 to void *
 * Ulong64ToPtr(i)  Truncate ulong64 to ptr      Cast ulong64 to void *
 */
#define Int32ToPtr(i)   ( (void *) ( (intptr) ((int32)(i))) )
#define Long32ToPtr(i)  ( (void *) ( (intptr) ((long32)(i))) )
#define Uint32ToPtr(i)  ( (void *) ((uintptr) ((uint32)(i))) )
#define Ulong32ToPtr(i) ( (void *) ((uintptr) ((ulong32)(i))) )

#define Int64ToPtr(i)   ( (void *) ( (intptr) ((int64)(i))) )
#define Long64ToPtr(i)  ( (void *) ( (intptr) ((long64)(i))) )
#define Uint64ToPtr(i)  ( (void *) ((uintptr) ((uint64)(i))) )
#define Ulong64ToPtr(i) ( (void *) ((uintptr) ((ulong64)(i))) )

/*
 * Pointer promotion/demotion
 * **************************
 *
 * These are intended for promoting/demoting native pointers.
 * The macros work different depending on how the source is compiled:
 *
 *                     Compiled as 32-bit            Compiled as 64-bit
 * Ptr32ToPtr(p)             (p)                        (uintptr)(p)
 * Ptr64ToPtr(p)          (uintptr)(p)                      (p)
 *
 */
#if defined(_32BIT)
   #if !defined(Ptr32ToPtr)
      #define Ptr32ToPtr(p)   (p)
   #endif
#else
   #if !defined(Ptr32ToPtr)
      #define Ptr32ToPtr(p)   ((uintptr)(p))
   #endif
#endif


#if defined(_32BIT)
   #if !defined(Ptr64ToPtr)
      #define Ptr64ToPtr(p)   ((uintptr)(p))
   #endif
#else
   #if !defined(Ptr64ToPtr)
      #define Ptr64ToPtr(p)   (p)
   #endif
#endif

/*
 * 64<->32 bit promotion/demotion
 * ******************************
 */
#define Uint64ToUint32(u)    (  (uint32) ((uintptr) ((uint64)(u))) )
#define Uint64ToInt32(u)     (   (int32) ((uintptr) ((uint64)(u))) )
#define Int64ToInt32(i)      (   (int32) ( (intptr) ((int64)(i))) )
#define Int64ToUint32(i)     (  (uint32) ( (intptr) ((int64)(i))) )
#define Ulong64ToUlong32(u)  ( (ulong32) ((uintptr) ((ulong64)(u))) )

#define Uint32ToUint64(u)    (  (uint64) ((uintptr) ((uint32)(u))) )
#define Int32ToUint64(i)     (  (uint64) ( (intptr) ((int32)(i))) )
#define Int32ToInt64(i)      (   (int64) ( (intptr) ((int32)(i))) )
#define Uint32ToInt64(u)     (   (int64) ((uintptr) ((uint32)(u))) )
#define Ulong32ToUlong64(u)  ( (ulong64) ((uintptr) ((ulong32)(u))) )

/*
 * Printf integer format conversion
 * ********************************
 */
#if defined(_AIX)
   #if defined(__64BIT__)
      #define _P64         "l"
   #else
      #if defined(_LONG_LONG)
         #define _P64      "ll"
      #endif
   #endif
#endif

#if defined(_LINUX) && !defined(__KERNEL__)
   #if (__WORDSIZE == 64)
      #define _P64         "l"
   #else
      #define _P64         "ll"
   #endif
#endif

#if defined(WINDOWS)
   #define _P64        "I64"
#endif

#if defined(_ZOS)
   #if defined(_LP64)
      #define _P64         "l"
   #elif defined(__LL)
      #define _P64        "ll"
   #else
      #define _P64         "l"
   #endif
#endif

#define _P64d       _P64 "d"
#define _P64i       _P64 "i"
#define _P64o       _P64 "o"
#define _P64u       _P64 "u"
#define _P64x       _P64 "x"
#define _P64X       _P64 "X"
#define _L64        _P64
#define _L64d       _P64d
#define _L64i       _P64i
#define _L64o       _P64o
#define _L64u       _P64u
#define _L64x       _P64x
#define _L64X       _P64X

#define _PZ64       "016" _P64
#define _PZ64d      _PZ64 "d"
#define _PZ64i      _PZ64 "i"
#define _PZ64o      _PZ64 "o"
#define _PZ64u      _PZ64 "u"
#define _PZ64x      _PZ64 "x"
#define _PZ64X      _PZ64 "X"
#define _LZ64       _PZ64
#define _LZ64d      _PZ64d
#define _LZ64i      _PZ64i
#define _LZ64o      _PZ64o
#define _LZ64u      _PZ64u
#define _LZ64x      _PZ64x
#define _LZ64X      _PZ64X

#if defined(_32BIT)
   /* pointers (P)/size_t (S) */
   #define _Pp         "x"
   #define _PZp        "08x"
   #define _PP         "X"
   #define _PZP        "08X"
   #define _S
   #define _Sd         "d"
   #define _Si         "i"
   #define _So         "o"
   #define _Su         "u"
   #define _Sx         "x"
   #define _SX         "X"
   #define _SZ         "08"
   #define _SZd        "08d"
   #define _SZi        "08i"
   #define _SZo        "08o"
   #define _SZu        "08u"
   #define _SZx        "08x"
   #define _SZX        "08X"
#else
   /* pointers (P)/size_t (S) */
   #define _PZp        _PZ64x
   #define _PP         _P64X
   #define _PZP        _PZ64X
   #define _S          _P64
   #define _Sd         _P64d
   #define _Si         _P64i
   #define _So         _P64o
   #define _Su         _P64u
   #define _Sx         _P64x
   #define _SX         _P64X
   #define _SZ         _PZ64
   #define _SZd        _PZ64d
   #define _SZi        _PZ64i
   #define _SZo        _PZ64o
   #define _SZu        _PZ64u
   #define _SZx        _PZ64x
   #define _SZX        _PZ64X
#endif


/*
* Miscellaneous macros
* ********************
*
* FieldOffset()
* -------------
*    Calculate the byte offset of a 'field' in a structure of type 'type'.
*
*/

#define FieldOffset(field, type)   ((int)(intptr)&(((type *)0)->field))


#endif /* _ITYPES_ */
