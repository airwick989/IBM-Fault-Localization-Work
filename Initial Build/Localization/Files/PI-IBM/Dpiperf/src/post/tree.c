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

/*****************************************************
   tree.c // par,chd,sib linkage
 *****************************************************/

#include "post.h"
#include "a2n.h"
#include <memory.h>

pTNode constTNode(pTNode p);

pTNode pp[64 * 1024];                   // 64k unique symbols at any level

struct off2tks
{
   int off;
   int ln;
   int tks;
};

/* ************************************************************ */
int qcmp_off(const void * p1, const void * p2)
{
   struct off2tks r;
   struct off2tks s;
   int off1, off2;

   r = *(struct off2tks *)p1;
   s = *(struct off2tks *)p2;

   off1 = r.off;
   off2 = s.off;

   if (off1 > off2) return(1);
   if (off1 < off2) return(-1);

   return(0);
}

/* ************************************************************ */
//int LKCONV qcmp_pp(const void * p1, const void * p2)
int qcmp_offpp(const void * p1, const void * p2)
{
   pTNode r;
   pTNode s;
   int off1, off2;

   r = *(pTNode *)p1;
   s = *(pTNode *)p2;

   // convert nm:0xhhh => dec
   off1 = strtol(r->nm, NULL, 0);
   off2 = strtol(s->nm, NULL, 0);

   if (off1 > off2) return(1);
   if (off1 < off2) return(-1);

   return(0);
}

/* ************************************************************ */
//int LKCONV qcmp_pp(const void * p1, const void * p2)
int qcmp_pp(const void * p1, const void * p2)
{
   pTNode r;
   pTNode s;

   r = *(pTNode *)p1;
   s = *(pTNode *)p2;

   if (r->cum > s->cum) return(1);
   if (r->cum < s->cum) return(-1);

   if (strcmp(r->nm, s->nm) > 0) return(1);
   if (strcmp(r->nm, s->nm) < 0) return(-1);

   return(0);
}

/******************************/
pTNode initTree(void)
{
   TNode n;
   pTNode p;

   p = &n;
   p->par = NULL;

   p = constTNode(p);
   p->nm = xStrdup("initTree","TOTAL");
   return(p);
}

void	 freeTree(pTNode p) {
	jfree(p->nm);
	while (p->chd) {
		freeTree(p->chd);
		pTNode nc = p->chd->sib;
		jfree(p->chd);
		p->chd = nc;
	}
	while (p->sib) {
		freeTree(p->sib);
		pTNode ns = p->sib->sib;
		jfree(p->sib);
		p->sib = ns;
	}
}

/******************************/
pTNode constTNode(pTNode p)
{
   pTNode n;

   n = (pTNode)zMalloc( "constTNode", sizeof(TNode));

   n->par  = p->par;                    // from p
   n->chd  = n->sib  = NULL;
   n->base = n->cum  = 0;
   n->nm   = "";
   n->sttaddr = 0;

   return n;
}

/******************************/
pTNode pusha(pTNode par, char * nm, uint64 sttaddr)
{
   TNode n;
   pTNode p, np;
   static int dupcnt = 0;
   static int repcnt = 0;
   static int sympushcnt = 0;


   if (gc_verbose_logmsg & gc.verbose)
   {
      if (sttaddr)
      {
         // symbol node
         sympushcnt++;
         
         WriteFlush(gc.msg, " *pusha()* cnt=%06d nm=%s addr=0x%8"_P64"X par->nm=%s\n",
                    sympushcnt, nm, sttaddr, par->nm);
      }
   }

   p = par->chd;

   while (p != NULL)
   {
      if (strcmp(p->nm, nm) == 0)
      {
         if (p->sttaddr == sttaddr)
         {
            // Either a symbol node and the address match, or any other
            // type of node. Other nodes have a zero address so if the names
            // are equal then they will be considered found.
            p->par->ccnt++;             // found
            return(p);
         }
         else
         {
            // We're trying to push a node that matches the name of a current
            // node but the addresses are different. Before yelping that we
            // have duplicate symbols, make sure there actually are valid symbols.
            // p->rc should tell us that.
            // The idea is to only have one "NoSymbol" or "NoSymbolFound" or
            // "<<section_name>>" node per module that has no symbols.
            //
            repcnt++;
            if (p->rc == A2N_NO_SYMBOLS)
            {
               // This module has no symbols so symbols can't be duplicate.
               if (repcnt <= 100)
               {               
                  if (gc_verbose_logmsg & gc.verbose)
                  {
                     WriteFlush(gc.msg, " *DUP-NOSYM* cnt=%06d node@ 0x%8"_P64"X new@ 0x%8"_P64"X %s\n",
                                repcnt, p->sttaddr, sttaddr, nm);
                  }
               }
            }
            else
            {
               // This module has symbols so symbols are duplicate.
               if (repcnt <= 100)
               {
                  if (gc_verbose_logmsg & gc.verbose)
                  {
                     WriteFlush(gc.msg, " *DUP* cnt=%06d node@ 0x%8"_P64"X new@ 0x%8"_P64"X %s\n",
                                repcnt, p->sttaddr, sttaddr, nm);
                  }
               }

               if (dupcnt <= 10)
               {
                  dupcnt++;
                  WriteFlush(gc.msg,
                             " DUP SYM : cnt=0x%08X - old %8" _P64 "x  new %8" _P64 "x %s\n",
                             dupcnt, p->sttaddr, sttaddr, nm);

                  if (dupcnt == 1)
                  {
                     ErrVMsgLog( " *****\n");
                     ErrVMsgLog( " ***** There are some duplicate symbols: same name, different address\n");
                     ErrVMsgLog( " ***** - They have been fixed up by POST.\n");
                     if (!gv.a2nrdup)
                     {
                        ErrVMsgLog( " ***** - May want to try the -a2nrdup option.\n");
                     }
#ifdef WIN32
                     ErrVMsgLog( " ***** - If duplicate symbol is a section name (\"<<name>>\") that's OK.\n");
                     WriteFlush(gc.msg, " *****   Section names most likely means there are no symbols for the\n");
                     WriteFlush(gc.msg, " *****   module. You may want to try the -nsf option to change\n");
                     WriteFlush(gc.msg, " *****   \"<<name>>\" into \"NoSymbols\", if you prefer that. Or you\n");
                     WriteFlush(gc.msg, " *****   may want to get symbols for the particular module(s).\n");
#endif
                     WriteFlush(stderr, " ***** - See post.msg file for more information.\n");
                     ErrVMsgLog( " *****\n");
                  }
               }
            }
         }
      }
      p = p->sib;
   }

   // make new node, attach @ front
   np = &n;
   np->par = par;
   p = constTNode(np);                  // par: new  chd,sib: NULL

   // p => 1st chd
   p->par   = par;
   p->sib   = par->chd;                 // curr 1st-chd => p's sib
   par->chd = p;                        // p becomes 1st chd

   p->nm      = xStrdup("pusha",nm);
   p->ccnt    = 1;
   p->sttaddr = sttaddr;

   return(p);
}

/******************************/
pTNode push(pTNode par, char * nm)
{
   TNode n;
   pTNode p, np;

   p = par->chd;

   while (p != NULL)
   {
      if (strcmp(p->nm, nm) == 0)
      {
         p->par->ccnt++;                // found
         return(p);
      }
      p = p->sib;
   }

   // make new node, attach @ front
   np = &n;
   np->par = par;
   p = constTNode(np);                  // par: new  chd,sib: NULL

   // p => 1st chd
   p->par  = par;
   p->sib  = par->chd;                  // curr 1st-chd => p's sib
   p->nm   = xStrdup("push",nm);
   p->ccnt = 1;

   par->chd = p;                        // p becomes 1st chd

   return(p);
}

/******************************/
void micro(pTNode p)
{
   // gv.disasm -> 0:asm-code 1:disasm
   // The node is a SYM node in a ModSymOff tree
   static pTNode * ppp;
   static struct off2tks * o2t;

   static int      tsize = 0;

   int             i, j, len, rc;
   int             offset = 0;
   int             size;
   int             onum;
   unsigned char * c;
   unsigned char * opc;
   char            buf[2048];
   pTNode          q;
   int             maxoff = 0;
   int             mode;

   // p is ptr to Symbol entry
   if (strcmp(p->nm, "<plt>") == 0) return;

   c   = (unsigned char *)p->cptr;      // code ptr BAD DOG !!
   len = p->clen;                       // code length
   rc  = p->rc;                         // symbol quality

   mode = p->flags & ( SDFLAG_MODULE_32BIT | SDFLAG_MODULE_64BIT );

   if ( 0 == mode )
   {
      mode = SDFLAG_MODULE_32BIT;

      OptVMsg( "Defaulting Mode to 32-bits for %s\n", p->nm );
   }

   q = p->chd;                          // linked list of offsets
   i = 0;

   // count offsets
   while (q)
   {
      i++;
      q = q->sib;
   }

   // calloc max offsets, grow static struct as necessary
   if (i > tsize)
   {
      int sz;

      if (i < 2000)
      {
         sz = 2000;
      }
      else sz = i;

      ppp =         (pTNode *)zMalloc( "micro",   sz * sizeof(pTNode) );
      o2t = (struct off2tks *)zMalloc( "off2tks", sz * sizeof(struct off2tks) );

      tsize = sz;
   }

   i = 0;
   q = p->chd;                          // linked list of offsets

   // put offsets in array for sorting
   while (q)
   {
      ppp[i] = q;

      o2t[i].off = strtol(q->nm, NULL, 0);   // hex offset
      o2t[i].tks = q->base;             // ticks @ offset
      o2t[i].ln  = q->rc;               // pick up lineno from offset node

      i++;
      q = q->sib;
   }
   onum = i;                            // no of unique offsets

   // sort offsets
   qsort((void *)o2t, onum, sizeof(struct off2tks), qcmp_off);

   fprintf(gd.micro, " F: %s\n", p->snm);

   // show sorted offsets

   for (i = 0; i < onum; i++)
   {
      fprintf(gd.micro, " T: %4x %4d %5d\n",
              o2t[i].off, o2t[i].ln, o2t[i].tks);
   }

   maxoff = o2t[onum - 1].off;
   i = offset = 0;

   // L: all line Nos
   // need MOD in micro (parent node nm)
   q = p->par;

#ifdef WIN32
   {
      if (p->module != NULL && p->symbol != NULL)
      {
         LINEDATA * ld;
         int i, cnt = 0;

         ld = A2nGetLineNumbersForSymbol(p->module, p->symbol);
         if (ld != NULL)
         {
            fprintf(gd.micro, " cnt %d\n",  ld->cnt);
            fflush(gd.micro);

            for (i = 0; i < ld->cnt; i++)
            {
               fprintf(gd.micro, " L: %d 0x%x 0x%x\n",
                       ld->line[i].lineno,
                       ld->line[i].offset_start,
                       ld->line[i].offset_end
                      );
            }
            fflush(gd.micro);
         }
      }
   }
#endif

   if ( ! gv.disasm )                   // print code in asc. break into 8 byte lines
   {
      if (rc == 0)
      {
         // ticks before code having ticks
         // C addr code
         // T addr tks
         int j, end, step  = 16;

         while (offset < len)
         {
            end = offset + step;
            if (end > len) end = len;

            fprintf(gd.micro, " C: %6x ", offset);
            for (j = offset; j < end; j++)
            {
               if (c != NULL)
               {
                  fprintf(gd.micro, "%02x", *c);
                  c++;
               }
               else
               {
                  fprintf(gd.micro, " NULLptr");
                  break;
               }
            }
            fprintf(gd.micro, "\n");
            fflush(gd.micro);

            offset += step;
         }
      }
      else
      {
         fprintf(gd.micro, " SorryNoSymbols=>NoCode\n");
         fflush(gd.micro);
      }

   }
   else                                 // disasm entire method/function
   {
      while (offset < len)
      {
         opc = (unsigned char *)i386_instruction((char *)c,
                                                 (uint64)0,
                                                 &size,
                                                 buf,
                                                 mode );

         while ( (i < onum) && (o2t[i].off < offset) )
         {                              // errors
            fprintf(gd.micro, " %6d %04X %s\n", o2t[i].tks, offset, "NotAligned");
            fflush(gd.micro);
            i++;
         }

         if ( i < onum && o2t[i].off == offset)
         {
            fprintf(gd.micro, " %6d", o2t[i].tks);
            i++;
         }
         else
         {
            fprintf(gd.micro, "      -");
         }
         fprintf(gd.micro, " %04X ", offset);

         for ( j = 0; j < 8; j++ )
         {
            if ( j < size )
            {
               fprintf(gd.micro, " %02X", c[j] );
            }
            else
            {
               fprintf(gd.micro, "   " );
            }
         }
         fprintf(gd.micro, " %s\n", buf);

         fflush(gd.micro);

         c      += size;
         offset += size;

         if (size <= 0)
         {
            fprintf(gd.micro, " 0 SIZE error ???. ending micro disasm\n");
            fflush(gd.micro);
            return;
         }
      }
   }
}

/******************************/
void treeline(pTNode p, int lev, FILE * fd)
{
   double pc = 0;
   int tlim  = (int)(gv.clip * gv.tottks);

   if (lev == 0)
   {
      gv.llev = lev;
      fprintf(fd, " %1s %3s %6s %5s     %s\n\n",
              "", "LAB", "TKS", "%%%", "NAMES");
   }

   // lev, base cum pc <lev> name

   // manage spaces(while tick filtering) for good looks
   if (p->cum >= tlim)
   {
      if ( (gv.llev > lev) && (gv.nospace == 0) ) fprintf(fd, "\n");
   }

   // lev 0 is root ??
   if (lev >= 1)
   {
      if (p->cum >= tlim)
      {
         pc = 100.0 * p->cum / gv.tottks;
         fprintf(fd, " %*s %3s %6d %5.2f %*s  %s\n",
                 lev, "", gv.flab[lev], p->cum, pc, lev, "", p->nm);
      }
      gv.llev = lev;
   }

   // micro
   // separate file
   // ticks : p->cum >= 1
   // code  :  p->cptr != NULL
   if (gv.micro == 1)
   {
      if (gv.off == 1)
      {
         if (gv.xind[3] == 5)
         {                              // Offsets in lev 3
            if (lev == 1)
            {                           // ModLevel
               if (p->cum >= 1)
               {                        // ticks
                  pc = 100.0 * p->cum / gv.tottks;
                  fprintf(gd.micro,
                          "\n ### %*s %3s %6d %5.2f %*s  %s\n",
                          lev, "", gv.flab[lev], p->cum, pc, lev, "", p->nm);
               }
            }
            else if (lev == 2)
            {                           // SymLevel
               fprintf(gd.micro,
                       " ### %*s %3s %6d %5.2f %*s  %s  %" _P64 "x %x\n",
                       lev, "",
                       gv.flab[lev],
                       p->cum,
                       pc,
                       lev, "",
                       p->nm,
                       p->sttaddr,      // bug fix (vs below, cptr)
                       p->clen);
               //p->nm, p->cptr, p->clen);

               if (p->cptr != NULL)
               {                        // Symbol has code
                  micro(p);             // asc-code or disasm
               }
            }
         }
      }
   }
}

/******************************/
void tree_cum(pTNode cx, int lev)
{
   pTNode nx;
   pTNode px;

   cx->cum = cx->base;
   nx = cx->chd;

   while (nx != NULL)
   {
      tree_cum(nx, lev + 1);
      nx = nx->sib;
   }

   px = cx->par;
   if (px != NULL)
   {                                    // lev 0 has no par
      px->cum += cx->cum;
   }
}

/* ************************************************************ */
void swap(pTNode * x, int M, int I)
{
   pTNode p;

   p    = x[M];
   x[M] = x[I];
   x[I] = p;
}

/* ************************************************************ */
void myqsort(pTNode * x, int L, int U)
{
   int r, I, M;
   pTNode T;

   if (L < U)
   {
      r = L;
      T = pp[L];
      M = L;

      for (I = L + 1; I <= U; I++)
      {                                 // x[L+1...M] < T <= x[M+1...i-1]
         if (x[I]->cum > T->cum)
         {
            M++;
            swap(x, M, I);
         }
         else if (x[I]->cum == T->cum)
         {
            if (1 == (I % 2))
            {
               M++;
               swap(x, M, I);
            }
         }
      }
      swap(x, L, M);

      myqsort(x,     L, M - 1);
      myqsort(x, M + 1,     U);
   }
}

/******************************/
void tree_out(pTNode n, int lev, FILE * fd)
{
   treeline(n, lev, fd);

   n = n->chd;

   while (n != NULL)
   {
      tree_out(n, lev + 1, fd);
      n = n->sib;
   }
}

/******************************/
pTNode tree_sort_out_free(pTNode cx, int lev, FILE * fd)
{
   pTNode nx;
   int i, num;

   treeline(cx, lev, fd);

   // @ entry
   nx = cx->chd;
   i = 0;

   // copy to array
   while (nx != NULL)
   {
      i++;
      pp[i] = nx;
      nx = nx->sib;
   }
   num = i;

   if (num >= 2)
   {
      myqsort(pp, 1, num);

      cx->chd = pp[1];

      for (i = 1; i < num; i++)
      {
         pp[i]->sib = pp[i + 1];
      }
      pp[num]->sib = NULL;
   }

   // @ body
   nx = cx->chd;
   while (nx != NULL)
   {
      nx = tree_sort_out_free(nx, lev + 1, fd);
   }

   nx = cx->sib;
   xFree(cx,sizeof(TNode));
   return(nx);
}

/******************************/
void tree_sort(pTNode cx, int lev)
{
   pTNode nx;
   int i, num;

   // @ entry
   nx = cx->chd;
   i = 0;

   while (nx != NULL)
   {
      i++;
      pp[i] = nx;
      nx = nx->sib;
   }
   num = i;

   if (num >= 2)
   {
      //qsort((void *)pp, num, sizeof(pTNode), qcmp_pp);
      myqsort(pp, 1, num);

      cx->chd = pp[1];

      for (i = 1; i < num; i++)
      {
         pp[i]->sib = pp[i + 1];
      }
      pp[num]->sib = NULL;
   }

   // @ body
   nx = cx->chd;
   while (nx != NULL)
   {
      tree_sort(nx, lev + 1);
      nx = nx->sib;
   }
}

/******************************/
// global dualx & dlev
void tree_dual(pTNode cx, int lev)
{
   pTNode nx;

   nx = cx->chd;

   while (nx != NULL)
   {
      if (dlev[lev + 1] != 1)
      {                                 // lev+1 GOOD => push
         dualx = pusha(dualx, nx->nm, nx->sttaddr);
      }
      dualx->base += nx->base;

      // normal walk
      tree_dual(nx, lev + 1);
      nx = nx->sib;
   }

   // lev GOOD => pop dual
   if (dlev[lev] != 1)
   {                                    // lev GOOD => POP
      dualx = dualx->par;
   }
}

/******************************/
void tree_new(pTNode n, int lev, pTNode root, int tklev)
{
   pTNode p, q;
   int i, j, tks;

   // depth-first-walk to a leaf
   // then create alt tree via global vars

   gv.stk[lev] = n;                     // stk from original tree

   if (lev == tklev)
   {                                    // @ leaf => create new tree entry
      // new tree entry from orig stk @ leaf
      tks = n->base;                    // tks only on orig tree leafs

      p = root;
      for (i = 1; i <= gv.xlevs; i++)
      {
         j = gv.xind[i];                // level in original tree
         q = gv.stk[j];                 // Node in original tree
         p = pusha(p, q->nm, q->sttaddr);


         // MAKE THIS A TNODE COPY
         // subroutine it
         p->cptr    = q->cptr;
         p->clen    = q->clen;
         p->rc      = q->rc;
         p->snm     = q->snm;
         p->module  = q->module;
         p->symbol  = q->symbol;
         p->sttaddr = q->sttaddr;
         p->flags   = q->flags;
      }
      p->base += tks;                   // put tks on new tree's leafs
   }

   n = n->chd;

   while (n != NULL)
   {
      tree_new(n, lev + 1, root, tklev);
      n = n->sib;
   }
}

/******************************/
void tree_mte(pTNode cx, int lev, FILE * fd)
{
   pTNode nx;

   fprintf(fd, " %4d %*s %s\n", lev, lev, "", cx->nm);

   nx = cx->chd;

   while (nx != NULL)
   {
      tree_mte(nx, lev + 1, fd);
      nx = nx->sib;
   }
   return;
}
