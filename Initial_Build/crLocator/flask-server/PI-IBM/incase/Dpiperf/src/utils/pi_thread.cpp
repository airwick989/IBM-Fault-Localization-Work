/*
 * Licensed Materials - Property of IBM
 * "Restricted Materials of IBM"
 * 
 * (c) Copyright IBM Corp. 2000, 2007 All Rights Reserved
 * 
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 */

#include "pi_thread.hpp"

#if defined(LINUX) || defined (AIX)
#include <sys/time.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#if defined(LINUX)
#include <pthread.h>
#else
#include <pthread.h>
#include <time.h>  // needed?
#include <sys/types.h> // is this needed
#include <sys/systemcfg.h>
#endif

/*
you need -qarch=pwr3 or greater to compile it

#include <stdio.h>

unsigned long long readtb(void)
   {
   unsigned long long tb;

   __fence();
#ifdef __64BIT__
   tb = __mftb();
#else
   tb = (unsigned long long)__mftbu() << 32;
   tb |= (unsigned long long)__mftb();
#endif
   __fence();

   return tb;
   }

int main(int argc, char **argv)
   {
   printf("Time base: %llu\n", readtb());
   sleep(1);
   printf("Time base: %llu\n", readtb());
   }

*/

unsigned GetTickCount()
{
   struct timeval tv;
   static struct timeval startTv;
   static bool firstTime = true;
   if (firstTime)
   {
      firstTime = false;
      gettimeofday(&startTv , NULL);
      return 0;
   }
   else
   {
      gettimeofday(&tv, NULL);
      struct timeval result;
      result.tv_sec = tv.tv_sec - startTv.tv_sec;
      result.tv_usec = tv.tv_usec - startTv.tv_usec;
      if (result.tv_usec < 0) {
         --(result.tv_sec);
         result.tv_usec += 1000000;
      }
      unsigned res = (unsigned)(result.tv_sec*1000 + result.tv_usec/1000);
      return res;
   }
}

unsigned GetNumProc()
{
   return sysconf(_SC_NPROCESSORS_ONLN);
}

void InitializeCriticalSection(CRITICAL_SECTION *cs)
{
   pthread_mutex_init(cs,  NULL);
}

void EnterCriticalSection(CRITICAL_SECTION *cs)
{
   pthread_mutex_lock(cs);
}

void LeaveCriticalSection(CRITICAL_SECTION *cs)
{
   pthread_mutex_unlock(cs);
}

pthread_t GetCurrentThread()
{
   return pthread_self();
}

int GetLastError() { return errno; }

unsigned SetThreadAffinityMask(THR_HANDLE hThread, size_t mask)
{
#ifdef LINUX
   cpu_set_t cpuMask;
   CPU_ZERO(&cpuMask);
   for (int cpuID=0; mask; cpuID++,mask>>=1)
   {
      if (mask & 1)
         CPU_SET(cpuID, &cpuMask);
   }
   //if (sched_setaffinity(hThread, sizeof(cpuMask), &cpuMask) < 0)
   if (pthread_setaffinity_np(hThread, sizeof(cpuMask), &cpuMask) < 0)

   {
      perror("Error setting affinity");
      return 0; /* failure */
   }
   else
      return 1;
#else // AIX
   return 0;
   if (bindprocessor(BINDTHREAD, hThread, mask) < 0)
   {
      perror("Error setting affinity");
      return 0; /* failure */
   }
   else
      return 1;
#endif
}

#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int _getch( ) 
{
   struct termios oldt, newt;
   int ch;
   tcgetattr( STDIN_FILENO, &oldt );
   newt = oldt;
   newt.c_lflag &= ~( ICANON | ECHO );
   tcsetattr( STDIN_FILENO, TCSANOW, &newt );
   ch = getchar();
   tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
   return ch;
}


void WaitForThreads(THR_HANDLE *threadArray, int numThreads)
{
   for (int i = 0; i < numThreads; i++)
   {
      int res = pthread_join(threadArray[i], NULL);
      switch (res) {
         case 0: break; // success
         case EINVAL:
            fprintf(stderr, "Error: thread is not joinable\n"); break;
         case ESRCH:
            fprintf(stderr, "Error: no such thread\n"); break;
         default:
            fprintf(stderr, "Error while trying to join a thread\n"); break;
      }
   }
}

void GetSystemTime(SYSTEMTIME *crtTime) { gettimeofday(crtTime, NULL);}


#else // Windows
#include <windows.h>
#define WIN32_LEAN_AND_MEAN
unsigned GetNumProc()
{
   SYSTEM_INFO systemInfo;
   GetSystemInfo(&systemInfo);
   return systemInfo.dwNumberOfProcessors;
}

void WaitForThreads(THR_HANDLE *threadArray, int numThreads)
{
   for (int i = 0; i < numThreads; i++)
   {
      if (WaitForSingleObject(threadArray[i], INFINITE) == WAIT_FAILED) {
         fprintf(stderr, "Wating for threads to finish failed\n");
         exit(-1);
      }
   }
}




#endif // WINDOWS

