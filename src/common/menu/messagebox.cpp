/*
** messagebox.cpp
** Confirmation, notification screns
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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

#include <ctype.h>
#include "menu.h"
#include "gstrings.h"
#include "i_video.h"
#include "c_dispatch.h"
#include "vm.h"
#include "menustate.h"

FName MessageBoxClass = NAME_MessageBoxMenu;

CVAR(Bool, m_quickexit, false, CVAR_ARCHIVE)

typedef void(*hfunc)();
DEFINE_ACTION_FUNCTION(DMessageBoxMenu, CallHandler)
{
	PARAM_PROLOGUE;
	PARAM_POINTERTYPE(Handler, hfunc);
	Handler();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DMenu *CreateMessageBoxMenu(DMenu *parent, const char *message, int messagemode, bool playsound, FName action = NAME_None, hfunc handler = nullptr)
{
	auto c = PClass::FindClass(MessageBoxClass);
	if (!c->IsDescendantOf(NAME_MessageBoxMenu)) c = PClass::FindClass(NAME_MessageBoxMenu);
	auto p = c->CreateNew();
	FString namestr = message;

	IFVIRTUALPTRNAME(p, NAME_MessageBoxMenu, Init)
	{
		VMValue params[] = { p, parent, &namestr, messagemode, playsound, action.GetIndex(), reinterpret_cast<void*>(handler) };
		VMCall(func, params, countof(params), nullptr, 0);
		return (DMenu*)p;
	}
	return nullptr;
}

//=============================================================================
//
//
//
//=============================================================================

void M_StartMessage(const char *message, int messagemode, FName action)
{
	if (CurrentMenu == NULL) 
	{
		// only play a sound if no menu was active before
		M_StartControlPanel(menuactive == MENU_Off);
	}
	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, message, messagemode, false, action);
	newmenu->mParentMenu = CurrentMenu;
	M_ActivateMenu(newmenu);
}

DEFINE_ACTION_FUNCTION(DMenu, StartMessage)
{
	PARAM_PROLOGUE;
	PARAM_STRING(msg);
	PARAM_INT(mode);
	PARAM_NAME(action);
	M_StartMessage(msg.GetChars(), mode, action);
	return 0;
}
