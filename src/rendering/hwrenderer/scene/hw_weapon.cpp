// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** hw_weapon.cpp
** Weapon sprite utilities
**
*/

#include "sbar.h"
#include "r_utility.h"
#include "v_video.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "models.h"
#include "hw_weapon.h"
#include "hw_fakeflat.h"
#include "texturemanager.h"

#include "hw_models.h"
#include "hw_dynlightdata.h"
#include "hw_material.h"
#include "hw_lighting.h"
#include "hw_cvars.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "flatvertices.h"
#include "hw_lightbuffer.h"
#include "hw_renderstate.h"

#include "vm.h"

EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, gl_fuzztype)
EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Bool, r_deathcamera)


//==========================================================================
//
// R_DrawPSprite
//
//==========================================================================

void HWDrawInfo::DrawPSprite(HUDSprite *huds, FRenderState &state)
{
	if (huds->RenderStyle.BlendOp == STYLEOP_Shadow)
	{
		state.SetColor(0.2f, 0.2f, 0.2f, 0.33f, huds->cm.Desaturation);
	}
	else
	{
		SetColor(state, Level, lightmode, huds->lightlevel, 0, isFullbrightScene(), huds->cm, huds->alpha, true);
	}
	state.SetLightIndex(-1);
	state.SetRenderStyle(huds->RenderStyle);
	state.SetTextureMode(huds->RenderStyle);
	state.SetObjectColor(huds->ObjectColor);
	if (huds->owner->Sector)
	{
		state.SetAddColor(huds->owner->Sector->AdditiveColors[sector_t::sprites] | 0xff000000);
	}
	else
	{
		state.SetAddColor(0);
	}
	state.SetDynLight(huds->dynrgb[0], huds->dynrgb[1], huds->dynrgb[2]);
	state.EnableBrightmap(!(huds->RenderStyle.Flags & STYLEF_ColorIsFixed));

	if (huds->mframe)
	{
		state.AlphaFunc(Alpha_GEqual, 0);

		FHWModelRenderer renderer(this, state, huds->lightindex);
		RenderHUDModel(&renderer, huds->weapon, huds->translation, huds->rotation + FVector3(huds->mx / 4., (huds->my - WEAPONTOP) / -4., 0), huds->pivot, huds->mframe);
		state.SetVertexBuffer(screen->mVertexData);
	}
	else
	{
		float thresh = (huds->texture->GetTranslucency() || huds->OverrideShader != -1) ? 0.f : gl_mask_sprite_threshold;
		state.AlphaFunc(Alpha_GEqual, thresh);
		FTranslationID trans = huds->weapon->GetTranslation();
		if ((huds->weapon->Flags & PSPF_PLAYERTRANSLATED)) trans = huds->owner->Translation;
		state.SetMaterial(huds->texture, UF_Sprite, CTF_Expand, CLAMP_XY_NOMIP, trans, huds->OverrideShader);
		state.Draw(DT_TriangleStrip, huds->mx, 4);
	}

	state.SetTextureMode(TM_NORMAL);
	state.AlphaFunc(Alpha_GEqual, gl_mask_sprite_threshold);
	state.SetObjectColor(0xffffffff);
	state.SetAddColor(0);
	state.SetDynLight(0, 0, 0);
	state.EnableBrightmap(false);
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void HWDrawInfo::DrawPlayerSprites(bool hudModelStep, FRenderState &state)
{
	auto oldlightmode = lightmode;
	if (!hudModelStep && isSoftwareLighting(oldlightmode)) SetFallbackLightMode();	// Software lighting cannot handle 2D content.
	for (auto &hudsprite : hudsprites)
	{
		if ((!!hudsprite.mframe) == hudModelStep)
			DrawPSprite(&hudsprite, state);
	}
	lightmode = oldlightmode;
}


//==========================================================================
//
//
//
//==========================================================================

static bool isBright(DPSprite *psp)
{
	if (psp != nullptr && psp->GetState() != nullptr)
	{
		bool disablefullbright = false;
		FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, nullAngle, nullptr);
		if (lump.isValid())
		{
			auto tex = TexMan.GetGameTexture(lump, true);
			if (tex) disablefullbright = tex->isFullbrightDisabled();
		}
		return psp->GetState()->GetFullbright() && !disablefullbright;
	}
	return false;
}

//==========================================================================
//
// Weapon position
//
//==========================================================================

static WeaponPosition2D GetWeaponPosition2D(player_t *player, double ticFrac)
{
	WeaponPosition2D w;
	P_BobWeapon(player, &w.bobx, &w.boby, ticFrac);

	// Interpolate the main weapon layer once so as to be able to add it to other layers.
	if ((w.weapon = player->FindPSprite(PSP_WEAPON)) != nullptr)
	{
		if (w.weapon->firstTic)
		{
			w.wx = (float)w.weapon->x;
			w.wy = (float)w.weapon->y;
		}
		else
		{
			w.wx = (float)(w.weapon->oldx + (w.weapon->x - w.weapon->oldx) * ticFrac);
			w.wy = (float)(w.weapon->oldy + (w.weapon->y - w.weapon->oldy) * ticFrac);
		}
	}
	else
	{
		w.wx = 0;
		w.wy = 0;
	}
	return w;
}

static WeaponPosition3D GetWeaponPosition3D(player_t *player, double ticFrac)
{
	WeaponPosition3D w;
	P_BobWeapon3D(player, &w.translation, &w.rotation, ticFrac);

	// Interpolate the main weapon layer once so as to be able to add it to other layers.
	if ((w.weapon = player->FindPSprite(PSP_WEAPON)) != nullptr)
	{
		if (w.weapon->firstTic)
		{
			w.wx = (float)w.weapon->x;
			w.wy = (float)w.weapon->y;
		}
		else
		{
			w.wx = (float)(w.weapon->oldx + (w.weapon->x - w.weapon->oldx) * ticFrac);
			w.wy = (float)(w.weapon->oldy + (w.weapon->y - w.weapon->oldy) * ticFrac);
		}
		
		auto weaponActor = w.weapon->GetCaller();

		if (weaponActor && weaponActor->IsKindOf(NAME_Weapon))
		{
			DVector3 *dPivot = (DVector3*) weaponActor->ScriptVar(NAME_BobPivot3D, nullptr);
			w.pivot.X = (float) dPivot->X;
			w.pivot.Y = (float) dPivot->Y;
			w.pivot.Z = (float) dPivot->Z;
		}
		else
		{
			w.pivot = FVector3(0,0,0);
		}
	}
	else
	{
		w.wx = 0;
		w.wy = 0;
		w.pivot = FVector3(0,0,0);
	}
	return w;
}

//==========================================================================
//
// Bobbing
//
//==========================================================================

static FVector2 BobWeapon2D(WeaponPosition2D &weap, DPSprite *psp, double ticFrac)
{
	if (psp->firstTic)
	{ // Can't interpolate the first tic.
		psp->firstTic = false;
		psp->ResetInterpolation();
	}

	float sx = float(psp->oldx + (psp->x - psp->oldx) * ticFrac);
	float sy = float(psp->oldy + (psp->y - psp->oldy) * ticFrac);

	if (psp->Flags & PSPF_ADDBOB)
	{
		sx += (psp->Flags & PSPF_MIRROR) ? -weap.bobx : weap.bobx;
		sy += weap.boby;
	}

	if (psp->Flags & PSPF_ADDWEAPON && psp->GetID() != PSP_WEAPON)
	{
		sx += weap.wx;
		sy += weap.wy;
	}
	return { sx, sy };
}

static FVector2 BobWeapon3D(WeaponPosition3D &weap, DPSprite *psp, FVector3 &translation, FVector3 &rotation, FVector3 &pivot, double ticFrac)
{
	if (psp->firstTic)
	{ // Can't interpolate the first tic.
		psp->firstTic = false;
		psp->ResetInterpolation();
	}

	float sx = float(psp->oldx + (psp->x - psp->oldx) * ticFrac);
	float sy = float(psp->oldy + (psp->y - psp->oldy) * ticFrac);
	float sz = 0;

	if (psp->Flags & PSPF_ADDBOB)
	{
		if (psp->Flags & PSPF_MIRROR)
		{
			translation = FVector3(-weap.translation.X, weap.translation.Y, weap.translation.Z);
			rotation = FVector3(-weap.rotation.X, weap.rotation.Y, weap.rotation.Z);
			pivot = FVector3(-weap.pivot.X, weap.pivot.Y, weap.pivot.Z);
		}
		else
		{
			translation = weap.translation ;
			rotation = weap.rotation ;
			pivot = weap.pivot ;
		}
	}
	else
	{
		translation = rotation = pivot = FVector3(0,0,0);
	}

	if (psp->Flags & PSPF_ADDWEAPON && psp->GetID() != PSP_WEAPON)
	{
		sx += weap.wx;
		sy += weap.wy;
	}
	return { sx, sy };
}

//==========================================================================
//
// Lighting
//
//==========================================================================

WeaponLighting HWDrawInfo::GetWeaponLighting(sector_t *viewsector, const DVector3 &pos, int cm, area_t in_area, const DVector3 &playerpos)
{
	WeaponLighting l;

	if (cm)
	{
		l.lightlevel = 255;
		l.cm.Clear();
		l.isbelow = false;
	}
	else
	{
		auto fakesec = hw_FakeFlat(viewsector, in_area, false);

		// calculate light level for weapon sprites
		l.lightlevel = hw_ClampLight(fakesec->lightlevel);

		// calculate colormap for weapon sprites
		if (viewsector->e->XFloor.ffloors.Size() && !(Level->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING))
		{
			TArray<lightlist_t> & lightlist = viewsector->e->XFloor.lightlist;
			for (unsigned i = 0; i<lightlist.Size(); i++)
			{
				double lightbottom;

				if (i<lightlist.Size() - 1)
				{
					lightbottom = lightlist[i + 1].plane.ZatPoint(pos);
				}
				else
				{
					lightbottom = viewsector->floorplane.ZatPoint(pos);
				}

				if (lightbottom < pos.Z)
				{
					l.cm = lightlist[i].extra_colormap;
					l.lightlevel = hw_ClampLight(*lightlist[i].p_lightlevel);
					break;
				}
			}
		}
		else
		{
			l.cm = fakesec->Colormap;
			if (Level->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING) l.cm.ClearColor();
		}

		l.lightlevel = CalcLightLevel(lightmode, l.lightlevel, getExtraLight(), true, 0);

		if (isSoftwareLighting(lightmode) || l.lightlevel < 92)
		{
			// Korshun: the way based on max possible light level for sector like in software renderer.
			double min_L = 36.0 / 31.0 - ((l.lightlevel / 255.0) * (63.0 / 31.0)); // Lightlevel in range 0-63
			if (min_L < 0)
				min_L = 0;
			else if (min_L > 1.0)
				min_L = 1.0;

			l.lightlevel = int((1.0 - min_L) * 255);
		}
		else
		{
			l.lightlevel = (2 * l.lightlevel + 255) / 3;
		}
		l.lightlevel = viewsector->CheckSpriteGlow(l.lightlevel, playerpos);
		l.isbelow = fakesec != viewsector && in_area == area_below;
	}

	// Korshun: fullbright fog in opengl, render weapon sprites fullbright (but don't cancel out the light color!)
	if (Level->brightfog && ((Level->flags&LEVEL_HASFADETABLE) || l.cm.FadeColor != 0))
	{
		l.lightlevel = 255;
	}
	return l;
}

//==========================================================================
//
//
//
//==========================================================================

void HUDSprite::SetBright(bool isbelow)
{
	if (!isbelow)
	{
		cm.MakeWhite();
	}
	else
	{
		// under water areas keep most of their color for fullbright objects
		cm.LightColor.r = (3 * cm.LightColor.r + 0xff) / 4;
		cm.LightColor.g = (3 * cm.LightColor.g + 0xff) / 4;
		cm.LightColor.b = (3 * cm.LightColor.b + 0xff) / 4;
	}
	lightlevel = 255;
}

//==========================================================================
//
// Render Style
//
//==========================================================================

bool HUDSprite::GetWeaponRenderStyle(DPSprite *psp, AActor *playermo, sector_t *viewsector, WeaponLighting &lighting)
{
	auto rs = psp->GetRenderStyle(playermo->RenderStyle, playermo->Alpha);

	visstyle_t vis;

	vis.RenderStyle = STYLE_Count;
	vis.Alpha = rs.second;
	vis.Invert = false;
	playermo->AlterWeaponSprite(&vis);

	alpha = (psp->Flags & PSPF_FORCEALPHA) ? 0.f : vis.Alpha;

	if (vis.RenderStyle != STYLE_Count && !(psp->Flags & PSPF_FORCESTYLE))
	{
		RenderStyle = vis.RenderStyle;
	}
	else
	{
		RenderStyle = rs.first;
	}
	if (RenderStyle.BlendOp == STYLEOP_None) return false;

	if (vis.Invert)
	{
		// this only happens for Strife's inverted weapon sprite
		RenderStyle.Flags |= STYLEF_InvertSource;
	}

	// Set the render parameters

	OverrideShader = -1;
	if (RenderStyle.BlendOp == STYLEOP_Fuzz)
	{
		if (gl_fuzztype != 0)
		{
			// Todo: implement shader selection here
			RenderStyle = LegacyRenderStyles[STYLE_Translucent];
			OverrideShader = SHADER_NoTexture + gl_fuzztype;
			alpha = 0.99f;	// trans may not be 1 here
		}
		else
		{
			RenderStyle.BlendOp = STYLEOP_Shadow;
		}
	}

	if (RenderStyle.Flags & STYLEF_TransSoulsAlpha)
	{
		alpha	= transsouls;
	}
	else if (RenderStyle.Flags & STYLEF_Alpha1)
	{
		alpha = 1.f;
	}
	else if (alpha == 0.f)
	{
		alpha = vis.Alpha;
	}
	if (!RenderStyle.IsVisible(alpha)) return false;	// if it isn't visible skip the rest.

	PalEntry ThingColor = (playermo->RenderStyle.Flags & STYLEF_ColorIsFixed) ? playermo->fillcolor : 0xffffff;
	ThingColor.a = 255;

	const bool bright = isBright(psp);
	ObjectColor = bright ? ThingColor : ThingColor.Modulate(viewsector->SpecialColors[sector_t::sprites]);

	lightlevel = lighting.lightlevel;
	cm = lighting.cm;
	if (bright) SetBright(lighting.isbelow);

	return true;
}

//==========================================================================
//
// Coordinates
//
//==========================================================================

bool HUDSprite::GetWeaponRect(HWDrawInfo *di, DPSprite *psp, float sx, float sy, player_t *player, double ticfrac)
{
	float			tx;
	float			scale;
	float			scalex;
	float			ftextureadj;
	float			ftexturemid;

	// decide which patch to use
	bool mirror;
	FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, nullAngle, &mirror);
	if (!lump.isValid()) return false;

	auto tex = TexMan.GetGameTexture(lump, false);
	if (!tex || !tex->isValid()) return false;
	auto& spi = tex->GetSpritePositioning(1);

	float vw = (float)viewwidth;
	float vh = (float)viewheight;

	FloatRect r = spi.GetSpriteRect();

	// calculate edges of the shape
	scalex = psp->baseScale.X * (320.0f / (240.0f * r_viewwindow.WidescreenRatio)) * (vw / 320);

	float x1, y1, x2, y2, u1, v1, u2, v2;

	tx = (psp->Flags & PSPF_MIRROR) ? ((160 - r.width) - (sx + r.left)) : (sx - (160 - r.left));
	x1 = tx * scalex + vw / 2;
	// [MC] Disabled these because vertices can be manipulated now.
	//if (x1 > vw)	return false; // off the right side
	x1 += viewwindowx;


	tx += r.width;
	x2 = tx * scalex + vw / 2;
	//if (x2 < 0) return false; // off the left side
	x2 += viewwindowx;

	// killough 12/98: fix psprite positioning problem
	ftextureadj = (120.0f / psp->baseScale.Y) - 100.0f; // [XA] scale relative to weapon baseline
	ftexturemid = 100.f - sy - r.top - psp->GetYAdjust(screenblocks >= 11) - ftextureadj;

	// [XA] note: Doom's native 1.2x aspect ratio was originally
	// handled here by multiplying SCREENWIDTH by 200 instead of
	// 240, but now the baseScale var defines this from now on.
	scale = psp->baseScale.Y * (SCREENHEIGHT*vw) / (SCREENWIDTH * 240.0f);
	y1 = viewwindowy + vh / 2 - (ftexturemid * scale);
	y2 = y1 + (r.height * scale) + 1;

	const bool flip = (psp->Flags & PSPF_FLIP);
	if (!(mirror) != !(flip))
	{
		u2 = spi.GetSpriteUL();
		v1 = spi.GetSpriteVT();
		u1 = spi.GetSpriteUR();
		v2 = spi.GetSpriteVB();
	}
	else
	{
		u1 = spi.GetSpriteUL();
		v1 = spi.GetSpriteVT();
		u2 = spi.GetSpriteUR();
		v2 = spi.GetSpriteVB();
	}

	// [MC] Code copied from DTA_Rotate.
	// Big thanks to IvanDobrovski who helped me modify this.

	WeaponInterp Vert;
	Vert.v[0] = FVector2(x1, y1);
	Vert.v[1] = FVector2(x1, y2);
	Vert.v[2] = FVector2(x2, y1);
	Vert.v[3] = FVector2(x2, y2);

	for (int i = 0; i < 4; i++)
	{
		const float cx = (flip) ? -psp->Coord[i].X : psp->Coord[i].X;
		Vert.v[i] += FVector2(cx * scalex, psp->Coord[i].Y * scale);
	}
	if (psp->rotation != nullAngle || !psp->scale.isZero())
	{
		// [MC] Sets up the alignment for starting the pivot at, in a corner.
		float anchorx, anchory;
		switch (psp->VAlign)
		{
			default:
			case PSPA_TOP:		anchory = 0.0;	break;
			case PSPA_CENTER:	anchory = 0.5;	break;
			case PSPA_BOTTOM:	anchory = 1.0;	break;
		}

		switch (psp->HAlign)
		{
			default:
			case PSPA_LEFT:		anchorx = 0.0;	break;
			case PSPA_CENTER:	anchorx = 0.5;	break;
			case PSPA_RIGHT:	anchorx = 1.0;	break;
		}
		// Handle PSPF_FLIP.
		if (flip) anchorx = 1.0 - anchorx;

		FAngle rot = FAngle::fromDeg(float((flip) ? -psp->rotation.Degrees() : psp->rotation.Degrees()));
		const float cosang = rot.Cos();
		const float sinang = rot.Sin();
		
		float xcenter, ycenter;
		const float width = x2 - x1;
		const float height = y2 - y1;
		const float px = float((flip) ? -psp->pivot.X : psp->pivot.X);
		const float py = float(psp->pivot.Y);

		// Set up the center and offset accordingly. PivotPercent changes it to be a range [0.0, 1.0]
		// instead of pixels and is enabled by default.
		if (psp->Flags & PSPF_PIVOTPERCENT)
		{
			xcenter = x1 + (width * anchorx + width * px);
			ycenter = y1 + (height * anchory + height * py);
		}
		else
		{
			xcenter = x1 + (width * anchorx + scalex * px);
			ycenter = y1 + (height * anchory + scale * py);
		}

		// Now adjust the position, rotation and scale of the image based on the latter two.
		for (int i = 0; i < 4; i++)
		{
			Vert.v[i] -= {xcenter, ycenter};
			const float xx = xcenter + psp->scale.X * (Vert.v[i].X * cosang + Vert.v[i].Y * sinang);
			const float yy = ycenter - psp->scale.Y * (Vert.v[i].X * sinang - Vert.v[i].Y * cosang);
			Vert.v[i] = {xx, yy};
		}
	}
	psp->Vert = Vert;

	if (psp->scale.X == 0.0 || psp->scale.Y == 0.0)
		return false;

	const bool interp = (psp->InterpolateTic || psp->Flags & PSPF_INTERPOLATE);

	for (int i = 0; i < 4; i++)
	{
		FVector2 t = Vert.v[i];
		if (interp)
			t = psp->Prev.v[i] + (psp->Vert.v[i] - psp->Prev.v[i]) * ticfrac;

		Vert.v[i] = t;
	}
	
	// [MC] If this is absolutely necessary, uncomment it. It just checks if all the vertices 
	// are all off screen either to the right or left, but is it honestly needed?
	/*
	if ((
		Vert.v[0].X > 0.0 &&
		Vert.v[1].X > 0.0 &&
		Vert.v[2].X > 0.0 &&
		Vert.v[3].X > 0.0) || (
		Vert.v[0].X < vw &&
		Vert.v[1].X < vw &&
		Vert.v[2].X < vw &&
		Vert.v[3].X < vw))
		return false;
	*/
	auto verts = screen->mVertexData->AllocVertices(4);
	mx = verts.second;

	verts.first[0].Set(Vert.v[0].X, Vert.v[0].Y, 0, u1, v1);
	verts.first[1].Set(Vert.v[1].X, Vert.v[1].Y, 0, u1, v2);
	verts.first[2].Set(Vert.v[2].X, Vert.v[2].Y, 0, u2, v1);
	verts.first[3].Set(Vert.v[3].X, Vert.v[3].Y, 0, u2, v2);

	texture = tex;
	return true;
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================
void HWDrawInfo::PreparePlayerSprites2D(sector_t * viewsector, area_t in_area)
{
	static PClass * wpCls = PClass::FindClass("Weapon");
	static unsigned ModifyBobLayerVIndex = GetVirtualIndex(wpCls, "ModifyBobLayer");
	static VMFunction * ModifyBobLayerOrigFunc = wpCls->Virtuals.Size() > ModifyBobLayerVIndex ? wpCls->Virtuals[ModifyBobLayerVIndex] : nullptr;
	
	AActor * playermo = players[consoleplayer].camera;
	player_t * player = playermo->player;

	const auto &vp = Viewpoint;

	AActor *camera = vp.camera;

	WeaponPosition2D weap = GetWeaponPosition2D(camera->player, vp.TicFrac);
	WeaponLighting light = GetWeaponLighting(viewsector, vp.Pos, isFullbrightScene(), in_area, camera->Pos());

	VMFunction * ModifyBobLayer = nullptr;
	DVector2 bobxy = DVector2(weap.bobx , weap.boby);

	if(weap.weapon && weap.weapon->GetCaller())
	{
		PClass * cls = weap.weapon->GetCaller()->GetClass();
		ModifyBobLayer = cls->Virtuals.Size() > ModifyBobLayerVIndex ? cls->Virtuals[ModifyBobLayerVIndex] : nullptr;

		if( ModifyBobLayer == ModifyBobLayerOrigFunc) ModifyBobLayer = nullptr;
	}

	// hack alert! Rather than changing everything in the underlying lighting code let's just temporarily change
	// light mode here to draw the weapon sprite.
	auto oldlightmode = lightmode;
	if (isSoftwareLighting(oldlightmode)) SetFallbackLightMode();

	for (DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		if (!psp->GetState()) continue;
		
		FSpriteModelFrame *smf = FindModelFrame(psp->Caller, psp->GetSprite(), psp->GetFrame(), false);

		// This is an 'either-or' proposition. This maybe needs some work to allow overlays with weapon models but as originally implemented this just won't work.
		if (smf) continue;

		HUDSprite hudsprite;
		hudsprite.owner = playermo;
		hudsprite.mframe = smf;
		hudsprite.weapon = psp;

		if (!hudsprite.GetWeaponRenderStyle(psp, camera, viewsector, light)) continue;

		if(ModifyBobLayer && (psp->Flags & PSPF_ADDBOB))
		{
			DVector2 out;
			VMValue param[] = { weap.weapon->GetCaller() , bobxy.X , bobxy.Y , psp->GetID() , vp.TicFrac };
			VMReturn ret(&out);

			VMCall(ModifyBobLayer, param, 5, &ret, 1);

			weap.bobx = out.X;
			weap.boby = out.Y;
		}

		FVector2 spos = BobWeapon2D(weap, psp, vp.TicFrac);

		hudsprite.dynrgb[0] = hudsprite.dynrgb[1] = hudsprite.dynrgb[2] = 0;
		hudsprite.lightindex = -1;
		// set the lighting parameters
		if (hudsprite.RenderStyle.BlendOp != STYLEOP_Shadow && Level->HasDynamicLights && !isFullbrightScene() && gl_light_sprites)
		{
			GetDynSpriteLight(playermo, nullptr, hudsprite.dynrgb);
		}

		if (!hudsprite.GetWeaponRect(this, psp, spos.X, spos.Y, player, vp.TicFrac)) continue;
		hudsprites.Push(hudsprite);
	}
	lightmode = oldlightmode;
}

void HWDrawInfo::PreparePlayerSprites3D(sector_t * viewsector, area_t in_area)
{
	static PClass * wpCls = PClass::FindClass("Weapon");
	
	static unsigned ModifyBobLayer3DVIndex = GetVirtualIndex(wpCls, "ModifyBobLayer3D");
	static unsigned ModifyBobPivotLayer3DVIndex = GetVirtualIndex(wpCls, "ModifyBobPivotLayer3D");

	static VMFunction * ModifyBobLayer3DOrigFunc = wpCls->Virtuals.Size() > ModifyBobLayer3DVIndex ? wpCls->Virtuals[ModifyBobLayer3DVIndex] : nullptr;
	static VMFunction * ModifyBobPivotLayer3DOrigFunc = wpCls->Virtuals.Size() > ModifyBobPivotLayer3DVIndex ? wpCls->Virtuals[ModifyBobPivotLayer3DVIndex] : nullptr;
	
	AActor * playermo = players[consoleplayer].camera;
	player_t * player = playermo->player;

	const auto &vp = Viewpoint;

	AActor *camera = vp.camera;

	WeaponPosition3D weap = GetWeaponPosition3D(camera->player, vp.TicFrac);
	WeaponLighting light = GetWeaponLighting(viewsector, vp.Pos, isFullbrightScene(), in_area, camera->Pos());

	VMFunction * ModifyBobLayer3D = nullptr;
	VMFunction * ModifyBobPivotLayer3D = nullptr;

	DVector3 translation = DVector3(weap.translation);
	DVector3 rotation = DVector3(weap.rotation);
	DVector3 pivot = DVector3(weap.pivot);

	if(weap.weapon && weap.weapon->GetCaller())
	{
		PClass * cls = weap.weapon->GetCaller()->GetClass();
		ModifyBobLayer3D = cls->Virtuals.Size() > ModifyBobLayer3DVIndex ? cls->Virtuals[ModifyBobLayer3DVIndex] : nullptr;
		ModifyBobPivotLayer3D = cls->Virtuals.Size() > ModifyBobPivotLayer3DVIndex ? cls->Virtuals[ModifyBobPivotLayer3DVIndex] : nullptr;

		if( ModifyBobLayer3D == ModifyBobLayer3DOrigFunc) ModifyBobLayer3D = nullptr;
		if( ModifyBobPivotLayer3D == ModifyBobPivotLayer3DOrigFunc) ModifyBobPivotLayer3D = nullptr;
	}

	// hack alert! Rather than changing everything in the underlying lighting code let's just temporarily change
	// light mode here to draw the weapon sprite.
	auto oldlightmode = lightmode;
	if (isSoftwareLighting(oldlightmode)) SetFallbackLightMode();

	for (DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		if (!psp->GetState()) continue;
		FSpriteModelFrame *smf = FindModelFrame(psp->Caller, psp->GetSprite(), psp->GetFrame(), false);

		// This is an 'either-or' proposition. This maybe needs some work to allow overlays with weapon models but as originally implemented this just won't work.
		if (!smf) continue;

		HUDSprite hudsprite;
		hudsprite.owner = playermo;
		hudsprite.mframe = smf;
		hudsprite.weapon = psp;

		if(ModifyBobLayer3D && (psp->Flags & PSPF_ADDBOB))
		{
			DVector3 t, r;
			
			VMReturn returns[2];

			returns[0].Vec3At(&t);
			returns[1].Vec3At(&r);

			VMValue param[] = { weap.weapon->GetCaller() , translation.X, translation.Y, translation.Z, rotation.X, rotation.Y, rotation.Z, psp->GetID() , vp.TicFrac };
			VMCall(ModifyBobLayer3D, param, 9, returns, 2);

			weap.translation = FVector3(t);
			weap.rotation = FVector3(r);
		}

		if(ModifyBobPivotLayer3D && (psp->Flags & PSPF_ADDBOB))
		{
			DVector3 p;

			VMReturn ret(&p);

			VMValue param[] = { weap.weapon->GetCaller() , pivot.X, pivot.Y, pivot.Z, psp->GetID() , vp.TicFrac };
			VMCall(ModifyBobPivotLayer3D, param, 6, &ret, 1);

			weap.pivot = FVector3(p);
		}

		if (!hudsprite.GetWeaponRenderStyle(psp, camera, viewsector, light)) continue;

		//FVector2 spos = BobWeapon3D(weap, psp, hudsprite.translation, hudsprite.rotation, hudsprite.pivot, vp.TicFrac);

		FVector2 spos = BobWeapon3D(weap, psp, hudsprite.translation, hudsprite.rotation, hudsprite.pivot, vp.TicFrac);

		hudsprite.dynrgb[0] = hudsprite.dynrgb[1] = hudsprite.dynrgb[2] = 0;
		hudsprite.lightindex = -1;
		// set the lighting parameters
		if (hudsprite.RenderStyle.BlendOp != STYLEOP_Shadow && Level->HasDynamicLights && !isFullbrightScene() && gl_light_sprites)
		{
			hw_GetDynModelLight(playermo, lightdata);
			hudsprite.lightindex = screen->mLights->UploadLights(lightdata);
			LightProbe* probe = FindLightProbe(playermo->Level, playermo->X(), playermo->Y(), playermo->Center());
			if (probe)
			{
				hudsprite.dynrgb[0] = probe->Red;
				hudsprite.dynrgb[1] = probe->Green;
				hudsprite.dynrgb[2] = probe->Blue;
			}
		}

		// [BB] In the HUD model step we just render the model and break out. 
		hudsprite.mx = spos.X;
		hudsprite.my = spos.Y;

		hudsprites.Push(hudsprite);
	}
	lightmode = oldlightmode;
}

void HWDrawInfo::PreparePlayerSprites(sector_t * viewsector, area_t in_area)
{

	AActor * playermo = players[consoleplayer].camera;
	player_t * player = playermo->player;
	
    const auto &vp = Viewpoint;

	AActor *camera = vp.camera;

	// this is the same as the software renderer
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) ||
		(r_deathcamera && camera->health <= 0))
		return;

	const bool hudModelStep = IsHUDModelForPlayerAvailable(camera->player);
	
	if(hudModelStep)
	{
		PreparePlayerSprites3D(viewsector,in_area);
	}
	else
	{
		PreparePlayerSprites2D(viewsector,in_area);
	}

	PrepareTargeterSprites(vp.TicFrac);
}


//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void HWDrawInfo::PrepareTargeterSprites(double ticfrac)
{
	AActor * playermo = players[consoleplayer].camera;
	player_t * player = playermo->player;
	AActor *camera = Viewpoint.camera;

	// this is the same as above
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) ||
		(r_deathcamera && camera->health <= 0))
		return;

	HUDSprite hudsprite;

	hudsprite.owner = playermo;
	hudsprite.mframe = nullptr;
	hudsprite.cm.Clear();
	hudsprite.lightlevel = 255;
	hudsprite.ObjectColor = 0xffffffff;
	hudsprite.alpha = 1;
	hudsprite.RenderStyle = DefaultRenderStyle();
	hudsprite.OverrideShader = -1;
	hudsprite.dynrgb[0] = hudsprite.dynrgb[1] = hudsprite.dynrgb[2] = 0;

	// The Targeter's sprites are always drawn normally.
	for (DPSprite *psp = player->FindPSprite(PSP_TARGETCENTER); psp != nullptr; psp = psp->GetNext())
	{
		if (psp->GetState() != nullptr && (psp->GetID() != PSP_TARGETCENTER || CrosshairImage == nullptr))
		{
			hudsprite.weapon = psp;
			
			if (hudsprite.GetWeaponRect(this, psp, psp->x, psp->y, player, ticfrac))
			{
				hudsprites.Push(hudsprite);
			}
		}
	}
}

