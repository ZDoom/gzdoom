
#pragma once

#include "ssa_int.h"
#include "ssa_ubyte_ptr.h"

class SSAPixelFormat4ub
{
public:
	SSAPixelFormat4ub() { }
	SSAPixelFormat4ub(SSAUBytePtr pixels, SSAInt width, SSAInt height) : _pixels(pixels) { }

	SSAUBytePtr pixels() { return _pixels; }
	SSAUBytePtr pixels() const { return _pixels; }

	SSAVec4f get4f(SSAInt index) const
	{
		return SSAVec4f(_pixels[index * 4].load_vec4ub()) * (1.0f / 255.0f);
	}

	void set4f(SSAInt index, const SSAVec4f &pixel)
	{
		_pixels[index * 4].store_vec4ub(SSAVec4i(pixel * 255.0f));
	}

private:
	SSAUBytePtr _pixels;
};
