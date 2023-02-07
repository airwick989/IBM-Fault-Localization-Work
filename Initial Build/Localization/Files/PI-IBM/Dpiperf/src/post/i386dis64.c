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
 * Description:  64 bit disassembler
 ****************************************************/
#include "encore.h"

char OPNM[32] = "???"; /* global inst type in ascii */

#ifndef NODIS
   char * dis_ia64_bundle(char * buf, char * data, char * addr);
#endif

/************************/
char * i386_instruction(
   char * data,  /* instruction    */
   char * addr,  /* address/offset */
   int  * size,
   char * buf,
   int    b32 )  /* 32(1) or 16(0) bit segment */
{
   char c;
   char * cp;
   size_t nib = 0;

   nib = 0xc & (size_t)data; /* 0 4 8 c */

   if(nib == 0) *size = 4;
   if(nib == 4) *size = 4;
   if(nib == 8) *size = 8;

   #ifndef NODIS
      dis_ia64_bundle(buf, data, addr);
   #endif

   // a movl takes last 12 of 16 byte bundle
   if(strstr(buf,  "movl") != NULL) (*size)=12;

   strcpy(OPNM, buf);
   cp = &OPNM[0];

   /* parse buf for op-code
    * or return whole buf and use strstr
    * to interpret in "inst_type"
    */
   if(0 == 1) {
      while((c = *cp) != 0) {
	 if( (c == ' ') || (c == '\t') ) {
	    *cp = 0;
	    break;
	 }
	 cp++;
      }
   }
   return(OPNM);
}

/************************/
int inst_type(char * opc) {
   int oc = 5;

   if     (strstr(opc, "br.call") != NULL) oc = 1;
   else if(strstr(opc, "br.ret")  != NULL) oc = 2;
   else if(strstr(opc, "iret")    != NULL) oc = 4;
   else if(strstr(opc, " call")   != NULL) oc = 1;
   else if(strstr(opc, " ret")    != NULL) oc = 2;
   else if(strstr(opc, "alloc")   != NULL) oc = 7;
   else                                    oc = 3;

   /*
      1  call
      2  ret
      3  other
      4  iret
      5  unknown
      7  alloc
    */

   return(oc);
}

/****************************/
int noStopOnInvalid(void) {
   return(0);
}
