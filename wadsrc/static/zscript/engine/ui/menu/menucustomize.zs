// This class allows global customization of certain menu aspects, e.g. replacing the menu caption.

class MenuDelegateBase ui
{
	virtual int DrawCaption(String title, Font fnt, int y, bool drawit)
	{
		if (drawit) screen.DrawText(fnt, OptionMenuSettings.mTitleColor, (screen.GetWidth() - fnt.StringWidth(title) * CleanXfac_1) / 2, 10 * CleanYfac_1, title, DTA_CleanNoMove_1, true);
		return (y + fnt.GetHeight()) * CleanYfac_1;	// return is spacing in screen pixels.
	}

	virtual void PlaySound(Name sound)
	{
	}

	virtual bool DrawSelector(ListMenuDescriptor desc)
	{
		return false;
	}

	virtual void MenuDismissed()
	{
		// overriding this allows to execute special actions when the menu closes
	}

	virtual Font PickFont(Font fnt)
	{
		if (generic_ui || !fnt) return NewSmallFont;
		if (fnt == SmallFont) return AlternativeSmallFont;
		if (fnt == BigFont) return AlternativeBigFont;
		return fnt;
	}
}
