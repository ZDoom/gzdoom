/*
** autozend.cpp
** This file contains the tails of lists stored in special data segments
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
** See autostart.cpp for an explanation of why I do things like this.
*/

#include "autosegs.h"

#if defined(_MSC_VER)

#pragma data_seg(".areg$z")
void *ARegTail = 0;

#pragma data_seg(".creg$z")
void *CRegTail = 0;

#pragma data_seg(".greg$z")
void *GRegTail = 0;

#pragma data_seg(".sreg$z")
void *SRegTail = 0;

#pragma data_seg()



#elif defined(__GNUC__)

#ifndef _WIN32
void *ARegTail __attribute__((section("areg"))) = 0;
void *CRegTail __attribute__((section("creg"))) = 0;
void *GRegTail __attribute__((section("greg"))) = 0;
void *SRegTail __attribute__((section("sreg"))) = 0;
#else

// I can't find any way to specify the order to link files with
// Dev C++, so when compiling with GCC under WIN32, I inspect
// the executable instead of letting the linker do all the work for
// me.

void **ARegTail;
void **CRegTail;
void **GRegTail;
void **SRegTail;
#endif

#elif

#error Please fix autozend.cpp for your compiler

#endif
