

// This is a font character that reads RLE compressed data.
class FFontChar2 : public FImageSource
{
public:
	FFontChar2 (int sourcelump, int sourcepos, int width, int height, int leftofs=0, int topofs=0);

	PalettedPixels CreatePalettedPixels(int conversion) override;
	int CopyPixels(FBitmap* bmp, int conversion);

	void SetSourceRemap(const PalEntry* sourceremap)
	{
		SourceRemap = sourceremap;
	}

protected:
	int SourceLump;
	int SourcePos;
	const PalEntry *SourceRemap;
};
