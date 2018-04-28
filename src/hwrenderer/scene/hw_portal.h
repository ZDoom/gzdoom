#pragma once

#include "hwrenderer/textures/hw_material.h"

struct GLSkyInfo
{
	float x_offset[2];
	float y_offset;		// doubleskies don't have a y-offset
	FMaterial * texture[2];
	FTextureID skytexno1;
	bool mirrored;
	bool doublesky;
	bool sky2;
	PalEntry fadecolor;

	bool operator==(const GLSkyInfo & inf)
	{
		return !memcmp(this, &inf, sizeof(*this));
	}
	bool operator!=(const GLSkyInfo & inf)
	{
		return !!memcmp(this, &inf, sizeof(*this));
	}
	void init(int sky1, PalEntry fadecolor);
};

struct GLHorizonInfo
{
	GLSectorPlane plane;
	int lightlevel;
	FColormap colormap;
	PalEntry specialcolor;
};

