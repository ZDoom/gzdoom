/*
** c_bind.h
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

#ifndef __C_BINDINGS_H__
#define __C_BINDINGS_H__

#include "doomdef.h"

struct event_t;
class FConfigFile;
class FCommandLine;

void C_NameKeys (char *str, int first, int second);

class FKeyBindings
{
	FString Binds[NUM_KEYS];

public:
	void PerformBind(FCommandLine &argv, const char *msg);
	bool DoKey(event_t *ev);
	void ArchiveBindings(FConfigFile *F, const char *matchcmd = NULL);
	int  GetKeysForCommand (const char *cmd, int *first, int *second);
	void UnbindACommand (const char *str);
	void UnbindAll ();
	void UnbindKey(const char *key);
	void DoBind (const char *key, const char *bind);
	void DefaultBind(const char *keyname, const char *cmd);

	void SetBind(unsigned int key, const char *bind)
	{
		if (key < NUM_KEYS) Binds[key] = bind;
	}

	const FString &GetBinding(unsigned int index) const
	{
		return Binds[index];
	}

	const char *GetBind(unsigned int index) const
	{
		if (index < NUM_KEYS) return Binds[index];
		else return NULL;
	}

};

extern FKeyBindings Bindings;
extern FKeyBindings DoubleBindings;
extern FKeyBindings AutomapBindings;
extern FKeyBindings MenuBindings;


bool C_DoKey (event_t *ev, FKeyBindings *binds, FKeyBindings *doublebinds);

// Stuff used by the customize controls menu
void C_SetDefaultBindings ();
void C_UnbindAll ();

extern const char *KeyNames[];

struct FKeyAction
{
	FString mTitle;
	FString mAction;
};

struct FKeySection
{
	FString mTitle;
	FString mSection;
	TArray<FKeyAction> mActions;
};
extern TArray<FKeySection> KeySections;

#endif //__C_BINDINGS_H__
