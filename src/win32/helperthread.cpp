/*
** helperthread.cpp
**
** Implements FHelperThread, the base class for helper threads. Includes
** a message queue for passing messages from the main thread to the
** helper thread.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#define _WIN32_WINNT	0x0400
#include "helperthread.h"

//==========================================================================
//
// ---Constructor---
//
//==========================================================================

FHelperThread::FHelperThread ()
{
	ThreadHandle = NULL;
	ThreadID = 0;
	Thread_Events[0] = Thread_Events[1] = 0;
	memset (Messages, 0, sizeof(Messages));
	MessageHead = 0;
	MessageTail = 0;
}

//==========================================================================
//
// ---Destructor---
//
//==========================================================================

FHelperThread::~FHelperThread ()
{
	DestroyThread ();
}

//==========================================================================
//
// LaunchThread
//
//==========================================================================

bool FHelperThread::LaunchThread ()
{
	int i;

	MessageHead = MessageTail = 0;
	for (i = 0; i < MSG_QUEUE_SIZE; i++)
	{
		if ((Messages[i].CompletionEvent = CreateEvent (NULL, FALSE, i > 0, NULL)) == NULL)
			break;
	}
	if (i < MSG_QUEUE_SIZE)
	{
		for (; i >= 0; i--)
		{
			CloseHandle (Messages[i].CompletionEvent);
		}
		return false;
	}
	InitializeCriticalSection (&Thread_Critical);
	if ((Thread_Events[0] = CreateEvent (NULL, TRUE, FALSE, NULL)))
	{
		if ((Thread_Events[1] = CreateEvent (NULL, TRUE, FALSE, NULL)))
		{
			if ((ThreadHandle = CreateThread (NULL, 0, ThreadLaunch, (LPVOID)this, 0, &ThreadID)))
			{
				HANDLE waiters[2] = { Messages[0].CompletionEvent, ThreadHandle };

				if (WaitForMultipleObjects (2, waiters, FALSE, INFINITE) == WAIT_OBJECT_0+1)
				{ // Init failed, and the thread exited
					DestroyThread ();
					return false;
				}
#if defined(_MSC_VER) && defined(_DEBUG)
				// Give the thread a name in the debugger
				struct
				{
					DWORD type;
					LPCSTR name;
					DWORD threadID;
					DWORD flags;
				} info =
				{
					0x1000, ThreadName(), ThreadID, 0
				};
				__try
				{
					RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR *)&info );
				}
				__except(EXCEPTION_CONTINUE_EXECUTION)
				{
				}
#endif

				return true;
			}
			CloseHandle (Thread_Events[1]);
		}
		CloseHandle (Thread_Events[0]);
	}
	DeleteCriticalSection (&Thread_Critical);
	for (i = 0; i < MSG_QUEUE_SIZE; i++)
	{
		CloseHandle (Messages[i].CompletionEvent);
	}
	return false;
}

//==========================================================================
//
// DestroyThread
//
//==========================================================================

void FHelperThread::DestroyThread ()
{
	int i;

	if (ThreadHandle == NULL)
		return;

	SetEvent (Thread_Events[THREAD_KillSelf]);
	// 5 seconds should be sufficient. If not, then we'll crash, but at
	// least that's better than hanging indefinitely.
	WaitForSingleObject (ThreadHandle, 5000);
	CloseHandle (ThreadHandle);

	EnterCriticalSection (&Thread_Critical);

	for (i = 0; i < 8; i++)
	{
		CloseHandle (Messages[i].CompletionEvent);
	}
	CloseHandle (Thread_Events[0]);
	CloseHandle (Thread_Events[1]);

	LeaveCriticalSection (&Thread_Critical);
	DeleteCriticalSection (&Thread_Critical);

	ThreadHandle = NULL;
}

//==========================================================================
//
// HaveThread
//
//==========================================================================

bool FHelperThread::HaveThread () const
{
	return ThreadHandle != NULL;
}

//==========================================================================
//
// SendMessage
//
//==========================================================================

DWORD FHelperThread::SendMessage (DWORD method,
	DWORD parm1, DWORD parm2, DWORD parm3, bool wait)
{
	for (;;)
	{
		EnterCriticalSection (&Thread_Critical);
		if (MessageHead - MessageTail == MSG_QUEUE_SIZE)
		{ // No room, so wait for oldest message to complete
			HANDLE waitEvent = Messages[MessageTail].CompletionEvent;
			LeaveCriticalSection (&Thread_Critical);
			WaitForSingleObject (waitEvent, 1000);
		}

		int head = MessageHead++ % MSG_QUEUE_SIZE;
		Messages[head].Method = method;
		Messages[head].Parm1 = parm1;
		Messages[head].Parm2 = parm2;
		Messages[head].Parm3 = parm3;
		ResetEvent (Messages[head].CompletionEvent);
		LeaveCriticalSection (&Thread_Critical);

		SetEvent (Thread_Events[THREAD_NewMessage]);

		if (wait)
		{
			WaitForSingleObject (Messages[head].CompletionEvent, INFINITE);
			return Messages[head].Return;
		}
		return 0;
	}
}

//==========================================================================
//
// ThreadLaunch (static)
//
//==========================================================================

DWORD WINAPI FHelperThread::ThreadLaunch (LPVOID me)
{
	return ((FHelperThread *)me)->ThreadLoop ();
}

//==========================================================================
//
// ThreadLoop
//
//==========================================================================

DWORD FHelperThread::ThreadLoop ()
{
	if (!Init ())
	{
		ExitThread (0);
	}
	else
	{
		SetEvent (Messages[0].CompletionEvent);
	}

	for (;;)
	{
		switch (MsgWaitForMultipleObjects (2, Thread_Events,
			FALSE, INFINITE, QS_ALLEVENTS))
		{
		case WAIT_OBJECT_0+1:
			// We should quit now.
			Deinit ();
			ExitThread (0);
			break;

		case WAIT_OBJECT_0:
			// Process any new messages. MessageTail is not updated until *after*
			// the message is finished processing.
			for (;;)
			{
				EnterCriticalSection (&Thread_Critical);
				if (MessageHead == MessageTail)
				{
					ResetEvent (Thread_Events[0]);
					LeaveCriticalSection (&Thread_Critical);
					break;	// Resume outer for (Wait for more messages)
				}
				int spot = MessageTail % MSG_QUEUE_SIZE;
				LeaveCriticalSection (&Thread_Critical);

				Messages[spot].Return = Dispatch (Messages[spot].Method,
					Messages[spot].Parm1, Messages[spot].Parm2,
					Messages[spot].Parm3);

				// No need to enter critical section, because only the CD thread
				// is allowed to touch MessageTail or signal the CompletionEvent
				SetEvent (Messages[spot].CompletionEvent);
				MessageTail++;
			}
			break;

		default:
			DefaultDispatch ();
		}
	}
}
