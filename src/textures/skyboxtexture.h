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

	FTexture * faces[6];
	bool fliptop;

	FSkyBox(const char *name = nullptr);
	TArray<uint8_t> Get8BitPixels(bool alphatex);
	FBitmap GetBgraBitmap(PalEntry *, int *trans) override;

	void SetSize()
	{
		if (faces[0]) 
		{
			CopySize(faces[0]);
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

	FImageSource *GetImage() const override
	{
		return faces[0] ? faces[0]->GetImage() : nullptr;
	}
};
