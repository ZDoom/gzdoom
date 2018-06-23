// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2018 Christoph Oelckers
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
** gl_drawinfo.cpp
** Basic scene draw info management class
**
*/

#include "a_sharedglobal.h"
#include "r_utility.h"
#include "r_sky.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "hw_fakeflat.h"
#include "hw_drawinfo.h"
#include "hw_portal.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/utility/hw_cvars.h"

EXTERN_CVAR(Float, r_visibility)
CVAR(Bool, gl_bandedswlight, false, CVAR_ARCHIVE)

sector_t * hw_FakeFlat(sector_t * sec, sector_t * dest, area_t in_area, bool back);

//==========================================================================
//
//
//
//==========================================================================

void HWDrawInfo::ClearBuffers()
{
	for(unsigned int i=0;i< otherfloorplanes.Size();i++)
	{
		gl_subsectorrendernode * node = otherfloorplanes[i];
		while (node)
		{
			gl_subsectorrendernode * n = node;
			node = node->next;
			delete n;
		}
	}
	otherfloorplanes.Clear();

	for(unsigned int i=0;i< otherceilingplanes.Size();i++)
	{
		gl_subsectorrendernode * node = otherceilingplanes[i];
		while (node)
		{
			gl_subsectorrendernode * n = node;
			node = node->next;
			delete n;
		}
	}
	otherceilingplanes.Clear();

	// clear all the lists that might not have been cleared already
	MissingUpperTextures.Clear();
	MissingLowerTextures.Clear();
	MissingUpperSegs.Clear();
	MissingLowerSegs.Clear();
	SubsectorHacks.Clear();
	CeilingStacks.Clear();
	FloorStacks.Clear();
	HandledSubsectors.Clear();
	spriteindex = 0;

	CurrentMapSections.Resize(level.NumMapSections);
	CurrentMapSections.Zero();

	sectorrenderflags.Resize(level.sectors.Size());
	ss_renderflags.Resize(level.subsectors.Size());
	no_renderflags.Resize(level.subsectors.Size());

	memset(&sectorrenderflags[0], 0, level.sectors.Size() * sizeof(sectorrenderflags[0]));
	memset(&ss_renderflags[0], 0, level.subsectors.Size() * sizeof(ss_renderflags[0]));
	memset(&no_renderflags[0], 0, level.nodes.Size() * sizeof(no_renderflags[0]));

	mClipPortal = nullptr;
	mCurrentPortal = nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

void HWDrawInfo::UpdateCurrentMapSection()
{
	const int mapsection = R_PointInSubsector(Viewpoint.Pos)->mapsection;
	CurrentMapSections.Set(mapsection);
}


//-----------------------------------------------------------------------------
//
// Sets the area the camera is in
//
//-----------------------------------------------------------------------------

void HWDrawInfo::SetViewArea()
{
    auto &vp = Viewpoint;
	// The render_sector is better suited to represent the current position in GL
	vp.sector = R_PointInSubsector(vp.Pos)->render_sector;

	// Get the heightsec state from the render sector, not the current one!
	if (vp.sector->GetHeightSec())
	{
		in_area = vp.Pos.Z <= vp.sector->heightsec->floorplane.ZatPoint(vp.Pos) ? area_below :
			(vp.Pos.Z > vp.sector->heightsec->ceilingplane.ZatPoint(vp.Pos) &&
				!(vp.sector->heightsec->MoreFlags&SECMF_FAKEFLOORONLY)) ? area_above : area_normal;
	}
	else
	{
		in_area = level.HasHeightSecs ? area_default : area_normal;	// depends on exposed lower sectors, if map contains heightsecs.
	}
}

//-----------------------------------------------------------------------------
//
// 
//
//-----------------------------------------------------------------------------

int HWDrawInfo::SetFullbrightFlags(player_t *player)
{
	FullbrightFlags = 0;

	// check for special colormaps
	player_t * cplayer = player? player->camera->player : nullptr;
	if (cplayer)
	{
		int cm = CM_DEFAULT;
		if (cplayer->extralight == INT_MIN)
		{
			cm = CM_FIRSTSPECIALCOLORMAP + INVERSECOLORMAP;
			Viewpoint.extralight = 0;
			FullbrightFlags = Fullbright;
			// This does never set stealth vision.
		}
		else if (cplayer->fixedcolormap != NOFIXEDCOLORMAP)
		{
			cm = CM_FIRSTSPECIALCOLORMAP + cplayer->fixedcolormap;
			FullbrightFlags = Fullbright;
			if (gl_enhanced_nv_stealth > 2) FullbrightFlags |= StealthVision;
		}
		else if (cplayer->fixedlightlevel != -1)
		{
			auto torchtype = PClass::FindActor(NAME_PowerTorch);
			auto litetype = PClass::FindActor(NAME_PowerLightAmp);
			for (AInventory * in = cplayer->mo->Inventory; in; in = in->Inventory)
			{
				//PalEntry color = in->CallGetBlend();

				// Need special handling for light amplifiers 
				if (in->IsKindOf(torchtype))
				{
					FullbrightFlags = Fullbright;
					if (gl_enhanced_nv_stealth > 1) FullbrightFlags |= StealthVision;
				}
				else if (in->IsKindOf(litetype))
				{
					FullbrightFlags = Fullbright;
					if (gl_enhanced_nightvision) FullbrightFlags |= Nightvision;
					if (gl_enhanced_nv_stealth > 0) FullbrightFlags |= StealthVision;
				}
			}
		}
		return cm;
	}
	else
	{
		return CM_DEFAULT;
	}
}

//-----------------------------------------------------------------------------
//
// R_FrustumAngle
//
//-----------------------------------------------------------------------------

angle_t HWDrawInfo::FrustumAngle()
{
	float tilt = fabs(Viewpoint.HWAngles.Pitch.Degrees);

	// If the pitch is larger than this you can look all around at a FOV of 90°
	if (tilt > 46.0f) return 0xffffffff;

	// ok, this is a gross hack that barely works...
	// but at least it doesn't overestimate too much...
	double floatangle = 2.0 + (45.0 + ((tilt / 1.9)))*Viewpoint.FieldOfView.Degrees*48.0 / AspectMultiplier(r_viewwindow.WidescreenRatio) / 90.0;
	angle_t a1 = DAngle(floatangle).BAMs();
	if (a1 >= ANGLE_180) return 0xffffffff;
	return a1;
}

//-----------------------------------------------------------------------------
//
// Setup the modelview matrix
//
//-----------------------------------------------------------------------------

void HWDrawInfo::SetViewMatrix(const FRotator &angles, float vx, float vy, float vz, bool mirror, bool planemirror)
{
	float mult = mirror ? -1.f : 1.f;
	float planemult = planemirror ? -level.info->pixelstretch : level.info->pixelstretch;

	VPUniforms.mViewMatrix.loadIdentity();
	VPUniforms.mViewMatrix.rotate(angles.Roll.Degrees, 0.0f, 0.0f, 1.0f);
	VPUniforms.mViewMatrix.rotate(angles.Pitch.Degrees, 1.0f, 0.0f, 0.0f);
	VPUniforms.mViewMatrix.rotate(angles.Yaw.Degrees, 0.0f, mult, 0.0f);
	VPUniforms.mViewMatrix.translate(vx * mult, -vz * planemult, -vy);
	VPUniforms.mViewMatrix.scale(-mult, planemult, 1);
}


//-----------------------------------------------------------------------------
//
// SetupView
// Setup the view rotation matrix for the given viewpoint
//
//-----------------------------------------------------------------------------
void HWDrawInfo::SetupView(float vx, float vy, float vz, bool mirror, bool planemirror)
{
	auto &vp = Viewpoint;
	vp.SetViewAngle(r_viewwindow);
	SetViewMatrix(vp.HWAngles, vx, vy, vz, mirror, planemirror);
	SetCameraPos(vp.Pos);
	ApplyVPUniforms();
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

IPortal * HWDrawInfo::FindPortal(const void * src)
{
	int i = Portals.Size() - 1;

	while (i >= 0 && Portals[i] && Portals[i]->GetSource() != src) i--;
	return i >= 0 ? Portals[i] : nullptr;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void HWViewpointUniforms::SetDefaults()
{
	mProjectionMatrix.loadIdentity();
	mViewMatrix.loadIdentity();
	mNormalViewMatrix.loadIdentity();
	mViewHeight = viewheight;
	mGlobVis = (float)R_GetGlobVis(r_viewwindow, r_visibility) / 32.f;
	mPalLightLevels = static_cast<int>(gl_bandedswlight) | (static_cast<int>(gl_fogmode) << 8);
	mClipLine.X = -10000000.0f;

}
