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
#include "r_utility.h"
#include "actor.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_drawinfo.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_portal.h"

struct GLEEHorizonPortal;

class GLPortal : public IPortal
{

private:

	enum
	{
		STP_Stencil,
		STP_DepthClear,
		STP_DepthRestore,
		STP_AllInOne
	};
	void DrawPortalStencil(int pass);

	ActorRenderFlags savedvisibility;
	TArray<unsigned int> mPrimIndices;

protected:
	int level;

	GLPortal(FPortalSceneState *state, bool local = false) : IPortal(state, local) { }

	bool Start(bool usestencil, bool doquery, HWDrawInfo *outer_di, HWDrawInfo **pDi) override;
	void End(HWDrawInfo *di, bool usestencil) override;
	void ClearScreen(HWDrawInfo *di);
};

class GLScenePortal : public GLPortal
{
public:
	HWScenePortalBase *mScene;
	GLScenePortal(FPortalSceneState *state, HWScenePortalBase *handler) : GLPortal(state)
	{
		mScene = handler;
		handler->SetOwner(this);
	}
	~GLScenePortal() { delete mScene; }
	virtual void * GetSource() const { return mScene->GetSource(); }
	virtual const char *GetName() { return mScene->GetName(); }
	virtual bool IsSky() { return mScene->IsSky(); }
	virtual bool NeedCap() { return true; }
	virtual bool NeedDepthBuffer() { return true; }
	virtual void DrawContents(HWDrawInfo *di) 
	{ 
		if (mScene->Setup(di, di->mClipper))
		{
			static_cast<FDrawInfo*>(di)->DrawScene(DM_PORTAL);
			mScene->Shutdown(di);
		}
		else ClearScreen(di);
	}
	virtual void RenderAttached(HWDrawInfo *di) { return mScene->RenderAttached(di); }
};


struct GLSkyPortal : public GLPortal
{
	GLSkyInfo * origin;
	friend struct GLEEHorizonPortal;

protected:
	virtual void DrawContents(HWDrawInfo *di);
	virtual void * GetSource() const { return origin; }
	virtual bool IsSky() { return true; }
	virtual bool NeedDepthBuffer() { return false; }
	virtual const char *GetName();

public:

	
	GLSkyPortal(FPortalSceneState *state, GLSkyInfo *  pt, bool local = false)
		: GLPortal(state, local)
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
	virtual void DrawContents(HWDrawInfo *di);
	virtual void * GetSource() const { return origin; }
	virtual bool NeedDepthBuffer() { return false; }
	virtual bool NeedCap() { return false; }
	virtual const char *GetName();

public:
	
	GLHorizonPortal(FPortalSceneState *state, GLHorizonInfo * pt, FRenderViewpoint &vp, bool local = false);
};

struct GLEEHorizonPortal : public GLPortal
{
	FSectorPortal * portal;

protected:
	virtual void DrawContents(HWDrawInfo *di);
	virtual void * GetSource() const { return portal; }
	virtual bool NeedDepthBuffer() { return false; }
	virtual bool NeedCap() { return false; }
	virtual const char *GetName();

public:
	
	GLEEHorizonPortal(FPortalSceneState *state, FSectorPortal *pt) : GLPortal(state)
	{
		portal=pt;
	}

};

#endif
