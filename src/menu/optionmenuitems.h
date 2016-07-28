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
// opens a submenu, action is a submenu name
//
//=============================================================================

class FOptionMenuItemSubmenu : public FOptionMenuItem
{
	int mParam;
public:
	FOptionMenuItemSubmenu(const char *label, const char *menu, int param = 0)
		: FOptionMenuItem(label, menu)
	{
		mParam = param;
	}

	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColorMore);
		return indent;
	}

	bool Activate()
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		M_SetMenu(mAction, mParam);
		return true;
	}
};


//=============================================================================
//
// Executes a CCMD, action is a CCMD name
//
//=============================================================================

class FOptionMenuItemCommand : public FOptionMenuItemSubmenu
{
public:
	FOptionMenuItemCommand(const char *label, const char *menu)
		: FOptionMenuItemSubmenu(label, menu)
	{
	}

	bool Activate()
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		C_DoCommand(mAction);
		return true;
	}

};

//=============================================================================
//
// Executes a CCMD after confirmation, action is a CCMD name
//
//=============================================================================

class FOptionMenuItemSafeCommand : public FOptionMenuItemCommand
{
	// action is a CCMD
protected:
	FString mPrompt;

public:
	FOptionMenuItemSafeCommand(const char *label, const char *menu, const char *prompt)
		: FOptionMenuItemCommand(label, menu)
		, mPrompt(prompt)
	{
	}

	bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mkey == MKEY_MBYes)
		{
			C_DoCommand(mAction);
			return true;
		}
		return FOptionMenuItemCommand::MenuEvent(mkey, fromcontroller);
	}

	bool Activate()
	{
		const char *msg = mPrompt.IsNotEmpty() ? mPrompt.GetChars() : "$SAFEMESSAGE";
		if (*msg == '$')
		{
			msg = GStrings(msg + 1);
		}

		const char *actionLabel = mLabel.GetChars();
		if (actionLabel != NULL)
		{
			if (*actionLabel == '$')
			{
				actionLabel = GStrings(actionLabel + 1);
			}
		}

		FString FullString;
		FullString.Format(TEXTCOLOR_WHITE "%s" TEXTCOLOR_NORMAL "\n\n" "%s", actionLabel != NULL ? actionLabel : "", msg);

		if (msg && FullString) M_StartMessage(FullString, 0);
		return true;
	}
};

//=============================================================================
//
// Base class for option lists
//
//=============================================================================

class FOptionMenuItemOptionBase : public FOptionMenuItem
{
protected:
	// action is a CVAR
	FName mValues;	// Entry in OptionValues table
	FBaseCVar *mGrayCheck;
	int mCenter;
public:

	enum
	{
		OP_VALUES = 0x11001
	};

	FOptionMenuItemOptionBase(const char *label, const char *menu, const char *values, const char *graycheck, int center)
		: FOptionMenuItem(label, menu)
	{
		mValues = values;
		mGrayCheck = (FBoolCVar*)FindCVar(graycheck, NULL);
		mCenter = center;
	}

	bool SetString(int i, const char *newtext)
	{
		if (i == OP_VALUES) 
		{
			FOptionValues **opt = OptionValues.CheckKey(newtext);
			mValues = newtext;
			if (opt != NULL && *opt != NULL) 
			{
				int s = GetSelection();
				if (s >= (int)(*opt)->mValues.Size()) s = 0;
				SetSelection(s);	// readjust the CVAR if its value is outside the range now
				return true;
			}
		}
		return false;
	}



	//=============================================================================
	virtual int GetSelection() = 0;
	virtual void SetSelection(int Selection) = 0;

	//=============================================================================
	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		bool grayed = mGrayCheck != NULL && !(mGrayCheck->GetGenericRep(CVAR_Bool).Bool);

		if (mCenter)
		{
			indent = (screen->GetWidth() / 2);
		}
		drawLabel(indent, y, selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColor, grayed);

		int overlay = grayed? MAKEARGB(96,48,0,0) : 0;
		const char *text;
		int Selection = GetSelection();
		FOptionValues **opt = OptionValues.CheckKey(mValues);
		if (Selection < 0 || opt == NULL || *opt == NULL)
		{
			text = "Unknown";
		}
		else
		{
			text = (*opt)->mValues[Selection].Text;
		}
		if (*text == '$') text = GStrings(text + 1);
		screen->DrawText (SmallFont, OptionSettings.mFontColorValue, indent + CURSORSPACE, y, 
			text, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);
		return indent;
	}

	//=============================================================================
	bool MenuEvent (int mkey, bool fromcontroller)
	{
		FOptionValues **opt = OptionValues.CheckKey(mValues);
		if (opt != NULL && *opt != NULL && (*opt)->mValues.Size() > 0)
		{
			int Selection = GetSelection();
			if (mkey == MKEY_Left)
			{
				if (Selection == -1) Selection = 0;
				else if (--Selection < 0) Selection = (*opt)->mValues.Size()-1;
			}
			else if (mkey == MKEY_Right || mkey == MKEY_Enter)
			{
				if (++Selection >= (int)(*opt)->mValues.Size()) Selection = 0;
			}
			else
			{
				return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
			}
			SetSelection(Selection);
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
		}
		else
		{
			return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
		}
		return true;
	}

	bool Selectable()
	{
		return !(mGrayCheck != NULL && !(mGrayCheck->GetGenericRep(CVAR_Bool).Bool));
	}
};

//=============================================================================
//
// Change a CVAR, action is the CVAR name
//
//=============================================================================

class FOptionMenuItemOption : public FOptionMenuItemOptionBase
{
	// action is a CVAR
	FBaseCVar *mCVar;
public:

	FOptionMenuItemOption(const char *label, const char *menu, const char *values, const char *graycheck, int center)
		: FOptionMenuItemOptionBase(label, menu, values, graycheck, center)
	{
		mCVar = FindCVar(mAction, NULL);
	}

	//=============================================================================
	int GetSelection()
	{
		int Selection = -1;
		FOptionValues **opt = OptionValues.CheckKey(mValues);
		if (opt != NULL && *opt != NULL && mCVar != NULL && (*opt)->mValues.Size() > 0)
		{
			if ((*opt)->mValues[0].TextValue.IsEmpty())
			{
				UCVarValue cv = mCVar->GetGenericRep(CVAR_Float);
				for(unsigned i = 0; i < (*opt)->mValues.Size(); i++)
				{ 
					if (fabs(cv.Float - (*opt)->mValues[i].Value) < FLT_EPSILON)
					{
						Selection = i;
						break;
					}
				}
			}
			else
			{
				const char *cv = mCVar->GetHumanString();
				for(unsigned i = 0; i < (*opt)->mValues.Size(); i++)
				{
					if ((*opt)->mValues[i].TextValue.CompareNoCase(cv) == 0)
					{
						Selection = i;
						break;
					}
				}
			}
		}
		return Selection;
	}

	void SetSelection(int Selection)
	{
		UCVarValue value;
		FOptionValues **opt = OptionValues.CheckKey(mValues);
		if (opt != NULL && *opt != NULL && mCVar != NULL && (*opt)->mValues.Size() > 0)
		{
			if ((*opt)->mValues[0].TextValue.IsEmpty())
			{
				value.Float = (float)(*opt)->mValues[Selection].Value;
				mCVar->SetGenericRep (value, CVAR_Float);
			}
			else
			{
				value.String = (*opt)->mValues[Selection].TextValue.LockBuffer();
				mCVar->SetGenericRep (value, CVAR_String);
				(*opt)->mValues[Selection].TextValue.UnlockBuffer();
			}
		}
	}
};

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
			FListMenuItem *it = m->GetItem(NAME_Controlmessage);
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
			mParentMenu->MenuEvent((ev->data1 == KEY_ESCAPE)? MKEY_Abort : MKEY_Input, 0);
			return true;
		}
		return false;
	}

	void Drawer()
	{
		mParentMenu->Drawer();
	}
};

#ifndef NO_IMP
IMPLEMENT_ABSTRACT_CLASS(DEnterKey)
#endif

//=============================================================================
//
// // Edit a key binding, Action is the CCMD to bind
//
//=============================================================================

class FOptionMenuItemControl : public FOptionMenuItem
{
	FKeyBindings *mBindings;
	int mInput;
	bool mWaiting;
public:

	FOptionMenuItemControl(const char *label, const char *menu, FKeyBindings *bindings)
		: FOptionMenuItem(label, menu)
	{
		mBindings = bindings;
		mWaiting = false;
	}


	//=============================================================================
	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, mWaiting? OptionSettings.mFontColorHighlight: 
			(selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColor));

		char description[64];
		int Key1, Key2;

		mBindings->GetKeysForCommand(mAction, &Key1, &Key2);
		C_NameKeys (description, Key1, Key2);
		if (description[0])
		{
			M_DrawConText(CR_WHITE, indent + CURSORSPACE, y + (OptionSettings.mLinespacing-8)*CleanYfac_1, description);
		}
		else
		{
			screen->DrawText(SmallFont, CR_BLACK, indent + CURSORSPACE, y + (OptionSettings.mLinespacing-8)*CleanYfac_1, "---",
				DTA_CleanNoMove_1, true, TAG_DONE);
		}
		return indent;
	}

	//=============================================================================
	bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (mkey == MKEY_Input)
		{
			mWaiting = false;
			mBindings->SetBind(mInput, mAction);
			return true;
		}
		else if (mkey == MKEY_Clear)
		{
			mBindings->UnbindACommand(mAction);
			return true;
		}
		else if (mkey == MKEY_Abort)
		{
			mWaiting = false;
			return true;
		}
		return false;
	}

	bool Activate()
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		mWaiting = true;
		DMenu *input = new DEnterKey(DMenu::CurrentMenu, &mInput);
		M_ActivateMenu(input);
		return true;
	}
};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuItemStaticText : public FOptionMenuItem
{
	EColorRange mColor;
public:
	FOptionMenuItemStaticText(const char *label, bool header)
		: FOptionMenuItem(label, NAME_None, true)
	{
		mColor = header ? OptionSettings.mFontColorHeader : OptionSettings.mFontColor;
	}

	FOptionMenuItemStaticText(const char *label, EColorRange cr)
		: FOptionMenuItem(label, NAME_None, true)
	{
		mColor = cr;
	}

	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, mColor);
		return -1;
	}

	bool Selectable()
	{
		return false;
	}

};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuItemStaticTextSwitchable : public FOptionMenuItem
{
	EColorRange mColor;
	FString mAltText;
	int mCurrent;

public:
	FOptionMenuItemStaticTextSwitchable(const char *label, const char *label2, FName action, EColorRange cr)
		: FOptionMenuItem(label, action, true)
	{
		mColor = cr;
		mAltText = label2;
		mCurrent = 0;
	}

	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		const char *txt = mCurrent? mAltText.GetChars() : mLabel.GetChars();
		if (*txt == '$') txt = GStrings(txt + 1);
		int w = SmallFont->StringWidth(txt) * CleanXfac_1;
		int x = (screen->GetWidth() - w) / 2;
		screen->DrawText (SmallFont, mColor, x, y, txt, DTA_CleanNoMove_1, true, TAG_DONE);
		return -1;
	}

	bool SetValue(int i, int val)
	{
		if (i == 0) 
		{
			mCurrent = val;
			return true;
		}
		return false;
	}

	bool SetString(int i, const char *newtext)
	{
		if (i == 0) 
		{
			mAltText = newtext;
			return true;
		}
		return false;
	}

	bool Selectable()
	{
		return false;
	}
};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuSliderBase : public FOptionMenuItem
{
	// action is a CVAR
	double mMin, mMax, mStep;
	int mShowValue;
	int mDrawX;
	int mSliderShort;

public:
	FOptionMenuSliderBase(const char *label, double min, double max, double step, int showval)
		: FOptionMenuItem(label, NAME_None)
	{
		mMin = min;
		mMax = max;
		mStep = step;
		mShowValue = showval;
		mDrawX = 0;
		mSliderShort = 0;
	}

	virtual double GetSliderValue() = 0;
	virtual void SetSliderValue(double val) = 0;

	//=============================================================================
	//
	// Draw a slider. Set fracdigits negative to not display the current value numerically.
	//
	//=============================================================================

	void DrawSlider (int x, int y, double min, double max, double cur, int fracdigits, int indent)
	{
		char textbuf[16];
		double range;
		int maxlen = 0;
		int right = x + (12*8 + 4) * CleanXfac_1;
		int cy = y + (OptionSettings.mLinespacing-8)*CleanYfac_1;

		range = max - min;
		double ccur = clamp(cur, min, max) - min;

		if (fracdigits >= 0)
		{
			mysnprintf(textbuf, countof(textbuf), "%.*f", fracdigits, max);
			maxlen = SmallFont->StringWidth(textbuf) * CleanXfac_1;
		}

		mSliderShort = right + maxlen > screen->GetWidth();

		if (!mSliderShort)
		{
			M_DrawConText(CR_WHITE, x, cy, "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12");
			M_DrawConText(CR_ORANGE, x + int((5 + ((ccur * 78) / range)) * CleanXfac_1), cy, "\x13");
		}
		else
		{
			// On 320x200 we need a shorter slider
			M_DrawConText(CR_WHITE, x, cy, "\x10\x11\x11\x11\x11\x11\x12");
			M_DrawConText(CR_ORANGE, x + int((5 + ((ccur * 38) / range)) * CleanXfac_1), cy, "\x13");
			right -= 5*8*CleanXfac_1;
		}

		if (fracdigits >= 0 && right + maxlen <= screen->GetWidth())
		{
			mysnprintf(textbuf, countof(textbuf), "%.*f", fracdigits, cur);
			screen->DrawText(SmallFont, CR_DARKGRAY, right, y, textbuf, DTA_CleanNoMove_1, true, TAG_DONE);
		}
	}


	//=============================================================================
	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColor);
		mDrawX = indent + CURSORSPACE;
		DrawSlider (mDrawX, y, mMin, mMax, GetSliderValue(), mShowValue, indent);
		return indent;
	}

	//=============================================================================
	bool MenuEvent (int mkey, bool fromcontroller)
	{
		double value = GetSliderValue();

		if (mkey == MKEY_Left)
		{
			value -= mStep;
		}
		else if (mkey == MKEY_Right)
		{
			value += mStep;
		}
		else
		{
			return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
		}
		SetSliderValue(clamp(value, mMin, mMax));
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
		return true;
	}

	bool MouseEvent(int type, int x, int y)
	{
		DOptionMenu *lm = static_cast<DOptionMenu*>(DMenu::CurrentMenu);
		if (type != DMenu::MOUSE_Click)
		{
			if (!lm->CheckFocus(this)) return false;
		}
		if (type == DMenu::MOUSE_Release)
		{
			lm->ReleaseFocus();
		}

		int slide_left = mDrawX+8*CleanXfac_1;
		int slide_right = slide_left + (10*8*CleanXfac_1 >> mSliderShort);	// 12 char cells with 8 pixels each.

		if (type == DMenu::MOUSE_Click)
		{
			if (x < slide_left || x >= slide_right) return true;
		}

		x = clamp(x, slide_left, slide_right);
		double v = mMin + ((x - slide_left) * (mMax - mMin)) / (slide_right - slide_left);
		if (v != GetSliderValue())
		{
			SetSliderValue(v);
			//S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
		}
		if (type == DMenu::MOUSE_Click)
		{
			lm->SetFocus(this);
		}
		return true;
	}

};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuSliderCVar : public FOptionMenuSliderBase
{
	FBaseCVar *mCVar;
public:
	FOptionMenuSliderCVar(const char *label, const char *menu, double min, double max, double step, int showval)
		: FOptionMenuSliderBase(label, min, max, step, showval)
	{
		mCVar = FindCVar(menu, NULL);
	}

	double GetSliderValue()
	{
		if (mCVar != NULL)
		{
			return mCVar->GetGenericRep(CVAR_Float).Float;
		}
		else
		{
			return 0;
		}
	}

	void SetSliderValue(double val)
	{
		if (mCVar != NULL)
		{
			UCVarValue value;
			value.Float = (float)val;
			mCVar->SetGenericRep(value, CVAR_Float);
		}
	}
};

//=============================================================================
//
//
//
//=============================================================================

class FOptionMenuSliderVar : public FOptionMenuSliderBase
{
	float *mPVal;
public:

	FOptionMenuSliderVar(const char *label, float *pVal, double min, double max, double step, int showval)
		: FOptionMenuSliderBase(label, min, max, step, showval)
	{
		mPVal = pVal;
	}

	double GetSliderValue()
	{
		return *mPVal;
	}

	void SetSliderValue(double val)
	{
		*mPVal = (float)val;
	}
};

//=============================================================================
//
// // Edit a key binding, Action is the CCMD to bind
//
//=============================================================================

class FOptionMenuItemColorPicker : public FOptionMenuItem
{
	FColorCVar *mCVar;
public:

	enum
	{
		CPF_RESET = 0x20001,
	};

	FOptionMenuItemColorPicker(const char *label, const char *menu)
		: FOptionMenuItem(label, menu)
	{
		FBaseCVar *cv = FindCVar(menu, NULL);
		if (cv != NULL && cv->GetRealType() == CVAR_Color)
		{
			mCVar = (FColorCVar*)cv;
		}
		else mCVar = NULL;
	}

	//=============================================================================
	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected? OptionSettings.mFontColorSelection : OptionSettings.mFontColor);

		if (mCVar != NULL)
		{
			int box_x = indent + CURSORSPACE;
			int box_y = y + CleanYfac_1;
			screen->Clear (box_x, box_y, box_x + 32*CleanXfac_1, box_y + OptionSettings.mLinespacing*CleanYfac_1,
				-1, (uint32)*mCVar | 0xff000000);
		}
		return indent;
	}

	bool SetValue(int i, int v)
	{
		if (i == CPF_RESET && mCVar != NULL)
		{
			mCVar->ResetToDefault();
			return true;
		}
		return false;
	}

	bool Activate()
	{
		if (mCVar != NULL)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
			DMenu *picker = StartPickerMenu(DMenu::CurrentMenu, mLabel, mCVar);
			if (picker != NULL)
			{
				M_ActivateMenu(picker);
				return true;
			}
		}
		return false;
	}
};

class FOptionMenuScreenResolutionLine : public FOptionMenuItem
{
	FString mResTexts[3];
	int mSelection;
	int mHighlight;
	int mMaxValid;
public:

	enum
	{
		SRL_INDEX = 0x30000,
		SRL_SELECTION = 0x30003,
		SRL_HIGHLIGHT = 0x30004,
	};

	FOptionMenuScreenResolutionLine(const char *action)
		: FOptionMenuItem("", action)
	{
		mSelection = 0;
		mHighlight = -1;
	}

	bool SetValue(int i, int v)
	{
		if (i == SRL_SELECTION)
		{
			mSelection = v;
			return true;
		}
		else if (i == SRL_HIGHLIGHT)
		{
			mHighlight = v;
			return true;
		}
		return false;
	}

	bool GetValue(int i, int *v)
	{
		if (i == SRL_SELECTION)
		{
			*v = mSelection;
			return true;
		}
		return false;
	}

	bool SetString(int i, const char *newtext)
	{
		if (i >= SRL_INDEX && i <= SRL_INDEX+2) 
		{
			mResTexts[i-SRL_INDEX] = newtext;
			if (mResTexts[0].IsEmpty()) mMaxValid = -1;
			else if (mResTexts[1].IsEmpty()) mMaxValid = 0;
			else if (mResTexts[2].IsEmpty()) mMaxValid = 1;
			else mMaxValid = 2;
			return true;
		}
		return false;
	}

	bool GetString(int i, char *s, int len)
	{
		if (i >= SRL_INDEX && i <= SRL_INDEX+2) 
		{
			strncpy(s, mResTexts[i-SRL_INDEX], len-1);
			s[len-1] = 0;
			return true;
		}
		return false;
	}

	bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mkey == MKEY_Left)
		{
			if (--mSelection < 0) mSelection = mMaxValid;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
			return true;
		}
		else if (mkey == MKEY_Right)
		{
			if (++mSelection > mMaxValid) mSelection = 0;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
			return true;
		}
		else 
		{
			return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
		}
		return false;
	}

	bool MouseEvent(int type, int x, int y)
	{
		int colwidth = screen->GetWidth() / 3;
		mSelection = x / colwidth;
		return FOptionMenuItem::MouseEvent(type, x, y);
	}

	bool Activate()
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		M_SetVideoMode();
		return true;
	}

	int Draw(FOptionMenuDescriptor *desc, int y, int indent, bool selected)
	{
		int colwidth = screen->GetWidth() / 3;
		EColorRange color;

		for (int x = 0; x < 3; x++)
		{
			if (selected && mSelection == x)
				color = OptionSettings.mFontColorSelection;
			else if (x == mHighlight)
				color = OptionSettings.mFontColorHighlight;
			else
				color = OptionSettings.mFontColorValue;

			screen->DrawText (SmallFont, color, colwidth * x + 20 * CleanXfac_1, y, mResTexts[x], DTA_CleanNoMove_1, true, TAG_DONE);
		}
		return colwidth * mSelection + 20 * CleanXfac_1 - CURSORSPACE;
	}

	bool Selectable()
	{
		return mMaxValid >= 0;
	}

	void Ticker()
	{
		if (Selectable() && mSelection > mMaxValid)
		{
			mSelection = mMaxValid;
		}
	}
};


//=============================================================================
//
// [TP] FOptionMenuFieldBase
//
// Base class for input fields
//
//=============================================================================

class FOptionMenuFieldBase : public FOptionMenuItem
{
public:
	FOptionMenuFieldBase ( const char* label, const char* menu, const char* graycheck ) :
		FOptionMenuItem ( label, menu ),
		mCVar ( FindCVar( mAction, NULL )),
		mGrayCheck (( graycheck && strlen( graycheck )) ? FindCVar( graycheck, NULL ) : NULL ) {}

	const char* GetCVarString()
	{
		if ( mCVar == NULL )
			return "";

		return mCVar->GetHumanString();
	}

	virtual FString Represent()
	{
		return GetCVarString();
	}

	int Draw ( FOptionMenuDescriptor*, int y, int indent, bool selected )
	{
		bool grayed = mGrayCheck != NULL && !( mGrayCheck->GetGenericRep( CVAR_Bool ).Bool );
		drawLabel( indent, y, selected ? OptionSettings.mFontColorSelection : OptionSettings.mFontColor, grayed );
		int overlay = grayed? MAKEARGB( 96, 48, 0, 0 ) : 0;

		screen->DrawText( SmallFont, OptionSettings.mFontColorValue, indent + CURSORSPACE, y,
			Represent().GetChars(), DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE );
		return indent;
	}

	bool GetString ( int i, char* s, int len )
	{
		if ( i == 0 )
		{
			strncpy( s, GetCVarString(), len );
			s[len - 1] = '\0';
			return true;
		}

		return false;
	}

	bool SetString ( int i, const char* s )
	{
		if ( i == 0 )
		{
			if ( mCVar )
			{
				UCVarValue vval;
				vval.String = s;
				mCVar->SetGenericRep( vval, CVAR_String );
			}

			return true;
		}

		return false;
	}

protected:
	// Action is a CVar in this class and derivatives.
	FBaseCVar* mCVar;
	FBaseCVar* mGrayCheck;
};

//=============================================================================
//
// [TP] FOptionMenuTextField
//
// A text input field widget, for use with string CVars.
//
//=============================================================================

class FOptionMenuTextField : public FOptionMenuFieldBase
{
public:
	FOptionMenuTextField ( const char *label, const char* menu, const char* graycheck ) :
		FOptionMenuFieldBase ( label, menu, graycheck ),
		mEntering ( false ) {}

	FString Represent()
	{
		FString text = mEntering ? mEditName : GetCVarString();

		if ( mEntering )
			text += ( gameinfo.gametype & GAME_DoomStrifeChex ) ? '_' : '[';

		return text;
	}

	int Draw(FOptionMenuDescriptor*desc, int y, int indent, bool selected)
	{
		if (mEntering)
		{
			// reposition the text so that the cursor is visible when in entering mode.
			FString text = Represent();
			int tlen = SmallFont->StringWidth(text) * CleanXfac_1;
			int newindent = screen->GetWidth() - tlen - CURSORSPACE;
			if (newindent < indent) indent = newindent;
		}
		return FOptionMenuFieldBase::Draw(desc, y, indent, selected);
	}

	bool MenuEvent ( int mkey, bool fromcontroller )
	{
		if ( mkey == MKEY_Enter )
		{
			S_Sound( CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE );
			strcpy( mEditName, GetCVarString() );
			mEntering = true;
			DMenu* input = new DTextEnterMenu ( DMenu::CurrentMenu, mEditName, sizeof mEditName, 2, fromcontroller );
			M_ActivateMenu( input );
			return true;
		}
		else if ( mkey == MKEY_Input )
		{
			if ( mCVar )
			{
				UCVarValue vval;
				vval.String = mEditName;
				mCVar->SetGenericRep( vval, CVAR_String );
			}

			mEntering = false;
			return true;
		}
		else if ( mkey == MKEY_Abort )
		{
			mEntering = false;
			return true;
		}

		return FOptionMenuItem::MenuEvent( mkey, fromcontroller );
	}

private:
	bool mEntering;
	char mEditName[128];
};

//=============================================================================
//
// [TP] FOptionMenuNumberField
//
// A numeric input field widget, for use with number CVars where sliders are inappropriate (i.e.
// where the user is interested in the exact value specifically)
//
//=============================================================================

class FOptionMenuNumberField : public FOptionMenuFieldBase
{
public:
	FOptionMenuNumberField ( const char *label, const char* menu, float minimum, float maximum,
		float step, const char* graycheck )
		: FOptionMenuFieldBase ( label, menu, graycheck ),
		mMinimum ( minimum ),
		mMaximum ( maximum ),
		mStep ( step )
	{
		if ( mMaximum <= mMinimum )
			swapvalues( mMinimum, mMaximum );

		if ( mStep <= 0 )
			mStep = 1;
	}

	bool MenuEvent ( int mkey, bool fromcontroller )
	{
		if ( mCVar )
		{
			float value = mCVar->GetGenericRep( CVAR_Float ).Float;

			if ( mkey == MKEY_Left )
			{
				value -= mStep;

				if ( value < mMinimum )
					value = mMaximum;
			}
			else if ( mkey == MKEY_Right || mkey == MKEY_Enter )
			{
				value += mStep;

				if ( value > mMaximum )
					value = mMinimum;
			}
			else
				return FOptionMenuItem::MenuEvent( mkey, fromcontroller );

			UCVarValue vval;
			vval.Float = value;
			mCVar->SetGenericRep( vval, CVAR_Float );
			S_Sound( CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE );
		}

		return true;
	}

private:
	float mMinimum;
	float mMaximum;
	float mStep;
};
