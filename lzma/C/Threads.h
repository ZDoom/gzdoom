/* Threads.h -- multithreading library
2008-11-22 : Igor Pavlov : Public domain */

#ifndef __7Z_THRESDS_H
#define __7Z_THRESDS_H

#ifdef _WIN32
#include <windows.h>
typedef DWORD WRes;

#include "Types.h"

typedef struct _CThread
{
  HANDLE handle;
} CThread;

#define Thread_Construct(thread) (thread)->handle = NULL
#define Thread_WasCreated(thread) ((thread)->handle != NULL)
 
typedef unsigned THREAD_FUNC_RET_TYPE;
#define THREAD_FUNC_CALL_TYPE __stdcall
#define THREAD_FUNC_DECL THREAD_FUNC_RET_TYPE THREAD_FUNC_CALL_TYPE

WRes Thread_Create(CThread *thread, THREAD_FUNC_RET_TYPE (THREAD_FUNC_CALL_TYPE *startAddress)(void *), LPVOID parameter);
WRes Thread_Wait(CThread *thread);
WRes Thread_Close(CThread *thread);

typedef struct _CEvent
{
  HANDLE handle;
} CEvent;

typedef CEvent CAutoResetEvent;
typedef CEvent CManualResetEvent;

#define Event_Construct(event) (event)->handle = NULL
#define Event_IsCreated(event) ((event)->handle != NULL)

WRes ManualResetEvent_Create(CManualResetEvent *event, int initialSignaled);
WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *event);
WRes AutoResetEvent_Create(CAutoResetEvent *event, int initialSignaled);
WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *event);
WRes Event_Set(CEvent *event);
WRes Event_Reset(CEvent *event);
WRes Event_Wait(CEvent *event);
WRes Event_Close(CEvent *event);


typedef struct _CSemaphore
{
  HANDLE handle;
} CSemaphore;

#define Semaphore_Construct(p) (p)->handle = NULL

WRes Semaphore_Create(CSemaphore *p, UInt32 initiallyCount, UInt32 maxCount);
WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 num);
WRes Semaphore_Release1(CSemaphore *p);
WRes Semaphore_Wait(CSemaphore *p);
WRes Semaphore_Close(CSemaphore *p);


typedef CRITICAL_SECTION CCriticalSection;

WRes CriticalSection_Init(CCriticalSection *p);
#define CriticalSection_Delete(p) DeleteCriticalSection(p)
#define CriticalSection_Enter(p) EnterCriticalSection(p)
#define CriticalSection_Leave(p) LeaveCriticalSection(p)

#else
typedef int WRes;

#include "Types.h"
#include <pthread.h>

/* #define DEBUG_SYNCHRO 1 */

typedef struct _CThread
{
	pthread_t _tid;
	int _created;

} CThread;

#define Thread_Construct(thread) (thread)->_created = 0
#define Thread_WasCreated(thread) ((thread)->_created != 0)

typedef unsigned THREAD_FUNC_RET_TYPE;
#define THREAD_FUNC_CALL_TYPE
#define THREAD_FUNC_DECL THREAD_FUNC_RET_TYPE THREAD_FUNC_CALL_TYPE

WRes Thread_Create(CThread *thread, THREAD_FUNC_RET_TYPE (THREAD_FUNC_CALL_TYPE *startAddress)(void *), void *parameter);
WRes Thread_Wait(CThread *thread);
WRes Thread_Close(CThread *thread);

typedef struct _CEvent
{
  int _created;
  int _manual_reset;
  int _state;
  pthread_mutex_t _mutex;
  pthread_cond_t  _cond;
} CEvent;

typedef CEvent CAutoResetEvent;
typedef CEvent CManualResetEvent;

#define Event_Construct(event) (event)->_created = 0
#define Event_IsCreated(event) ((event)->_created)

WRes ManualResetEvent_Create(CManualResetEvent *event, int initialSignaled);
WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *event);
WRes AutoResetEvent_Create(CAutoResetEvent *event, int initialSignaled);
WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *event);
WRes Event_Set(CEvent *event);
WRes Event_Reset(CEvent *event);
WRes Event_Wait(CEvent *event);
WRes Event_Close(CEvent *event);


typedef struct _CSemaphore
{
  int _created;
  UInt32 _count;
  UInt32 _maxCount;
  pthread_mutex_t _mutex;
  pthread_cond_t  _cond;
} CSemaphore;

#define Semaphore_Construct(p) (p)->_created = 0

WRes Semaphore_Create(CSemaphore *p, UInt32 initiallyCount, UInt32 maxCount);
WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 num);
#define Semaphore_Release1(p) Semaphore_ReleaseN(p, 1)
WRes Semaphore_Wait(CSemaphore *p);
WRes Semaphore_Close(CSemaphore *p);

typedef struct {
	pthread_mutex_t _mutex;
} CCriticalSection;

WRes CriticalSection_Init(CCriticalSection *p);
#define CriticalSection_Delete(p) pthread_mutex_destroy(&((p)->_mutex))
#define CriticalSection_Enter(p)  pthread_mutex_lock(&((p)->_mutex))
#define CriticalSection_Leave(p)  pthread_mutex_unlock(&((p)->_mutex))

#endif

#endif

