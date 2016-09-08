

#include "gl/system/gl_system.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "version.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
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
		screen->SetGamma(Gamma); //Brightness (self);
	}
}

CUSTOM_CVAR (Float, vid_contrast, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma(Gamma); //SetContrast (self);
	}
}


// Do some tinkering with the menus so that certain options only appear
// when they are actually valid.
void gl_SetupMenu()
{
#ifndef HAVE_MMX
	FOptionValues **opt = OptionValues.CheckKey("HqResizeModes");
	if (opt != NULL) 
	{
		for(int i = (*opt)->mValues.Size()-1; i>=0; i--)
		{
			// Delete HQnX resize modes for non MSVC targets
			if ((*opt)->mValues[i].Value >= 7.0)
			{
				(*opt)->mValues.Delete(i);
			}
		}
	}
#endif
}
