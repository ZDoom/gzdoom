//
// Globally visible constants.
//
#pragma once
#define HU_FONTSTART	uint8_t('!')		// the first font characters
#define HU_FONTEND		uint8_t('\377')	// the last font characters

// Calculate # of glyphs in font.
#define HU_FONTSIZE		(HU_FONTEND - HU_FONTSTART + 1)



void InitDoomFonts()
{
	// load the heads-up font
	if (!(SmallFont = V_GetFont("SmallFont", "SMALLFNT")))
	{
		if (fileSystem.CheckNumForName("FONTA_S") >= 0)
		{
			int wadfile = -1;
			auto a = fileSystem.CheckNumForName("FONTA33", ns_graphics);
			if (a != -1) wadfile = fileSystem.GetFileContainer(a);
			if (wadfile > fileSystem.GetBaseNum())
			{
				// The font has been replaced, so we need to create a copy of the original as well.
				SmallFont = new FFont("SmallFont", "FONTA%02u", nullptr, HU_FONTSTART, HU_FONTSIZE, 1, -1);
				SmallFont->SetCursor('[');
			}
			else
			{
				SmallFont = new FFont("SmallFont", "FONTA%02u", "defsmallfont", HU_FONTSTART, HU_FONTSIZE, 1, -1);
				SmallFont->SetCursor('[');
			}
		}
		else if (fileSystem.CheckNumForName("STCFN033", ns_graphics) >= 0)
		{
			int wadfile = -1;
			auto a = fileSystem.CheckNumForName("STCFN065", ns_graphics);
			if (a != -1) wadfile = fileSystem.GetFileContainer(a);
			if (wadfile > fileSystem.GetBaseNum())
			{
				// The font has been replaced, so we need to create a copy of the original as well.
				SmallFont = new FFont("SmallFont", "STCFN%.3d", nullptr, HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART, -1, -1, false, false, true);
			}
			else
			{
				SmallFont = new FFont("SmallFont", "STCFN%.3d", "defsmallfont", HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART, -1, -1, false, false, true);
			}
		}
	}

	// Create the original small font as a fallback for incomplete definitions.
	if (fileSystem.CheckNumForName("FONTA_S") >= 0)
	{
		OriginalSmallFont = new FFont("OriginalSmallFont", "FONTA%02u", "defsmallfont", HU_FONTSTART, HU_FONTSIZE, 1, -1, -1, false, true);
		OriginalSmallFont->SetCursor('[');
	}
	else if (fileSystem.CheckNumForName("STCFN033", ns_graphics) >= 0)
	{
		OriginalSmallFont = new FFont("OriginalSmallFont", "STCFN%.3d", "defsmallfont", HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART, -1, -1, false, true, true);
	}


	if (!(SmallFont2 = V_GetFont("SmallFont2")))	// Only used by Strife
	{
		if (fileSystem.CheckNumForName("STBFN033", ns_graphics) >= 0)
		{
			SmallFont2 = new FFont("SmallFont2", "STBFN%.3d", "defsmallfont2", HU_FONTSTART, HU_FONTSIZE, HU_FONTSTART, -1);
		}
	}

	//This must be read before BigFont so that it can be properly substituted.
	BigUpper = V_GetFont("BigUpper");

	if (!(BigFont = V_GetFont("BigFont")))
	{
		if (fileSystem.CheckNumForName("FONTB_S") >= 0)
		{
			BigFont = new FFont("BigFont", "FONTB%02u", "defbigfont", HU_FONTSTART, HU_FONTSIZE, 1, -1);
		}
	}
	
	if (!BigFont)
	{
		// Load the generic fallback if no BigFont is found.
		BigFont = V_GetFont("BigFont", "ZBIGFONT");
	}

	if (fileSystem.CheckNumForName("FONTB_S") >= 0)
	{
		OriginalBigFont = new FFont("OriginalBigFont", "FONTB%02u", "defbigfont", HU_FONTSTART, HU_FONTSIZE, 1, -1, -1, false, true);
	}
	else
	{
		OriginalBigFont = new FFont("OriginalBigFont", nullptr, "bigfont", HU_FONTSTART, HU_FONTSIZE, 1, -1, -1, false, true);
	}

	// let PWAD BIGFONTs override the stock BIGUPPER font. (This check needs to be made smarter.)
	if (BigUpper && BigFont->GetType() != FFont::Folder && BigUpper->GetType() == FFont::Folder)
	{
		delete BigUpper;
		BigUpper = BigFont;
	}

	if (BigUpper == nullptr)
	{
		BigUpper = BigFont;
	}
	if (!ConFont)
	{
		ConFont = SmallFont;
	}
	if (!(IntermissionFont = FFont::FindFont("IntermissionFont")))
	{
		if (TexMan.CheckForTexture("WINUM0", ETextureType::MiscPatch).isValid())
		{
			IntermissionFont = FFont::FindFont("IntermissionFont_Doom");
		}
		if (IntermissionFont == nullptr)
		{
			IntermissionFont = BigFont;
		}
	}
	// This can only happen if gzdoom.pk3 is corrupted. ConFont should always be present.
	if (ConFont == nullptr)
	{
		I_FatalError("Console font not found.");
	}
	// SmallFont and SmallFont2 have no default provided by the engine. BigFont only has in non-Raven games.
	if (OriginalSmallFont == nullptr)
	{
		OriginalSmallFont = ConFont;
	}
	if (SmallFont == nullptr)
	{
		SmallFont = OriginalSmallFont;
	}
	if (SmallFont2 == nullptr)
	{
		SmallFont2 = SmallFont;
	}
	if (BigFont == nullptr)
	{
		BigFont = OriginalBigFont;
	}
	AlternativeSmallFont = OriginalSmallFont;
	AlternativeBigFont = OriginalBigFont;
}
