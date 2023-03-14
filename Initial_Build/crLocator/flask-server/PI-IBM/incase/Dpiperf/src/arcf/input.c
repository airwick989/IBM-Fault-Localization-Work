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
/* ARCF Input formats:
     - ascii :    many
     - binary :
       - javaos/strace
       - dekko/strace
       - aix/trace
 */
/* ************************************************************ */
/* hma added #define _XOPEN_SOURCE_EXTENDED 1  */
#define _XOPEN_SOURCE_EXTENDED 1

#include "global.h"
#include "arc.h"
#include "version.h"
#include "sdate.h"

int    memflag    = 0;

/* binary formats */
int    g_oopt     = 0;
int    g_lopt     = 0;
char   nullstr[2] = {""};
char * g_ostr     = nullstr;

/************************************************************* */
void treeOpt(char * s)
{
   int i;

   i = s[0] - '0';
   if ( (i >= 0) && (i <= 9) )
   {
      /* symbolic only */
      fprintf(stderr, " ERROR: Use Symbolic Refs (mtreeN & mtreeO)\n");
      Usage();
      exit(1);
   }

   g_lopt = 0;
   g_ostr = Remember(s);                /* mtree symbolic field */

   /* should be BASE AO AB LO LB */
}

/************************************************************* */
void optErr(char * why)
{
   fprintf(stderr, "************************************************************\n");
   fprintf(stderr, "OPTION-ERR: %s\n", why);
   fprintf(stderr, "************************************************************\n");
   fflush(stderr);
   // Usage();
   exit(1);
}

/************************************************************* */
// Collaspe ThreadNames. Alternative to xbtree
void thrdnames(char * tnm)
{
   int i, tcnt;
   char buf[2048];
   STRING * tokarr;

   gv.tfile = fopen(tnm, "r");
   if (gv.tfile == NULL)
   {
      fprintf(stderr, " input.c : can't open : %s\n", tnm);
      exit(1);
   }

   i = 0;
   while (fgets(buf, 2048, gv.tfile) != NULL)
   {
      tokarr = tokenize(buf, &tcnt);

      gv.tnmarr[i] = Remember(tokarr[0]);
      i++;
   }
   gv.tnms = i;

   OptMsg("\n Collapse ThreadNames List\n");
   for (i = 0; i < gv.tnms; i++)
   {
      OptVMsg(" %d %s\n", i, gv.tnmarr[i]);
   }
   OptMsg("\n");

}

/************************************************************* */
void Scan_Args(int argc, STRING * argv)
{
   STRING s, t;
   int    i;
   int    fopt = 0;                     // -F specified
   int    topt = 0;                     // -t specified

   argc--;                              // Skip the program name

   if (argc == 0) {
      Usage();
      exit(0);
   }

   while ( argc > 0 )
   {
      //char buf[8];
      float f;

      argc--; argv++;                   // Next argument
      s = *argv;

      if (strcmp(s, "-help") == 0 || strcmp(s, "-?") == 0 || strcmp(s, "?") == 0) {
         Usage();
         exit(0);
      }

      if (strcmp(s, "-verbose") == 0)
      {
         //gv.verbose = 1;             // Already done in main, just discard the parameter
         continue;
      }

      if (strcmp(s, "-abs") == 0)
      {
         gv.abs = 1;                    // Values are absolute, not deltas
         continue;
      }

      if (strcmp(s, "-prune") == 0)
      {
         if ( argc <= 0 )
         {
            optErr( "Missing data for -prune option" );
         }
         argc--; argv++;

         sscanf(*argv, "%f", &f);       /* % cum */
         gv.prune = (UNIT)f;

         OptVMsg(" case: -prune %.2f\n", gv.prune);
         continue;
      }

      if (strcmp(s, "-prunesize") == 0)
      {
         if ( argc <= 0 )
         {
            optErr( "Missing data for -prunesize option" );
         }
         argc--; argv++;

         gv.pruneSize = atoi(*argv);
         OptVMsg(" case: -prunesize %d\n", gv.pruneSize);
         continue;
      }

      if (strcmp(s, "-snms") == 0)
      {
         gv.snms = 1;
         continue;
      }

      if (strcmp(s, "-cnode") == 0)
      {
         gv.cnode = 1;
         continue;
      }

      if (strcmp(s, "-FW") == 0)
      {
         if ( argc <= 0 )
         {
            optErr( "Missing data for -FW option" );
         }
         argc--; argv++;

         gv.wfnm = Remember(*argv);     // where file
         gv.where = fopen(gv.wfnm, "rb");
         if (gv.where == 0)
         {
            fprintf(stderr, " input2.c : Can't open %s\n", gv.wfnm);
            pexit(" input2.c ");
         }

         OptVMsg(" case: -FW %s\n", gv.wfnm);
         continue;
      }

      if (strcmp(s, "-ptclip") == 0)
      {
         if ( argc <= 0 )
         {
            optErr( "Missing data for -ptclip option" );
         }
         argc--; argv++;

         sscanf(*argv, "%f", &f);       /* % of cum */
         gv.ptclip = (double)f;

         OptVMsg(" case: -ptclip %.1f\n", gv.ptclip);
         continue;
      }

      if (strcmp(s, "-ctclip") == 0)
      {
         if ( argc <= 0 )
         {
            optErr( "Missing data for -ctclip option" );
         }
         argc--; argv++;

         sscanf(*argv, "%f", &f);       /* pruning % of cum */
         gv.ctclip = (double)f;

         OptVMsg(" case: -ctclip %.1f\n", gv.ctclip);
         continue;
      }

      if (strcmp(s, "-pdep") == 0)
      {
         if ( argc <= 0 )
         {
            optErr( "Missing data for -pdep option" );
         }
         argc--; argv++;
         gv.pdep = atoi(*argv);
         OptVMsg(" case: -pdep %d\n", gv.pdep);
         continue;
      }

      if (strcmp(s, "-cdep") == 0)
      {
         if ( argc <= 0 )
         {
            optErr( "Missing data for -cdep option" );
         }
         argc--; argv++;
         gv.cdep = atoi(*argv);
         OptVMsg(" case: -cdep %d\n", gv.cdep);
         continue;
      }

      if (strcmp(s, "-sdep") == 0)
      {
         if ( argc <= 0 )
         {
            optErr( "Missing data for -sdep option" );
         }
         argc--; argv++;
         gv.sdep = atoi(*argv);
         OptVMsg(" case: -sdep %d\n", gv.sdep);
         continue;
      }

      if (strcmp(s, "-sobjs") == 0)
      {
         gv.sobjs = 1;                  /* separate mtreeO psuedo objs by size */
         continue;
      }
      if (strcmp(s, "-nrecur") == 0)
      {
         gv.norecur = 1;                /* no recursion */
         continue;
      }
      if (strcmp(s, "-nobtree") == 0)
      {
         gv.nobtree = 1;                /* no xbtree */
         continue;
      }
      if (strcmp(s, "-ksum") == 0)
      {
         gv.ksum = 1;
         continue;
      }
      if (strcmp(s, "-tsort") == 0)
      {
         gv.tsort = 1;
         continue;
      }

      if (strcmp(s, "-nopre") == 0)
      {
         gv.nopre = 1;
         continue;
      }

      if (strcmp(s, "-nicnt") == 0)
      {
         gv.nicnt = 1;
         continue;
      }

      if (strcmp(s, "-html") == 0)
      {
         gv.html = 1;
         continue;
      }

      if (strcmp(s, "-oldheader") == 0)
      {
         gv.oldheader = 1;
         continue;
      }

      if (strcmp(s, "-ctree") == 0)
      {
         char * cp;

         if ( argc <= 0 )
         {
            optErr( "Missing data for -ctree option" );
         }
         argc--; argv++;
         cp = Remember(*argv);
         gv.ctstr[gv.ctree] = cp;
         gv.ctree++;

         OptVMsg(" ctree %d ctstr %s\n",
                 gv.ctree, gv.ctstr[gv.ctree - 1]);
         continue;
      }

      if (strcmp(s, "-tnms") == 0)
      {
         char * cp;

         if ( argc <= 0 )
         {
            optErr( "Missing data for -ctree option" );
         }
         argc--; argv++;
         gv.tnms = 1;
         cp = *argv;

         OptVMsg(" tfile %s\n", cp);
         thrdnames(cp);

         continue;
      }

      if (strcmp(s, "-F") == 0)
      {
         OptMsg(" case: F\n");
         if (argc < 1)
         {
            optErr("No filename specified for -F Option");
         }
         argc--; argv++;
         nrmfile = Remember(*argv);
         fopt = 1;

         /* array of infile names */
         gv.infiles[gv.ninfiles] = Remember(*argv);
         gv.ninfiles++;
         /*
         fprintf(stderr," case: -F %s %d\n", nrmfile, gv.ninfiles);
          */
         if (gv.ninfiles >= 2) gv.deltree = 1;

         continue;
      }

      if (strcmp(s, "-dbgmsg") == 0) {
         OptMsg(" case: dbgmsg\n");
         gv.dbgmsg = 1;
         continue;
      }

      if (strcmp(s, "-acpu") == 0) {
         OptMsg(" case: acpu\n");
         gv.append_cpu = 1;
         continue;
      }

      if (strcmp(s, "-ptcpu") == 0) {
         OptMsg(" case: ptcpu\n");
         gv.append_cpu_pidtid = 1;
         continue;
      }

      if (strcmp(s, "-gdisp") == 0) {
         OptMsg(" case: gdisp\n");
         gv.group_disp = 1;
         gv.append_cpu_pidtid = 1;
         continue;
      }

      if (strcmp(s, "-fcpu") == 0) {
         int len;

         if (argc <= 0)
            optErr( "Missing CPU number for -fcpu option");

         argc--; argv++;
         len = (int)strlen(*argv);
         for (i = 0; i < len; i++) {
            if (*argv[i] < '0' || *argv[i] >'9')
               optErr("Numeric data required for -fcpu option");
         }

         gv.filter_cpu = atoi(*argv);
         OptMsg(" case: fcpu\n");
         continue;
      }

      if (strcmp(s, "-fthr") == 0) {
         int len;

         if (argc <= 0)
            optErr( "Missing pid_tid for -fthr option");

         argc--; argv++;
         gv.filter_pidtid = Remember(*argv);
         len = (int)strlen(gv.filter_pidtid);
         for (i = 0; i < len; i++) {
            if (!isxdigit(gv.filter_pidtid[i]) && gv.filter_pidtid[i] != '_')
               optErr("Value for -fthr option must be valid hex digits in the form: pid_tid\n");
         }

         OptMsg(" case: fthr\n");
         continue;
      }

      if (*s++ == '-')
      {
         while (*s != '\0')
         {
            switch (*s++)
            {
            case '-':                   /* skip option */
               OptMsg(" case: -\n");
               break;

            case 'a':                   /* all records in xflow */
               OptMsg(" case: a\n");
               gv.allrecs = 1;
               break;

            case 'c':                   /* ascii condensed flow */
               OptMsg(" case: c\n");
               gv.cond = 1;
               break;

            case 'i':                   /* remove indent str in xtree */
               OptMsg(" case: i\n");
               gv.ind = 0;
               break;

            case 'd':                   /* embed dptr in xtree */
               OptMsg(" case: d\n");
               gv.dptr = 1;
               break;

            case 'D':                   /* debug */
               /* 24 => bytes out file for aix */
               OptMsg(" case: D\n");
               if (argc <= 0)
               {
                  optErr("Missing data for -D option");
               }
               argc--; argv++;
               gv.db = atoi(*argv);
               break;

            case 'e':                   /* error mode */
               /*
                   Explain modes here:
                */
               OptMsg(" case: e\n");
               gv.emode = 1;
               break;

            case 'f':                   /* ascii flow */
               OptMsg(" case: f\n");
               gv.flow = 1;
               break;

            case 'h':                   /* aix hdf file */
               OptMsg(" case: H\n");
               if (argc <= 0)
               {
                  optErr("Missing data for -h option");
               }
               argc--; argv++;
               printf(" aix hdf file %s\n", *argv);

#ifdef _AIX32
               proc_aixhdf(*argv);
#endif

               break;

            case 'H':                   /* hdf input & format */
               OptMsg(" case: H\n");
               if (argc <= 0)
               {
                  optErr("Missing data for -H option");
               }
               argc--; argv++;
               printf(" hdfname %s\n", *argv);
               proc_hdf(*argv);
               break;

            case 'o':                   /* show all dekko/stace hooks */
               OptMsg(" case: o\n");
               g_oopt = 1;
               break;

            case 'j':                   /* strace2 hdf */
               OptMsg(" case: j\n");
               gv.jhdf = 1;
               if (argc <= 0)
               {
                  optErr("Missing data for -j option");
               }
               argc--; argv++;
               printf(" hdfname %s\n", *argv);
               proc_jhdf(*argv);
               break;

            case 'p':                   /* partial log option */
               /* -p stp OR -p stt stp */
               OptMsg(" case: p\n");
               if (argc <= 0)
               {
                  optErr("Missing data for -p option");
               }
               argc--; argv++;

               i = atoi(*argv);         /* 0 => all */
               if (i > 0)
               {
                  gv.hkstp = i;         /* 1 parm => 1 to stop */
                  if (argc > 1)
                  {
                     t = *(argv + 1);
                     if (t[0] != '-')
                     {                  /* 2 parms => start to stop */
                        if (argc <= 0)
                        {
                           optErr("Missing data for -p option");
                        }
                        argc--; argv++;
                        gv.hkstt = gv.hkstp;
                        gv.hkstp = atoi(*argv);
                     }
                  }
               }
               fprintf(stderr," case: p : (%d/%d)\n", gv.hkstt, gv.hkstp);

               break;

            case 'P':                   /* parent list */
               OptMsg(" case: P\n");
               if (argc <= 0)
               {
                  optErr("Missing data for -P option");
               }
               argc--; argv++;
               if ( (plist = fopen(*argv, "r") ) == NULL)
               {
                  fprintf(stderr, " input.c : can't open : %s\n", *argv);
                  exit(1);
               }
               break;

            case 'r':                   /* output scaling */
               OptMsg(" case: r\n");
               if (argc <= 0)
               {
                  optErr("Missing data for -r option");
               }
               argc--; argv++;
               if ( **argv < '0' || **argv >'9' )
               {
                  optErr("Numeric data required for -r option");
               }
               gv.raw = atoi(*argv);
               break;

            case 'R':                   /* reverse tree node order */
               OptMsg(" case: R\n");
               gv.rev = 1;
               break;

            case 't':                   /* type - default to PARX/nrm */
               OptMsg(" case: t\n");
               if (argc <= 0)
               {
                  optErr("Missing data for -t option");
               }
               argc--; argv++;
               topt = 1;

               /* bin read jraw.c */
               if (strcmp(*argv, "javaosbin") == 0)
               {
                  gv.ftype = 0;
               }
               else if (strcmp(*argv, "nrm") == 0)
               {
                  gv.ftype = -1;
               }
               else if (strcmp(*argv, "aixtrc") == 0)
               {
                  gv.tags = 1;          /* handles IJCBN stuff */
                  gv.ftype = -2;
               }
               else if (strcmp(*argv, "jbytecodes") == 0)
               {
                  gv.ftype = 1;
                  memflag = 0;
               }
               else if (strcmp(*argv, "jheapcalls") == 0)
               {
                  gv.ftype = 1;
                  memflag = 1;
               }
               else if (strcmp(*argv, "jheapbytes") == 0)
               {
                  gv.ftype = 1;
                  memflag = 2;
               }
               else if (strcmp(*argv, "javascript") == 0)
               {
                  gv.ftype = 2;
               }
               else if (strcmp(*argv, "plumber") == 0)
               {
                  gv.ftype = 3;
                  if (argc <= 0)
                  {
                     optErr("Missing data for -t option");
                  }
                  argc--; argv++;
                  gv.ptype = atoi(*argv);
               }
               else if (strcmp(*argv, "jctree") == 0)
               {
                  gv.ftype = 5;
               }
               else if (strcmp(*argv, "javaos") == 0)
               {
                  gv.ftype = 7;
               }
               else if (strcmp(*argv, "javar_stks") == 0)
               {
                  gv.ftype = 8;
               }
               else if (strcmp(*argv, "javar_stks2") == 0)
               {
                  gv.ftype = 9;
               }
               else if (strcmp(*argv, "jra_heapcalls") == 0)
               {
                  gv.ftype = 1;
                  memflag = 3;
               }
               else if (strcmp(*argv, "jra_heapbytes") == 0)
               {
                  gv.ftype = 1;
                  memflag = 4;
               }
               else if (strcmp(*argv, "jrf_heapcalls") == 0)
               {
                  gv.ftype = 1;
                  memflag = 5;
               }
               else if (strcmp(*argv, "jrf_heapbytes") == 0)
               {
                  gv.ftype = 1;
                  memflag = 6;
               }
               else if (strcmp(*argv, "jaix") == 0)
               {
                  /* java aix jit ascii */
                  gv.ftype = 10;
               }
               else if (strcmp(*argv, "jaixjit") == 0)
               {
                  gv.ftype = 11;        /* java aix jit binary */
               }
               else if (strcmp(*argv, "javaos_tpstks") == 0)
               {
                  gv.ftype = 12;        /* javaos tprof stacks */
               }
               else if (strcmp(*argv, "xtree_sub") == 0)
               {
                  /* xbtree sub: 1-2 */
                  gv.ftype = 13;
               }
               else if (strcmp(*argv, "xtree_add") == 0)
               {
                  /* xbtree add: 1+2+.. */
                  gv.ftype = 14;
               }
               else if (strcmp(*argv, "xtree_min") == 0)
               {
                  /* xbtree add: 1+2+.. */
                  gv.ftype = 27;
                  gv.xmin = 1;
               }
               else if (strcmp(*argv, "mtree") == 0)
               {
                  gv.ftype = 15;        /* input mtree (bytecodes) */
               }
               else if (strcmp(*argv, "mtree_cycs") == 0)
               {
                  gv.ftype = 16;        /* input mtree (cycles) */
               }

               else if (strcmp(*argv, "generic") == 0)
               {
                  gv.ftype = 6;
               }
               else if (strcmp(*argv, "genericIJ") == 0)
               {
                  gv.ftype = 17;
               }
               else if (strcmp(*argv, "genericMP") == 0)
               {
                  gv.ftype = 18;
               }
               else if (strcmp(*argv, "genericDel") == 0)
               {
                  gv.ftype = 19;
               }
               else if (strcmp(*argv, "genericL") == 0)
               {
                  gv.ftype = 20;        /* locks */
                  if (argc <= 0)
                  {
                     optErr("Missing data for -t option");
                  }
                  argc--; argv++;
                  g_lopt = atoi(*argv); /* 1 - t
                            2 - m */
               }
               else if (strcmp(*argv, "mtreeN") == 0)
               {
                  gv.ftype = 21;
                  if (argc <= 0)
                  {
                     optErr("Missing data for -t option");
                  }
                  argc--; argv++;
                  treeOpt(*argv);
               }
               else if (strcmp(*argv, "mtreeO") == 0)
               {
                  gv.ftype = 24;
                  if (argc <= 0)
                  {
                     optErr("Missing data for -t option");
                  }
                  argc--; argv++;
                  treeOpt(*argv);
               }
               else if (strcmp(*argv, "allocStacks") == 0)
               {
                  gv.ftype = 22;
               }
               else if (strcmp(*argv, "generic2") == 0)
               {
                  gv.ftype = 23;
               }
               else if (strcmp(*argv, "genericN") == 0)
               {
                  gv.ftype = 25;
                  if (argc <= 0)
                  {
                     optErr("Missing data for -t option");
                  }
                  argc--; argv++;
                  g_lopt = atoi(*argv); /* No of generic metrics */

               }
               else if (strcmp(*argv, "genericMod") == 0)
               {
                  /* barrier level */
                  gv.ftype = 26;
                  gv.mod = 1;           /* use mod for popping control */
               }
               else if (strcmp(*argv, "genericWAS") == 0)
               {
                  gv.ftype = 28;        // MJD generic trace for WAS
               }
               else if (strcmp(*argv, "ctrace") == 0)
               {
                  gv.ftype = 100;
               }
               else if (strcmp(*argv, "itrace") == 0)
               {
                  gv.ftype = 101;
               }
               else
               {
                  optErr("Bad -t Option");
               }
               break;

            case 'u':                   /* units */
               OptMsg(" case: u (units)\n");
               if (argc <= 0)
               {
                  optErr("Missing data for -u option");
               }
               argc--; argv++;
               gv.uflag = 1;
               gv.Units = Remember(*argv);
               gv.ustr  = Remember(*argv);
               break;

            case 'w':                   /* wall/elapsed */
               OptMsg(" case: w\n");
               gv.wflag = 1;
               break;

//          case '?':                   /* help option */
//             OptMsg(" case: ?\n");
//             Usage();
//             exit(0);
//             break;

            default:
               OptMsg(" case: default\n");
               fprintf(stderr, "input.c: Bad option %c\n", *s);
               fprintf(stderr, "Enter \"arcf -help\" for valid options\n");
               // Usage();
               exit(1);
            }
         }
      }                                 /* if '-' */
   }


   if ( 0 == fopt )
   {
      optErr("Input file must be specified with -F option");
   }

   if ( 0 == topt )
   {
      if ( strstr( nrmfile, "log-rt") )
      {
         gv.ftype = 21;

         g_lopt = 0;
         g_ostr = 0;    // Default for log-rt* is -t mtreeN <first-base>
      }
      else
      {
         optErr("Input type must be specified with -t option");
      }
   }
}

void Usage()
{
   //hdr(stderr);

   fprintf(stderr, "usage: arcf [options] \n");
   fprintf(stderr, "\n");

   fprintf(stderr, "\t -F filename : input file (REQUIRED)\n");
   fprintf(stderr, "\t -verbose    : (display all optional messages)\n");
   fprintf(stderr, "\t -dbgmsg     : (display debug messages)\n");
   fprintf(stderr, "\t -t type     : type processing for -F file\n");
   fprintf(stderr, "\t               (default is -t mtreeN BASE for *log-rt*)\n");
   fprintf(stderr, "\t    ASCII FORMATS\n");
   fprintf(stderr, "\t      mtreeN [x|FT]\n");
   fprintf(stderr, "\t          x  - Any field in log-rt LV record\n");
   fprintf(stderr, "\t          FT - Metric is supplied by FROM / TO\n");
   fprintf(stderr, "\t               records. See -FW option\n");
   fprintf(stderr, "\t          \n");
   fprintf(stderr, "\t      mtreeO [AO|AB|LO|LB]\n");
   fprintf(stderr, "\t        Object_Classes -> Leaf_Methods\n");
   fprintf(stderr, "\t          AO   - Allocated Objects\n");
   fprintf(stderr, "\t          AB   - Allocated Bytes\n");
   fprintf(stderr, "\t          LO   - Live Objects\n");
   fprintf(stderr, "\t          LB   - Live Bytes\n");

   fprintf(stderr, "\t      xtree_sub\n");
   fprintf(stderr, "\t        -F f1 -F f2 => f1 - f2\n");
   fprintf(stderr, "\t      xtree_add\n");
   fprintf(stderr, "\t        -F f1 -F f2 => f1 + f2\n");
   fprintf(stderr, "\t      xtree_min\n");
   fprintf(stderr, "\t        -F f1 -F f2 => min(f1, f2)\n");

   fprintf(stderr, "\t      generic    - absolute time\n");
   fprintf(stderr, "\t      generic2   - RTARCF log-gen reduction\n");
   fprintf(stderr, "\t      genericDel - delta time\n");
   fprintf(stderr, "\t      genericMod - modnm used for popping\n");
   fprintf(stderr, "\t      genericIJ  - I:mnm matchs J:mnm\n");
   fprintf(stderr, "\t      genericWAS - TOD pid pName action class method \n");   /* MJD generic WAS */
   fprintf(stderr, "\t      itrace     - itrace reduction\n");
   fprintf(stderr, "\t      ctrace     - ctrace reduction\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "\t    BINARY FORMATS \n");
   fprintf(stderr, "\t      nrm(dekko/strace)\n");
   fprintf(stderr, "\t      aixtrc\n\n");

   fprintf(stderr, "\n");
   fprintf(stderr, "\t Auxillary Options\n");
   fprintf(stderr, "\t -f   : flow\n");
   fprintf(stderr, "\t -c   : condensed flow\n");
   fprintf(stderr, "\t -r k : add scaled raw base & cum to xfiles (base/k) (cum/k),\n");
   fprintf(stderr, "\t        Default is 1, use -r 0 to disable\n");
   fprintf(stderr, "\t -u units  : units string\n");

   fprintf(stderr, "\t -acpu  : append CPU number to *ALL* records in x-files when -f itrace.\n");
   fprintf(stderr, "\t          With -acpu lines looks like:\n");
   fprintf(stderr, "\t              \"0 - > _IO_file_overflow_internal:/lib/libc-2.4.so_0_6\"\n");
   fprintf(stderr, "\t              (\"0\" is the CPL (ring) and \"6\" the CPU number)\n");
   fprintf(stderr, "\t          Without -acpu the CPU number is not appended to the line.\n");
   fprintf(stderr, "\t -ptcpu : append CPU number *ONLY* to \"pidtid\" records in x-files\n");
   fprintf(stderr, "\t          when -f itrace. With -ptcpu the \"pidtid\" line looks like:\n");
   fprintf(stderr, "\t              \"0  pidtid 1700_1700_itrace1pid_3\"\n");
   fprintf(stderr, "\t              (where the trailing \"3\" is the CPU number)\n");
   fprintf(stderr, "\t          Without -ptcpu the CPU number is not appended to the line.\n");
   fprintf(stderr, "\t -fcpu ## : filter on given CPU number when -f itrace\n");
   fprintf(stderr, "\t -fthr pid_tid : filter on given pid_tid (thread) when -f itrace\n");
   fprintf(stderr, "\t                 pid_tid must be hex as given in arc file\n");
   fprintf(stderr, "\t -gdisp : group xflow by dispatches per CPU when -f itrace\n");
   fprintf(stderr, "\t          The default is to interleave the xflow with events\n");
   fprintf(stderr, "\t          as they occur in the arc file. With -gdisp all events\n");
   fprintf(stderr, "\t          that occur between thread dispatches in a CPU are\n");
   fprintf(stderr, "\t          grouped together. The flow is no longer a true flow,\n");
   fprintf(stderr, "\t          but more a flow within a dispatching interval.\n");
   fprintf(stderr, "\t          * -gdisp implies -ptcpu.\n");

   fprintf(stderr, "\t -prune p : prune xtree & xarc \n");
   fprintf(stderr, "\t            delete if cum < p%%\n");

   fprintf(stderr, "\t -prunesize n : prune xprof\n");
   fprintf(stderr, "\t                output only top n, default 25\n");

   fprintf(stderr, "\t -sobjs : mtreeO support\n");
   fprintf(stderr, "\t          add size to Object_Classes\n");

   fprintf(stderr, "\t -p [stt] stp : stt(start) stp(stop) control\n");
   fprintf(stderr, "\t                            \n");
   fprintf(stderr, "\t -tsort : sort xtree children by cum\n");
   fprintf(stderr, "\t -e   : Modifies generic mode error handling\n");
   fprintf(stderr, "\t -a   : Embed generic raw data in xflow\n");
   fprintf(stderr, "\t -abs : Input values are absolute, not deltas\n");
   fprintf(stderr, "\t -oldheader : use old header style on output files\n");
   fprintf(stderr, "\t -html : Add html tags to output files\n");

   fprintf(stderr, "\n");
   fprintf(stderr, "\t Adding Parent & Child trees to xarc\n");
   fprintf(stderr, "\t  -pdep n : depth of Parent Trees - n  (def:0)\n");
   fprintf(stderr, "\t  -cdep n : depth of Child Trees  - n  (def:0)\n");
   fprintf(stderr, "\t  -ptclip n : Create Par Tree if  > n%% (def:1%%)\n");
   fprintf(stderr, "\t  -ctclip n : Create Chd Tree if  > n%% (def:1%%)\n");
   fprintf(stderr, "\t  -snms : short java names for Parent/Child trees\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "\t -FW file  : Where File. FROM/TO Dispatch info\n");
   fprintf(stderr, "\n");


   fprintf(stderr, "BPM&C Performance Tools, Austin, TX\n\n");

   /***** end of options *****/

   /* DEPRECATED profiling options
      fprintf(stderr, "\t -P pfile       : profile control\n");
      fprintf(stderr, "\t    pfile contents(column 1)\n");
      fprintf(stderr, "\t      H : Profile all Hooks & PidTids\n");
      fprintf(stderr, "\t      P : Profile all PidTids\n");
      fprintf(stderr, "\t      B : Reverse Profile all Hooks & PidTid\n");
      fprintf(stderr, "\t      h hookname : Profile & Reverse Profile 1 hook\n");
      fprintf(stderr, "\t      p pidtid   : Profile 1 PidTid\n");
   */

   /* DEPRECATED ascii input formats
      fprintf(stderr, "\t      jbytecodes \n");
      fprintf(stderr, "\t      jheapcalls jheapbytes \n");
      fprintf(stderr, "\t      jra_heapcalls jra_heapbytes \n");
      fprintf(stderr, "\t      jrf_heapcalls jrf_heapbytes\n");
      fprintf(stderr, "\t      javascript jctree\n");
      fprintf(stderr, "\t      javaos\n");
      fprintf(stderr, "\t      jaix jaixjit\n");
      fprintf(stderr, "\t      javar_stks javar_stks2\n");
      fprintf(stderr, "\t      javaos_tpstks\n");
      fprintf(stderr, "\t      plumber n\n");
      fprintf(stderr, "\t      genericMP - MultiProcessor arcf\n");
   */
}

/************************************************************* */
void hdr(FILE * fd)
{
   fprintf( fd, "\n ARCFLOW %02x.%02x.%02x\n\n",
            VERSION, RELEASE, SPIN);

   fprintf(fd, "  (Built    : %s)\n\n", BUILDDATE);
   fprintf(fd, "  (Source   : %s)\n\n", SOURCEDATE);
   fprintf(fd, "  (Platform : %s)\n\n", _PLATFORM_STR);
}
