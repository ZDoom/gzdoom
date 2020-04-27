#pragma once

#include "textures.h"
//-----------------------------------------------------------------------------
//
// This is not a real texture but will be added to the texture manager
// so that it can be handled like any other sky.
//
//-----------------------------------------------------------------------------

class FSkyBox : public FImageTexture
{
public:

	FTexture *previous;
	FTexture * faces[6];
	bool fliptop;

	FSkyBox(const char *name);
	void SetSize();

	bool Is3Face() const
	{
		return faces[5] == nullptr;
	}

	bool IsFlipped() const
	{
		return fliptop;
	}
};
