/*
** gl_flat.cpp
** Flat rendering
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
#include "a_sharedglobal.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"
#include "g_level.h"
#include "doomstat.h"
#include "d_player.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_convert.h"
#include "gl/utility/gl_templates.h"

//==========================================================================
//
// Sets the texture matrix according to the plane's texture positioning
// information
//
//==========================================================================

bool gl_SetPlaneTextureRotation(const GLSectorPlane * secplane, FMaterial * gltexture)
{
	// only manipulate the texture matrix if needed.
	if (secplane->xoffs != 0 || secplane->yoffs != 0 ||
		secplane->xscale != FRACUNIT || secplane->yscale != FRACUNIT ||
		secplane->angle != 0 || 
		gltexture->TextureWidth(GLUSE_TEXTURE) != 64 ||
		gltexture->TextureHeight(GLUSE_TEXTURE) != 64)
	{
		float uoffs=FIXED2FLOAT(secplane->xoffs)/gltexture->TextureWidth(GLUSE_TEXTURE);
		float voffs=FIXED2FLOAT(secplane->yoffs)/gltexture->TextureHeight(GLUSE_TEXTURE);

		float xscale1=FIXED2FLOAT(secplane->xscale);
		float yscale1=FIXED2FLOAT(secplane->yscale);
		if (gltexture->tex->bHasCanvas)
		{
			yscale1 = 0 - yscale1;
		}
		float angle=-ANGLE_TO_FLOAT(secplane->angle);

		float xscale2=64.f/gltexture->TextureWidth(GLUSE_TEXTURE);
		float yscale2=64.f/gltexture->TextureHeight(GLUSE_TEXTURE);

		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glScalef(xscale1 ,yscale1,1.0f);
		glTranslatef(uoffs,voffs,0.0f);
		glScalef(xscale2 ,yscale2,1.0f);
		glRotatef(angle,0.0f,0.0f,1.0f);
		return true;
	}
	return false;
}


//==========================================================================
//
// Flats 
//
//==========================================================================

void GLFlat::DrawSubsectorLights(subsector_t * sub, int pass)
{
	Plane p;
	Vector nearPt, up, right, t1;
	float scale;
	unsigned int k;
	seg_t *v;

	FLightNode * node = sub->lighthead[pass==GLPASS_LIGHT_ADDITIVE];
	gl_RenderState.Apply();
	while (node)
	{
		ADynamicLight * light = node->lightsource;
		
		if (light->flags2&MF2_DORMANT)
		{
			node=node->nextLight;
			continue;
		}
		iter_dlightf++;

		// we must do the side check here because gl_SetupLight needs the correct plane orientation
		// which we don't have for Legacy-style 3D-floors
		fixed_t planeh = plane.plane.ZatPoint(light->x, light->y);
		if (gl_lights_checkside && ((planeh<light->z && ceiling) || (planeh>light->z && !ceiling)))
		{
			node=node->nextLight;
			continue;
		}

		p.Set(plane.plane);
		if (!gl_SetupLight(p, light, nearPt, up, right, scale, Colormap.colormap, false, foggy)) 
		{
			node=node->nextLight;
			continue;
		}
		draw_dlightf++;

		// Render the light
		glBegin(GL_TRIANGLE_FAN);
		for(k = 0, v = sub->firstline; k < sub->numlines; k++, v++)
		{
			vertex_t *vt = v->v1;
			float zc = plane.plane.ZatPoint(vt->fx, vt->fy) + dz;

			t1.Set(vt->fx, zc, vt->fy);
			Vector nearToVert = t1 - nearPt;
			glTexCoord2f( (nearToVert.Dot(right) * scale) + 0.5f, (nearToVert.Dot(up) * scale) + 0.5f);

			glVertex3f(vt->fx, zc, vt->fy);
		}

		glEnd();
		node = node->nextLight;
	}
}


//==========================================================================
//
// Flats 
//
//==========================================================================
extern FDynLightData lightdata;

bool GLFlat::SetupSubsectorLights(bool lightsapplied, subsector_t * sub)
{
	Plane p;

	lightdata.Clear();
	for(int i=0;i<2;i++)
	{
		FLightNode * node = sub->lighthead[i];
		while (node)
		{
			ADynamicLight * light = node->lightsource;
			
			if (light->flags2&MF2_DORMANT)
			{
				node=node->nextLight;
				continue;
			}
			iter_dlightf++;

			// we must do the side check here because gl_SetupLight needs the correct plane orientation
			// which we don't have for Legacy-style 3D-floors
			fixed_t planeh = plane.plane.ZatPoint(light->x, light->y);
			if (gl_lights_checkside && ((planeh<light->z && ceiling) || (planeh>light->z && !ceiling)))
			{
				node=node->nextLight;
				continue;
			}

			p.Set(plane.plane);
			gl_GetLight(p, light, Colormap.colormap, false, false, lightdata);
			node = node->nextLight;
		}
	}

	int numlights[3];

	lightdata.Combine(numlights, gl.MaxLights());
	if (numlights[2] > 0)
	{
		draw_dlightf+=numlights[2]/2;
		gl_RenderState.EnableLight(true);
		gl_RenderState.SetLights(numlights, &lightdata.arrays[0][0]);
		gl_RenderState.Apply();
		return true;
	}
	if (lightsapplied) 
	{
		gl_RenderState.EnableLight(false);
		gl_RenderState.Apply();
	}
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

void GLFlat::DrawSubsector(subsector_t * sub)
{
	glBegin(GL_TRIANGLE_FAN);

	for(unsigned int k=0; k<sub->numlines; k++)
	{
		vertex_t *vt = sub->firstline[k].v1;
		glTexCoord2f(vt->fx/64.f, -vt->fy/64.f);
		float zc = plane.plane.ZatPoint(vt->fx, vt->fy) + dz;
		glVertex3f(vt->fx, zc, vt->fy);
	}
	glEnd();

	flatvertices += sub->numlines;
	flatprimitives++;
}

//==========================================================================
//
//
//
//==========================================================================

void GLFlat::DrawSubsectors(int pass, bool istrans)
{
	bool lightsapplied = false;

	gl_RenderState.Apply();
	if (sub)
	{
		// This represents a single subsector
		if (pass == GLPASS_ALL) lightsapplied = SetupSubsectorLights(lightsapplied, sub);
		DrawSubsector(sub);
	}
	else
	{
		if (vboindex >= 0)
		{
			//glColor3f( 1.f,.5f,.5f);
			int index = vboindex;
			for (int i=0; i<sector->subsectorcount; i++)
			{
				subsector_t * sub = sector->subsectors[i];
				// This is just a quick hack to make translucent 3D floors and portals work.
				if (gl_drawinfo->ss_renderflags[sub-subsectors]&renderflags || istrans)
				{
					if (pass == GLPASS_ALL) lightsapplied = SetupSubsectorLights(lightsapplied, sub);
					glDrawArrays(GL_TRIANGLE_FAN, index, sub->numlines);
					flatvertices += sub->numlines;
					flatprimitives++;
				}
				index += sub->numlines;
			}
		}
		else
		{
			//glColor3f( .5f,1.f,.5f); // these are for testing the VBO stuff.
			// Draw the subsectors belonging to this sector
			for (int i=0; i<sector->subsectorcount; i++)
			{
				subsector_t * sub = sector->subsectors[i];
				if (gl_drawinfo->ss_renderflags[sub-subsectors]&renderflags || istrans)
				{
					if (pass == GLPASS_ALL) lightsapplied = SetupSubsectorLights(lightsapplied, sub);
					DrawSubsector(sub);
				}
			}
		}

		// Draw the subsectors assigned to it due to missing textures
		if (!(renderflags&SSRF_RENDER3DPLANES))
		{
			gl_subsectorrendernode * node = (renderflags&SSRF_RENDERFLOOR)?
				gl_drawinfo->GetOtherFloorPlanes(sector->sectornum) :
				gl_drawinfo->GetOtherCeilingPlanes(sector->sectornum);

			while (node)
			{
				if (pass == GLPASS_ALL) lightsapplied = SetupSubsectorLights(lightsapplied, node->sub);
				DrawSubsector(node->sub);
				node = node->next;
			}
		}
	}
	gl_RenderState.EnableLight(false);
}


//==========================================================================
//
//
//
//==========================================================================
void GLFlat::Draw(int pass)
{
	int i;
	int rel = getExtraLight();

#ifdef _DEBUG
	if (sector->sectornum == 2)
	{
		int a = 0;
	}
#endif


	switch (pass)
	{
	case GLPASS_BASE:
		gl_SetColor(lightlevel, rel, &Colormap,1.0f);
		if (!foggy) gl_SetFog(lightlevel, rel, &Colormap, false);
		DrawSubsectors(pass, false);
		break;

	case GLPASS_PLAIN:			// Single-pass rendering
	case GLPASS_ALL:
	case GLPASS_BASE_MASKED:
		gl_SetColor(lightlevel, rel, &Colormap,1.0f);
		if (!foggy || pass != GLPASS_BASE_MASKED) gl_SetFog(lightlevel, rel, &Colormap, false);
		// fall through
	case GLPASS_TEXTURE:
	{
		gltexture->Bind(Colormap.colormap);
		bool pushed = gl_SetPlaneTextureRotation(&plane, gltexture);
		DrawSubsectors(pass, false);
		if (pushed) 
		{
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
		}
		break;
	}

	case GLPASS_LIGHT:
	case GLPASS_LIGHT_ADDITIVE:

		if (!foggy)	gl_SetFog((255+lightlevel)>>1, 0, &Colormap, false);
		else gl_SetFog(lightlevel, 0, &Colormap, true);	

		if (sub)
		{
			DrawSubsectorLights(sub, pass);
		}
		else
		{
			// Draw the subsectors belonging to this sector
			for (i=0; i<sector->subsectorcount; i++)
			{
				subsector_t * sub = sector->subsectors[i];

				if (gl_drawinfo->ss_renderflags[sub-subsectors]&renderflags)
				{
					DrawSubsectorLights(sub, pass);
				}
			}

			// Draw the subsectors assigned to it due to missing textures
			if (!(renderflags&SSRF_RENDER3DPLANES))
			{
				gl_subsectorrendernode * node = (renderflags&SSRF_RENDERFLOOR)?
					gl_drawinfo->GetOtherFloorPlanes(sector->sectornum) :
					gl_drawinfo->GetOtherCeilingPlanes(sector->sectornum);

				while (node)
				{
					DrawSubsectorLights(node->sub, pass);
					node = node->next;
				}
			}
		}
		break;

	case GLPASS_TRANSLUCENT:
		if (renderstyle==STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE);
		gl_SetColor(lightlevel, rel, &Colormap, alpha);
		gl_SetFog(lightlevel, rel, &Colormap, false);
		gl_RenderState.AlphaFunc(GL_GEQUAL,gl_mask_threshold*(alpha));
		if (!gltexture)	
		{
			gl_RenderState.EnableTexture(false);
			DrawSubsectors(pass, true);
			gl_RenderState.EnableTexture(true);
		}
		else 
		{
			if (foggy) gl_RenderState.EnableBrightmap(false);
			gltexture->Bind(Colormap.colormap);
			bool pushed = gl_SetPlaneTextureRotation(&plane, gltexture);
			DrawSubsectors(pass, true);
			gl_RenderState.EnableBrightmap(true);
			if (pushed)
			{
				glPopMatrix();
				glMatrixMode(GL_MODELVIEW);
			}
		}
		if (renderstyle==STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	}
}


//==========================================================================
//
// GLFlat::PutFlat
//
// Checks texture, lighting and translucency settings and puts this
// plane in the appropriate render list.
//
//==========================================================================
inline void GLFlat::PutFlat(bool fog)
{
	int list;

	if (gl_fixedcolormap) 
	{
		Colormap.GetFixedColormap();
	}
	if (renderstyle!=STYLE_Translucent || alpha < 1.f - FLT_EPSILON || fog)
	{
		int list = (renderflags&SSRF_RENDER3DPLANES) ? GLDL_TRANSLUCENT : GLDL_TRANSLUCENTBORDER;
		gl_drawinfo->drawlists[list].AddFlat (this);
	}
	else if (gltexture != NULL)
	{
		static DrawListType list_indices[2][2][2]={
			{ { GLDL_PLAIN, GLDL_FOG      }, { GLDL_MASKED,      GLDL_FOGMASKED      } },
			{ { GLDL_LIGHT, GLDL_LIGHTFOG }, { GLDL_LIGHTMASKED, GLDL_LIGHTFOGMASKED } }
		};

		bool light = gl_forcemultipass;
		bool masked = gltexture->isMasked() && ((renderflags&SSRF_RENDER3DPLANES) || stack);

		if (!gl_fixedcolormap)
		{
			foggy = gl_CheckFog(&Colormap, lightlevel) || level.flags&LEVEL_HASFADETABLE;

			if (gl_lights && !gl_dynlight_shader && GLRenderer->mLightCount)	// Are lights touching this sector?
			{
				for(int i=0;i<sector->subsectorcount;i++) if (sector->subsectors[i]->lighthead[0] != NULL)
				{
					light=true;
				}
			}
		}
		else foggy = false;

		list = list_indices[light][masked][foggy];
		if (list == GLDL_LIGHT && gltexture->tex->gl_info.Brightmap && gl_BrightmapsActive()) list = GLDL_LIGHTBRIGHT;

		gl_drawinfo->drawlists[list].AddFlat (this);
	}
}

//==========================================================================
//
// This draws one flat 
// The passed sector does not indicate the area which is rendered. 
// It is only used as source for the plane data.
// The whichplane boolean indicates if the flat is a floor(false) or a ceiling(true)
//
//==========================================================================

void GLFlat::Process(sector_t * model, int whichplane, bool fog)
{
	plane.GetFromSector(model, whichplane);

	if (!fog)
	{
		if (plane.texture==skyflatnum) return;

		gltexture=FMaterial::ValidateTexture(plane.texture, true);
		if (!gltexture) return;
		if (gltexture->tex->isFullbright()) 
		{
			Colormap.LightColor.r = Colormap.LightColor.g = Colormap.LightColor.b = 0xff;
			lightlevel=255;
		}
	}
	else 
	{
		gltexture = NULL;
		lightlevel = abs(lightlevel);
	}

	// get height from vplane
	if (whichplane == sector_t::floor && sector->transdoor) dz = -1;
	else dz = 0;

	z = plane.plane.ZatPoint(0.f, 0.f);
	
	PutFlat(fog);
	rendered_flats++;
}

//==========================================================================
//
// Sets 3D floor info. Common code for all 4 cases 
//
//==========================================================================

void GLFlat::SetFrom3DFloor(F3DFloor *rover, bool top, bool underside)
{
	F3DFloor::planeref & plane = top? rover->top : rover->bottom;

	// FF_FOG requires an inverted logic where to get the light from
	lightlist_t *light = P_GetPlaneLight(sector, plane.plane, underside);
	lightlevel = *light->p_lightlevel;
	
	if (rover->flags & FF_FOG) Colormap.LightColor = (light->extra_colormap)->Fade;
	else Colormap.CopyLightColor(light->extra_colormap);

	alpha = rover->alpha/255.0f;
	renderstyle = rover->flags&FF_ADDITIVETRANS? STYLE_Add : STYLE_Translucent;
	if (plane.model->VBOHeightcheck(plane.isceiling))
	{
		vboindex = plane.vindex;
	}
	else
	{
		vboindex = -1;
	}
}

//==========================================================================
//
// Process a sector's flats for rendering
// This function is only called once per sector.
// Subsequent subsectors are just quickly added to the ss_renderflags array
//
//==========================================================================

void GLFlat::ProcessSector(sector_t * frontsector)
{
	lightlist_t * light;

#ifdef _DEBUG
	if (frontsector->sectornum==0)
	{
		int a = 0;
	}
#endif

	// Get the real sector for this one.
	sector=&sectors[frontsector->sectornum];	
	extsector_t::xfloor &x = sector->e->XFloor;
	this->sub=NULL;

	byte &srf = gl_drawinfo->sectorrenderflags[sector->sectornum];

	//
	//
	//
	// do floors
	//
	//
	//
	if (frontsector->floorplane.ZatPoint(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy)) <= FIXED2FLOAT(viewz))
	{
		// process the original floor first.

		srf |= SSRF_RENDERFLOOR;

		lightlevel = gl_ClampLight(frontsector->GetFloorLight());
		Colormap=frontsector->ColorMap;
		if ((stack = (frontsector->portals[sector_t::floor] != NULL)))
		{
			gl_drawinfo->AddFloorStack(sector);
			alpha = frontsector->GetAlpha(sector_t::floor)/65536.0f;
		}
		else
		{
			alpha = 1.0f-frontsector->GetReflect(sector_t::floor);
		}
		if (frontsector->VBOHeightcheck(sector_t::floor))
		{
			vboindex = frontsector->vboindex[sector_t::floor];
		}
		else
		{
			vboindex = -1;
		}

		ceiling=false;
		renderflags=SSRF_RENDERFLOOR;

		if (x.ffloors.Size())
		{
			light = P_GetPlaneLight(sector, &frontsector->floorplane, false);
			if ((!(sector->GetFlags(sector_t::floor)&PLANEF_ABSLIGHTING) || light!=&x.lightlist[0])	
				&& (light->p_lightlevel != &frontsector->lightlevel))
			{
				lightlevel = *light->p_lightlevel;
			}

			Colormap.CopyLightColor(light->extra_colormap);
		}
		renderstyle = STYLE_Translucent;
		if (alpha!=0.0f) Process(frontsector, false, false);
	}
	
	//
	//
	//
	// do ceilings
	//
	//
	//
	if (frontsector->ceilingplane.ZatPoint(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy)) >= FIXED2FLOAT(viewz))
	{
		// process the original ceiling first.

		srf |= SSRF_RENDERCEILING;

		lightlevel = gl_ClampLight(frontsector->GetCeilingLight());
		Colormap=frontsector->ColorMap;
		if ((stack = (frontsector->portals[sector_t::ceiling] != NULL))) 
		{
			gl_drawinfo->AddCeilingStack(sector);
			alpha = frontsector->GetAlpha(sector_t::ceiling)/65536.0f;
		}
		else
		{
			alpha = 1.0f-frontsector->GetReflect(sector_t::ceiling);
		}

		if (frontsector->VBOHeightcheck(sector_t::ceiling))
		{
			vboindex = frontsector->vboindex[sector_t::ceiling];
		}
		else
		{
			vboindex = -1;
		}

		ceiling=true;
		renderflags=SSRF_RENDERCEILING;

		if (x.ffloors.Size())
		{
			light = P_GetPlaneLight(sector, &sector->ceilingplane, true);

			if ((!(sector->GetFlags(sector_t::ceiling)&PLANEF_ABSLIGHTING))
				&& (light->p_lightlevel != &frontsector->lightlevel))
			{
				lightlevel = *light->p_lightlevel;
			}
			Colormap.CopyLightColor(light->extra_colormap);
		}
		renderstyle = STYLE_Translucent;
		if (alpha!=0.0f) Process(frontsector, true, false);
	}

	//
	//
	//
	// do 3D floors
	//
	//
	//

	stack=false;
	if (x.ffloors.Size())
	{
		player_t * player=players[consoleplayer].camera->player;

		renderflags=SSRF_RENDER3DPLANES;
		srf |= SSRF_RENDER3DPLANES;
		// 3d-floors must not overlap!
		fixed_t lastceilingheight=sector->CenterCeiling();	// render only in the range of the
		fixed_t lastfloorheight=sector->CenterFloor();		// current sector part (if applicable)
		F3DFloor * rover;	
		int k;
		
		// floors are ordered now top to bottom so scanning the list for the best match
		// is no longer necessary.

		ceiling=true;
		for(k=0;k<(int)x.ffloors.Size();k++)
		{
			rover=x.ffloors[k];
			
			if ((rover->flags&(FF_EXISTS|FF_RENDERPLANES|FF_THISINSIDE))==(FF_EXISTS|FF_RENDERPLANES))
			{
				if (rover->flags&FF_FOG && gl_fixedcolormap) continue;
				if (rover->flags&(FF_INVERTPLANES|FF_BOTHPLANES))
				{
					fixed_t ff_top=rover->top.plane->ZatPoint(CenterSpot(sector));
					if (ff_top<lastceilingheight)
					{
						if (FIXED2FLOAT(viewz) <= rover->top.plane->ZatPoint(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy)))
						{
							SetFrom3DFloor(rover, true, !!(rover->flags&FF_FOG));
							Colormap.FadeColor=frontsector->ColorMap->Fade;
							Process(rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
						}
						lastceilingheight=ff_top;
					}
				}
				if (!(rover->flags&FF_INVERTPLANES))
				{
					fixed_t ff_bottom=rover->bottom.plane->ZatPoint(CenterSpot(sector));
					if (ff_bottom<lastceilingheight)
					{
						if (FIXED2FLOAT(viewz)<=rover->bottom.plane->ZatPoint(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy)))
						{
							SetFrom3DFloor(rover, false, !(rover->flags&FF_FOG));
							Colormap.FadeColor=frontsector->ColorMap->Fade;
							Process(rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
						}
						lastceilingheight=ff_bottom;
						if (rover->alpha<255) lastceilingheight++;
					}
				}
			}
		}
			  
		ceiling=false;
		for(k=x.ffloors.Size()-1;k>=0;k--)
		{
			rover=x.ffloors[k];
			
			if ((rover->flags&(FF_EXISTS|FF_RENDERPLANES|FF_THISINSIDE))==(FF_EXISTS|FF_RENDERPLANES))
			{
				if (rover->flags&FF_FOG && gl_fixedcolormap) continue;
				if (rover->flags&(FF_INVERTPLANES|FF_BOTHPLANES))
				{
					fixed_t ff_bottom=rover->bottom.plane->ZatPoint(CenterSpot(sector));
					if (ff_bottom>lastfloorheight || (rover->flags&FF_FIX))
					{
						if (FIXED2FLOAT(viewz) >= rover->bottom.plane->ZatPoint(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy)))
						{
							SetFrom3DFloor(rover, false, !(rover->flags&FF_FOG));
							Colormap.FadeColor=frontsector->ColorMap->Fade;

							if (rover->flags&FF_FIX)
							{
								lightlevel = gl_ClampLight(rover->model->lightlevel);
								Colormap = rover->GetColormap();
							}

							Process(rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
						}
						lastfloorheight=ff_bottom;
					}
				}
				if (!(rover->flags&FF_INVERTPLANES))
				{
					fixed_t ff_top=rover->top.plane->ZatPoint(CenterSpot(sector));
					if (ff_top>lastfloorheight)
					{
						if (FIXED2FLOAT(viewz) >= rover->top.plane->ZatPoint(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy)))
						{
							SetFrom3DFloor(rover, true, !!(rover->flags&FF_FOG));
							Colormap.FadeColor=frontsector->ColorMap->Fade;
							Process(rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
						}
						lastfloorheight=ff_top;
						if (rover->alpha<255) lastfloorheight--;
					}
				}
			}
		}
	}
}

