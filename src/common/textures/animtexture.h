#pragma once

#include "textures.h"


class AnimTexture : public FTexture
{
	uint8_t Palette[768];
	TArray<uint8_t> Image;
	int pixelformat;
public:
	enum
	{
		Paletted = 0,
		RGB = 1,
		YUV = 2
	};
	AnimTexture() = default;
	void SetFrameSize(int format, int width, int height);
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
	void SetSize(int format, int width, int height);
	void SetFrame(const uint8_t* palette, const void* data);
	FGameTexture* GetFrame()
	{
		return tex[active];
	}

	FTextureID GetFrameID()
	{
		return tex[active]->GetID();
	}
};
