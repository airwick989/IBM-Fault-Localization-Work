/*
 *
 *  Copyright (c) IBM Corporation 1996.
 *  Property of IBM Corporation.
 *  All Rights Reserved.
 *
 *  Origin 30 (IBM)
 *
 */

#include "global.h"

/* ************************************************************ */
void init_global()
{
   /* all vars set  to 0 at define time */
   int i;

   gv.apt       = "StartingPT";
   gv.ptp       = ptptr(gv.apt);        /* unknown pidtid to start */

   gv.abs       = 0;
   gv.nstr[0]   = CZERO;
   gv.hkstt     = 0;
   gv.hkstp     = 0xffffffff;
   gv.ind       = 1;
   gv.Units     = "UnDefined";

   gv.pdep      = 0;                    // default to xarc format
   gv.cdep      = 0;
   gv.sdep      = 100;                  // use clip below instead ??
   gv.ptclip    = 1;                    // 1% Parent: no expand if < ptclip
   gv.ctclip    = 1;                    // 1% Child: no expand & stop expand when < ctclip
   gv.pruneSize = 25;

   gv.raw       = 1;                    // Default is scaling by 1, include columns

   gv.curcpu = -1;
   gv.filter_cpu = -1;                  // No CPU filtering
   gv.filter_pidtid = NULL;             // No pid_tid filtering
   gv.group_disp = 0;                   // Don't group by dispatches. Interleaving xflow
   gv.dbgmsg = 0;

   gv.maxTokens = MAX_TOKENS;

   for (i = 0; i < 100; i++)
   {
      gv.indent[i] = '-';
      if (4 == i % 5) gv.indent[i] = '+';
   }

   for (i = 0; i <= 21; i++)
   {
      gv.chist[i].btm   = 0;
      gv.chist[i].nodes = 0;
   }

   for (i = 0; i < ARCF_MAX_CPUS; i++) {
      gv.ftempcpu[i] = NULL;
      gv.curthr[i] = NULL;
      gv.ftemp_cnt[i] = 0;
   }

   return;
}

/* ************************************************************ */
/* binary search of HKDB by method name */
int LKCONV bs_hkmeth(const void * key, const void * ptr)
{
   struct HKDB * hp;
   int rc;
   STRING meth;

   meth = (STRING)key;
   hp = *(struct HKDB **)ptr;

   if (strcmp(meth, hp->hstr) >= 0)
   {
      if (strcmp(meth, hp->hstr) == 0)
      {
         rc = 0;
      }
      else
      {
         rc = 1;
      }
   }
   else
   {
      rc = -1;
   }

   return(rc);
}

/* ************************************************************ */
/* bsearch HKDB, add if not found */
struct HKDB * hkdb_find(STRING meth, int flg)
{
   struct HKDB * hp;
   struct HKDB ** php;

   php = (struct HKDB **)bsearch((void *)meth, (void *)hkdb,
                                 gv.hkcnt, sizeof(struct HKDB *), bs_hkmeth);

   if (php != NULL)
   {
      hp = *php;
   }
   else
   {
      hp = 0;
   }

   if (hp == 0)
   {
      if (flg == 1)
      {                                 /* 0 => exit , 1 => add hook if missing */
         hp = add_hook(meth);           /* add & resort HKDB */
      }
   }
   return(hp);
}

/* ************************************************************ */
int smname(char * nin, char * buf)
{
   char * s1;
   char * cp;
   char c;
   int state = 1;                       // logic to find key pts
   char * stt = 0;
   char * nm;

   nm = Remember(nin);

   s1 = strstr(nm, ".");

   if (s1 != nm)
   {
      cp = nm;
      c = *cp;
      stt = cp + 2;

      while (c != '\0')
      {
         if (c == '.')
         {
            state = 2;
         }
         if (state == 1)
         {
            if (c == '/')
            {
               stt = cp + 1;
            }
         }
         if (state == 2)
         {
            if (c == '(')
            {
               *cp = '\0';
               state = 3;
            }
         }
         cp++;
         c = *cp;
      }

      s1 = nm;
      if (stt == nm + 2)
      {                                 // no /'s
         sprintf(buf, "%s", s1);
      }
      else
      {
         s1[2] = '\0';
         sprintf(buf, "%s%s", s1, stt);
      }

      return(0);
   }
   return(1);
}

/* ************************************************************ */
struct HKDB * add_hook(STRING meth)
{
   struct HKDB * hp;
   struct HKDB * hpt;
   int i, rc;
   char buf[1024] = {""};

   hp = const_hkdb(1);                  /* create new hkdb entry */
   /* gv.hkcnt++ */

   hp->hstr = Remember(meth);
   hp->hstr2 = hp->hstr;

   rc = smname(hp->hstr, buf);

   if (rc == 0)
   {
      hp->hstr3 = Remember(buf);
   }
   else
   {
      hp->hstr3 = hp->hstr2;
   }

   //printf("%s\n", hp->hstr3);

   /* insert sort */
   for (i = gv.hkcnt - 1; i > 0; i--)
   {
      hpt = hkdb[i - 1];
      if (strcmp(meth, hpt->hstr) < 0)
      {
         hkdb[i] = hpt;
      }
      else
      {
         hkdb[i] = hp;
         break;
      }
   }
   if (i == 0) hkdb[0] = hp;

   return(hp);
}

/* UNUSED routine */
/* ************************************************************ */
void hksort(void)
{
   struct HKDB * hp1;
   struct HKDB * hp2;
   struct HKDB * tmp;
   int i;

   /* bubble sort */
   for (i = 0; i < gv.hkcnt - 1; i++)
   {
      hp1 = hkdb[i];
      hp2 = hkdb[i + 1];
      if (strcmp(hp1->hstr, hp2->hstr) > 0)
      {
         tmp = hkdb[i];
         hkdb[i] = hkdb[i + 1];
         hkdb[i + 1] = tmp;
      }
   }
}

/* ************************************************************ */
void show_hkdb(void)
{
   int i;

   OptMsg("\n SHOW_HKDB\n");
   for (i = 0; i < gv.hkcnt; i++)
   {
      OptVMsg(" %4d %8p <%s>\n", i, hkdb[i], hkdb[i]->hstr);
   }
   OptMsg("\n");
}

