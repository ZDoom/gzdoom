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

#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "screen_triangle.h"

class PolyDrawArgs
{
public:
	void SetTexture(const uint8_t *texels, int width, int height);
	void SetTexture2(const uint8_t* texels, int width, int height);
	void SetDepthTest(bool enable) { mDepthTest = enable; }
	void SetStencilTest(bool enable) { mStencilTest = enable; }
	void SetStencilTestValue(uint8_t stencilTestValue) { mStencilTestValue = stencilTestValue; }
	void SetWriteColor(bool enable) { mWriteColor = enable; }
	void SetWriteStencil(bool enable, uint8_t stencilWriteValue = 0) { mWriteStencil = enable; mStencilWriteValue = stencilWriteValue; }
	void SetWriteDepth(bool enable) { mWriteDepth = enable; }

	bool WriteColor() const { return mWriteColor; }

	const uint8_t *TexturePixels() const { return mTexturePixels; }
	int TextureWidth() const { return mTextureWidth; }
	int TextureHeight() const { return mTextureHeight; }

	const uint8_t* Texture2Pixels() const { return mTexture2Pixels; }
	int Texture2Width() const { return mTexture2Width; }
	int Texture2Height() const { return mTexture2Height; }

	bool WriteStencil() const { return mWriteStencil; }
	bool StencilTest() const { return mStencilTest; }
	uint8_t StencilTestValue() const { return mStencilTestValue; }
	uint8_t StencilWriteValue() const { return mStencilWriteValue; }

	bool DepthTest() const { return mDepthTest; }
	bool WriteDepth() const { return mWriteDepth; }

private:
	bool mDepthTest = false;
	bool mStencilTest = true;
	bool mWriteStencil = true;
	bool mWriteColor = true;
	bool mWriteDepth = true;
	const uint8_t *mTexturePixels = nullptr;
	int mTextureWidth = 0;
	int mTextureHeight = 0;
	const uint8_t* mTexture2Pixels = nullptr;
	int mTexture2Width = 0;
	int mTexture2Height = 0;
	uint8_t mStencilTestValue = 0;
	uint8_t mStencilWriteValue = 0;
};
