/*
** gl_walls_draw.cpp
** Wall rendering
**
**---------------------------------------------------------------------------
** Copyright 2000-2005 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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

#include "gl/system/gl_system.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "a_sharedglobal.h"
#include "gl/gl_functions.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"

EXTERN_CVAR(Bool, gl_seamless)

//==========================================================================
//
// Sets up the texture coordinates for one light to be rendered
//
//==========================================================================
bool GLWall::PrepareLight(texcoord * tcs, ADynamicLight * light)
{
	float vtx[]={glseg.x1,zbottom[0],glseg.y1, glseg.x1,ztop[0],glseg.y1, glseg.x2,ztop[1],glseg.y2, glseg.x2,zbottom[1],glseg.y2};
	Plane p;
	Vector nearPt, up, right;
	float scale;

	p.Init(vtx,4);

	if (!p.ValidNormal()) 
	{
		return false;
	}

	if (!gl_SetupLight(p, light, nearPt, up, right, scale, Colormap.colormap, true, !!(flags&GLWF_FOGGY))) 
	{
		return false;
	}

	if (tcs != NULL)
	{
		Vector t1;
		int outcnt[4]={0,0,0,0};

		for(int i=0;i<4;i++)
		{
			t1.Set(&vtx[i*3]);
			Vector nearToVert = t1 - nearPt;
			tcs[i].u = (nearToVert.Dot(right) * scale) + 0.5f;
			tcs[i].v = (nearToVert.Dot(up) * scale) + 0.5f;

			// quick check whether the light touches this polygon
			if (tcs[i].u<0) outcnt[0]++;
			if (tcs[i].u>1) outcnt[1]++;
			if (tcs[i].v<0) outcnt[2]++;
			if (tcs[i].v>1) outcnt[3]++;

		}
		// The light doesn't touch this polygon
		if (outcnt[0]==4 || outcnt[1]==4 || outcnt[2]==4 || outcnt[3]==4) return false;
	}

	draw_dlight++;
	return true;
}

//==========================================================================
//
// Collect lights for shader
//
//==========================================================================
FDynLightData lightdata;

void GLWall::SetupLights()
{
	float vtx[]={glseg.x1,zbottom[0],glseg.y1, glseg.x1,ztop[0],glseg.y1, glseg.x2,ztop[1],glseg.y2, glseg.x2,zbottom[1],glseg.y2};
	Plane p;

	lightdata.Clear();
	p.Init(vtx,4);

	if (!p.ValidNormal()) 
	{
		return;
	}
	for(int i=0;i<2;i++)
	{
		FLightNode *node;
		if (seg->sidedef == NULL)
		{
			node = NULL;
		}
		else if (!(seg->sidedef->Flags & WALLF_POLYOBJ))
		{
			node = seg->sidedef->lighthead[i];
		}
		else if (sub)
		{
			// Polobject segs cannot be checked per sidedef so use the subsector instead.
			node = sub->lighthead[i];
		}
		else node = NULL;

		// Iterate through all dynamic lights which touch this wall and render them
		while (node)
		{
			if (!(node->lightsource->flags2&MF2_DORMANT))
			{
				iter_dlight++;

				Vector fn, pos;

				float x = FIXED2FLOAT(node->lightsource->x);
				float y = FIXED2FLOAT(node->lightsource->y);
				float z = FIXED2FLOAT(node->lightsource->z);
				float dist = fabsf(p.DistToPoint(x, z, y));
				float radius = (node->lightsource->GetRadius() * gl_lights_size);
				float scale = 1.0f / ((2.f * radius) - dist);

				if (radius > 0.f && dist < radius)
				{
					Vector nearPt, up, right;

					pos.Set(x,z,y);
					fn=p.Normal();
					fn.GetRightUp(right, up);

					Vector tmpVec = fn * dist;
					nearPt = pos + tmpVec;

					Vector t1;
					int outcnt[4]={0,0,0,0};
					texcoord tcs[4];

					// do a quick check whether the light touches this polygon
					for(int i=0;i<4;i++)
					{
						t1.Set(&vtx[i*3]);
						Vector nearToVert = t1 - nearPt;
						tcs[i].u = (nearToVert.Dot(right) * scale) + 0.5f;
						tcs[i].v = (nearToVert.Dot(up) * scale) + 0.5f;

						if (tcs[i].u<0) outcnt[0]++;
						if (tcs[i].u>1) outcnt[1]++;
						if (tcs[i].v<0) outcnt[2]++;
						if (tcs[i].v>1) outcnt[3]++;

					}
					if (outcnt[0]!=4 && outcnt[1]!=4 && outcnt[2]!=4 && outcnt[3]!=4) 
					{
						gl_GetLight(p, node->lightsource, Colormap.colormap, true, false, lightdata);
					}
				}
			}
			node = node->nextLight;
		}
	}
	int numlights[3];

	lightdata.Combine(numlights, gl.MaxLights());
	if (numlights[2] > 0)
	{
		draw_dlight+=numlights[2]/2;
		gl_RenderState.EnableLight(true);
		gl_RenderState.SetLights(numlights, &lightdata.arrays[0][0]);
	}
}

//==========================================================================
//
// General purpose wall rendering function
// everything goes through here
//
//==========================================================================

void GLWall::RenderWall(int textured, float * color2, ADynamicLight * light)
{
	texcoord tcs[4];
	bool split = (gl_seamless && !(textured&4) && seg->sidedef != NULL && !(seg->sidedef->Flags & WALLF_POLYOBJ));

	if (!light)
	{
		tcs[0]=lolft;
		tcs[1]=uplft;
		tcs[2]=uprgt;
		tcs[3]=lorgt;
		if (!!(flags&GLWF_GLOW) && (textured & 2))
		{
			gl_RenderState.SetGlowPlanes(topplane, bottomplane);
			gl_RenderState.SetGlowParams(topglowcolor, bottomglowcolor);
		}

	}
	else
	{
		if (!PrepareLight(tcs, light)) return;
	}


	gl_RenderState.Apply();

	// the rest of the code is identical for textured rendering and lights

	glBegin(GL_TRIANGLE_FAN);

	// lower left corner
	if (textured&1) glTexCoord2f(tcs[0].u,tcs[0].v);
	glVertex3f(glseg.x1,zbottom[0],glseg.y1);

	if (split && glseg.fracleft==0) SplitLeftEdge(tcs);

	// upper left corner
	if (textured&1) glTexCoord2f(tcs[1].u,tcs[1].v);
	glVertex3f(glseg.x1,ztop[0],glseg.y1);

	if (split && !(flags & GLWF_NOSPLITUPPER)) SplitUpperEdge(tcs);

	// color for right side
	if (color2) glColor4fv(color2);

	// upper right corner
	if (textured&1) glTexCoord2f(tcs[2].u,tcs[2].v);
	glVertex3f(glseg.x2,ztop[1],glseg.y2);

	if (split && glseg.fracright==1) SplitRightEdge(tcs);

	// lower right corner
	if (textured&1) glTexCoord2f(tcs[3].u,tcs[3].v); 
	glVertex3f(glseg.x2,zbottom[1],glseg.y2);

	if (split && !(flags & GLWF_NOSPLITLOWER)) SplitLowerEdge(tcs);

	glEnd();

	vertexcount+=4;

}

//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderFogBoundary()
{
	if (gl_fogmode && gl_fixedcolormap == 0)
	{
		// with shaders this can be done properly
		if (gl.shadermodel == 4 || (gl.shadermodel == 3 && gl_fog_shader))
		{
			int rel = rellight + getExtraLight();
			gl_SetFog(lightlevel, rel, &Colormap, false);
			gl_RenderState.SetEffect(EFF_FOGBOUNDARY);
			gl_RenderState.EnableAlphaTest(false);
			RenderWall(0, NULL);
			gl_RenderState.EnableAlphaTest(true);
			gl_RenderState.SetEffect(EFF_NONE);
		}
		else
		{
			// otherwise some approximation is needed. This won't look as good
			// as the shader version but it's an acceptable compromise.
			float fogdensity=gl_GetFogDensity(lightlevel, Colormap.FadeColor);

			float xcamera=FIXED2FLOAT(viewx);
			float ycamera=FIXED2FLOAT(viewy);

			float dist1=Dist2(xcamera,ycamera, glseg.x1,glseg.y1);
			float dist2=Dist2(xcamera,ycamera, glseg.x2,glseg.y2);


			// these values were determined by trial and error and are scale dependent!
			float fogd1=(0.95f-exp(-fogdensity*dist1/62500.f)) * 1.05f;
			float fogd2=(0.95f-exp(-fogdensity*dist2/62500.f)) * 1.05f;

			gl_ModifyColor(Colormap.FadeColor.r, Colormap.FadeColor.g, Colormap.FadeColor.b, Colormap.colormap);
			float fc[4]={Colormap.FadeColor.r/255.0f,Colormap.FadeColor.g/255.0f,Colormap.FadeColor.b/255.0f,fogd2};

			gl_RenderState.EnableTexture(false);
			gl_RenderState.EnableFog(false);
			gl_RenderState.AlphaFunc(GL_GREATER,0);
			glDepthFunc(GL_LEQUAL);
			glColor4f(fc[0],fc[1],fc[2], fogd1);
			if (glset.lightmode == 8) glVertexAttrib1f(VATTR_LIGHTLEVEL, 1.0); // Korshun.

			flags &= ~GLWF_GLOW;
			RenderWall(4,fc);

			glDepthFunc(GL_LESS);
			gl_RenderState.EnableFog(true);
			gl_RenderState.AlphaFunc(GL_GEQUAL,0.5f);
			gl_RenderState.EnableTexture(true);
		}
	}
}


//==========================================================================
//
// 
//
//==========================================================================
void GLWall::RenderMirrorSurface()
{
	if (GLRenderer->mirrortexture == NULL) return;

	// For the sphere map effect we need a normal of the mirror surface,
	Vector v(glseg.y2-glseg.y1, 0 ,-glseg.x2+glseg.x1);
	v.Normalize();
	glNormal3fv(&v[0]);

	// Use sphere mapping for this
	gl_RenderState.SetEffect(EFF_SPHEREMAP);

	gl_SetColor(lightlevel, 0, &Colormap ,0.1f);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA,GL_ONE);
	gl_RenderState.AlphaFunc(GL_GREATER,0);
	glDepthFunc(GL_LEQUAL);
	gl_SetFog(lightlevel, getExtraLight(), &Colormap, true);

	FMaterial * pat=FMaterial::ValidateTexture(GLRenderer->mirrortexture);
	pat->BindPatch(Colormap.colormap, 0);

	flags &= ~GLWF_GLOW;
	//flags |= GLWF_NOSHADER;
	RenderWall(0,NULL);

	gl_RenderState.SetEffect(EFF_NONE);

	// Restore the defaults for the translucent pass
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.AlphaFunc(GL_GEQUAL,0.5f*gl_mask_sprite_threshold);
	glDepthFunc(GL_LESS);

	// This is drawn in the translucent pass which is done after the decal pass
	// As a result the decals have to be drawn here.
	if (seg->sidedef->AttachedDecals)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -128.0f);
		glDepthMask(false);
		DoDrawDecals();
		glDepthMask(true);
		glPolygonOffset(0.0f, 0.0f);
		glDisable(GL_POLYGON_OFFSET_FILL);
		gl_RenderState.SetTextureMode(TM_MODULATE);
		gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
}


//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderTranslucentWall()
{
	bool transparent = gltexture? gltexture->GetTransparent() : false;
	
	// currently the only modes possible are solid, additive or translucent
	// and until that changes I won't fix this code for the new blending modes!
	bool isadditive = RenderStyle == STYLE_Add;

	if (!transparent) gl_RenderState.AlphaFunc(GL_GEQUAL,gl_mask_threshold*fabs(alpha));
	else gl_RenderState.EnableAlphaTest(false);
	if (isadditive) gl_RenderState.BlendFunc(GL_SRC_ALPHA,GL_ONE);

	int extra;
	if (gltexture) 
	{
		if (flags&GLWF_FOGGY) gl_RenderState.EnableBrightmap(false);
		gl_RenderState.EnableGlow(!!(flags & GLWF_GLOW));
		gltexture->Bind(Colormap.colormap, flags, 0);
		extra = getExtraLight();
	}
	else 
	{
		gl_RenderState.EnableTexture(false);
		extra = 0;
	}

	gl_SetColor(lightlevel, extra, &Colormap, fabsf(alpha));
	if (type!=RENDERWALL_M2SNF) gl_SetFog(lightlevel, extra, &Colormap, isadditive);
	else gl_SetFog(255, 0, NULL, false);

	RenderWall(5,NULL);

	// restore default settings
	if (isadditive) gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (transparent) gl_RenderState.EnableAlphaTest(true);

	if (!gltexture)	
	{
		gl_RenderState.EnableTexture(true);
	}
	gl_RenderState.EnableBrightmap(true);
	gl_RenderState.EnableGlow(false);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::Draw(int pass)
{
	FLightNode * node;
	int rel;

#ifdef _DEBUG
	if (seg->linedef-lines==879)
	{
		int a = 0;
	}
#endif


	// This allows mid textures to be drawn on lines that might overlap a sky wall
	if ((flags&GLWF_SKYHACK && type==RENDERWALL_M2S) || type == RENDERWALL_COLORLAYER)
	{
		if (pass != GLPASS_DECALS)
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(-1.0f, -128.0f);
		}
	}

	switch (pass)
	{
	case GLPASS_ALL:			// Single-pass rendering
		SetupLights();
		// fall through
	case GLPASS_PLAIN:			// Single-pass rendering
		rel = rellight + getExtraLight();
		gl_SetColor(lightlevel, rel, &Colormap,1.0f);
		if (type!=RENDERWALL_M2SNF) gl_SetFog(lightlevel, rel, &Colormap, false);
		else gl_SetFog(255, 0, NULL, false);

		gl_RenderState.EnableGlow(!!(flags & GLWF_GLOW));
		gltexture->Bind(Colormap.colormap, flags, 0);
		RenderWall(3, NULL);
		gl_RenderState.EnableGlow(false);
		gl_RenderState.EnableLight(false);
		break;

	case GLPASS_BASE:			// Base pass for non-masked polygons (all opaque geometry)
	case GLPASS_BASE_MASKED:	// Base pass for masked polygons (2sided mid-textures and transparent 3D floors)
		rel = rellight + getExtraLight();
		gl_SetColor(lightlevel, rel, &Colormap,1.0f);
		if (!(flags&GLWF_FOGGY)) 
		{
			if (type!=RENDERWALL_M2SNF) gl_SetFog(lightlevel, rel, &Colormap, false);
			else gl_SetFog(255, 0, NULL, false);
		}
		gl_RenderState.EnableGlow(!!(flags & GLWF_GLOW));
		// fall through

		if (pass != GLPASS_BASE)
		{
			gltexture->Bind(Colormap.colormap, flags, 0);
		}
		RenderWall(pass == GLPASS_BASE? 2:3, NULL);
		gl_RenderState.EnableGlow(false);
		gl_RenderState.EnableLight(false);
		break;

	case GLPASS_TEXTURE:		// modulated texture
		gltexture->Bind(Colormap.colormap, flags, 0);
		RenderWall(1, NULL);
		break;

	case GLPASS_LIGHT:
	case GLPASS_LIGHT_ADDITIVE:
		// black fog is diminishing light and should affect lights less than the rest!
		if (!(flags&GLWF_FOGGY)) gl_SetFog((255+lightlevel)>>1, 0, NULL, false);
		else gl_SetFog(lightlevel, 0, &Colormap, true);	

		if (seg->sidedef == NULL)
		{
			node = NULL;
		}
		else if (!(seg->sidedef->Flags & WALLF_POLYOBJ))
		{
			// Iterate through all dynamic lights which touch this wall and render them
			node = seg->sidedef->lighthead[pass==GLPASS_LIGHT_ADDITIVE];
		}
		else if (sub)
		{
			// To avoid constant rechecking for polyobjects use the subsector's lightlist instead
			node = sub->lighthead[pass==GLPASS_LIGHT_ADDITIVE];
		}
		else node = NULL;
		while (node)
		{
			if (!(node->lightsource->flags2&MF2_DORMANT))
			{
				iter_dlight++;
				RenderWall(1, NULL, node->lightsource);
			}
			node = node->nextLight;
		}
		break;

	case GLPASS_DECALS:
	case GLPASS_DECALS_NOFOG:
		if (seg->sidedef && seg->sidedef->AttachedDecals)
		{
			if (pass==GLPASS_DECALS) 
			{
				gl_SetFog(lightlevel, rellight + getExtraLight(), &Colormap, false);
			}
			DoDrawDecals();
		}
		break;

	case GLPASS_TRANSLUCENT:
		switch (type)
		{
		case RENDERWALL_MIRRORSURFACE:
			RenderMirrorSurface();
			break;

		case RENDERWALL_FOGBOUNDARY:
			RenderFogBoundary();
			break;

		default:
			RenderTranslucentWall();
			break;
		}
	}

	if ((flags&GLWF_SKYHACK && type==RENDERWALL_M2S) || type == RENDERWALL_COLORLAYER)
	{
		if (pass!=GLPASS_DECALS)
		{
			glDisable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(0, 0);
		}
	}
}
