//----------------------------------------------------------------------------
//
//   IBM Performance Inspector
//   Copyright (c) International Business Machines Corp., 2003 - 2007
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public License
//   along with this library; if not, write to the Free Software Foundation,
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//----------------------------------------------------------------------------

#ifndef _CORE_H_
   #define _CORE_H_

   #include "itypes.h"

   #if defined(_WINDOWS)
      #include <windows.h>
      #include <winbase.h>
      #include <conio.h>
      #include <sys\types.h>
      #include <sys\stat.h>
      #include <io.h>
      #include <limits.h>

      #define __OpSys 1
   #endif

   #if defined(_LINUX)
      #include <libgen.h>     // basename/dirname
      #include <sys/types.h>
      #include <sys/stat.h>
      #include <unistd.h>
      #include <stdint.h>
      #define __OpSys 2
   #endif

   #if defined(_AIX)
      #include <sys/types.h>
      #define __OpSys 3
   #endif

   #if defined(_MVS)
      #define __OpSys 4
   #endif

   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>
   #include <stdarg.h>
   #include <errno.h>
   #include <ctype.h>
   #include <fcntl.h>
   #include <time.h>

   #include <math.h>
   #include <assert.h>
   #include <memory.h>

#endif
