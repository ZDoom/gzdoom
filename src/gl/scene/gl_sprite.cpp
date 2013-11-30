/*
** gl_sprite.cpp
** Sprite/Particle rendering
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
#include "p_effect.h"
#include "g_level.h"
#include "doomstat.h"
#include "gl/gl_functions.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/models/gl_models.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"

CVAR(Bool, gl_usecolorblending, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, gl_spritebrightfog, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Bool, gl_sprite_blend, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Int, gl_spriteclip, 1, CVAR_ARCHIVE)
CVAR(Float, gl_sclipthreshold, 10.0, CVAR_ARCHIVE)
CVAR(Float, gl_sclipfactor, 1.8, CVAR_ARCHIVE)
CVAR(Int, gl_particles_style, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // 0 = square, 1 = round, 2 = smooth
CVAR(Int, gl_billboard_mode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, gl_billboard_particles, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, gl_enhanced_nv_stealth, 3, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Int, gl_fuzztype, 0, CVAR_ARCHIVE)
{
	if (self < 0 || self > 7) self = 0;
}

extern bool r_showviewer;
EXTERN_CVAR (Float, transsouls)

extern TArray<spritedef_t> sprites;
extern TArray<spriteframe_t> SpriteFrames;
extern TArray<PalEntry> BloodTranslationColors;

enum HWRenderStyle
{
	STYLEHW_Normal,			// default
	STYLEHW_Solid,			// drawn solid (needs special treatment for sprites)
	STYLEHW_NoAlphaTest,	// disable alpha test
};


void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending)
{
	int tm, sb, db, be;

	gl_GetRenderStyle(style, drawopaque, allowcolorblending, &tm, &sb, &db, &be);
	gl_RenderState.BlendEquation(be);
	gl_RenderState.BlendFunc(sb, db);
	gl_RenderState.SetTextureMode(tm);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLSprite::Draw(int pass)
{
	if (pass!=GLPASS_PLAIN && pass != GLPASS_ALL && pass!=GLPASS_TRANSLUCENT) return;

	// Hack to enable bright sprites in faded maps
	uint32 backupfade = Colormap.FadeColor.d;
	if (gl_spritebrightfog && fullbright)
		Colormap.FadeColor = 0;


	bool additivefog = false;
	int rel = getExtraLight();

	if (pass==GLPASS_TRANSLUCENT)
	{
		// The translucent pass requires special setup for the various modes.

		// Brightmaps will only be used when doing regular drawing ops and having no fog
		if (!gl_spritebrightfog && (!gl_isBlack(Colormap.FadeColor) || level.flags&LEVEL_HASFADETABLE || 
			RenderStyle.BlendOp != STYLEOP_Add))
		{
			gl_RenderState.EnableBrightmap(false);
		}

		gl_SetRenderStyle(RenderStyle, false, 
			// The rest of the needed checks are done inside gl_SetRenderStyle
			trans > 1.f - FLT_EPSILON && gl_usecolorblending && gl_fixedcolormap < CM_FIRSTSPECIALCOLORMAP && actor && 
			fullbright && gltexture && !gltexture->GetTransparent());

		if (hw_styleflags == STYLEHW_NoAlphaTest)
		{
			gl_RenderState.EnableAlphaTest(false);
		}
		else
		{
			gl_RenderState.AlphaFunc(GL_GEQUAL,trans*gl_mask_sprite_threshold);
		}

		if (RenderStyle.BlendOp == STYLEOP_Shadow)
		{
			float fuzzalpha=0.44f;
			float minalpha=0.1f;

			// fog + fuzz don't work well without some fiddling with the alpha value!
			if (!gl_isBlack(Colormap.FadeColor))
			{
				float xcamera=FIXED2FLOAT(viewx);
				float ycamera=FIXED2FLOAT(viewy);

				float dist=Dist2(xcamera,ycamera, x,y);

				if (!Colormap.FadeColor.a) Colormap.FadeColor.a=clamp<int>(255-lightlevel,60,255);

				// this value was determined by trial and error and is scale dependent!
				float factor=0.05f+exp(-Colormap.FadeColor.a*dist/62500.f);
				fuzzalpha*=factor;
				minalpha*=factor;
			}

			gl_RenderState.AlphaFunc(GL_GEQUAL,minalpha*gl_mask_sprite_threshold);
			glColor4f(0.2f,0.2f,0.2f,fuzzalpha);
			additivefog = true;
		}
		else if (RenderStyle.BlendOp == STYLEOP_Add && RenderStyle.DestAlpha == STYLEALPHA_One)
		{
			additivefog = true;
		}
	}
	if (RenderStyle.BlendOp!=STYLEOP_Shadow)
	{
		if (actor)
		{
			lightlevel = gl_SetSpriteLighting(RenderStyle, actor, lightlevel, rel, &Colormap, ThingColor, trans,
							 fullbright || gl_fixedcolormap >= CM_FIRSTSPECIALCOLORMAP, false);
		}
		else if (particle)
		{
			if (gl_light_particles)
			{
				lightlevel = gl_SetSpriteLight(particle, lightlevel, rel, &Colormap, trans, ThingColor);
			}
			else 
			{
				gl_SetColor(lightlevel, rel, &Colormap, trans, ThingColor);
			}
		}
		else return;
	}

	if (gl_isBlack(Colormap.FadeColor)) foglevel=lightlevel;

	if (RenderStyle.Flags & STYLEF_FadeToBlack) 
	{
		Colormap.FadeColor=0;
		additivefog = true;
	}

	if (RenderStyle.Flags & STYLEF_InvertOverlay) 
	{
		Colormap.FadeColor = Colormap.FadeColor.InverseColor();
		additivefog=false;
	}

	gl_SetFog(foglevel, rel, &Colormap, additivefog);

	if (gltexture) gltexture->BindPatch(Colormap.colormap, translation, OverrideShader);
	else if (!modelframe) gl_RenderState.EnableTexture(false);

	if (!modelframe)
	{
		// [BB] Billboard stuff
		const bool drawWithXYBillboard = ( (particle && gl_billboard_particles) || (!(actor && actor->renderflags & RF_FORCEYBILLBOARD)
		                                   //&& GLRenderer->mViewActor != NULL
		                                   && (gl_billboard_mode == 1 || (actor && actor->renderflags & RF_FORCEXYBILLBOARD ))) );
		gl_RenderState.Apply();
		glBegin(GL_TRIANGLE_STRIP);
		if ( drawWithXYBillboard )
		{
			// Rotate the sprite about the vector starting at the center of the sprite
			// triangle strip and with direction orthogonal to where the player is looking
			// in the x/y plane.
			float xcenter = (x1+x2)*0.5;
			float ycenter = (y1+y2)*0.5;
			float zcenter = (z1+z2)*0.5;
			float angleRad = DEG2RAD(270. - float(GLRenderer->mAngles.Yaw));
			
			Matrix3x4 mat;
			mat.MakeIdentity();
			mat.Translate( xcenter, zcenter, ycenter);
			mat.Rotate(-sin(angleRad), 0, cos(angleRad), -GLRenderer->mAngles.Pitch);
			mat.Translate( -xcenter, -zcenter, -ycenter);
			Vector v1 = mat * Vector(x1,z1,y1);
			Vector v2 = mat * Vector(x2,z1,y2);
			Vector v3 = mat * Vector(x1,z2,y1);
			Vector v4 = mat * Vector(x2,z2,y2);

			if (gltexture)
			{
				glTexCoord2f(ul, vt); glVertex3fv(&v1[0]);
				glTexCoord2f(ur, vt); glVertex3fv(&v2[0]);
				glTexCoord2f(ul, vb); glVertex3fv(&v3[0]);
				glTexCoord2f(ur, vb); glVertex3fv(&v4[0]);
			}
			else	// Particle
			{
				glVertex3fv(&v1[0]);
				glVertex3fv(&v2[0]);
				glVertex3fv(&v3[0]);
				glVertex3fv(&v4[0]);
			}

		}
		else
		{
			if (gltexture)
			{
				glTexCoord2f(ul, vt); glVertex3f(x1, z1, y1);
				glTexCoord2f(ur, vt); glVertex3f(x2, z1, y2);
				glTexCoord2f(ul, vb); glVertex3f(x1, z2, y1);
				glTexCoord2f(ur, vb); glVertex3f(x2, z2, y2);
			}
			else	// Particle
			{
				glVertex3f(x1, z1, y1);
				glVertex3f(x2, z1, y2);
				glVertex3f(x1, z2, y1);
				glVertex3f(x2, z2, y2);
			}
		}
		glEnd();
	}
	else
	{
		gl_RenderModel(this, Colormap.colormap);
	}

	if (pass==GLPASS_TRANSLUCENT)
	{
		gl_RenderState.EnableBrightmap(true);
		gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl_RenderState.BlendEquation(GL_FUNC_ADD);
		gl_RenderState.SetTextureMode(TM_MODULATE);

		// [BB] Restore the alpha test after drawing a smooth particle.
		if (hw_styleflags == STYLEHW_NoAlphaTest)
		{
			gl_RenderState.EnableAlphaTest(true);
		}
		else
		{
			gl_RenderState.AlphaFunc(GL_GEQUAL,gl_mask_sprite_threshold);
		}
	}

	// End of gl_sprite_brightfog hack: restore FadeColor to normalcy
	if (backupfade != Colormap.FadeColor.d)
		Colormap.FadeColor = backupfade;

	gl_RenderState.EnableTexture(true);
	gl_RenderState.SetDynLight(0,0,0);
}


//==========================================================================
//
// 
//
//==========================================================================
inline void GLSprite::PutSprite(bool translucent)
{
	int list;
	// [BB] Allow models to be drawn in the GLDL_TRANSLUCENT pass.
	if (translucent || !modelframe)
	{
		list = GLDL_TRANSLUCENT;
	}
	else if ((!gl_isBlack (Colormap.FadeColor) || level.flags&LEVEL_HASFADETABLE))
	{
		list = GLDL_FOGMASKED;
	}
	else
	{
		list = GLDL_MASKED;
	}
	gl_drawinfo->drawlists[list].AddSprite(this);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLSprite::SplitSprite(sector_t * frontsector, bool translucent)
{
	GLSprite copySprite;
	fixed_t lightbottom;
	float maplightbottom;
	unsigned int i;
	bool put=false;
	TArray<lightlist_t> & lightlist=frontsector->e->XFloor.lightlist;

	for(i=0;i<lightlist.Size();i++)
	{
		// Particles don't go through here so we can safely assume that actor is not NULL
		if (i<lightlist.Size()-1) lightbottom=lightlist[i+1].plane.ZatPoint(actor->x,actor->y);
		else lightbottom=frontsector->floorplane.ZatPoint(actor->x,actor->y);

		maplightbottom=FIXED2FLOAT(lightbottom);
		if (maplightbottom<z2) maplightbottom=z2;

		if (maplightbottom<z1)
		{
			copySprite=*this;
			copySprite.lightlevel = gl_ClampLight(*lightlist[i].p_lightlevel);
			copySprite.Colormap.CopyLightColor(lightlist[i].extra_colormap);

			if (glset.nocoloredspritelighting)
			{
				int v = (copySprite.Colormap.LightColor.r + copySprite.Colormap.LightColor.g + copySprite.Colormap.LightColor.b )/3;
				copySprite.Colormap.LightColor.r=
				copySprite.Colormap.LightColor.g=
				copySprite.Colormap.LightColor.b=(255+v+v)/3;
			}

			if (!gl_isWhite(ThingColor))
			{
				copySprite.Colormap.LightColor.r=(copySprite.Colormap.LightColor.r*ThingColor.r)>>8;
				copySprite.Colormap.LightColor.g=(copySprite.Colormap.LightColor.g*ThingColor.g)>>8;
				copySprite.Colormap.LightColor.b=(copySprite.Colormap.LightColor.b*ThingColor.b)>>8;
			}

			z1=copySprite.z2=maplightbottom;
			vt=copySprite.vb=copySprite.vt+ 
				(maplightbottom-copySprite.z1)*(copySprite.vb-copySprite.vt)/(z2-copySprite.z1);
			copySprite.PutSprite(translucent);
			put=true;
		}
	}
}


void GLSprite::SetSpriteColor(sector_t *sector, fixed_t center_y)
{
	fixed_t lightbottom;
	float maplightbottom;
	unsigned int i;
	TArray<lightlist_t> & lightlist=actor->Sector->e->XFloor.lightlist;

	for(i=0;i<lightlist.Size();i++)
	{
		// Particles don't go through here so we can safely assume that actor is not NULL
		if (i<lightlist.Size()-1) lightbottom=lightlist[i+1].plane.ZatPoint(actor->x,actor->y);
		else lightbottom=sector->floorplane.ZatPoint(actor->x,actor->y);

		//maplighttop=FIXED2FLOAT(lightlist[i].height);
		maplightbottom=FIXED2FLOAT(lightbottom);
		if (maplightbottom<z2) maplightbottom=z2;

		if (maplightbottom<center_y)
		{
			lightlevel=*lightlist[i].p_lightlevel;
			Colormap.CopyLightColor(lightlist[i].extra_colormap);

			if (glset.nocoloredspritelighting)
			{
				int v = (Colormap.LightColor.r + Colormap.LightColor.g + Colormap.LightColor.b )/3;
				Colormap.LightColor.r=
				Colormap.LightColor.g=
				Colormap.LightColor.b=(255+v+v)/3;
			}

			if (!gl_isWhite(ThingColor))
			{
				Colormap.LightColor.r=(Colormap.LightColor.r*ThingColor.r)>>8;
				Colormap.LightColor.g=(Colormap.LightColor.g*ThingColor.g)>>8;
				Colormap.LightColor.b=(Colormap.LightColor.b*ThingColor.b)>>8;
			}
			return;
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void GLSprite::Process(AActor* thing,sector_t * sector)
{
	sector_t rs;
	sector_t * rendersector;
	// don't draw the thing that's used as camera (for viewshifts during quakes!)
	if (thing==GLRenderer->mViewActor) return;

	// Don't waste time projecting sprites that are definitely not visible.
	if (thing == NULL || thing->sprite == 0 || !thing->IsVisibleToPlayer())
	{
		return;
	}

	int spritenum = thing->sprite;
	fixed_t spritescaleX = thing->scaleX;
	fixed_t spritescaleY = thing->scaleY;
	if (thing->player != NULL)
	{
		P_CheckPlayerSprite(thing, spritenum, spritescaleX, spritescaleY);
	}

	if (thing->renderflags & RF_INVISIBLE || !thing->RenderStyle.IsVisible(thing->alpha)) 
	{
		if (!(thing->flags & MF_STEALTH) || !gl_fixedcolormap || !gl_enhanced_nightvision)
			return; 
	}

	// If this thing is in a map section that's not in view it can't possibly be visible
	if (!(currentmapsection[thing->subsector->mapsection>>3] & (1 << (thing->subsector->mapsection & 7)))) return;

	// [RH] Interpolate the sprite's position to make it look smooth
	fixed_t thingx = thing->PrevX + FixedMul (r_TicFrac, thing->x - thing->PrevX);
	fixed_t thingy = thing->PrevY + FixedMul (r_TicFrac, thing->y - thing->PrevY);
	fixed_t thingz = thing->PrevZ + FixedMul (r_TicFrac, thing->z - thing->PrevZ);

	// Too close to the camera. This doesn't look good if it is a sprite.
	if (P_AproxDistance(thingx-viewx, thingy-viewy)<2*FRACUNIT)
	{
		// exclude vertically moving objects from this check.
		if (!(thing->velx==0 && thing->vely==0 && thing->velz!=0))
		{
			if (!gl_FindModelFrame(RUNTIME_TYPE(thing), spritenum, thing->frame, false))
			{
				return;
			}
		}
	}

	// don't draw first frame of a player missile
	if (thing->flags&MF_MISSILE && thing->target==GLRenderer->mViewActor && GLRenderer->mViewActor != NULL)
	{
		if (P_AproxDistance(thingx-viewx, thingy-viewy) < thing->Speed ) return;
	}

	if (GLRenderer->mCurrentPortal)
	{
		int clipres = GLRenderer->mCurrentPortal->ClipPoint(thingx, thingy);
		if (clipres == GLPortal::PClip_InFront) return;
	}

	player_t *player=&players[consoleplayer];
	FloatRect r;

	if (sector->sectornum!=thing->Sector->sectornum)
	{
		rendersector=gl_FakeFlat(thing->Sector, &rs, false);
	}
	else
	{
		rendersector=sector;
	}
	

	x = FIXED2FLOAT(thingx);
	z = FIXED2FLOAT(thingz-thing->floorclip);
	y = FIXED2FLOAT(thingy);

	// [RH] Make floatbobbing a renderer-only effect.
	if (thing->flags2 & MF2_FLOATBOB)
	{
		float fz = FIXED2FLOAT(thing->GetBobOffset(r_TicFrac));
		z += fz;
	}
	
	modelframe = gl_FindModelFrame(RUNTIME_TYPE(thing), spritenum, thing->frame, !!(thing->flags & MF_DROPPED));
	if (!modelframe)
	{
		angle_t ang = R_PointToAngle(thingx, thingy);

		bool mirror;
		FTextureID patch = gl_GetSpriteFrame(spritenum, thing->frame, -1, ang - thing->angle, &mirror);
		if (!patch.isValid()) return;
		gltexture=FMaterial::ValidateTexture(patch, false);
		if (!gltexture) return;

		if (gl.flags & RFL_NPOT_TEXTURE)	// trimming only works if non-power-of-2 textures are supported
		{
			vt = gltexture->GetSpriteVT();
			vb = gltexture->GetSpriteVB();
			gltexture->GetRect(&r, GLUSE_SPRITE);
			if (mirror)
			{
				r.left=-r.width-r.left;	// mirror the sprite's x-offset
				ul = gltexture->GetSpriteUL();
				ur = gltexture->GetSpriteUR();
			}
			else
			{
				ul = gltexture->GetSpriteUR();
				ur = gltexture->GetSpriteUL();
			}
		}
		else
		{
			vt = gltexture->GetVT();
			vb = gltexture->GetVB();
			gltexture->GetRect(&r, GLUSE_PATCH);
			if (mirror)
			{
				r.left=-r.width-r.left;	// mirror the sprite's x-offset
				ul = gltexture->GetUL();
				ur = gltexture->GetUR();
			}
			else
			{
				ul = gltexture->GetUR();
				ur = gltexture->GetUL();
			}
		}

		r.Scale(FIXED2FLOAT(spritescaleX),FIXED2FLOAT(spritescaleY));

		float rightfac=-r.left;
		float leftfac=rightfac-r.width;

		z1=z-r.top;
		z2=z1-r.height;

		float spriteheight = FIXED2FLOAT(spritescaleY) * gltexture->GetScaledHeightFloat(GLUSE_SPRITE);
		
		// Tests show that this doesn't look good for many decorations and corpses
		if (spriteheight>0 && gl_spriteclip>0)
		{
			bool smarterclip = false; // Set to true if one condition triggers the test below
			if (((thing->player || thing->flags3&MF3_ISMONSTER ||
				thing->IsKindOf(RUNTIME_CLASS(AInventory)))	&& (thing->flags&MF_ICECORPSE ||
				!(thing->flags&MF_CORPSE))) || (gl_spriteclip==3 && (smarterclip = true)) || gl_spriteclip > 1)
			{
				float btm= 1000000.0f;
				float top=-1000000.0f;
				extsector_t::xfloor &x = thing->Sector->e->XFloor;

				if (x.ffloors.Size())
				{
					for(unsigned int i=0;i<x.ffloors.Size();i++)
					{
						F3DFloor * ff=x.ffloors[i];
						fixed_t floorh=ff->top.plane->ZatPoint(thingx, thingy);
						fixed_t ceilingh=ff->bottom.plane->ZatPoint(thingx, thingy);
						if (floorh==thing->floorz) 
						{
							btm=FIXED2FLOAT(floorh);
						}
						if (ceilingh==thing->ceilingz) 
						{
							top=FIXED2FLOAT(ceilingh);
						}
						if (btm != 1000000.0f && top != -1000000.0f)
						{
							break;
						}
					}
				}
				else if (thing->Sector->heightsec && !(thing->Sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
				{
					if (thing->flags2&MF2_ONMOBJ && thing->floorz==
						thing->Sector->heightsec->floorplane.ZatPoint(thingx, thingy))
					{
						btm=FIXED2FLOAT(thing->floorz);
						top=FIXED2FLOAT(thing->ceilingz);
					}
				}
				if (btm==1000000.0f) 
					btm= FIXED2FLOAT(thing->Sector->floorplane.ZatPoint(thingx, thingy)-thing->floorclip);
				if (top==-1000000.0f)
					top= FIXED2FLOAT(thing->Sector->ceilingplane.ZatPoint(thingx, thingy));

				float diffb = z2 - btm;
				float difft = z1 - top;
				if (diffb >= 0 /*|| !gl_sprite_clip_to_floor*/) diffb = 0;
				// Adjust sprites clipping into ceiling and adjust clipping adjustment for tall graphics
				if (smarterclip)
				{
					// Reduce slightly clipping adjustment of corpses
					if (thing->flags & MF_CORPSE || spriteheight > abs(diffb))
					{
						float ratio = clamp<float>((abs(diffb) * (float)gl_sclipfactor/(spriteheight+1)), 0.5, 1.0);
						diffb*=ratio;
					}
					if (!diffb)
					{
						if (difft <= 0) difft = 0;
						if (difft >= (float)gl_sclipthreshold) 
						{
							// dumb copy of the above.
							if (!(thing->flags3&MF3_ISMONSTER) || (thing->flags&MF_NOGRAVITY) || (thing->flags&MF_CORPSE) || difft > (float)gl_sclipthreshold)
							{
								difft=0;
							}
						}
						if (spriteheight > abs(difft))
						{
							float ratio = clamp<float>((abs(difft) * (float)gl_sclipfactor/(spriteheight+1)), 0.5, 1.0);
							difft*=ratio;
						}
						z2-=difft;
						z1-=difft;
					}
				}
				if (diffb <= (0 - (float)gl_sclipthreshold))	// such a large displacement can't be correct! 
				{
					// for living monsters standing on the floor allow a little more.
					if (!(thing->flags3&MF3_ISMONSTER) || (thing->flags&MF_NOGRAVITY) || (thing->flags&MF_CORPSE) || diffb<(-1.8*(float)gl_sclipthreshold))
					{
						diffb=0;
					}
				}
				z2-=diffb;
				z1-=diffb;
			}
		}
		float viewvecX = GLRenderer->mViewVector.X;
		float viewvecY = GLRenderer->mViewVector.Y;

		x1=x-viewvecY*leftfac;
		x2=x-viewvecY*rightfac;
		y1=y+viewvecX*leftfac;
		y2=y+viewvecX*rightfac;		
	}
	else 
	{
		x1 = x2 = x;
		y1 = y2 = y;
		z1 = z2 = z;
		gltexture=NULL;
	}

	depth = DMulScale20 (thing->x-viewx, viewtancos, thing->y-viewy, viewtansin);

	// light calculation

	bool enhancedvision=false;

	// allow disabling of the fullbright flag by a brightmap definition
	// (e.g. to do the gun flashes of Doom's zombies correctly.
	fullbright = (thing->flags5 & MF5_BRIGHT) ||
		((thing->renderflags & RF_FULLBRIGHT) && (!gl_BrightmapsActive() || !gltexture || !gltexture->tex->gl_info.bBrightmapDisablesFullbright));

	lightlevel=fullbright? 255 : 
		gl_ClampLight(rendersector->GetTexture(sector_t::ceiling) == skyflatnum ? 
			rendersector->GetCeilingLight() : rendersector->GetFloorLight());
	foglevel = (BYTE)clamp<short>(rendersector->lightlevel, 0, 255);

	lightlevel = (byte)gl_CheckSpriteGlow(rendersector, lightlevel, thingx, thingy, thingz);

	// colormap stuff is a little more complicated here...
	if (gl_fixedcolormap) 
	{
		if ((gl_enhanced_nv_stealth > 0 && gl_fixedcolormap == CM_LITE)		// Infrared powerup only
			|| (gl_enhanced_nv_stealth == 2 && gl_fixedcolormap >= CM_TORCH)// Also torches
			|| (gl_enhanced_nv_stealth == 3))								// Any fixed colormap
			enhancedvision=true;

		Colormap.GetFixedColormap();

		if (gl_fixedcolormap==CM_LITE)
		{
			if (gl_enhanced_nightvision &&
				(thing->IsKindOf(RUNTIME_CLASS(AInventory)) || thing->flags3&MF3_ISMONSTER || thing->flags&MF_MISSILE || thing->flags&MF_CORPSE))
			{
				Colormap.colormap = CM_FIRSTSPECIALCOLORMAP + INVERSECOLORMAP;
			}
		}
	}
	else 
	{
		Colormap=rendersector->ColorMap;
		if (fullbright)
		{
			if (rendersector == &sectors[rendersector->sectornum] || in_area != area_below)	
				// under water areas keep their color for fullbright objects
			{
				// Only make the light white but keep everything else (fog, desaturation and Boom colormap.)
				Colormap.LightColor.r=
				Colormap.LightColor.g=
				Colormap.LightColor.b=0xff;
			}
			else
			{
				Colormap.LightColor.r = (3*Colormap.LightColor.r + 0xff)/4;
				Colormap.LightColor.g = (3*Colormap.LightColor.g + 0xff)/4;
				Colormap.LightColor.b = (3*Colormap.LightColor.b + 0xff)/4;
			}
		}
		else if (glset.nocoloredspritelighting)
		{
			int v = (Colormap.LightColor.r /* * 77 */ + Colormap.LightColor.g /**143 */ + Colormap.LightColor.b /**35*/)/3;//255;
			Colormap.LightColor.r=
			Colormap.LightColor.g=
			Colormap.LightColor.b=(255+v+v)/3;
		}
	}

	translation=thing->Translation;

	ThingColor=0xffffff;
	RenderStyle = thing->RenderStyle;
	OverrideShader = 0;
	trans = FIXED2FLOAT(thing->alpha);
	hw_styleflags = STYLEHW_Normal;

	if (RenderStyle.BlendOp >= STYLEOP_Fuzz && RenderStyle.BlendOp <= STYLEOP_FuzzOrRevSub)
	{
		RenderStyle.CheckFuzz();
		if (RenderStyle.BlendOp == STYLEOP_Fuzz)
		{
			if (gl.shadermodel >= 4 && gl_fuzztype != 0)
			{
				// Todo: implement shader selection here
				RenderStyle = LegacyRenderStyles[STYLE_Translucent];
				OverrideShader = gl_fuzztype + 4;
				trans = 0.99f;	// trans may not be 1 here
				hw_styleflags |= STYLEHW_NoAlphaTest;
			}
			else
			{
				RenderStyle.BlendOp = STYLEOP_Shadow;
			}
		}
	}

	if (RenderStyle.Flags & STYLEF_TransSoulsAlpha)
	{
		trans = transsouls;
	}
	else if (RenderStyle.Flags & STYLEF_Alpha1)
	{
		trans = 1.f;
	}

	if (trans >= 1.f-FLT_EPSILON && RenderStyle.BlendOp != STYLEOP_Shadow && (
			(RenderStyle.SrcAlpha == STYLEALPHA_One && RenderStyle.DestAlpha == STYLEALPHA_Zero) ||
			(RenderStyle.SrcAlpha == STYLEALPHA_Src && RenderStyle.DestAlpha == STYLEALPHA_InvSrc)
			))
	{
		// This is a non-translucent sprite (i.e. STYLE_Normal or equivalent)
		trans=1.f;
		

		if (!gl_sprite_blend || modelframe)
		{
			RenderStyle.SrcAlpha = STYLEALPHA_One;
			RenderStyle.DestAlpha = STYLEALPHA_Zero;
			hw_styleflags = STYLEHW_Solid;
		}
		else
		{
			RenderStyle.SrcAlpha = STYLEALPHA_Src;
			RenderStyle.DestAlpha = STYLEALPHA_InvSrc;
		}


	}
	if ((gltexture && gltexture->GetTransparent()) || (RenderStyle.Flags & STYLEF_RedIsAlpha))
	{
		if (hw_styleflags == STYLEHW_Solid)
		{
			RenderStyle.SrcAlpha = STYLEALPHA_Src;
			RenderStyle.DestAlpha = STYLEALPHA_InvSrc;
		}
		hw_styleflags = STYLEHW_NoAlphaTest;
 	}

	if (enhancedvision && gl_enhanced_nightvision)
	{
		if (RenderStyle.BlendOp == STYLEOP_Shadow)
		{
			// enhanced vision makes them more visible!
			trans=0.5f;
			RenderStyle = STYLE_Translucent;
		}
		else if (thing->flags & MF_STEALTH)	
		{
			// enhanced vision overcomes stealth!
			if (trans < 0.5f) trans = 0.5f;
		}
	}

	if (trans==0.0f) return;

	// end of light calculation

	actor=thing;
	index = GLRenderer->gl_spriteindex++;
	particle=NULL;
	
	const bool drawWithXYBillboard = ( !(actor->renderflags & RF_FORCEYBILLBOARD)
									   && players[consoleplayer].camera
									   && (gl_billboard_mode == 1 || actor->renderflags & RF_FORCEXYBILLBOARD ) );


	if (drawWithXYBillboard || modelframe)
	{
		if (!gl_fixedcolormap && !fullbright) SetSpriteColor(actor->Sector, actor->y + (actor->height>>1));
		PutSprite(hw_styleflags != STYLEHW_Solid);
	}
	else if (thing->Sector->e->XFloor.lightlist.Size()==0 || gl_fixedcolormap || fullbright) 
	{
		PutSprite(hw_styleflags != STYLEHW_Solid);
	}
	else
	{
		SplitSprite(thing->Sector, hw_styleflags != STYLEHW_Solid);
	}
	rendered_sprites++;
}


//==========================================================================
//
// 
//
//==========================================================================

void GLSprite::ProcessParticle (particle_t *particle, sector_t *sector)//, int shade, int fakeside)
{
	if (GLRenderer->mCurrentPortal)
	{
		int clipres = GLRenderer->mCurrentPortal->ClipPoint(particle->x, particle->y);
		if (clipres == GLPortal::PClip_InFront) return;
	}

	player_t *player=&players[consoleplayer];
	
	if (particle->trans==0) return;

	lightlevel = gl_ClampLight(sector->GetTexture(sector_t::ceiling) == skyflatnum ? 
		sector->GetCeilingLight() : sector->GetFloorLight());
	foglevel = sector->lightlevel;

	if (gl_fixedcolormap) 
	{
		Colormap.GetFixedColormap();
	}
	else if (!particle->bright)
	{
		TArray<lightlist_t> & lightlist=sector->e->XFloor.lightlist;
		int lightbottom;

		Colormap = sector->ColorMap;
		for(unsigned int i=0;i<lightlist.Size();i++)
		{
			if (i<lightlist.Size()-1) lightbottom = lightlist[i+1].plane.ZatPoint(particle->x,particle->y);
			else lightbottom = sector->floorplane.ZatPoint(particle->x,particle->y);

			if (lightbottom < particle->y)
			{
				lightlevel = *lightlist[i].p_lightlevel;
				Colormap.LightColor = (lightlist[i].extra_colormap)->Color;
				break;
			}
		}
	}
	else
	{
		lightlevel = 255;
		Colormap = sector->ColorMap;
		Colormap.ClearColor();
	}

	trans=particle->trans/255.0f;
	RenderStyle = STYLE_Translucent;
	OverrideShader = 0;

	ThingColor = particle->color;
	gl_ModifyColor(ThingColor.r, ThingColor.g, ThingColor.b, Colormap.colormap);
	ThingColor.a=0;

	modelframe=NULL;
	gltexture=NULL;

	// [BB] Load the texture for round or smooth particles
	if (gl_particles_style)
	{
		FTexture *lump = NULL;
		if (gl_particles_style == 1)
		{
			lump = GLRenderer->glpart2;
		}
		else if (gl_particles_style == 2)
		{
			lump = GLRenderer->glpart;
		}

		if (lump != NULL)
		{
			gltexture=FMaterial::ValidateTexture(lump);
			translation = 0;

			ul = gltexture->GetUL();
			ur = gltexture->GetUR();
			vt = gltexture->GetVT();
			vb = gltexture->GetVB();
			FloatRect r;
			gltexture->GetRect(&r, GLUSE_PATCH);
		}
	}

	x= FIXED2FLOAT(particle->x);
	y= FIXED2FLOAT(particle->y);
	z= FIXED2FLOAT(particle->z);
	
	float scalefac=particle->size/4.0f;
	// [BB] The smooth particles are smaller than the other ones. Compensate for this here.
	if (gl_particles_style==2)
		scalefac *= 1.7;

	float viewvecX = GLRenderer->mViewVector.X;
	float viewvecY = GLRenderer->mViewVector.Y;

	x1=x+viewvecY*scalefac;
	x2=x-viewvecY*scalefac;
	y1=y-viewvecX*scalefac;
	y2=y+viewvecX*scalefac;
	z1=z-scalefac;
	z2=z+scalefac;

	depth = DMulScale20 (particle->x-viewx, viewtancos, particle->y-viewy, viewtansin);

	actor=NULL;
	this->particle=particle;
	
	// [BB] Translucent particles have to be rendered without the alpha test.
	if (gl_particles_style != 2 && trans>=1.0f-FLT_EPSILON) hw_styleflags = STYLEHW_Solid;
	else hw_styleflags = STYLEHW_NoAlphaTest;

	PutSprite(hw_styleflags != STYLEHW_Solid);
	rendered_sprites++;
}



