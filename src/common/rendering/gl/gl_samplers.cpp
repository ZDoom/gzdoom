/*
** gl_samplers.cpp
**
** Texture sampler handling
**
**---------------------------------------------------------------------------
** Copyright 2015-2019 Christoph Oelckers
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
#include "gl_system.h"
#include "c_cvars.h"

#include "gl_interface.h"
#include "hw_cvars.h"
#include "gl_debug.h"
#include "gl_renderer.h"
#include "gl_samplers.h"
#include "hw_material.h"
#include "i_interface.h"

namespace OpenGLRenderer
{

extern TexFilter_s TexFilter[];


FSamplerManager::FSamplerManager()
{
	glGenSamplers(NUMSAMPLERS, mSamplers);

	glSamplerParameteri(mSamplers[CLAMP_X], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(mSamplers[CLAMP_Y], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(mSamplers[CLAMP_XY], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(mSamplers[CLAMP_XY], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(mSamplers[CLAMP_NOFILTER_X], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(mSamplers[CLAMP_NOFILTER_Y], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(mSamplers[CLAMP_NOFILTER_XY], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(mSamplers[CLAMP_NOFILTER_XY], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	for (int i = CLAMP_NOFILTER; i <= CLAMP_NOFILTER_XY; i++)
	{
		glSamplerParameteri(mSamplers[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(mSamplers[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameterf(mSamplers[i], GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);

	}

	glSamplerParameteri(mSamplers[CLAMP_XY_NOMIP], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(mSamplers[CLAMP_XY_NOMIP], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameterf(mSamplers[CLAMP_XY_NOMIP], GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);
	glSamplerParameterf(mSamplers[CLAMP_CAMTEX], GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);

	SetTextureFilterMode();


	for (int i = 0; i < NUMSAMPLERS; i++)
	{
		FString name;
		name.Format("mSamplers[%d]", i);
		FGLDebug::LabelObject(GL_SAMPLER, mSamplers[i], name.GetChars());
	}
}

FSamplerManager::~FSamplerManager()
{
	UnbindAll();
	glDeleteSamplers(NUMSAMPLERS, mSamplers);
}

void FSamplerManager::UnbindAll()
{
	for (int i = 0; i < IHardwareTexture::MAX_TEXTURES; i++)
	{
		glBindSampler(i, 0);
	}
}
	
uint8_t FSamplerManager::Bind(int texunit, int num, int lastval)
{
	unsigned int samp = mSamplers[num];
	glBindSampler(texunit, samp);
	return 255;
}

	
void FSamplerManager::SetTextureFilterMode()
{
	UnbindAll();
	int filter = sysCallbacks && sysCallbacks->DisableTextureFilter && sysCallbacks->DisableTextureFilter() ? 0 : gl_texture_filter;

	for (int i = 0; i < 4; i++)
	{
		glSamplerParameteri(mSamplers[i], GL_TEXTURE_MIN_FILTER, TexFilter[filter].minfilter);
		glSamplerParameteri(mSamplers[i], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
		glSamplerParameterf(mSamplers[i], GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
	}
	glSamplerParameteri(mSamplers[CLAMP_XY_NOMIP], GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
	glSamplerParameteri(mSamplers[CLAMP_XY_NOMIP], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
	glSamplerParameteri(mSamplers[CLAMP_CAMTEX], GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
	glSamplerParameteri(mSamplers[CLAMP_CAMTEX], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
}


}