/*
** attern.cpp
** Termination handling
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
** Copyright 2019 Christoph Oelckers
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


#include <algorithm>
#include "tarray.h"
#include "atterm.h"

static TArray<std::pair<void (*)(void), const char *>> TermFuncs;


//==========================================================================
//
// atterm
//
// Our own atexit because atexit can be problematic under Linux, though I
// forget the circumstances that cause trouble.
//
//==========================================================================

void addterm(void (*func)(), const char *name)
{
	// Make sure this function wasn't already registered.

	for (auto &term : TermFuncs)
	{
		if (term.first == func)
		{
			return;
		}
	}
	TermFuncs.Push(std::make_pair(func, name));
}

//==========================================================================
//
// call_terms
//
//==========================================================================

void call_terms()
{
	for(int i = TermFuncs.Size()-1; i >= 0; i--)
	{
		TermFuncs[i].first();
	}
	TermFuncs.Clear();
}

//==========================================================================
//
// popterm
//
// Removes the most recently register atterm function.
//
//==========================================================================

void popterm()
{
	if (TermFuncs.Size() > 0)
	{
		TermFuncs.Pop();
	}
}

