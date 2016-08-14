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
}

FSkyVertexBuffer::~FSkyVertexBuffer()
{
}

void FSkyVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	if (gl.glslversion > 0)
	{
		glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FSkyVertex), &VSO->x);
		glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FSkyVertex), &VSO->u);
		glVertexAttribPointer(VATTR_COLOR, 4, GL_UNSIGNED_BYTE, true, sizeof(FSkyVertex), &VSO->color);
		glEnableVertexAttribArray(VATTR_VERTEX);
		glEnableVertexAttribArray(VATTR_TEXCOORD);
		glEnableVertexAttribArray(VATTR_COLOR);
		glDisableVertexAttribArray(VATTR_VERTEX2);
	}
	else
	{
		glVertexPointer(3, GL_FLOAT, sizeof(FSkyVertex), &VSO->x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(FSkyVertex), &VSO->u);
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FSkyVertex), &VSO->color);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::SkyVertex(int r, int c, bool zflip)
{
	static const FAngle maxSideAngle = 60.f;
	static const float scale = 10000.;

	FAngle topAngle = (c / (float)mColumns * 360.f);
	FAngle sideAngle = maxSideAngle * (mRows - r) / mRows;
	float height = sideAngle.Sin();
	float realRadius = scale * sideAngle.Cos();
	FVector2 pos = topAngle.ToVector(realRadius);
	float z = (!zflip) ? scale * height : -scale * height;

	FSkyVertex vert;

	vert.color = r == 0 ? 0xffffff : 0xffffffff;

	// And the texture coordinates.
	if (!zflip)	// Flipped Y is for the lower hemisphere.
	{
		vert.u = (-c / (float)mColumns);
		vert.v = (r / (float)mRows);
	}
	else
	{
		vert.u = (-c / (float)mColumns);
		vert.v = 1.0f + ((mRows - r) / (float)mRows);
	}

	if (r != 4) z += 300;
	// And finally the vertex.
	vert.x = -pos.X;	// Doom mirrors the sky vertically!
	vert.y = z - 1.f;
	vert.z = pos.Y;

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
	bool zflip = !!(hemi & SKYHEMI_LOWER);

	mPrimStart.Push(mVertices.Size());

	for (c = 0; c < mColumns; c++)
	{
		SkyVertex(1, c, zflip);
	}

	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for (r = 0; r < mRows; r++)
	{
		mPrimStart.Push(mVertices.Size());
		for (c = 0; c <= mColumns; c++)
		{
			SkyVertex(r + zflip, c, zflip);
			SkyVertex(r + 1 - zflip, c, zflip);
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
	mVertices[0].Set(1.0f, 1.0f, -1.0f);
	mVertices[1].Set(1.0f, -1.0f, -1.0f);
	mVertices[2].Set(-1.0f, 0.0f, -1.0f);

	mVertices[3].Set(1.0f, 1.0f, -1.0f);
	mVertices[4].Set(1.0f, -1.0f, -1.0f);
	mVertices[5].Set(0.0f, 0.0f, 1.0f);

	mVertices[6].Set(-1.0f, 0.0f, -1.0f);
	mVertices[7].Set(1.0f, 1.0f, -1.0f);
	mVertices[8].Set(0.0f, 0.0f, 1.0f);

	mVertices[9].Set(1.0f, -1.0f, -1.0f);
	mVertices[10].Set(-1.0f, 0.0f, -1.0f);
	mVertices[11].Set(0.0f, 0.0f, 1.0f);

	mColumns = 128;
	mRows = 4;
	CreateSkyHemisphere(SKYHEMI_UPPER);
	CreateSkyHemisphere(SKYHEMI_LOWER);
	mPrimStart.Push(mVertices.Size());

	mSideStart = mVertices.Size();
	mFaceStart[0] = mSideStart + 10;
	mFaceStart[1] = mFaceStart[0] + 4;
	mFaceStart[2] = mFaceStart[1] + 4;
	mFaceStart[3] = mFaceStart[2] + 4;
	mFaceStart[4] = mFaceStart[3] + 4;
	mFaceStart[5] = mFaceStart[4] + 4;
	mFaceStart[6] = mFaceStart[5] + 4;
	mVertices.Reserve(10 + 7*4);
	FSkyVertex *ptr = &mVertices[mSideStart];

	// all sides
	ptr[0].SetXYZ(128.f, 128.f, -128.f, 0, 0);
	ptr[1].SetXYZ(128.f, -128.f, -128.f, 0, 1);
	ptr[2].SetXYZ(-128.f, 128.f, -128.f, 0.25f, 0);
	ptr[3].SetXYZ(-128.f, -128.f, -128.f, 0.25f, 1);
	ptr[4].SetXYZ(-128.f, 128.f, 128.f, 0.5f, 0);
	ptr[5].SetXYZ(-128.f, -128.f, 128.f, 0.5f, 1);
	ptr[6].SetXYZ(128.f, 128.f, 128.f, 0.75f, 0);
	ptr[7].SetXYZ(128.f, -128.f, 128.f, 0.75f, 1);
	ptr[8].SetXYZ(128.f, 128.f, -128.f, 1, 0);
	ptr[9].SetXYZ(128.f, -128.f, -128.f, 1, 1);

	// north face
	ptr[10].SetXYZ(128.f, 128.f, -128.f, 0, 0);
	ptr[11].SetXYZ(-128.f, 128.f, -128.f, 1, 0);
	ptr[12].SetXYZ(128.f, -128.f, -128.f, 0, 1);
	ptr[13].SetXYZ(-128.f, -128.f, -128.f, 1, 1);

	// east face
	ptr[14].SetXYZ(-128.f, 128.f, -128.f, 0, 0);
	ptr[15].SetXYZ(-128.f, 128.f, 128.f, 1, 0);
	ptr[16].SetXYZ(-128.f, -128.f, -128.f, 0, 1);
	ptr[17].SetXYZ(-128.f, -128.f, 128.f, 1, 1);

	// south face
	ptr[18].SetXYZ(-128.f, 128.f, 128.f, 0, 0);
	ptr[19].SetXYZ(128.f, 128.f, 128.f, 1, 0);
	ptr[20].SetXYZ(-128.f, -128.f, 128.f, 0, 1);
	ptr[21].SetXYZ(128.f, -128.f, 128.f, 1, 1);

	// west face
	ptr[22].SetXYZ(128.f, 128.f, 128.f, 0, 0);
	ptr[23].SetXYZ(128.f, 128.f, -128.f, 1, 0);
	ptr[24].SetXYZ(128.f, -128.f, 128.f, 0, 1);
	ptr[25].SetXYZ(128.f, -128.f, -128.f, 1, 1);

	// bottom face
	ptr[26].SetXYZ(128.f, -128.f, -128.f, 0, 0);
	ptr[27].SetXYZ(-128.f, -128.f, -128.f, 1, 0);
	ptr[28].SetXYZ(128.f, -128.f, 128.f, 0, 1);
	ptr[29].SetXYZ(-128.f, -128.f, 128.f, 1, 1);

	// top face
	ptr[30].SetXYZ(128.f, 128.f, -128.f, 0, 0);
	ptr[31].SetXYZ(-128.f, 128.f, -128.f, 1, 0);
	ptr[32].SetXYZ(128.f, 128.f, 128.f, 0, 1);
	ptr[33].SetXYZ(-128.f, 128.f, 128.f, 1, 1);

	// top face flipped
	ptr[34].SetXYZ(128.f, 128.f, -128.f, 0, 1);
	ptr[35].SetXYZ(-128.f, 128.f, -128.f, 1, 1);
	ptr[36].SetXYZ(128.f, 128.f, 128.f, 0, 0);
	ptr[37].SetXYZ(-128.f, 128.f, 128.f, 1, 0);

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
		// The color array can only be activated now if this is drawn without shader
		if (gl.glslversion == 0)
		{
			glEnableClientState(GL_COLOR_ARRAY);
		}
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
		gl_RenderState.mModelMatrix.rotate(-180.0f + x_offset, 0.f, 1.f, 0.f);

		float xscale = texw < 1024.f ? floor(1024.f / float(texw)) : 1.f;
		float yscale = 1.f;
		if (texh < 128)
		{
			// smaller sky textures must be tiled. We restrict it to 128 sky pixels, though
			gl_RenderState.mModelMatrix.translate(0.f, -1250.f, 0.f);
			gl_RenderState.mModelMatrix.scale(1.f, 128 / 230.f, 1.f);
			yscale = 128 / texh;	// intentionally left as integer.
		}
		else if (texh < 200)
		{
			gl_RenderState.mModelMatrix.translate(0.f, -1250.f, 0.f);
			gl_RenderState.mModelMatrix.scale(1.f, texh / 230.f, 1.f);
		}
		else if (texh <= 240)
		{
			gl_RenderState.mModelMatrix.translate(0.f, (200 - texh + tex->tex->SkyOffset + skyoffset)*skyoffsetfactor, 0.f);
			gl_RenderState.mModelMatrix.scale(1.f, 1.f + ((texh - 200.f) / 200.f) * 1.17f, 1.f);
		}
		else
		{
			gl_RenderState.mModelMatrix.translate(0.f, (-40 + tex->tex->SkyOffset + skyoffset)*skyoffsetfactor, 0.f);
			gl_RenderState.mModelMatrix.scale(1.f, 1.2f * 1.17f, 1.f);
			yscale = 240.f / texh;
		}
		gl_RenderState.EnableTextureMatrix(true);
		gl_RenderState.mTextureMatrix.loadIdentity();
		gl_RenderState.mTextureMatrix.scale(mirror ? -xscale : xscale, yscale, 1.f);
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

	if (sb->faces[5]) 
	{
		faces=4;

		// north
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(0), 4);

		// east
		tex = FMaterial::ValidateTexture(sb->faces[1], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(1), 4);

		// south
		tex = FMaterial::ValidateTexture(sb->faces[2], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(2), 4);

		// west
		tex = FMaterial::ValidateTexture(sb->faces[3], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(3), 4);
	}
	else 
	{
		faces=1;
		tex = FMaterial::ValidateTexture(sb->faces[0], false);
		gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
		gl_RenderState.Apply();
		glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(-1), 10);
	}

	// top
	tex = FMaterial::ValidateTexture(sb->faces[faces], false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
	gl_RenderState.Apply();
	glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(sb->fliptop? 6:5), 4);

	// bottom
	tex = FMaterial::ValidateTexture(sb->faces[faces+1], false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, 0, -1, false);
	gl_RenderState.Apply();
	glDrawArrays(GL_TRIANGLE_STRIP, GLRenderer->mSkyVBO->FaceStart(4), 4);

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
	GLRenderer->SetupView(0, 0, 0, ViewAngle, !!(MirrorFlag & 1), !!(PlaneMirrorFlag & 1));

	gl_RenderState.SetVertexBuffer(GLRenderer->mSkyVBO);
	if (origin->texture[0] && origin->texture[0]->tex->gl_info.bSkybox)
	{
		RenderBox(origin->skytexno1, origin->texture[0], origin->x_offset[0], origin->sky2);
	}
	else
	{
		if (origin->texture[0]==origin->texture[1] && origin->doublesky) origin->doublesky=false;	

		if (origin->texture[0])
		{
			gl_RenderState.SetTextureMode(TM_OPAQUE);
			RenderDome(origin->texture[0], origin->x_offset[0], origin->y_offset, origin->mirrored, FSkyVertexBuffer::SKYMODE_MAINLAYER);
			gl_RenderState.SetTextureMode(TM_MODULATE);
		}
		
		gl_RenderState.AlphaFunc(GL_GREATER, 0.f);
		
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
	}
	gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
	gl_MatrixStack.Pop(gl_RenderState.mViewMatrix);
	gl_RenderState.ApplyMatrices();
	glset.lightmode = oldlightmode;
	gl_RenderState.SetDepthClamp(oldClamp);
}

