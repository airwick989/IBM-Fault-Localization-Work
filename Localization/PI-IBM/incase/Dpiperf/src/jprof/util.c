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


//
// All prototypes in jprof.h
//

#include "version.h"
#include "jprof.h"
#include "tree.h"

#if defined (_LINUX)
   #include <sys/utsname.h>
#endif

#include "utils/common.h"
#include "utils/pi_time.h"

//
// get_current_time()
// ******************
//
// Return current date and time
//
void get_current_time( int * hh,
                       int * mm,
                       int * ss,
                       int * YY,
                       int * MM,
                       int * DD )
{
   time_t      t;
   struct tm * ptm;

   time(&t);

   ptm = localtime(&t);

   if (hh)
   {
      *hh = ptm->tm_hour;
   }
   if (mm)
   {
      *mm = ptm->tm_min;
   }
   if (ss)
   {
      *ss = ptm->tm_sec;
   }
   if (YY)
   {
      *YY = ptm->tm_year + 1900;
   }
   if (MM)
   {
      *MM = ptm->tm_mon + 1;
   }
   if (DD)
   {
      *DD = ptm->tm_mday;
   }

   return;
}


//
// read_cycles_on_processor()
// **************************
//
UINT64 read_cycles_on_processor(int cpu)
{
   UINT64 cycles;

   GetCycles((void *)&cycles);

   return(cycles);
}

/******************************/
char * dupJavaStr(char * jnm)
{
   char * cp;

   if ( jnm )
   {
      cp = xStrdup("dupJavaStr",jnm);

      Force2Native(cp);
   }
   else
   {
      cp = "NULL";
   }
   return(cp);
}

/******************************/
char * tmpJavaStr(char * jnm, char * buf)   // buf must not be null
{
   char * cp;

   if ( NULL != jnm )
   {
      cp = strcpy(buf, jnm);

      Force2Native(cp);
   }
   else
   {
      cp = "NULL";
   }
   return(cp);
}

#if defined(_CHAR_EBCDIC)

/******************************/
void Force2Java(char * str)
{
   char *p = str;

   if ( NULL != str )
   {
      while ( 0 != *p )
      {
         if ( 0 != ( 0x80 & *p ) )      // Non-ASCII character found
         {
            p = NULL;
            break;
         }
         p++;
      }
      if ( NULL == p )                  // String not ASCII
      {
         __etoa(str);                   // Convert to ASCII
      }
   }
}

/******************************/
void Force2Native(char * str)
{
   char *p = str;

   if ( NULL != str )
   {
      while ( 0 != *p )
      {
         if ( 0 != ( 0x80 & *p ) )      // Non-ASCII character found
         {
            p = NULL;
            break;
         }
         p++;
      }
      if ( NULL != p )                  // String probably ASCII
      {
         __atoe(str);                   // Convert to EBCDIC
      }
   }
}
/******************************/
void Java2Native(char * str)
{
   if ( NULL != str )
   {
      __atoe(str);
   }
}

/******************************/
void Native2Java(char * str)
{
   if ( NULL != str )
   {
      __etoa(str);
   }
}
#endif

/******************************/
char * getJprofPath()
{
#if defined(_WINDOWS)
   char path[MAX_PATH];

   if ( GetModuleFileName( GetModuleHandle("jprof.dll"), path, MAX_PATH ))
   {
      return( xStrdup("Jprof Path", path) );
   }
#endif
   return(NULL);
}

/******************************/
char * get_platform_info(void)
{
#if defined(_WINDOWS) || defined(_LINUX)

// Builds string kinda like this: ", vM.m.b-spx, PERFDD=vx.x.xx, PerfUtil=vx.x.xx, CPUS=4, HTT=1, MC=1, Sig=0xffffffff"

   #if defined(_WINDOWS)
   OSVERSIONINFO osvi;
   #else // _LINUX
   struct utsname ubuf;
   #endif
   UINT32 u;
   char   temp[128];
   char   buf[2048];
   char * info;

   info    = buf;
   info[0] = 0;

   #if defined(_WINDOWS)
      #if defined(_32BIT)
   if (IsProcessor64Bits())
   {
      // Running 32-bit dll on a 64-bit system. Indicate it.
      strcat(info, " (on x64)");
   }
      #endif

   ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
   if (GetVersionEx(&osvi))
   {
      // OS=vMajor.minor.build_number-SP
      sprintf(temp, ", OS=v%d.%d.%d-%s",
              osvi.dwMajorVersion, osvi.dwMinorVersion,
              (osvi.dwBuildNumber & 0xFFFF), osvi.szCSDVersion);
      strcat(info, temp);
   }

   #else  // _LINUX

   if (uname(&ubuf) != 0)
   {
      return(0);
   }

   snprintf(temp, 64, ", OS=%s", ubuf.release);
   strcat(info, temp);

   #endif

   sprintf(temp, ", PerfUtil=v%d.%d.%d", V_MAJOR, V_MINOR, V_REV);
   strcat(info, temp);


#if defined (_WINDOWS)
   sprintf(temp, ", PERFDD=v%d.%d.%d", (u >> 24), ((u & 0x00FF0000) >> 16), (u & 0x0000FFFF));
#else
   sprintf(temp, ", PERFDD=v%08X", u);
#endif
   strcat(info, temp);

   sprintf(temp, ", CPUS=%d", PiGetActiveProcessorCount());
   strcat(info, temp);

   sprintf(temp, ", HT=%d", IsHyperThreadingEnabled());
   strcat(info, temp);

   sprintf(temp, ", MC=%d", IsProcessorMultiCore());
   strcat(info, temp);

   sprintf(temp, ", CpuSig=0x%08X", GetProcessorSignature());
   strcat(info, temp);

#if defined(HAVE_IsTscInvariant)

   strcat( info,
	  	  IsTscInvariant()
		  ? ", Invariant_TSC"
		  : ", Variable_TSC" );

#endif

   sprintf(temp, ", %s", GetProcessorManufacturerString());
   strcat(info, temp);

   if ( info[0] )
   {
      info = xStrdup( "Platform Info", info );
   }
   else
   {
      info = 0;
   }
   return(info);

#else

   return(0);

#endif
}

/******************************/
FILE * OpenJprofFile( char *filename, char *filemode, char *whyExit )
{
   FILE *file;

   if ( 'r' == *filemode )
   {
      ErrVMsgLog("\n Reading %s\n", filename);
   }
   else
   {
      ErrVMsgLog("\n Writing %s\n", filename);
   }

   if ( ( file = fopen( filename, filemode ) ) == NULL)
   {
      if ( 0 != whyExit )
      {
         gc.verbose |= gc_verbose_maximum;   // Max verbosity on error exit

         ErrVMsgLog("ERROR opening %s. errno = %d (%s)\n", filename, errno, strerror(errno));

         err_exit( whyExit );
         exit(-99);
      }
   }
   return( file );
}

/******************************/
FILE * file_open( char * ext, int cnt )
{
   FILE * fd;

   if ( 0 == gv->tempfn[0] )
   {
      if ( cnt )
      {
         sprintf(gv->tempfn, "%s-%s.%d%s%s",
                 ge.fnm,
                 ext,
                 cnt,
                 ge.pidext,
                 ge.logSuffix );
      }
      else
      {
         sprintf(gv->tempfn, "%s-%s%s%s",
                 ge.fnm,
                 ext,
                 ge.pidext,
                 ge.logSuffix );
      }
   }
   fd = OpenJprofFile(gv->tempfn, "w+b", gv->tempfn);

   gv->tempfn[0] = 0;                   // One-time use of temp filename

   WriteFlush(fd, gv->version);
   WriteFlush(fd, gv->loadtime);
   WriteFlush(fd, " JPROF Options: %s\n\n", gv->options);

   return(fd);
}

//----------------------------------------------------------------------
// Parse a list of period delimited integers
//----------------------------------------------------------------------
int parseIntList( char *str, int *array, int limit )
{
   int    num = 0;
   char * p;

   while ( 0 != *str && num < limit )
   {
      p = str;

      while ( 0 != *p && '.' != *p )
      {
         p++;
      }
      if ( 0 == *p )
      {
         p[1] = 0;                      // Mark end of string with double 0
      }
      *p++ = 0;                         // Mark end of this int

      array[ num++ ] = atoi( str );

      str = p;
   }
   if ( 0 != *str )
   {
      err_exit( "Too many integers in list" );
   }
   return( num );
}

//======================================================================
// nxShowList - Show the entries in a Selection List
//======================================================================

void nxShowList( char * selList )       // Thread safe
{
   char * p;
   char * q;
   UINT32 hdrSelEntry;

   if ( selList )
   {
      ErrVMsgLog( "\nSelection List:\n\n" );

      p  = selList;

      q  = p + *(UINT32 *)p;            // End of list

      p += sizeof(UINT32);              // Address of first entry

      while ( p < q )                   // More entries to go
      {
         hdrSelEntry = *(UINT32 *)p;

         ErrVMsgLog( "%s%s%s\n",
                     ( SELLIST_FLAG_INCLUDE & hdrSelEntry ) ? "+" : "-",
                     p+4,
                     ( SELLIST_FLAG_PREFIX  & hdrSelEntry ) ? "*" : "" );

         p += ( ( SELLIST_MASK_LENGTH & hdrSelEntry ) + 8 ) & ~0x03;
      }
   }
}

//======================================================================
// nxGetString - get Inclusive/Exclusive string and modifier from list
//======================================================================

char * nxGetString()                    // Not thread safe, only used during init
{
   char * p;
   char * nxString;

   if ( gc.posNeg )
   {
      gv->nxInclExcl = ( gc.posNeg > 0 ) ? '+' : '-';
      gc.posNeg      = 0;
   }
   else
   {
      gv->nxInclExcl = gv->nxSeparator; // Use old trailing separator as Incl/Excl indicator
   }
   gv->nxModifier = "";
   nxString      = "";                  // Always return a string pointer
   p             = gv->nxList;

   if ( *p )
   {
      if ( '+' == *p || '-' == *p )
      {
         gv->nxInclExcl = *p++;         // Handle a leading + or -
      }
      nxString = p;

      while ( *p                        // Find list separator
              && '+' != *p
              && '-' != *p
              && '(' != *p )
      {
         p++;
      }

      if ( '(' == *p )                  // Separator will appear after ')'
      {
         *p++ = 0;                      // Remove leading '(' and end nxString

         gv->nxModifier = p;

         while ( *p                     // Find end of modifier
                 && ')' != *p )
         {
            p++;
         }

         if ( 0 == *p )
         {
            err_exit( "Invalid modifier in Inclusive/Exclusive list\n" );
         }

         *p++ = 0;                      // Remove trailing ')' and end nxModifier

         if ( 0 != *p                   // Not end of list entry
              && '+' != *p
              && '-' != *p )
         {
            err_exit( "Invalid modifier in Inclusive/Exclusive list\n" );
         }
      }
   }
   gv->nxSeparator = *p;                // Save the trailing separator

   if ( *p )
   {
      *p++ = 0;                         // Remove the separator
   }
   gv->nxList      = p;                 // Update the list pointer

   return( nxString );
}

//======================================================================
// nxCheckString - check state of string vs Inclusive/Exclusive List
//======================================================================

int nxCheckString( char * selList, char * str )   // Thread safe
{
   char * p;
   char * q;
   int    n;
   int    selected = 1;                 // Default is selected if list is null
   int    inclusive;                    // Entry is inclusive
   int    hdrSelEntry;
   int    strLength;

   if ( selList && str )
   {
      p  = selList;

      q  = p + *(UINT32 *)p;            // End of list

      p += sizeof(UINT32);              // Address of first entry

      if ( p < q
           && ( SELLIST_FLAG_INCLUDE & *(UINT32 *)p ) )
      {
         selected = 0;                  // Default is unselected if first entry is INCLUSIVE
      }
      strLength = (int)strlen(str);

      while ( p < q )                   // More entries to go
      {
         hdrSelEntry = *(UINT32 *)p;

         p += sizeof(UINT32);           // Address of string in entry

         inclusive = ( SELLIST_FLAG_INCLUDE & hdrSelEntry ) ? 1 : 0;   // Convert flag to 0 or 1

         if ( inclusive != selected )   // State can not change if inclusive == selected
         {
            if ( SELLIST_FLAG_PREFIX & hdrSelEntry )   // String in entry is a prefix
            {
               n = SELLIST_MASK_LENGTH & hdrSelEntry;   // Length of string in entry

               if ( strLength >= n )    // String must be at least the length of the prefix to match
               {
                  if ( 0 == strncmp( str, p, n ) )   // String begins with prefix
                  {
                     selected = inclusive;   // Selected flag becomes same as inclusive/exclusive flag of entry
                  }
               }
            }
            else
            {
               if ( 0 == strcmp( str, p ) )   // Strings match
               {
                  selected = inclusive; // Selected flag becomes same as inclusive/exclusive flag of entry
               }
            }
         }
         p += ( ( SELLIST_MASK_LENGTH & hdrSelEntry ) + 4 ) & ~0x03;
      }
   }
   return( selected );
}

//======================================================================
// nxAddEntries - add Inclusive/Exclusive entries to a Selection List
//======================================================================

void nxAddEntries( char ** pSelList, char * str )   // Not thread safe, only used during init
{
   char * p;
   char * nxString;
   int    inclusive;

   UINT32 hdrSelEntry;

   UINT32 sizeEntry;
   UINT32 sizeList;
   UINT32 sizeString;

   gv->nxList = str;

   if ( 0 == *pSelList )
   {
      *pSelList = xMalloc( "selList", sizeof(UINT32) );   // Allocate an empty Selection List

      **(UINT32 **)pSelList = sizeof(UINT32);   // Indicate only trailing null entry
   }

   for (;;)
   {
      nxString   = nxGetString();       // Extract one string from the input
      sizeString = (UINT32)strlen( nxString );

      if ( 0 == sizeString )
      {
         break;
      }

      hdrSelEntry = 0;
      inclusive   = 0;

      if ( '-' != gv->nxInclExcl )      // Not an exclusive entry
      {
         hdrSelEntry |= SELLIST_FLAG_INCLUDE;
         inclusive    = 1;
      }
      if ( '*' == nxString[ sizeString-1 ] )
      {
         hdrSelEntry |= SELLIST_FLAG_PREFIX;

         nxString[ --sizeString ] = 0;  // Remove '*' from end of the string
      }
      if ( *gv->nxModifier )
      {
         hdrSelEntry |= SELLIST_FLAG_SUFFIX;

         // More needed here
      }
      hdrSelEntry |= sizeString;

      // Perform minimal entry validation

      sizeList = **(UINT32 **)pSelList;

      if ( sizeList > sizeof(UINT32) )  // First entry can always be added
      {
         if ( inclusive == nxCheckString( *pSelList, nxString ) )   // New string already included or excluded
         {
            nxShowList( *pSelList );

            ErrVMsgLog( "\n%s%s%s is redundant\n",
                        ( SELLIST_FLAG_INCLUDE & hdrSelEntry ) ? "+" : "-",
                        nxString,
                        ( SELLIST_FLAG_PREFIX  & hdrSelEntry ) ? "*" : "" );
            err_exit( 0 );
         }
      }

      // Add the new entry to the list

      sizeEntry = ( sizeString + 8 ) & ~0x03;   // Assumes sizeof(UINT32) == 4 and includes trailing zero

      *pSelList = xRealloc( "selList",
                            sizeList + sizeEntry,
                            *pSelList,
                            sizeList );

      p = *pSelList + sizeList;         // Point to old end of list

      *(UINT32 *)p = hdrSelEntry;       // Copy entry header to list

      strcpy( p + sizeof(UINT32), nxString );   // Copy entry string to list

      **(UINT32 **)pSelList = sizeList + sizeEntry;   // Update the length of the list
   }
}

//======================================================================
// Common routines to read Call Stacks and gather statistics
//======================================================================

UINT64 CSStatsProlog()
{
   UINT64 timeStamp = 0;

   GetCycles((void *)&timeStamp);

   return( timeStamp );
}

void CSStatsEpilog( UINT64 csTimeStamp, int numFrames )
{
   UINT64 timeStamp = 0;

   if ( numFrames <= MAX_CALL_STACK_COUNT )
   {
      GetCycles((void *)&timeStamp);

      timeStamp -= csTimeStamp;

      if ( 0 == gv->CallStackCountsByFrames )
      {
         gv->CallStackCountsByFrames = (UINT32 *)zMalloc( "CallStack_Stats", ( MAX_CALL_STACK_COUNT + 1 ) * sizeof(UINT32) );
      }
      if ( 0 == gv->CallStackCyclesByFrames )
      {
         gv->CallStackCyclesByFrames = (UINT64 *)zMalloc( "CallStack_Stats", ( MAX_CALL_STACK_COUNT + 1 ) * sizeof(UINT64) );
      }
      if ( numFrames <= MAX_CALL_STACK_COUNT )
      {
         gv->CallStackCountsByFrames[numFrames]++;   // These are not thread safe on SMP
         gv->CallStackCyclesByFrames[numFrames] += timeStamp;
      }
   }
}

void allocEnExCs( thread_t * tp )
{
   tp->max_frames += 16;

   tp->frame_buffer = xRealloc( "tp->frame_buffer:EnExCs",
                                tp->max_frames * sizeof( jmethodID ),
                                tp->frame_buffer,   // Creates new buffer, if zero
                                ( tp->max_frames - 16 ) * sizeof( jmethodID ) );
}

//----------------------------------------------------------------------
// Get the call stack for the specified thread
//----------------------------------------------------------------------

int get_callstack( thread_t * tp, thread_t * self_tp )
{
   int             flags;
   int             size;
   int             fShrinkStack = 0;
   jthread         thread;
   jint            n;

   MVMsg(gc_mask_Show_Stack_Traces,(">> get_callstack for %s\n", tp->thread_name));

   if ( ge.EnExCs )
   {
      if ( tp->num_frames >= tp->max_frames )
      {
         allocEnExCs( tp );
      }
   }
   else
   {
      thread = 0;                       // Get stack trace for self

      if ( tp != self_tp && ge.jvmti )
      {
         thread = (*self_tp->env_id)->NewLocalRef(self_tp->env_id, tp->thread);
      }

      tp->num_frames = 0;

      if ( tp->frame_buffer == NULL )
      {
         if ( ge.jvmti )
         {
            (*ge.jvmti)->GetFrameCount( ge.jvmti,
                                        thread,
                                        &n );

            tp->max_frames = ( 16 + n ) & ~15;
         }
         else
         {
            tp->max_frames = 2048;
            fShrinkStack   = 1;
         }
         tp->frame_buffer = xMalloc( "tp->frame_buffer",
                                     tp->max_frames * sizeof( jvmtiFrameInfoExtended ) );
      }

      for (;;)
      {
         if ( tp->thread
              && ( tp == self_tp
                   || self_tp->env_id ) )
         {
            if ( tp == self_tp || thread )
            {
               if ( ge.getStackTraceExtended )
               {
#if defined(_WINDOWS)
                  ;                                                  debug_trace_hook((0x17, 0x13, 2, 0, tp, 0));
                  __try
                  {
#endif
                     flags = COM_IBM_GET_STACK_TRACE_EXTRA_FRAME_INFO
                             + (( gv->prunecallstacks ) ? COM_IBM_GET_STACK_TRACE_PRUNE_UNREPORTED_METHODS : 0 );

                     csStatsProlog();
                     (ge.getStackTraceExtended)(ge.jvmti,
                                                flags,
                                                thread,   // New local reference or 0 (self)
                                                0,
                                                tp->max_frames,
                                                (void *)tp->frame_buffer,
                                                &tp->num_frames);
                     csStatsEpilog(tp->num_frames);

#if defined(_WINDOWS)
                  }
                  __except (EXCEPTION_EXECUTE_HANDLER)   // This is usually fatal
                  {
                     ErrVMsgLog("**ERROR** Exception 0x%08X getting Ext JVMTI call stack for %s\n",
                                GetExceptionCode(), tp->thread_name);
                     tp->num_frames = 0;
                  }
                  ;                                                  debug_trace_hook((0x17, 0x14, 2, 0, tp, 0));
#endif
               }
               else
               {

#if defined(_WINDOWS)
                  ;                                                  debug_trace_hook((0x17, 0x13, 2, 0, tp, 0));
                  __try
                  {
#endif

                     csStatsProlog();
                     (*ge.jvmti)->GetStackTrace(ge.jvmti,
                                                thread,
                                                0,
                                                tp->max_frames,
                                                (jvmtiFrameInfo *)tp->frame_buffer,
                                                &tp->num_frames);
                     csStatsEpilog(tp->num_frames);

#if defined(_WINDOWS)
                  }
                  __except (EXCEPTION_EXECUTE_HANDLER)   // This is usually fatal
                  {
                     ErrVMsgLog("**ERROR** Exception 0x%08X getting JVMTI call stack for %s\n",
                                GetExceptionCode(), tp->thread_name);
                     tp->num_frames = 0;
                  }
                  ;                                                  debug_trace_hook((0x17, 0x14, 2, 0, tp, 0));
#endif
               }
            }
            else
            {
               thread     = tp->thread;
               tp->thread = 0;
            }
         }

         gv->scs_total_stacks++;
         gv->scs_total_frames += tp->num_frames;

         if ( tp->num_frames < tp->max_frames )
         {
            break;
         }

         size               = tp->max_frames * sizeof( jvmtiFrameInfoExtended );

         if ( ge.jvmti )
         {
            (*ge.jvmti)->GetFrameCount( ge.jvmti,
                                        thread,
                                        &n );

            tp->max_frames  = ( 16 + n ) & ~15;
         }
         else
         {
            tp->max_frames += 16;
         }

         tp->frame_buffer   = xRealloc( "tp->frame_buffer",
                                        tp->max_frames * sizeof( jvmtiFrameInfoExtended ),
                                        tp->frame_buffer,
                                        size );
         fShrinkStack = 0;
      }

      if ( fShrinkStack )               // Need to shrink the initial stack
      {
         size             = tp->max_frames * sizeof( jvmtiFrameInfoExtended );

         tp->max_frames   = ( 16 + tp->num_frames ) & ~15;

         tp->frame_buffer = xRealloc( "tp->frame_buffer",
                                      tp->max_frames * sizeof( jvmtiFrameInfoExtended ),
                                      tp->frame_buffer,
                                      size );
      }
      if ( gv->max_max_frames < tp->max_frames )
      {
         gv->max_max_frames = tp->max_frames;
      }

      if ( thread && self_tp->env_id )
      {
         (*self_tp->env_id)->DeleteLocalRef(self_tp->env_id, thread);
      }
   }
   MVMsg(gc_mask_Show_Stack_Traces,("<< get_callstack for %s\n", tp->thread_name));

   return( tp->num_frames );
}

//----------------------------------------------------------------------
// Push the call stack from the specified thread into the trees
//----------------------------------------------------------------------

pNode push_callstack( thread_t * tp )
{
   int       i;
   FrameInfo fi;
   pNode     np = tp->currT;            // Start at the thread node

   if ( tp->num_frames )
   {
      MVMsg( gc_mask_CallStack_Stats,
             ( "push_callstack: buffer = %p, frames = %d, thread = %s\n",
               tp->frame_buffer,
               tp->num_frames,
               tp->thread_name ));

      //   FIX ME:  Need to prevent buffer free while walking buffer

      for ( i = tp->num_frames - 1; i >= 0; i-- )
      {
#ifndef _LEXTERN
         if (ge.scs)
         {
            fi.type = gv_type_scs_Method;
         }
         else
#endif
         {
            fi.type = gv_type_Other;
         }

         getFrameInfo( tp->frame_buffer,
                       tp->num_frames,
                       i,
                       &fi );

         np = push( np, &fi );
      }
   }
   tp->currm = np;

   return( np );
}

/******************************/
void err_exit(char * why)
{
   if ( why )
   {
      ErrVMsgLog("\nJPROF EXITING <%s>\n", why);
   }
   else
   {
      ErrVMsgLog("\nJPROF EXITING\n");
   }

   logAllocStats();

   LogAllHashStats();

   gc.ShuttingDown = 1;                 // Don't attempt normal shutdown

   dumpStack(0);                        // Attempt to dump the JPROF callstack

   exit(0);
}

//----------------------------------------------------------------------
// Return the line number given a method and location
// Supported only for JVMTI with ExtendedCallStacks option
//----------------------------------------------------------------------

jint get_linenumber( jmethodID method, jlocation location )
{
   jint lineTableSize;
   jvmtiLineNumberEntry *lineTable;
   jint line_number = 0;                // zero indicates unknown

   if ( (*ge.jvmti)->GetLineNumberTable(ge.jvmti, method, &lineTableSize, &lineTable) == JVMTI_ERROR_NONE )
   {
      jint i;
      jint lineIndex = -1;

      for (i = 0; i < lineTableSize; ++i)
      {
         if (location < lineTable[i].start_location)
            break;
         lineIndex = i;
      }
      if (lineIndex != -1)
         line_number = lineTable[lineIndex].line_number;
      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)lineTable);
   }
   return( line_number );
}

//----------------------------------------------------------------------
// Return the source file name given a class
// Supported only for JVMTI with ExtendedCallStacks option
//----------------------------------------------------------------------

char * get_sourcefilename( jclass clazz )
{
   char * source_name = NULL;
   char * fnm;
   jint   rc;

   rc = (*ge.jvmti)->GetSourceFileName(ge.jvmti, clazz, &fnm);

   if (rc == JVMTI_ERROR_NONE )
   {
      source_name = xStrdup("SourceFile", fnm);

      (*ge.jvmti)->Deallocate(ge.jvmti, (unsigned char *)fnm);
   }
   else if ( rc != JVMTI_ERROR_ABSENT_INFORMATION )
   {
      ErrVMsgLog("GetSourceFileName failed: rc = %d\n", rc );
   }
   return( source_name );
}

//----------------------------------------------------------------------
// Extract and return information from a frame_buffer
// fi->linenum is referenced only for ge.jvmti && ge.ExtendedCallStacks
//----------------------------------------------------------------------

void getFrameInfo( void                 * frame_buffer,
                   int                    num_frames,
                   int                    fb_index,
                   FrameInfo            * fi )
{
   fi->linenum = 0;

   if ( ge.EnExCs )
   {
      fi->ptr  = ((jmethodID *)frame_buffer)[num_frames - fb_index - 1];   // Reverse order
   }
   else
   {
      if ( ge.getStackTraceExtended )
      {
         fi->ptr = ((jvmtiFrameInfoExtended *)frame_buffer)[fb_index].method;

         if ( COM_IBM_STACK_FRAME_EXTENDED_JITTED
              == ((jvmtiFrameInfoExtended *)frame_buffer)[fb_index].type )
         {
            fi->type = gv_type_Jitted;
         }
         else if ( -1
                   == ((jvmtiFrameInfoExtended *)frame_buffer)[fb_index].location )
         {
            fi->type = gv_type_Native;
         }
         else
         {
            fi->type = gv_type_Interp;
         }
         if ( ge.ExtendedCallStacks )
         {
            if ( fi->type != gv_type_Native )
            {
               fi->linenum = get_linenumber( fi->ptr, ((jvmtiFrameInfoExtended *)frame_buffer)[fb_index].location );
            }
         }
      }
      else
      {
         fi->ptr = ((jvmtiFrameInfo *)frame_buffer)[fb_index].method;

         if ( ge.ExtendedCallStacks )
         {
            fi->linenum = get_linenumber( fi->ptr, ((jvmtiFrameInfo *)frame_buffer)[fb_index].location );
         }
      }

      fi->ptr = insert_method( fi->ptr, 0 );   // Transform mid to mp
   }
}

/******************************/
void dump_callstack( thread_t * tp )
{
   int       i;
   char    * c;
   char    * m;
   FrameInfo fi = {0};

   int       num_frames = tp->num_frames;

   if ( num_frames )
   {
      WriteFlush(gc.msg, "####### Begin callstack for %s\n", tp->thread_name);

      if ( num_frames > 25 )
      {
         num_frames = 25;
      }

      for ( i = num_frames - 1; i >= 0; i-- )
      {
         char * srcName = NULL;

         getFrameInfo( tp->frame_buffer,
                       tp->num_frames,
                       i,
                       &fi );

         getMethodName(fi.ptr, &c, &m);

         if ( ge.ExtendedCallStacks )
         {
            method_t * mp = lookup_method( fi.ptr, 0 );   // try to get a source file name

            if (mp && mp->class_entry && mp->class_entry->source_name)
            {
               srcName = mp->class_entry->source_name;
            }
         }

         if ( srcName )
         {
            if ( fi.linenum )
            {
               WriteFlush(gc.msg, "%2d tid=%8x mid=%p <%s.%s@%s:%d>\n",
                          i, tp->tid, fi.ptr, c, m, srcName, fi.linenum);
            }
            else
            {
               WriteFlush(gc.msg, "%2d tid=%8x mid=%p <%s.%s@%s>\n",
                          i, tp->tid, fi.ptr, c, m, srcName);
            }
         }
         else
         {
            WriteFlush(gc.msg, "%2d tid=%8x mid=%p <%s.%s>\n",
                       i, tp->tid, fi.ptr, c, m);
         }
      }
      WriteFlush(gc.msg, "####### End callstack for %s\n", tp->thread_name);
   }
   return;
}

//----------------------------------------------------------------------
// Get Thread Call Stack and push it into the trees
//----------------------------------------------------------------------

int pushThreadCallStack( thread_t * tp, thread_t * self_tp, jmethodID mid )
{
   int       num_frames = 0;
   pNode     np;

// OVMsg(( ">> pushThreadCallStack\n" ));

   if ( tp->mt3 == gv_type_Compiling
        || ( ( gv->calltree || gv->calltrace )
             && 0 == gv->rton ) )
   {
      return( 0 );                      // Return 0 frames
   }

   // FIX ME:  Use the recorded call stack, if available
   num_frames = get_callstack(tp, self_tp);   // tp->frame_buffer has the stack frames

   if ( num_frames )
   {
      np = push_callstack( tp );        // Push the callstack into the trees
   }

#if defined(_WINDOWS) || defined(_LINUX)
   debug_trace_hook((0x17, 0x15, 2, 0, tp, num_frames));
#endif

   // Free the frame buffer, if we think we rarely need it
   if ( gv->deallocFrameBuffer )
   {
      freeFrameBuffer(tp);
   }

// OVMsg(( "<< pushThreadCallStack\n" ));

   return( num_frames );
}

void logFailure(int rc)
{
   ErrVMsgLog( "Parsing of JPROF options failed, rc = %d\n", rc);

   if ( 0 == gc.msg )
   {
      ErrVMsgLog("\n Writing log-failure\n");
      gc.msg = fopen( "log-failure", "w+b" );
   }
   if ( gc.msg )
   {
      writeDelayedMsgs();
   }
}

//======================================================================
// Read the current timeStamp
//======================================================================

thread_t * readTimeStamp( int tid )
{
   UINT64      timeStamp;
   thread_t  * tp;

   timeStamp = get_timestamp();

   if ( 0 == tid )
   {
      tid = CurrentThreadID();
   }
   tp = insert_thread(tid);

   if (tp) {

      tp->cpuNum    = 0;

      if ( gv->NoMetrics )
      {
         tp->timeStamp = gv->timeStamp++;  // Not thread safe, but does not matter
      }
      else
      {
         tp->timeStamp = timeStamp;
      }
   }

   return( tp );
}
