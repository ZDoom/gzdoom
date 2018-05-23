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
** gl_flat.cpp
** Flat processing
**
*/

#include "a_sharedglobal.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "p_lnspec.h"
#include "r_data/matrix.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/utility/hw_lighting.h"
#include "hwrenderer/textures/hw_material.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hw_drawstructs.h"

#ifdef _DEBUG
CVAR(Int, gl_breaksec, -1, 0)
#endif
//==========================================================================
//
// Sets the texture matrix according to the plane's texture positioning
// information
//
//==========================================================================

bool hw_SetPlaneTextureRotation(const GLSectorPlane * secplane, FMaterial * gltexture, VSMatrix &dest)
{
	// only manipulate the texture matrix if needed.
	if (!secplane->Offs.isZero() ||
		secplane->Scale.X != 1. || secplane->Scale.Y != 1 ||
		secplane->Angle != 0 ||
		gltexture->TextureWidth() != 64 ||
		gltexture->TextureHeight() != 64)
	{
		float uoffs = secplane->Offs.X / gltexture->TextureWidth();
		float voffs = secplane->Offs.Y / gltexture->TextureHeight();

		float xscale1 = secplane->Scale.X;
		float yscale1 = secplane->Scale.Y;
		if (gltexture->tex->bHasCanvas)
		{
			yscale1 = 0 - yscale1;
		}
		float angle = -secplane->Angle;

		float xscale2 = 64.f / gltexture->TextureWidth();
		float yscale2 = 64.f / gltexture->TextureHeight();

		dest.loadIdentity();
		dest.scale(xscale1, yscale1, 1.0f);
		dest.translate(uoffs, voffs, 0.0f);
		dest.scale(xscale2, yscale2, 1.0f);
		dest.rotate(angle, 0.0f, 0.0f, 1.0f);
		return true;
	}
	return false;
}

//==========================================================================
//
// Flats 
//
//==========================================================================

bool GLFlat::SetupLights(int pass, FLightNode * node, FDynLightData &lightdata, int portalgroup)
{
	Plane p;

	if (renderstyle == STYLE_Add && !level.lightadditivesurfaces) return false;	// no lights on additively blended surfaces.

	lightdata.Clear();
	while (node)
	{
		ADynamicLight * light = node->lightsource;

		if (light->flags2&MF2_DORMANT)
		{
			node = node->nextLight;
			continue;
		}
		iter_dlightf++;

		// we must do the side check here because gl_GetLight needs the correct plane orientation
		// which we don't have for Legacy-style 3D-floors
		double planeh = plane.plane.ZatPoint(light);
		if ((planeh<light->Z() && ceiling) || (planeh>light->Z() && !ceiling))
		{
			node = node->nextLight;
			continue;
		}

		p.Set(plane.plane.Normal(), plane.plane.fD());
		lightdata.GetLight(portalgroup, p, light, false);
		node = node->nextLight;
	}

	return true;
}

bool GLFlat::SetupSubsectorLights(int pass, subsector_t * sub, FDynLightData &lightdata)
{
	return SetupLights(pass, sub->lighthead, lightdata, sub->sector->PortalGroup);
}

bool GLFlat::SetupSectorLights(int pass, sector_t * sec, FDynLightData &lightdata)
{
	return SetupLights(pass, sec->lighthead, lightdata, sec->PortalGroup);
}

//==========================================================================
//
// GLFlat::PutFlat
//
// submit to the renderer
//
//==========================================================================

inline void GLFlat::PutFlat(HWDrawInfo *di, bool fog)
{
	if (di->FixedColormap)
	{
		Colormap.Clear();
	}
	dynlightindex = -1;	// make sure this is always initialized to something proper.
	di->AddFlat(this, fog);
}

//==========================================================================
//
// This draws one flat 
// The passed sector does not indicate the area which is rendered. 
// It is only used as source for the plane data.
// The whichplane boolean indicates if the flat is a floor(false) or a ceiling(true)
//
//==========================================================================

void GLFlat::Process(HWDrawInfo *di, sector_t * model, int whichplane, bool fog)
{
	plane.GetFromSector(model, whichplane);
	if (whichplane != int(ceiling))
	{
		// Flip the normal if the source plane has a different orientation than what we are about to render.
		plane.plane.FlipVert();
	}

	if (!fog)
	{
		gltexture=FMaterial::ValidateTexture(plane.texture, false, true);
		if (!gltexture) return;
		if (gltexture->tex->isFullbright()) 
		{
			Colormap.MakeWhite();
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
	
	PutFlat(di, fog);
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
	lightlevel = hw_ClampLight(*light->p_lightlevel);
	
	if (rover->flags & FF_FOG)
	{
		Colormap.LightColor = light->extra_colormap.FadeColor;
		FlatColor = 0xffffffff;
	}
	else
	{
		Colormap.CopyFrom3DLight(light);
		FlatColor = *plane.flatcolor;
	}


	alpha = rover->alpha/255.0f;
	renderstyle = rover->flags&FF_ADDITIVETRANS? STYLE_Add : STYLE_Translucent;
	if (plane.model->VBOHeightcheck(plane.isceiling))
	{
		iboindex = plane.vindex;
	}
	else
	{
		iboindex = -1;
	}
}

//==========================================================================
//
// Process a sector's flats for rendering
// This function is only called once per sector.
// Subsequent subsectors are just quickly added to the ss_renderflags array
//
//==========================================================================

void GLFlat::ProcessSector(HWDrawInfo *di, sector_t * frontsector)
{
	lightlist_t * light;
	FSectorPortal *port;

#ifdef _DEBUG
	if (frontsector->sectornum == gl_breaksec)
	{
		int a = 0;
	}
#endif

	// Get the real sector for this one.
	sector = &level.sectors[frontsector->sectornum];
	extsector_t::xfloor &x = sector->e->XFloor;
	dynlightindex = -1;

	uint8_t &srf = di->sectorrenderflags[sector->sectornum];

	//
	//
	//
	// do floors
	//
	//
	//
	if (frontsector->floorplane.ZatPoint(r_viewpoint.Pos) <= r_viewpoint.Pos.Z)
	{
		// process the original floor first.

		srf |= SSRF_RENDERFLOOR;

		lightlevel = hw_ClampLight(frontsector->GetFloorLight());
		Colormap = frontsector->Colormap;
		FlatColor = frontsector->SpecialColors[sector_t::floor];
		port = frontsector->ValidatePortal(sector_t::floor);
		if ((stack = (port != NULL)))
		{
			if (port->mType == PORTS_STACKEDSECTORTHING)
			{
				di->AddFloorStack(sector);	// stacked sector things require visplane merging.
			}
			alpha = frontsector->GetAlpha(sector_t::floor);
		}
		else
		{
			alpha = 1.0f - frontsector->GetReflect(sector_t::floor);
		}

		if (alpha != 0.f && frontsector->GetTexture(sector_t::floor) != skyflatnum)
		{
			if (frontsector->VBOHeightcheck(sector_t::floor))
			{
				iboindex = frontsector->iboindex[sector_t::floor];
			}
			else
			{
				iboindex = -1;
			}

			ceiling = false;
			renderflags = SSRF_RENDERFLOOR;

			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sector, &frontsector->floorplane, false);
				if ((!(sector->GetFlags(sector_t::floor)&PLANEF_ABSLIGHTING) || light->lightsource == NULL)
					&& (light->p_lightlevel != &frontsector->lightlevel))
				{
					lightlevel = hw_ClampLight(*light->p_lightlevel);
				}

				Colormap.CopyFrom3DLight(light);
			}
			renderstyle = STYLE_Translucent;
			Process(di, frontsector, sector_t::floor, false);
		}
	}

	//
	//
	//
	// do ceilings
	//
	//
	//
	if (frontsector->ceilingplane.ZatPoint(r_viewpoint.Pos) >= r_viewpoint.Pos.Z)
	{
		// process the original ceiling first.

		srf |= SSRF_RENDERCEILING;

		lightlevel = hw_ClampLight(frontsector->GetCeilingLight());
		Colormap = frontsector->Colormap;
		FlatColor = frontsector->SpecialColors[sector_t::ceiling];
		port = frontsector->ValidatePortal(sector_t::ceiling);
		if ((stack = (port != NULL)))
		{
			if (port->mType == PORTS_STACKEDSECTORTHING)
			{
				di->AddCeilingStack(sector);
			}
			alpha = frontsector->GetAlpha(sector_t::ceiling);
		}
		else
		{
			alpha = 1.0f - frontsector->GetReflect(sector_t::ceiling);
		}

		if (alpha != 0.f && frontsector->GetTexture(sector_t::ceiling) != skyflatnum)
		{
			if (frontsector->VBOHeightcheck(sector_t::ceiling))
			{
				iboindex = frontsector->iboindex[sector_t::ceiling];
			}
			else
			{
				iboindex = -1;
			}

			ceiling = true;
			renderflags = SSRF_RENDERCEILING;

			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sector, &sector->ceilingplane, true);

				if ((!(sector->GetFlags(sector_t::ceiling)&PLANEF_ABSLIGHTING))
					&& (light->p_lightlevel != &frontsector->lightlevel))
				{
					lightlevel = hw_ClampLight(*light->p_lightlevel);
				}
				Colormap.CopyFrom3DLight(light);
			}
			renderstyle = STYLE_Translucent;
			Process(di, frontsector, sector_t::ceiling, false);
		}
	}

	//
	//
	//
	// do 3D floors
	//
	//
	//

	stack = false;
	if (x.ffloors.Size())
	{
		player_t * player = players[consoleplayer].camera->player;

		renderflags = SSRF_RENDER3DPLANES;
		srf |= SSRF_RENDER3DPLANES;
		// 3d-floors must not overlap!
		double lastceilingheight = sector->CenterCeiling();	// render only in the range of the
		double lastfloorheight = sector->CenterFloor();		// current sector part (if applicable)
		F3DFloor * rover;
		int k;

		// floors are ordered now top to bottom so scanning the list for the best match
		// is no longer necessary.

		ceiling = true;
		Colormap = frontsector->Colormap;
		for (k = 0; k < (int)x.ffloors.Size(); k++)
		{
			rover = x.ffloors[k];

			if ((rover->flags&(FF_EXISTS | FF_RENDERPLANES | FF_THISINSIDE)) == (FF_EXISTS | FF_RENDERPLANES))
			{
				if (rover->flags&FF_FOG && di->FixedColormap) continue;
				if (!rover->top.copied && rover->flags&(FF_INVERTPLANES | FF_BOTHPLANES))
				{
					double ff_top = rover->top.plane->ZatPoint(sector->centerspot);
					if (ff_top < lastceilingheight)
					{
						if (r_viewpoint.Pos.Z <= rover->top.plane->ZatPoint(r_viewpoint.Pos))
						{
							SetFrom3DFloor(rover, true, !!(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;
							Process(di, rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
						}
						lastceilingheight = ff_top;
					}
				}
				if (!rover->bottom.copied && !(rover->flags&FF_INVERTPLANES))
				{
					double ff_bottom = rover->bottom.plane->ZatPoint(sector->centerspot);
					if (ff_bottom < lastceilingheight)
					{
						if (r_viewpoint.Pos.Z <= rover->bottom.plane->ZatPoint(r_viewpoint.Pos))
						{
							SetFrom3DFloor(rover, false, !(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;
							Process(di, rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
						}
						lastceilingheight = ff_bottom;
						if (rover->alpha < 255) lastceilingheight += EQUAL_EPSILON;
					}
				}
			}
		}

		ceiling = false;
		for (k = x.ffloors.Size() - 1; k >= 0; k--)
		{
			rover = x.ffloors[k];

			if ((rover->flags&(FF_EXISTS | FF_RENDERPLANES | FF_THISINSIDE)) == (FF_EXISTS | FF_RENDERPLANES))
			{
				if (rover->flags&FF_FOG && di->FixedColormap) continue;
				if (!rover->bottom.copied && rover->flags&(FF_INVERTPLANES | FF_BOTHPLANES))
				{
					double ff_bottom = rover->bottom.plane->ZatPoint(sector->centerspot);
					if (ff_bottom > lastfloorheight || (rover->flags&FF_FIX))
					{
						if (r_viewpoint.Pos.Z >= rover->bottom.plane->ZatPoint(r_viewpoint.Pos))
						{
							SetFrom3DFloor(rover, false, !(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;

							if (rover->flags&FF_FIX)
							{
								lightlevel = hw_ClampLight(rover->model->lightlevel);
								Colormap = rover->GetColormap();
							}

							Process(di, rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
						}
						lastfloorheight = ff_bottom;
					}
				}
				if (!rover->top.copied && !(rover->flags&FF_INVERTPLANES))
				{
					double ff_top = rover->top.plane->ZatPoint(sector->centerspot);
					if (ff_top > lastfloorheight)
					{
						if (r_viewpoint.Pos.Z >= rover->top.plane->ZatPoint(r_viewpoint.Pos))
						{
							SetFrom3DFloor(rover, true, !!(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;
							Process(di, rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
						}
						lastfloorheight = ff_top;
						if (rover->alpha < 255) lastfloorheight -= EQUAL_EPSILON;
					}
				}
			}
		}
	}
}

