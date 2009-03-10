/* Threads.c -- multithreading library
2008-08-05
Igor Pavlov
Public domain */

#include "Threads.h"

#ifdef _WIN32
#include <process.h>

static WRes GetError()
{
  DWORD res = GetLastError();
  return (res) ? (WRes)(res) : 1;
}

WRes HandleToWRes(HANDLE h) { return (h != 0) ? 0 : GetError(); }
WRes BOOLToWRes(BOOL v) { return v ? 0 : GetError(); }

static WRes MyCloseHandle(HANDLE *h)
{
  if (*h != NULL)
    if (!CloseHandle(*h))
      return GetError();
  *h = NULL;
  return 0;
}

WRes Thread_Create(CThread *thread, THREAD_FUNC_RET_TYPE (THREAD_FUNC_CALL_TYPE *startAddress)(void *), LPVOID parameter)
{
  unsigned threadId; /* Windows Me/98/95: threadId parameter may not be NULL in _beginthreadex/CreateThread functions */
  thread->handle =
    /* CreateThread(0, 0, startAddress, parameter, 0, &threadId); */
    (HANDLE)_beginthreadex(NULL, 0, startAddress, parameter, 0, &threadId);
    /* maybe we must use errno here, but probably GetLastError() is also OK. */
  return HandleToWRes(thread->handle);
}

WRes WaitObject(HANDLE h)
{
  return (WRes)WaitForSingleObject(h, INFINITE);
}

WRes Thread_Wait(CThread *thread)
{
  if (thread->handle == NULL)
    return 1;
  return WaitObject(thread->handle);
}

WRes Thread_Close(CThread *thread)
{
  return MyCloseHandle(&thread->handle);
}

WRes Event_Create(CEvent *p, BOOL manualReset, int initialSignaled)
{
  p->handle = CreateEvent(NULL, manualReset, (initialSignaled ? TRUE : FALSE), NULL);
  return HandleToWRes(p->handle);
}

WRes ManualResetEvent_Create(CManualResetEvent *p, int initialSignaled)
  { return Event_Create(p, TRUE, initialSignaled); }
WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *p)
  { return ManualResetEvent_Create(p, 0); }

WRes AutoResetEvent_Create(CAutoResetEvent *p, int initialSignaled)
  { return Event_Create(p, FALSE, initialSignaled); }
WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *p)
  { return AutoResetEvent_Create(p, 0); }

WRes Event_Set(CEvent *p) { return BOOLToWRes(SetEvent(p->handle)); }
WRes Event_Reset(CEvent *p) { return BOOLToWRes(ResetEvent(p->handle)); }
WRes Event_Wait(CEvent *p) { return WaitObject(p->handle); }
WRes Event_Close(CEvent *p) { return MyCloseHandle(&p->handle); }


WRes Semaphore_Create(CSemaphore *p, UInt32 initiallyCount, UInt32 maxCount)
{
  p->handle = CreateSemaphore(NULL, (LONG)initiallyCount, (LONG)maxCount, NULL);
  return HandleToWRes(p->handle);
}

WRes Semaphore_Release(CSemaphore *p, LONG releaseCount, LONG *previousCount)
{
  return BOOLToWRes(ReleaseSemaphore(p->handle, releaseCount, previousCount));
}
WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 releaseCount)
{
  return Semaphore_Release(p, (LONG)releaseCount, NULL);
}
WRes Semaphore_Release1(CSemaphore *p)
{
  return Semaphore_ReleaseN(p, 1);
}

WRes Semaphore_Wait(CSemaphore *p) { return WaitObject(p->handle); }
WRes Semaphore_Close(CSemaphore *p) { return MyCloseHandle(&p->handle); }

WRes CriticalSection_Init(CCriticalSection *p)
{
  /* InitializeCriticalSection can raise only STATUS_NO_MEMORY exception */
#ifndef __GNUC__
  __try
#endif
  {
    InitializeCriticalSection(p);
    /* InitializeCriticalSectionAndSpinCount(p, 0); */
  }
#ifndef __GNUC__
  __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
#endif
  return 0;
}

#else

#include <pthread.h>
#include <stdlib.h>

#include <errno.h>

#if defined(__linux__) 
#define PTHREAD_MUTEX_ERRORCHECK PTHREAD_MUTEX_ERRORCHECK_NP
#endif


WRes Thread_Create(CThread *thread, THREAD_FUNC_RET_TYPE (THREAD_FUNC_CALL_TYPE *startAddress)(void *), void *parameter)
{ 
	pthread_attr_t attr;
	int ret;

	thread->_created = 0;

	ret = pthread_attr_init(&attr);
	if (ret) return ret;

	ret = pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
	if (ret) return ret;

	ret = pthread_create(&thread->_tid, &attr, (void * (*)(void *))startAddress, parameter);

	/* ret2 = */ pthread_attr_destroy(&attr);

	if (ret) return ret;
	
	thread->_created = 1;

	return 0; // SZ_OK;
}

WRes Thread_Wait(CThread *thread)
{
  void *thread_return;
  int ret;

  if (thread->_created == 0)
    return EINVAL;

  ret = pthread_join(thread->_tid,&thread_return);
  thread->_created = 0;
  
  return ret;
}

WRes Thread_Close(CThread *thread)
{
    if (!thread->_created) return SZ_OK;
    
    pthread_detach(thread->_tid);
    thread->_tid = 0;
    thread->_created = 0;
    return SZ_OK;
}

WRes Event_Create(CEvent *p, BOOL manualReset, int initialSignaled)
{
  pthread_mutex_init(&p->_mutex,0);
  pthread_cond_init(&p->_cond,0);
  p->_manual_reset = manualReset;
  p->_state        = (initialSignaled ? TRUE : FALSE);
  p->_created = 1;
  return 0;
}

WRes Event_Set(CEvent *p) {
  pthread_mutex_lock(&p->_mutex);
  p->_state = TRUE;
  pthread_cond_broadcast(&p->_cond);
  pthread_mutex_unlock(&p->_mutex);
  return 0;
}

WRes Event_Reset(CEvent *p) {
  pthread_mutex_lock(&p->_mutex);
  p->_state = FALSE;
  pthread_mutex_unlock(&p->_mutex);
  return 0;
}
 
WRes Event_Wait(CEvent *p) {
  pthread_mutex_lock(&p->_mutex);
  while (p->_state == FALSE)
  {
     pthread_cond_wait(&p->_cond, &p->_mutex);
  }
  if (p->_manual_reset == FALSE)
  {
     p->_state = FALSE;
  }
  pthread_mutex_unlock(&p->_mutex);
  return 0;
}

WRes Event_Close(CEvent *p) { 
  if (p->_created)
  {
    p->_created = 0;
    pthread_mutex_destroy(&p->_mutex);
    pthread_cond_destroy(&p->_cond);
  }
  return 0;
}

WRes Semaphore_Create(CSemaphore *p, UInt32 initiallyCount, UInt32 maxCount)
{
  pthread_mutex_init(&p->_mutex,0);
  pthread_cond_init(&p->_cond,0);
  p->_count    = initiallyCount;
  p->_maxCount = maxCount;
  p->_created  = 1;
  return 0;
}

WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 releaseCount)
{
  UInt32 newCount;

  if (releaseCount < 1) return EINVAL;

  pthread_mutex_lock(&p->_mutex);

  newCount = p->_count + releaseCount;
  if (newCount > p->_maxCount)
  {
    pthread_mutex_unlock(&p->_mutex);
    return EINVAL;
  }
  p->_count = newCount;
  pthread_cond_broadcast(&p->_cond);
  pthread_mutex_unlock(&p->_mutex);
  return 0;
}

WRes Semaphore_Wait(CSemaphore *p) {
  pthread_mutex_lock(&p->_mutex);
  while (p->_count < 1)
  {
     pthread_cond_wait(&p->_cond, &p->_mutex);
  }
  p->_count--;
  pthread_mutex_unlock(&p->_mutex);
  return 0;
}

WRes Semaphore_Close(CSemaphore *p) {
  if (p->_created)
  {
    p->_created = 0;
    pthread_mutex_destroy(&p->_mutex);
    pthread_cond_destroy(&p->_cond);
  }
  return 0;
}

WRes CriticalSection_Init(CCriticalSection * lpCriticalSection)
{
	return pthread_mutex_init(&(lpCriticalSection->_mutex),0);
}

WRes ManualResetEvent_Create(CManualResetEvent *p, int initialSignaled)
  { return Event_Create(p, TRUE, initialSignaled); }

WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *p) 
  { return ManualResetEvent_Create(p, 0); }

WRes AutoResetEvent_Create(CAutoResetEvent *p, int initialSignaled)
  { return Event_Create(p, FALSE, initialSignaled); }
WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *p) 
  { return AutoResetEvent_Create(p, 0); }

#endif
