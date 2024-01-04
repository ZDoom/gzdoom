/*
**
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
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

namespace FileSys {

#ifdef _WIN32

#ifndef _WINNT_
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

class FInternalCriticalSection
{
public:
	void Enter()
	{
		AcquireSRWLockExclusive(&CritSec);
	}
	void Leave()
	{
		ReleaseSRWLockExclusive(&CritSec);
	}
private:
	SRWLOCK CritSec = SRWLOCK_INIT;
};


FInternalCriticalSection *CreateCriticalSection()
{
	return new FInternalCriticalSection();
}

void DeleteCriticalSection(FInternalCriticalSection *c)
{
	delete c;
}

void EnterCriticalSection(FInternalCriticalSection *c)
{
	c->Enter();
}

void LeaveCriticalSection(FInternalCriticalSection *c)
{
	c->Leave();
}

#else

#include "critsec.h"

#include <pthread.h>

class FInternalCriticalSection
{
public:
	FInternalCriticalSection();
	~FInternalCriticalSection();

	void Enter();
	void Leave();

private:
	pthread_mutex_t m_mutex;

};

// TODO: add error handling

FInternalCriticalSection::FInternalCriticalSection()
{
	pthread_mutexattr_t attributes;
	pthread_mutexattr_init(&attributes);
	pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_mutex, &attributes);

	pthread_mutexattr_destroy(&attributes);
}

FInternalCriticalSection::~FInternalCriticalSection()
{
	pthread_mutex_destroy(&m_mutex);
}

void FInternalCriticalSection::Enter()
{
	pthread_mutex_lock(&m_mutex);
}

void FInternalCriticalSection::Leave()
{
	pthread_mutex_unlock(&m_mutex);
}


FInternalCriticalSection *CreateCriticalSection()
{
	return new FInternalCriticalSection();
}

void DeleteCriticalSection(FInternalCriticalSection *c)
{
	delete c;
}

void EnterCriticalSection(FInternalCriticalSection *c)
{
	c->Enter();
}

void LeaveCriticalSection(FInternalCriticalSection *c)
{
	c->Leave();
} 

#endif

}