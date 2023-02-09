/*
 *
 *  Copyright (c) IBM Corporation 1996.
 *  Property of IBM Corporation.
 *  All Rights Reserved.
 *
 *  Origin 30 (IBM)
 *
 */

/* ************************************************************ */
#include "global.h"

#include "utils/common.h"
/* ************************************************************ */

FILE * xopen(STRING fn, STRING opt)
{
   FILE * fh;

   OptVMsg( " Opening %s\n", fn );

   if ( 0 == (fh = fopen(fn, opt) ) )
   {
      printf(" Can't Open : %s(%s)\n", fn, opt);
      pexit(" xopen ");                 // Does not return
   }

   if ( 0 == gv.oldheader )
   {
      hdr(fh);
   }
   return(fh);
}

/* ************************************************************ */
/* in place tokenizing by replacing whitespace with nulls       */
char ** tokenize( char * str, int * pcnt )
{
   char c;
   int  n    = 0;                       // number of tokens found
   int  owsp = 1;                       // old whitespace
   int  nwsp;                           // new whitespace

   gv.asTokens[0] = str;         

   while ( n < gv.maxTokens
           && 0 != ( c = *str ) )
   {
      nwsp = ( ' ' == c ) || ( '\t' == c ) || ( '\n' == c ) || ( '\r' == c );

      if ( nwsp )                       // New character is whitespace
      {
         *str = '\0';                   // Change whitespace to null
      }
      else                              // new != whitespace
      {
         if ( owsp )                    // Changed from whitespace to non-whitespace 
         {
            gv.asTokens[n] = str;       // start of n-th token ( 0-based )
            n++;
         }
      }
      owsp = nwsp;
      str++;
   }
   gv.maxTokens = MAX_TOKENS;           // Restore max tokens to default value

   *pcnt = n;                           // Return number of tokens found

   return( gv.asTokens );               // Return pointer to array of Token strings
}

/* ************************************************************ */
int ptrace(STRING name)
{
   fprintf(stdout, "<< %s >> \n", name);
   OptVMsg("<< %s >> \n", name);
   fflush(stdout);
   return(0);
}

/* ************************************************************ */
void panic(STRING why)
{
   fprintf(stderr, "%s\n", why);
   exit(-1);
}

/* **************************************
 * malloc and copy a string value
 * ************************************** */
char * Remember( char * s )
{
   return( xStrdup( "Remember", s ) );
}


/* ************************************************************ */
STRING printrep(unsigned char c)
{
   static unsigned char pr[8];

   if (c < 127)
   {
      if (c < 32)
      {                                 /* control chars */
         pr[0] = '^';
         pr[1] = (unsigned char)(c + 64);
         pr[2] = '\0';
      }
      else
      {                                 /* printable chars */
         pr[0] = c;
         pr[1] = '\0';
      }
   }
   else
   {
      if (c == 127) return("<del>");
      else
      {
         /* if(c <= \0377) { */         /* 128-255. Octal \ooo */
         if (c < 0xff)
         {                              /* 128-255. Octal \ooo */
            pr[0] = '\\';
            pr[3] = (unsigned char)('0' + (c & 7));
            c     = (unsigned char)((int)c >> 3);
            pr[2] = (unsigned char)('0' + (c & 7));
            c     = (unsigned char)((int)c >> 3);
            pr[1] = (unsigned char)('0' + (c & 3));
            pr[4] = '\0';
         }
         else
         {                              /* > 256 ??? */
            (void) sprintf((char *)pr, "0x%04x", (int)c);
         }
      }
   }
   return((char *)pr);
}

/* ************************************************************ */
void pexit (char *s)
{
   fprintf(stderr, " errno %d\n", errno);
   perror(s);
   exit(1);
}

/******************************/
void err_exit(char * why)
{
   if ( why )
   {
      ErrVMsgLog("\nARCF EXITING <%s>\n", why);
   }
   else
   {
      ErrVMsgLog("\nARCF EXITING\n");
   }
   exit(0);
}

void writeStderr(char * msg)            // Allow hooking of stderr
{
   fprintf(stderr, "%s", msg); 
}

void writeStdout(char * msg)            // Allow hooking of stdout
{
   fprintf(stdout, "%s", msg); 
}
