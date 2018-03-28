#ifndef __2DDRAWER_H
#define __2DDRAWER_H

#include "tarray.h"
#include "textures.h"
#include "v_palette.h"
#include "r_data/renderstyle.h"
#include "r_data/colormaps.h"

struct DrawParms;

class F2DDrawer
{
public:

	enum EDrawType : uint8_t
	{
		DrawTypeTriangles,
		DrawTypeLines,
		DrawTypePoints,
	};

	enum ETextureDrawMode : uint8_t
	{
		DTM_Normal = 0,
		DTM_Stencil = 1,
		DTM_Opaque = 2,
		DTM_Invert = 3,
		DTM_AlphaTexture = 4,
		DTM_InvertOpaque = 6,
	};

	enum ETextureFlags : uint8_t
	{
		DTF_Wrap = 1,
		DTF_Scissor = 2,
		DTF_SpecialColormap = 4,
		DTF_IngameLighting = 8
	};


	// This vertex type is hardware independent and needs conversion when put into a buffer.
	struct TwoDVertex
	{
		float x, y, z;
		float u, v;
		PalEntry color0;

		void Set(float xx, float zz, float yy)
		{
			x = xx;
			z = zz;
			y = yy;
			u = 0;
			v = 0;
			color0 = 0;
		}

		void Set(double xx, double zz, double yy, double uu, double vv, PalEntry col)
		{
			x = (float)xx;
			z = (float)zz;
			y = (float)yy;
			u = (float)uu;
			v = (float)vv;
			color0 = col;
		}

	};
	
	struct RenderCommand
	{
		EDrawType mType;
		int mVertIndex;
		int mVertCount;
		int mIndexIndex;
		int mIndexCount;

		FTexture *mTexture;
		FRemapTable *mTranslation;
		int mScissor[4];
		uint32_t mColorOverlay;
		int mDesaturate;
		FRenderStyle mRenderStyle;
		PalEntry mColor1, mColor2;	// Can either be the overlay color or the special colormap ramp. Both features can not be combined.
		ETextureDrawMode mDrawMode;
		uint8_t mFlags;

		RenderCommand()
		{
			memset(this, 0, sizeof(*this));
		}

		// If these fields match, two draw commands can be batched.
		bool isCompatible(const RenderCommand &other) const
		{
			return mTexture == other.mTexture &&
				mType == other.mType &&
				mTranslation == other.mTranslation &&
				!memcmp(mScissor, other.mScissor, sizeof(mScissor)) &&
				other.mColorOverlay == 0 && mColorOverlay == 0 &&
				mDesaturate == other.mDesaturate &&
				mRenderStyle == other.mRenderStyle &&
				mDrawMode == other.mDrawMode &&
				mFlags == other.mFlags &&
				mColor1 == other.mColor1 &&
				mColor2 == other.mColor2;

		}
	};

	TArray<int> mIndices;
	TArray<TwoDVertex> mVertices;
	TArray<RenderCommand> mData;
	
	int AddCommand(const RenderCommand *data);
	void AddIndices(int firstvert, int count, ...);
	bool SetStyle(FTexture *tex, DrawParms &parms, PalEntry &color0, RenderCommand &quad);
	void SetColorOverlay(PalEntry color, float alpha, PalEntry &vertexcolor, PalEntry &overlaycolor);

public:
	void AddTexture(FTexture *img, DrawParms &parms);
	void AddPoly(FTexture *texture, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel);
	void AddFlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin);

	void AddColorOnlyQuad(int left, int top, int width, int height, PalEntry color);

	void AddDim(PalEntry color, float damount, int x1, int y1, int w, int h);
	void AddClear(int left, int top, int right, int bottom, int palcolor, uint32_t color);
	
		
	void AddLine(int x1, int y1, int x2, int y2, int palcolor, uint32_t color);
	void AddPixel(int x1, int y1, int palcolor, uint32_t color);

	void Clear();
};


#endif
