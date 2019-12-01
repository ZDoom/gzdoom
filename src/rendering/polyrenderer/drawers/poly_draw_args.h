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

class PolyRenderThread;
class FSoftwareTexture;
class Mat4f;

enum class PolyDrawMode
{
	Points,
	Lines,
	Triangles,
	TriangleFan,
	TriangleStrip
};

class PolyClipPlane
{
public:
	PolyClipPlane() : A(0.0f), B(0.0f), C(0.0f), D(1.0f) { }
	PolyClipPlane(float a, float b, float c, float d) : A(a), B(b), C(c), D(d) { }

	float A, B, C, D;
};

struct TriVertex
{
	TriVertex() { }
	TriVertex(float x, float y, float z, float w, float u, float v) : x(x), y(y), z(z), w(w), u(u), v(v) { }

	float x, y, z, w;
	float u, v;
};

struct PolyLight
{
	uint32_t color;
	float x, y, z;
	float radius;
};

class PolyDrawArgs
{
public:
	//void SetClipPlane(int index, const PolyClipPlane &plane) { mClipPlane[index] = plane; }
	void SetTexture(const uint8_t *texels, int width, int height);
	void SetTexture(FSoftwareTexture *texture, FRenderStyle style);
	void SetTexture(FSoftwareTexture *texture, uint32_t translationID, FRenderStyle style);
	void SetTexture2(const uint8_t* texels, int width, int height);
	void SetLight(FSWColormap *basecolormap, uint32_t lightlevel, double globVis, bool fixed);
	void SetNoColormap() { mLight = 255; mFixedLight = true; mLightRed = 256; mLightGreen = 256; mLightBlue = 256; mLightAlpha = 256; mFadeRed = 0; mFadeGreen = 0; mFadeBlue = 0; mFadeAlpha = 0; mDesaturate = 0; mSimpleShade = true; mColormaps = nullptr; }
	void SetDepthTest(bool enable) { mDepthTest = enable; }
	void SetStencilTest(bool enable) { mStencilTest = enable; }
	void SetStencilTestValue(uint8_t stencilTestValue) { mStencilTestValue = stencilTestValue; }
	void SetWriteColor(bool enable) { mWriteColor = enable; }
	void SetWriteStencil(bool enable, uint8_t stencilWriteValue = 0) { mWriteStencil = enable; mStencilWriteValue = stencilWriteValue; }
	void SetWriteDepth(bool enable) { mWriteDepth = enable; }
	void SetStyle(TriBlendMode blendmode, double alpha = 1.0) { mBlendMode = blendmode; mAlpha = (uint32_t)(alpha * 256.0 + 0.5); }
	void SetStyle(const FRenderStyle &renderstyle, double alpha, uint32_t fillcolor, uint32_t translationID, FSoftwareTexture *texture, bool fullbright);
	void SetColor(uint32_t bgra, uint8_t palindex);
	void SetLights(PolyLight *lights, int numLights) { mLights = lights; mNumLights = numLights; }
	void SetDynLightColor(uint32_t color) { mDynLightColor = color; }

	//const PolyClipPlane &ClipPlane(int index) const { return mClipPlane[index]; }

	bool WriteColor() const { return mWriteColor; }

	FSoftwareTexture *Texture() const { return mTexture; }
	const uint8_t *TexturePixels() const { return mTexturePixels; }
	int TextureWidth() const { return mTextureWidth; }
	int TextureHeight() const { return mTextureHeight; }
	const uint8_t *Translation() const { return mTranslation; }

	const uint8_t* Texture2Pixels() const { return mTexture2Pixels; }
	int Texture2Width() const { return mTexture2Width; }
	int Texture2Height() const { return mTexture2Height; }

	bool WriteStencil() const { return mWriteStencil; }
	bool StencilTest() const { return mStencilTest; }
	uint8_t StencilTestValue() const { return mStencilTestValue; }
	uint8_t StencilWriteValue() const { return mStencilWriteValue; }

	bool DepthTest() const { return mDepthTest; }
	bool WriteDepth() const { return mWriteDepth; }

	TriBlendMode BlendMode() const { return mBlendMode; }
	uint32_t Color() const { return mColor; }
	uint32_t Alpha() const { return mAlpha; }

	float GlobVis() const { return mGlobVis; }
	uint32_t Light() const { return mLight; }
	const uint8_t *BaseColormap() const { return mColormaps; }
	uint16_t ShadeLightAlpha() const { return mLightAlpha; }
	uint16_t ShadeLightRed() const { return mLightRed; }
	uint16_t ShadeLightGreen() const { return mLightGreen; }
	uint16_t ShadeLightBlue() const { return mLightBlue; }
	uint16_t ShadeFadeAlpha() const { return mFadeAlpha; }
	uint16_t ShadeFadeRed() const { return mFadeRed; }
	uint16_t ShadeFadeGreen() const { return mFadeGreen; }
	uint16_t ShadeFadeBlue() const { return mFadeBlue; }
	uint16_t ShadeDesaturate() const { return mDesaturate; }

	bool SimpleShade() const { return mSimpleShade; }
	bool NearestFilter() const { return mNearestFilter; }
	bool FixedLight() const { return mFixedLight; }

	PolyLight *Lights() const { return mLights; }
	int NumLights() const { return mNumLights; }
	uint32_t DynLightColor() const { return mDynLightColor; }

	const FVector3 &Normal() const { return mNormal; }
	void SetNormal(const FVector3 &normal) { mNormal = normal; }

private:
	bool mDepthTest = false;
	bool mStencilTest = true;
	bool mWriteStencil = true;
	bool mWriteColor = true;
	bool mWriteDepth = true;
	FSoftwareTexture *mTexture = nullptr;
	const uint8_t *mTexturePixels = nullptr;
	int mTextureWidth = 0;
	int mTextureHeight = 0;
	const uint8_t *mTranslation = nullptr;
	const uint8_t* mTexture2Pixels = nullptr;
	int mTexture2Width = 0;
	int mTexture2Height = 0;
	uint8_t mStencilTestValue = 0;
	uint8_t mStencilWriteValue = 0;
	const uint8_t *mColormaps = nullptr;
	PolyClipPlane mClipPlane[3];
	TriBlendMode mBlendMode = TriBlendMode::Fill;
	uint32_t mLight = 0;
	uint32_t mColor = 0;
	uint32_t mAlpha = 0;
	uint16_t mLightAlpha = 0;
	uint16_t mLightRed = 0;
	uint16_t mLightGreen = 0;
	uint16_t mLightBlue = 0;
	uint16_t mFadeAlpha = 0;
	uint16_t mFadeRed = 0;
	uint16_t mFadeGreen = 0;
	uint16_t mFadeBlue = 0;
	uint16_t mDesaturate = 0;
	float mGlobVis = 0.0f;
	bool mSimpleShade = true;
	bool mNearestFilter = true;
	bool mFixedLight = false;
	PolyLight *mLights = nullptr;
	int mNumLights = 0;
	FVector3 mNormal;
	uint32_t mDynLightColor = 0;
};
