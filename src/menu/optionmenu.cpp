/*
** optionmenu.cpp
** Handler class for the option menus and associated items
**
**---------------------------------------------------------------------------
** Copyright 2010-2017 Christoph Oelckers
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

#include "v_video.h"
#include "menu/menu.h"
#include "vm.h"


//=============================================================================
//
//
//
//=============================================================================

DMenuItemBase *DOptionMenuDescriptor::GetItem(FName name)
{
	for(unsigned i=0;i<mItems.Size(); i++)
	{
		FName nm = mItems[i]->mAction;
		if (nm == name) return mItems[i];
	}
	return NULL;
}

void SetCVarDescription(FBaseCVar* cvar, const FString* label)
{
	cvar->AddDescription(*label);
}

DEFINE_ACTION_FUNCTION_NATIVE(_OptionMenuItemOption, SetCVarDescription, SetCVarDescription)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(cv, FBaseCVar);
	PARAM_STRING(label);
	SetCVarDescription(cv, &label);
	return 0;
}