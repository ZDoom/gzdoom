#pragma once

#include "textures.h"


class AnimTexture : public FTexture
{
	uint8_t Palette[768];
	TArray<uint8_t> Image;
public:
	AnimTexture() = default;
	void SetFrameSize(int width, int height);
	void SetFrame(const uint8_t* palette, const void* data);
	virtual FBitmap GetBgraBitmap(const PalEntry* remap, int* trans) override;
};

class AnimTextures
{
	int active;
	FGameTexture* tex[2];

public:
	AnimTextures();
	~AnimTextures();
	void SetSize(int width, int height);
	void SetFrame(const uint8_t* palette, const void* data);
	FGameTexture* GetFrame();
};
