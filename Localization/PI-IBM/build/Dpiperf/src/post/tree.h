/*   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2008
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

#ifndef TREE_H
#define TREE_H

///////////////////////////
//// tprof TNode ////
typedef struct _TNode   TNode;
typedef struct _TNode * pTNode;

//   type      char   id     name
//   ========  =====  =====  ======
//   pid       p      pid    sprintf("0x%4x_%s", pid, pnm)
//   tid       t      tid    sprintf("0x%4x_%s", tid, tnm)
//   mod       m      0      mod
//   sym       n      0      sym
//   offset    ?      ?      ?

// Node needs to be redesigned for space
// node content => void *
// only fillin for symbol Node
struct _TNode
{
   UINT64 sttaddr;                      // non-zero *only* for symbol nodes

   int    base;                         // ticks
   int    cum;                          // cum ticks

   char * nm;                           // ascii string (pnm, snm, sym, ...)

   // NB: void * data candidates( inlined for max memory usage )
   // need DEEP copies
   char * cptr;                         // code ptr (to start of symbol)
   int    clen;                         // code len

   // ENIO Warning.
   //  overloading rc : symNode     => quality
   //                 :  offsetNode => line_no
   int    rc;
   char * snm;                          // sym src_file ptr
   void * module;                       // a2n shortcuts for late data
   void * symbol;                       // a2n shortcuts for late data
   // end void * stuff

   unsigned int  flags;                 // Flags from SYMDATA

   int    ccnt;                         // no of children ????
   pTNode par;                          // parent
   pTNode chd;                          // 1st child
   pTNode sib;                          // next sibling
};

pTNode push(pTNode p, char * nm);
pTNode pusha(pTNode p, char * nm, uint64 addr);
pTNode initTree(void);
void	 freeTree(pTNode p);
pTNode dualx;
int dlev[5];

///////////////////////////
//// post rt-tree Node ////
typedef struct _Node   Node;
typedef struct _Node * pNode;

struct _Node
{
   int  base;
   int  calls;

   char * mnm;                          // method
   char * mod;                          // module

   int    ccnt;                         // no of children
   pNode  par;                          // parent
   pNode  chd;                          // 1st child
   pNode  sib;                          // next sibling
};

// remember jita2n data from files
typedef struct _jrec JREC;

   #define jrec_cflag_Code    0x0001
   #define jrec_cflag_MMI     0x0002

struct _jrec
{
   JREC  * next;                        // Pointer to next JREC in list
   int     size;                        // Size of code
   uint64  addr;                        // Address of code when executing
   uint64  time;                         // Time stamp
   char  * mnm;                         // Method name
   char  * code;                        // Address of saved bytecodes
   int     cflag;                       // Flags: uses jrec_flags_????
   int     seqNo;                       // Sequence number from driver
   int     already_sent;                // Already sent to A2N
};

// remember jtnm data from files
typedef struct _jtnm JTNM;

struct _jtnm
{
   JTNM   * next;                       // Pointer to next record in list
   char   * tnm;                        // Thread name ("" if END event)
   unsigned long long int time;         // Time stamp
   int    tid;                          // Thread ID
   int    tnmid;                        // 0 or trcid when written
};


void    tree_sort(pTNode cx, int lev);
pTNode  tree_sort_out_free(pTNode cx, int lev, FILE * fd);
void    tree_new(pTNode n, int lev, pTNode root, int tklev);
void    tree_cum(pTNode cx, int lev);
void    xtreeOut(char * fnm);
#endif
