/*
** gl_decal.cpp
** OpenGL decal rendering code
**
**---------------------------------------------------------------------------
** Copyright 2003-2005 Christoph Oelckers
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

#include "doomdata.h"
#include "gl/system/gl_system.h"
#include "a_sharedglobal.h"
#include "r_utility.h"

#include "gl/system/gl_cvars.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"

struct DecalVertex
{
	float x,y,z;
	float u,v;
};

//==========================================================================
//
//
//
//==========================================================================
void GLWall::DrawDecal(DBaseDecal *decal)
{
	line_t * line=seg->linedef;
	side_t * side=seg->sidedef;
	int i;
	float zpos;
	int light;
	int rel;
	float a;
	bool flipx, flipy;
	DecalVertex dv[4];
	FTextureID decalTile;
	

	if (decal->RenderFlags & RF_INVISIBLE) return;
	if (type==RENDERWALL_FFBLOCK && gltexture->isMasked()) return;	// No decals on 3D floors with transparent textures.

	//if (decal->sprite != 0xffff)
	{
		decalTile = decal->PicNum;
		flipx = !!(decal->RenderFlags & RF_XFLIP);
		flipy = !!(decal->RenderFlags & RF_YFLIP);
	}
	/*
	else
	{
	decalTile = SpriteFrames[sprites[decal->sprite].spriteframes + decal->frame].lump[0];
	flipx = SpriteFrames[sprites[decal->sprite].spriteframes + decal->frame].flip & 1;
	}
	*/

	FTexture *texture = TexMan[decalTile];
	if (texture == NULL) return;

	FMaterial *tex;


	tex = FMaterial::ValidateTexture(texture, true);


	// the sectors are only used for their texture origin coordinates
	// so we don't need the fake sectors for deep water etc.
	// As this is a completely split wall fragment no further splits are
	// necessary for the decal.
	sector_t *frontsector;

	// for 3d-floor segments use the model sector as reference
	if ((decal->RenderFlags&RF_CLIPMASK)==RF_CLIPMID) frontsector=decal->Sector;
	else frontsector=seg->frontsector;

	switch (decal->RenderFlags & RF_RELMASK)
	{
	default:
		// No valid decal can have this type. If one is encountered anyway
		// it is in some way invalid so skip it.
		return;
		//zpos = decal->z;
		//break;

	case RF_RELUPPER:
		if (type!=RENDERWALL_TOP) return;
		if (line->flags & ML_DONTPEGTOP)
		{
			zpos = decal->Z + frontsector->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			zpos = decal->Z + seg->backsector->GetPlaneTexZ(sector_t::ceiling);
		}
		break;
	case RF_RELLOWER:
		if (type!=RENDERWALL_BOTTOM) return;
		if (line->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + frontsector->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			zpos = decal->Z + seg->backsector->GetPlaneTexZ(sector_t::floor);
		}
		break;
	case RF_RELMID:
		if (type==RENDERWALL_TOP || type==RENDERWALL_BOTTOM) return;
		if (line->flags & ML_DONTPEGBOTTOM)
		{
			zpos = decal->Z + frontsector->GetPlaneTexZ(sector_t::floor);
		}
		else
		{
			zpos = decal->Z + frontsector->GetPlaneTexZ(sector_t::ceiling);
		}
	}
	
	if (decal->RenderFlags & RF_FULLBRIGHT)
	{
		light = 255;
		rel = 0;
	}
	else
	{
		light = lightlevel;
		rel = rellight + getExtraLight();
	}
	
	FColormap p = Colormap;
	
	if (glset.nocoloredspritelighting)
	{
		p.Decolorize();
	}
	
	
	
	a = decal->Alpha;
	
	// now clip the decal to the actual polygon
	float decalwidth = tex->TextureWidth()  * decal->ScaleX;
	float decalheight= tex->TextureHeight() * decal->ScaleY;
	float decallefto = tex->GetLeftOffset() * decal->ScaleX;
	float decaltopo  = tex->GetTopOffset()  * decal->ScaleY;

	
	float leftedge = glseg.fracleft * side->TexelLength;
	float linelength = glseg.fracright * side->TexelLength - leftedge;

	// texel index of the decal's left edge
	float decalpixpos = (float)side->TexelLength * decal->LeftDistance - (flipx? decalwidth-decallefto : decallefto) - leftedge;

	float left,right;
	float lefttex,righttex;

	// decal is off the left edge
	if (decalpixpos < 0)
	{
		left = 0;
		lefttex = -decalpixpos;
	}
	else
	{
		left = decalpixpos;
		lefttex = 0;
	}
	
	// decal is off the right edge
	if (decalpixpos + decalwidth > linelength)
	{
		right = linelength;
		righttex = right - decalpixpos;
	}
	else
	{
		right = decalpixpos + decalwidth;
		righttex = decalwidth;
	}
	if (right<=left) return;	// nothing to draw

	// one texture unit on the wall as vector
	float vx=(glseg.x2-glseg.x1)/linelength;
	float vy=(glseg.y2-glseg.y1)/linelength;
		
	dv[1].x=dv[0].x=glseg.x1+vx*left;
	dv[1].y=dv[0].y=glseg.y1+vy*left;

	dv[3].x=dv[2].x=glseg.x1+vx*right;
	dv[3].y=dv[2].y=glseg.y1+vy*right;
		
	zpos+= (flipy? decalheight-decaltopo : decaltopo);

	dv[1].z=dv[2].z = zpos;
	dv[0].z=dv[3].z = dv[1].z - decalheight;
	dv[1].v=dv[2].v = tex->GetVT();

	dv[1].u=dv[0].u = tex->GetU(lefttex / decal->ScaleX);
	dv[3].u=dv[2].u = tex->GetU(righttex / decal->ScaleX);
	dv[0].v=dv[3].v = tex->GetVB();


	// now clip to the top plane
	float vzt=(ztop[1]-ztop[0])/linelength;
	float topleft=this->ztop[0]+vzt*left;
	float topright=this->ztop[0]+vzt*right;

	// completely below the wall
	if (topleft<dv[0].z && topright<dv[3].z) 
		return;

	if (topleft<dv[1].z || topright<dv[2].z)
	{
		// decal has to be clipped at the top
		// let texture clamping handle all extreme cases
		dv[1].v=(dv[1].z-topleft)/(dv[1].z-dv[0].z)*dv[0].v;
		dv[2].v=(dv[2].z-topright)/(dv[2].z-dv[3].z)*dv[3].v;
		dv[1].z=topleft;
		dv[2].z=topright;
	}

	// now clip to the bottom plane
	float vzb=(zbottom[1]-zbottom[0])/linelength;
	float bottomleft=this->zbottom[0]+vzb*left;
	float bottomright=this->zbottom[0]+vzb*right;

	// completely above the wall
	if (bottomleft>dv[1].z && bottomright>dv[2].z) 
		return;

	if (bottomleft>dv[0].z || bottomright>dv[3].z)
	{
		// decal has to be clipped at the bottom
		// let texture clamping handle all extreme cases
		dv[0].v=(dv[1].z-bottomleft)/(dv[1].z-dv[0].z)*(dv[0].v-dv[1].v) + dv[1].v;
		dv[3].v=(dv[2].z-bottomright)/(dv[2].z-dv[3].z)*(dv[3].v-dv[2].v) + dv[2].v;
		dv[0].z=bottomleft;
		dv[3].z=bottomright;
	}


	if (flipx)
	{
		float ur = tex->GetUR();
		for(i=0;i<4;i++) dv[i].u=ur-dv[i].u;
	}
	if (flipy)
	{
		float vb = tex->GetVB();
		for(i=0;i<4;i++) dv[i].v=vb-dv[i].v;
	}

	// calculate dynamic light effect.
	if (gl_lights && GLRenderer->mLightCount && !gl_fixedcolormap && gl_light_sprites)
	{
		// Note: This should be replaced with proper shader based lighting.
		double x, y;
		decal->GetXY(seg->sidedef, x, y);
		gl_SetDynSpriteLight(NULL, x, y, zpos, sub);
	}

	// alpha color only has an effect when using an alpha texture.
	if (decal->RenderStyle.Flags & STYLEF_RedIsAlpha)
	{
		gl_RenderState.SetObjectColor(decal->AlphaColor|0xff000000);
	}




	gl_SetRenderStyle(decal->RenderStyle, false, false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, decal->Translation, 0, !!(decal->RenderStyle.Flags & STYLEF_RedIsAlpha));


	// If srcalpha is one it looks better with a higher alpha threshold
	if (decal->RenderStyle.SrcAlpha == STYLEALPHA_One) gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
	else gl_RenderState.AlphaFunc(GL_GREATER, 0.f);


	gl_SetColor(light, rel, p, a);
	// for additively drawn decals we must temporarily set the fog color to black.
	PalEntry fc = gl_RenderState.GetFogColor();
	if (decal->RenderStyle.BlendOp == STYLEOP_Add && decal->RenderStyle.DestAlpha == STYLEALPHA_One)
	{
		gl_RenderState.SetFog(0,-1);
	}

	FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();
	for (i = 0; i < 4; i++)
	{
		ptr->Set(dv[i].x, dv[i].z, dv[i].y, dv[i].u, dv[i].v);
		ptr++;
	}

	if (lightlist == NULL)
	{
		gl_RenderState.Apply();
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_FAN);
	}
	else
	{
		unsigned int offset;
		unsigned int count = GLRenderer->mVBO->GetCount(ptr, &offset);
		for (unsigned k = 0; k < lightlist->Size(); k++)
		{
			secplane_t &lowplane = k == (*lightlist).Size() - 1 ? bottomplane : (*lightlist)[k + 1].plane;

			float low1 = lowplane.ZatPoint(dv[1].x, dv[1].y);
			float low2 = lowplane.ZatPoint(dv[2].x, dv[2].y);

			if (low1 < dv[1].z || low2 < dv[2].z)
			{
				int thisll = (*lightlist)[k].caster != NULL ? gl_ClampLight(*(*lightlist)[k].p_lightlevel) : lightlevel;
				FColormap thiscm;
				thiscm.FadeColor = Colormap.FadeColor;
				thiscm.CopyFrom3DLight(&(*lightlist)[k]);
				gl_SetColor(thisll, rel, thiscm, a);
				if (glset.nocoloredspritelighting) thiscm.Decolorize();
				gl_SetFog(thisll, rel, &thiscm, RenderStyle == STYLE_Add);
				gl_RenderState.SetSplitPlanes((*lightlist)[k].plane, lowplane);

				gl_RenderState.Apply();
				GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, offset, count);
			}
			if (low1 <= dv[0].z && low2 <= dv[3].z) break;
		}
	}

	rendered_decals++;
	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetFog(fc,-1);
	gl_RenderState.SetDynLight(0,0,0);
}

//==========================================================================
//
//
//
//==========================================================================
void GLWall::DoDrawDecals()
{
	if (seg->sidedef && seg->sidedef->AttachedDecals)
	{
		if (lightlist != NULL)
		{
			gl_RenderState.EnableSplit(true);
		}
		else
		{
			gl_SetFog(lightlevel, rellight + getExtraLight(), &Colormap, false);
		}

		DBaseDecal *decal = seg->sidedef->AttachedDecals;
		while (decal)
		{
			DrawDecal(decal);
			decal = decal->WallNext;
		}

		if (lightlist != NULL)
		{
			gl_RenderState.EnableSplit(false);
		}

	}
}

