/*
** autostart.cpp
** This file contains the heads of lists stored in special data segments
**
**---------------------------------------------------------------------------
** Copyright 1998-2005 Randy Heit
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
** The particular scheme used here was chosen because it's small.
**
** An alternative that will work with any C++ compiler is to use static
** classes to build these lists at run time. Under Visual C++, doing things
** that way can require a lot of extra space, which is why I'm doing things
** this way.
**
** In the case of TypeInfo lists (section creg), I orginally used the
** constructor to do just that, and the code for that still exists if you
** compile with something other than Visual C++ or GCC.
*/

#include "autosegs.h"

#if defined(_MSC_VER)

#pragma comment(linker, "/merge:.areg=.data /merge:.creg=.data /merge:.greg=.data /merge:.sreg=.data /merge:.wreg=.data")

#pragma data_seg(".areg$a")
void *ARegHead = 0;

#pragma data_seg(".creg$a")
void *CRegHead = 0;

#pragma data_seg(".greg$a")
void *GRegHead = 0;

#pragma data_seg(".sreg$a")
void *SRegHead = 0;

#pragma data_seg()



#elif defined(__GNUC__)

#ifndef _WIN32
void *ARegHead __attribute__((section("areg"))) = 0;
void *CRegHead __attribute__((section("creg"))) = 0;
void *GRegHead __attribute__((section("greg"))) = 0;
void *SRegHead __attribute__((section("sreg"))) = 0;
#else

// I can't find any way to specify the order to link files with
// Dev C++, so when compiling with GCC under WIN32, I inspect
// the executable instead of letting the linker do all the work for
// me.

void **ARegHead;
void **CRegHead;
void **GRegHead;
void **SRegHead;

#endif

#elif

#error Please fix autostart.cpp for your compiler

#endif


#ifdef REGEXEPEEK

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>

struct AutoSegStuff
{
	char name[IMAGE_SIZEOF_SHORT_NAME];
	REGINFO *HeadPtr;
	REGINFO *TailPtr;
};

static const AutoSegStuff Stuff[5] =
{
	{ "areg", &ARegHead, &ARegTail },
	{ "creg", &CRegHead, &CRegTail },
	{ "greg", &GRegHead, &GRegTail },
	{ "sreg", &SRegHead, &SRegTail },
};

void InitAutoSegMarkers ()
{
	BYTE *module = (BYTE *)GetModuleHandle (NULL);
	IMAGE_DOS_HEADER *dosHeader = (IMAGE_DOS_HEADER *)module;
	IMAGE_NT_HEADERS *ntHeaders = (IMAGE_NT_HEADERS *)(module + dosHeader->e_lfanew);
	IMAGE_SECTION_HEADER *sections = IMAGE_FIRST_SECTION (ntHeaders);
	int i, j;

	for (i = 0; i < 5; ++i)
	{
		for (j = 0; j < ntHeaders->FileHeader.NumberOfSections; ++j)
		{
			if (memcmp (sections[j].Name, Stuff[i].name, IMAGE_SIZEOF_SHORT_NAME) == 0)
			{
				*Stuff[i].HeadPtr = (REGINFO)(sections[j].VirtualAddress + module - 4);
				*Stuff[i].TailPtr = (REGINFO)(sections[j].VirtualAddress + module + sections[j].Misc.VirtualSize);
				break;
			}
		}
	}
}

#endif
