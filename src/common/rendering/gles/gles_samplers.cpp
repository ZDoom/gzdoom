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
#include "gles_system.h"
#include "c_cvars.h"

#include "hw_cvars.h"

#include "gles_renderer.h"
#include "gles_samplers.h"
#include "hw_material.h"
#include "i_interface.h"

namespace OpenGLESRenderer
{

extern TexFilter_s TexFilter[];


FSamplerManager::FSamplerManager()
{
	SetTextureFilterMode();
}

FSamplerManager::~FSamplerManager()
{

}

void FSamplerManager::UnbindAll()
{

}

uint8_t FSamplerManager::Bind(int texunit, int num, int lastval)
{

	int filter = sysCallbacks.DisableTextureFilter && sysCallbacks.DisableTextureFilter() ? 0 : gl_texture_filter;
	bool anisoAvailable = gles.anistropicFilterAvailable && (!sysCallbacks.DisableTextureFilter || !sysCallbacks.DisableTextureFilter());

	glActiveTexture(GL_TEXTURE0 + texunit);
	switch (num)
	{
	case CLAMP_NONE:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		if (lastval >= CLAMP_XY_NOMIP)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[filter].minfilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
			if (anisoAvailable)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
		}
		break;

	case CLAMP_X:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		if (lastval >= CLAMP_XY_NOMIP)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[filter].minfilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
			if (anisoAvailable)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
		}
		break;

	case CLAMP_Y:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (lastval >= CLAMP_XY_NOMIP)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[filter].minfilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
			if (anisoAvailable)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
		}
		break;

	case CLAMP_XY:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (lastval >= CLAMP_XY_NOMIP)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[filter].minfilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
			if (anisoAvailable)
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
		}
		break;

	case CLAMP_XY_NOMIP:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
		if (anisoAvailable)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);
		break;

	case CLAMP_NOFILTER:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (anisoAvailable)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);
		break;

	case CLAMP_NOFILTER_X:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (anisoAvailable)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);
		break;

	case CLAMP_NOFILTER_Y:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (anisoAvailable)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);
		break;

	case CLAMP_NOFILTER_XY:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (anisoAvailable)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);
		break;

	case CLAMP_CAMTEX:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
		if (anisoAvailable)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0);
		break;
	}
	glActiveTexture(GL_TEXTURE0);
	return 255;
}


void FSamplerManager::SetTextureFilterMode()
{
	/*
	GLRenderer->FlushTextures();

	GLint bounds[IHardwareTexture::MAX_TEXTURES];

	// Unbind all
	for(int i = IHardwareTexture::MAX_TEXTURES-1; i >= 0; i--)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glGetIntegerv(GL_SAMPLER_BINDING, &bounds[i]);
		glBindSampler(i, 0);
	}

	int filter = sysCallbacks.DisableTextureFilter && sysCallbacks.DisableTextureFilter() ? 0 : gl_texture_filter;

	for (int i = 0; i < 4; i++)
	{
		glSamplerParameteri(mSamplers[i], GL_TEXTURE_MIN_FILTER, TexFilter[filter].minfilter);
		glSamplerParameteri(mSamplers[i], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
		glSamplerParameterf(mSamplers[i], GL_TEXTURE_MAX_ANISOTROPY_EXT, filter > 0? gl_texture_filter_anisotropic : 1.0);
	}
	glSamplerParameteri(mSamplers[CLAMP_XY_NOMIP], GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
	glSamplerParameteri(mSamplers[CLAMP_XY_NOMIP], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
	glSamplerParameteri(mSamplers[CLAMP_CAMTEX], GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
	glSamplerParameteri(mSamplers[CLAMP_CAMTEX], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
	for(int i = 0; i < IHardwareTexture::MAX_TEXTURES; i++)
	{
		glBindSampler(i, bounds[i]);
	}
	*/
}


}