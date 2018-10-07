// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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


#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "hw_cvars.h"
#include "hwrenderer/textures/hw_material.h"
#include "menu/menu.h"



// OpenGL stuff moved here
// GL related CVARs
CVAR(Bool, gl_portals, true, 0)
CVAR(Bool, gl_noquery, false, 0)
CVAR(Bool,gl_mirrors,true,0)	// This is for debugging only!
CVAR(Bool,gl_mirror_envmap, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR(Bool, gl_seamless, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Int, r_mirror_recursions,4,CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	if (self<0) self=0;
	if (self>10) self=10;
}
bool gl_plane_reflection_i;	// This is needed in a header that cannot include the CVAR stuff...
CUSTOM_CVAR(Bool, gl_plane_reflection, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	gl_plane_reflection_i = self;
}

CUSTOM_CVAR(Bool, gl_render_precise, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	gl_seamless=self;
}

CUSTOM_CVAR (Float, vid_brightness, 0.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma();
	}
}

CUSTOM_CVAR (Float, vid_contrast, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma();
	}
}

CUSTOM_CVAR (Float, vid_saturation, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma();
	}
}

CVAR(Int, gl_satformula, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

//==========================================================================
//
// Texture CVARs
//
//==========================================================================
CUSTOM_CVAR(Float,gl_texture_filter_anisotropic,8.0f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	screen->TextureFilterChanged();
}

CCMD(gl_flush)
{
	FMaterial::FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_filter, 4, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self < 0 || self > 6) self=4;
	screen->TextureFilterChanged();
}

CUSTOM_CVAR(Bool, gl_texture_usehires, true, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	FMaterial::FlushAll();
}

CVAR(Bool, gl_precache, false, CVAR_ARCHIVE)

CVAR(Bool, gl_trimsprites, true, CVAR_ARCHIVE);


//==========================================================================
//
// Sprite CVARs
//
//==========================================================================

CVAR(Bool, gl_usecolorblending, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, gl_spritebrightfog, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Bool, gl_sprite_blend, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR(Int, gl_spriteclip, 1, CVAR_ARCHIVE)
CVAR(Float, gl_sclipthreshold, 10.0, CVAR_ARCHIVE)
CVAR(Float, gl_sclipfactor, 1.8f, CVAR_ARCHIVE)
CVAR(Int, gl_particles_style, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // 0 = square, 1 = round, 2 = smooth
CVAR(Int, gl_billboard_mode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, gl_billboard_faces_camera, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, gl_billboard_particles, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, gl_enhanced_nv_stealth, 3, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Int, gl_fuzztype, 0, CVAR_ARCHIVE)
{
	if (self < 0 || self > 8) self = 0;
}

CUSTOM_CVAR(Int, gl_shadowmap_filter, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 8) self = 1;
}
