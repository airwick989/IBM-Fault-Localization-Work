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
 * platform.h
 * **********
 *
 * This file defines a common set of OS, architecture and word size
 * macros that can be used to uniquely identify a target platform.
 * The macros are infered from macros already defined by the compiler
 * standard makefiles or the user.
 * It should be the first file included in your C source file.
 *
 * *********************************************************************
 * This header file is compiler dependant. It has been tested with
 * the following compilers:
 *
 * Windows:    Microsoft Visual C++ v5
 *             Microsoft Visual C++ v6
 *             Microsoft Visual C++ v7 (.NET/2003)
 *             Microsoft Visual C++ v8 (2005)
 *
 * Linux:      gcc v2.95.3, v3.2
 *
 * AIX:        IBM C and C++, v3.6.6
 *             VisualAge C++ Professional / C for AIX, v5
 *
 * OS/2:       VisualAge C++, v3
 *
 * z/OS:       z/OS C/C++ Compiler V1R2 (31-bit)
 * *********************************************************************
 *
 *
 * The following OS macros are defined:
 * ------------------------------------
 *  _OS2      and:        _OS232
 *  _WINDOWS  and either: _WINDOWS32  or _WINDOWS64
 *  _LINUX    and either: _LINUX32    or _LINUX64
 *  _AIX      and either: _AIX32      or _AIX64
 *  _ZOS      and either: _ZOS31      or _ZOS64
 *
 * The following architecture macros are defined:
 * ----------------------------------------------
 *  _X86, _I386 and _IA32            * Intel IA32
 *  _IA64                            * Intel IA64
 *  _X86_64, _X64 and _AMD64         * AMD x86-64
 *  _PPC and _PPC32                  * PowerPC 32-bit
 *  _PPC and _PPC64                  * PowerPC 64-bit
 *  _S390                            * System 390 31-bit
 *  _S390 and _S390X                 * System 390 64-bit
 *
 * The following word size macros are defined:
 * -------------------------------------------
 *  _32BIT                           * 32-bit word size
 *  _64BIT                           * 64-bit word size
 *
 * The following "endianness" macros are defined:
 * ----------------------------------------------
 *  _BIG_ENDIAN    and _BIGENDIAN    * Big endian byte order
 *  _LITTLE_ENDIAN and _LITTLEENDIAN * Little endian byte order
 *
 * The following "character encoding" macros are defined:
 * ------------------------------------------------------
 *  _CHAR_ASCII                      * ASCII character encoding
 *  _CHAR_EBCDIC                     * EBCDIC character encoding
 *
 * The following "platform" macro is defined:
 * ------------------------------------------
 * _PLATFORM_STR
 *
 *   Values are:
 *       "OS/2"                      * OS/2 (x86 only)
 *       "Windows-x86"               * Windows on x86
 *       "Windows-IA64"              * Windows on IA64
 *       "Windows-x86-64"            * Windows on AMD64/EM64T
 *       "Linux-x86"                 * Linux on x86
 *       "Linux-IA64"                * Linux on IA64
 *       "Linux-PPC32"               * Linux on PPC32
 *       "Linux-PPC64"               * Linux on PPC64
 *       "Linux-s390"                * Linux on S390
 *       "Linux-s390x"               * Linux on S390X
 *       "Linux-x86-64"              * Linux on AMD64/EM64T
 *       "AIX-PPC32"                 * AIX on PPC32
 *       "AIX-PPC64"                 * AIX on PPC64
 *       "AIX-IA64"                  * AIC on IA64
 *       "z/OS-31"                   * z/OS on z/31
 *       "z/OS-64"                   * z/OS on z/64
 *
 * _ARCH_STR
 *
 *   Values are:
 *       "x86"                       * Intel IA32
 *       "x64"                       * AMD64 or Intel EM64T (on Windows)
 *       "x86-64"                    * AMD64 or Intel EM64T (on Linux)
 *       "IA64"                      * Intel IA64
 *       "PPC32"                     * 32-bit PowerPC
 *       "PPC64"                     * 64-bit PowerPC
 *       "s390"                      * IBM System/390 (31-bit)
 *       "s390x"                     * IBM System/390 (64-bit)
 *
 *************************************************************************
 *                          Change History
 *                          **************
 *
 * Vers  When       Why
 * ----  --------  -------------------------------------------------------
 * 1.00  03/16/02  platform.h is born (split off itypes.h)
 * 1.01  05/01/02  limited MVS/s390 support
 * 1.02  05/06/02  Linux on s390 support
 * 1.03  08/02/02  Added OS## macros
 * 1.04  03/06/03  Added _LITTLEENDIAN, _S390X
 * 1.05  03/25/03  Added _PLATFORM_STR
 * 1.06  07/29/03  Don't redefine variables already defined
 * 1.07  10/01/03  Add z/OS support, remove _MVS
 * 1.08  11/18/03  Add _CHAR_ASCII/EBCDIC macros
 * 1.09  09/07/04  Add _I386 and _IA32 to Win32
 * 1.10  10/07/04  Changed to use x86-64 instead of AMD64
 * 1.11  09/30/05  - Use x86_64 instead of x86-64 (Linux)
 *                 - Use x64 instead of x86-64 (Windows)
 * 1.12  12/12/07  Add _ARCH_STR macro
 *************************************************************************
 */
#ifndef _H_PLATFORM_
#define _H_PLATFORM_

#define PLATFORM_H_VERSION   (1.12)



/*
 * Windows (Visual C++)
 * *******
 */
#if defined(_WIN32) || defined(WIN32)
   #if !defined(_WINDOWS)
      #define _WINDOWS
   #endif
   #if !defined(WINDOWS)
      #define WINDOWS
   #endif

   #if !defined(_LITTLEENDIAN)
      #define _LITTLEENDIAN
   #endif
   #if !defined(_LITTLE_ENDIAN)
      #define _LITTLE_ENDIAN
   #endif

   #if !defined(_CHAR_ASCII)
      #define _CHAR_ASCII
   #endif

   #if defined(_WIN64) || defined (WIN64)
      #if !defined(_64BIT)
         #define _64BIT
      #endif
      #if !defined(_WINDOWS64)
         #define _WINDOWS64
      #endif
      #if defined(_M_IA64) || defined(_IA64_)
         #define _PLATFORM_STR "Windows-IA64"
         #define _ARCH_STR "IA64"
         #if !defined(_IA64)
            #define _IA64
         #endif
      #elif defined(_M_AMD64) || defined(_AMD64_) || defined(_M_X64)
         #define _PLATFORM_STR "Windows-x64"
         #define _ARCH_STR "x64"
         #if !defined(_X86_64)
            #define _X86_64
         #endif
         #if !defined(_X64)
            #define _X64
         #endif
         #if !defined(_AMD64)
            #define _AMD64
         #endif
      #else
         #error ***** PLATFORM.H: Unknown 64-bit Windows architecture ****
      #endif
   #else
      #if !defined(_32BIT)
         #define _32BIT
      #endif
      #if !defined(_WINDOWS32)
         #define _WINDOWS32
      #endif
      #if defined (_M_IX86) || defined (_X86_)
         #define _PLATFORM_STR "Windows-x86"
         #define _ARCH_STR "x86"
         #if !defined(_X86)
            #define _X86
         #endif
         #if !defined(_I386)
            #define _I386
         #endif
         #if !defined(_IA32)
            #define _IA32
         #endif
      #else
         #error ***** PLATFORM.H: Unknown 32-bit Windows architecture ****
      #endif
   #endif

/*
 * Linux (using gcc)
 * *****
 */
#elif defined(linux) || defined(LINUX) || defined(_LINUX)
   #include <stdint.h>
   #include <endian.h>

   #if !defined(_LINUX)
      #define _LINUX
   #endif
   #if !defined(LINUX)
      #define LINUX
   #endif

   #if !defined(_CHAR_ASCII)
      #define _CHAR_ASCII
   #endif

   #if (__WORDSIZE == 32)
      #define _32BIT
      #if !defined(_LINUX32)
         #define _LINUX32
      #endif
   #elif (__WORDSIZE == 64)
      #define _64BIT
      #if !defined(_LINUX64)
         #define _LINUX64
      #endif
   #else
      #error ***** PLATFORM.H: Unknown __WORDSIZE value ****
   #endif

   #if (__BYTE_ORDER == __LITTLE_ENDIAN)
      #define _LITTLEENDIAN
      #if !defined(_LITTLE_ENDIAN)
         #define _LITTLE_ENDIAN
      #endif
   #elif (__BYTE_ORDER == __BIG_ENDIAN)
      #define _BIGENDIAN
      #if !defined(_BIG_ENDIAN)
         #define _BIG_ENDIAN
      #endif
   #else
      #error ***** PLATFORM.H: Unknown __BYTE_ORDER value ****
   #endif

   #if defined(__i386__) || defined(IA32)
      #define _PLATFORM_STR "Linux-x86"
      #define _ARCH_STR "x86"
      #if !defined(_X86)
         #define _X86
      #endif
      #if !defined(_I386)
         #define _I386
      #endif
      #if !defined(_IA32)
         #define _IA32
      #endif
   #elif defined(__ia64__) || defined(IA64)
      #define _PLATFORM_STR "Linux-IA64"
      #define _ARCH_STR "IA64"
      #if !defined(_IA64)
         #define _IA64
      #endif
   #elif defined(PPC32) || defined(PPC64) || defined(__powerpc__) || defined(__powerpc64__)
      #if !defined(_PPC)
         #define _PPC
      #endif
      #if (__WORDSIZE == 32)
         #define _PLATFORM_STR "Linux-PPC32"
         #define _ARCH_STR "PPC32"
         #if !defined(_PPC32)
            #define _PPC32
         #endif
      #else
         #define _PLATFORM_STR "Linux-PPC64"
         #define _ARCH_STR "PPC64"
         #if !defined(_PPC64)
            #define _PPC64
         #endif
      #endif
   #elif defined(__s390__) || defined(__s390x__)
      #if !defined(_S390)
         #define _S390
      #endif
      #if (__WORDSIZE == 32)
         #define _PLATFORM_STR "Linux-s390"
         #define _ARCH_STR "s390"
      #else
         #define _PLATFORM_STR "Linux-s390x"
         #define _ARCH_STR "s390x"
         #if !defined(_S390X)
            #define _S390X
         #endif
      #endif
   #elif defined(__x86_64__) || defined(X8664) || defined(X86_64) || defined(AMD64) || defined(AMD_64)
      #define _PLATFORM_STR "Linux-x86_64"
      #define _ARCH_STR "x86-64"
      #if !defined(_X86_64)
         #define _X86_64
      #endif
      #if !defined(_X64)
         #define _X64
      #endif
      #if !defined(_AMD64)
         #define _AMD64
      #endif
   #else
      #error ***** PLATFORM.H: Unknown architecture value ****
   #endif

/*
 * AIX
 * ***
 */
#elif defined(_AIX) || defined(AIX)
   #if !defined(_AIX)
      #define _AIX
   #endif
   #if !defined(AIX)
      #define AIX
   #endif

   #if !defined(_CHAR_ASCII)
      #define _CHAR_ASCII
   #endif

   #define _BIGENDIAN
   #if !defined(_BIG_ENDIAN)
      #define _BIG_ENDIAN
   #endif

   #if defined(_POWER)
      #if !defined(_PPC)
         #define _PPC
      #endif
      #if defined(__64BIT__)
         #define _64BIT
         #define _PLATFORM_STR "AIX-PPC64"
         #define _ARCH_STR "PPC64"
         #if !defined(_AIX64)
            #define _AIX64
         #endif
         #if !defined(_PPC64)
            #define _PPC64
         #endif
      #else
         #define _32BIT
         #define _PLATFORM_STR "AIX-PPC32"
         #define _ARCH_STR "PPC32"
         #if !defined(_AIX32)
            #define _AIX32
         #endif
         #if !defined(_PPC32)
            #define _PPC32
         #endif
      #endif
   #elif defined(_ia64) || defined(__ia64)
      #define _64BIT
      #define _PLATFORM_STR "AIX-IA64"
         #define _ARCH_STR "IA64"
      #if !defined(_AIX64)
         #define _AIX64
      #endif
      #if !defined(_IA64)
         #define _IA64
      #endif
   #else
      #error ***** PLATFORM.H: Unknown AIX architecture and/or wordsize combination ****
   #endif

/*
 * z/OS
 * ****
 */
#elif defined(__MVS__) || defined(MVS)
   #include <limits.h>
   #include <stddef.h>
   #include <inttypes.h>

   #define _S390

   #if !defined(_CHAR_EBCDIC)
      #define _CHAR_EBCDIC
   #endif

   #define _BIGENDIAN
   #if !defined(_BIG_ENDIAN)
      #define _BIG_ENDIAN
   #endif

   #if !defined(_ZOS)
      #define _ZOS
   #endif
   #if !defined(ZOS)
      #define ZOS
   #endif
   #if !defined(_MVS)
      #define _MVS
   #endif
   #if !defined(MVS)
      #define MVS
   #endif

   #if defined(_LP64)
      #define _ZOS64
      #define _MVS64
      #define _S390X
      #define _64BIT
      #define _PLATFORM_STR "z/OS-64"
      #define _ARCH_STR "s390z"
   #else
      #define _ZOS32
      #define _MVS32
      #define _32BIT
      #define _PLATFORM_STR "z/OS-31"
      #define _ARCH_STR "s390"
   #endif

/*
 * OS/2
 * ****
 */
#elif defined(__OS2DEF__) || defined(OS2)
   /* This needs work! */
   #define _32BIT
   #define _PLATFORM_STR "OS/2"
   #define _ARCH_STR "x86"

   #if !defined(_OS2)
      #define _OS2
   #endif
   #if !defined(_OS232)
      #define _OS232
   #endif
   #if !defined(_X86)
      #define _X86
   #endif

   #if !defined(_CHAR_ASCII)
      #define _CHAR_ASCII
   #endif

   #define _LITTLEENDIAN
   #if !defined(_LITTLE_ENDIAN)
      #define _LITTLE_ENDIAN
   #endif

/*
 * ?????
 * *****
 */
#else
   #error ***** PLATFORM.H: Unable to determine target OS ****
#endif

#endif /* _H_PLATFORM_  */
