/*
 * IBM Performance Inspector
 * Copyright (c) International Business Machines Corp., 1996 - 2008
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 *------------------------------- Change History -------------------------------
 *
 * Date      Version  Description
 *------------------------------------------------------------------------------
 * 07/28/00  1.01     Initial Coding
 * 09/01/00  1.02     Added start delay and run time
 * 01/19/01  1.03     Added ! escape sequence in command mode
 * 03/11/01  1.04     Port to AIX (from NT version)
 * 03/13/01  1.05     - Fix so AIX version does connection retry
 *                    - Change escape characters to '-' and '/'
 *                    - Port to OS/2
 * 03/29/01  1.06     Port to Linux
 * 02/22/02  1.07     Port to Windows IA64 and Linux PPC32
 * 04/10/02  1.08     - Don't use '-' for escape anymore
 *                    - Allow user to specify escape character
 *                    - Added jlmdump, mondump, jlm, jlmc and esc
 * 04/19/02  1.09     Add jlmdumpr, startfull, startlite
 * 05/06/02  1.10     Add s390 MVS support
 * 03/06/03  1.11     Add s390x, linux IA64/PPC64
 * 03/24/03  1.12     Add -f, -js and -jsl
 * 07/17/03  1.13     Add -jd, -rs, -rd and -rds
 * 07/18/03  1.14     Add -c (to send arbitrary commands) and -ta
 * 08/05/03  1.15     Fix -c syntax and error messages
 * 08/05/03  1.16     Add -cx
 * 11/17/03  1.17     Add (fix) z/OS
 * 02/04/04  1.18     Add Windows AMD64
 * 03/09/04  1.19     Set return code on exit (if errors)
 * 04/02/04  1.20     Add broadcast (-b) option, removed OS/2 support
 * 05/14/04  1.21     Fix sleep() on z/O
 * 10/08/04  1.22     Ignore escape command - send everything along
 * 12/07/04  1.23     - Handle "quit/exit" so that JVM also exits
 *                    - Add "shutdown" to exit both JVM and rtdriver
 *                    - Add "exit_rtdriver" to exit rtdriver only
 *                    - Add "hdf/heapdump=5"
 *                    - Document "off"
 * 08/18/05  1.24     Fixed "jdr" (jlm dump and reset)
 * 08/31/05  1.25     Added "ping" command, -ack option
 * 03/22/06  1.26     Increase command line size to 128
 * 04/12/06  1.27     Increase command line size to 260
 * 04/18/06  1.28     Increase command line size to 512
 * 04/24/06  1.29     - Always receive ACK from jprof
 *                    - Remove -ack option
 *                    - Prefix all commands to jprof with a comma
 * 06/02/06  1.30     No ASCII<->EBCDIC conversion when sending commands
 * 07/11/06  1.31     Minor cleanup of help text
 * 09/16/06  1.32     Remove trailing blanks from user commands
 * 09/26/06  1.33     Remove trailing blanks *correctly* from user commands
 * 09/28/06  1.34     - Remove "exit", "quit" and "stop" user commands.
 *                    - Add -np option.
 *                    - Add -shutdown_stop option (for backward compatibility)
 * 04/05/07  1.35     Deprecate the following options:
 *                      -s <#sec>, -js <#sec>, -jsl <#sec>, -f #sec,
 *                      -jd #sec, -rs #sec, -r #sec, -rd #sec, -rds #sec
 *                    - Clean up documentation
 *                    - Add -i option
 * 05/18/07  1.36     Run at higher priority
 * 03/11/08  1.37     - Timestamp commands/responses to/from jprof
 *                    - Handle command completion responses
 * 03/14/08  1.38     - Add support for query_pid command
 *                    - Handle new command completion responses
 * 03/25/08  1.39     Removed "off"
 * 05/30/08  1.40.0   Changed version format
 * 07/17/08  1.41.0   New help syntax and more passthru to JPROF
 * 02/03/09  2.00.0   piClient mode: pure passthru to JPROF with messages as ACK
 * 09/25/09  2.00.1   Add "xx" as alias for "exit_rtdriver"
 * 12/03/09  2.00.2   Allow broadcast in piClient mode
 * 12/04/09  2.01     Check for multiple instances of JPROF
 * 08/12/10  2.02     - Withdrew support for the following options:
 *                        -s, -js, -jsl, -f, -jd, -rs, -r, -rd and -rds
 * 08/19/10  2.03     Add -load option
 * 09/03/10  2.04     Continue on to interactive mode with -load (unless -nc)
 *------------------------------------------------------------------------------
 */
#define me     "RTDriver"
#define ID_STR "** %s for %s: v2.04, 09/03/2010 **\n\n"


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#if defined(WINDOWS)                   /***** WINDOWS *****/
   #if !defined(WIN32)
      #define WIN32
   #endif
   #if defined(IA32) || defined(i386)
      #define tgt_platform  "Windows-x86"
   #elif defined(IA64)
      #define tgt_platform  "Windows-IA64"
      #if !defined(WIN64)
         #define WIN64
      #endif
   #elif defined(AMD64)
      #define tgt_platform  "Windows-x64"
      #if !defined(WIN64)
         #define WIN64
      #endif
   #else
      #error WINDOWS requires IA32/i386, IA64 or AMD64.
   #endif

   #include <windows.h>
   #include <conio.h>
   #include <fcntl.h>
   #include <sys\types.h>
   #include <sys\stat.h>
   #include <io.h>

   #define sleep  Sleep
   #define last_error   WSAGetLastError()

#elif defined(AIX)                     /***** AIX *****/
   #if defined(PPC32)
      #define tgt_platform  "AIX-PPC32"
   #elif defined(PPC64)
      #define tgt_platform  "AIX-PPC64"
   #else
      #error AIX requires PPC32 or PPC64.
   #endif

   #include <sys/types.h>
   #include <sys/socket.h>
   #include <sys/socketvar.h>
   #include <netinet/in.h>
   #include <netdb.h>
   #include <arpa/inet.h>

   #define closesocket  close
   #define stricmp   strcasecmp
   #define strnicmp  strncasecmp
   #define last_error   errno

   #define SOCKET_ERROR      -1
   #define INVALID_SOCKET    -1

#elif defined(LINUX)                   /***** LINUX *****/

   #if defined(CONFIG_X86)
      #define tgt_platform  "Linux-x86"
   #elif defined(CONFIG_IA64)
      #define tgt_platform  "Linux-IA64"
   #elif defined(CONFIG_X86_64)
      #define tgt_platform  "Linux-X86_64"
   #elif defined(CONFIG_PPC32)
      #define tgt_platform  "Linux-PPC32"
   #elif defined(CONFIG_PPC64)
      #define tgt_platform  "Linux-PPC64"
   #elif defined(CONFIG_S390)
      #define tgt_platform  "Linux-s390"
   #elif defined(CONFIG_S390X)
      #define tgt_platform  "Linux-s390x"
   #else
      #error LINUX requires IA32, IA64, AMD64, PPC32, PPC64, S390, or S390X.
   #endif

   #include <sys/types.h>
   #include <sys/socket.h>
   #include <sys/socketvar.h>
   #include <netinet/in.h>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <signal.h>
   #include <dirent.h>
   #include <dlfcn.h>

   #define closesocket  close
   #define stricmp   strcasecmp
   #define strnicmp  strncasecmp
   #define last_error   errno

   #define SOCKET_ERROR      -1
   #define INVALID_SOCKET    -1

#elif defined(ZOS)                     /***** ZOS *****/
   #define tgt_platform  "z/OS"

   #include <unistd.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <netdb.h>
   #include <arpa/inet.h>

   #define closesocket  close
   #define stricmp   strcasecmp
   #define strnicmp  strncasecmp
   #define last_error   errno

   #define SOCKET_ERROR      -1
   #define INVALID_SOCKET    -1

#else
   #error Not a supported platform.  Must be WINDOWS, AIX, LINUX, or ZOS.
#endif

#define INVALID_PORT        -1
#define DEFAULT_PORT      8888
#define MAX_PORTS            8
#define COMMAND_LEN        512
#define MAX_ARG_LEN         80
#define SHORT_HELP           0
#define LONG_HELP            1

int   sock[MAX_PORTS];
int   port[MAX_PORTS];

int   fName         = 0;                /* -n */
int   fAddr         = 0;                /* -a */
int   fLocal        = 0;                /* -l */
int   fPort         = 0;                /* -p */
int   fAbsoluteTime = 0;                /* -ta */
int   fBroadcast    = 0;                /* -b */
int   fCommaPrefix  = 1;                /* -np */
int   fInteractive  = 0;                /* -i */

char  szServerName[MAX_ARG_LEN];
char  szServerAddr[MAX_ARG_LEN];
char  szServerPort[MAX_ARG_LEN];

short iPort;

#define MAX_USER_COMMANDS  64
int cmd_cnt = 0;                        /* -c */
struct user_cmd {
   char * cmd;                          /* Command */
   int  cmd_len;                        /* Command length */
   int  cmd_delay;                      /* Command delay (in seconds) */
   int  cmd_exec;                       /* Execute command, instead of sending to host */
   int  cmd_sent;                       /* Command has already been sent */
} uc[MAX_USER_COMMANDS];

static void Help(int HelpType);
static void GetArgs(int argc, char * argv[]);
static void ErrorMessage(const char * from);
static void PromptForCommands(FILE * fp);
static int  SendCommand(char * str, int slen);
static void CloseConnections(void);
static void so(char * format, ...);
static void se(char * format, ...);
static void se_exit(int rc, char * format, ...);
static void se_close_exit(int rc, char * format, ...);
static void Delay(int seconds);
static void get_current_time(int * hh, int * mm, int * ss);
#if defined(LINUX)
int jpload(void);
#endif

void piClient(FILE * fp);

int  piPort[MAX_PORTS] = {0};           /* Ports   used by piClient */
int  piSock[MAX_PORTS] = {0};           /* Sockets used by piClient */
int  pihost_cnt = 0;

#define PI_MAX_INPUT 1024

/* piClient mode commands that don't need to be broadcast */
#define NUM_NO_BROADCAST_CMDS  5
char * no_broadcast_cmds[NUM_NO_BROADCAST_CMDS] = {
   "HELP",
   "ALLHELP",
   "JPROFHELP",
   "ALLJPROFHELP",
   "JPROFINFO",
};



/*
 * main()
 * ------
 */
int main(int argc, char * argv[])
{
   struct sockaddr_in serv_addr;
   struct hostent     * server;
   int                sockfd;
   int                rc, i, j;
   int                connected = 0;
   char               * p;
   int                hh, mm, ss;
#if defined(WINDOWS)
   WSADATA            wsdata;
   WORD               wVersionRequested;
#endif


   so(ID_STR, me, tgt_platform);        /* Say who we are */
   GetArgs(argc, argv);                 /* Parse command line */

#if defined(WINDOWS)
   /* Make sure we have the right version of winsock and initialize it. */
   wVersionRequested = MAKEWORD(1,1);
   if (WSAStartup(wVersionRequested, &wsdata))
      se_exit(last_error,"** %s: WSAStartup() error. rc = %d.\n", me, last_error);
#endif

   /* Initialize serv_addr structure */
   memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;

   /* Initialize sockets */
   for (i = 0; i < MAX_PORTS; i++) {
      sock[i] = INVALID_SOCKET;
      port[i] = INVALID_PORT;
   }

   /*
    * Do name resolution ...
    * The goal is to come up with the IP address of the host.
    *
    * When we're done we'll have either:
    * - fLocal: Use local host        (need to resolve name)
    * - fName:  Use given name        (resolve IP addr for messages)
    * - fAddr:  Use given IP address  (resolve host name for messages)
    */
   if (fLocal) {
      /* Get this machine's host name */
      rc = gethostname(szServerName, sizeof(szServerName));
      if (rc == SOCKET_ERROR)
         se_close_exit(last_error, "** %s: gethostname() error. rc = %d.\n", me, last_error);

      fName = 1;                        /* So we connect by name */
   }

   if (fName) {
      /* User gave us a name or told us to use local host */
      server = gethostbyname(szServerName);
      if (server == NULL)
         se_close_exit(last_error, "** %s: gethostbyname() error. rc = %d.\n", me, last_error);

	  if (server->h_length != 4)
         se_close_exit(last_error, "** %s: unexpected IP address, not IPv4() error. rc = %d.\n", me, last_error);
      serv_addr.sin_addr.s_addr = *((unsigned int *)server->h_addr_list[0]);

      p = inet_ntoa(serv_addr.sin_addr);
      strcpy(szServerAddr, p);
   }
   else {
      /* User gave us an IP address */
      serv_addr.sin_addr.s_addr = inet_addr(szServerAddr);
      server = gethostbyaddr((char *)&serv_addr.sin_addr.s_addr, sizeof(serv_addr.sin_addr.s_addr), AF_INET);
      if (server == NULL)
         se_close_exit(last_error, "** %s: gethostbyaddr() error. rc = %d.\n", me, last_error);

      strcpy(szServerName, server->h_name);
   }

   /*
    * Initialize port number.
    * It will be either the default port (and we try all 4) or * the given port only.
    */
   if (fPort)
      iPort = (short)atoi(szServerPort);
   else
      iPort = DEFAULT_PORT;

   /*
    * Try to connect to the host ...
    * We attempt to connect on ports 8888 thru 8891.
    */
#if defined(LINUX)
ConnectionRetry:
#endif
   j = 0;
   for (i = 0; i < MAX_PORTS; i++) {
      get_current_time(&hh, &mm, &ss);

      so("** %s: %02d:%02d:%02d connecting to %s (%s) on port %d ... ", me, hh, mm, ss, szServerName, szServerAddr, iPort);

      sockfd = (int)socket(AF_INET, SOCK_STREAM, 0);
      if (sockfd == INVALID_SOCKET)
         se_close_exit(last_error, "** %s: socket() error. rc = %d.\n", me, last_error);

      serv_addr.sin_port = htons(iPort);

      rc = connect(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));

      if (SOCKET_ERROR == rc) {         /* Connect failed.  Keep trying if possible ... */
         closesocket(sockfd);

         if (fPort) {
            se_close_exit(last_error, "\n** %s: Failed to establish connection on port %d. Quitting.\n", me, iPort);
         }
         so("\n");
      }
      else {                            /* Connected. Remember socket and port */
         connected++;
         port[j] = iPort;
         sock[j] = sockfd;
         so("OK\n");

         if (!fBroadcast)
            break;                      /* Single connection found */
      }

      iPort++;
      j++;
   }

   if (!connected) {                    /* No ports connected */
      se_close_exit(last_error, "\n** %s: Failed to establish any connections. Quitting.\n", me);
   }

#if defined(WINDOWS)
   /* Raise priority to, maybe, improve responsiveness */
   if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
      se("** %s: SetPriorityClass() error %d. Continuing at normal priority.\n", me, GetLastError());
   }
   else {
      if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL)) {
         se("** %s: SetThreadPriority() error %d. Continuing at normal priority.\n", me, GetLastError());
      }
   }
#endif

   /*
    * Send user commands if requested
    */
   if (cmd_cnt) {
      if (fAbsoluteTime) {
         int  sent, last_time, next_wait, smallest_delay;

         last_time = 0;
         sent = 0;

         /* Send all the commands with no time delays */
         for (i = 0; i < cmd_cnt; i++) {
            if (uc[i].cmd_delay == 0) {
               uc[i].cmd_sent = 1;
               sent++;
               if (uc[i].cmd_exec == 1) {
                  so("** %s: Executing '%s' user command ...\n", me, uc[i].cmd);
                  rc = system(uc[i].cmd);
                  so("** %s: User command rc = %d.\n", me, rc);
               }
               else {
                  rc = SendCommand(uc[i].cmd, uc[i].cmd_len);
                  if (rc == SOCKET_ERROR)
                     se_close_exit(last_error, "** %s: send() for '%s' user command failed. rc = %d.\n", me, uc[i].cmd, last_error);
               }
            }
         }

         if (sent == cmd_cnt) {
            /* Everything was sent */
            CloseConnections();
            goto TheEnd;
         }

         while (sent != cmd_cnt) {
            /* Find smallest delay time left */
            smallest_delay = 0x7FFFFFFF;
            for (i = 0; i < cmd_cnt; i++) {
               if (!uc[i].cmd_sent) {
                  if (uc[i].cmd_delay < smallest_delay)
                     smallest_delay = uc[i].cmd_delay;
               }
            }

            next_wait = smallest_delay - last_time;
            last_time = smallest_delay;
            so("** %s: Waiting %d seconds to send next user command ...\n", me, next_wait);
            Delay(next_wait);

            /* Send commands */
            for (i = 0; i < cmd_cnt; i++) {
               if (!uc[i].cmd_sent && uc[i].cmd_delay == last_time) {
                  uc[i].cmd_sent = 1;
                  sent++;
                  if (uc[i].cmd_exec == 1) {
                     so("** %s: Executing '%s' user command ...\n", me, uc[i].cmd);
                     rc = system(uc[i].cmd);
                     so("** %s: User command rc = %d.\n", me, rc);
                  }
                  else {
                     rc = SendCommand(uc[i].cmd, uc[i].cmd_len);
                     if (rc == SOCKET_ERROR)
                        se_close_exit(last_error, "** %s: send() for '%s' user command failed. rc = %d.\n", me, uc[i].cmd, last_error);
                  }
               }
            }
         }
      }
      else {
         /* Commands are relative to the previous one */
         for (i = 0; i < cmd_cnt; i++) {
            if (uc[i].cmd_delay != 0) {
               so("** %s: Waiting to send '%s' user command in %d seconds ...\n", me, uc[i].cmd, uc[i].cmd_delay);
               Delay(uc[i].cmd_delay);
            }

            if (uc[i].cmd_exec == 1) {
               so("** %s: Executing '%s' user command ...\n", me, uc[i].cmd);
               rc = system(uc[i].cmd);
               so("** %s: User command rc = %d.\n", me, rc);
            }
            else {
               rc = SendCommand(uc[i].cmd, uc[i].cmd_len);
               if (rc == SOCKET_ERROR)
                  se_close_exit(last_error, "** %s: send() for '%s' user command failed. rc = %d.\n", me, uc[i].cmd, last_error);
            }
         }
      }

      if (!fInteractive) {
         CloseConnections();
         goto TheEnd;
      }
   }

   if (pihost_cnt) {
      piClient(stdin);
   }
   else {
      /* Go into interactive command session */
      so("** %s: Entering command mode ...\n\n", me);
      PromptForCommands(stdin);
   }
   CloseConnections();

   TheEnd:
#if defined(WINDOWS)
   WSACleanup();
#endif
   return(0);
}


/*
 * SendCommand()
 * -------------
 */
int SendCommand(char * str, int slen)
{
   int  i;
   int  rc = 0;
   char ack_buffer[80];
   char cmd_buffer[COMMAND_LEN];
   int  cmd_len;
   int  hh, mm, ss;

   /*
    * Prefix a comma to the command(s). This will cause JProf to
    * handle commands correctly in cases where it takes too long
    * to process a command before receiving another one (and both
    * commands getting concatenated together into an unrecognizable
    * new command).
    */
   if (fCommaPrefix) {
      cmd_buffer[0] = ',';
      strcpy(cmd_buffer+1, str);
      cmd_len = slen + 1;
   }
   else {
      strcpy(cmd_buffer, str);
      cmd_len = slen;
   }

   for (i = 0; i < MAX_PORTS; i++) {
      if (sock[i] != INVALID_SOCKET) {
         if (strcmp("\x0E\x0F\x0E\x0F", str) == 0) {  /* Initial handshaking for piClient, SO-SI-SO-SI */
            strcpy(cmd_buffer, str);
            rc = send(sock[i], cmd_buffer, cmd_len, MSG_DONTROUTE);
            if (SOCKET_ERROR == rc)
               break;

            rc = recv(sock[i], ack_buffer, 1, 0);
            if (SOCKET_ERROR == rc)
               break;

            get_current_time(&hh, &mm, &ss);

            if (strncmp(ack_buffer, "K", 1) == 0) {
               so( "** %s: %02d:%02d:%02d piClient handshaking completed successfully by host on port %d\n",
                   me, hh, mm, ss, port[i] );

               piPort[ pihost_cnt ] = port[i];
               piSock[ pihost_cnt ] = sock[i];
               pihost_cnt++;
            }
            else {
               se( "** %s: %02d:%02d:%02d Duplicate piClient rejected by host on port %d.\n",
                   me, hh, mm, ss, port[i] );

               se_close_exit(last_error, "** %s: %02d:%02d:%02d Specify ports when restarting JPROF and RTDRIVER.\n",
                              me, hh, mm, ss );
            }
         }
         else {
            get_current_time(&hh, &mm, &ss);
            so("** %s: %02d:%02d:%02d Sending '%s' command on port %d ...\n", me, hh, mm, ss, str, port[i]);

            rc = send(sock[i], cmd_buffer, cmd_len, MSG_DONTROUTE);
            if (rc == SOCKET_ERROR)
               break;

            /* Receive acknowledgement that jprof completed the command */
            rc = recv(sock[i], ack_buffer, sizeof(ack_buffer), 0);
            if (rc == SOCKET_ERROR) {
               if (stricmp(str, "shutdown") != 0) {
                  /* Socket may already be gone. Ignore error. */
                  so("** %s: ***** rc = SOCKET ERROR on recv() for completion ACK *****\n", me);
               }
               break;
            }

            get_current_time(&hh, &mm, &ss);
            if (strncmp(ack_buffer, "K", 1) == 0) {
               so("** %s: %02d:%02d:%02d Command '%s' completed successfully by host on port %d ...\n", me, hh, mm, ss, str, port[i]);
            }
            else if (strncmp(ack_buffer, "N", 1) == 0) {
               so("** %s: %02d:%02d:%02d Command '%s' completed *unsuccessfully* by host on port %d ...\n", me, hh, mm, ss, str, port[i]);
            }
            else {
               if (strcmp(str, "query_pid") == 0)
                  so("** %s: %02d:%02d:%02d PID = %s", me, hh, mm, ss, ack_buffer);
               else
                  so("** %s: %02d:%02d:%02d Invalid completion response to command '%s' on port %d ...\n", me, hh, mm, ss, str, port[i]);
            }

            if (!fBroadcast)
               break;                   /* No broadcast - stop of first send */
         }
      }
   }

   return (rc);
}


#if defined(LINUX)
/*
 * valid_pid()
 * -----------
 */
int valid_pid(pid_t pid)
{
   DIR * pdir;
   char name[64];

   sprintf(name, "/proc/%u", (uint32_t)pid);
   pdir = opendir(name);
   if (pdir) {
      closedir(pdir);
      return (1);
   }
   else {
      return (0);
   }
}
#endif


/*
 * GetArgs()
 * ---------
 *
 * See syntax diagram below ...
 *
 */
static void GetArgs(int argc, char * argv[])
{
   int i, j;

   if (argc == 1) {
      Help(SHORT_HELP);
      exit (-1);
   }

   for (i = 1; i < argc; i++) {
      if (stricmp(argv[i], "-n") == 0) {
         if (fName)
            se_exit(-1, "*ERROR* Can only specify the -n option once.\n");

         fName = 1;
         if (argv[++i])                 /* Server name */
            strcpy(szServerName, argv[i]);
         else
            se_exit(-1, "*ERROR* Must specify a name with -n option.\n");
      }
      else if (stricmp(argv[i], "-a") == 0) {
         if (fAddr)
            se_exit(-1, "*ERROR* Can only specify the -a option once.\n");

         fAddr = 1;
         if (argv[++i])                 /* Server IP address */
            strcpy(szServerAddr, argv[i]);
         else
            se_exit(-1, "*ERROR* Must specify an IP address with -a option.\n");
      }
      else if (stricmp(argv[i], "-b") == 0) {
         fBroadcast = 1;                /* Broadcast to all JVMs */
      }
      else if (stricmp(argv[i], "-i") == 0) {
         fInteractive = 1;              /* Go interactive after commands */
      }
      else if (stricmp(argv[i], "-np") == 0) {
         fCommaPrefix = 0;              /* No comma prefix */
      }
      else if (stricmp(argv[i], "-l") == 0) {
         fLocal = 1;                    /* Server is Local machine */
      }
      else if (stricmp(argv[i], "-p") == 0) {
         if (fPort)
            se_exit(-1, "*ERROR* Can only specify the -p option once.\n");

         fPort = 1;
         if (argv[++i])                 /* Server port number */
            strcpy(szServerPort, argv[i]);
         else
            se_exit(-1, "*ERROR* Must specify a port number with -p option.\n");
      }
      else if (stricmp(argv[i], "-ta") == 0) {
         fAbsoluteTime = 1;             /* Absolute times */
      }
      else if (stricmp(argv[i], "-c") == 0  ||  stricmp(argv[i], "-cx") == 0) {
         if (cmd_cnt == MAX_USER_COMMANDS)
            se_exit(-1, "*ERROR* Too many user commands. Max allowed is %d.\n", cmd_cnt);

         if (stricmp(argv[i], "-cx") == 0)
            uc[cmd_cnt].cmd_exec = 1;

         if (argv[++i]) {               /* Command ? */
            if (isdigit((int)argv[i][0]))
               se_exit(-1, "*ERROR* Invalid -c option syntax. Must be: -c command <#sec>\n");

            uc[cmd_cnt].cmd = argv[i];
            uc[cmd_cnt].cmd_len = (int)strlen(argv[i]);
            uc[cmd_cnt].cmd_delay = 0;

            if (argv[++i]) {            /* Delay ? */
               if (isdigit((int)argv[i][0])) {
                  for (j = 1; j < (int)strlen(argv[i]); j++) {
                     if (!isdigit((int)argv[i][j]))
                        se_exit(-1, "*ERROR* Invalid time delay value with -c option. Must be a number.\n");
                  }

                  uc[cmd_cnt].cmd_delay = atoi(argv[i]);
                  if (uc[cmd_cnt].cmd_delay < 0)
                     se_exit(-1, "*ERROR* Invalid time delay value with -c option. Must be positive.\n");
               }
               else {
                  i--;                  /* Not a number. Assume other option */
               }
            }
            cmd_cnt++;
         }
         else {
            se_exit(-1, "*ERROR* Missing command with -c option.\n");
         }
      }
      else if ((stricmp(argv[i], "?") == 0) ||
               (stricmp(argv[i], "??") == 0) ||
               (stricmp(argv[i], "-?") == 0) ||
               (stricmp(argv[i], "-h") == 0) ||
               (stricmp(argv[i], "-help") == 0)) {
         Help(LONG_HELP);
         exit (-1);
      }
      else {
         se_exit(-1, "*ERROR* Invalid option %s\n", argv[i]);
      }
   }


   if (!fName && !fAddr && !fLocal)
      se_exit(-1, "*ERROR* Either -n, -a or -l must be specified.\n");

   if ((fName + fAddr + fLocal) > 1)
      se_exit(-1, "*ERROR* Only one of -n, -a or -l can be specified.\n");

   if (fPort && fBroadcast)
      se_exit(-1, "*ERROR* -p and -b are not allowed together.\n");

   return;
}


/*
 * CommandModeHelp()
 * -----------------
 */
void CommandModeHelp(void)
{
   so("\n  RTDriver commands\n");
   so("  -----------------\n");
   so("    ?|help            - Display this help (but you already knew that).\n");
   so("    shutdown          - Stop data collection, write data to file, terminate JVM\n");
   so("                        and exit RTDriver.\n");
   so("    exit_rtdriver|xx  - Close client connection and exit RTDriver.\n");
   so("                        JVM continues to run. You can achieve the same\n");
   so("                        result by pressing Ctrl-C or Ctrl-Break.\n");
   so("\n");
   so("  JProf commands\n");
   so("  --------------\n");
   so("    start<=#>         - Start collection. If '#', start after '#' seconds.\n");
   so("    flush             - Write data to file. Data collection continues.\n");
   so("    fgc               - Request immediate garbage collection.\n");
   so("    ping              - No-op command to see if JProf is accepting commands.\n");
   so("\n");
   so("  JLM commands\n");
   so("  ------------\n");
   so("    jlm|jlmstart      - Start JLM collection *WITH* hold time accounting.\n");
   so("    jlml|jlmlitestart - Start JLM collection *WITHOUT* hold time accounting.\n");
   so("    jreset|jlmreset   - Reset JLM.\n");
   so("    jstop|jlmstop     - Turn off JLM.\n");
   so("    jd|jlmdump        - Request JLM dump.\n");
   so("    jdr|jlmdumpr      - Request JLM dump and Reset JLM.\n");
   so("\n");
   so("  Notes:\n");
   so("  ------\n");
   so("    * JVM/RTDriver termination/exit commands.\n");
   so("      - Terminate JVM and exit RTDriver:\n");
   so("           \"shutdown\"\n");
   so("      - Leave JVM running and exit RTDriver:\n");
   so("           \"exit_rtdriver\"  -or-  Ctrl-C/Ctrl-Break\n");
   so("      - Turn off instrumentation, leave JVM running and exit RTDriver:\n");
   so("           \"reset\", followed by \"exit_rtdriver\"  -or-  Ctrl-C/Ctrl-Break\n");
   so("    * Any command other than the ones listed above will be sent, as-is,\n");
   so("      to the host (JProf or the JVM itself). See the JProf documentation\n");
   so("      for other valid commands.\n");
   so("    * If you want commands to be executed (by JProf) as close together as\n");
   so("      possible then enter the commands separated by comma without blank space\n");
   so("      between them. For example:\n");
   so("      - fgc,hd\n");
   so("        Will cause an FGC followed immediately by an HD.\n");
   so("      - fgc,jlmstart\n");
   so("        Will cause an FGC followed immediately by a JLMSTART.\n");
   so("\n\n");
   return;
}


#define NULL_ENTER_CMD      1
#define SEND_CMD            2
#define EXIT_CMD            4
#define QUIT_CMD            5
#define HELP_CMD            6
#define SEND_MULTIPLE_CMD   7
#define INVALID_CMD         -1
#define DEPRECATED_CMD      -2


static char * jdump      = "jlmdump";   /* jlm dump */
static char * jdumpreset = "jlmdumpr";  /* jlm dump and reset */
static char * jreset     = "jlmreset";  /* jlm reset */
static char * jlmoff     = "jlmstop";   /* turn off JLM */
static char * jlmcon     = "jlmstartlite";   /* turn on JLM without hold-time */
static char * jlmon      = "jlmstart";  /* turn on JLM including hold-time */


/*
 * strnlower()
 * --------------
 */
void strnlower(char * s, int len)
{
   for (len--; len >= 0; len--) {
      s[len] = (char)tolower(s[len]);
   }
   return;
}


/*
 * CheckCommand()
 * --------------
 */
int CheckCommand(int incnt,     char * incmd,
                 int * outcnt,  char ** outcmd,
                 int * outcnt2, char ** outcmd2)
{
   if (incnt <= 0)
      return(NULL_ENTER_CMD);

   if ( (incnt == 5 && (!strnicmp("start", incmd, 5) ||     /* start  */
                        !strnicmp("flush", incmd, 5) ||     /* flush  */
                        !strnicmp("reset", incmd, 5))) ||   /* reset  */
        (incnt >= 6 && !strnicmp("start=", incmd, 6)) ) {   /* start= */
      strnlower(incmd, incnt);
      *outcnt = incnt;
      *outcmd = incmd;
      return (SEND_CMD);
   }

   if ( (incnt == 2 && !strnicmp("jd", incmd, 2)) ||        /* jlmdump */
        (incnt == 5 && !strnicmp("jdump", incmd, 5)) ||
        (incnt == 7 && !strnicmp("jlmdump", incmd, 7)) ) {
      *outcnt = (int)strlen(jdump);
      *outcmd = jdump;
      return (SEND_CMD);
   }

   if ( (incnt == 3 && !strnicmp("jdr", incmd, 3)) ||       /* jlmdump & reset */
        (incnt == 6 && !strnicmp("jdumpr", incmd, 6)) ||
        (incnt == 8 && !strnicmp("jlmdumpr", incmd, 8)) )  {
      *outcnt  = (int)strlen(jdump);                        /* first command is dump */
      *outcmd  = jdump;
      *outcnt2 = (int)strlen(jreset);                       /* second command is reset */
      *outcmd2 = jreset;
      return (SEND_MULTIPLE_CMD);
   }

   if ( (incnt == 2 && !strnicmp("gc", incmd, 2)) ||        /* force GC */
        (incnt == 3 && !strnicmp("fgc", incmd, 3)) ||
        (incnt == 7 && !strnicmp("forcegc", incmd, 7)) ) {
      *outcnt = (int)strlen("fgc");
      *outcmd = "fgc";
      return (SEND_CMD);
   }

   if ( (incnt == 3 && !strnicmp("jlm", incmd, 3)) ||       /* jlm full on */
        (incnt == 8 && !strnicmp("jlmstart", incmd, 8)) ) {
      *outcnt = (int)strlen(jlmon);
      *outcmd = jlmon;
      return (SEND_CMD);
   }

   if ( (incnt ==  4 && !strnicmp("jlml", incmd, 4))  ||    /* jlm counters only on */
        (incnt ==  7 && !strnicmp("jlmlite", incmd, 7)) ||
        (incnt ==  9 && !strnicmp("jlmlstart", incmd, 9)) ||
        (incnt == 12 && !strnicmp("jlmlitestart", incmd, 12)) ) {
      *outcnt = (int)strlen(jlmcon);
      *outcmd = jlmcon;
      return (SEND_CMD);
   }

   if ( (incnt == 5 && !strnicmp("jstop", incmd, 5)) ||     /* jlm off */
        (incnt == 6 && !strnicmp("jlmoff", incmd, 6)) ||
        (incnt == 7 && !strnicmp("jlmstop", incmd, 7)) ) {
      *outcnt = (int)strlen(jlmoff);
      *outcmd = jlmoff;
      return (SEND_CMD);
   }

   if ( (incnt == 8 && !strnicmp("jlmreset", incmd, 8)) ||  /* jlm reset */
        (incnt == 6 && !strnicmp("jreset", incmd, 6)) ) {
      *outcnt = (int)strlen(jreset);
      *outcmd = jreset;
      return (SEND_CMD);
   }

   if (incnt == 8 && (!strnicmp("shutdown", incmd, 8))) {   /* shutdown JVM and exit rtdriver */
      return (QUIT_CMD);
   }

   if ( (incnt == 2 && !strnicmp("xx", incmd, 2)) ||        /* exit rtdriver only */
        (incnt == 13 && !strnicmp("exit_rtdriver", incmd, 13)) ) {
      return (EXIT_CMD);
   }

   if ( (incnt == 1 && !strnicmp("?", incmd, 1)) ||         /* help */
        (incnt == 4 && !strnicmp("help", incmd, 4)) ) {
      return (HELP_CMD);
   }

   strnlower(incmd, incnt);                                 /* unknown command. send as-is */
   *outcnt = (int)strlen(incmd);
   *outcmd = incmd;
   return (SEND_CMD);
}


/*
 * so()
 * ----
 */
static void so(char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stdout, format, argptr);
   fflush(stdout);
   va_end(argptr);
   return;
}


/*
 * se()
 * ----
 */
static void se(char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stderr, format, argptr);
   fflush(stderr);
   va_end(argptr);
   return;
}


/*
 * se_exit()
 * ---------
 */
static void se_exit(int rc, char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stderr, format, argptr);
   fflush(stderr);
   va_end(argptr);
   exit (rc);
}


/*
 * se_close_exit()
 * ---------------
 */
static void se_close_exit(int rc, char * format, ...)
{
   va_list  argptr;

   va_start(argptr, format);
   vfprintf(stderr, format, argptr);
   fflush(stderr);
   va_end(argptr);

   CloseConnections();

#if defined(WINDOWS)
   WSACleanup();
#endif

   exit (rc);
}


/*
 * PromptForCommands()
 * -------------------
 */
static void PromptForCommands(FILE * fp)
{
   int rc, rc1 = 0, rc2 = 0, i;
   int clientcnt;
   char clientline[COMMAND_LEN];
   int sendcnt, sendcnt2;
   char * sendline, * sendline2;

   so("\nCommand> ");

   while (fgets(clientline, COMMAND_LEN, fp) != NULL) {
      clientcnt = (int)strlen(clientline);
      clientline[--clientcnt] = 0;      /* Remove 'new line' and decrement char cnt */

      /* Remove trailing blanks */
      i = clientcnt - 1;
      while (i >= 0 && clientline[i] == ' ') {
         clientline[i] = 0;
         clientcnt--;
         i--;
      }

      if (clientcnt > 0) {
         rc = CheckCommand(clientcnt, clientline, &sendcnt, &sendline, &sendcnt2, &sendline2);
         if (rc == SEND_CMD) {
            rc1 = SendCommand(sendline, sendcnt);
            if (rc1 == SOCKET_ERROR) {
               so("** %s: send() error. rc = %d.\n", me, last_error);
               so("             You may want to retry the command.\n");
            }
         }
         else if (rc == SEND_MULTIPLE_CMD) {
            rc1 = SendCommand(sendline, sendcnt);
            if (rc1 == SOCKET_ERROR) {
               so("** %s: send() for '%s' command failed. rc = %d.\n", me, sendline, last_error);
            }
            else {
               rc2 = SendCommand(sendline2, sendcnt2);
               if (rc2 == SOCKET_ERROR)
                  so("** %s: send() for '%s' command failed. rc = %d.\n", me, sendline2, last_error);
            }
            if (rc1 == SOCKET_ERROR || rc2 == SOCKET_ERROR)
               so("             You may want to retry the '%s' command.\n", clientline);
         }
         else if (rc == QUIT_CMD) {     /* shutdown */
            rc1 = SendCommand("shutdown", 8);
            break;
         }
         else if (rc == EXIT_CMD) {     /* exit_rtdriver */
            break;
         }
         else if (rc == HELP_CMD) {
            CommandModeHelp();
         }
         else if (rc == DEPRECATED_CMD) {
            so("\n  ** The \"");

            if (strnicmp(clientline, "quit", 4) == 0)
               so("quit");
            else if (strnicmp(clientline, "exit", 4) == 0)
               so("exit");
            else
               so("stop");

            so("\" command is no longer supported. **\n");
            so("     - To terminate the JVM and exit RTDriver use \"shutdown\".\n");
            so("     - To leave the JVM running and exit RTDriver use \"exit_rtdriver\"\n");
            so("       or press Ctrl-C/Ctrl-Break.\n");
         }
         else {
            so("\n  ** Invalid command '%s' **\n", sendline);
         }
      }

      so("\nCommand> ");
   }
   return;
}


/*
 * CloseConnections()
 * ------------------
 */
static void CloseConnections(void)
{
   int i;
   int hh, mm, ss;

   for (i = 0; i < MAX_PORTS; i++) {
      if (sock[i] != INVALID_SOCKET) {
         get_current_time(&hh, &mm, &ss);
         so("** %s: %02d:%02d:%02d closing connection on port %d.\n", me, hh, mm, ss, port[i]);
         shutdown(sock[i], 2);
         closesocket(sock[i]);
      }
   }

   return;
}


/*
 * Delay()
 * -------
 */
static void Delay(int seconds)
{
#if defined(WINDOWS)
   seconds *= 1000;
#endif                                      /* Convert to milliseconds */
   sleep(seconds);
   return;
}


/*
 * get_current_time()
 * ------------------
 */
static void get_current_time(int * hh, int * mm, int * ss)
{
   time_t t;
   struct tm * ptm;

   time(&t);
   ptm = localtime(&t);
   if (hh) *hh = ptm->tm_hour;
   if (mm) *mm = ptm->tm_min;
   if (ss) *ss = ptm->tm_sec;
   return;
}


/*
 * Help()
 * ------
 */
static void Help(int HelpType)
{
   int i, LinesToPrint;
   char c;
#if defined(WINDOWS)
   HANDLE hStdout;
   CONSOLE_SCREEN_BUFFER_INFO csbi;
   BOOL  result;
   int Rows = 0;
#endif


   /*
    * This is the actual help text, line by line.
    * - Don't include \n
    * - Have a blank string to skip a line
    */
   char * HelpText[] = {
      "Usage:",
      "------",
      " ",
      "   JProf control:",
      "   **************",
      "      rtdriver {-n name | -a ipaddr | -l} [-p port | -b] [-np]",
      "                                          [-ta] [-i]",
      "                                          [[{-c | -cx} cmd [#sec]] ...]",
      " ",
      "               |                        | |                           |",
      "               +------------------------+ +---------------------------+",
      "                     one required                  optional",
      " ",
      "$##$",                           /* Marks end of short help. Not displayed for long help. */
      " ",
      "Where:",
      "------",
      "              ***** Target host options *****",
      "  -n name     Specifies the host name.",
      " ",
      "  -a ipaddr   Specifies the host IP address.",
      " ",
      "  -l          Specifies that Local machine is host.",
      " ",
      "  -p port     Specifies the host port number, in the range 8888 - 8895,",
      "              to connect to.",
      "              * Default port number is 8888.",
      " ",
      "  -b          Causes RTDriver to attempt to connect to all valid ports",
      "              (8888 - 8895) that accept a connection, and to broadcast all",
      "              commands to all ports to which a connection was established.",
      "              This allows simultaneous control of multiple JVMs.",
      "              * Default is to connect to single port and to not broadcast",
      "                commands.",
      " ",
      "  -np         Causes RTDriver to NOT prefix commands sent over the socket",
      "              with commas.",
      "              * Default is to prefix all commands with a comma before",
      "                sending them over the socket. JProf handles commas as",
      "                command separators thus allowing multiple commands to be",
      "                sent and processed correctly.",
      " ",
      "              ***** Automation options *****",
      "  -ta         Causes command delay values in user commands to be absolute",
      "              times from the time RTDriver is invoked.",
      "              * If not specified command delay values are relative to the",
      "                previous command (if any).",
      "              * Option is ignored if no delay values are given.",
      "              * -ta option is only allowed with -n, -a, -l, -p and -c.",
      "              * The -ta option *MUST* precede any -c/-cx options.",
      " ",
      "  -c  cmd [#sec]    Send 'cmd' to the host",
      "  -cx cmd [#sec]    Execute 'cmd'",
      "              Automatically send (-c) or execute (-cx) 'cmd' upon connecting",
      "              to host.",
      "              In description that follows the word \"send\" is used to mean",
      "              either 'send to host' (if the -c option is used) or 'execute'",
      "              (if the -cx option is used).",
      "              * If command contains blanks then enclose the entire command",
      "                in double quotes. For example:",
      "                   -c \"dumpheap type=3\"",
      "              * Command execution is sequential with commands sent in the",
      "                order in which they were entered (left to right).",
      "              * There is an implied delay (socket traffic overhead) between",
      "                commands. If you *DON'T* want that delay then specify all",
      "                the required commands, separated by commas, as one command.",
      "                - For example:",
      "                     -c fgc -c hd",
      "                - Is not the same as:",
      "                     -c fgc,hd",
      "                - \"-c fgc -c hd\" are sent as two commands and there is",
      "                  a delay (probably very small) between sending them.",
      "                - \"-c fgc,hd\" is sent as one command, which JProf splits",
      "                  into fgc followed by hd.",
      "              * Commands specified with the -cx option are executed using the",
      "                system() library function. RTDriver will not continue until",
      "                the command completes.",
      "              * If a delay value (#sec) is given, the command is sent:",
      "                - #sec seconds after connecting to host if -ta is specified.",
      "                - #sec seconds from previous command if -ta is not specified.",
      "              * If no delay value is given, the command is sent:",
      "                - Immediately after connecting to host if -ta is specified.",
      "                - Immediately after previous command if -ta is not specified.",
      "              * If using absolute time and more than one command specifies the",
      "                same delay time, the commands are executed as they appear in",
      "                the command line from left to right.",
      "              * Delay value is in seconds and must be a decimal number.",
      "              * -c/-cx option(s) are only allowed with -n, -a, -l, -p and -ta.",
      "              * Interactive command mode is never entered if -c or -cx is",
      "                specified.",
      "                - If you need to go into interactive mode then invoke",
      "                  RTDriver again without the -c or -cx options.",
      " ",
      "  -i          Causes interactive command mode to be entered *AFTER* the",
      "              last user command is sent.",
      "              * If not specified RTDriver exits after the last command is",
      "                sent.",
      "              * -i should be specified before any command (-c/-cx).",
      "              * This option is really only needed if, after sending the",
      "                given commands, you would like to enter interactive mode",
      "                to continue entering commands. If no commands are given",
      "                (no -c/-cx) then RTDriver will enter interactive command",
      "                mode anyway.",
      " ",
      " ",
      " ",
      "  Valid interactive commands (after connecting):",
      " ",
      "     RTDriver commands",
      "     -----------------",
      "       ?|help            - Display this help (but you already knew that).",
      "       shutdown          - Stop data collection, write data to file, terminate",
      "                           JVM and exit RTDriver.",
      "       exit_rtdriver     - Close client connection and exit RTDriver.",
      "                           JVM continues to run. You can achieve the same",
      "                           result by pressing Ctrl-C or Ctrl-Break.",
      " ",
      "     JProf commands",
      "     --------------",
      "       start[=#]         - Start collection. If '#', start after '#' seconds.",
      "       flush             - Write data to file. Data collection continues.",
      "       reset             - Resets data collection. Can restart with 'start'.",
      "       end               - Turns off all callflow events, waits for all pending",
      "                           events to complete, does 'flush' and then 'reset'.",
      "       fgc               - Request immediate garbage collection.",
      "       ping              - No-op command to see if JProf is accepting commands.",
      " ",
      "     JLM commands",
      "     ------------",
      "       jlm|jlmstart      - Start JLM collection *WITH* hold time accounting.",
      "       jlml|jlmlitestart - Start JLM collection *WITHOUT* hold time accounting.",
      "       jreset|jlmreset   - Reset JLM.",
      "       jstop|jlmstop     - Turn off JLM.",
      "       jd|jlmdump        - Request JLM dump.",
      "       jdr|jlmdumpr      - Request JLM dump and Reset JLM.",
      " ",
      "     Interactive command notes:",
      "     --------------------------",
      "       * JVM/RTDriver termination/exit commands.",
      "         - Terminate JVM and exit RTDriver:",
      "              \"shutdown\"",
      "         - Leave JVM running and exit RTDriver:",
      "              \"exit_rtdriver\"  -or-  Ctrl-C/Ctrl-Break",
      "         - Turn off instrumentation, leave JVM running and exit RTDriver:",
      "              \"reset\", followed by \"exit_rtdriver\"  -or-  Ctrl-C/Ctrl-Break",
      "       * Any command other than the ones listed above will be sent, as-is,",
      "         to the host (JProf or the JVM itself). See the JProf documentation",
      "         for other valid commands.",
      "       * If you want commands to be executed (by JProf) as close together as",
      "         possible then enter the commands separated by comma without blank",
      "         space between them. For example:",
      "         - fgc,hd",
      "           Will cause an FGC followed immediately by an HD.",
      "         - fgc,jlmstart",
      "           Will cause an FGC followed immediately by a JLMSTART.",
      " ",
      "Notes:",
      "------",
      "  * Either -n, -a or -l must be specified.",
      "  * -p and -b are optional and mutually exclusive options.",
      "  * If neither -p nor -b is specified RTDriver will attempt to connect to",
      "    host on ports 8888 thru 8895, stopping as soon as the first successful",
      "    connection is made.",
      "  * If -p is specified RTDriver will only attempt to connect to host on",
      "    the given port.",
      "  * If -b is specified RTDriver will attempt to connect to host on all",
      "    valid ports (8888 thru 8895). Commands will be sent (broadcast) to",
      "    all ports to which a successful connection was made.",
      "  * Interactive command mode is not entered if either -c or -cx is specified.",
      "    If you need to enter additional commands then just invoke RTDriver again.",
      "  * You can create fairly complex automatic command sequences by using the",
      "    -c and/or -cx options.",
      "  * Commands specified with the -cx option are executed using the system()",
      "    library function. RTDriver will not continue until the command completes.",
      "  * Using the wrong commands will not necessarily result in an error message",
      "    from RTDriver. However, the wrong command will be sent across the socket",
      "    to the server (either the JVM itself or JProf) and results will be",
      "    unpredictable.",
      "  * legacy vs. piClient interactive modes:",
      "    - In legacy interactive mode rtdriver sends commands to JProf and does",
      "      not wait for responses. There is no way to know if the command completed",
      "      succesfully or not.",
      "    - If using broadcast mode (-b option) the interactive mode used makes",
      "      a difference in how broadcast behaves:",
      "      * In legacy mode broadcast sends commands to all JProf instances as",
      "        quickly as possible. Very broadcast-like.",
      "      * In piClient mode broadcast sends a command to, and waits for a",
      "        response from, one JProf instance at a time. For long-running",
      "        commands, like 'flush' and 'end', it could cause considerable delays",
      "        between successive commands.",
      " ",
      "Examples:",
      "---------",
      "  1) rtdriver -l",
      "     Connect to local machine and enter interactive command mode.",
      " ",
      "  2) rtdriver -l -b",
      "     Connect to local machine on all ports which accept a connection",
      "     and enter interactive command mode. Interactive commands are sent",
      "     to all connected ports",
      " ",
      "  3) rtdriver -l -c jlmstart 10 -c jlmdump 50 -c jlmdump 10 -c jlmstop",
      "     Connect to local machine, wait 10 seconds, send a 'jlmstart' command,",
      "     wait 50 seconds, send a 'jlmdump' command, wait 10 seconds, send a",
      "     'jlmdump' command followed by a 'jlmstop' command.",
      "     **** Invocations 3 and 4 are functionally identical ****",
      " ",
      "  4) rtdriver -l -ta -c jlmstart 10 -c jlmdump 60 -c jlmdump 70 -c jlmstop 70",
      "     Connect to local machine, 10 seconds after connecting send a 'jlmstart'",
      "     command, 60 seconds after connecting send a 'jlmdump' command, 70",
      "     seconds after connecting send another 'jlmdump' command followed by a",
      "     'jlmstop' command.",
      "     **** Invocations 3 and 4 are functionally identical ****",
      " ",
      "  5) rtdriver -l -ta -i -c jlmstart 10 -c jlmdump 60 -c jlmdump 70",
      "     Connect to local machine, 10 seconds after connecting send a 'jlmstart'",
      "     command, 60 seconds after connecting send a 'jlmdump' command, 70",
      "     seconds after connecting send another 'jlmdump' command and then",
      "     enter interactive command mode.",
      " ",
      "  6) rtdriver -l -c start 60 -c flush 65 -c reset",
      "     Connect to local machine, 60 seconds after connecting send a 'start'",
      "     command, 65 seconds after that send a 'flush' command followed by a",
      "     'reset' command.",
      "     **** Invocations 6 and 7 are functionally identical ****",
      " ",
      "  7) rtdriver -l -ta -c start 60 -c flush 125 -c reset 125",
      "     Connect to local machine, 60 seconds after connecting send a 'start'",
      "     command, 125 seconds after connecting send a 'flush' command followed",
      "     by a 'reset' command.",
      "     **** Invocations 6 and 7 are functionally identical ****",
      " ",
      "  8) rtdriver -l -b -ta -c start 60 -c flush 125 -c reset 125",
      "     Connect to all ports (that accept a connection) on the local machine,",
      "     60 seconds after connecting broadcast a 'start' command on all ports,",
      "     125 seconds after connecting broadcast a 'flush' command followed",
      "     by a 'reset' command.",
      "     **** This command allows simultaneous control of more than 1 JVM ****",
      " ",
      "  9) rtdriver -n test -c start 300 -cx \"java test\" -c flush -c reset",
      "     Connect to machine 'test', 5 minutes (300 seconds) after connecting send",
      "     a 'start' command, immediately after that execute command \"java test\",",
      "     after that command completes send a 'flush' command followed by a",
      "     'reset' command.",
      " ",
      " 10) rtdriver -l -c start 30 -c fgc 10 -c flush -c reset",
      "     Connect to local machine, 30 seconds after connecting send",
      "     a 'start' command, 10 seconds after that send an 'fgc' command",
      "     followed by a 'flush' command followed by a 'reset' command.",
      " ",
      " 11) rtdriver -l -c start 30 -c fgc,flush,reset 10",
      "     This is very similar to invocation number 10 above except that",
      "     10 seconds after sending the 'start' command the commands 'fgc,flush,reset'",
      "     are sent as one and will be executed back-to-back-to-back by JProf.",
      " ",
   };


#if defined(WINDOWS)
   /*
    * Figure out max number of rows we can display without
    * the text scrolling off the top of the screen.
    */
   hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
   if (hStdout != INVALID_HANDLE_VALUE) {
      result = GetConsoleScreenBufferInfo(hStdout, &csbi);
      if (result)
         Rows = csbi.dwSize.Y - 3;      /* Will pause every this many lines */
      else
         Rows = 0;                      /* Will scroll right off the top */
   }
#endif

   /* Print out help text ... */
   LinesToPrint = sizeof(HelpText) / sizeof(char *);
   for (i = 0; i < LinesToPrint; i++) {
#if defined(WINDOWS)
      /* On Windows display one screen-full at a time */
      if (Rows != 0  && LinesToPrint > Rows) {
         if ((i > 0) && (i % Rows) == 0) {
            so("\n           ----- press Enter to continue -----\n");
            c = (char)getchar();
         }
      }
#endif

      if (strncmp(HelpText[i], "$##$", 4) == 0) {
         if (HelpType == SHORT_HELP) {
            so("\n  *** Enter 'rtdriver ?' for detailed help text ***\n");
            return;
         }
         else {
            continue;                   /* Skip line for long help */
         }
      }

      so("%s\n", HelpText[i]);
   }

   return;
}


/*
 * piClient()
 *
 * Performance Inspector Client -- Messages from JPROF used as ACK
 */
void piClient(FILE * fp)
{
   int rc;
   int m;
   int n;
   int cmd_len;
   int cmd_nlen;                        /* Network format of length */
   int rsp_nlen;                        /* Network format of length */
   int host;
   int rsp_id;
   int broadcast;
   char * p;
   char buf[PI_MAX_INPUT];
   char cmd[PI_MAX_INPUT];

   int hh, mm, ss;

   for (;;) {
      get_current_time(&hh, &mm, &ss);
      so( "\n%02d:%02d:%02d Enter command> ", hh, mm, ss );

      if (fgets( buf, PI_MAX_INPUT, fp) == NULL)
         break;

      n = (int)strlen(buf);
      buf[--n] = 0;                      /* Remove 'new line' character and decrement length */

      while (n > 0 && buf[n-1] == ' ') { /* Remove trailing blanks */
         buf[--n] = 0;
      }

      if (n) {
         if (stricmp(buf, "exit_rtdriver") == 0 || stricmp(buf, "xx") == 0) {
            get_current_time(&hh, &mm, &ss);
            so("** %s: %02d:%02d:%02d Exiting piClient\n", me, hh, mm, ss );
            return;
         }

         strcpy(cmd, buf);              /* Remember command and size (in case we're broadcasting) */
         cmd_len  = n;
         cmd_nlen = htonl(cmd_len);

         broadcast = 1;                 /* Some commands only need to be sent to one JVM */

         if (pihost_cnt > 1) {
            for (m = 0; m < NUM_NO_BROADCAST_CMDS; m++) {
               if (!stricmp(cmd, no_broadcast_cmds[m])) {
                  broadcast = 0;
                  break;
               }
            }
         }

         for (host = 0; host < pihost_cnt; host++) {
            rsp_id = 0;
            rc = send( piSock[host], (char *)&cmd_nlen, sizeof(int), MSG_DONTROUTE );
            if (rc == SOCKET_ERROR)
               return;                  /* Abort on error */

            rc = send(piSock[host], cmd, cmd_len, MSG_DONTROUTE);
            if (rc == SOCKET_ERROR)
               return;                  /* Abort on error */

            for (;;) {
               n = sizeof(int);
               p = (char *)&rsp_nlen;

               while ( n > 0 ) {        /* Get size of data to receive */
                  m = recv( piSock[host], p, n, 0);
                  if (SOCKET_ERROR == m || 0 == m)
                     return;            /* Abort on error or disconnect */

                  p += m;
                  n -= m;
               }

               n = ntohl(rsp_nlen);     /* Convert from Network to Host data format */
               if (n == 0)
                  break;                /* End of strings from JPROF */

               if (pihost_cnt > 1 && !rsp_id) {
                  rsp_id = 1;
                  get_current_time(&hh, &mm, &ss);
                  so("** %s: %02d:%02d:%02d Response from port %d:\n", me, hh, mm, ss, piPort[host] );
               }

               while (n > 0) {          /* Send all received data to stdout */
                  if (m > PI_MAX_INPUT-1) {
                     m = PI_MAX_INPUT-1;
                  }

                  m = recv( piSock[host], buf, n, 0);
                  if (m == SOCKET_ERROR || n == 0 )
                     return;            /* Abort on error or disconnect */

                  buf[m] = 0;
                  so( "%s", buf );
                  n -= m;
               }
            }

            if (!broadcast)
               break;                   /* It does not make sense to broadcast this command */
         }

         if (stricmp(cmd, "shutdown") == 0) {
            get_current_time(&hh, &mm, &ss);
            so("** %s: %02d:%02d:%02d Exiting piClient\n", me, hh, mm, ss );
            return;
         }
      }
   }

   return;
}


#if defined(LINUX)
/*
 * read_cycles()
 * -------------
 */
uint32_t read_cycles(void)
{
#if defined(CONFIG_S390) || defined(CONFIG_S390X) 
   uint64_t cval;
   __asm__ __volatile__("la     1,%0\n stck    0(1)":"=m"(cval)
                        ::"cc", "1");

   return (cval);
#else

   uint32_t low, high;
#if defined(CONFIG_X86) || defined(CONFIG_X86_64)
   __asm__ __volatile__("rdtsc":"=a"(low), "=d"(high));
#endif 

#if defined(PPC) || defined(CONFIG_PPC64)
   uint32_t temp1 = 1;
   high = 2;   
   while (temp1 != high) {
      __asm__ __volatile__("mftbu %0":"=r"(temp1));
      __asm__ __volatile__("mftb  %0":"=r"(low));
      __asm__ __volatile__("mftbu %0":"=r"(high));
   }
#endif
   return (low);
#endif
}

#endif
