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
struct FGLLinePortal;

enum
{
	GLSector_NoSkyDraw = 89,
	GLSector_Skybox = 90,
};

enum WallTypes
{
	RENDERWALL_NONE,
	RENDERWALL_TOP,
	RENDERWALL_M1S,
	RENDERWALL_M2S,
	RENDERWALL_BOTTOM,
	RENDERWALL_SKY,
	RENDERWALL_FOGBOUNDARY,
	RENDERWALL_HORIZON,
	RENDERWALL_SKYBOX,
	RENDERWALL_SECTORSTACK,
	RENDERWALL_PLANEMIRROR,
	RENDERWALL_MIRROR,
	RENDERWALL_MIRRORSURFACE,
	RENDERWALL_M2SNF,
	RENDERWALL_M2SFOG,
	RENDERWALL_COLOR,
	RENDERWALL_FFBLOCK,
	RENDERWALL_COLORLAYER,
	RENDERWALL_LINETOLINE,
	// Insert new types at the end!
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
	float Texheight;
	float	Angle;
	FVector2 Offs;
	FVector2 Scale;

	void GetFromSector(sector_t * sec, int ceiling)
	{
		Offs.X = (float)sec->GetXOffsetF(ceiling);
		Offs.Y = (float)sec->GetYOffsetF(ceiling);
		Scale.X = (float)sec->GetXScaleF(ceiling);
		Scale.Y = (float)sec->GetYScaleF(ceiling);
		Angle = (float)sec->GetAngleF(ceiling).Degrees;
		texture = sec->GetTexture(ceiling);
		plane = sec->GetSecPlane(ceiling);
		Texheight = (float)((ceiling == sector_t::ceiling)? plane.fD() : -plane.fD());
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
		GLWF_FOGGY=8,
		GLWF_GLOW=16,		// illuminated by glowing flats
		GLWF_NOSHADER=32,	// cannot be drawn with shaders.
		GLWF_NOSPLITUPPER=64,
		GLWF_NOSPLITLOWER=128,
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
	
	float ViewDistance;

	int lightlevel;
	BYTE type;
	BYTE flags;
	short rellight;

	float topglowcolor[4];
	float bottomglowcolor[4];

	int firstdynlight, lastdynlight;

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
	void CheckTexturePosition();

	void SetupLights();
	bool PrepareLight(texcoord * tcs, ADynamicLight * light);
	void RenderWall(int textured, float * color2, ADynamicLight * light=NULL);

	void FloodPlane(int pass);

	void SkyPlane(sector_t *sector, int plane, bool allowmirror);
	void SkyLine(sector_t *sec, line_t *line);
	void SkyNormal(sector_t * fs,vertex_t * v1,vertex_t * v2);
	void SkyTop(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);
	void SkyBottom(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);

	void Put3DWall(lightlist_t * lightlist, bool translucent);
	void SplitWall(sector_t * frontsector, bool translucent);
	void LightPass();
	void SetHorizon(vertex_t * ul, vertex_t * ur, vertex_t * ll, vertex_t * lr);
	bool DoHorizon(seg_t * seg,sector_t * fs, vertex_t * v1,vertex_t * v2);

	bool SetWallCoordinates(seg_t * seg, FTexCoordInfo *tci, float ceilingrefheight,
		float topleft, float topright, float bottomleft, float bottomright, float t_ofs);

	void DoTexture(int type,seg_t * seg,int peg,
						   float ceilingrefheight, float floorrefheight,
						   float CeilingHeightstart,float CeilingHeightend,
						   float FloorHeightstart,float FloorHeightend,
						   float v_offset);

	void DoMidTexture(seg_t * seg, bool drawfogboundary,
					  sector_t * front, sector_t * back,
					  sector_t * realfront, sector_t * realback,
					  float fch1, float fch2, float ffh1, float ffh2,
					  float bch1, float bch2, float bfh1, float bfh2);

	void GetPlanePos(F3DFloor::planeref * planeref, float & left, float & right);

	void BuildFFBlock(seg_t * seg, F3DFloor * rover,
					  float ff_topleft, float ff_topright, 
					  float ff_bottomleft, float ff_bottomright);
	void InverseFloors(seg_t * seg, sector_t * frontsector,
					   float topleft, float topright, 
					   float bottomleft, float bottomright);
	void ClipFFloors(seg_t * seg, F3DFloor * ffloor, sector_t * frontsector,
					float topleft, float topright, 
					float bottomleft, float bottomright);
	void DoFFloorBlocks(seg_t * seg, sector_t * frontsector, sector_t * backsector,
					  float fch1, float fch2, float ffh1, float ffh2,
					  float bch1, float bch2, float bfh1, float bfh2);

	void DrawDecal(DBaseDecal *actor);
	void DoDrawDecals();

	void RenderFogBoundary();
	void RenderMirrorSurface();
	void RenderTranslucentWall();

	void SplitLeftEdge(texcoord * tcs);
	void SplitRightEdge(texcoord * tcs);
	void SplitUpperEdge(texcoord * tcs);
	void SplitLowerEdge(texcoord * tcs);

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

	bool SetupSubsectorLights(bool lightsapplied, subsector_t * sub);
	void DrawSubsector(subsector_t * sub);
	void DrawSubsectorLights(subsector_t * sub, int pass);
	void DrawSkyboxSector(int pass);
	void DrawSubsectors(int pass, bool istrans);

	void PutFlat(bool fog = false);
	void Process(sector_t * model, int whichplane, bool notexture);
	void SetFrom3DFloor(F3DFloor *rover, bool top, bool underside);
	void ProcessSector(sector_t * frontsector);
	void Draw(int pass);
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

	int lightlevel;
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

	void SplitSprite(sector_t * frontsector, bool translucent);
	void SetLowerParam();
	void PerformSpriteClipAdjustment(AActor *thing, const DVector2 &thingpos, float spriteheight);

public:

	void Draw(int pass);
	void PutSprite(bool translucent);
	void Process(AActor* thing,sector_t * sector);
	void ProcessParticle (particle_t *particle, sector_t *sector);//, int shade, int fakeside)
	void SetThingColor(PalEntry);
	void SetSpriteColor(sector_t *sector, double y);

	// Lines start-end and fdiv must intersect.
	double CalcIntersectionVertex(GLWall * w2);
};

inline float Dist2(float x1,float y1,float x2,float y2)
{
	return sqrtf((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
}

// Light + color

bool gl_GetSpriteLight(AActor *Self, float x, float y, float z, subsector_t * subsec, int desaturation, float * out, line_t *line = NULL, int side = 0);
int gl_SetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm, float alpha, PalEntry ThingColor = 0xffffff, bool weapon=false);

void gl_GetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm,
					   float *red, float *green, float *blue,
					   PalEntry ThingColor, bool weapon);

int gl_SetSpriteLighting(FRenderStyle style, AActor *thing, int lightlevel, int rellight, FColormap *cm, 
						  PalEntry ThingColor, float alpha, bool fullbright, bool weapon);

int gl_SetSpriteLight(particle_t * thing, int lightlevel, int rellight, FColormap *cm, float alpha, PalEntry ThingColor = 0xffffff);
void gl_GetLightForThing(AActor * thing, float upper, float lower, float & r, float & g, float & b);




#endif
