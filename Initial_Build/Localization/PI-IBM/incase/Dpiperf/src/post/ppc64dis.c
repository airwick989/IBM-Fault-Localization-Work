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
#include "encore.h"

char OPNM[64] = {"NoDis"};              /* global inst type in ascii */
char * dis_ppc64_instr(char * buf, char * data, char * addr);

/************************/
char * i386_instruction( char * data,   /* instruction    */
                         char * addr,   /* address/offset */
                         int  * size,
                         char * buf,
                         int    b32 )   /* 32(1) or 16(0) bit segment */
{
   char c;
   char * cp;

#ifndef NODIS
   dis_ppc64_instr(buf, data, addr);

   *size = 4;

   strcpy(OPNM, buf);
   cp = &OPNM[0];

   /* parse for opcode (1st field) */
   while ((c = *cp) != 0)
   {
      if ( (c == ' ') || (c == '\t') )
      {
         *cp = 0;
         break;
      }
      cp++;
   }
#endif

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
