#ifndef __2DDRAWER_H
#define __2DDRAWER_H

#include "tarray.h"
#include "gl/data/gl_vertexbuffer.h"

class F2DDrawer : public FSimpleVertexBuffer
{
	enum EDrawType
	{
		DrawTypeTexture,
		DrawTypeDim,
		DrawTypeFlatFill,
		DrawTypePoly,
		DrawTypeLine,
		DrawTypePixel
	};
	
	struct DataGeneric
	{
		EDrawType mType;
		uint32_t mLen;
		int mVertIndex;
		int mVertCount;
	};
	
	struct DataTexture : public DataGeneric
	{
		FMaterial *mTexture;
		int mScissor[4];
		uint32_t mColorOverlay;
		int mTranslation;
		FRenderStyle mRenderStyle;
		bool mMasked;
		bool mAlphaTexture;
	};
	
	struct DataFlatFill : public DataGeneric
	{
		FMaterial *mTexture;
	};
	
	struct DataSimplePoly : public DataGeneric
	{
		FMaterial *mTexture;
		int mLightLevel;
		FDynamicColormap *mColormap;
	};

	TArray<FSimpleVertex> mVertices;
	TArray<uint8_t> mData;
	int mLastLineCmd = -1;	// consecutive lines can be batched into a single draw call so keep this info around.
	
	int AddData(const DataGeneric *data);
	
public:
	void AddTexture(FTexture *img, DrawParms &parms);
	void AddDim(PalEntry color, float damount, int x1, int y1, int w, int h);
	void AddClear(int left, int top, int right, int bottom, int palcolor, uint32 color);
	void AddFlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin);
	
	void AddPoly(FTexture *texture, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, FDynamicColormap *colormap, int lightlevel);
		
	void AddLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color);
	void AddPixel(int x1, int y1, int palcolor, uint32 color);
		
	void Flush();
};


#endif
