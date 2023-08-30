/*   Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003
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

   #include <stdio.h>
   #include <string.h>

   char disstr[16] = {"ZOS_INST"};
   int istop  = 1; /* invalid-stop 1 :: def: continue */

/****************************/
int noStopOnInvalid(void) {
   istop = 0; /* def: stop (istop = 1) */
   return(0);
}

/****************************************************
 * Function Name: disasm_instruction
 *
 * Description:   Disassembles one ZOS instruction.
 *
 * Output:
 *                return
 *                * buf  - disasm output buffer
 ****************************************************/
char * i386_instruction(
   char   * data,  /* instruction                */
   long     addr,  /* address/offset             */
   int    * size,
   char   * buf,
   int      b32 )  /* 32(1) or 16(0) bit segment */

{
   char * p = disstr;

   strcpy(buf, disstr);
   *size = 4;
   return(p);
}

/************************/
int inst_type(char * opc) {
   int rc;
   int oc;

   if(     strncmp(opc, "call", 4) == 0) oc = 1;
   else if(strncmp(opc, "ret" , 3) == 0) oc = 2;
   else if(strncmp(opc, "j"  ,  1) == 0) oc = 3;
   else if(strncmp(opc, "iret", 4) == 0) oc = 4;
   else                                  oc = 5;

   return(rc);
}

