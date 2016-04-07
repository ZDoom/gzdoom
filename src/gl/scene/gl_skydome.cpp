/*
** gl_sky.cpp
**
** Draws the sky.  Loosely based on the JDoom sky and the ZDoomGL 0.66.2 sky.
**
**---------------------------------------------------------------------------
** Copyright 2003 Tim Stump
** Copyright 2005 Christoph Oelckers
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
** 4. Full disclosure of the entire project's source code, except for third
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
#include "doomtype.h"
#include "g_level.h"
#include "sc_man.h"
#include "w_wad.h"
#include "r_state.h"
#include "r_utility.h"
//#include "gl/gl_intern.h"

#include "gl/system/gl_interface.h"
#include "gl/data/gl_data.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_bitmap.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_skyboxtexture.h"
#include "gl/textures/gl_material.h"


//-----------------------------------------------------------------------------
//
// Shamelessly lifted from Doomsday (written by Jaakko Keränen)
// also shamelessly lifted from ZDoomGL! ;)
//
//-----------------------------------------------------------------------------

CVAR (Int, gl_sky_detail, 16, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
EXTERN_CVAR (Bool, r_stretchsky)

extern int skyfog;

// The texture offset to be applied to the texture coordinates in SkyVertex().

static int rows, columns;	
static bool yflip;
static int texw;
static float yAdd;
static bool foglayer;
static bool secondlayer;
static float R,G,B;
static bool skymirror; 

#define SKYHEMI_UPPER		0x1
#define SKYHEMI_LOWER		0x2
#define SKYHEMI_JUST_CAP	0x4	// Just draw the top or bottom cap.


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void SkyVertex(int r, int c)
{
	static const FAngle maxSideAngle = 60.f;
	static const float scale = 10000.;

	FAngle topAngle= (c / (float)columns * 360.f);
	FAngle sideAngle = maxSideAngle * (rows - r) / rows;
	float height = sideAngle.Sin();
	float realRadius = scale * sideAngle.Cos();
	float x = realRadius * topAngle.Cos();
	float y = (!yflip) ? scale * height : -scale * height;
	float z = realRadius * topAngle.Sin();
	float color = r * 1.f / rows;
	float u, v;
	float timesRepeat;
	
	timesRepeat = (short)(4 * (256.f / texw));
	if (timesRepeat == 0.f) timesRepeat = 1.f;
	
	if (!foglayer)
	{
		gl_SetColor(255, 0, NULL, r==0? 0.0f : 1.0f);
		
		// And the texture coordinates.
		if(!yflip)	// Flipped Y is for the lower hemisphere.
		{
			u = (-timesRepeat * c / (float)columns) ;
			v = (r / (float)rows) + yAdd;
		}
		else
		{
			u = (-timesRepeat * c / (float)columns) ;
			v = 1.0f + ((rows-r)/(float)rows) + yAdd;
		}
		
		
		glTexCoord2f(skymirror? -u:u, v);
	}
	if (r != 4) y+=300;
	// And finally the vertex.
	glVertex3f(-x, y - 1.f, z);  // Doom mirrors the sky vertically!
}


//-----------------------------------------------------------------------------
//
// Hemi is Upper or Lower. Zero is not acceptable.
// The current texture is used. SKYHEMI_NO_TOPCAP can be used.
//
//-----------------------------------------------------------------------------

static void RenderSkyHemisphere(int hemi, bool mirror)
{
	int r, c;
	
	if (hemi & SKYHEMI_LOWER)
	{
		yflip = true;
	}
	else
	{
		yflip = false;
	}

	skymirror = mirror;
	
	// The top row (row 0) is the one that's faded out.
	// There must be at least 4 columns. The preferable number
	// is 4n, where n is 1, 2, 3... There should be at least
	// two rows because the first one is always faded.
	rows = 4;
	
	if (hemi & SKYHEMI_JUST_CAP)
	{
		return;
	}


	// Draw the cap as one solid color polygon
	if (!foglayer)
	{
		columns = 4 * (gl_sky_detail > 0 ? gl_sky_detail : 1);
		foglayer=true;
		gl_RenderState.EnableTexture(false);
		gl_RenderState.Apply(true);


		if (!secondlayer)
		{
			glColor3f(R, G ,B);
			glBegin(GL_TRIANGLE_FAN);
			for(c = 0; c < columns; c++)
			{
				SkyVertex(1, c);
			}
			glEnd();
		}

		gl_RenderState.EnableTexture(true);
		foglayer=false;
		gl_RenderState.Apply();
	}
	else
	{
		gl_RenderState.Apply(true);
		columns=4;	// no need to do more!
		glBegin(GL_TRIANGLE_FAN);
		for(c = 0; c < columns; c++)
		{
			SkyVertex(0, c);
		}
		glEnd();
	}
	
	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for(r = 0; r < rows; r++)
	{
		if (yflip)
		{
			glBegin(GL_TRIANGLE_STRIP);
            SkyVertex(r + 1, 0);
			SkyVertex(r, 0);
			for(c = 1; c <= columns; c++)
			{
				SkyVertex(r + 1, c);
				SkyVertex(r, c);
			}
			glEnd();
		}
		else
		{
			glBegin(GL_TRIANGLE_STRIP);
            SkyVertex(r, 0);
			SkyVertex(r + 1, 0);
			for(c = 1; c <= columns; c++)
			{
				SkyVertex(r, c);
				SkyVertex(r + 1, c);
			}
			glEnd();
		}
	}
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CVAR(Float, skyoffset, 0, 0)	// for testing

static void RenderDome(FTextureID texno, FMaterial * tex, float x_offset, float y_offset, bool mirror, int CM_Index)
{
	int texh = 0;
	bool texscale = false;

	// 57 worls units roughly represent one sky texel for the glTranslate call.
	const float skyoffsetfactor = 57;

	if (tex)
	{
		glPushMatrix();
		tex->Bind(CM_Index, 0, 0);
		texw = tex->TextureWidth(GLUSE_TEXTURE);
		texh = tex->TextureHeight(GLUSE_TEXTURE);

		glRotatef(-180.0f+x_offset, 0.f, 1.f, 0.f);
		yAdd = y_offset/texh;

		if (texh < 128)
		{
			// smaller sky textures must be tiled. We restrict it to 128 sky pixels, though
			glTranslatef(0.f, -1250.f, 0.f);
			glScalef(1.f, 128/230.f, 1.f);
			glMatrixMode(GL_TEXTURE);
			glPushMatrix();
			glLoadIdentity();
			glScalef(1.f, 128 / texh, 1.f); // intentionally left as integer.
			glMatrixMode(GL_MODELVIEW);
			texscale = true;
		}
		else if (texh < 200)
		{
			glTranslatef(0.f, -1250.f, 0.f);
			glScalef(1.f, texh/230.f, 1.f);
		}
		else if (texh <= 240)
		{
			glTranslatef(0.f, (200 - texh + tex->tex->SkyOffset + skyoffset)*skyoffsetfactor, 0.f);
			glScalef(1.f, 1.f + ((texh-200.f)/200.f) * 1.17f, 1.f);
		}
		else
		{
			glTranslatef(0.f, (-40 + tex->tex->SkyOffset + skyoffset)*skyoffsetfactor, 0.f);
			glScalef(1.f, 1.2f * 1.17f, 1.f);
			glMatrixMode(GL_TEXTURE);
			glPushMatrix();
			glLoadIdentity();
			glScalef(1.f, 240.f / texh, 1.f);
			glMatrixMode(GL_MODELVIEW);
			texscale = true;
		}
	}

	if (tex && !secondlayer) 
	{
		PalEntry pe = tex->tex->GetSkyCapColor(false);
		if (CM_Index!=CM_DEFAULT) ModifyPalette(&pe, &pe, CM_Index, 1);

		R=pe.r/255.0f;
		G=pe.g/255.0f;
		B=pe.b/255.0f;

		if (gl_fixedcolormap)
		{
			float rr, gg, bb;

			gl_GetLightColor(255, 0, NULL, &rr, &gg, &bb);
			R*=rr;
			G*=gg;
			B*=bb;
		}
	}

	RenderSkyHemisphere(SKYHEMI_UPPER, mirror);

	if (tex && !secondlayer) 
	{
		PalEntry pe = tex->tex->GetSkyCapColor(true);
		if (CM_Index!=CM_DEFAULT) ModifyPalette(&pe, &pe, CM_Index, 1);
		R=pe.r/255.0f;
		G=pe.g/255.0f;
		B=pe.b/255.0f;

		if (gl_fixedcolormap != CM_DEFAULT)
		{
			float rr,gg,bb;

			gl_GetLightColor(255, 0, NULL, &rr, &gg, &bb);
			R*=rr;
			G*=gg;
			B*=bb;
		}
	}

	RenderSkyHemisphere(SKYHEMI_LOWER, mirror);
	if (texscale)
	{
		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
	}
	if (tex) glPopMatrix();

}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void RenderBox(FTextureID texno, FMaterial * gltex, float x_offset, int CM_Index, bool sky2)
{
	FSkyBox * sb = static_cast<FSkyBox*>(gltex->tex);
	int faces;
	FMaterial * tex;

	if (!sky2)
		glRotatef(-180.0f+x_offset, glset.skyrotatevector.X, glset.skyrotatevector.Z, glset.skyrotatevector.Y);
	else
		glRotatef(-180.0f+x_offset, glset.skyrotatevector2.X, glset.skyrotatevector2.Z, glset.skyrotatevector2.Y);

	glColor3f(R, G ,B);

	if (sb->faces[5]) 
	{
		faces=4;

		// north
		tex = FMaterial::ValidateTexture(sb->faces[0]);
		tex->Bind(CM_Index, GLT_CLAMPX|GLT_CLAMPY, 0);
		gl_RenderState.Apply();
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f(0, 0);
		glVertex3f(128.f, 128.f, -128.f);
		glTexCoord2f(1, 0);
		glVertex3f(-128.f, 128.f, -128.f);
		glTexCoord2f(1, 1);
		glVertex3f(-128.f, -128.f, -128.f);
		glTexCoord2f(0, 1);
		glVertex3f(128.f, -128.f, -128.f);
		glEnd();

		// east
		tex = FMaterial::ValidateTexture(sb->faces[1]);
		tex->Bind(CM_Index, GLT_CLAMPX|GLT_CLAMPY, 0);
		gl_RenderState.Apply();
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f(0, 0);
		glVertex3f(-128.f, 128.f, -128.f);
		glTexCoord2f(1, 0);
		glVertex3f(-128.f, 128.f, 128.f);
		glTexCoord2f(1, 1);
		glVertex3f(-128.f, -128.f, 128.f);
		glTexCoord2f(0, 1);
		glVertex3f(-128.f, -128.f, -128.f);
		glEnd();

		// south
		tex = FMaterial::ValidateTexture(sb->faces[2]);
		tex->Bind(CM_Index, GLT_CLAMPX|GLT_CLAMPY, 0);
		gl_RenderState.Apply();
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f(0, 0);
		glVertex3f(-128.f, 128.f, 128.f);
		glTexCoord2f(1, 0);
		glVertex3f(128.f, 128.f, 128.f);
		glTexCoord2f(1, 1);
		glVertex3f(128.f, -128.f, 128.f);
		glTexCoord2f(0, 1);
		glVertex3f(-128.f, -128.f, 128.f);
		glEnd();

		// west
		tex = FMaterial::ValidateTexture(sb->faces[3]);
		tex->Bind(CM_Index, GLT_CLAMPX|GLT_CLAMPY, 0);
		gl_RenderState.Apply();
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f(0, 0);
		glVertex3f(128.f, 128.f, 128.f);
		glTexCoord2f(1, 0);
		glVertex3f(128.f, 128.f, -128.f);
		glTexCoord2f(1, 1);
		glVertex3f(128.f, -128.f, -128.f);
		glTexCoord2f(0, 1);
		glVertex3f(128.f, -128.f, 128.f);
		glEnd();
	}
	else 
	{
		faces=1;
		// all 4 sides
		tex = FMaterial::ValidateTexture(sb->faces[0]);
		tex->Bind(CM_Index, GLT_CLAMPX|GLT_CLAMPY, 0);

		gl_RenderState.Apply();
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f(0, 0);
		glVertex3f(128.f, 128.f, -128.f);
		glTexCoord2f(.25f, 0);
		glVertex3f(-128.f, 128.f, -128.f);
		glTexCoord2f(.25f, 1);
		glVertex3f(-128.f, -128.f, -128.f);
		glTexCoord2f(0, 1);
		glVertex3f(128.f, -128.f, -128.f);
		glEnd();

		// east
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f(.25f, 0);
		glVertex3f(-128.f, 128.f, -128.f);
		glTexCoord2f(.5f, 0);
		glVertex3f(-128.f, 128.f, 128.f);
		glTexCoord2f(.5f, 1);
		glVertex3f(-128.f, -128.f, 128.f);
		glTexCoord2f(.25f, 1);
		glVertex3f(-128.f, -128.f, -128.f);
		glEnd();

		// south
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f(.5f, 0);
		glVertex3f(-128.f, 128.f, 128.f);
		glTexCoord2f(.75f, 0);
		glVertex3f(128.f, 128.f, 128.f);
		glTexCoord2f(.75f, 1);
		glVertex3f(128.f, -128.f, 128.f);
		glTexCoord2f(.5f, 1);
		glVertex3f(-128.f, -128.f, 128.f);
		glEnd();

		// west
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f(.75f, 0);
		glVertex3f(128.f, 128.f, 128.f);
		glTexCoord2f(1, 0);
		glVertex3f(128.f, 128.f, -128.f);
		glTexCoord2f(1, 1);
		glVertex3f(128.f, -128.f, -128.f);
		glTexCoord2f(.75f, 1);
		glVertex3f(128.f, -128.f, 128.f);
		glEnd();
	}

	// top
	tex = FMaterial::ValidateTexture(sb->faces[faces]);
	tex->Bind(CM_Index, GLT_CLAMPX|GLT_CLAMPY, 0);
	gl_RenderState.Apply();
	glBegin(GL_TRIANGLE_FAN);
	if (!sb->fliptop)
	{
		glTexCoord2f(0, 0);
		glVertex3f(128.f, 128.f, -128.f);
		glTexCoord2f(1, 0);
		glVertex3f(-128.f, 128.f, -128.f);
		glTexCoord2f(1, 1);
		glVertex3f(-128.f, 128.f, 128.f);
		glTexCoord2f(0, 1);
		glVertex3f(128.f, 128.f, 128.f);
	}
	else
	{
		glTexCoord2f(0, 0);
		glVertex3f(128.f, 128.f, 128.f);
		glTexCoord2f(1, 0);
		glVertex3f(-128.f, 128.f, 128.f);
		glTexCoord2f(1, 1);
		glVertex3f(-128.f, 128.f, -128.f);
		glTexCoord2f(0, 1);
		glVertex3f(128.f, 128.f, -128.f);
	}
	glEnd();


	// bottom
	tex = FMaterial::ValidateTexture(sb->faces[faces+1]);
	tex->Bind(CM_Index, GLT_CLAMPX|GLT_CLAMPY, 0);
	gl_RenderState.Apply();
	glBegin(GL_TRIANGLE_FAN);
	glTexCoord2f(0, 0);
	glVertex3f(128.f, -128.f, -128.f);
	glTexCoord2f(1, 0);
	glVertex3f(-128.f, -128.f, -128.f);
	glTexCoord2f(1, 1);
	glVertex3f(-128.f, -128.f, 128.f);
	glTexCoord2f(0, 1);
	glVertex3f(128.f, -128.f, 128.f);
	glEnd();


}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
void GLSkyPortal::DrawContents()
{
	bool drawBoth = false;
	int CM_Index;
	PalEntry FadeColor(0,0,0,0);

	// We have no use for Doom lighting special handling here, so disable it for this function.
	int oldlightmode = glset.lightmode;
	if (glset.lightmode == 8) glset.lightmode = 2;


	if (gl_fixedcolormap) 
	{
		CM_Index=gl_fixedcolormap<CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size()? gl_fixedcolormap:CM_DEFAULT;
	}
	else 
	{
		CM_Index=CM_DEFAULT;
		FadeColor=origin->fadecolor;
	}

	gl_RenderState.EnableFog(false);
	gl_RenderState.EnableAlphaTest(false);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	GLRenderer->SetupView(0, 0, 0, ViewAngle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	if (origin->texture[0] && origin->texture[0]->tex->gl_info.bSkybox)
	{
		if (gl_fixedcolormap != CM_DEFAULT)
		{						
			float rr,gg,bb;

			gl_GetLightColor(255, 0, NULL, &rr, &gg, &bb);
			R=rr;
			G=gg;
			B=bb;
		}
		else R=G=B=1.f;

		RenderBox(origin->skytexno1, origin->texture[0], origin->x_offset[0], CM_Index, origin->sky2);
		gl_RenderState.EnableAlphaTest(true);
	}
	else
	{
		if (origin->texture[0]==origin->texture[1] && origin->doublesky) origin->doublesky=false;	

		if (origin->texture[0])
		{
			gl_RenderState.SetTextureMode(TM_OPAQUE);
			RenderDome(origin->skytexno1, origin->texture[0], origin->x_offset[0], origin->y_offset, origin->mirrored, CM_Index);
			gl_RenderState.SetTextureMode(TM_MODULATE);
		}
		
		gl_RenderState.EnableAlphaTest(true);
		gl_RenderState.AlphaFunc(GL_GEQUAL,0.05f);
		
		if (origin->doublesky && origin->texture[1])
		{
			secondlayer=true;
			RenderDome(FNullTextureID(), origin->texture[1], origin->x_offset[1], origin->y_offset, false, CM_Index);
			secondlayer=false;
		}

		if (skyfog>0 && (FadeColor.r ||FadeColor.g || FadeColor.b))
		{
			gl_RenderState.EnableTexture(false);
			foglayer=true;
			glColor4f(FadeColor.r/255.0f,FadeColor.g/255.0f,FadeColor.b/255.0f,skyfog/255.0f);
			RenderDome(FNullTextureID(), NULL, 0, 0, false, CM_DEFAULT);
			gl_RenderState.EnableTexture(true);
			foglayer=false;
		}
	}
	glPopMatrix();
	glset.lightmode = oldlightmode;
}

