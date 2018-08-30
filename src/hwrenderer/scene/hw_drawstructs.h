#pragma once
//==========================================================================
//
// One wall segment in the draw list
//
//==========================================================================
#include <functional>
#include "r_defs.h"
#include "r_data/renderstyle.h"
#include "textures/textures.h"
#include "r_data/colormaps.h"
#include "g_levellocals.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

struct GLHorizonInfo;
struct GLSkyInfo;
struct F3DFloor;
class FMaterial;
struct FTexCoordInfo;
struct FSectorPortalGroup;
struct FFlatVertex;
struct FLinePortalSpan;
struct FDynLightData;
class VSMatrix;
struct FSpriteModelFrame;
struct particle_t;
struct DecalVertex;
struct GLDecal;
enum area_t : int;

enum
{
	ALPHA_GREATER = 1,
	ALPHA_GEQUAL = 2
};

enum TexMode
{
    TM_UNDEFINED = -1,
	TM_MODULATE = 0,	// (r, g, b, a)
	TM_MASK,			// (1, 1, 1, a)
	TM_OPAQUE,			// (r, g, b, 1)
	TM_INVERSE,			// (1-r, 1-g, 1-b, a)
	TM_REDTOALPHA,		// (1, 1, 1, r)
	TM_CLAMPY,			// (r, g, b, (t >= 0.0 && t <= 1.0)? a:0)
	TM_INVERTOPAQUE,	// (1-r, 1-g, 1-b, 1)
	TM_FOGLAYER,		// (renders a fog layer in the shape of the active texture)
    TM_COUNT,           // Terminator. Not a valid index.
	TM_FIXEDCOLORMAP = TM_FOGLAYER,	// repurposes the objectcolor uniforms to render a fixed colormap range. (Same constant because they cannot be used in the same context.
};

enum HWRenderStyle
{
	STYLEHW_Normal,			// default
	STYLEHW_Solid,			// drawn solid (needs special treatment for sprites)
	STYLEHW_NoAlphaTest,	// disable alpha test
};

enum WallTypes
{
	RENDERWALL_NONE,
	RENDERWALL_TOP,
	RENDERWALL_M1S,
	RENDERWALL_M2S,
	RENDERWALL_BOTTOM,
	RENDERWALL_FOGBOUNDARY,
	RENDERWALL_MIRRORSURFACE,
	RENDERWALL_M2SNF,
	RENDERWALL_COLOR,
	RENDERWALL_FFBLOCK,
	// Insert new types at the end!
};

enum PortalTypes
{
	PORTALTYPE_SKY,
	PORTALTYPE_HORIZON,
	PORTALTYPE_SKYBOX,
	PORTALTYPE_SECTORSTACK,
	PORTALTYPE_PLANEMIRROR,
	PORTALTYPE_MIRROR,
	PORTALTYPE_LINETOLINE,
};

struct AttributeBufferData
{
    enum EDefaultAttribute
    {
        StencilIndex,
        TextureModeFor2DIndex,
        MaxDefaultIndex = TextureModeFor2DIndex + TM_COUNT
    };
    
	FVector4 uLightColor;
	FVector4 uObjectColor;
	FVector4 uObjectColor2;
	FVector4 uGlowTopPlane;
	FVector4 uGlowTopColor;
	FVector4 uGlowBottomPlane;
	FVector4 uGlowBottomColor;
	FVector4 uSplitTopPlane;
	FVector4 uSplitBottomPlane;
	FVector4 uFogColor;
	FVector4 uDynLightColor;
	float uLightLevel;
	float uFogDensity;
	float uLightFactor;
	float uLightDist;
	float uDesaturationFactor;
	float uAlphaThreshold;
	int uTextureMode;
	int uFogEnabled;
	int uLightIndex;
	int uTexMatrixIndex;
	int uLightIsAttr;

	void SetDefaults()
	{
		uObjectColor =
			uLightColor = { 1.f, 1.f,1.f,1.f };
		uObjectColor2.Zero();
		uFogColor.Zero();
		uDynLightColor.Zero();
		uGlowTopPlane.Zero();
		uGlowBottomPlane.Zero();
		uGlowTopColor.Zero();
		uGlowBottomColor.Zero();
		uSplitTopPlane.Zero();
		uSplitBottomPlane.Zero();
		uTextureMode = TM_UNDEFINED;
		uDesaturationFactor = 0;
		uAlphaThreshold = 0.5f;
		uLightLevel = -1;
		uFogDensity = uLightFactor = uLightDist = 0;
		uLightIndex = -1;
		uTexMatrixIndex = 0;
		uLightIsAttr = 0;
		uFogEnabled = 0;
	}

	//==========================================================================
	//
	// Sets texture mode from render style
	//
	//==========================================================================

	void SetTextureMode(FRenderStyle style, bool drawopaque)
	{
		uTextureMode = drawopaque ? TM_OPAQUE : TM_MODULATE;
		if (style.Flags & STYLEF_RedIsAlpha)
		{
			uTextureMode = TM_REDTOALPHA;
		}
		else if (style.Flags & STYLEF_ColorIsFixed)
		{
			uTextureMode = TM_MASK;
		}
		else if (style.Flags & STYLEF_InvertSource)
		{
			uTextureMode = TM_INVERSE;
		}
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void SetGlowPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		DVector3 tn = top.Normal();
		DVector3 bn = bottom.Normal();
		uGlowTopPlane = { (float)tn.X, (float)tn.Y, 1.f / (float)tn.Z, (float)top.fD() };
		uGlowBottomPlane = { (float)bn.X, (float)bn.Y, 1.f / (float)bn.Z, (float)bottom.fD() };
	}

	void SetGlowParams(float *t, float *b)
	{
		uGlowTopColor = { t[0], t[1], t[2], t[3] };
		uGlowBottomColor = { b[0], b[1], b[2], b[3] };
	}

	void ClearGlow()
	{
		uGlowTopColor.W = 0;
		uGlowBottomColor.W = 0;
	}

	//==========================================================================
	//
	// set current light color
	//
	//==========================================================================

	void SetColorAlpha(PalEntry pe, float alpha = 1.f, int desat = 0)
	{
		uLightColor = { pe.r / 255.f, pe.g / 255.f, pe.b / 255.f, alpha };
		uDesaturationFactor = desat / 255.f;
	}

	void SetColorAlpha(float r, float g, float b, float alpha, int desat = 0)
	{
		uLightColor = { r, g, b, alpha };
		uDesaturationFactor = desat / 255.f;
	}

	void SetSoftLightLevel(int llevel)
	{
		if (level.lightmode == 8) uLightLevel = llevel / 255.f;
		else uLightLevel = -1.f;
	}

	void SetFog(PalEntry pe, float d)
	{
		const float LOG2E = 1.442692f;	// = 1/log(2)
		uFogColor = { pe.r / 255.f, pe.g / 255.f, pe.b / 255.f, 1.f };
		if (d >= 0.0f) uFogDensity = d * (-LOG2E / 64000.f);
	}

	void SetLightParms(float f, float d)
	{
		uLightFactor = f;
		uLightDist = d;
	}

	void SetDynLightColor(float r, float g, float b)
	{
		uDynLightColor = { r, g, b, 1 };
	}

	void SetObjectColor(float r, float g, float b)
	{
		uObjectColor = { r, g, b, 1 };
	}

	void SetObjectColor(PalEntry pe)
	{
		uObjectColor = { pe.r / 255.f, pe.g / 255.f, pe.b / 255.f, 1.f };
	}

	void SetObjectColor2(PalEntry pe)
	{
		uObjectColor2 = { pe.r / 255.f, pe.g / 255.f, pe.b / 255.f, 1.f };
	}

	void ClearObjectColor2()
	{
		uObjectColor2.W = 0;
	}

	void SetSplitPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		DVector3 tn = top.Normal();
		DVector3 bn = bottom.Normal();
		uSplitTopPlane = { (float)tn.X, (float)tn.Y, 1.f / (float)tn.Z, (float)top.fD() };
		uSplitBottomPlane = { (float)bn.X, (float)bn.Y, 1.f / (float)bn.Z, (float)bottom.fD() };
	}

	void DisableSplitPlanes()
	{
		uSplitTopPlane.Zero();
		uSplitBottomPlane.Zero();
	}

	void AlphaFunc(int func, float thresh)
	{
		if (func == ALPHA_GREATER) uAlphaThreshold = thresh;
		else uAlphaThreshold = thresh - 0.001f;
	}

    void CreateDefaultEntries(std::function<void(AttributeBufferData&)> callback);
	void SetColor(int sectorlightlevel, int rellight, bool fullbright, const FColormap &cm, float alpha, bool weapon = false);
	void SetShaderLight(float level, float olight);
	void SetFog(int lightlevel, int rellight, bool fullbright, const FColormap *cmap, bool isadditive);

};

//==========================================================================
//
// One sector plane, still in fixed point
//
//==========================================================================

struct GLSectorPlane
{
	FTextureID texture;
	secplane_t plane;
	float Texheight;
	float	Angle;
	FVector2 Offs;
	FVector2 Scale;
	int *pUbIndexMatrix;

	void GetFromSector(sector_t * sec, int ceiling)
	{
		Offs.X = (float)sec->GetXOffset(ceiling);
		Offs.Y = (float)sec->GetYOffset(ceiling);
		Scale.X = (float)sec->GetXScale(ceiling);
		Scale.Y = (float)sec->GetYScale(ceiling);
		Angle = (float)sec->GetAngle(ceiling).Degrees;
		texture = sec->GetTexture(ceiling);
		plane = sec->GetSecPlane(ceiling);
		Texheight = (float)((ceiling == sector_t::ceiling)? plane.fD() : -plane.fD());
		pUbIndexMatrix = sec->planes[ceiling].pUbIndexMatrix;
	}
};

struct GLSeg
{
	float x1,x2;
	float y1,y2;
	float fracleft, fracright;	// fractional offset of the 2 vertices on the linedef

	FVector3 Normal() const 
	{
		// we do not use the vector math inlines here because they are not optimized for speed but accuracy in the playsim and this is called quite frequently.
		float x = y2 - y1;
		float y = x1 - x2;
		float ilength = 1.f / sqrtf(x*x + y*y);
		return FVector3(x * ilength, 0, y * ilength);
	}
};

struct texcoord
{
	float u,v;
};

struct HWDrawInfo;

struct WallSortInfo
{
	uint8_t type;
	uint16_t flags;
	vertex_t * vertexes[2];				// required for polygon splitting
	GLSeg glseg;
	texcoord tcs[4];
	float ViewDistance;

	float PointOnSide(float x, float y)
	{
		return -((y - glseg.y1)*(glseg.x2 - glseg.x1) - (x - glseg.x1)*(glseg.y2 - glseg.y1));
	}
};

struct WallAttributeInfo
{
	FColormap Colormap;
	int lightlevel;
	float alpha;
	int rellight;
	float topglowcolor[4], bottomglowcolor[4];
	secplane_t *topplane, *bottomplane;
	int dynlightindex;

	TArray<lightlist_t> *lightlist;

	AttributeBufferData attrBuffer;
};

struct WallRenderInfo
{
	enum
	{
		PT_TRIANGLES,
		PT_STRIP,
		PT_FAN
	};
	FMaterial *gltexture;
	ERenderStyle RenderStyle;
	unsigned int attrindex;
	unsigned int vertindex;
	unsigned int vertcount;
	uint8_t primtype;
	bool indexed;
	bool alphateston;

	union
	{
		// it's either one of them but never more!
		FSectorPortal *secportal;	// sector portal (formerly skybox)
		GLSkyInfo * sky;			// for normal sky
		GLHorizonInfo * horizon;	// for horizon information
		FSectorPortalGroup * portal;			// stacked sector portals
		secplane_t * planemirror;	// for plane mirrors
		FLinePortalSpan *lineportal;	// line-to-line portals
	};

};


class GLWall : public WallSortInfo, public WallRenderInfo
{
public:
	static const char passflag[];

	enum
	{
		GLWF_CLAMPX=1,
		GLWF_CLAMPY=2,
		GLWF_SKYHACK=4,
		GLWF_GLOW=8,		// illuminated by glowing flats
		GLWF_NOSPLITUPPER=16,
		GLWF_NOSPLITLOWER=32,
		GLWF_NOSPLIT=64,
		GLWF_TRANSLUCENT = 128,
		GLWF_SPLITENABLED = 256,
	};

	enum
	{
		RWF_BLANK = 0,
		RWF_TEXTURED = 1,	// actually not being used anymore because with buffers it's even less efficient not writing the texture coordinates - but leave it here
		RWF_NOSPLIT = 4,
		RWF_NORENDER = 8,
	};

	enum
	{
		LOLFT,
		UPLFT,
		UPRGT,
		LORGT,
	};


	friend struct HWDrawList;
	friend class GLPortal;


	float ztop[2],zbottom[2];
	


	// these are not the same as ytop and ybottom!!!
	float zceil[2];
	float zfloor[2];


public:
	seg_t * seg;			// this gives the easiest access to all other structs involved
	subsector_t * sub;		// For polyobjects
//private:

	void PostWall(WallAttributeInfo &wri, HWDrawInfo *di);
	void ProcessFogBoundary(WallAttributeInfo &wri, HWDrawInfo *di);
	void ProcessMirrorSurface(WallAttributeInfo &wri, HWDrawInfo *di);
	void ProcessTexturedWall(WallAttributeInfo &wri, HWDrawInfo *di);
	void ProcessColorPlane(WallAttributeInfo &wri, HWDrawInfo *di);
	void BuildAttributes(WallAttributeInfo &wri, HWDrawInfo *di);

	void AddMirrorSurface(WallAttributeInfo &wri, HWDrawInfo *di);

	void PutWall(WallAttributeInfo &wri, HWDrawInfo *di, bool translucent);
	void PutPortal(HWDrawInfo *di, int ptype);
	void CheckTexturePosition(FTexCoordInfo *tci);

	void Put3DWall(WallAttributeInfo &wri, HWDrawInfo *di, lightlist_t * lightlist, bool translucent);
	void SplitWall(WallAttributeInfo &wri, HWDrawInfo *di, sector_t * frontsector, bool translucent);

	void SetupLights(WallAttributeInfo &wri, HWDrawInfo *di, FDynLightData &lightdata);

	void MakeVertices(HWDrawInfo *di, bool nosplit);

	void SkyPlane(HWDrawInfo *di, sector_t *sector, int plane, bool allowmirror, PalEntry fadecolor);
	void SkyLine(HWDrawInfo *di, sector_t *sec, line_t *line, PalEntry fadecolor);
	void SkyNormal(HWDrawInfo *di, sector_t * fs,vertex_t * v1,vertex_t * v2, PalEntry fadecolor);
	void SkyTop(HWDrawInfo *di, seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2, PalEntry fadecolor);
	void SkyBottom(HWDrawInfo *di, seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2, PalEntry fadecolor);

	bool DoHorizon(WallAttributeInfo &wri, HWDrawInfo *di, seg_t * seg,sector_t * fs, vertex_t * v1,vertex_t * v2);

	bool SetWallCoordinates(seg_t * seg, FTexCoordInfo *tci, float ceilingrefheight,
		float topleft, float topright, float bottomleft, float bottomright, float t_ofs);

	void DoTexture(WallAttributeInfo &wri, HWDrawInfo *di, int type,seg_t * seg,int peg,
						   float ceilingrefheight, float floorrefheight,
						   float CeilingHeightstart,float CeilingHeightend,
						   float FloorHeightstart,float FloorHeightend,
						   float v_offset);

	void DoMidTexture(WallAttributeInfo &wri, HWDrawInfo *di, seg_t * seg, bool drawfogboundary,
					  sector_t * front, sector_t * back,
					  sector_t * realfront, sector_t * realback,
					  float fch1, float fch2, float ffh1, float ffh2,
					  float bch1, float bch2, float bfh1, float bfh2);

	void GetPlanePos(F3DFloor::planeref * planeref, float & left, float & right);

	void BuildFFBlock(WallAttributeInfo &wri, HWDrawInfo *di, seg_t * seg, F3DFloor * rover,
					  float ff_topleft, float ff_topright, 
					  float ff_bottomleft, float ff_bottomright);
	void InverseFloors(WallAttributeInfo &wri, HWDrawInfo *di, seg_t * seg, sector_t * frontsector,
					   float topleft, float topright, 
					   float bottomleft, float bottomright);
	void ClipFFloors(WallAttributeInfo &wri, HWDrawInfo *di, seg_t * seg, F3DFloor * ffloor, sector_t * frontsector,
					float topleft, float topright, 
					float bottomleft, float bottomright);
	void DoFFloorBlocks(WallAttributeInfo &wri, HWDrawInfo *di, seg_t * seg, sector_t * frontsector, sector_t * backsector,
					  float fch1, float fch2, float ffh1, float ffh2,
					  float bch1, float bch2, float bfh1, float bfh2);

	void SetDecalAttributes(WallAttributeInfo &wri, HWDrawInfo *di, GLDecal *decal, DecalVertex *dv);
	void ProcessDecal(WallAttributeInfo &wri, HWDrawInfo *di, DBaseDecal *decal, const FVector3 &normal);
    void ProcessDecals(WallAttributeInfo &wri, HWDrawInfo *di);

	int CreateVertices(FFlatVertex *&ptr, bool nosplit);
	void SplitLeftEdge (FFlatVertex *&ptr);
	void SplitRightEdge(FFlatVertex *&ptr);
	void SplitUpperEdge(FFlatVertex *&ptr);
	void SplitLowerEdge(FFlatVertex *&ptr);

	void CountLeftEdge (unsigned &ptr);
	void CountRightEdge(unsigned &ptr);

	int CountVertices();

public:
	GLWall() = default;

	GLWall(const GLWall &other)
	{
		memcpy(this, &other, sizeof(GLWall));
	}

	GLWall & operator=(const GLWall &other)
	{
		memcpy(this, &other, sizeof(GLWall));
		return *this;
	}

	void Process(HWDrawInfo *di, seg_t *seg, sector_t *frontsector, sector_t *backsector);
	void ProcessLowerMiniseg(HWDrawInfo *di, seg_t *seg, sector_t *frontsector, sector_t *backsector);
};

//==========================================================================
//
// One flat plane in the draw list
//
//==========================================================================

class GLFlat
{
public:
	sector_t * sector;
	float z; // the z position of the flat (only valid for non-sloped planes)
	FMaterial *gltexture;

	FColormap Colormap;	// light and fog
	PalEntry FlatColor;
	ERenderStyle renderstyle;

	float alpha;
	GLSectorPlane plane;
	int lightlevel;
	bool stack;
	bool ceiling;
	bool alphateston;
	uint8_t renderflags;
	int iboindex;
	int attrindex;

	int dynlightindex;

	void BuildAttributes(AttributeBufferData &attr, HWDrawInfo *di, bool trans);

	void CreateSkyboxVertices(FFlatVertex *buffer);
	void SetupLights(HWDrawInfo *di, FLightNode *head, FDynLightData &lightdata, int portalgroup);

	void PutFlat(HWDrawInfo *di, bool fog = false);
	void Process(HWDrawInfo *di, sector_t * model, int whichplane, bool notexture);
	void SetFrom3DFloor(F3DFloor *rover, bool top, bool underside);
	void ProcessSector(HWDrawInfo *di, sector_t * frontsector);
	
	GLFlat() {}

	GLFlat(const GLFlat &other)
	{
		memcpy(this, &other, sizeof(GLFlat));
	}

	GLFlat & operator=(const GLFlat &other)
	{
		memcpy(this, &other, sizeof(GLFlat));
		return *this;
	}

};

//==========================================================================
//
// One sprite in the draw list
//
//==========================================================================


class GLSprite
{
public:
	int lightlevel;
	uint8_t foglevel;
	uint8_t hw_styleflags;
	bool fullbright;
	bool polyoffset;
	PalEntry ThingColor;	// thing's own color
	FColormap Colormap;
	FSpriteModelFrame * modelframe;
	FRenderStyle RenderStyle;
	int OverrideShader;

	int translation;
	int index;
	int depth;

	float topclip;
	float bottomclip;

	float x,y,z;	// needed for sorting!

	float ul,ur;
	float vt,vb;
	float x1,y1,z1;
	float x2,y2,z2;

	FMaterial *gltexture;
	float trans;
	AActor * actor;
	particle_t * particle;
	TArray<lightlist_t> *lightlist;
	DRotator Angles;

	int dynlightindex;
	int vertexindex;
	int modelindex;
	int attrindex;
	bool modelmirrored;
	bool translucentpass;
	bool alphateston;
	bool foglayer;

	void SetAttributes(AttributeBufferData &attr, HWDrawInfo *di);
	void PerformSpriteClipAdjustment(AActor *thing, const DVector2 &thingpos, float spriteheight);
	bool CalculateVertices(HWDrawInfo *di, FVector3 *v, DVector3 *vp);

public:

	GLSprite() {}
	void PutSprite(HWDrawInfo *di, bool translucent);
	void Process(HWDrawInfo *di, AActor* thing,sector_t * sector, area_t in_area, int thruportal = false);
	void ProcessParticle (HWDrawInfo *di, particle_t *particle, sector_t *sector);//, int shade, int fakeside)

	GLSprite(const GLSprite &other)
	{
		memcpy(this, &other, sizeof(GLSprite));
	}

	GLSprite & operator=(const GLSprite &other)
	{
		memcpy(this, &other, sizeof(GLSprite));
		return *this;
	}

};




struct DecalVertex
{
	float x, y, z;
	float u, v;
};

struct GLDecal
{
	FMaterial *gltexture;
	DBaseDecal *decal;
	unsigned int vertindex;
	unsigned int attrindex;
	FVector3 Normal;

};


inline float Dist2(float x1,float y1,float x2,float y2)
{
	return sqrtf((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
}

bool hw_SetPlaneTextureRotation(const GLSectorPlane * secplane, FMaterial * gltexture, VSMatrix &mat);
void hw_GetDynModelLight(AActor *self, FDynLightData &modellightdata);

extern const float LARGE_VALUE;
