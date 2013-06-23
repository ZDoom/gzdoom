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

class ASkyViewpoint;

struct GLHorizonInfo
{
	GLSectorPlane plane;
	int lightlevel;
	FColormap colormap;
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
};

extern UniqueList<GLSkyInfo> UniqueSkies;
extern UniqueList<GLHorizonInfo> UniqueHorizons;
extern UniqueList<secplane_t> UniquePlaneMirrors;

class GLPortal
{
	static TArray<GLPortal *> portals;
	static int recursion;
	static unsigned int QueryObject;
protected:
	static int MirrorFlag;
	static int PlaneMirrorFlag;
	static int renderdepth;

public:
	static int PlaneMirrorMode;
	static int inupperstack;
	static int	instack[2];
	static bool	inskybox;

private:
	void DrawPortalStencil();

	fixed_t savedviewx;
	fixed_t savedviewy;
	fixed_t savedviewz;
	angle_t savedviewangle;
	AActor * savedviewactor;
	area_t savedviewarea;
	unsigned char clipsave;
	GLPortal *NextPortal;
	TArray<BYTE> savedmapsection;

protected:
	TArray<GLWall> lines;
	int level;

	GLPortal() { portals.Push(this); }
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

public:

	enum
	{
		PClip_InFront,
		PClip_Inside,
		PClip_Behind
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
	virtual int ClipPoint(fixed_t x, fixed_t y) { return PClip_Inside; }

	static void BeginScene();
	static void StartFrame();
	static bool RenderFirstSkyPortal(int recursion);
	static void EndFrame();
	static GLPortal * FindPortal(const void * src);
};


struct GLMirrorPortal : public GLPortal
{
	// mirror portals always consist of single linedefs!
	line_t * linedef;

protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return linedef; }
	virtual const char *GetName();

public:
	
	GLMirrorPortal(line_t * line)
	{
		linedef=line;
	}

	virtual bool NeedCap() { return false; }
	virtual int ClipSeg(seg_t *seg);
	virtual int ClipPoint(fixed_t x, fixed_t y);
};


struct GLSkyboxPortal : public GLPortal
{
	AActor * origin;

protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return origin; }
	virtual bool IsSky() { return true; } // later!
	virtual const char *GetName();

public:

	
	GLSkyboxPortal(AActor * pt)
	{
		origin=pt;
	}

};


struct GLSkyPortal : public GLPortal
{
	GLSkyInfo * origin;

protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return origin; }
	virtual bool IsSky() { return true; }
	virtual bool NeedDepthBuffer() { return false; }
	virtual const char *GetName();

public:

	
	GLSkyPortal(GLSkyInfo *  pt)
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

protected:
	virtual void DrawContents();
	virtual void * GetSource() const { return origin; }
	virtual bool NeedDepthBuffer() { return false; }
	virtual bool NeedCap() { return false; }
	virtual const char *GetName();

public:
	
	GLHorizonPortal(GLHorizonInfo * pt)
	{
		origin=pt;
	}

};

#endif
