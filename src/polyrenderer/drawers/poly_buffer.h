/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#pragma once

#include <vector>

struct TriVertex;

class PolyZBuffer
{
public:
	static PolyZBuffer *Instance();
	void Resize(int newwidth, int newheight);
	int Width() const { return width; }
	int Height() const { return height; }
	int BlockWidth() const { return (width + 7) / 8; }
	int BlockHeight() const { return (height + 7) / 8; }
	float *Values() { return values.data(); }

private:
	int width;
	int height;
	std::vector<float> values;
};

class PolyStencilBuffer
{
public:
	static PolyStencilBuffer *Instance();
	void Clear(int newwidth, int newheight, uint8_t stencil_value = 0);
	int Width() const { return width; }
	int Height() const { return height; }
	int BlockWidth() const { return (width + 7) / 8; }
	int BlockHeight() const { return (height + 7) / 8; }
	uint8_t *Values() { return values.data(); }
	uint32_t *Masks() { return masks.data(); }

private:
	int width;
	int height;

	// 8x8 blocks of stencil values, plus a mask for each block indicating if values are the same for early out stencil testing
	std::vector<uint8_t> values;
	std::vector<uint32_t> masks;
};
