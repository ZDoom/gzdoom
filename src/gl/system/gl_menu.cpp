

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
CVAR(Bool, gl_render_segs, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
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
	//gl_render_segs=self;
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
	if (gl.shadermodel < 4)
	{
		// Radial fog and Doom lighting are not available in SM < 4 cards
		// The way they are implemented does not work well on older hardware.

		FOptionValues **opt = OptionValues.CheckKey("LightingModes");
		if (opt != NULL) 
		{
			for(int i = (*opt)->mValues.Size()-1; i>=0; i--)
			{
				// Delete 'Doom' lighting mode
				if ((*opt)->mValues[i].Value == 2.0 || (*opt)->mValues[i].Value == 8.0)
				{
					(*opt)->mValues.Delete(i);
				}
			}
		}

		opt = OptionValues.CheckKey("FogMode");
		if (opt != NULL) 
		{
			for(int i = (*opt)->mValues.Size()-1; i>=0; i--)
			{
				// Delete 'Radial' fog mode
				if ((*opt)->mValues[i].Value == 2.0)
				{
					(*opt)->mValues.Delete(i);
				}
			}
		}

		// disable features that don't work without shaders.
		if (gl_lightmode == 2 || gl_lightmode == 8) gl_lightmode = 3;
		if (gl_fogmode == 2) gl_fogmode = 1;
		if (gl_dynlight_shader) gl_dynlight_shader = false;
	}

	if (gl.shadermodel != 3)
	{
		// The shader menu will only be visible on SM3. 
		// SM2 won't use shaders unless unavoidable (and then it's automatic) and SM4 will always use shaders.
		// Find the OpenGLOptions menu and remove the item named GLShaderOptions.
		
		FMenuDescriptor **desc = MenuDescriptors.CheckKey("OpenGLOptions");
		if (desc != NULL && (*desc)->mType == MDESC_OptionsMenu)
		{
			FOptionMenuDescriptor *opt = (FOptionMenuDescriptor *)*desc;

			FName shader = "GLShaderOptions";
			for(unsigned i=0;i<opt->mItems.Size();i++)
			{
				FName nm = opt->mItems[i]->GetAction(NULL);
				if (nm == shader) 
				{
					delete opt->mItems[i];
					opt->mItems.Delete(i);
				}
			}
		}
	}
}
