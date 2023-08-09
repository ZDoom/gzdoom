
#include "engineerrors.h"
#include "textures.h"
#include "image.h"
#include "v_font.h"
#include "filesystem.h"
#include "utf8.h"
#include "sc_man.h"
#include "texturemanager.h"
#include "fontinternals.h"
#include "schrift.h"

class FTTFGlyph : public FImageSource
{
public:
	FTTFGlyph(int width, int height, int leftoffset, int topoffset, TArray<uint8_t> pixels, PalEntry* palette)
	{
		Width = width;
		Height = height;
		LeftOffset = leftoffset;
		TopOffset = topoffset;
		Pixels = std::move(pixels);
		Palette = palette;
	}

	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override
	{
		PalettedPixels OutPixels(Pixels.Size());
		memcpy(OutPixels.Data(), Pixels.Data(), Pixels.Size());
		return OutPixels;
	}

	int CopyPixels(FBitmap* bmp, int conversion)
	{
		if (conversion == luminance)
			conversion = normal;	// luminance images have no use as an RGB source.

		auto ppix = CreatePalettedPixels(conversion);
		bmp->CopyPixelData(0, 0, ppix.Data(), Width, Height, 1, Width, 0, Palette);
		return 0;
	}

private:
	TArray<uint8_t> Pixels;
	PalEntry* Palette = nullptr;
};

class FTTFFont : public FFont
{
public:
	FTTFFont(const char* fontname, int height, int lump) : FFont(lump)
	{
		auto lumpdata = fileSystem.ReadFile(lump);

		SFT sft = {};
		sft.xScale = height;
		sft.yScale = height;
		sft.flags = SFT_DOWNWARD_Y;
		sft.font = sft_loadmem(lumpdata.GetMem(), lumpdata.GetSize());
		if (!sft.font)
			I_FatalError("Could not load truetype font file");

		SFT_LMetrics lmtx;
		if (sft_lmetrics(&sft, &lmtx) < 0)
			I_FatalError("Could not get truetype font metrics");

		FontName = fontname;
		FontHeight = (int)std::floor((lmtx.ascender + lmtx.descender + lmtx.lineGap)) + 2;

		{
			SFT_Glyph gid;
			SFT_GMetrics mtx;
			if (sft_lookup(&sft, 32, &gid) < 0)
				I_FatalError("Could not find the glyph id for the space character");

			if (sft_gmetrics(&sft, gid, &mtx) < 0)
				I_FatalError("Could not get the glyph metrics for the space character");

			SpaceWidth = (int)std::round(mtx.advanceWidth);
		}

		GlobalKerning = 0;

		FirstChar = 10;
		LastChar = 255;

		for (int i = 0; i < 256; i++)
		{
			palette[i] = PalEntry(i, 255, 255, 255);
		}

		Chars.Resize(LastChar - FirstChar + 1);
		for (int i = FirstChar; i <= LastChar; i++)
		{
			SFT_Glyph gid;
			if (sft_lookup(&sft, i, &gid) < 0)
				continue;

			SFT_GMetrics mtx;
			if (sft_gmetrics(&sft, gid, &mtx) < 0)
				continue;

			if (mtx.minWidth <= 0 || mtx.minHeight <= 0)
				continue;

			SFT_Image img = {};
			img.width = (mtx.minWidth + 3) & ~3;
			img.height = mtx.minHeight;

			TArray<uint8_t> pixels(img.width * img.height, true);
			img.pixels = pixels.Data();
			if (sft_render(&sft, gid, img) < 0)
				continue;

			Chars[i - FirstChar].OriginalPic = MakeGameTexture(new FImageTexture(CreateGlyph(lmtx, mtx, img.width, img.height, std::move(pixels))), nullptr, ETextureType::FontChar);
			Chars[i - FirstChar].XMove = (int)std::floor(mtx.advanceWidth) + 1;
			TexMan.AddGameTexture(Chars[i - FirstChar].OriginalPic);
		}
	}

	void LoadTranslations() override
	{
		int minlum = 0;
		int maxlum = 255;

		Translations.Resize(NumTextColors);
		for (int i = 0; i < NumTextColors; i++)
		{
			if (i == CR_UNTRANSLATED) Translations[i] = 0;
			else Translations[i] = LuminosityTranslation(i * 2, minlum, maxlum);
		}
	}

	virtual FTTFGlyph* CreateGlyph(const SFT_LMetrics& lmtx, const SFT_GMetrics& mtx, int width, int height, TArray<uint8_t> pixels)
	{
		return new FTTFGlyph(width, height, (int)std::round(-mtx.leftSideBearing), -mtx.yOffset - (int)std::round(lmtx.ascender + lmtx.descender + lmtx.lineGap * 0.5), std::move(pixels), palette);
	}

	PalEntry palette[256];
};

FFont* CreateTTFFont(const char* fontname, int height, int lump)
{
	return new FTTFFont(fontname, height, lump);
}
