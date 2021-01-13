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

#include "templates.h"
#include "c_cvars.h"
#include "doomtype.h"
#include "r_data/colormaps.h"
#include "hwrenderer/textures/hw_material.h"

#include "hwrenderer/utility/hw_cvars.h"
#include "metal/textures/ml_samplers.h"
#include "metal/textures/ml_hwtexture.h"
#include "metal/system/ml_buffer.h"
#include "metal/system/ml_framebuffer.h"
#include "metal/renderer/ml_renderstate.h"


namespace MetalRenderer
{
MetalTexFilter texFilter[]=
{
    {MTLSamplerMinMagFilterNearest,   MTLSamplerMinMagFilterNearest,    false},
    {MTLSamplerMinMagFilterNearest,   MTLSamplerMinMagFilterNearest,    true },
    {MTLSamplerMinMagFilterLinear,    MTLSamplerMinMagFilterLinear,     false},
    {MTLSamplerMinMagFilterLinear,    MTLSamplerMinMagFilterLinear,     true },
    {MTLSamplerMinMagFilterLinear,    MTLSamplerMinMagFilterLinear,     true },
    {MTLSamplerMinMagFilterNearest,   MTLSamplerMinMagFilterNearest,    true },
    {MTLSamplerMinMagFilterLinear,    MTLSamplerMinMagFilterNearest,    true },
};

int TexFormat[]={
    MTLPixelFormatRGBA8Unorm,
    MTLPixelFormatRGBA8Unorm,
    MTLPixelFormatRGBA8Unorm,
    MTLPixelFormatRGBA8Unorm,
    // [BB] Added compressed texture formats.
    MTLPixelFormatBC1_RGBA,
    MTLPixelFormatBC1_RGBA,
    MTLPixelFormatBC2_RGBA,
    MTLPixelFormatBC3_RGBA,
};

MTLHardwareTexture::MTLHardwareTexture()
{
    for (int i = 0; i < STATE_TEXTURES_COUNT; i++)
        metalState[i].Id = -1;
    
    mBuffer = new MTLBuffer();
};

MTLHardwareTexture::~MTLHardwareTexture()
{
    UnbindAll();
    delete mBuffer;
};




//===========================================================================
//
//
//
//===========================================================================
void MTLHardwareTexture::AllocateBuffer(int w, int h, int texelsize)
{
    mlTextureBytes = texelsize;
    bufferpitch = w;
}


uint8_t* MTLHardwareTexture::MapBuffer()
{
    return (uint8_t*)mBuffer->GetData();
}

void MTLHardwareTexture::ResetAll()
{
    //[mTex release];
}

void MTLHardwareTexture::Reset(size_t id)
{
    //[mTex release];
}

int MTLHardwareTexture::Bind(int texunit, bool needmipmap)
{
    for (int i = 0; i < STATE_TEXTURES_COUNT; i++)
    {
        if (metalState[i].Id == texunit)
        {
            return i;
        }
    }
    return -1;
}

unsigned int MTLHardwareTexture::CreateWipeScreen(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name)
{
    //nameTex(name);
    int rh,rw;
    //int texformat = MTLPixelFormatBGRA8Unorm;//GL_RGBA8;// TexFormat[gl_texture_format];
    bool deletebuffer=false;

    rw = w;//GetTexDimension(w);
    rh = h;//GetTexDimension(h);

    if (!buffer)
    {
        // The texture must at least be initialized if no data is present.
        mipmapped = false;
        buffer=(unsigned char *)calloc(4,rw * (rh+1));
        deletebuffer=true;
        //texheight = -h;
    }
    else
    {
        if (rw < w || rh < h)
        {
            // The texture is larger than what the hardware can handle so scale it down.
            unsigned char * scaledbuffer=(unsigned char *)calloc(4,rw * (rh+1));
            if (scaledbuffer)
            {
                Resize(w, h, rw, rh, buffer, scaledbuffer);
                deletebuffer=true;
                buffer=scaledbuffer;
            }
        }
    }
    
    
    OBJC_ID(MTLTexture) tex;
    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.width = rw;
    desc.height = rh;
    desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
    desc.storageMode = MTLStorageModeManaged;
    desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2D;
    desc.sampleCount = 1;
    
    tex = [device newTextureWithDescriptor:desc];
    [desc release];
    
    if(buffer)
    {
        MTLRegion region = MTLRegionMake2D(0, 0, rw, rh);
        [tex replaceRegion:region mipmapLevel:0 withBytes:buffer bytesPerRow:(4*rw)];
    }


    if (deletebuffer && buffer)
        free(buffer);
    
    if (0)//mipmap)
    {
        mipmapped = true;
    }
    
    return 0;
    
}

unsigned int MTLHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name)
{
    CreateTexture(buffer,w,h,texunit,mipmap,name);
    return 1;
}

bool MTLHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, const char *name)
{
    //nameTex(name);
    int rh,rw;
    //int texformat = MTLPixelFormatBGRA8Unorm;//GL_RGBA8;// TexFormat[gl_texture_format];
    bool deletebuffer=false;

    rw = w;//GetTexDimension(w);
    rh = h;//GetTexDimension(h);

    if (!buffer)
    {
        // The texture must at least be initialized if no data is present.
        mipmapped = false;
        buffer=(unsigned char *)calloc(4,rw * (rh+1));
        deletebuffer=true;
        //texheight = -h;
    }
    else
    {
        if (rw < w || rh < h)
        {
            // The texture is larger than what the hardware can handle so scale it down.
            unsigned char * scaledbuffer=(unsigned char *)calloc(4,rw * (rh+1));
            if (scaledbuffer)
            {
                Resize(w, h, rw, rh, buffer, scaledbuffer);
                deletebuffer=true;
                buffer=scaledbuffer;
            }
        }
    }
    
    

    MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
    desc.width = rw;
    desc.height = rh;
    desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
    desc.storageMode = MTLStorageModeManaged;
    desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2D;
    desc.sampleCount = 1;
    
    int currentTexId = FindFreeTexIndex();
    
    if (currentTexId == -1)
        assert(true);
    
    metalState[currentTexId].mTextures = [device newTextureWithDescriptor:desc];
    metalState[currentTexId].Id = texunit;
    [desc release];
    
    if(buffer)
    {
        MTLRegion region = MTLRegionMake2D(0, 0, rw, rh);
        [metalState[currentTexId].mTextures replaceRegion:region mipmapLevel:0 withBytes:buffer bytesPerRow:(4*rw)];
    }


    if (deletebuffer && buffer)
        free(buffer);
    
    if (0)//mipmap)
    {
        mipmapped = true;
    }
    
    return currentTexId;
}

bool MTLHardwareTexture::BindOrCreate(FTexture *tex, int texunit, int clampmode, int translation, int flags, OBJC_ID(MTLRenderCommandEncoder) encoder)
{
    //if (texunit != -1)
    //{
    int usebright = false;

    if (translation <= 0)
    {
        translation = -translation;
    }
    else
    {
        auto remap = TranslationToTable(translation);
        translation = remap == nullptr ? 0 : remap->GetUniqueIndex();
    }

    bool needmipmap = (clampmode <= CLAMP_XY);
    int texid = Bind(texunit, needmipmap);
    // Bind it to the system.
    if (texid == -1)
    {
        int w = 0, h = 0;

        // Create this texture
        
        FTextureBuffer texbuffer;

        if (!tex->isHardwareCanvas())
        {
            texbuffer = tex->CreateTexBuffer(translation, flags | CTF_ProcessData);
            w = texbuffer.mWidth;
            h = texbuffer.mHeight;
        }
        else
        {
            w = tex->GetWidth();
            h = tex->GetHeight();
        }
        //tex.id or name
        texid = CreateTexture(texbuffer.mBuffer, w, h, texunit, needmipmap, /*translation,*/ "FHardwareTexture.BindOrCreate");
        //if (!CreateTexture(texbuffer.mBuffer, w, h, texunit, needmipmap, /*translation,*/ "FHardwareTexture.BindOrCreate"))
        //{
        //    // could not create texture
        //    return false;
        //}
    }
    if (tex->isHardwareCanvas())
        static_cast<FCanvasTexture*>(tex)->NeedUpdate();
    
    FImageSource * t = tex->GetImage();
    
    
    MLRenderer->mSamplerManager->Bind(texunit, clampmode, 255);
    if (encoder)
    {
        [encoder setFragmentTexture:metalState[texid].mTextures atIndex:1];
        MLRenderer->mSamplerManager->BindToShader(encoder);
    }
    return true;
}

void MTLHardwareTexture::UnbindAll()
{
    for (int i = 0; i < STATE_TEXTURES_COUNT; i++)
    {
        if (metalState[i].Id != -1)
        {
            metalState[i].Id = -1;
            [metalState[i].mTextures release];
        }
    }
}

void MTLHardwareTexture::Unbind(int texunit)
{
    for (int i = 0; i < STATE_TEXTURES_COUNT; i++)
    {
        if (metalState[i].Id == texunit)
        {
            metalState[i].Id = -1;
            [metalState[i].mTextures release];
        }
    }
}

}
