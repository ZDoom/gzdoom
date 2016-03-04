#ifndef __GL_WALL_H
#define __GL_WALL_H
//==========================================================================
//
// One wall segment in the draw list
//
//==========================================================================
#include "r_defs.h"
#include "textures/textures.h"
#include "gl/renderer/gl_colormap.h"

struct GLHorizonInfo;
struct F3DFloor;
struct model_t;
struct FSpriteModelFrame;
struct particle_t;
class ADynamicLight;
class FMaterial;
struct GLDrawList;
struct GLSkyInfo;
struct FTexCoordInfo;
struct FPortal;
struct FFlatVertex;
struct FGLLinePortal;


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

struct GLSeg
{
	float x1,x2;
	float y1,y2;
	float fracleft, fracright;	// fractional offset of the 2 vertices on the linedef
};

struct texcoord
{
	float u,v;
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
	fixed_t texheight;
	fixed_t xoffs,  yoffs;
	fixed_t	xscale, yscale;
	angle_t	angle;

	void GetFromSector(sector_t * sec, int ceiling)
	{
		xoffs = sec->GetXOffset(ceiling);
		yoffs = sec->GetYOffset(ceiling);
		xscale = sec->GetXScale(ceiling);
		yscale = sec->GetYScale(ceiling);
		angle = sec->GetAngle(ceiling);
		texture = sec->GetTexture(ceiling);
		plane = sec->GetSecPlane(ceiling);
		texheight = (ceiling == sector_t::ceiling)? plane.d : -plane.d;
	}
};


class GLWall
{
public:

	enum
	{
		//GLWF_CLAMPX=1, use GLT_* for these!
		//GLWF_CLAMPY=2,
		GLWF_SKYHACK=4,
		GLWF_GLOW=8,		// illuminated by glowing flats
		GLWF_NOSPLITUPPER=16,
		GLWF_NOSPLITLOWER=32,
		GLWF_NOSPLIT=64,
	};

	enum
	{
		RWF_BLANK = 0,
		RWF_TEXTURED = 1,	// actually not being used anymore because with buffers it's even less efficient not writing the texture coordinates - but leave it here
		RWF_NOSPLIT = 4,
		RWF_NORENDER = 8,
	};


	friend struct GLDrawList;
	friend class GLPortal;

	GLSeg glseg;
	vertex_t * vertexes[2];				// required for polygon splitting
	float ztop[2],zbottom[2];
	texcoord uplft, uprgt, lolft, lorgt;
	float alpha;
	FMaterial *gltexture;

	FColormap Colormap;
	ERenderStyle RenderStyle;
	
	fixed_t viewdistance;

	TArray<lightlist_t> *lightlist;
	int lightlevel;
	BYTE type;
	BYTE flags;
	short rellight;

	float topglowcolor[4];
	float bottomglowcolor[4];

	int dynlightindex;

	union
	{
		// it's either one of them but never more!
		AActor * skybox;			// for skyboxes
		GLSkyInfo * sky;			// for normal sky
		GLHorizonInfo * horizon;	// for horizon information
		FPortal * portal;			// stacked sector portals
		secplane_t * planemirror;	// for plane mirrors
		FGLLinePortal *lineportal;	// line-to-line portals
	};


	FTextureID topflat,bottomflat;
	secplane_t topplane, bottomplane;	// we need to save these to pass them to the shader for calculating glows.

	// these are not the same as ytop and ybottom!!!
	float zceil[2];
	float zfloor[2];

public:
	seg_t * seg;			// this gives the easiest access to all other structs involved
	subsector_t * sub;		// For polyobjects
private:

	void CheckGlowing();
	void PutWall(bool translucent);
	void PutPortal(int ptype);
	void CheckTexturePosition();

	void Put3DWall(lightlist_t * lightlist, bool translucent);
	void SplitWall(sector_t * frontsector, bool translucent);

	void SetupLights();
	bool PrepareLight(texcoord * tcs, ADynamicLight * light);
	void RenderWall(int textured, unsigned int *store = NULL);
	void RenderTextured(int rflags);

	void FloodPlane(int pass);

	void SkyPlane(sector_t *sector, int plane, bool allowmirror);
	void SkyLine(sector_t *sec, line_t *line);
	void SkyNormal(sector_t * fs,vertex_t * v1,vertex_t * v2);
	void SkyTop(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);
	void SkyBottom(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);

	void LightPass();
	void SetHorizon(vertex_t * ul, vertex_t * ur, vertex_t * ll, vertex_t * lr);
	bool DoHorizon(seg_t * seg,sector_t * fs, vertex_t * v1,vertex_t * v2);

	bool SetWallCoordinates(seg_t * seg, FTexCoordInfo *tci, float ceilingrefheight,
							int topleft,int topright, int bottomleft,int bottomright, int texoffset);

	void DoTexture(int type,seg_t * seg,int peg,
						   int ceilingrefheight,int floorrefheight,
						   int CeilingHeightstart,int CeilingHeightend,
						   int FloorHeightstart,int FloorHeightend,
						   int v_offset);

	void DoMidTexture(seg_t * seg, bool drawfogboundary,
					  sector_t * front, sector_t * back,
					  sector_t * realfront, sector_t * realback,
					  fixed_t fch1, fixed_t fch2, fixed_t ffh1, fixed_t ffh2,
					  fixed_t bch1, fixed_t bch2, fixed_t bfh1, fixed_t bfh2);

	void GetPlanePos(F3DFloor::planeref *planeref, int &left, int &right);

	void BuildFFBlock(seg_t * seg, F3DFloor * rover,
					  fixed_t ff_topleft, fixed_t ff_topright, 
					  fixed_t ff_bottomleft, fixed_t ff_bottomright);
	void InverseFloors(seg_t * seg, sector_t * frontsector,
					   fixed_t topleft, fixed_t topright, 
					   fixed_t bottomleft, fixed_t bottomright);
	void ClipFFloors(seg_t * seg, F3DFloor * ffloor, sector_t * frontsector,
					fixed_t topleft, fixed_t topright, 
					fixed_t bottomleft, fixed_t bottomright);
	void DoFFloorBlocks(seg_t * seg, sector_t * frontsector, sector_t * backsector,
					  fixed_t fch1, fixed_t fch2, fixed_t ffh1, fixed_t ffh2,
					  fixed_t bch1, fixed_t bch2, fixed_t bfh1, fixed_t bfh2);

	void DrawDecal(DBaseDecal *actor);
	void DoDrawDecals();

	void RenderFogBoundary();
	void RenderMirrorSurface();
	void RenderTranslucentWall();

	void SplitLeftEdge(texcoord * tcs, FFlatVertex *&ptr);
	void SplitRightEdge(texcoord * tcs, FFlatVertex *&ptr);
	void SplitUpperEdge(texcoord * tcs, FFlatVertex *&ptr);
	void SplitLowerEdge(texcoord * tcs, FFlatVertex *&ptr);

public:

	void Process(seg_t *seg, sector_t *frontsector, sector_t *backsector);
	void ProcessLowerMiniseg(seg_t *seg, sector_t *frontsector, sector_t *backsector);
	void Draw(int pass);

	float PointOnSide(float x,float y)
	{
		return -((y-glseg.y1)*(glseg.x2-glseg.x1)-(x-glseg.x1)*(glseg.y2-glseg.y1));
	}

	// Lines start-end and fdiv must intersect.
	double CalcIntersectionVertex(GLWall * w2)
	{
		float ax = glseg.x1, ay=glseg.y1;
		float bx = glseg.x2, by=glseg.y2;
		float cx = w2->glseg.x1, cy=w2->glseg.y1;
		float dx = w2->glseg.x2, dy=w2->glseg.y2;
		return ((ay-cy)*(dx-cx)-(ax-cx)*(dy-cy)) / ((bx-ax)*(dy-cy)-(by-ay)*(dx-cx));
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
	friend struct GLDrawList;

	sector_t * sector;
	subsector_t * sub;	// only used for translucent planes
	float dz; // z offset for rendering hacks
	float z; // the z position of the flat (only valid for non-sloped planes)
	FMaterial *gltexture;

	FColormap Colormap;	// light and fog
	ERenderStyle renderstyle;

	float alpha;
	GLSectorPlane plane;
	int lightlevel;
	bool stack;
	bool foggy;
	bool ceiling;
	BYTE renderflags;
	int vboindex;
	int vboheight;

	int dynlightindex;

	void SetupSubsectorLights(int pass, subsector_t * sub, int *dli = NULL);
	void DrawSubsector(subsector_t * sub);
	void DrawSubsectorLights(subsector_t * sub, int pass);
	void DrawSubsectors(int pass, bool processlights, bool istrans);
	void ProcessLights(bool istrans);

	void PutFlat(bool fog = false);
	void Process(sector_t * model, int whichplane, bool notexture);
	void SetFrom3DFloor(F3DFloor *rover, bool top, bool underside);
	void ProcessSector(sector_t * frontsector);
	void Draw(int pass, bool trans);
};


//==========================================================================
//
// One sprite in the draw list
//
//==========================================================================


class GLSprite
{
public:
	friend struct GLDrawList;
	friend void Mod_RenderModel(GLSprite * spr, model_t * mdl, int framenumber);

	BYTE lightlevel;
	BYTE foglevel;
	BYTE hw_styleflags;
	bool fullbright;
	PalEntry ThingColor;	// thing's own color
	FColormap Colormap;
	FSpriteModelFrame * modelframe;
	FRenderStyle RenderStyle;
	int OverrideShader;

	int translation;
	int index;
	int depth;

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

	void SetLowerParam();
	void PerformSpriteClipAdjustment(AActor *thing, fixed_t thingx, fixed_t thingy, float spriteheight);

public:

	void Draw(int pass);
	void PutSprite(bool translucent);
	void Process(AActor* thing,sector_t * sector);
	void ProcessParticle (particle_t *particle, sector_t *sector);//, int shade, int fakeside)
	void SetThingColor(PalEntry);

	// Lines start-end and fdiv must intersect.
	double CalcIntersectionVertex(GLWall * w2);
};

inline float Dist2(float x1,float y1,float x2,float y2)
{
	return sqrtf((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
}

// Light + color

void gl_SetDynSpriteLight(AActor *self, fixed_t x, fixed_t y, fixed_t z, subsector_t *subsec);
void gl_SetDynSpriteLight(AActor *actor, particle_t *particle);

#endif
