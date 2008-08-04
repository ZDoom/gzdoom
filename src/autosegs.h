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

#ifdef __GNUC__
#ifdef __MACH__
#define AREG_SECTION "__DATA,areg"
#define CREG_SECTION "__DATA,creg"
#define GREG_SECTION "__DATA,greg"
#else
#define AREG_SECTION "areg"
#define CREG_SECTION "creg"
#define GREG_SECTION "greg"
#endif
#endif

#define REGMARKER(x) (x)
typedef void *REGINFO;

// List of ActorInfos and the TypeInfos they belong to
extern REGINFO ARegHead;
extern REGINFO ARegTail;

// List of AT_GAME_SET functions
extern REGINFO GRegHead;
extern REGINFO GRegTail;

// List of TypeInfos
extern REGINFO CRegHead;
extern REGINFO CRegTail;

template<class T, REGINFO *_head, REGINFO *_tail>
class TAutoSegIteratorNoArrow
{
	public:
		TAutoSegIteratorNoArrow ()
		{
			// Weirdness. Mingw's linker puts these together backwards.
			if (_head < _tail)
			{
				Head = _head;
				Tail = _tail;
			}
			else
			{
				Head = _tail;
				Tail = _head;
			}
			Probe = (T *)REGMARKER(Head);
		}
		operator T () const
		{
			return *Probe;
		}
		TAutoSegIteratorNoArrow &operator++()
		{
			do
			{
				++Probe;
			} while (*Probe == 0 && Probe < (T *)REGMARKER(Tail));
			return *this;
		}
		void Reset ()
		{
			Probe = (T *)REGMARKER(Head);
		}

	protected:
		T *Probe;
		REGINFO *Head;
		REGINFO *Tail;
};

template<class T, REGINFO *head, REGINFO *tail>
class TAutoSegIterator : public TAutoSegIteratorNoArrow<T, head, tail>
{
	public:
		T operator->() const
		{
			return *(this->Probe);
		}
};

#endif
