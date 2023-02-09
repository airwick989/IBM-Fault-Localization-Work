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

#ifndef PI_THREAD_HPP__
#define PI_THREAD_HPP__

#include <stdio.h>
#include <stdlib.h>

//#define LINUX


unsigned GetNumProc();


#if defined(LINUX) || defined(AIX)
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sched.h>

#define DWORD_PTR size_t
#define DWORD unsigned
#define TRUE true
#define SLEEP(n) usleep(n*1000)
#define CRITICAL_SECTION pthread_mutex_t
#define THR_HANDLE pthread_t
#define SYSTEMTIME struct timeval
unsigned GetTickCount();
void InitializeCriticalSection(CRITICAL_SECTION *cs);
void EnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);
pthread_t GetCurrentThread();
int _getch();
int GetLastError();

unsigned SetThreadAffinityMask(THR_HANDLE hThread, size_t mask);

#define WAIT_ABANDONED 0x80
#define WAIT_OBJECT_0  0x0
#define WAIT_TIMEOUT   0x102
#define WAIT_FAILED    -1
class SignallingObject_t
{
   pthread_cond_t  _condition;
   pthread_mutex_t _mutex;
   volatile long   _flag;
public:
   void init()
   {
      _flag = 0;
      if (pthread_cond_init(&_condition, NULL) != 0) {
         fprintf(stderr, "Cannot initialize the signalling object\n"); exit(-1);
      }
      if (pthread_mutex_init(&_mutex, NULL) != 0) {
         fprintf(stderr, "Cannot initialize the signalling object\n"); exit(-1);
      }
   }
   void signal()
   {
      pthread_mutex_lock(&_mutex);
      _flag = 1;
      if (pthread_cond_signal(&_condition) != 0) {
         fprintf(stderr, "Cannot do pthread_cond_signal\n"); exit(-1);
      }
      pthread_mutex_unlock(&_mutex);
   }
   int wait(unsigned timeout)
   {
      struct timespec abstime;
      struct timeval now;
      pthread_mutex_lock(&_mutex);
      if (_flag == 1)
      {
         // someone just signnaled me
         _flag = 0;
         return WAIT_OBJECT_0;
      }

      gettimeofday(&now, NULL);
      unsigned additionalSeconds = timeout/1000;
      unsigned additionalUsec = (timeout%1000)*1000;
      abstime.tv_nsec = (now.tv_usec + additionalUsec)*1000;
      if (abstime.tv_nsec >= 1000000000)
      {
         abstime.tv_nsec -= 1000000000;
         additionalSeconds++;
      }
      abstime.tv_sec = now.tv_sec + additionalSeconds;
      int res;
      do  {
         res = pthread_cond_timedwait(&_condition, &_mutex, &abstime);
         // avoid spurious wakeups
      }while(res==0 && _flag==0);
      _flag = 0;
      pthread_mutex_unlock(&_mutex);

      if (res == ETIMEDOUT)
         return WAIT_TIMEOUT;
      if (res == 0)
         return WAIT_OBJECT_0;
      if (res == EINTR)
         return WAIT_ABANDONED;
      else
         return WAIT_FAILED;
   }
};
void GetSystemTime(SYSTEMTIME *crtTime);
 
#else // Windows
#include <windows.h>
#define WIN32_LEAN_AND_MEAN
#define SLEEP Sleep
#define rdtsc __asm _emit 0x0f __asm _emit 0x31

class SignallingObject_t
{
public:
   HANDLE _hEvent;
   void init()
   {
      _hEvent = CreateEvent(NULL, false, false, NULL);
      if (!_hEvent) {
         fprintf(stderr, "Cannot create event\n"); exit(-1);
      }
   }
   void signal()
   {
      if (SetEvent(_hEvent) == 0) {
         fprintf(stderr, "Cannot signal the event\n"); exit(-1);
      }
   }
   int wait(unsigned timeout)
   {
      return WaitForSingleObject(_hEvent, timeout);
   }
};


#endif // Windows

void WaitForThreads(THR_HANDLE *threadArray, int numThreads);


// This class assumes that a CRITICAL_SECTION has already
// been initialized
class pi_critical_section
{
public:

   pi_critical_section(CRITICAL_SECTION *cs)
      : _cs(cs)
      {
      EnterCriticalSection(_cs);
      }

   ~pi_critical_section()
      {
      LeaveCriticalSection(_cs);
      }

private:

   CRITICAL_SECTION *_cs;
};


#endif // PI_THREAD_HPP__



