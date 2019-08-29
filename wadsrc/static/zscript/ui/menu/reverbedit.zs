
class ReverbEdit : OptionMenu
{
	static native double GetValue(int index);
	static native double SetValue(int index, double value);
	static native bool GrayCheck();
	static native string, int GetSelectedEnvironment();
	static native void FillSelectMenu(String ccmd, OptionMenuDescriptor desc);
	static native void FillSaveMenu(OptionMenuDescriptor desc);
	static native int GetSaveSelection(int num);
	static native void ToggleSaveSelection(int num);

	override void Init(Menu parent, OptionMenuDescriptor desc)
	{
		super.Init(parent, desc);
		OnReturn();
	}
	
	override void OnReturn()
	{
		string env;
		int id;
		
		[env, id] = GetSelectedEnvironment();
		
		let li = GetItem('EvironmentName');
		if (li != NULL)
		{
			if (id != -1)
			{
				li.SetValue(0, 1);
				li.SetString(0, env);
			}
			else
			{
				li.SetValue(0, 0);
			}
		}
		li = GetItem('EvironmentID');
		if (li != NULL)
		{
			if (id != -1)
			{
				li.SetValue(0, 1);
				li.SetString(0, String.Format("%d, %d", (id >> 8) & 255, id & 255));
			}
			else
			{
				li.SetValue(0, 0);
			}
		}
	}
}

class ReverbSelect : OptionMenu
{
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Init(Menu parent, OptionMenuDescriptor desc)
	{
		ReverbEdit.FillSelectMenu("selectenvironment", desc);
		super.Init(parent, desc);
	}
}

class ReverbSave : OptionMenu
{
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Init(Menu parent, OptionMenuDescriptor desc)
	{
		ReverbEdit.FillSaveMenu(desc);
		super.Init(parent, desc);
	}
}

//=============================================================================
//
// Change a CVAR, command is the CVAR name
//
//=============================================================================

class OptionMenuItemReverbSaveSelect : OptionMenuItemOptionBase
{
	int mValIndex;

	OptionMenuItemReverbSaveSelect Init(String label, int index, Name values)
	{
		Super.Init(label, 'None', values, null, 0);
		mValIndex = index;
		return self;
	}

	//=============================================================================
	override int GetSelection()
	{
		return ReverbEdit.GetSaveSelection(mValIndex);
	}

	override void SetSelection(int Selection)
	{
		ReverbEdit.ToggleSaveSelection(mValIndex);
	}
}


//=============================================================================
//
// opens a submenu, command is a submenu name
//
//=============================================================================

class OptionMenuItemReverbSelect : OptionMenuItemSubMenu
{
	OptionMenuItemReverbSelect Init(String label, Name command)
	{
		Super.init(label, command, 0, false);
		return self;
	}

	
	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		int x = drawLabel(indent, y, selected? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor);

		String text = ReverbEdit.GetSelectedEnvironment();
		drawValue(indent, y, OptionMenuSettings.mFontColorValue, text);
		return indent;
	}
}


//=============================================================================
//
//
//
//=============================================================================

class OptionMenuItemReverbOption : OptionMenuItemOptionBase
{
	int mValIndex;

	OptionMenuItemReverbOption Init(String label, int valindex, Name values)
	{
		Super.Init(label, "", values, null, false);
		mValIndex = valindex;
		return self;
	}

	override bool isGrayed()
	{
		return ReverbEdit.GrayCheck();
	}

	override int GetSelection()
	{
		return int(ReverbEdit.GetValue(mValIndex));
	}
	
	override void SetSelection(int Selection)
	{
		ReverbEdit.SetValue(mValIndex, Selection);
	}
}

//=============================================================================
//
//
//
//=============================================================================

class OptionMenuItemSliderReverbEditOption : OptionMenuSliderBase
{
	int mValIndex;
	String mEditValue;
	TextEnterMenu mEnter;
	
	OptionMenuItemSliderReverbEditOption Init(String label, double min, double max, double step, int showval, int valindex)
	{
		Super.Init(label, min, max, step, showval);
		mValIndex = valindex;
		mEnter = null;
		return self;
	}
	

	override double GetSliderValue()
	{
		return ReverbEdit.GetValue(mValIndex);
	}

	override void SetSliderValue(double val)
	{
		ReverbEdit.SetValue(mValIndex, val);
	}
	
	override bool Selectable()
	{
		return !ReverbEdit.GrayCheck();
	}

	virtual String Represent()
	{
		return mEnter.GetText() .. Menu.OptionFont().GetCursor();
	}

	//=============================================================================
	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected ? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor, ReverbEdit.GrayCheck());

		mDrawX = indent + CursorSpace();
		if (mEnter)
		{
			drawText(mDrawX, y, OptionMenuSettings.mFontColorValue, Represent());
		}
		else
		{
			DrawSlider (mDrawX, y, mMin, mMax, GetSliderValue(), mShowValue, indent);
		}
		return indent;
	}
	
	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mkey == Menu.MKEY_Enter)
		{
			Menu.MenuSound("menu/choose");
			mEnter = TextEnterMenu.OpenTextEnter(Menu.GetCurrentMenu(), Menu.OptionFont(), String.Format("%.3f", GetSliderValue()), -1, fromcontroller);
			mEnter.ActivateMenu();
			return true;
		}
		else if (mkey == Menu.MKEY_Input)
		{
			String val = mEnter.GetText();
			SetSliderValue(val.toDouble());
			mEnter = null;
			return true;
		}
		else if (mkey == Menu.MKEY_Abort)
		{
			mEnter = null;
			return true;
		}

		return Super.MenuEvent(mkey, fromcontroller);
	}
	
}

