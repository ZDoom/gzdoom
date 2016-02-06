/*
** autosegs.h
** Arrays built at link-time
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

#ifndef AUTOSEGS_H
#define AUTOSEGS_H

#include "doomtype.h"

#define REGMARKER(x) (x)
typedef void *REGINFO;

// List of Action functons
extern REGINFO ARegHead;
extern REGINFO ARegTail;

// List of TypeInfos
extern REGINFO CRegHead;
extern REGINFO CRegTail;

// List of properties
extern REGINFO GRegHead;
extern REGINFO GRegTail;

// List of variables
extern REGINFO MRegHead;
extern REGINFO MRegTail;

// List of MAPINFO map options
extern REGINFO YRegHead;
extern REGINFO YRegTail;

class FAutoSegIterator
{
	public:
		FAutoSegIterator(REGINFO &head, REGINFO &tail)
		{
			// Weirdness. Mingw's linker puts these together backwards.
			if (&head <= &tail)
			{
				Head = &head;
				Tail = &tail;
			}
			else
			{
				Head = &tail;
				Tail = &head;
			}
			Probe = Head;
		}
		REGINFO operator*() const NO_SANITIZE
		{
			return *Probe;
		}
		FAutoSegIterator &operator++() NO_SANITIZE
		{
			do
			{
				++Probe;
			} while (*Probe == 0 && Probe < Tail);
			return *this;
		}
		void Reset()
		{
			Probe = Head;
		}

	protected:
		REGINFO *Probe;
		REGINFO *Head;
		REGINFO *Tail;
};

#endif
