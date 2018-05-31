#pragma once
//==========================================================================
//
// One wall segment in the draw list
//
//==========================================================================
#include "r_defs.h"
#include "r_data/renderstyle.h"
#include "textures/textures.h"
#include "r_data/colormaps.h"

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
enum area_t : int;

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

class GLWall
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
		GLWF_TRANSLUCENT = 128
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

	vertex_t * vertexes[2];				// required for polygon splitting
	FMaterial *gltexture;
	TArray<lightlist_t> *lightlist;

	GLSeg glseg;
	float ztop[2],zbottom[2];
	texcoord tcs[4];
	float alpha;

	FColormap Colormap;
	ERenderStyle RenderStyle;
	
	float ViewDistance;

	int lightlevel;
	uint8_t type;
	uint8_t flags;
	short rellight;

	float topglowcolor[4];
	float bottomglowcolor[4];

	int dynlightindex;

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


	secplane_t topplane, bottomplane;	// we need to save these to pass them to the shader for calculating glows.

	// these are not the same as ytop and ybottom!!!
	float zceil[2];
	float zfloor[2];

	unsigned int vertindex;
	unsigned int vertcount;

public:
	seg_t * seg;			// this gives the easiest access to all other structs involved
	subsector_t * sub;		// For polyobjects
//private:

	void PutWall(HWDrawInfo *di, bool translucent);
	void PutPortal(HWDrawInfo *di, int ptype);
	void CheckTexturePosition(FTexCoordInfo *tci);

	void Put3DWall(HWDrawInfo *di, lightlist_t * lightlist, bool translucent);
	bool SplitWallComplex(HWDrawInfo *di, sector_t * frontsector, bool translucent, float& maplightbottomleft, float& maplightbottomright);
	void SplitWall(HWDrawInfo *di, sector_t * frontsector, bool translucent);

	void SetupLights(HWDrawInfo *di, FDynLightData &lightdata);

	void MakeVertices(HWDrawInfo *di, bool nosplit);

	void SkyPlane(HWDrawInfo *di, sector_t *sector, int plane, bool allowmirror);
	void SkyLine(HWDrawInfo *di, sector_t *sec, line_t *line);
	void SkyNormal(HWDrawInfo *di, sector_t * fs,vertex_t * v1,vertex_t * v2);
	void SkyTop(HWDrawInfo *di, seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);
	void SkyBottom(HWDrawInfo *di, seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);

	bool DoHorizon(HWDrawInfo *di, seg_t * seg,sector_t * fs, vertex_t * v1,vertex_t * v2);

	bool SetWallCoordinates(seg_t * seg, FTexCoordInfo *tci, float ceilingrefheight,
		float topleft, float topright, float bottomleft, float bottomright, float t_ofs);

	void DoTexture(HWDrawInfo *di, int type,seg_t * seg,int peg,
						   float ceilingrefheight, float floorrefheight,
						   float CeilingHeightstart,float CeilingHeightend,
						   float FloorHeightstart,float FloorHeightend,
						   float v_offset);

	void DoMidTexture(HWDrawInfo *di, seg_t * seg, bool drawfogboundary,
					  sector_t * front, sector_t * back,
					  sector_t * realfront, sector_t * realback,
					  float fch1, float fch2, float ffh1, float ffh2,
					  float bch1, float bch2, float bfh1, float bfh2);

	void GetPlanePos(F3DFloor::planeref * planeref, float & left, float & right);

	void BuildFFBlock(HWDrawInfo *di, seg_t * seg, F3DFloor * rover,
					  float ff_topleft, float ff_topright, 
					  float ff_bottomleft, float ff_bottomright);
	void InverseFloors(HWDrawInfo *di, seg_t * seg, sector_t * frontsector,
					   float topleft, float topright, 
					   float bottomleft, float bottomright);
	void ClipFFloors(HWDrawInfo *di, seg_t * seg, F3DFloor * ffloor, sector_t * frontsector,
					float topleft, float topright, 
					float bottomleft, float bottomright);
	void DoFFloorBlocks(HWDrawInfo *di, seg_t * seg, sector_t * frontsector, sector_t * backsector,
					  float fch1, float fch2, float ffh1, float ffh2,
					  float bch1, float bch2, float bfh1, float bfh2);

    void ProcessDecal(HWDrawInfo *di, DBaseDecal *decal, const FVector3 &normal);
    void ProcessDecals(HWDrawInfo *di);

	int CreateVertices(FFlatVertex *&ptr, bool nosplit);
	void SplitLeftEdge (FFlatVertex *&ptr);
	void SplitRightEdge(FFlatVertex *&ptr);
	void SplitUpperEdge(FFlatVertex *&ptr);
	void SplitLowerEdge(FFlatVertex *&ptr);

	void CountLeftEdge (unsigned &ptr);
	void CountRightEdge(unsigned &ptr);

	int CountVertices();

public:
	GLWall() {}

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

	float PointOnSide(float x,float y)
	{
		return -((y-glseg.y1)*(glseg.x2-glseg.x1)-(x-glseg.x1)*(glseg.y2-glseg.y1));
	}

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
	float dz; // z offset for rendering hacks
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
	uint8_t renderflags;
	int iboindex;
	//int vboheight;

	int dynlightindex;

	bool SetupLights(int pass, FLightNode *head, FDynLightData &lightdata, int portalgroup);
	bool SetupSubsectorLights(int pass, subsector_t * sub, FDynLightData &lightdata);
	bool SetupSectorLights(int pass, sector_t * sec, FDynLightData &lightdata);

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

	void SplitSprite(HWDrawInfo *di, sector_t * frontsector, bool translucent);
	void PerformSpriteClipAdjustment(AActor *thing, const DVector2 &thingpos, float spriteheight);
	bool CalculateVertices(HWDrawInfo *di, FVector3 *v);

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
	TArray<lightlist_t> *lightlist;
	DBaseDecal *decal;
	DecalVertex dv[4];
	float zcenter;
	unsigned int vertindex;

	FRenderStyle renderstyle;
	int lightlevel;
	int rellight;
	float alpha;
	FColormap Colormap;
	secplane_t bottomplane;
	FVector3 Normal;

};


inline float Dist2(float x1,float y1,float x2,float y2)
{
	return sqrtf((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
}

bool hw_SetPlaneTextureRotation(const GLSectorPlane * secplane, FMaterial * gltexture, VSMatrix &mat);
void hw_GetDynModelLight(AActor *self, FDynLightData &modellightdata);

extern const float LARGE_VALUE;
