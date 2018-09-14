#ifndef __2DDRAWER_H
#define __2DDRAWER_H

#include "tarray.h"
#include "textures.h"
#include "v_palette.h"
#include "r_data/renderstyle.h"
#include "r_data/colormaps.h"

struct DrawParms;

// intermediate struct for shape drawing

enum EClearWhich
{
	C_Verts = 1,
	C_Coords = 2,
	C_Indices = 4,
};

class DShape2D : public DObject
{

	DECLARE_CLASS(DShape2D,DObject)
public:
	TArray<int> mIndices;
	TArray<DVector2> mVertices;
	TArray<DVector2> mCoords;
};

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
        DTF_Burn = 4,
	};


	// This vertex type is hardware independent and needs conversion when put into a buffer.
	struct TwoDVertex
	{
		float x, y, z;
		float u, v;
		PalEntry color0;

		void Set(float xx, float yy, float zz)
		{
			x = xx;
			z = zz;
			y = yy;
			u = 0;
			v = 0;
			color0 = 0;
		}

		void Set(double xx, double yy, double zz, double uu, double vv, PalEntry col)
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
		PalEntry mSpecialColormap[2];
		int mScissor[4];
		int mDesaturate;
		FRenderStyle mRenderStyle;
		PalEntry mColor1;	// Overlay color
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
				mSpecialColormap == other.mSpecialColormap &&
				!memcmp(mScissor, other.mScissor, sizeof(mScissor)) &&
				mDesaturate == other.mDesaturate &&
				mRenderStyle == other.mRenderStyle &&
				mDrawMode == other.mDrawMode &&
				mFlags == other.mFlags &&
				mColor1 == other.mColor1;

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
	void AddShape(FTexture *img, DShape2D *shape, DrawParms &parms);
	void AddPoly(FTexture *texture, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel);
	void AddFlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin);

	void AddColorOnlyQuad(int left, int top, int width, int height, PalEntry color, FRenderStyle *style);

	void AddDim(PalEntry color, float damount, int x1, int y1, int w, int h);
	void AddClear(int left, int top, int right, int bottom, int palcolor, uint32_t color);
	
		
	void AddLine(int x1, int y1, int x2, int y2, int palcolor, uint32_t color);
	void AddThickLine(int x1, int y1, int x2, int y2, double thickness, uint32_t color);
	void AddPixel(int x1, int y1, int palcolor, uint32_t color);

	void Clear();
};


#endif
