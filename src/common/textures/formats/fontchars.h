

// This is a font character that loads a texture and recolors it.
class FFontChar1 : public FImageSource
{
public:
   FFontChar1 (FImageSource *sourcelump);
   TArray<uint8_t> CreatePalettedPixels(int conversion) override;
   void SetSourceRemap(const uint8_t *sourceremap)  {  SourceRemap = sourceremap;  }
   const uint8_t *ResetSourceRemap() { auto p = SourceRemap; SourceRemap = nullptr; return p; }
   FImageSource *GetBase() const { return BaseTexture; }

protected:

   FImageSource *BaseTexture;
   const uint8_t *SourceRemap;
};

// This is a font character that reads RLE compressed data.
class FFontChar2 : public FImageSource
{
public:
	FFontChar2 (int sourcelump, int sourcepos, int width, int height, int leftofs=0, int topofs=0);

	TArray<uint8_t> CreatePalettedPixels(int conversion) override;
	void SetSourceRemap(const uint8_t *sourceremap);

protected:
	int SourceLump;
	int SourcePos;
	const uint8_t *SourceRemap;
};
