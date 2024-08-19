#pragma once

#include "textures.h"


class FireTexture : public FTexture
{
	TArray<uint8_t> Image;
	TArray<PalEntry> Palette;

public:
	FireTexture();
	void SetPalette(TArray<PalEntry>& colors);
	void Update();
	virtual FBitmap GetBgraBitmap(const PalEntry* remap, int* trans) override;
	virtual TArray<uint8_t> Get8BitPixels(bool alphatex) override;
};