// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
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
/*
** gltexture.cpp
** Low level OpenGL texture handling. These classes are also
** containers for the various translations a texture can have.
**
*/

#include "gl_load/gl_system.h"
#include "templates.h"
#include "c_cvars.h"
#include "r_data/colormaps.h"
#include "hwrenderer/textures/hw_material.h"

#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/system/gl_debug.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/textures/gl_samplers.h"


TexFilter_s TexFilter[]={
	{GL_NEAREST,					GL_NEAREST,		false},
	{GL_NEAREST_MIPMAP_NEAREST,		GL_NEAREST,		true},
	{GL_LINEAR,						GL_LINEAR,		false},
	{GL_LINEAR_MIPMAP_NEAREST,		GL_LINEAR,		true},
	{GL_LINEAR_MIPMAP_LINEAR,		GL_LINEAR,		true},
	{GL_NEAREST_MIPMAP_LINEAR,		GL_NEAREST,		true},
	{GL_LINEAR_MIPMAP_LINEAR,		GL_NEAREST,		true},
};

int TexFormat[]={
	GL_RGBA8,
	GL_RGB5_A1,
	GL_RGBA4,
	GL_RGBA2,
	// [BB] Added compressed texture formats.
	GL_COMPRESSED_RGBA_ARB,
	GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
	GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
	GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
};



//===========================================================================
// 
//	Static texture data
//
//===========================================================================
unsigned int FHardwareTexture::lastbound[FHardwareTexture::MAX_TEXTURES];

//===========================================================================
// 
//	Quick'n dirty image rescaling.
//
// This will only be used when the source texture is larger than
// what the hardware can manage (extremely rare in Doom)
//
// Code taken from wxWidgets
//
//===========================================================================

struct BoxPrecalc
{
	int boxStart;
	int boxEnd;
};

static void ResampleBoxPrecalc(TArray<BoxPrecalc>& boxes, int oldDim)
{
	int newDim = boxes.Size();
	const double scale_factor_1 = double(oldDim) / newDim;
	const int scale_factor_2 = (int)(scale_factor_1 / 2);

	for (int dst = 0; dst < newDim; ++dst)
	{
		// Source pixel in the Y direction
		const int src_p = int(dst * scale_factor_1);

		BoxPrecalc& precalc = boxes[dst];
		precalc.boxStart = clamp<int>(int(src_p - scale_factor_1 / 2.0 + 1), 0, oldDim - 1);
		precalc.boxEnd = clamp<int>(MAX<int>(precalc.boxStart + 1, int(src_p + scale_factor_2)), 0, oldDim - 1);
	}
}

void FHardwareTexture::Resize(int swidth, int sheight, int width, int height, unsigned char *src_data, unsigned char *dst_data)
{

	// This function implements a simple pre-blur/box averaging method for
	// downsampling that gives reasonably smooth results To scale the image
	// down we will need to gather a grid of pixels of the size of the scale
	// factor in each direction and then do an averaging of the pixels.

	TArray<BoxPrecalc> vPrecalcs(height, true);
	TArray<BoxPrecalc> hPrecalcs(width, true);

	ResampleBoxPrecalc(vPrecalcs, sheight);
	ResampleBoxPrecalc(hPrecalcs, swidth);

	int averaged_pixels, averaged_alpha, src_pixel_index;
	double sum_r, sum_g, sum_b, sum_a;

	for (int y = 0; y < height; y++)         // Destination image - Y direction
	{
		// Source pixel in the Y direction
		const BoxPrecalc& vPrecalc = vPrecalcs[y];

		for (int x = 0; x < width; x++)      // Destination image - X direction
		{
			// Source pixel in the X direction
			const BoxPrecalc& hPrecalc = hPrecalcs[x];

			// Box of pixels to average
			averaged_pixels = 0;
			averaged_alpha = 0;
			sum_r = sum_g = sum_b = sum_a = 0.0;

			for (int j = vPrecalc.boxStart; j <= vPrecalc.boxEnd; ++j)
			{
				for (int i = hPrecalc.boxStart; i <= hPrecalc.boxEnd; ++i)
				{
					// Calculate the actual index in our source pixels
					src_pixel_index = j * swidth + i;

					int a = src_data[src_pixel_index * 4 + 3];
					if (a > 0)	// do not use color from fully transparent pixels
					{
						sum_r += src_data[src_pixel_index * 4 + 0];
						sum_g += src_data[src_pixel_index * 4 + 1];
						sum_b += src_data[src_pixel_index * 4 + 2];
						sum_a += a;
						averaged_pixels++;
					}
					averaged_alpha++;

				}
			}

			// Calculate the average from the sum and number of averaged pixels
			dst_data[0] = (unsigned char)xs_CRoundToInt(sum_r / averaged_pixels);
			dst_data[1] = (unsigned char)xs_CRoundToInt(sum_g / averaged_pixels);
			dst_data[2] = (unsigned char)xs_CRoundToInt(sum_b / averaged_pixels);
			dst_data[3] = (unsigned char)xs_CRoundToInt(sum_a / averaged_alpha);
			dst_data += 4;
		}
	}
}



//===========================================================================
// 
//	Loads the texture image into the hardware
//
// NOTE: For some strange reason I was unable to find the source buffer
// should be one line higher than the actual texture. I got extremely
// strange crashes deep inside the GL driver when I didn't do it!
//
//===========================================================================

unsigned int FHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name)
{
	int rh,rw;
	int texformat = GL_RGBA8;// TexFormat[gl_texture_format];
	bool deletebuffer=false;

	/*
	if (forcenocompression)
	{
		texformat = GL_RGBA8;
	}
	*/
	TranslatedTexture * glTex=GetTexID(translation);
	bool firstCall = glTex->glTexID == 0;
	if (firstCall) glGenTextures(1,&glTex->glTexID);
	if (texunit != 0) glActiveTexture(GL_TEXTURE0+texunit);
	glBindTexture(GL_TEXTURE_2D, glTex->glTexID);
	FGLDebug::LabelObject(GL_TEXTURE, glTex->glTexID, name);
	lastbound[texunit] = glTex->glTexID;

	rw = GetTexDimension(w);
	rh = GetTexDimension(h);
	if (glBufferID > 0)
	{
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		buffer = nullptr;
	}
	else if (!buffer)
	{
		// The texture must at least be initialized if no data is present.
		glTex->mipmapped = false;
		buffer=(unsigned char *)calloc(4,rw * (rh+1));
		deletebuffer=true;
		//texheight=-h;	
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
	// store the physical size.

	int sourcetype;
	if (glTextureBytes > 0)
	{
		if (glTextureBytes < 4) glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		static const int ITypes[] = { GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 };
		static const int STypes[] = { GL_RED, GL_RG, GL_BGR, GL_BGRA };

		texformat = ITypes[glTextureBytes - 1];
		sourcetype = STypes[glTextureBytes - 1];
	}
	else
	{
		sourcetype = GL_BGRA;
	}
	
	if (!firstCall && glBufferID > 0)
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rw, rh, sourcetype, GL_UNSIGNED_BYTE, buffer);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, texformat, rw, rh, 0, sourcetype, GL_UNSIGNED_BYTE, buffer);

	if (deletebuffer && buffer) free(buffer);
	else if (glBufferID)
	{
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	if (mipmap && TexFilter[gl_texture_filter].mipmapping)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
		glTex->mipmapped = true;
	}

	if (texunit != 0) glActiveTexture(GL_TEXTURE0);
	return glTex->glTexID;
}


//===========================================================================
// 
//
//
//===========================================================================
void FHardwareTexture::AllocateBuffer(int w, int h, int texelsize)
{
	int rw = GetTexDimension(w);
	int rh = GetTexDimension(h);
	if (texelsize < 1 || texelsize > 4) texelsize = 4;
	glTextureBytes = texelsize;
	if (rw == w || rh == h)
	{
		glGenBuffers(1, &glBufferID);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glBufferID);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, w*h*texelsize, nullptr, GL_STREAM_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
}


uint8_t *FHardwareTexture::MapBuffer()
{
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, glBufferID);
	return (uint8_t*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
}

//===========================================================================
// 
//	Creates a texture
//
//===========================================================================
FHardwareTexture::FHardwareTexture(bool nocompression) 
{
	forcenocompression = nocompression;

	glDefTex.glTexID = 0;
	glDefTex.translation = 0;
	glDefTex.mipmapped = false;
	glDepthID = 0;
}


//===========================================================================
// 
//	Deletes a texture id and unbinds it from the texture units
//
//===========================================================================
void FHardwareTexture::TranslatedTexture::Delete()
{
	if (glTexID != 0) 
	{
		for(int i = 0; i < MAX_TEXTURES; i++)
		{
			if (lastbound[i] == glTexID)
			{
				lastbound[i] = 0;
			}
		}
		glDeleteTextures(1, &glTexID);
		glTexID = 0;
		mipmapped = false;
	}
}

//===========================================================================
// 
//	Frees all associated resources
//
//===========================================================================
void FHardwareTexture::Clean(bool all)
{
	int cm_arraysize = CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size();

	if (all)
	{
		glDefTex.Delete();
	}
	for(unsigned int i=0;i<glTex_Translated.Size();i++)
	{
		glTex_Translated[i].Delete();
	}
	glTex_Translated.Clear();
	if (glDepthID != 0) glDeleteRenderbuffers(1, &glDepthID);
	glDepthID = 0;
}

//===========================================================================
// 
// Deletes all allocated resources and considers translations
// This will only be called for sprites
//
//===========================================================================

void FHardwareTexture::CleanUnused(SpriteHits &usedtranslations)
{
	if (usedtranslations.CheckKey(0) == nullptr)
	{
		glDefTex.Delete();
	}
	for (int i = glTex_Translated.Size()-1; i>= 0; i--)
	{
		if (usedtranslations.CheckKey(glTex_Translated[i].translation) == nullptr)
		{
			glTex_Translated[i].Delete();
			glTex_Translated.Delete(i);
		}
	}
}

//===========================================================================
// 
//	Destroys the texture
//
//===========================================================================
FHardwareTexture::~FHardwareTexture() 
{ 
	Clean(true); 
	glDeleteBuffers(1, &glBufferID);
}


//===========================================================================
// 
//	Gets a texture ID address and validates all required data
//
//===========================================================================

FHardwareTexture::TranslatedTexture *FHardwareTexture::GetTexID(int translation)
{
	if (translation == 0)
	{
		return &glDefTex;
	}

	// normally there aren't more than very few different 
	// translations here so this isn't performance critical.
	for (unsigned int i = 0; i < glTex_Translated.Size(); i++)
	{
		if (glTex_Translated[i].translation == translation)
		{
			return &glTex_Translated[i];
		}
	}

	int add = glTex_Translated.Reserve(1);
	glTex_Translated[add].translation = translation;
	glTex_Translated[add].glTexID = 0;
	glTex_Translated[add].mipmapped = false;
	return &glTex_Translated[add];
}

//===========================================================================
// 
//	Binds this patch
//
//===========================================================================
unsigned int FHardwareTexture::Bind(int texunit, int translation, bool needmipmap)
{
	TranslatedTexture *pTex = GetTexID(translation);

	if (pTex->glTexID != 0)
	{
		if (lastbound[texunit] == pTex->glTexID) return pTex->glTexID;
		lastbound[texunit] = pTex->glTexID;
		if (texunit != 0) glActiveTexture(GL_TEXTURE0 + texunit);
		glBindTexture(GL_TEXTURE_2D, pTex->glTexID);
		// Check if we need mipmaps on a texture that was creted without them.
		if (needmipmap && !pTex->mipmapped && TexFilter[gl_texture_filter].mipmapping)
		{
			glGenerateMipmap(GL_TEXTURE_2D);
			pTex->mipmapped = true;
		}
		if (texunit != 0) glActiveTexture(GL_TEXTURE0);
		return pTex->glTexID;
	}
	return 0;
}

unsigned int FHardwareTexture::GetTextureHandle(int translation)
{
	TranslatedTexture *pTex = GetTexID(translation);
	return pTex->glTexID;
}

void FHardwareTexture::Unbind(int texunit)
{
	if (lastbound[texunit] != 0)
	{
		if (texunit != 0) glActiveTexture(GL_TEXTURE0+texunit);
		glBindTexture(GL_TEXTURE_2D, 0);
		if (texunit != 0) glActiveTexture(GL_TEXTURE0);
		lastbound[texunit] = 0;
	}
}

void FHardwareTexture::UnbindAll()
{
	for(int texunit = 0; texunit < 16; texunit++)
	{
		Unbind(texunit);
	}
	gl_RenderState.ClearLastMaterial();
}

//===========================================================================
// 
//	Creates a depth buffer for this texture
//
//===========================================================================

int FHardwareTexture::GetDepthBuffer(int width, int height)
{
	if (glDepthID == 0)
	{
		glGenRenderbuffers(1, &glDepthID);
		glBindRenderbuffer(GL_RENDERBUFFER, glDepthID);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 
			GetTexDimension(width), GetTexDimension(height));
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	return glDepthID;
}


//===========================================================================
// 
//	Binds this texture's surfaces to the current framrbuffer
//
//===========================================================================

void FHardwareTexture::BindToFrameBuffer(int width, int height)
{
	width = GetTexDimension(width);
	height = GetTexDimension(height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glDefTex.glTexID, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GetDepthBuffer(width, height));
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, GetDepthBuffer(width, height));
}


//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

bool FHardwareTexture::BindOrCreate(FTexture *tex, int texunit, int clampmode, int translation, int flags)
{
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

	// Bind it to the system.
	if (!Bind(texunit, translation, needmipmap))
	{

		int w = 0, h = 0;

		// Create this texture
		unsigned char * buffer = nullptr;

		if (!tex->bHasCanvas)
		{
			buffer = tex->CreateTexBuffer(translation, w, h, flags | CTF_ProcessData);
		}
		else
		{
			w = tex->GetWidth();
			h = tex->GetHeight();
		}
		if (!CreateTexture(buffer, w, h, texunit, needmipmap, translation, "FHardwareTexture.BindOrCreate"))
		{
			// could not create texture
			delete[] buffer;
			return false;
		}
		delete[] buffer;
	}
	if (tex->bHasCanvas) static_cast<FCanvasTexture*>(tex)->NeedUpdate();
	GLRenderer->mSamplerManager->Bind(texunit, clampmode, 255);
	return true;
}

