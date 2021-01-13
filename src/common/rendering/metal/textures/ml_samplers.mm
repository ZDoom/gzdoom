//
//---------------------------------------------------------------------------
//
// Copyright(C) 2020-2021 Eugene Grigorchuk
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

#include "c_cvars.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/textures/hw_material.h"
#include "ml_samplers.h"
#include "metal/renderer/ml_renderstate.h"
#include "metal/renderer/ml_renderer.h"

namespace MetalRenderer
{

MetalTexFilter Filters[] =
{
    {MTLSamplerMinMagFilterNearest,   MTLSamplerMinMagFilterNearest,    false},
    {MTLSamplerMinMagFilterNearest,   MTLSamplerMinMagFilterNearest,    true },
    {MTLSamplerMinMagFilterLinear,    MTLSamplerMinMagFilterLinear,     false},
    {MTLSamplerMinMagFilterLinear,    MTLSamplerMinMagFilterLinear,     true },
    {MTLSamplerMinMagFilterLinear,    MTLSamplerMinMagFilterLinear,     true },
    {MTLSamplerMinMagFilterNearest,   MTLSamplerMinMagFilterNearest,    true },
    {MTLSamplerMinMagFilterLinear,    MTLSamplerMinMagFilterNearest,    true },
};

MTLSamplerManager::MTLSamplerManager(OBJC_ID(MTLDevice) device)
{
    mDevice = device;
    CreateDesc();
    SetTextureFilterMode();
    CreateSamplers();
    currentSampler = mSamplers[4];
    mRepeatMode = false;
}

void MTLSamplerManager::CreateSamplers()
{
    for (int i = 0; i < 7; i++)
    {
        mSamplers[i] = [mDevice newSamplerStateWithDescriptor:mDesc[i]];
    }
}

void MTLSamplerManager::CreateDesc()
{
    for (int i = 0; i < 7; i++)
    {
        mDesc[i] = [MTLSamplerDescriptor new];
    }
    
    mDesc[5].minFilter = MTLSamplerMinMagFilterNearest;
    mDesc[5].magFilter = MTLSamplerMinMagFilterNearest;
    mDesc[5].maxAnisotropy = 1.f;
    mDesc[4].maxAnisotropy = 1.f;
    mDesc[6].maxAnisotropy = 1.f;
    
    mDesc[1].sAddressMode = MTLSamplerAddressModeClampToEdge; //GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    mDesc[2].tAddressMode = MTLSamplerAddressModeClampToEdge; //GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    mDesc[3].sAddressMode = MTLSamplerAddressModeClampToEdge; //GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    mDesc[3].tAddressMode = MTLSamplerAddressModeClampToEdge;
    mDesc[3].rAddressMode = MTLSamplerAddressModeRepeat;//GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    mDesc[4].sAddressMode = MTLSamplerAddressModeClampToEdge; //GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    mDesc[4].tAddressMode = MTLSamplerAddressModeClampToEdge; //GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void MTLSamplerManager::DestroyDesc()
{
    for (int i = 0; i < 7; i++)
    {
        [mDesc[i] release];
    }
}

void MTLSamplerManager::DestroySamplers()
{
    for (int i = 0; i < 7; i++)
    {
        [mSamplers[i] release];
    }
}

void MTLSamplerManager::Destroy()
{
    DestroyDesc();
    DestroySamplers();
}

void MTLSamplerManager::SetTextureFilterMode()
{
    //DestroySamplers();
           
    int filter = V_IsHardwareRenderer() ? ml_texture_filter : 0;

    mDesc[0].rAddressMode = MTLSamplerAddressModeRepeat; //GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    mDesc[0].tAddressMode = MTLSamplerAddressModeRepeat; //GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    mDesc[0].sAddressMode = MTLSamplerAddressModeRepeat;
    mDesc[0].minFilter     = MTLSamplerMinMagFilterLinear;
    mDesc[0].magFilter     = MTLSamplerMinMagFilterLinear;
    mDesc[0].lodMinClamp = -1000.f;
    mDesc[0].lodMaxClamp = 1000.f;
    mDesc[0].maxAnisotropy = 8;
    
    mDesc[2].rAddressMode = MTLSamplerAddressModeRepeat; //GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    mDesc[2].tAddressMode = MTLSamplerAddressModeRepeat; //GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    mDesc[2].sAddressMode = MTLSamplerAddressModeRepeat;
    mDesc[2].lodMinClamp = -1000.f;
    mDesc[2].lodMaxClamp = 1000.f;
    mDesc[2].minFilter     = MTLSamplerMinMagFilterLinear;
    mDesc[2].magFilter     = MTLSamplerMinMagFilterLinear;
    mDesc[2].maxAnisotropy = 8;
    
    for (int i = 1; i < 4; i++)
    {
        mDesc[i].minFilter     = Filters[filter].minfilter;
        mDesc[i].magFilter     = Filters[filter].magfilter;
        mDesc[i].maxAnisotropy = ml_texture_filter_anisotropic;
        //glSamplerParameteri(mSamplers[i], GL_TEXTURE_MIN_FILTER, TexFilter[filter].minfilter);
        //glSamplerParameteri(mSamplers[i], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
        //glSamplerParameterf(mSamplers[i], GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
    }
    
    mDesc[4].minFilter = Filters[filter].minfilter;
    mDesc[4].magFilter = Filters[filter].magfilter;
    mDesc[6].minFilter = Filters[filter].minfilter;
    mDesc[6].magFilter = Filters[filter].magfilter;
    //glSamplerParameteri(mSamplers[4], GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
    //glSamplerParameteri(mSamplers[4], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);
    //glSamplerParameteri(mSamplers[6], GL_TEXTURE_MIN_FILTER, TexFilter[filter].magfilter);
    //glSamplerParameteri(mSamplers[6], GL_TEXTURE_MAG_FILTER, TexFilter[filter].magfilter);

    CreateSamplers();
}

void MTLSamplerManager::BindToShader(OBJC_ID(MTLRenderCommandEncoder) encoder)
{
    [encoder setFragmentSamplerState:currentSampler atIndex:3];
}

uint8_t MTLSamplerManager::Bind(int texunit, int num, int lastval)
{
    //printf("Bind Sampler -> %d\n", num);
    currentSampler =  mSamplers[num];
    mRepeatMode    = false;
    return 255;
}

void MTLSamplerManager::SetRepeatAddressMode(bool val)
{
    mRepeatMode = val;
}

}
