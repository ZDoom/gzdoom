// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2014-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "gl_load/gl_system.h"
#include "c_cvars.h"

#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/system/gl_debug.h"
#include "gl/renderer/gl_renderer.h"
#include "gl_samplers.h"
#include "hwrenderer/textures/hw_material.h"

extern TexFilter_s TexFilter[];


FSamplerManager::FSamplerManager()
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

	for (int i = 0; i < 7; i++)
	{
		FString name;
		name.Format("mSamplers[%d]", i);
		FGLDebug::LabelObject(GL_SAMPLER, mSamplers[i], name.GetChars());
	}
}

FSamplerManager::~FSamplerManager()
{
	UnbindAll();
	glDeleteSamplers(7, mSamplers);
}

void FSamplerManager::UnbindAll()
{
	for (int i = 0; i < FHardwareTexture::MAX_TEXTURES; i++)
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
	int filter = V_IsHardwareRenderer() ? gl_texture_filter : 0;

	for (int i = 0; i < 4; i++)
	{
		glSamplerParameteri(mSamplers[i], GL_TEXTURE_MIN_FILTER, TexFilter[filter].minfilter);
		glSamplerParameteri(mSamplers[i], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
		glSamplerParameterf(mSamplers[i], GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
	}
	glSamplerParameteri(mSamplers[4], GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
	glSamplerParameteri(mSamplers[4], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
	glSamplerParameteri(mSamplers[6], GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
	glSamplerParameteri(mSamplers[6], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
}


