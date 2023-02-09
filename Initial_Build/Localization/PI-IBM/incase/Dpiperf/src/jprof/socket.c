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
#include "jprof.h"

#define DEFAULT_PORT   8888
#define MAX_PORTS        16
#define COMMAND_LEN     512

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_AIX)
   #include <unistd.h>
   #include <sys/socket.h>
   #include <sys/socketvar.h>
   #include <netinet/in.h>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <pthread.h>
#elif defined(_LINUX)
   #include <unistd.h>
   #include <sys/socket.h>
   #include <sys/socketvar.h>
   #include <netinet/in.h>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <pthread.h>
#elif defined(_WINDOWS)
   #include <stdio.h>
   #include <stdlib.h>
   #include <windows.h>
   #include <winbase.h>
   #include <limits.h>
   #include <conio.h>
   #include <io.h>
#elif defined(_ZOS)
   #include <unistd.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <pthread.h>
   #include <ctype.h>
#else
   #error socket.h: define 1 of these :  _WINDOWS, _AIX, _LINUX  _ZOS
#endif

#ifndef NO_ERROR
   #define NO_ERROR 0
#endif

extern int getpid(void);

/*****************/
static void ErrorMessage(char * from)
{
#if defined(_WINDOWS)
   WriteFlush(gc.msg, "%s error. rc = %d\n", from, WSAGetLastError());
#else
   WriteFlush(gc.msg, "%s error. rc = %d\n", from, errno);
#endif
   return;
}

/*****************/
void CloseSocket(SOCKET s)
{
#if defined(_WINDOWS)
   closesocket(s);
#else
   close(s);
#endif
}

/*****************/
SOCKET find_socket(int port_number)
{
   SOCKET s;
   int rc;
   struct sockaddr_in local_sin;

   memset(&local_sin, 0, sizeof(local_sin));

   local_sin.sin_family      = AF_INET;
   local_sin.sin_addr.s_addr = htonl(INADDR_ANY);
   local_sin.sin_port        = htons((short)port_number);

   OVMsg((" Trying port %d ...\n", port_number));

   // create TCP/IP socket
   s = socket(AF_INET, SOCK_STREAM, 0);

   if (s == INVALID_SOCKET)
   {
      ErrorMessage("socket()");
      return(s);
   }
   OVMsg((" success for socket()\n"));

   // associate local address with socket.
   rc = bind(s, (struct sockaddr *)&local_sin, sizeof(local_sin));

   if (rc == SOCKET_ERROR)
   {
      ErrorMessage("bind()");
      return(INVALID_SOCKET);
   }
   OVMsg((" success for bind()\n"));

   // listen on socket. allow <= 5 connections
   rc = listen(s, 5);

   if (rc == SOCKET_ERROR)
   {
      ErrorMessage("listen()");
      return(INVALID_SOCKET);
   }
   OVMsg((" success for listen()\n"));

   OVMsg((" find_socket: socket = %d\n", s));
   return(s);
}

/*****************/
//####
//#### This should be invoked once: at initialization when the listener thread
//#### is created. After that, if successful, ge.socketJprof should not be closed
//#### unless we're killing the listener thread and want to stop listening
//#### for connections.
//####
int RTCreateSocket(void)
{
#if defined(_WINDOWS)
   WSADATA        wsdata;
   WORD           wVersionRequested;
#endif

   short i;

   if (ge.socketCreated)
   {
      //##### THIS SHOULD NEVER HAPPEN
      WriteFlush(gc.msg, " Socket (%d) already created\n", ge.socketJprof);
      return(NO_ERROR);
   }

#if defined(_WINDOWS)
   // right version of winsock and initialize it.
   wVersionRequested = MAKEWORD(1,1);
   if (WSAStartup(wVersionRequested, &wsdata))
   {
      ErrorMessage("WSAStartup()");
      return(-1);
   }
#endif

   if ( ge.port_number )                // Trying a specific port requested or the one we were using
   {
      ge.socketJprof = find_socket( ge.port_number );

      if ( INVALID_SOCKET == ge.socketJprof )
      {
         WriteFlush(gc.msg, "**ERROR** Failed to connect on port %d\n", ge.port_number);
         return(-1);
      }
   }
   else
   {
      for ( i = 0; i < MAX_PORTS; i++ )
      {
         ge.socketJprof = find_socket( DEFAULT_PORT + i );

         if ( INVALID_SOCKET != ge.socketJprof )
         {
            ge.port_number = DEFAULT_PORT + i;
            break;
         }
      }

      if ( INVALID_SOCKET == ge.socketJprof )
      {
         WriteFlush(gc.msg, "**ERROR** Failed to connect on any port\n");
         return(-1);
      }
   }

   ErrVMsgLog("\nServer socket (%d) created for PID %d. Listening on port %d\n", ge.socketJprof, getpid(), ge.port_number);
   //#### ge.socketCreated is not needed.
   //#### The fact that ge.socketJprof contains a valid socket number (ie. not INVALID_SOCKET)
   //#### means the socket is created. There's no need for a flag to indicate that. Use the socket.
   ge.socketCreated = 1;
   return(NO_ERROR);
}

///============================================================================
/// piClient Support Routines
///
/// The heart of the piClient support is a pair of wrappers, WriteStdout and 
/// WriteStderr, that echo all output to stdout and stderr to the socket if 
/// ge.fEchoToSocket is set.  This flag is set only during the processing of a 
/// command from the client.  
///
/// piClient mode is entered whenever the ShiftOut-ShiftIn-ShiftOut-ShiftIn 
/// sequence is received from the client, RTDriver.  This sequence was chosen 
/// because it cannot be typed or printed and it uses the same character codes 
/// in both ASCII and EBCDIC.  
///
/// All strings sent to and from the piClient are preceeded with an integer 
/// that indicates the length of the string which follows.  Thus, the socket 
/// code can continue to read data until the expected number of characters 
/// have been received.  The client will continue to wait for more strings 
/// until it receives a length of 0, indicating command completion.  
///============================================================================

void sendInt(int n)
{
   int nn = htonl(n);                   // Network version of integer

   send(ge.socketClient, (char *)&nn, sizeof(int), MSG_DONTROUTE);
}

void sendStr(char * s)
{
   int n = (int)strlen(s);

   sendInt(n);

   send(ge.socketClient, s, n, MSG_DONTROUTE);
}

void writeStderr(char * msg)            // Allow hooking of stderr
{
   int n;

   if ( msg )
   {
      n = (int)strlen(msg);

      if ( n )
      {
         fwrite(msg, n, 1, stderr);

         if ( ge.fEchoToSocket )        // Echo all messages to socket
         {
            sendStr(msg);
         }
      }
   }
}

void writeStdout(char * msg)            // Allow hooking of stdout
{
   int n;

   if ( msg )
   {
      n = (int)strlen(msg);

      if ( n )
      {
         fwrite(msg, n, 1, stdout);

         if ( ge.fEchoToSocket )        // Echo all messages to socket
         {
            sendStr(msg);
         }
      }
   }
}

int  recvInt()
{
   int    m;
   int    n;
   int    nn;
   char * p;

   n = sizeof(int);
   p = (char *)&nn;

   while ( n > 0 )                      // Get size of data to receive
   {
      m = recv(ge.socketClient, p, n, 0);

      if ( SOCKET_ERROR == m || 0 == m )   // Abort on error or disconnect
      {
         return(m);
      }
      n -= m;
      p += m;
   }
   return( ntohl(nn) );                 // Convert from Network to Host data format
}

int recvStr( char * line )
{
   int    m;
   int    n;
   char * p;

   n = recvInt();
   p = line;

   while ( n > 0 )                      // Send all received data to stdout
   {
      m = n;

      if ( m > COMMAND_LEN-1 )
      {
         m = COMMAND_LEN-1;
      }
      m = recv( ge.socketClient, p, m, 0);

      if ( SOCKET_ERROR == m || 0 == m )   // Abort on error or disconnect
      {
         return(m);
      }
      p[m] = 0;

      n -= m;
      p += m;
   }
   return( (int)( p - line ) );
}

/*****************/
// listen for client socket command
void XJNICALL RTWaitForClient(void * x)
{
   struct sockaddr_in claddr;

#if defined(_AIX) || defined(_ZOS)
   socklen_t client_len;
#else
   int client_len;
#endif

   int n;
   char line[COMMAND_LEN];
   int zcnt;
   int rc;

   zcnt = 0;

   if (ge.socketJprof == INVALID_SOCKET)
   {
      WriteFlush(gc.msg, " RTWaitForClient: invalid socket. Quitting.\n");
      return;
   }

   client_len = sizeof(claddr);

#if defined(_WINDOWS)
   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

//---------------------------------------------------
   Start:
//---------------------------------------------------

   // wait for client to connect
   ge.socketClient = accept(ge.socketJprof, (struct sockaddr *)&claddr,&client_len);

   if (ge.socketClient == INVALID_SOCKET)
   {
      ErrorMessage("accept()");
      return;
   }

   // rx commands and process them
   for (;;)
   {
      if ( ge.piClient )
      {
         n = recvStr(line);
      }
      else
      {
         memset(line, 0, COMMAND_LEN);
         n = recv(ge.socketClient, line, COMMAND_LEN, 0);
      }

      if (n == 0 || n == SOCKET_ERROR)
      {
         // n == 0:            Connection closed by client
         // n == SOCKET_ERROR: Connection reset by client
         if (++zcnt < 5)
            continue;

         //####
         //#### There's no need to close the jprof socket. JProf is still listening
         //#### for client connections. If the clien closes its connection then all
         //#### JProf needs to do is go back and wait to accept another connection
         //#### (if the clien ever starts another one)
         //####
//##     CloseSocket(ge.socketJprof);
//##     ge.socketCreated = 0;

         // Shutdown (for send/recv) and close the client socket
         shutdown(ge.socketClient, 2);
         CloseSocket(ge.socketClient);

         if ( ge.piClient )
         {
            ge.piClient = 0;
            ErrVMsgLog( "Leaving piClient mode\n" );
         }
         OptVMsgLog(" RTWaitForClient: Client connection closed. Re-Initializing Socket %d on port %d ...\n", ge.socketJprof, ge.port_number);

//##     rc = RTCreateSocket();

//##     if (rc == NO_ERROR)
//##     {
         OptVMsgLog(" RTWaitForClient: ready to accept connections on port %d again ...\n", ge.port_number);
         zcnt = 0;
         goto Start;
//##     }
//##     else
//##     {
//##        OptVMsgLog(" RTWaitForClient: RTCreateSocket() rc = %d. Quitting\n", rc);
//##        break;
//##     }
      }
      else if ( 0 == strcmp( "\x0E\x0F\x0E\x0F", line )   // Initial handshaking for piClient,
                || 0 == strcmp( ",\x0E\x0F\x0E\x0F", line ) )   // line == ShiftOut-ShiftIn-ShiftOut-ShiftIn
      {
         if ( ge.piClient )
         {
            ErrVMsgLog( "Rejecting duplicate piClient request\n" );

            send(ge.socketClient, "N", 1, 0);   // NOT OK
         }
         else
         {
            ge.piClient = 1;

            ErrVMsgLog( "Entering piClient mode\n" );

            send(ge.socketClient, "K", 1, 0);   // OK
         }
      }
      else
      {
         // Handle the piClient, copying messages from stderr to the socket.

         if ( ge.piClient )
         {
            ge.fEchoToSocket = 1;

            rc = procSockCmd( 0, line );

            ge.fEchoToSocket = 0;

            if ( rc )
            {
               sendStr( "REJECTED" );
            }
            else
            {
               sendStr( "ACCEPTED" );
            }
            sendInt(0);                 // Indicate that this command is complete
         }

         // Handle queries right here. Queries have immediate_response semantics.
         // The response is the result of the query, as a string.

         else if ( 0 == strcmp(line, "query_pid")
                   || 0 == strcmp(line, ",query_pid") )
         {
            char response[64];
            int len;

            sprintf(response, "%d\n", CurrentProcessID());
            len = 1 + (int)strlen(response);
            send(ge.socketClient, response, len, 0);
         }

         // If immediate responses or shutdown command, respond now
         // If we don't respond now (before executing the command) we
         // must respond later, or rtdriver will wait forever.

         else if ( ge.immediate_response
                   || 0 == strcmp(line, "shutdown")
                   || 0 == strcmp(line, ",shutdown") )
         {
            send(ge.socketClient, "K", 1, 0);   // ACK

            rc = procSockCmd( 0, line );
         }
         else
         {
            rc = procSockCmd( 0, line );

            if ( rc )
            {
               send(ge.socketClient, "N", 1, 0);   // NOT OK
            }
            else
            {
               send(ge.socketClient, "K", 1, 0);   // OK
            }
         }
         zcnt = 0;
      }
   }

//----- UNREACHABLE CODE ------------------------------------------------------
//
// WriteFlush(gc.msg, " JPROF: RTDriver listener thread terminating. Closing sockets.\n");
// shutdown(ge.socketJprof, 2);
// CloseSocket(ge.socketJprof);
// shutdown(ge.socketClient, 2);
// CloseSocket(ge.socketClient);
// ge.socketCreated = 0;
// return;

}
