#pragma once

#include "hw_drawinfo.h"
#include "hw_drawstructs.h"
#include "hwrenderer/textures/hw_material.h"

struct GLSkyInfo
{
	float x_offset[2];
	float y_offset;		// doubleskies don't have a y-offset
	FMaterial * texture[2];
	FTextureID skytexno1;
	bool mirrored;
	bool doublesky;
	bool sky2;
	PalEntry fadecolor;

	bool operator==(const GLSkyInfo & inf)
	{
		return !memcmp(this, &inf, sizeof(*this));
	}
	bool operator!=(const GLSkyInfo & inf)
	{
		return !!memcmp(this, &inf, sizeof(*this));
	}
	void init(int sky1, PalEntry fadecolor);
};

struct GLHorizonInfo
{
	GLSectorPlane plane;
	int lightlevel;
	FColormap colormap;
	PalEntry specialcolor;
};

struct FPortalSceneState;

class IPortal
{
	friend struct FPortalSceneState;
protected:
	FPortalSceneState * mState;
	TArray<GLWall> lines;
public:
	IPortal(FPortalSceneState *s, bool local);
	virtual ~IPortal() {}
	virtual int ClipSeg(seg_t *seg, const DVector3 &viewpos) { return PClip_Inside; }
	virtual int ClipSubsector(subsector_t *sub) { return PClip_Inside; }
	virtual int ClipPoint(const DVector2 &pos) { return PClip_Inside; }
	virtual line_t *ClipLine() { return nullptr; }
	virtual void * GetSource() const = 0;	// GetSource MUST be implemented!
	virtual const char *GetName() = 0;
	virtual bool IsSky() { return false; }
	virtual bool NeedCap() { return true; }
	virtual bool NeedDepthBuffer() { return true; }
	virtual void DrawContents(HWDrawInfo *di) = 0;
	virtual void RenderAttached(HWDrawInfo *di) {}
	virtual bool Start(bool usestencil, bool doquery, HWDrawInfo *outer_di, HWDrawInfo **pDi) = 0;
	virtual void End(HWDrawInfo *di, bool usestencil) = 0;

	void AddLine(GLWall * l)
	{
		lines.Push(*l);
	}

	void RenderPortal(bool usestencil, bool doquery, HWDrawInfo *outer_di)
	{
		// Start may perform an occlusion query. If that returns 0 there
		// is no need to draw the stencil's contents and there's also no
		// need to restore the affected area becasue there is none!
		HWDrawInfo *di;
		if (Start(usestencil, doquery, outer_di, &di))
		{
			DrawContents(di);
			End(di, usestencil);
		}
	}


};

struct FPortalSceneState
{
	TArray<IPortal *> portals;
	int recursion = 0;

	int MirrorFlag = 0;
	int PlaneMirrorFlag = 0;
	int renderdepth = 0;

	int PlaneMirrorMode = 0;
	bool inskybox = 0;

	UniqueList<GLSkyInfo> UniqueSkies;
	UniqueList<GLHorizonInfo> UniqueHorizons;
	UniqueList<secplane_t> UniquePlaneMirrors;

	int skyboxrecursion = 0;

	void BeginScene()
	{
		UniqueSkies.Clear();
		UniqueHorizons.Clear();
		UniquePlaneMirrors.Clear();
	}

	int GetRecursion() const
	{
		return recursion;
	}

	bool isMirrored() const
	{
		return !!((MirrorFlag ^ PlaneMirrorFlag) & 1);
	}

	IPortal * FindPortal(const void * src);
	void StartFrame();
	bool RenderFirstSkyPortal(int recursion, HWDrawInfo *outer_di);
	void EndFrame(HWDrawInfo *outer_di);


};

inline IPortal::IPortal(FPortalSceneState *s, bool local) : mState(s)
{
	if (!local) s->portals.Push(this);
}
