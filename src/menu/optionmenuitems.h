void M_DrawConText (int color, int x, int y, const char *str);
void M_DrawSlider (int x, int y, double min, double max, double cur,int fracdigits);



//=============================================================================
//
// opens a submenu, action is a submenu name
//
//=============================================================================

class FOptionMenuItemSubmenu : public FOptionMenuItem
{
public:
	FOptionMenuItemSubmenu(const char *label, const char *menu)
		: FOptionMenuItem(label, menu)
	{
	}

	void Draw(FOptionMenuDescriptor *desc, int y, int indent)
	{
		drawLabel(indent, y, OptionSettings.mFontColorMore);
	}

	bool Activate()
	{
		M_SetMenu(mAction, 0);
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
public:
	FOptionMenuItemSafeCommand(const char *label, const char *menu)
		: FOptionMenuItemCommand(label, menu)
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
		M_StartMessage("Do you really want to do this?", 0);
		return true;
	}
};

//=============================================================================
//
// Change a CVAR, action is the CVAR name
//
//=============================================================================

class FOptionMenuItemOption : public FOptionMenuItem
{
	// action is a CVAR
	FOptionValues *mValues;
	FBaseCVar *mCVar;
	FBoolCVar *mGrayCheck;
	int mSelection;
public:

	FOptionMenuItemOption(const char *label, const char *menu, const char *values, const char *graycheck)
		: FOptionMenuItem(label, menu)
	{
		FOptionValues **opt = OptionValues.CheckKey(values);
		if (opt != NULL) 
		{
			mValues = *opt;
			mSelection = -1;
			mCVar = FindCVar(menu, NULL);
			mGrayCheck = (FBoolCVar*)FindCVar(graycheck, NULL);
			if (mGrayCheck != NULL && mGrayCheck->GetRealType() != CVAR_Bool) mGrayCheck = NULL;
			if (mCVar != NULL)
			{
				UCVarValue cv = mCVar->GetGenericRep(CVAR_Float);
				for(unsigned i=0;i<mValues->mValues.Size(); i++)
				{
					if (fabs(cv.Float - mValues->mValues[i].Value) < FLT_EPSILON)
					{
						mSelection = i;
						break;
					}
				}
			}
		}
		else
		{
			mValues = NULL;
		}
	}

	//=============================================================================
	void Draw(FOptionMenuDescriptor *desc, int y, int indent)
	{
		bool grayed = mGrayCheck != NULL && !(**mGrayCheck);
		drawLabel(indent, y, OptionSettings.mFontColor, grayed);

		if (mValues != NULL && mCVar != NULL)
		{
			int overlay = grayed? MAKEARGB(96,48,0,0) : 0;
			const char *text;
			if (mSelection < 0)
			{
				text = "Unknown";
			}
			else
			{
				text = mValues->mValues[mSelection].Text;
			}
			screen->DrawText (SmallFont, OptionSettings.mFontColorValue, indent + CURSORSPACE, y, 
				text, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);
		}
	}

	//=============================================================================
	bool MenuEvent (int mkey, bool fromcontroller)
	{
		UCVarValue value;
		if (mValues != NULL && mCVar != NULL)
		{
			if (mkey == MKEY_Left)
			{
				if (mSelection == -1) mSelection = 0;
				else if (--mSelection < 0) mSelection = mValues->mValues.Size()-1;
			}
			else if (mkey == MKEY_Right || mkey == MKEY_Enter)
			{
				if (++mSelection >= (int)mValues->mValues.Size()) mSelection = 0;
			}
			else
			{
				return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
			}
			value.Float = (float)mValues->mValues[mSelection].Value;
			mCVar->SetGenericRep (value, CVAR_Float);
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
			return true;
		}
		return false;
	}

	bool Selectable()
	{
		return !(mGrayCheck != NULL && !(**mGrayCheck));
	}
};

/* color option

				}
				else
				{
					screen->DrawText (SmallFont, item->type == cdiscrete ? v : ValueColor,
						indent + cursorspace, y,
						item->type != discretes ? item->e.values[v].name : item->e.valuestrings[v].name.GetChars(),
						DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);
				}

		// print value
*/

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
		if (mParentMenu->IsKindOf(RUNTIME_CLASS(DListMenu)))
		{
			DListMenu *m = barrier_cast<DListMenu*>(mParentMenu);
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

IMPLEMENT_ABSTRACT_CLASS(DEnterKey)

//=============================================================================
//
// // Edit a key binding, Action is the CCMD to bind
//
//=============================================================================

class FOptionMenuItemControl : public FOptionMenuItem
{
	FKeyBindings *mBindings;
	int mKey1, mKey2;
	int mInput;
	bool mWaiting;
public:

	FOptionMenuItemControl(const char *label, const char *menu, FKeyBindings *bindings)
		: FOptionMenuItem(label, menu)
	{
		mBindings = bindings;
		mBindings->GetKeysForCommand(mAction, &mKey1, &mKey2);
		mWaiting = false;
	}

	//=============================================================================
	void Draw(FOptionMenuDescriptor *desc, int y, int indent)
	{
		drawLabel(indent, y, mWaiting? OptionSettings.mFontColorHighlight: OptionSettings.mFontColor);

		char description[64];

		C_NameKeys (description, mKey1, mKey2);
		if (description[0])
		{
			M_DrawConText(CR_WHITE, indent + CURSORSPACE, y-1+OptionSettings.mLabelOffset, description);
		}
		else
		{
			screen->DrawText(SmallFont, CR_BLACK, indent + CURSORSPACE, y + OptionSettings.mLabelOffset, "---",
				DTA_CleanNoMove_1, true, TAG_DONE);
		}
	}

	//=============================================================================
	bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (mkey == MKEY_Input)
		{
			mWaiting = false;
			mBindings->SetBind(mInput, mAction);
			mBindings->GetKeysForCommand(mAction, &mKey1, &mKey2);
			return true;
		}
		else if (mkey == MKEY_Clear)
		{
			mBindings->UnbindACommand(mAction);
			mBindings->GetKeysForCommand(mAction, &mKey1, &mKey2);
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
		mColor = header? OptionSettings.mFontColorHeader : OptionSettings.mFontColor;
	}

	void Draw(FOptionMenuDescriptor *desc, int y, int indent)
	{
		drawLabel(indent, y, mColor);
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
	FOptionMenuItemStaticTextSwitchable(const char *label, const char *label2, FName action, bool header)
		: FOptionMenuItem(label, action, true)
	{
		mColor = header? OptionSettings.mFontColorHeader : OptionSettings.mFontColor;
		mAltText = label2;
		mCurrent = 0;
	}

	void Draw(FOptionMenuDescriptor *desc, int y, int indent)
	{
		int w = SmallFont->StringWidth(mLabel) * CleanXfac_1;
		int x = (screen->GetWidth() - w) / 2;
		screen->DrawText (SmallFont, mColor, x, y, mCurrent? mAltText : mLabel, DTA_CleanNoMove_1, true, TAG_DONE);
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

class FOptionMenuSliderItem : public FOptionMenuItem
{
	// action is a CVAR
	float mMin, mMax, mStep;
	float mValue;
	bool mShowValue;
	FBaseCVar *mCVar;
	float *mPVal;
public:
	FOptionMenuSliderItem(const char *label, const char *menu, double min, double max, double step, bool showval)
		: FOptionMenuItem(label, NAME_None)
	{
		mMin = (float)min;
		mMax = (float)max;
		mStep = (float)step;
		mShowValue = showval;
		mCVar = FindCVar(menu, NULL);
		mPVal = NULL;
	}

	FOptionMenuSliderItem(const char *label, float *pVal, double min, double max, double step, bool showval)
		: FOptionMenuItem(label, NAME_None)
	{
		mMin = (float)min;
		mMax = (float)max;
		mStep = (float)step;
		mShowValue = showval;
		mPVal = pVal;
		mCVar = NULL;
	}

	//=============================================================================
	void Draw(FOptionMenuDescriptor *desc, int y, int indent)
	{
		drawLabel(indent, y, OptionSettings.mFontColor);

		UCVarValue value;

		if (mCVar != NULL)
		{
			value = mCVar->GetGenericRep(CVAR_Float);
		}
		else if (mPVal != NULL)
		{
			value.Float = *mPVal;		
		}
		else return;
		M_DrawSlider (indent + CURSORSPACE, y + OptionSettings.mLabelOffset, mMin, mMax, value.Float, mShowValue? 1:0);
	}

	//=============================================================================
	bool MenuEvent (int mkey, bool fromcontroller)
	{
		UCVarValue value;

		if (mCVar != NULL || mPVal != NULL)
		{
			if (mCVar != NULL)
			{
				value = mCVar->GetGenericRep(CVAR_Float);
			}
			else if (mPVal != NULL)
			{
				value.Float = *mPVal;		
			}

			if (mkey == MKEY_Left)
			{
				value.Float -= mStep;
			}
			else if (mkey == MKEY_Right)
			{
				value.Float += mStep;
			}
			else
			{
				return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
			}
			value.Float = clamp(value.Float, mMin, mMax);
			if (mCVar != NULL) mCVar->SetGenericRep (value, CVAR_Float);
			else if (mPVal != NULL) *mPVal = value.Float;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
			return true;
		}
		return false;
	}

};


/*
		if (item->type != screenres &&
			item->type != joymore && (item->type != discrete || item->c.discretecenter != 1))
*/

		