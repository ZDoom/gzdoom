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
