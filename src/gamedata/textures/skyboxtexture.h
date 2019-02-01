#pragma once

#include "textures.h"
//-----------------------------------------------------------------------------
//
// This is not a real texture but will be added to the texture manager
// so that it can be handled like any other sky.
//
//-----------------------------------------------------------------------------

class FSkyBox : public FTexture
{
public:

	FTexture *previous;
	FTexture * faces[6];
	bool fliptop;

	FSkyBox(const char *name);
	TArray<uint8_t> Get8BitPixels(bool alphatex);
	FBitmap GetBgraBitmap(PalEntry *, int *trans) override;
	FImageSource *GetImage() const override;


	void SetSize()
	{
		if (!previous && faces[0]) previous = faces[0];
		if (previous)
		{
			CopySize(previous);
		}
	}

	bool Is3Face() const
	{
		return faces[5] == nullptr;
	}

	bool IsFlipped() const
	{
		return fliptop;
	}
};
