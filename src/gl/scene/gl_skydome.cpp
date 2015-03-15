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
//#include "gl/gl_intern.h"

#include "gl/system/gl_interface.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
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

CVAR(Float, skyoffset, 0, 0)	// for testing

extern int skyfog;

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyVertexBuffer::FSkyVertexBuffer()
{
	CreateDome();

	glBindVertexArray(vao_id);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FSkyVertex), &VSO->x);
	glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FSkyVertex), &VSO->u);
	glVertexAttribPointer(VATTR_COLOR, 4, GL_UNSIGNED_BYTE, true, sizeof(FSkyVertex), &VSO->color);
	glEnableVertexAttribArray(VATTR_VERTEX);
	glEnableVertexAttribArray(VATTR_TEXCOORD);
	glEnableVertexAttribArray(VATTR_COLOR);
	glBindVertexArray(0);

}

FSkyVertexBuffer::~FSkyVertexBuffer()
{
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::SkyVertex(int r, int c, bool yflip)
{
	static const angle_t maxSideAngle = ANGLE_180 / 3;
	static const fixed_t scale = 10000 << FRACBITS;

	angle_t topAngle= (angle_t)(c / (float)mColumns * ANGLE_MAX);
	angle_t sideAngle = maxSideAngle * (mRows - r) / mRows;
	fixed_t height = finesine[sideAngle>>ANGLETOFINESHIFT];
	fixed_t realRadius = FixedMul(scale, finecosine[sideAngle>>ANGLETOFINESHIFT]);
	fixed_t x = FixedMul(realRadius, finecosine[topAngle>>ANGLETOFINESHIFT]);
	fixed_t y = (!yflip) ? FixedMul(scale, height) : FixedMul(scale, height) * -1;
	fixed_t z = FixedMul(realRadius, finesine[topAngle>>ANGLETOFINESHIFT]);

	FSkyVertex vert;
	
	vert.color = r == 0 ? 0xffffff : 0xffffffff;
		
	// And the texture coordinates.
	if(!yflip)	// Flipped Y is for the lower hemisphere.
	{
		vert.u = (-c / (float)mColumns) ;
		vert.v = (r / (float)mRows);
	}
	else
	{
		vert.u = (-c / (float)mColumns);
		vert.v = 1.0f + ((mRows - r) / (float)mRows);
	}

	if (r != 4) y+=FRACUNIT*300;
	// And finally the vertex.
	vert.x =-FIXED2FLOAT(x);	// Doom mirrors the sky vertically!
	vert.y = FIXED2FLOAT(y) - 1.f;
	vert.z = FIXED2FLOAT(z);

	mVertices.Push(vert);
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::CreateSkyHemisphere(int hemi)
{
	int r, c;
	bool yflip = !!(hemi & SKYHEMI_LOWER);

	mPrimStart.Push(mVertices.Size());

	for (c = 0; c < mColumns; c++)
	{
		SkyVertex(1, c, yflip);
	}

	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for (r = 0; r < mRows; r++)
	{
		mPrimStart.Push(mVertices.Size());
		for (c = 0; c <= mColumns; c++)
		{
			SkyVertex(r + yflip, c, yflip);
			SkyVertex(r + 1 - yflip, c, yflip);
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::CreateDome()
{
	// the first thing we put into the buffer is the fog layer object which is just 4 triangles around the viewpoint.

	mVertices.Reserve(12);
	mVertices[0].Set( 1.0f,  1.0f, -1.0f);
	mVertices[1].Set( 1.0f, -1.0f, -1.0f);
	mVertices[2].Set(-1.0f,  0.0f, -1.0f);

	mVertices[3].Set( 1.0f,  1.0f, -1.0f);
	mVertices[4].Set( 1.0f, -1.0f, -1.0f);
	mVertices[5].Set( 0.0f,  0.0f,  1.0f);

	mVertices[6].Set(-1.0f, 0.0f, -1.0f);
	mVertices[7].Set( 1.0f, 1.0f, -1.0f);
	mVertices[8].Set( 0.0f, 0.0f,  1.0f);

	mVertices[9].Set(1.0f, -1.0f, -1.0f);
	mVertices[10].Set(-1.0f, 0.0f, -1.0f);
	mVertices[11].Set( 0.0f, 0.0f,  1.0f);

	mColumns = 128;
	mRows = 4;
	CreateSkyHemisphere(SKYHEMI_UPPER);
	CreateSkyHemisphere(SKYHEMI_LOWER);
	mPrimStart.Push(mVertices.Size());
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glBufferData(GL_ARRAY_BUFFER, mVertices.Size() * sizeof(FSkyVertex), &mVertices[0], GL_STATIC_DRAW);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

inline void FSkyVertexBuffer::RenderRow(int prim, int row)
{
	glDrawArrays(prim, mPrimStart[row], mPrimStart[row + 1] - mPrimStart[row]);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::RenderDome(FMaterial *tex, int mode)
{
	int rc = mRows + 1;

	// The caps only get drawn for the main layer but not for the overlay.
	if (mode == SKYMODE_MAINLAYER && tex != NULL)
	{
		PalEntry pe = tex->tex->GetSkyCapColor(false);
		gl_RenderState.SetObjectColor(pe);
		gl_RenderState.EnableTexture(false);
		gl_RenderState.Apply();
		RenderRow(GL_TRIANGLE_FAN, 0);

		pe = tex->tex->GetSkyCapColor(true);
		gl_RenderState.SetObjectColor(pe);
		gl_RenderState.Apply();
		RenderRow(GL_TRIANGLE_FAN, rc);
		gl_RenderState.EnableTexture(true);
	}
	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.Apply();
	for (int i = 1; i <= mRows; i++)
	{
		RenderRow(GL_TRIANGLE_STRIP, i);
		RenderRow(GL_TRIANGLE_STRIP, rc + i);
	}
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void RenderDome(FMaterial * tex, float x_offset, float y_offset, bool mirror, int mode)
{
	int texh = 0;
	int texw = 0;

	// 57 world units roughly represent one sky texel for the glTranslate call.
	const float skyoffsetfactor = 57;

	if (tex)
	{
		gl_RenderState.SetMaterial(tex, CLAMP_NONE, 0, -1, false);
		texw = tex->TextureWidth();
		texh = tex->TextureHeight();
		gl_RenderState.EnableModelMatrix(true);

		gl_RenderState.mModelMatrix.loadIdentity();
		gl_RenderState.mModelMatrix.rotate(-180.0f+x_offset, 0.f, 1.f, 0.f);

		float xscale = 1024.f / float(texw);
		float yscale = 1.f;
		if (texh < 128)
		{
			// smaller sky textures must be tiled. We restrict it to 128 sky pixels, though
			gl_RenderState.mModelMatrix.translate(0.f, -1250.f, 0.f);
			gl_RenderState.mModelMatrix.scale(1.f, 128/230.f, 1.f);
			yscale = 128 / texh;	// intentionally left as integer.
		}
		else if (texh < 200)
		{
			gl_RenderState.mModelMatrix.translate(0.f, -1250.f, 0.f);
			gl_RenderState.mModelMatrix.scale(1.f, texh/230.f, 1.f);
		}
		else if (texh <= 240)
		{
			gl_RenderState.mModelMatrix.translate(0.f, (200 - texh + tex->tex->SkyOffset + skyoffset)*skyoffsetfactor, 0.f);
			gl_RenderState.mModelMatrix.scale(1.f, 1.f + ((texh-200.f)/200.f) * 1.17f, 1.f);
		}
		else
		{
			gl_RenderState.mModelMatrix.translate(0.f, (-40 + tex->tex->SkyOffset + skyoffset)*skyoffsetfactor, 0.f);
			gl_RenderState.mModelMatrix.scale(1.f, 1.2f * 1.17f, 1.f);
			yscale = 240.f / texh;
		}
		gl_RenderState.EnableTextureMatrix(true);
		gl_RenderState.mTextureMatrix.loadIdentity();
		gl_RenderState.mTextureMatrix.scale(mirror? -xscale : xscale, yscale, 1.f);
		gl_RenderState.mTextureMatrix.translate(1.f, y_offset / texh, 1.f);
	}

	GLRenderer->mSkyVBO->RenderDome(tex, mode);
	gl_RenderState.EnableTextureMatrix(false);
	gl_RenderState.EnableModelMatrix(false);
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void RenderBox(FTextureID texno, FMaterial * gltex, float x_offset, bool sky2)
{
	FSkyBox * sb = static_cast<FSkyBox*>(gltex->tex);
	int faces;
	FMaterial * tex;

	gl_RenderState.EnableModelMatrix(true);
	gl_RenderState.mModelMatrix.loadIdentity();

	if (!sky2)
		gl_RenderState.mModelMatrix.rotate(-180.0f+x_offset, glset.skyrotatevector.X, glset.skyrotatevector.Z, glset.skyrotatevector.Y);
	else
		gl_RenderState.mModelMatrix.rotate(-180.0f+x_offset, glset.skyrotatevector2.X, glset.skyrotatevector2.Z, glset.skyrotatevector2.Y);

	FFlatVertex *ptr;
	if (sb->faces[5]) 
	{
		faces=4;

		// north
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();

		ptr = GLRenderer->mVBO->GetBuffer();
		ptr->Set(128.f, 128.f, -128.f, 0, 0);
		ptr++;
		ptr->Set(-128.f, 128.f, -128.f, 1, 0);
		ptr++;
		ptr->Set(128.f, -128.f, -128.f, 0, 1);
		ptr++;
		ptr->Set(-128.f, -128.f, -128.f, 1, 1);
		ptr++;
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);

		// east
		tex = FMaterial::ValidateTexture(sb->faces[1], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();

		ptr = GLRenderer->mVBO->GetBuffer();
		ptr->Set(-128.f, 128.f, -128.f, 0, 0);
		ptr++;
		ptr->Set(-128.f, 128.f, 128.f, 1, 0);
		ptr++;
		ptr->Set(-128.f, -128.f, -128.f, 0, 1);
		ptr++;
		ptr->Set(-128.f, -128.f, 128.f, 1, 1);
		ptr++;
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);

		// south
		tex = FMaterial::ValidateTexture(sb->faces[2], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();

		ptr = GLRenderer->mVBO->GetBuffer();
		ptr->Set(-128.f, 128.f, 128.f, 0, 0);
		ptr++;
		ptr->Set(128.f, 128.f, 128.f, 1, 0);
		ptr++;
		ptr->Set(-128.f, -128.f, 128.f, 0, 1);
		ptr++;
		ptr->Set(128.f, -128.f, 128.f, 1, 1);
		ptr++;
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);

		// west
		tex = FMaterial::ValidateTexture(sb->faces[3], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();

		ptr = GLRenderer->mVBO->GetBuffer();
		ptr->Set(128.f, 128.f, 128.f, 0, 0);
		ptr++;
		ptr->Set(128.f, 128.f, -128.f, 1, 0);
		ptr++;
		ptr->Set(128.f, -128.f, 128.f, 0, 1);
		ptr++;
		ptr->Set(128.f, -128.f, -128.f, 1, 1);
		ptr++;
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);
	}
	else 
	{
		faces=1;
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();

		ptr = GLRenderer->mVBO->GetBuffer();
		ptr->Set(128.f, 128.f, -128.f, 0, 0);
		ptr++;
		ptr->Set(128.f, -128.f, -128.f, 0, 1);
		ptr++;
		ptr->Set(-128.f, 128.f, -128.f, 0.25f, 0);
		ptr++;
		ptr->Set(-128.f, -128.f, -128.f, 0.25f, 1);
		ptr++;
		ptr->Set(-128.f, 128.f, 128.f, 0.5f, 0);
		ptr++;
		ptr->Set(-128.f, -128.f, 128.f, 0.5f, 1);
		ptr++;
		ptr->Set(128.f, 128.f, 128.f, 0.75f, 0);
		ptr++;
		ptr->Set(128.f, -128.f, 128.f, 0.75f, 1);
		ptr++;
		ptr->Set(128.f, 128.f, -128.f, 1, 0);
		ptr++;
		ptr->Set(128.f, -128.f, -128.f, 1, 1);
		ptr++;
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);
	}

	// top
	tex = FMaterial::ValidateTexture(sb->faces[faces], false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
	gl_RenderState.Apply();

	ptr = GLRenderer->mVBO->GetBuffer();
	ptr->Set(128.f, 128.f, -128.f, 0, sb->fliptop);
	ptr++;
	ptr->Set(-128.f, 128.f, -128.f, 1, sb->fliptop);
	ptr++;
	ptr->Set(128.f, 128.f, 128.f, 0, !sb->fliptop);
	ptr++;
	ptr->Set(-128.f, 128.f, 128.f, 1, !sb->fliptop);
	ptr++;
	GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);

	// bottom
	tex = FMaterial::ValidateTexture(sb->faces[faces+1], false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
	gl_RenderState.Apply();

	ptr = GLRenderer->mVBO->GetBuffer();
	ptr->Set(128.f, -128.f, -128.f, 0, 0);
	ptr++;
	ptr->Set(-128.f, -128.f, -128.f, 1, 0);
	ptr++;
	ptr->Set(128.f, -128.f, 128.f, 0, 1);
	ptr++;
	ptr->Set(-128.f, -128.f, 128.f, 1, 1);
	ptr++;
	GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_STRIP);
	gl_RenderState.EnableModelMatrix(false);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
void GLSkyPortal::DrawContents()
{
	bool drawBoth = false;

	// We have no use for Doom lighting special handling here, so disable it for this function.
	int oldlightmode = glset.lightmode;
	if (glset.lightmode == 8)
	{
		glset.lightmode = 2;
		gl_RenderState.SetSoftLightLevel(-1);
	}


	gl_RenderState.ResetColor();
	gl_RenderState.EnableFog(false);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	bool oldClamp = gl_RenderState.SetDepthClamp(true);

	gl_MatrixStack.Push(gl_RenderState.mViewMatrix);
	GLRenderer->SetupView(0, 0, 0, viewangle, !!(MirrorFlag&1), !!(PlaneMirrorFlag&1));

	if (origin->texture[0] && origin->texture[0]->tex->gl_info.bSkybox)
	{
		RenderBox(origin->skytexno1, origin->texture[0], origin->x_offset[0], origin->sky2);
	}
	else
	{
		gl_RenderState.SetVertexBuffer(GLRenderer->mSkyVBO);
		if (origin->texture[0]==origin->texture[1] && origin->doublesky) origin->doublesky=false;	

		if (origin->texture[0])
		{
			gl_RenderState.SetTextureMode(TM_OPAQUE);
			RenderDome(origin->texture[0], origin->x_offset[0], origin->y_offset, origin->mirrored, FSkyVertexBuffer::SKYMODE_MAINLAYER);
			gl_RenderState.SetTextureMode(TM_MODULATE);
		}
		
		gl_RenderState.AlphaFunc(GL_GEQUAL, 0.05f);
		
		if (origin->doublesky && origin->texture[1])
		{
			RenderDome(origin->texture[1], origin->x_offset[1], origin->y_offset, false, FSkyVertexBuffer::SKYMODE_SECONDLAYER);
		}

		if (skyfog>0 && gl_fixedcolormap == CM_DEFAULT && (origin->fadecolor & 0xffffff) != 0)
		{
			PalEntry FadeColor = origin->fadecolor;
			FadeColor.a = clamp<int>(skyfog, 0, 255);

			gl_RenderState.EnableTexture(false);
			gl_RenderState.SetObjectColor(FadeColor);
			gl_RenderState.Apply();
			glDrawArrays(GL_TRIANGLES, 0, 12);
			gl_RenderState.EnableTexture(true);
			gl_RenderState.SetObjectColor(0xffffffff);
		}
		gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
	}
	gl_MatrixStack.Pop(gl_RenderState.mViewMatrix);
	gl_RenderState.ApplyMatrices();
	glset.lightmode = oldlightmode;
	gl_RenderState.SetDepthClamp(oldClamp);
}

