/*
** helperthread.h
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

#ifndef __HELPERTHREAD_H__
#define __HELPERTHREAD_H__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class FHelperThread
{
	struct Message
	{
		DWORD Method;
		DWORD Parm1, Parm2, Parm3;
		HANDLE CompletionEvent;	// Set to signalled when message is finished processing
		DWORD Return;
	};

	enum { MSG_QUEUE_SIZE = 8 };

public:
	FHelperThread ();
	virtual ~FHelperThread ();
	void DestroyThread ();

	bool LaunchThread ();
	DWORD SendMessage (DWORD method, DWORD parm1, DWORD parm2,
		DWORD parm3, bool wait);
	bool HaveThread () const;

protected:
	enum EThreadEvents { THREAD_NewMessage, THREAD_KillSelf };
	virtual bool Init () { return true; }
	virtual void Deinit () {}
	virtual void DefaultDispatch () {}
	virtual DWORD Dispatch (DWORD method, DWORD parm1=0, DWORD parm2=0, DWORD parm3=0) = 0;
	virtual const char *ThreadName () = 0;

	HANDLE ThreadHandle;
	DWORD ThreadID;
	HANDLE Thread_Events[2];
	CRITICAL_SECTION Thread_Critical;
	Message Messages[MSG_QUEUE_SIZE];
	DWORD MessageHead;
	DWORD MessageTail;

private:
	void ReleaseSynchronizers ();
	static DWORD WINAPI ThreadLaunch (LPVOID me);

	DWORD ThreadLoop ();
};

#endif //__HELPERTHREAD_H__
