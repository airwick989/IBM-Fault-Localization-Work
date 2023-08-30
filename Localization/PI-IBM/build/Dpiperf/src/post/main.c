
#include "encore.h"
#include "a2n.h"
#include "sdate.h"

#include "post.h"
#include "bputil.h"

/**********************************/
void Ver_Bld_Plat(FILE * fd)
{
   fprintf(fd, "\n");
   fprintf(fd, " POST - Version 7.7.3\n");
   fprintf(fd, "  Itrace & Tprof Post Processor\n\n");
   fprintf(fd, "   (Built    : %s)\n\n", BUILDDATE);
   fprintf(fd, "   (Source   : %s)\n\n", SOURCEDATE);
   fprintf(fd, "   (Platform : %s)\n\n", _PLATFORM_STR);
}

/**********************************/
void usage(void)
{
   Ver_Bld_Plat(stderr);

   fprintf(stderr, "   Common\n\n");
   fprintf(stderr, "     -verbose    : (display all optional messages)\n");
   fprintf(stderr, "     -verbmsg    : (display all optional messages to stderr)\n");
   fprintf(stderr, "     -mask=xxxx  : (debugging mask, implies -verbmsg)\n");
   fprintf(stderr, "     -r nrm2     : nrm2 file (def: swtrace.nrm2)\n");
   fprintf(stderr, "     -show       : ascii dump of nrm2 data (post.show)\n");
   fprintf(stderr, "     -showx      : -show, but no other processing\n");
   fprintf(stderr, "     -p  stp     : process records [1, stp]\n");
   fprintf(stderr, "     -pp stt stp : process records [stt, stp]\n");
   fprintf(stderr, "     -?          : Help\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "   A2N\n\n");
   fprintf(stderr, "    See A2N documentation for further details\n");
#if defined(_LINUX)
   fprintf(stderr, "     -k kpath    : kernel file (for symbols)\n");
   fprintf(stderr, "                  (def: /usr/src/linux/vmlinux)\n");
   fprintf(stderr, "     -kmap kpath : kernel map (for symbols)\n");
   fprintf(stderr, "     -kas        : use /proc/kallsyms (for kernel symbols)\n");
   fprintf(stderr, "                   and /proc/modules (for kernel modules symbols)\n");
#endif
   fprintf(stderr, "     -nv         : Don't validate Symbols \n");
   fprintf(stderr, "     -sympath p  : Alternate symbols path list\n");
   fprintf(stderr, "     -lmrg fnm   : Use merge file\n");
   fprintf(stderr, "                   (create merge file with 'genmsi')\n");
   fprintf(stderr, "     -jdir [path/]pref : Location & prefix of jprof jita2n & jtnm files\n");
   fprintf(stderr, "                   (def: log)\n");
   fprintf(stderr, "              o jprof 'jita2n' option creates jita2n files\n");
   fprintf(stderr, "              o jprof 'threadinfo' option creates jtnm files\n");
   fprintf(stderr, "              o jprof 'fnm=xxx' option relocates jita2n & jtnm files \n");
   fprintf(stderr, "     -nsf        : Return \"NoSymbols\" or \"NoSymbolFound\", instead of\n");
   fprintf(stderr, "                   a section name, when there are no symbols (NoSymbols) or\n");
   fprintf(stderr, "                   when there are symbols but an address can't be resolved\n");
   fprintf(stderr, "                   to a symbol (NoSymbolFound).\n");
   fprintf(stderr, "     -jita2nmsg  : show jita2n processing verbose messages\n");
   fprintf(stderr, "     -anysym     : harvest any available symbols (e.g. exports)\n");
   fprintf(stderr, "     -a2ndbg     : a2n debug mode on \n");
   fprintf(stderr, "     -a2nrdup    : a2n rename duplicate symbols \n");
   fprintf(stderr, "     -a2nmmi NM  : collapse Interpreter & rename as NM\n");
   fprintf(stderr, "     -a2nrng Modnm Stt Stp Rnm\n");
   fprintf(stderr, "              o Collapse symbols between 'Stt' & 'Stp' in module Modnm.\n");
   fprintf(stderr, "              o Rename the range to 'Rnm'\n");
   fprintf(stderr, "     -a2nrfn FNM : Use the file FNM to collapse symbol ranges\n");
   fprintf(stderr, "              o Each file line has 4 parms as for -a2nrng option\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "   tprof\n\n");
   fprintf(stderr, "     -aixtrc fnm : aix ascii trace file\n");
   //fprintf(stderr, "     -clip pc   : clip %% (def .1%%)\n");
   fprintf(stderr, "     -clip pc   : clip %% (def 0)\n");
   fprintf(stderr, "     -pnm       : report by process name (pnm)\n");
   fprintf(stderr, "                  (def: pid_pnm)\n");
   fprintf(stderr, "     -tnm       : report by thread name (tnm)\n");
   fprintf(stderr, "                  (def: tid_tnm)\n");
   fprintf(stderr, "     -cpu       : add _cpu to process name\n");
   fprintf(stderr, "     -rsel v    : select subset of def reports\n");
   fprintf(stderr, "                  (P PM PMS PT PTM PTMS M MS)\n");
   fprintf(stderr, "                  v = xxxxxxxx, where x is 0 or 1\n");
   fprintf(stderr, "                  e.g. -rsel 10100111\n");
   fprintf(stderr, "     -x xf      : 1 extra report per line in xf\n");
   fprintf(stderr, "                  e.g. 31 => MOD_PID report\n");
   fprintf(stderr, "     -off       : Add Mod_Sym_Offset report\n");
   fprintf(stderr, "     -disasm    : Add Mod_Sym_Offset report with disassembly\n");
   fprintf(stderr, "     -twin b e  : tprof windowing (b->e)\n");
   fprintf(stderr, "     -tpsel n   : select which tprof hooks to use\n");
   fprintf(stderr, "     -itsel n   : select a metric-field from itrace hook\n");
   fprintf(stderr, "     -tmod      : tag MODULE with 32b/64b id\n");
   fprintf(stderr, "     -d n       : Additional debug output\n");
   fprintf(stderr, "                  2) show major-minor per hook\n");
   fprintf(stderr, "                  3) cf details in gd.arc\n");
   fprintf(stderr, "                  4) embed nrm2 in gd.arc\n");
   fprintf(stderr, "                  5) +debug to post.show\n");
   fprintf(stderr, "                  6) show tprof addresses => stderr\n");
   fprintf(stderr, "                  7) use lr for tprof\n");
#if defined(_LINUX)
   fprintf(stderr, "     -compat    : Do not print the tprof mode line in tprof.out\n");
#endif
   fprintf(stderr, "\n");
   fprintf(stderr, "   ITrace\n\n");
   fprintf(stderr, "     Use jprof jinsts option. Includes jitted code in jita2n file.\n");
   fprintf(stderr, "     -arc        : Create arc file (subroutine flow)\n");
   fprintf(stderr, "         reduction:\n");
   fprintf(stderr, "            arcf -F arc -t itrace -r 1\n");
   fprintf(stderr, "     -c            : compact arc (1 line / subroutine)\n");
   fprintf(stderr, "     -int3         : process between 1st & 2nd int3 hks\n");
   fprintf(stderr, "     -ss           : Show individual instructions\n");
   fprintf(stderr, "     -ssi          : Show individual instructions w opcode\n");
   fprintf(stderr, "     -m            : ITrace only (ignore tprof hooks)\n");
   fprintf(stderr, "                     (def: tprof)\n");
   fprintf(stderr, "     -ck           : Compact consecutive kernel trace records into one\n");
   fprintf(stderr, "     -cp           : Compact calls to <plt>\n");
   fprintf(stderr, "     -dcnt         : Use disassembly count instead of trace count when available\n");
   fprintf(stderr, "\n");

   exit(-1);
}

/**********************************/
void parmerr(char * s)
{
   fprintf(stderr, " Missing Parm for %s\n\n", s);
   usage();
}

/**********************************/
void parmerr2(char * s1, char * s2)
{
   fprintf(stderr, " Bad Option Combo : %s %s\n\n", s1, s2);
   usage();
}

/**********************************/
void mlegend(void)
{
   FILE * fd = gd.micro;

   fprintf(fd, "\n");
   fprintf(fd, "\n Legend for tprof.micro\n");
   fprintf(fd,   " ======================\n\n");
   fprintf(fd, "   ### MOD  Ticks  Ticks%%  ModuleName\n");
   fprintf(fd, "   ### Sym  Ticks  Ticks%%  SymbolName  StartAddr  Size\n");
   fprintf(fd, "   F: SourceFileName\n");
   fprintf(fd, "   T: Offset(Hex)  LineNo  Ticks\n");
   fprintf(fd, "   C: CodeAddr Code\n");
   fprintf(fd, "\n\n");
}

/**********************************/
void arc_hdr(void)
{
   fprintf(gd.arc, "\n");
   fprintf(gd.arc, "# arc Field Definition:\n");
   fprintf(gd.arc, "#   1: cpu no.\n");
   fprintf(gd.arc, "#   2: K(kernel) or U(user)\n");
   fprintf(gd.arc, "#   3: last instruction type\n");
   fprintf(gd.arc, "#     0=INTRPT, 1=CALL,  2=RETURN, 3=JUMP,\n");
   fprintf(gd.arc, "#     4=IRET,   5=OTHER, 6=UNDEFD, 7=ALLOC\n");
   fprintf(gd.arc, "#   4: no. of instructions\n");
   fprintf(gd.arc, "#   5: @|? = Call_Flow | pid_tid\n");
   fprintf(gd.arc, "#   6: offset (from symbol start)\n");
   fprintf(gd.arc, "#   7: symbol:module\n");
   fprintf(gd.arc, "#   8: pid_tid_pidname_[threadname]\n");
   fprintf(gd.arc, "#   9: last instruction type (string)\n");
   fprintf(gd.arc, "#  10: line number (if available)\n");
   fprintf(gd.arc, "\n");
}

/**********************************/
char * get_current_directory(void)
{
   char *p = NULL;
   char buf[4096];

#if defined(_LINUX) || defined(_ZOS) || defined(_AIX)
   if (NULL == getcwd(buf, 4096))
   {
      ErrVMsgLog("getcwd failed\n");
      exit(-1);
   }
   strcat(buf, "/");
#elif defined(_WINDOWS)
   GetCurrentDirectory(4096, buf);
   strcat(buf, "\\");
#endif
   p = xStrdup("get_current_directory",buf);

   return( p );
}

/**********************************/
int main (int argc, char * argv[])
{
   char * nrmnm, * arcnm, * tpout, * tpaddr;
   char defn[]      = "swtrace.nrm2";
   char defa[]      = "arc";
   char deftp[]     = "tprof.out";
   char deftpaddr[] = "tprof.addr";
   char * a2nrfn = "";
   char * a2nrng[4] = { NULL};


   int64  nstt  = 0;
   int64  nstp  = INT64_MAX;

   char *p;

   initGV();

   gc.msg     = xopen("post.msg", "w");

   Ver_Bld_Plat(stdout);
   Ver_Bld_Plat(gc.msg);

   gv.clip = .001;                      // 0.1% tick filter
   gv.clip = 0;                         // tick filter
#if defined(BUILD_STAP)
   gv.stap = 1;                         // stap filtering required
#endif

   nrmnm   = defn;
   arcnm   = defa;
   tpout   = deftp;
   tpaddr  = deftpaddr;

   /* parse options */
   if (argc >= 2)
   {
      int  i;

      for (i = 1; i < argc; i++)        // Pre-parse debugging options
      {
         if (strcmp(argv[i], "-verbose") == 0)
         {
            gc.verbose |= gc_verbose_stderr;
         }

         else if (strcmp(argv[i], "-verbmsg") == 0)
         {
            gc.verbose |= gc_verbose_logmsg;
         }

         else if (strncmp(argv[i], "-mask=", 6) == 0)
         {
            gc.mask     = strtoul(argv[i]+6, NULL, 16);
            gc.verbose |= gc_verbose_logmsg;

            OptVMsgLog( "Parsed: -mask=0x%08X\n", gc.mask );
         }
      }

      // i=0 -> (argc - 1)
      for (i = 1; i < argc; i++)
      {
         if (argv[i][0] == '-')
         {                              /* option */
            if (strcmp(argv[i], "-verbose") == 0)
            {
               ;                        //pre-parsed
            }

            else if (strcmp(argv[i], "-verbmsg") == 0)
            {
               ;                        //pre-parsed
            }

            else if (strncmp(argv[i], "-mask=", 6) == 0)
            {
               ;                        //pre-parsed
            }

            else if (strcmp(argv[i], "-tpsel") == 0)
            {
               i++;
               if (i >= argc) parmerr("-tpsel");
               gv.tpsel = atoi(argv[i]);   /* which tprof hooks */
            }

            else if (strcmp(argv[i], "-undup") == 0)
            {
               gv.undup = 1;
            }

            else if (strcmp(argv[i], "-oprof") == 0)
            {
               gv.oprof = 1;
            }

            else if (strcmp(argv[i], "-itsel") == 0)
            {
               i++;
               if (i >= argc) parmerr("-itsel");
               gv.itsel = atoi(argv[i]);   /* which itrace MM field */
            }

            else if (strcmp(argv[i], "-tmod") == 0)
            {
               gv.tmod = 1;             // tag module with (32bit) (64bit)
            }


            else if (strcmp(argv[i], "-r") == 0)
            {
               i++;
               if (i >= argc) parmerr("-r");
               nrmnm = argv[i];         /* nrm2 file */
            }

            else if (strcmp(argv[i], "-anysym") == 0)
            {
               a2nanysym = 1;
            }

            // a2n debug
            else if (strcmp(argv[i], "-a2ndbg") == 0)
            {
               gv.a2ndbg = 1;
            }

            // show jitted method processing
            else if (strcmp(argv[i], "-jita2nmsg") == 0)
            {
               jita2nmsg = 1;
            }

            // a2n rename duplicate symbols
            else if (strcmp(argv[i], "-a2nrdup") == 0)
            {
               gv.a2nrdup = 1;
            }

            // test jit a2n without nrm2
            else if (strcmp(argv[i], "-a2ntest") == 0)
            {
               a2ntestflag = 1;
            }

            // a2n range collapse file
            else if (strcmp(argv[i], "-a2nrfn") == 0)
            {
               i++;
               if (i >= argc) parmerr("-a2nrfn");
               a2nrfn = xStrdup("-a2nrfn",argv[i]);   // a2n range file
               A2nSetRangeDescFilename(a2nrfn);
            }

            else if (strcmp(argv[i], "-a2nmmi") == 0)
            {
               i++;
               if (i >= argc) parmerr("-a2nmmi");
               fprintf(gc.msg, " mmi %s\n", argv[i]);
               A2nSetCollapseMMIRange(argv[i]);
            }

            else if (strcmp(argv[i], "-a2nrng") == 0)
            {
               i++;
               if (i >= argc) parmerr("-a2nrng");
               a2nrng[0] = xStrdup("-a2nrng",argv[i]);
               i++;
               if (i >= argc) parmerr("-a2nrng");
               a2nrng[1] = xStrdup("-a2nrng",argv[i]);
               i++;
               if (i >= argc) parmerr("-a2nrng");
               a2nrng[2] = xStrdup("-a2nrng",argv[i]);
               i++;
               if (i >= argc) parmerr("-a2nrng");
               a2nrng[3] = xStrdup("-a2nrng",argv[i]);

               fprintf(gc.msg, " %s %s:%s %s\n",
                       a2nrng[0], a2nrng[1], a2nrng[2], a2nrng[3]);
               fflush(gc.msg);

               A2nCollapseSymbolRange(
                                     a2nrng[0], a2nrng[1], a2nrng[2], a2nrng[3]);
            }

            else if (strcmp(argv[i], "-noskew") == 0)
            {
               gv.noskew = 1;
            }
            else if (strcmp(argv[i], "-sympath") == 0)
            {
               i++;
               if (i >= argc) parmerr("-sympath");
               gv.sympath = xStrdup("-sympath",argv[i]);
            }

#ifdef _AIX
            // aix ascii trace
            else if (strcmp(argv[i], "-aixtrc") == 0)
            {
               gv.aixtrc = 1;
               i++;
               if (i >= argc) parmerr("-aixtrc");
               trcInit(argv[i]);
            }
#endif

            // show ascii dump of nrm2
            else if (strcmp(argv[i], "-show") == 0)
            {
               gd.show = xopen("post.show", "w");
            }

            // force oldjit for comparison
            else if (strcmp(argv[i], "-oldjit") == 0)
            {
               gv.oldjit = 1;
            }

            // show ascii dump of nrm2, no other processing
            else if (strcmp(argv[i], "-showx") == 0)
            {
               gd.show = xopen("post.show", "w");
               gv.showx = 1;
            }

            // show ascii dump of nrm2, no other processing
            // add raw file data
            else if (strcmp(argv[i], "-showxx") == 0)
            {
               gd.show = xopen("post.show", "w");
               gv.showx = 2;
            }

            else if (strcmp(argv[i], "-clip") == 0)
            {
               i++;
               if (i >= argc) parmerr("-clip");
               if (argv[i][0] == '-')
               {
                  parmerr2(" -clip", argv[i]);
               }
               gv.clip = .01 * atof(argv[i]);   /* clip % */
            }

            else if (strcmp(argv[i], "-c") == 0)
            {
               gv.compact |= POST_COMPACT_FUNCTION;   /* compact arc file */
            }
            else if (strcmp(argv[i], "-ck") == 0)
            {
               gv.compact |= POST_COMPACT_KERNEL;
            }
            else if (strcmp(argv[i], "-cp") == 0)
            {
               gv.compact |= POST_COMPACT_PLT;
            }
            else if (strcmp(argv[i], "-dcnt") == 0)
            {
               gv.use_disasm_count = 1;
            }

            else if (strcmp(argv[i], "-k") == 0)
            {
               i++;
               if (i >= argc) parmerr("-k");
               knmptr = xStrdup("-k",argv[i]);   /* kernel file */
               knew |= 0x1;
            }
            else if (strcmp(argv[i], "-kmap") == 0)
            {
               i++;
               if (i >= argc) parmerr("-kmap");
               kmap = xStrdup("-kmap",argv[i]);   /* kernel map file */
               knew |= 0x2;
            }
            else if (strcmp(argv[i], "-kas") == 0)
            {
               knew |= 0x4;             /* use kallsyms and modules files */
               kas_off = kas_size = kmd_off = kmd_size = 0;
            }
            else if (strcmp(argv[i], "-nv") == 0)
            {
               gv.nv = 1;               /* dont validate symbols */
            }
            else if (strcmp(argv[i], "-nsf") == 0)
            {
               a2n_nsf = 1;             /* NoSymbols/NoSymbolFound instead of section */
            }
            else if ( strcmp(argv[i], "-off") == 0
                      || strcmp(argv[i], "-disasm") == 0 )
            {
               if ( 0 == gv.off )
               {
                  gv.off = 1;           /* Mod_Sym_Offset report */
                  gd.micro = xopen("tprof.micro", "w");   /* micro reports */
                  mlegend();
                  gv.micro = 1;         /* micro reports */
               }
               if ( strcmp(argv[i], "-disasm") == 0 )
               {
                  gv.disasm = 1;
               }
            }
            else if (strcmp(argv[i], "-compat") == 0)
            {
               i++;
               gv.tprof_compat = 1;
            }
            else if (strcmp(argv[i], "-x") == 0)
            {
               i++;
               if (i >= argc) parmerr("-x");
               gd.xfd = xopen(argv[i], "r");   /* extra reports */
               fprintf(gc.msg, " x option : %s\n", argv[i]);
            }
            else if (strcmp(argv[i], "-jdir") == 0)
            {
               i++;
               if (i >= argc) parmerr("-jdir");
               jitpre = xStrdup("-jdir",argv[i]);   /* jita2n file(s) prefix */
            }
            else if (strcmp(argv[i], "-lmrg") == 0)
            {
               i++;
               if (i >= argc) parmerr("-lmrg");
               mrgfnm = xStrdup("-lmrg",argv[i]);   /* merge file name */
            }
            else if (strcmp(argv[i], "-ss") == 0)
            {
               gv.showss = 1;
            }
            else if (strcmp(argv[i], "-ssi") == 0)
            {
               gv.showss = 2;
            }
            else if (strcmp(argv[i], "-cpu") == 0)
            {
               gv.cpu = 1;
            }
            else if (strcmp(argv[i], "-nt") == 0)
            {
               gv.ntonly = 1;
            }
            else if (strcmp(argv[i], "-nospace") == 0)
            {
               gv.nospace = 1;
            }
            else if (strcmp(argv[i], "-arc") == 0)
            {
               gv.arc = 1;
            }
            else if (strcmp(argv[i], "-rsel") == 0)
            {
               i++;
               if (i >= argc) parmerr("-rsel");
               gv.rsel = 1;
               gv.rselvec = argv[i];
            }
            else if (strcmp(argv[i], "-pnm") == 0)
            {
               gv.pnmonly = 1;
            }
            else if (strcmp(argv[i], "-tnm") == 0)
            {
               gv.tnmonly = 1;
            }
            else if (strcmp(argv[i], "-int3") == 0)
            {
               int3f = 0;
               aout  = 0;
               fprintf(gc.msg, " Using int3: aout = %d\n", aout);
            }
            else if (strcmp(argv[i], "-d") == 0)
            {
               i++;
               if (i >= argc) parmerr("-d");
               gv.db = atoi(argv[i]);
               fprintf(stderr, " debug %d\n", gv.db);
            }
            else if (strcmp(argv[i], "-p") == 0)
            {
               i++;
               if (i >= argc) parmerr("-p");
               nstp = atoll(argv[i]);   /* 1st n recs of raw */
               fprintf(gc.msg, " nstp = %"_L64"d\n", nstp);
            }
            else if (strcmp(argv[i], "-m") == 0)
            {
               gv.mtrace = 1;           // mtrace
               fprintf(gc.msg, " -m option(mtrace)\n");
            }
            else if (strcmp(argv[i], "-pp") == 0)
            {
               i++;
               if (i >= argc) parmerr("-pp");
               nstt = atoll(argv[i]);   /* from */
               i++;
               if (i >= argc) parmerr("-pp");
               nstp = atoll(argv[i]);   /* to */
               fprintf(gc.msg, " nstt = %"_L64"d\n", nstt);
               fprintf(gc.msg, " nstp = %"_L64"d\n", nstp);
            }
            else if (strcmp(argv[i], "-twin") == 0)
            {
               // tprof reduction between tstt -> tstp
               i++;
               if (i >= argc) parmerr("-twin");
               gv.tstt = atoll(argv[i]);   /* from */
               i++;
               if (i >= argc) parmerr("-twin");
               gv.tstp = atoll(argv[i]);   /* to */
               fprintf(gc.msg, " tstt = %"_L64"d\n", gv.tstt);
               fprintf(gc.msg, " tstp = %"_L64d"\n", gv.tstp);
            }
            else if (argv[i][1] == '?')
            {
               usage();
            }
            else
            {
               fprintf(stderr, "\n INVALID OPTION: <%s>\n", argv[i]);
               usage();
            }
         }
         else
         {
            fprintf(stderr, "\n INVALID OPTION: <%s>\n", argv[i]);
            usage();
         }
      }
   }

   OptVMsg("gv.compact 0x%x\n", gv.compact);
   OptVMsg(" aft options\n");

   if (gv.off)
   {
      gv.clip = 0.0;
   }

   gf_nrm2 = nrmnm;

#if defined(BUILD_STAP)
   if (gv.stap)
   {
      int fd;

      strcpy(tmp_nrm2, "/tmp/stap_cvtXXXXXX");
      fd = mkstemp(tmp_nrm2);
      if (!tmp_nrm2[0])
      {
         fprintf(stderr, " ERROR unable to create temp nrm2 file\n");
         exit(1);
      }
      if (!stap_to_nrm2(nrmnm, tmp_nrm2))
      {
         exit(1);
      }
      gf_nrm2 = tmp_nrm2;
   }
#endif

   if (jitpre == NULL)
   {
#if !defined(_WINDOWS)
      jitpre = default_jita2n_file_prefix;
#else
      char * p;
      char tbuf[MAX_PATH];

      p = getenv("IBMPERF_JPROF_LOG_FILES_PATH");
      if (p != NULL)
      {
         strcpy(tbuf, p);
         strcat(tbuf, "\\");
         p = getenv("IBMPERF_JPROF_LOG_FILES_NAME_PREFIX");
         if (p != NULL)
         {
            strcat(tbuf, p);
         }
         else
         {
            strcat(tbuf, default_jita2n_file_prefix);
         }
         jitpre = xStrdup("prefix",tbuf);
      }
      else
      {
         jitpre = default_jita2n_file_prefix;
      }
#endif
   }

   // open files & init A2N
   if (gv.showx == 0)
   {                                    // showx=1 => no processing
      if (gv.arc)
      {
         gd.arc = xopen(arcnm,   "w");
         arc_hdr();
      }
//    gd.ptree = xopen("ptree", "w");

      if (initSymUtil() != 0)
      {
         fprintf(stderr, "ERROR initing SymUtil\n");
         exit(1);
      }
   }

   // Open Files
   if (gv.aixtrc != 1)
   {
      if (init_raw_file(gf_nrm2) != 0)
      {
         fprintf(stderr, " Error opening %s\n", gf_nrm2);
         exit(1);
      }
   }

   if (gv.noskew == 1)
   {
      noskew();
   }

   // tprof. init tree
   root = initTree();

   // nrm2 structure
   tr = (trace_t *)xMalloc("main",sizeof(trace_t));

   // READ TRACE FILE ( *nrm2 or aixtrace )
   if (gv.aixtrc != 1)
   {
      readnrm2(nstt, nstp);
   }
#ifdef _AIX
   else
   {
      readAixTrc();
   }
#endif



   // a2n quality
   if (necnt1 + necnt2 >= 1)
   {
      OptVMsg(" %4d %s\n",
              necnt1, " UnExpected Brk. Prev Hk Callflow");
      OptVMsg(" %4d %s\n",
              necnt2, "   Expected Brk. Prev Hk Other");
   }

   if (a2nrc[0] >= 1)
   {
      char s[6][8] =
      {"Good", "NoSymFd", "NoSyms", "NoMod", "+++", "---"};
      int i;

      OptVMsg("\n %8s %s\n", "", "A2N-Quality");
      for (i = 0; i <= 5; i++)
      {
         if (a2nrc[i] >= 1)
         {
            OptVMsg(" %8d %s\n", a2nrc[i], s[i]);
         }
      }
   }

   OptVMsg(" END OF TRACE. Start of Reports\n");

   if ( ( (gv.mtrace == 0) && (hmaj[0x10] >= 1) )
        || gv.aixtrc == 1 )
   {                                    // tprof
      gd.tp     = xopen("tprof.out", "w");
      output();
   }

   if ( 0 != ( p = get_current_directory() ) )
   {
      WriteFlush(stdout, "\nOutput files placed in directory \"%s\".\n", p);
      xFreeStr(p);
   }

   //if(gv.ptree == 1) {
   //   xtreeOut("ptree");
   //}

   hashStats( &gv.thread_table );
   hashStats( &gv.process_table );

   logAllocStats();

   return(0);
}
