/*
** i_module.cpp
**
**---------------------------------------------------------------------------
** Copyright 2016 Braden Obrzut
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

#include "i_module.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif


#ifndef _WIN32
#define LoadLibraryA(x) dlopen((x), RTLD_LAZY)
#define GetProcAddress(a,b) dlsym((a),(b))
#define FreeLibrary(x) dlclose((x))
using HMODULE = void*;
#endif

bool FModule::Load(std::initializer_list<const char*> libnames)
{
	for(auto lib : libnames)
	{
		if(!Open(lib))
			continue;

		StaticProc *proc;
		for(proc = reqSymbols;proc;proc = proc->Next)
		{
			if(!(proc->Call = GetSym(proc->Name)) && !proc->Optional)
			{
				Unload();
				break;
			}
		}

		if(IsLoaded())
			return true;
	}

	return false;
}

void FModule::Unload()
{
	if(handle)
	{
		FreeLibrary((HMODULE)handle);
		handle = nullptr;
	}
}

bool FModule::Open(const char* lib)
{
#ifdef _WIN32
	if((handle = GetModuleHandleA(lib)) != nullptr)
		return true;
#else
	// Loading an empty string in Linux doesn't do what we expect it to.
	if(*lib == '\0')
		return false;
#endif
	handle = LoadLibraryA(lib);
	return handle != nullptr;
}

void *FModule::GetSym(const char* name)
{
	return (void *)GetProcAddress((HMODULE)handle, name);
}

std::string module_progdir(".");	// current program directory used to look up dynamic libraries. Default to something harmless in case the user didn't set it.

void FModule_SetProgDir(const char* progdir)
{
	module_progdir = progdir;
}
