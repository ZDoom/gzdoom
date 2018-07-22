#pragma once

#include "portal.h"
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
public:
	FPortalSceneState * mState;
	TArray<GLWall> lines;

	IPortal(FPortalSceneState *s, bool local);
	virtual ~IPortal() {}
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

	void StartFrame();
	bool RenderFirstSkyPortal(int recursion, HWDrawInfo *outer_di);
	void EndFrame(HWDrawInfo *outer_di);


};

inline IPortal::IPortal(FPortalSceneState *s, bool local) : mState(s)
{
	//if (!local) s->portals.Push(this);
}


class HWScenePortalBase
{
protected:
	IPortal *mOwner;
public:
	HWScenePortalBase() {}
	virtual ~HWScenePortalBase() {}
	void SetOwner(IPortal *p) { mOwner = p; }
	void ClearClipper(HWDrawInfo *di, Clipper *clipper);

	virtual int ClipSeg(seg_t *seg, const DVector3 &viewpos) { return PClip_Inside; }
	virtual int ClipSubsector(subsector_t *sub) { return PClip_Inside; }
	virtual int ClipPoint(const DVector2 &pos) { return PClip_Inside; }
	virtual line_t *ClipLine() { return nullptr; }

	virtual bool IsSky() { return false; }
	virtual bool NeedCap() { return false; }
	virtual bool NeedDepthBuffer() { return true; }
	virtual void * GetSource() const = 0;	// GetSource MUST be implemented!
	virtual const char *GetName() = 0;

	virtual bool Setup(HWDrawInfo *di, Clipper *clipper) = 0;
	virtual void Shutdown(HWDrawInfo *di) {}
	virtual void RenderAttached(HWDrawInfo *di) {}

};

struct HWLinePortal : public HWScenePortalBase
{
	// this must be the same as at the start of line_t, so that we can pass in this structure directly to P_ClipLineToPortal.
	vertex_t	*v1, *v2;	// vertices, from v1 to v2
	DVector2	delta;		// precalculated v2 - v1 for side checking

	angle_t		angv1, angv2;	// for quick comparisons with a line or subsector

	HWLinePortal(line_t *line)
	{
		v1 = line->v1;
		v2 = line->v2;
		CalcDelta();
	}

	HWLinePortal(FLinePortalSpan *line)
	{
		if (line->lines[0]->mType != PORTT_LINKED || line->v1 == nullptr)
		{
			// For non-linked portals we must check the actual linedef.
			line_t *lline = line->lines[0]->mDestination;
			v1 = lline->v1;
			v2 = lline->v2;
		}
		else
		{
			// For linked portals we can check the merged span.
			v1 = line->v1;
			v2 = line->v2;
		}
		CalcDelta();
	}

	void CalcDelta()
	{
		delta = v2->fPos() - v1->fPos();
	}

	line_t *line()
	{
		vertex_t **pv = &v1;
		return reinterpret_cast<line_t*>(pv);
	}

	int ClipSeg(seg_t *seg, const DVector3 &viewpos) override;
	int ClipSubsector(subsector_t *sub) override;
	int ClipPoint(const DVector2 &pos);
	bool NeedCap() override { return false; }
};

struct HWMirrorPortal : public HWLinePortal
{
	// mirror portals always consist of single linedefs!
	line_t * linedef;

protected:
	bool Setup(HWDrawInfo *di, Clipper *clipper) override;
	void Shutdown(HWDrawInfo *di) override;
	void * GetSource() const override { return linedef; }
	const char *GetName() override;

public:

	HWMirrorPortal(line_t * line)
		: HWLinePortal(line)
	{
		linedef = line;
	}
};


struct HWLineToLinePortal : public HWLinePortal
{
	FLinePortalSpan *glport;
protected:
	bool Setup(HWDrawInfo *di, Clipper *clipper) override;
	virtual void * GetSource() const override { return glport; }
	virtual const char *GetName() override;
	virtual line_t *ClipLine() override { return line(); }
	virtual void RenderAttached(HWDrawInfo *di) override;

public:

	HWLineToLinePortal(FLinePortalSpan *ll)
		: HWLinePortal(ll)
	{
		glport = ll;
	}
};


struct HWSkyboxPortal : public HWScenePortalBase
{
	bool oldclamp;
	int old_pm;
	FSectorPortal * portal;

protected:
	bool Setup(HWDrawInfo *di, Clipper *clipper) override;
	void Shutdown(HWDrawInfo *di) override;
	virtual void * GetSource() const { return portal; }
	virtual bool IsSky() { return true; }
	virtual const char *GetName();

public:


	HWSkyboxPortal(FSectorPortal * pt)
	{
		portal = pt;
	}

};


struct HWSectorStackPortal : public HWScenePortalBase
{
	TArray<subsector_t *> subsectors;
protected:
	bool Setup(HWDrawInfo *di, Clipper *clipper) override;
	void Shutdown(HWDrawInfo *di) override;
	virtual void * GetSource() const { return origin; }
	virtual bool IsSky() { return true; }	// although this isn't a real sky it can be handled as one.
	virtual const char *GetName();
	FSectorPortalGroup *origin;

public:

	HWSectorStackPortal(FSectorPortalGroup *pt)
	{
		origin = pt;
	}
	void SetupCoverage(HWDrawInfo *di);
	void AddSubsector(subsector_t *sub)
	{
		subsectors.Push(sub);
	}

};

struct HWPlaneMirrorPortal : public HWScenePortalBase
{
	int old_pm;
protected:
	bool Setup(HWDrawInfo *di, Clipper *clipper) override;
	void Shutdown(HWDrawInfo *di) override;
	virtual void * GetSource() const { return origin; }
	virtual const char *GetName();
	secplane_t * origin;

public:

	HWPlaneMirrorPortal(secplane_t * pt)
	{
		origin = pt;
	}

};

