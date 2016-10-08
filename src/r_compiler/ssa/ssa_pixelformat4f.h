
#pragma once

#include "ssa_int.h"
#include "ssa_float_ptr.h"

class SSAPixelFormat4f
{
public:
	SSAPixelFormat4f() { }
	SSAPixelFormat4f(SSAFloatPtr pixels, SSAInt width, SSAInt height) : _pixels(pixels) { }

	SSAFloatPtr pixels() { return _pixels; }
	SSAFloatPtr pixels() const { return _pixels; }

	SSAVec4f get4f(SSAInt index, bool constantScopeDomain) const
	{
		return _pixels[index * 4].load_vec4f(constantScopeDomain);
	}

	void set4f(SSAInt index, const SSAVec4f &pixel)
	{
		_pixels[index * 4].store_vec4f(pixel);
	}

protected:
	SSAFloatPtr _pixels;
};
