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

#ifndef ___TREE_H
   #define ___TREE_H

// tree API
int    RTEnEx(thread_t * tp);
void   xtreeOut();
char * RTCurrentMemoryArea(thread_t * tp);
   #define cbR_Alloc  0
   #define cbR_Free   1
void   cbRollup(int mode, object_t * op);
pNode  tree_free(pNode cx, int lev);
pNode  tree_dual(pNode cx, int lev);
pNode  pushstk(pNode par, FrameInfo * fi);
void   showstack(thread_t * tp, char * s);
pNode  push(pNode par, FrameInfo *fi);

void treeWalker( void (*func)(pNode,int), pNode pn );
void treeKiller( void (*func)(pNode,int), pNode pn );

void zeroNode(pNode p, int level);      // Reset all data in node
void freeNode(pNode p, int level);      // Free the node
void freeNodeObjs(pNode p, int useOldSizes);   // Free the objects in the node

void freeLeaf(pNode p, int level);      // Free non-thread nodes, else reset data

   #ifdef RESET2STACK
void tree_stack(thread_t * tp);
   #endif
void tree_free0(thread_t * tp);

Node * checkNearbyNodes(  Node * curNode, char * addr );
Node * checkNearbyNodes2( Node * curNode, char * addr );

void   updateLUAddrs( thread_t * tp,
                      UINT64     sample_r3_eip,
                      UINT64     sample_r0_eip );

void   resolveLUAddrs( Node * np );
void   resolveRawAddrs( Node * parent );

Node * pushUAddr( Node * np, char * addr, int kernel );

#endif
