#ifndef __GL_WALL_H
#define __GL_WALL_H
//==========================================================================
//
// One wall segment in the draw list
//
//==========================================================================
#include "r_defs.h"
#include "r_data/renderstyle.h"
#include "textures/textures.h"
#include "r_data/colormaps.h"
#include "hwrenderer/scene/hw_drawstructs.h"

#pragma warning(disable:4244)

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
struct FSectorPortalGroup;
struct FFlatVertex;
struct FLinePortalSpan;
class GLSceneDrawer;
struct FDynLightData;

struct FDrawInfo;
struct HWDrawInfo;

//==========================================================================
//
// One flat plane in the draw list
//
//==========================================================================

class GLFlat
{
public:
	friend struct GLDrawList;

	GLSceneDrawer *mDrawer;
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
	int vboindex;
	//int vboheight;

	int dynlightindex;

	GLFlat(GLSceneDrawer *drawer)
	{
		mDrawer = drawer;
	}
	// compatibility fallback stuff.
	void DrawSubsectorLights(subsector_t * sub, int pass);
	void DrawLightsCompat(int pass);
	bool PutFlatCompat(bool fog);

	void SetupSubsectorLights(int pass, subsector_t * sub, int *dli = NULL);
	void DrawSubsector(subsector_t * sub);
	void DrawSkyboxSector(int pass, bool processlights);
	void DrawSubsectors(int pass, bool processlights, bool istrans);
	void ProcessLights(bool istrans);

	void PutFlat(bool fog = false);
	void Process(sector_t * model, int whichplane, bool notexture);
	void SetFrom3DFloor(F3DFloor *rover, bool top, bool underside);
	void ProcessSector(sector_t * frontsector);
	void Draw(int pass, bool trans);

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
	friend struct GLDrawList;
	friend void Mod_RenderModel(GLSprite * spr, model_t * mdl, int framenumber);

	GLSceneDrawer *mDrawer;
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

	void SplitSprite(sector_t * frontsector, bool translucent);
	void SetLowerParam();
	void PerformSpriteClipAdjustment(AActor *thing, const DVector2 &thingpos, float spriteheight);
	void CalculateVertices(FVector3 *v);

public:

	GLSprite(GLSceneDrawer *drawer)
	{
		mDrawer = drawer;
	}
	void Draw(int pass);
	void PutSprite(bool translucent);
	void Process(AActor* thing,sector_t * sector, int thruportal = false);
	void ProcessParticle (particle_t *particle, sector_t *sector);//, int shade, int fakeside)
	void SetThingColor(PalEntry);

	// Lines start-end and fdiv must intersect.
	double CalcIntersectionVertex(GLWall * w2);

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

// Light + color

void gl_SetDynSpriteLight(AActor *self, float x, float y, float z, subsector_t *subsec);
void gl_SetDynSpriteLight(AActor *actor, particle_t *particle);
int gl_SetDynModelLight(AActor *self, int dynlightindex);

#endif
