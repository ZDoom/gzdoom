//**************************************************************************
//**
//** helperthread.cpp
//**
//** Implements FHelperThread, the base class for helper threads. Includes
//** a message queue for passing messages from the main thread to the
//** helper thread.
//**
//**************************************************************************

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

	return 0;
}
