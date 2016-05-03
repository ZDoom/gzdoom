/*
** gl_samplers.cpp
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
#include "templates.h"
#include "c_cvars.h"
#include "c_dispatch.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl_samplers.h"
#include "gl_material.h"

extern TexFilter_s TexFilter[];


FSamplerManager::FSamplerManager()
{
	if (gl.flags & RFL_SAMPLER_OBJECTS)
	{
		glGenSamplers(7, mSamplers);
		SetTextureFilterMode();
		glSamplerParameteri(mSamplers[5], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(mSamplers[5], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameterf(mSamplers[5], GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);
		glSamplerParameterf(mSamplers[4], GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);
		glSamplerParameterf(mSamplers[6], GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);

		glSamplerParameteri(mSamplers[1], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(mSamplers[2], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(mSamplers[3], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(mSamplers[3], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(mSamplers[4], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(mSamplers[4], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

}

FSamplerManager::~FSamplerManager()
{
	if (gl.flags & RFL_SAMPLER_OBJECTS)
	{
		UnbindAll();
		glDeleteSamplers(7, mSamplers);
	}
}

void FSamplerManager::UnbindAll()
{
	if (gl.flags & RFL_SAMPLER_OBJECTS)
	{
		for (int i = 0; i < FHardwareTexture::MAX_TEXTURES; i++)
		{
			glBindSampler(i, 0);
		}
	}
}
	
BYTE FSamplerManager::Bind(int texunit, int num, int lastval)
{
	if (gl.flags & RFL_SAMPLER_OBJECTS)
	{
		unsigned int samp = mSamplers[num];
		glBindSampler(texunit, samp);
		return 255;
	}
	else
	{
		glActiveTexture(GL_TEXTURE0 + texunit);
		switch (num)
		{
		case CLAMP_NONE:
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			if (lastval >= CLAMP_XY_NOMIP)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].minfilter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
			}
			break;

		case CLAMP_X:
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			if (lastval >= CLAMP_XY_NOMIP)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].minfilter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
			}
			break;

		case CLAMP_Y:
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			if (lastval >= CLAMP_XY_NOMIP)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].minfilter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
			}
			break;

		case CLAMP_XY:
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			if (lastval >= CLAMP_XY_NOMIP)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].minfilter);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
			}
			break;

		case CLAMP_XY_NOMIP:
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].magfilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
			break;

		case CLAMP_NOFILTER:
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
			break;

		case CLAMP_CAMTEX:
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].magfilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
			break;
		}
		glActiveTexture(GL_TEXTURE0);
		return num;
	}
}

	
void FSamplerManager::SetTextureFilterMode()
{
	if (gl.flags & RFL_SAMPLER_OBJECTS)
	{
		UnbindAll();

		for (int i = 0; i < 4; i++)
		{
			glSamplerParameteri(mSamplers[i], GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].minfilter);
			glSamplerParameteri(mSamplers[i], GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
			glSamplerParameterf(mSamplers[i], GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
		}
		glSamplerParameteri(mSamplers[4], GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].magfilter);
		glSamplerParameteri(mSamplers[4], GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
		glSamplerParameteri(mSamplers[6], GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].magfilter);
		glSamplerParameteri(mSamplers[6], GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
	}
	else
	{
		GLRenderer->FlushTextures();
	}
}


