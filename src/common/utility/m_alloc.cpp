/*
** m_alloc.cpp
** Wrappers for the malloc family of functions that count used bytes.
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Marisa Heit
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

#if defined(__FreeBSD__)
#include <stdlib.h>
#include <malloc_np.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#include <malloc/malloc.h>
#elif defined(__OpenBSD__) || defined(__DragonFly__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#if defined(__sun__) || defined(__sun)
size_t _msize(void* block)
{
	// TODO: find an actual fix for solaris
	(void) block;
	return 0;
}
#endif


#include "engineerrors.h"
#include "dobjgc.h"

#ifndef _MSC_VER
#define _NORMAL_BLOCK			0
#define _malloc_dbg(s,b,f,l)	malloc(s)
#define _realloc_dbg(p,s,b,f,l)	realloc(p,s)
#endif

#ifndef _DEBUG
#if !defined(__solaris__) && !defined(__sun__) && !defined(_sun) && !defined(__OpenBSD__) && !defined(__DragonFly__)
void *M_Malloc(size_t size)
{
	void *block = malloc(size);

	if (block == nullptr)
		I_FatalError("Could not malloc %zu bytes", size);

	GC::ReportAlloc(_msize(block));
	return block;
}

void *M_Realloc(void *memblock, size_t size)
{
	size_t oldsize = memblock ? _msize(memblock) : 0;
	void *block = realloc(memblock, size);
	if (block == nullptr)
	{
		I_FatalError("Could not realloc %zu bytes", size);
	}
	GC::ReportRealloc(oldsize, _msize(block));
	return block;
}

#else
void *M_Malloc(size_t size)
{
	void *block = malloc(size+sizeof(size_t));

	if (block == nullptr)
		I_FatalError("Could not malloc %zu bytes", size);

	size_t *sizeStore = (size_t *) block;
	*sizeStore = size;
	block = sizeStore+1;

	GC::ReportAlloc(_msize(block));
	return block;
}

void *M_Realloc(void *memblock, size_t size)
{
	if (memblock == nullptr)
		return M_Malloc(size);

	size_t oldsize = _msize(memblock);
	void *block = realloc(((size_t*) memblock)-1, size+sizeof(size_t));
	if (block == nullptr)
	{
		I_FatalError("Could not realloc %zu bytes", size);
	}

	size_t *sizeStore = (size_t *) block;
	*sizeStore = size;
	block = sizeStore+1;

	GC::ReportRealloc(oldsize, _msize(block));
	return block;
}
#endif

void* M_Calloc(size_t v1, size_t v2)
{
	auto p = M_Malloc(v1 * v2);
	memset(p, 0, v1 * v2);
	return p;
}

#else
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#if !defined(__solaris__) && !defined(__sun__) && !defined(_sun) && !defined(__OpenBSD__) && !defined(__DragonFly__)
void *M_Malloc_Dbg(size_t size, const char *file, int lineno)
{
	void *block = _malloc_dbg(size, _NORMAL_BLOCK, file, lineno);

	if (block == nullptr)
		I_FatalError("Could not malloc %zu bytes in %s, line %d", size, file, lineno);

	GC::ReportAlloc(_msize(block));
	return block;
}

void *M_Realloc_Dbg(void *memblock, size_t size, const char *file, int lineno)
{
	size_t oldsize = memblock ? _msize(memblock) : 0;
	void *block = _realloc_dbg(memblock, size, _NORMAL_BLOCK, file, lineno);
	if (block == nullptr)
	{
		I_FatalError("Could not realloc %zu bytes in %s, line %d", size, file, lineno);
	}
	GC::ReportRealloc(oldsize, _msize(block));
	return block;
}
#else
void *M_Malloc_Dbg(size_t size, const char *file, int lineno)
{
	void *block = _malloc_dbg(size+sizeof(size_t), _NORMAL_BLOCK, file, lineno);

	if (block == nullptr)
		I_FatalError("Could not malloc %zu bytes in %s, line %d", size, file, lineno);

	size_t *sizeStore = (size_t *) block;
	*sizeStore = size;
	block = sizeStore+1;

	GC::ReportAlloc(_msize(block));
	return block;
}

void *M_Realloc_Dbg(void *memblock, size_t size, const char *file, int lineno)
{
	if (memblock == nullptr)
		return M_Malloc_Dbg(size, file, lineno);

	size_t oldsize = _msize(memblock);
	void *block = _realloc_dbg(((size_t*) memblock)-1, size+sizeof(size_t), _NORMAL_BLOCK, file, lineno);

	if (block == nullptr)
	{
		I_FatalError("Could not realloc %zu bytes in %s, line %d", size, file, lineno);
	}

	size_t *sizeStore = (size_t *) block;
	*sizeStore = size;
	block = sizeStore+1;

	GC::ReportRealloc(oldsize, _msize(block));
	return block;
}
#endif
#endif

void M_Free (void *block)
{
	if (block != nullptr)
	{
		GC::ReportDealloc(_msize(block));
#if !defined(__solaris__) && !defined(__sun__) && !defined(_sun) && !defined(__OpenBSD__) && !defined(__DragonFly__)
		free(block);
#else
		free(((size_t*) block)-1);
#endif
	}
}

