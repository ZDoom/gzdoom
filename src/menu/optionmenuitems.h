/*
** optionmenuitems.h
** Control items for option menus
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
#include "v_text.h"
#include "gstrings.h"


void M_DrawConText (int color, int x, int y, const char *str);
void M_SetVideoMode();

//=============================================================================
//
// This class is used to capture the key to be used as the new key binding
// for a control item
//
//=============================================================================

class DEnterKey : public DMenu
{
	DECLARE_CLASS(DEnterKey, DMenu)

	int *pKey;

public:
	DEnterKey(DMenu *parent, int *keyptr)
	: DMenu(parent)
	{
		pKey = keyptr;
		SetMenuMessage(1);
		menuactive = MENU_WaitKey;	// There should be a better way to disable GUI capture...
	}

	bool TranslateKeyboardEvents()
	{
		return false; 
	}

	void SetMenuMessage(int which)
	{
		if (mParentMenu->IsKindOf(RUNTIME_CLASS(DOptionMenu)))
		{
			DOptionMenu *m = barrier_cast<DOptionMenu*>(mParentMenu);
			DMenuItemBase *it = m->GetItem(NAME_Controlmessage);
			if (it != NULL)
			{
				it->SetValue(0, which);
			}
		}
	}

	bool Responder(event_t *ev)
	{
		if (ev->type == EV_KeyDown)
		{
			*pKey = ev->data1;
			menuactive = MENU_On;
			SetMenuMessage(0);
			Close();
			mParentMenu->CallMenuEvent((ev->data1 == KEY_ESCAPE)? MKEY_Abort : MKEY_Input, 0);
			return true;
		}
		return false;
	}

	void Drawer()
	{
		mParentMenu->CallDrawer();
	}
};

#ifndef NO_IMP
IMPLEMENT_CLASS(DEnterKey, true, false)
#endif

