#if 0 //ndef __GL_THREADS_H
#define __GL_THREADS_H

#ifdef WIN32
#include <process.h>
#endif

#include "critsec.h"

// system specific Base classes - should be externalized to a separate header later.
#ifdef WIN32
class FEvent
{
	HANDLE mEvent;

public:

	FEvent(bool manual = true, bool initial = false)
	{
		mEvent = CreateEvent(NULL, manual, initial, NULL);
	}

	~FEvent()
	{
		CloseHandle(mEvent);
	}

	void Set()
	{
		SetEvent(mEvent);
	}

	void Reset()
	{
		ResetEvent(mEvent);
	}
};

class FThread
{
protected:
	uintptr_t hThread;
	bool mTerminateRequest;
public:

	FThread(int stacksize)
	{
		hThread = _beginthreadex(NULL, stacksize, StaticRun, this, 0, NULL);
	}

	virtual ~FThread()
	{
		CloseHandle((HANDLE)hThread);
	}

	void SignalTerminate()
	{
		mTerminateRequest = true;
	}

	virtual void Run() = 0;

private:
	static unsigned __stdcall StaticRun(void *param)
	{
		FThread *thread = (FThread*)param;
		thread->Run();
		return 0;
	}

};

#else
class FEvent
{
public:

	FEvent(bool manual = true, bool initial = false)
	{
	}

	~FEvent()
	{
	}

	void Set()
	{
	}

	void Reset()
	{
	}
};

class FThread
{
protected:
	bool mTerminateRequest;
public:

	FThread(int stacksize)
	{
	}

	virtual ~FThread()
	{
	}

	void SignalTerminate()
	{
		mTerminateRequest = true;
	}

	virtual void Run() = 0;
};

#endif



enum
{
	CS_ValidateTexture,
	CS_Hacks,
	CS_Drawlist,
	CS_Portals,

	MAX_GL_CRITICAL_SECTIONS
};

class FJob
{
	friend class FJobQueue;
	FJob *pNext;
	FJob *pPrev;

public:
	FJob() { pNext = pPrev = NULL; }
	virtual ~FJob();
	virtual void Run() = 0;
};

class FJobQueue
{
	FCriticalSection mCritSec;	// for limiting access
	FEvent mEvent;				// signals that the queue is emoty or not
	FJob *pFirst;
	FJob *pLast;

public:
	FJobQueue();
	~FJobQueue();

	void AddJob(FJob *job);
	FJob *GetJob();
};

class FJobThread : public FThread
{
	FJobQueue *mQueue;

public:
	FJobThread(FJobQueue *queue);
	~FJobThread();
	void Run();
};

class FGLJobProcessWall : public FJob
{
	seg_t *mSeg;
	subsector_t *mSub;
	int mFrontsector, mBacksector;
public:
	FGLJobProcessWall(seg_t *seg, subsector_t *sub, int frontsector, int backsector)
	{
		mSeg = seg;
		mSub = sub;
		mFrontsector = frontsector;
		mBacksector = backsector;
	}

	void Run();
};

class FGLJobProcessSprites : public FJob
{
	sector_t *mSector;
public:
	FGLJobProcessSprites(sector_t *sector);
	void Run();
};

class FGLJobProcessParticles : public FJob
{
	subsector_t *mSubsector;
public:
	FGLJobProcessParticles(subsector_t *subsector);
	void Run();
};

class FGLJobProcessFlats : public FJob
{
	subsector_t *mSubsector;
public:
	FGLJobProcessFlats(subsector_t *subsector);
	void Run();
};


class FGLThreadManager
{
#if 0
	FCriticalSection mCritSecs[MAX_GL_CRITICAL_SECTIONS];
	FJobQueue mJobs;
	FJobThread *mThreads[2];

public:
	FGLThreadManager() {}	
	~FGLThreadManager() {}

	void StartJobs();

	void PauseJobs();

	void EnterCS(int index)
	{
		mCritSecs[index].Enter();
	}

	void LeaveCS(int index)
	{
		mCritSecs[index].Leave();
	}

	void AddJob(FJob *job)
	{
		mJobs.AddJob(job);
	}

	FJob *GetJob()
	{
		return mJobs.GetJob();
	}
#endif
};

#endif