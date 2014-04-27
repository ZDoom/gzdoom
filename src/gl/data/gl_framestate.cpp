/*
** gl_framestate.cpp
** Encapsulates the per frame state in a uniform buffer so that this
** rarely changed data doesn't need to be tracked per shader.
**
**---------------------------------------------------------------------------
** Copyright 2014 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
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
#include "gl/data/gl_framestate.h"
#include "gl/data/gl_data.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/vsMathLib.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_colormap.h"
#include "m_fixed.h"
#include "r_utility.h"

//==========================================================================
//
// 
//
//==========================================================================

FFrameState::FFrameState()
{
	int bytesize = sizeof(FrameStateData);
	glGenBuffers(1, &mBufferId);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, mBufferId);
	glBufferStorage(GL_UNIFORM_BUFFER, bytesize, NULL, GL_DYNAMIC_STORAGE_BIT);
	bDSA = !!glewIsSupported("GL_EXT_direct_state_access");
	UpdateFor2D(false);
	memset(&mData, 0, sizeof(mData));
}

//==========================================================================
//
// 
//
//==========================================================================

FFrameState::~FFrameState()
{
	glDeleteBuffers(1, &mBufferId);
}

//==========================================================================
//
//  Gather the frame state from the global variables it is set in.
//
//==========================================================================

void FFrameState::UpdateFor3D()
{
	VSML.copy(mData.mViewMatrix, VSML.VIEW);
	VSML.copy(mData.mProjectionMatrix, VSML.PROJECTION);
	mData.mLightMode = glset.lightmode;
	mData.mFogMode = gl_fogmode;
	mData.mCameraPos[0] = FIXED2FLOAT(viewx);
	mData.mCameraPos[2] = FIXED2FLOAT(viewy);
	mData.mCameraPos[1] = FIXED2FLOAT(viewz);
	//mClipHeight is set directly from the portal code.

	if (gl_fixedcolormap > CM_TORCH)
	{
		mData.mFixedColormap = 2;
	}
	else if (gl_fixedcolormap == CM_LITE)
	{
		mData.mFixedColormap = 2;
	}
	else if (gl_fixedcolormap >= CM_FIRSTSPECIALCOLORMAP && gl_fixedcolormap < CM_MAXCOLORMAP)
	{
		mData.mFixedColormap = 1;
	}
	else
	{
		mData.mFixedColormap = 0;
	}

	//float mFixedColormapStart[4];
	//float mFixedColormapEnd[4];

	if (bDSA)
	{
		glNamedBufferSubDataEXT(mBufferId, 0, sizeof(FrameStateData), &mData);
	}
	else
	{
		glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameStateData), &mData);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FFrameState::UpdateFor2D(bool weapon)
{
	VSML.copy(mData.mViewMatrix, VSML.VIEW);
	VSML.copy(mData.mProjectionMatrix, VSML.PROJECTION);
	mData.mLightMode = 0;
	mData.mFogMode = 0;
	mData.mCameraPos[0] = mData.mCameraPos[1] = mData.mCameraPos[2] = 0.f;
	mData.mClipHeight = 0.f;

	// keep the fixed colormap settings from the 3D scene if this is for the HUD weapon
	if (!weapon)
	{
		mData.mFixedColormap = 0;
	}

	if (bDSA)
	{
		glNamedBufferSubDataEXT(mBufferId, 0, sizeof(FrameStateData), &mData);
	}
	else
	{
		glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameStateData), &mData);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FFrameState::UpdateViewMatrix()
{
	VSML.copy(mData.mViewMatrix, VSML.VIEW);
	if (bDSA)
	{
		glNamedBufferSubDataEXT(mBufferId, 0, sizeof(mData.mViewMatrix), &mData);
	}
	else
	{
		glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(mData.mViewMatrix), &mData);
	}
}

