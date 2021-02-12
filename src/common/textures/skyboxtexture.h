#pragma once

#include "textures.h"

//-----------------------------------------------------------------------------
//
// Todo: Get rid of this
// The faces can easily be stored in the material layer array
//
//-----------------------------------------------------------------------------

class FSkyBox : public FImageTexture
{
public:

	FGameTexture* previous;
	FGameTexture* faces[6];	// the faces need to be full materials as they can have all supported effects.
	bool fliptop;

	FSkyBox(const char* name);
	void SetSize();

	bool Is3Face() const
	{
		return faces[5] == nullptr;
	}

	bool IsFlipped() const
	{
		return fliptop;
	}

	FGameTexture* GetSkyFace(int num) const 
	{
		return faces[num];
	}

	bool GetSkyFlip() const
	{
		return fliptop;
	}
};
