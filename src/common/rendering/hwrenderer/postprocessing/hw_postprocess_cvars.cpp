/*
**  Postprocessing framework
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
*/

#include "hw_postprocess_cvars.h"
#include "v_video.h"

//==========================================================================
//
// CVARs
//
//==========================================================================
CVAR(Bool, gl_bloom, false, CVAR_ARCHIVE);
CUSTOM_CVAR(Float, gl_bloom_amount, 1.4f, CVAR_ARCHIVE)
{
	if (self < 0.1f) self = 0.1f;
}

CVAR(Float, gl_exposure_scale, 1.3f, CVAR_ARCHIVE)
CVAR(Float, gl_exposure_min, 0.35f, CVAR_ARCHIVE)
CVAR(Float, gl_exposure_base, 0.35f, CVAR_ARCHIVE)
CVAR(Float, gl_exposure_speed, 0.05f, CVAR_ARCHIVE)

CUSTOM_CVAR(Int, gl_tonemap, 0, CVAR_ARCHIVE)
{
	if (self < 0 || self > 5)
		self = 0;
}

CVAR(Bool, gl_lens, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR(Float, gl_lens_k, -0.12f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, gl_lens_kcube, 0.1f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, gl_lens_chromatic, 1.12f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Int, gl_fxaa, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self >= IFXAAShader::Count)
	{
		self = 0;
	}
}

CUSTOM_CVAR(Int, gl_ssao, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 3)
		self = 0;
}

CUSTOM_CVAR(Int, gl_ssao_portals, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
}

CVAR(Float, gl_ssao_strength, 0.7f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, gl_ssao_debug, 0, 0)
CVAR(Float, gl_ssao_bias, 0.2f, 0)
CVAR(Float, gl_ssao_radius, 80.0f, 0)
CUSTOM_CVAR(Float, gl_ssao_blur, 16.0f, 0)
{
	if (self < 0.1f) self = 0.1f;
}

CUSTOM_CVAR(Float, gl_ssao_exponent, 1.8f, 0)
{
	if (self < 0.1f) self = 0.1f;
}

CUSTOM_CVAR(Float, gl_paltonemap_powtable, 2.0f, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	screen->UpdatePalette();
}

CUSTOM_CVAR(Bool, gl_paltonemap_reverselookup, true, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	screen->UpdatePalette();
}

CVAR(Float, gl_menu_blur, -1.0f, CVAR_ARCHIVE)

