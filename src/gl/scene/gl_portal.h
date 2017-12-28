/*
** gl_renderstruct.h
**   Generalized portal maintenance classes to make rendering special effects easier
**   and help add future extensions
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __GL_PORTAL_H
#define __GL_PORTAL_H

#include "tarray.h"
//#include "gl/gl_intern.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/utility/gl_templates.h"
#include "gl/data/gl_data.h"

struct GLHorizonInfo
{
	GLSectorPlane plane;
	int lightlevel;
	FColormap colormap;
	PalEntry specialcolor;
};

struct GLSkyInfo
{
	float x_offset[2];
	float y_offset;		// doubleskies don't have a y-offset
	FMaterial * texture[2];
	FTextureID skytexno1;
	bool mirrored;
	bool doublesky;
	bool sky2;
	PalEntry fadecolor;	// if this isn't made part of the dome things will become more complicated when sky fog is used.

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

extern UniqueList<GLSkyInfo> UniqueSkies;
extern UniqueList<GLHorizonInfo> UniqueHorizons;
extern UniqueList<secplane_t> UniquePlaneMirrors;
extern UniqueList<FGLLinePortal> UniqueLineToLines;
struct GLEEHorizonPortal;
class GLSceneDrawer;

class GLPortal
{
	static TArray<GLPortal *> portals;
	static int recursion;
	static unsigned int QueryObject;
protected:
	static TArray<float> planestack;
	static int MirrorFlag;
	static int PlaneMirrorFlag;
	static int renderdepth;

public:
	static GLSceneDrawer *drawer;
	static int PlaneMirrorMode;
	static int inupperstack;
	static int	instack[2];
	static bool	inskybox;

private:
	void DrawPortalStencil();

	DVector3 savedviewpath[2];
	DVector3 savedViewPos;
	DVector3 savedViewActorPos;
	DRotator savedAngles;
	bool savedshowviewer;
	AActor * savedviewactor;
	area_t savedviewarea;
	ActorRenderFlags savedvisibility;
	GLPortal *PrevPortal;
	GLPortal *PrevClipPortal;
	TArray<uint8_t> savedmapsection;
	TArray<unsigned int> mPrimIndices;

protected:
	TArray<GLWall> lines;
	int level;

	GLPortal(bool local = false) { if (!local) portals.Push(this); }
	virtual ~GLPortal() { }

	bool Start(bool usestencil, bool doquery);
	void End(bool usestencil);
	virtual void DrawContents()=0;
	virtual void * GetSource() const =0;	// GetSource MUST be implemented!
	void ClearClipper();
	virtual bool IsSky() { return false; }
	virtual bool NeedCap() { return true; }
	virtual bool NeedDepthBuffer() { return true; }
	void ClearScreen();
	virtual const char *GetName() = 0;
	void SaveMapSection();
	void RestoreMapSection();
	virtual void PushState() {}
	virtual void PopState() {}

public:

	enum
	{
		PClip_InFront,
		PClip_Inside,
		PClip_Behind,
	};

	void RenderPortal(bool usestencil, bool doquery)
	{
		// Start may perform an occlusion query. If that returns 0 there
		// is no need to draw the stencil's contents and there's also no
		// need to restore the affected area becasue there is none!
		if (Start(usestencil, doquery))
		{
			DrawContents();
			End(usestencil);
		}
	}

	void AddLine(GLWall * l)
	{
		lines.Push(*l);
	}

	static int GetRecursion()
	{
		return recursion;
	}

	virtual int ClipSeg(seg_t *seg) { return PClip_Inside; }
	virtual int ClipSubsector(subsector_t *sub) { return PClip_Inside; }
	virtual int ClipPoint(const DVector2 &pos) { return PClip_Inside; }
	virtual line_t *ClipLine() { return NULL; }
	virtual void RenderAttached() {}

	static void BeginScene();
	static void StartFrame();
	static bool RenderFirstSkyPortal(int recursion);
	static void EndFrame();
	static GLPortal * FindPortal(const void * src);

	static void Initialize();
	static void Shutdown();
};

struct GLLinePortal : public GLPortal
{
	// this must be the same as at the start of line_t, so that we can pass in this structure directly to P_ClipLineToPortal.
	vertex_t	*v1, *v2;	// vertices, from v1 to v2
	DVector2	delta;		// precalculated v2 - v1 for side checking

	angle_t		angv1, angv2;	// for quick comparisons with a line or subsector

	GLLinePortal(line_t *line)
	{
		v1 = line->v1;
		v2 = line->v2;
		CalcDelta();
	}

	GLLinePortal(FGLLinePortal *line)
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

	virtual int ClipSeg(seg_t *seg);
	virtual int ClipSubsector(subsector_t *sub);
	virtual int ClipPoint(const DVector2 &pos);
	virtual bool NeedCap() { return false; }
	virtual void PushState();
	virtual void PopState();
};


struct GLMirrorPortal : public GLLinePortal
{
	// mirror portals always consist of single linedefs!
	line_t * linedef;

protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return linedef; }
	virtual const char *GetName();

public:
	
	GLMirrorPortal(line_t * line)
		: GLLinePortal(line)
	{
		linedef=line;
	}
};


struct GLLineToLinePortal : public GLLinePortal
{
	FGLLinePortal *glport;
protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return glport; }
	virtual const char *GetName();
	virtual line_t *ClipLine() { return line(); }
	virtual void RenderAttached();

public:
	
	GLLineToLinePortal(FGLLinePortal *ll)
		: GLLinePortal(ll)
	{
		glport = ll;
	}
};


struct GLSkyboxPortal : public GLPortal
{
	FSectorPortal * portal;

protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return portal; }
	virtual bool IsSky() { return true; }
	virtual const char *GetName();

public:

	
	GLSkyboxPortal(FSectorPortal * pt)
	{
		portal=pt;
	}

};


struct GLSkyPortal : public GLPortal
{
	GLSkyInfo * origin;
	friend struct GLEEHorizonPortal;

protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return origin; }
	virtual bool IsSky() { return true; }
	virtual bool NeedDepthBuffer() { return false; }
	virtual const char *GetName();

public:

	
	GLSkyPortal(GLSkyInfo *  pt, bool local = false)
		: GLPortal(local)
	{
		origin=pt;
	}

};



struct GLSectorStackPortal : public GLPortal
{
	TArray<subsector_t *> subsectors;
protected:
	virtual ~GLSectorStackPortal();
	virtual void DrawContents();
	virtual void * GetSource() const { return origin; }
	virtual bool IsSky() { return true; }	// although this isn't a real sky it can be handled as one.
	virtual const char *GetName();
	FPortal *origin;

public:
	
	GLSectorStackPortal(FPortal *pt) 
	{
		origin=pt;
	}
	void SetupCoverage();
	void AddSubsector(subsector_t *sub)
	{
		subsectors.Push(sub);
	}

};

struct GLPlaneMirrorPortal : public GLPortal
{
protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return origin; }
	virtual const char *GetName();
	virtual void PushState();
	virtual void PopState();
	secplane_t * origin;

public:

	GLPlaneMirrorPortal(secplane_t * pt) 
	{
		origin=pt;
	}

};


struct GLHorizonPortal : public GLPortal
{
	GLHorizonInfo * origin;
	unsigned int voffset;
	unsigned int vcount;
	friend struct GLEEHorizonPortal;

protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return origin; }
	virtual bool NeedDepthBuffer() { return false; }
	virtual bool NeedCap() { return false; }
	virtual const char *GetName();

public:
	
	GLHorizonPortal(GLHorizonInfo * pt, bool local = false);
};

struct GLEEHorizonPortal : public GLPortal
{
	FSectorPortal * portal;

protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return portal; }
	virtual bool NeedDepthBuffer() { return false; }
	virtual bool NeedCap() { return false; }
	virtual const char *GetName();

public:
	
	GLEEHorizonPortal(FSectorPortal *pt)
	{
		portal=pt;
	}

};

#endif
