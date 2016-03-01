/*
** autostart.cpp
** This file contains the heads of lists stored in special data segments
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
** The particular scheme used here was chosen because it's small.
**
** An alternative that will work with any C++ compiler is to use static
** classes to build these lists at run time. Under Visual C++, doing things
** that way can require a lot of extra space, which is why I'm doing things
** this way.
**
** In the case of PClass lists (section creg), I orginally used the
** constructor to do just that, and the code for that still exists if you
** compile with something other than Visual C++ or GCC.
*/

#include "autosegs.h"

#if defined(_MSC_VER)

// The various reg sections are used to group pointers spread across multiple
// source files into cohesive arrays in the final executable. We don't
// actually care about these sections themselves and merge them all into
// a single section during the final link. (.rdata is the standard section
// for initialized read-only data.)

#pragma comment(linker, "/merge:.areg=.rdata /merge:.creg=.rdata /merge:.greg=.rdata")
#pragma comment(linker, "/merge:.yreg=.rdata")

#pragma section(".areg$a",read)
__declspec(allocate(".areg$a")) void *const ARegHead = 0;

#pragma section(".creg$a",read)
__declspec(allocate(".creg$a")) void *const CRegHead = 0;

#pragma section(".greg$a",read)
__declspec(allocate(".greg$a")) void *const GRegHead = 0;

#pragma section(".yreg$a",read)
__declspec(allocate(".yreg$a")) void *const YRegHead = 0;

// We want visual styles support under XP
#if defined _M_IX86

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

#elif defined _M_IA64

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")

#elif defined _M_X64

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")

#else

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#endif

#elif defined(__GNUC__)

#include "doomtype.h"

// I don't know of an easy way to merge sections together with the GNU linker,
// so GCC users will see all of these sections appear in the final executable.
// (There are linker scripts, but that apparently involves extracting the
// default script from ld and then modifying it.)

void *const ARegHead __attribute__((section(SECTION_AREG))) = 0;
void *const CRegHead __attribute__((section(SECTION_CREG))) = 0;
void *const GRegHead __attribute__((section(SECTION_GREG))) = 0;
void *const YRegHead __attribute__((section(SECTION_YREG))) = 0;

#else

#error Please fix autostart.cpp for your compiler

#endif
