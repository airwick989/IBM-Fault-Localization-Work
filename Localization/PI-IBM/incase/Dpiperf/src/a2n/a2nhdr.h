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

#ifndef _A2NHDR_H_
#define _A2NHDR_H_

#include <itypes.h>

#if defined(_AIX)
   #include <sys/stat.h>
   #include <extension.h>
   #include <libgen.h>                      // basename/dirname
#endif
#if defined(_LINUX)
   #include <pthread.h>
   #include <libgen.h>                      // basename/dirname
   #include <sys/types.h>
   #include <sys/stat.h>
   #include <unistd.h>
   #include <stdint.h>
#ifndef _LEXTERN
#if defined(PI_STATIC_BFD)
   #include "../../binutils-2.15.97/include/bfd.h"
#else
   #include <bfd.h>
#endif   
#endif   
#endif
#if defined(_ZOS)
   #include <libgen.h>                      // basename/dirname
   #include <sys/types.h>
   #include <sys/stat.h>
   #include <unistd.h>
#endif
#if defined(_WINDOWS)
   #include <windows.h>
   #include <conio.h>
   #include <sys\types.h>
   #include <sys\stat.h>
   #include <io.h>
#endif

#define MAXLINE    1024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>

#include "a2n.h"                            // Exported/external stuff
#include "harvester.h"                      // Harvester stuff
#include "a2nint.h"                         // Internal stuff
#include "gv.h"                             // A2N globals
#include "util.h"                           // Utility stuff
#include "saveres.h"                        // Save/Restore stuff

#if defined(_AIX)
   #include "aixsyms.h"
#endif
#if defined(_LINUX)
   //#include "config.h"
   #include "linux/linuxsyms.h"
#endif
#if defined(_WINDOWS)
   #include "windowsyms.h"
#endif

#endif // _A2NHDR_H_
