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
// Wraps a Windows critical section object.

#ifndef _WINNT_
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

class FInternalCriticalSection
{
public:
	FInternalCriticalSection()
	{
		InitializeCriticalSection(&CritSec);
	}
	~FInternalCriticalSection()
	{
		DeleteCriticalSection(&CritSec);
	}
	void Enter()
	{
		EnterCriticalSection(&CritSec);
	}
	void Leave()
	{
		LeaveCriticalSection(&CritSec);
	}
#if 0
	// SDL has no equivalent functionality, so better not use it on Windows.
	bool TryEnter()
	{
		return TryEnterCriticalSection(&CritSec) != 0;
	}
#endif
private:
	CRITICAL_SECTION CritSec;
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
