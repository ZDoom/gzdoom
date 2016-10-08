
#pragma once

#include "ssa_int.h"
#include "ssa_ubyte_ptr.h"

class SSAPixelFormat4ub_rev
{
public:
	SSAPixelFormat4ub_rev() { }
	SSAPixelFormat4ub_rev(SSAUBytePtr pixels, SSAInt width, SSAInt height) : _pixels(pixels) { }

	SSAUBytePtr pixels() { return _pixels; }
	SSAUBytePtr pixels() const { return _pixels; }

	SSAVec4f get4f(SSAInt index, bool constantScopeDomain) const
	{
		return SSAVec4f::shuffle(SSAVec4f(_pixels[index * 4].load_vec4ub(constantScopeDomain)) * (1.0f / 255.0f), 3, 2, 1, 0);
	}

	void set4f(SSAInt index, const SSAVec4f &pixel)
	{
		_pixels[index * 4].store_vec4ub(SSAVec4i(SSAVec4f::shuffle(pixel * 255.0f, 3, 2, 1, 0)));
	}

public:
	SSAUBytePtr _pixels;
};
