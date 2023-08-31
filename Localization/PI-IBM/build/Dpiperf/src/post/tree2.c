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

/*****************************************************************
   tree.c // par,chd,sib linkage
 *****************************************************************/

#include "post.h"
#include <memory.h>
#include "a2n.h"
#include "hash.h"

pNode constNode(void);
pNode push2(pNode par, pNode new);

char lfm[128] = { "%4d %7d %12d %6d  %s:%s\n"};   // lv call base nm
char lfmt[128] = { "%4d %7d %12d %6d  %s\n"};   // lv call base nm

/******************************/
void TreeHeader(FILE * fd)
{
   fprintf(fd, " POST-Tree\n");
   fprintf(fd, "  Units :: Instructions\n\n");
   fprintf(fd, " %4s %7s %12s %s\n",
           "LV", "CALLS", "BASE", "NAME");
}

/******************************/
pNode constNode(void)
{
   return( (pNode)zMalloc( "constNode", sizeof(Node)) );
}

/******************************/
pNode push2(pNode par, pNode new)
{
   pNode p, q, r;

   q = NULL;
   p = par->chd;

   while (p != NULL)
   {
      if (strcmp(p->mnm, new->mnm) == 0)
      {
         if (strcmp(p->mod, new->mod) == 0)
         {
            return(p);                  // Node found
         }
      }
      q = p;
      p = p->sib;
   }

   // Add Node
   r = constNode();                     // new: mnm & mod
   r->par = par;
   r->mnm = xStrdup("method",new->mnm);
   r->mod = xStrdup("module",new->mod);

   // add at end
   if (q == NULL)
   {
      par->chd = r;                     // 1st chd
   }
   else
   {
      q->sib   = r;                     // add to last sib
   }

   // add at front of chd list
   //r->sib   = par->chd; // chd => 1st sib
   //par->chd = r;        // r   => chd

   return(r);
}

/******************************/
static pNode findOnStack(pNode np, pNode mp, int * pscnt)
{
   // np(currm) mp(exiting method)
   pNode top;
   int pcnt = 0;
   top = np;

   *pscnt = 0;
   while (np->par != NULL)
   {
      pcnt++;
      if (strcmp(np->mnm, mp->mnm) == 0)
      {
         if (strcmp(np->mod, mp->mod) == 0)
         {
            return np;
         }
      }
      np  = np->par;                    // pop
   }

   *pscnt = pcnt;                       // pop cnt on Failure
   return NULL;                         // not on stack
}

/******************************/
int penex(thread_t * tp, char dir, char * mnm, char * mod)
{
   Node  n;
   pNode mp;
   pNode p;
   int   pcnt;                          // no of pops to get to Node

// if (gv.ptree == 0)
   if ( 1 )
      return(0);

   mp    = (pNode)(tp->currm);
   n.mnm = mnm;
   n.mod = mod;

   //fprintf(stderr, " %c %s:%s\n", dir, mnm, mod);

   if (dir == '>')
   {                                    // entry
      tp->currm = mp = push2(mp, &n);
      mp->calls++;
      return(0);
   }

   if (dir == '<')
   {                                    // exit
      p = findOnStack(mp, &n, &pcnt);   // !Null => p ptr to Node
      if (p != NULL)
      {
         tp->currm = p->par;
      }
      else
      {
         tp->currm = tp->currT;
         fprintf(gc.msg, " < Err: POP to pid\n");
         fprintf(gc.msg, "   TOP  %s:%s\n", mp->mnm, mp->mod);
         fprintf(gc.msg, "     <  %s:%s\n", mnm, mod);
      }
      return(0);
   }

   if (dir == '@')
   {                                    // exit
      p = findOnStack(mp, &n, &pcnt);   // !Null => p ptr to Node
      if (p != NULL)
      {
         tp->currm = p;
      }
      else
      {                                 // failed
         if (pcnt == 1)
         {
            mp = tp->currm = tp->currT;
         }
         mp = tp->currm = push2(mp, &n);
         mp->calls++;
      }
      return(0);
   }

   if (dir == '?')
   {                                    // exit
      tp->currm = push2(mp, &n);        // push
      tp->currm = tp->currm->par;       // pop back
      return(0);
   }
   return(-1);
}

/******************************/
static void xtreeLine(pNode n, int ind, FILE * fd)
{
   if (ind == 0)
   {                                    // thread
      fprintf(fd, lfmt, ind, n->calls, n->base, 0, n->mod);
   }
   else
   {
      fprintf(fd, lfm, ind, n->calls, n->base, 0, n->mnm, n->mod);
   }
}

/******************************/
static void xtree(pNode n, int ind, FILE * fd)
{
   xtreeLine(n, ind, fd);
   n = n->chd;                          // 1st child

   while (n != NULL)
   {
      xtree(n, ind + 1, fd);
      n = n->sib;
   }
}

/******************************/
void xtreeOut(char * fnm)
{
   FILE     * fd;
   thread_t * tp;

   fd = xopen(fnm, "w+b");

   TreeHeader(fd);

   tp = get_thread_head();              // loop thru threads
   while (tp)
   {
      if (tp->currT != NULL)
      {
         xtree(tp->currT, 0, fd);
      }
      tp = tp->next;
   }
   fflush(fd);
   fclose(fd);
   fflush(stderr);
}
