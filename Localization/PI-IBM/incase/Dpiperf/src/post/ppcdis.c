/*   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2007
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/****************************************************
 * Function Name: i386_instruction
 *
 * Description:   Wrapper for ppc disasm
 ****************************************************/

#include <string.h>
#include "itypes.h"

#if defined(USE_LIBOPCODES)
   #include "dis-asm.h"

unsigned long disassemble( char *target, char *insbuf, bfd_vma ip, disassembler_ftype func );
#endif

char OPNM[32];                          /* global inst type in ascii */

/************************/
char * i386_instruction( char * data,   /* instruction    */
                         uint64 addr,   /* address/offset */   //STJ64
                         int  * size,
                         char * buf,
                         int    b32 )   /* 32(1) or 16(0) bit segment */
{
   char * src;
   char * dst;

#if defined(USE_LIBOPCODES)

   *size = disassemble( buf, data, (bfd_vma)addr, print_insn_big_powerpc );

#else

   unsigned long opcode = *((unsigned long *)data);

   if ( 0x4C000020 == ( opcode & 0xFC0007FE ) )   // Includes bclrl (RETURN-CALL)
   {
      strcpy( buf, "bclr" );            // Typical RETURN instruction
   }
   else if ( 0x44000001 == ( opcode & 0xF4000001 ) )
   {
      strcpy( buf, "bcl" );             // Typical CALL instruction
   }
   else if ( 0x4C000421 == ( opcode & 0xFC0007FF ) )
   {
      strcpy( buf, "bcctrl" );          // Typical CALL instruction
   }
   else
   {
      strcpy( buf, "other" );
   }
   *size = 4;

#endif

   src = buf;
   dst = OPNM;

   while ( *src && ' ' != *src && '\t' != *src )
   {
      *dst++ = *src++;
   }
   *dst = 0;

   return(OPNM);
}

/************************/
/* Get Instruction Type
 ************************/

// return from inst_type call
#define INTRPT 0
#define CALL   1
#define RETURN 2
#define JUMP   3
#define IRET   4
#define OTHER  5
#define UNDEFD 6
#define ALLOC  7

int inst_type(char * opcode)            // return CALL RETURN JUMP IRET or OTHER
{
   int oc  = OTHER;
   int len = strlen( opcode );          // Size of this opcode string

   if ( 'b' == opcode[0]  )             // bx = Branch instruction
   {
      oc = JUMP;

      if ( 'l' == opcode[ len-1 ] )
      {
         oc = CALL;                     // bxl = Branch and Link instruction
      }
      else if ( len > 2
                && 'l' == opcode[ len-2 ]
                && 'r' == opcode[ len-1 ] )
      {
         oc = RETURN;                   // bxlr = Branch to Link Register instruction
      }
   }
   return(oc);
}

/****************************/
int noStopOnInvalid(void)
{
   return(0);
}
