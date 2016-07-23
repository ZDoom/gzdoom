/*
** Global texture data
**
**---------------------------------------------------------------------------
** Copyright 2004-2009 Christoph Oelckers
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
#include "c_cvars.h"
#include "w_wad.h"
#include "templates.h"
#include "colormatcher.h"
#include "r_data/r_translate.h"
#include "c_dispatch.h"
#ifdef _WIN32
#include "win32gliface.h"
#endif
#include "v_palette.h"
#include "sc_man.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_material.h"
#include "gl/textures/gl_samplers.h"

//==========================================================================
//
// Texture CVARs
//
//==========================================================================
CUSTOM_CVAR(Float,gl_texture_filter_anisotropic,8.0f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (GLRenderer != NULL && GLRenderer->mSamplerManager != NULL) GLRenderer->mSamplerManager->SetTextureFilterMode();
}

CCMD(gl_flush)
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

CUSTOM_CVAR(Int, gl_texture_filter, 4, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self < 0 || self > 6) self=4;
	if (GLRenderer != NULL && GLRenderer->mSamplerManager != NULL) GLRenderer->mSamplerManager->SetTextureFilterMode();
}

CUSTOM_CVAR(Int, gl_texture_format, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	// [BB] The number of available texture modes depends on the GPU capabilities.
	// RFL_TEXTURE_COMPRESSION gives us one additional mode and RFL_TEXTURE_COMPRESSION_S3TC
	// another three.
	int numOfAvailableTextureFormat = 4;
	if ( gl.flags & RFL_TEXTURE_COMPRESSION && gl.flags & RFL_TEXTURE_COMPRESSION_S3TC )
		numOfAvailableTextureFormat = 8;
	else if ( gl.flags & RFL_TEXTURE_COMPRESSION )
		numOfAvailableTextureFormat = 5;
	if (self < 0 || self > numOfAvailableTextureFormat-1) self=0;
	GLRenderer->FlushTextures();
}

CUSTOM_CVAR(Bool, gl_texture_usehires, true, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

CVAR(Bool, gl_precache, false, CVAR_ARCHIVE)

CVAR(Bool, gl_trimsprites, true, CVAR_ARCHIVE);

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



bool HasGlobalBrightmap;
FRemapTable GlobalBrightmap;

//===========================================================================
// 
// Examines the colormap to see if some of the colors have to be
// considered fullbright all the time.
//
//===========================================================================

void gl_GenerateGlobalBrightmapFromColormap()
{
	HasGlobalBrightmap = false;
	int lump = Wads.CheckNumForName("COLORMAP");
	if (lump == -1) lump = Wads.CheckNumForName("COLORMAP", ns_colormaps);
	if (lump == -1) return;
	FMemLump cmap = Wads.ReadLump(lump);
	FMemLump palette = Wads.ReadLump("PLAYPAL");
	const unsigned char *cmapdata = (const unsigned char *)cmap.GetMem();
	const unsigned char *paldata = (const unsigned char *)palette.GetMem();

	const int black = 0;
	const int white = ColorMatcher.Pick(255,255,255);


	GlobalBrightmap.MakeIdentity();
	memset(GlobalBrightmap.Remap, white, 256);
	for(int i=0;i<256;i++) GlobalBrightmap.Palette[i]=PalEntry(255,255,255,255);
	for(int j=0;j<32;j++)
	{
		for(int i=0;i<256;i++)
		{
			// the palette comparison should be for ==0 but that gives false positives with Heretic
			// and Hexen.
			if (cmapdata[i+j*256]!=i || (paldata[3*i]<10 && paldata[3*i+1]<10 && paldata[3*i+2]<10))
			{
				GlobalBrightmap.Remap[i]=black;
				GlobalBrightmap.Palette[i] = PalEntry(255, 0, 0, 0);
			}
		}
	}
	for(int i=0;i<256;i++)
	{
		HasGlobalBrightmap |= GlobalBrightmap.Remap[i] == white;
		if (GlobalBrightmap.Remap[i] == white) DPrintf("Marked color %d as fullbright\n",i);
	}
}

//===========================================================================
//
// averageColor
//  input is RGBA8 pixel format.
//	The resulting RGB color can be scaled uniformly so that the highest 
//	component becomes one.
//
//===========================================================================
PalEntry averageColor(const DWORD *data, int size, int maxout)
{
	int				i;
	unsigned int	r, g, b;



	// First clear them.
	r = g = b = 0;
	if (size==0) 
	{
		return PalEntry(255,255,255);
	}
	for(i = 0; i < size; i++)
	{
		r += BPART(data[i]);
		g += GPART(data[i]);
		b += RPART(data[i]);
	}

	r = r/size;
	g = g/size;
	b = b/size;

	int maxv=MAX(MAX(r,g),b);

	if(maxv && maxout)
	{
		r = Scale(r, maxout, maxv);
		g = Scale(g, maxout, maxv);
		b = Scale(b, maxout, maxv);
	}
	return PalEntry(255,r,g,b);
}



//==========================================================================
//
// GL status data for a texture
//
//==========================================================================

FTexture::MiscGLInfo::MiscGLInfo() throw()
{
	bGlowing = false;
	GlowColor = 0;
	GlowHeight = 128;
	bSkybox = false;
	FloorSkyColor = 0;
	CeilingSkyColor = 0;
	bFullbright = false;
	bSkyColorDone = false;
	bBrightmapChecked = false;
	bDisableFullbright = false;
	bNoFilter = false;
	bNoCompress = false;
	bNoExpand = false;
	areas = NULL;
	areacount = 0;
	mIsTransparent = -1;
	shaderspeed = 1.f;
	shaderindex = 0;

	Material[1] = Material[0] = NULL;
	SystemTexture[1] = SystemTexture[0] = NULL;
	Brightmap = NULL;
}

FTexture::MiscGLInfo::~MiscGLInfo()
{
	for (int i = 0; i < 2; i++)
	{
		if (Material[i] != NULL) delete Material[i];
		Material[i] = NULL;

		if (SystemTexture[i] != NULL) delete SystemTexture[i];
		SystemTexture[i] = NULL;
	}

	// this is just a reference to another texture in the texture manager.
	Brightmap = NULL;

	if (areas != NULL) delete [] areas;
	areas = NULL;
}

//===========================================================================
// 
// Checks if the texture has a default brightmap and creates it if so
//
//===========================================================================
void FTexture::CreateDefaultBrightmap()
{
	if (!gl_info.bBrightmapChecked)
	{
		// Check for brightmaps
		if (UseBasePalette() && HasGlobalBrightmap &&
			UseType != TEX_Decal && UseType != TEX_MiscPatch && UseType != TEX_FontChar &&
			gl_info.Brightmap == NULL && bWarped == 0
			) 
		{
			// May have one - let's check when we use this texture
			const BYTE *texbuf = GetPixels();
			const int white = ColorMatcher.Pick(255,255,255);

			int size = GetWidth() * GetHeight();
			for(int i=0;i<size;i++)
			{
				if (GlobalBrightmap.Remap[texbuf[i]] == white)
				{
					// Create a brightmap
					DPrintf("brightmap created for texture '%s'\n", Name.GetChars());
					gl_info.Brightmap = new FBrightmapTexture(this);
					gl_info.bBrightmapChecked = 1;
					TexMan.AddTexture(gl_info.Brightmap);
					return;
				}
			}
			// No bright pixels found
			DPrintf("No bright pixels found in texture '%s'\n", Name.GetChars());
			gl_info.bBrightmapChecked = 1;
		}
		else
		{
			// does not have one so set the flag to 'done'
			gl_info.bBrightmapChecked = 1;
		}
	}
}


//==========================================================================
//
// Calculates glow color for a texture
//
//==========================================================================

void FTexture::GetGlowColor(float *data)
{
	if (gl_info.bGlowing && gl_info.GlowColor == 0)
	{
		int w, h;
		unsigned char *buffer = GLRenderer->GetTextureBuffer(this, w, h);

		if (buffer)
		{
			gl_info.GlowColor = averageColor((DWORD *) buffer, w*h, 153);
			delete[] buffer;
		}

		// Black glow equals nothing so switch glowing off
		if (gl_info.GlowColor == 0) gl_info.bGlowing = false;
	}
	data[0]=gl_info.GlowColor.r/255.0f;
	data[1]=gl_info.GlowColor.g/255.0f;
	data[2]=gl_info.GlowColor.b/255.0f;
}

//===========================================================================
// 
//	Gets the average color of a texture for use as a sky cap color
//
//===========================================================================

PalEntry FTexture::GetSkyCapColor(bool bottom)
{
	PalEntry col;
	int w;
	int h;

	if (!gl_info.bSkyColorDone)
	{
		gl_info.bSkyColorDone = true;

		unsigned char *buffer = GLRenderer->GetTextureBuffer(this, w, h);

		if (buffer)
		{
			gl_info.CeilingSkyColor = averageColor((DWORD *) buffer, w * MIN(30, h), 0);
			if (h>30)
			{
				gl_info.FloorSkyColor = averageColor(((DWORD *) buffer)+(h-30)*w, w * 30, 0);
			}
			else gl_info.FloorSkyColor = gl_info.CeilingSkyColor;
			delete[] buffer;
		}
	}
	return bottom? gl_info.FloorSkyColor : gl_info.CeilingSkyColor;
}

//===========================================================================
// 
//	Finds gaps in the texture which can be skipped by the renderer
//  This was mainly added to speed up one area in E4M6 of 007LTSD
//
//===========================================================================

bool FTexture::FindHoles(const unsigned char * buffer, int w, int h)
{
	const unsigned char * li;
	int y,x;
	int startdraw,lendraw;
	int gaps[5][2];
	int gapc=0;


	// already done!
	if (gl_info.areacount) return false;
	if (UseType == TEX_Flat) return false;	// flats don't have transparent parts
	gl_info.areacount=-1;	//whatever happens next, it shouldn't be done twice!

	// large textures are excluded for performance reasons
	if (h>512) return false;	

	startdraw=-1;
	lendraw=0;
	for(y=0;y<h;y++)
	{
		li=buffer+w*y*4+3;

		for(x=0;x<w;x++,li+=4)
		{
			if (*li!=0) break;
		}

		if (x!=w)
		{
			// non - transparent
			if (startdraw==-1) 
			{
				startdraw=y;
				// merge transparent gaps of less than 16 pixels into the last drawing block
				if (gapc && y<=gaps[gapc-1][0]+gaps[gapc-1][1]+16)
				{
					gapc--;
					startdraw=gaps[gapc][0];
					lendraw=y-startdraw;
				}
				if (gapc==4) return false;	// too many splits - this isn't worth it
			}
			lendraw++;
		}
		else if (startdraw!=-1)
		{
			if (lendraw==1) lendraw=2;
			gaps[gapc][0]=startdraw;
			gaps[gapc][1]=lendraw;
			gapc++;

			startdraw=-1;
			lendraw=0;
		}
	}
	if (startdraw!=-1)
	{
		gaps[gapc][0]=startdraw;
		gaps[gapc][1]=lendraw;
		gapc++;
	}
	if (startdraw==0 && lendraw==h) return false;	// nothing saved so don't create a split list

	FloatRect * rcs = new FloatRect[gapc];

	for(x=0;x<gapc;x++)
	{
		// gaps are stored as texture (u/v) coordinates
		rcs[x].width=rcs[x].left=-1.0f;
		rcs[x].top=(float)gaps[x][0]/(float)h;
		rcs[x].height=(float)gaps[x][1]/(float)h;
	}
	gl_info.areas=rcs;
	gl_info.areacount=gapc;

	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FTexture::CheckTrans(unsigned char * buffer, int size, int trans)
{
	if (gl_info.mIsTransparent == -1) 
	{
		gl_info.mIsTransparent = trans;
		if (trans == -1)
		{
			DWORD * dwbuf = (DWORD*)buffer;
			for(int i=0;i<size;i++)
			{
				DWORD alpha = dwbuf[i]>>24;

				if (alpha != 0xff && alpha != 0)
				{
					gl_info.mIsTransparent = 1;
					return;
				}
			}
			gl_info.mIsTransparent = 0;
		}
	}
}


//===========================================================================
// 
// smooth the edges of transparent fields in the texture
//
//===========================================================================

#ifdef WORDS_BIGENDIAN
#define MSB 0
#define SOME_MASK 0xffffff00
#else
#define MSB 3
#define SOME_MASK 0x00ffffff
#endif

#define CHKPIX(ofs) (l1[(ofs)*4+MSB]==255 ? (( ((DWORD*)l1)[0] = ((DWORD*)l1)[ofs]&SOME_MASK), trans=true ) : false)

bool FTexture::SmoothEdges(unsigned char * buffer,int w, int h)
{
	int x,y;
	bool trans=buffer[MSB]==0; // If I set this to false here the code won't detect textures 
	// that only contain transparent pixels.
	bool semitrans = false;
	unsigned char * l1;

	if (h<=1 || w<=1) return false;  // makes (a) no sense and (b) doesn't work with this code!

	l1=buffer;


	if (l1[MSB]==0 && !CHKPIX(1)) CHKPIX(w);
	else if (l1[MSB]<255) semitrans=true;
	l1+=4;
	for(x=1;x<w-1;x++, l1+=4)
	{
		if (l1[MSB]==0 &&  !CHKPIX(-1) && !CHKPIX(1)) CHKPIX(w);
		else if (l1[MSB]<255) semitrans=true;
	}
	if (l1[MSB]==0 && !CHKPIX(-1)) CHKPIX(w);
	else if (l1[MSB]<255) semitrans=true;
	l1+=4;

	for(y=1;y<h-1;y++)
	{
		if (l1[MSB]==0 && !CHKPIX(-w) && !CHKPIX(1)) CHKPIX(w);
		else if (l1[MSB]<255) semitrans=true;
		l1+=4;
		for(x=1;x<w-1;x++, l1+=4)
		{
			if (l1[MSB]==0 &&  !CHKPIX(-w) && !CHKPIX(-1) && !CHKPIX(1) && !CHKPIX(-w-1) && !CHKPIX(-w+1) && !CHKPIX(w-1) && !CHKPIX(w+1)) CHKPIX(w);
			else if (l1[MSB]<255) semitrans=true;
		}
		if (l1[MSB]==0 && !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(w);
		else if (l1[MSB]<255) semitrans=true;
		l1+=4;
	}

	if (l1[MSB]==0 && !CHKPIX(-w)) CHKPIX(1);
	else if (l1[MSB]<255) semitrans=true;
	l1+=4;
	for(x=1;x<w-1;x++, l1+=4)
	{
		if (l1[MSB]==0 &&  !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(1);
		else if (l1[MSB]<255) semitrans=true;
	}
	if (l1[MSB]==0 && !CHKPIX(-w)) CHKPIX(-1);
	else if (l1[MSB]<255) semitrans=true;

	return trans || semitrans;
}

//===========================================================================
// 
// Post-process the texture data after the buffer has been created
//
//===========================================================================

bool FTexture::ProcessData(unsigned char * buffer, int w, int h, bool ispatch)
{
	if (bMasked) 
	{
		bMasked = SmoothEdges(buffer, w, h);
		if (bMasked && !ispatch) FindHoles(buffer, w, h);
	}
	return true;
}

//===========================================================================
//
// fake brightness maps
// These are generated for textures affected by a colormap with
// fullbright entries.
// These textures are only used internally by the GL renderer so
// all code for software rendering support is missing
//
//===========================================================================

FBrightmapTexture::FBrightmapTexture (FTexture *source)
{
	Name = "";
	SourcePic = source;
	CopySize(source);
	bNoDecals = source->bNoDecals;
	Rotations = source->Rotations;
	UseType = source->UseType;
	bMasked = false;
	id.SetInvalid();
	SourceLump = -1;
}

FBrightmapTexture::~FBrightmapTexture ()
{
}

const BYTE *FBrightmapTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	// not needed
	return NULL;
}

const BYTE *FBrightmapTexture::GetPixels ()
{
	// not needed
	return NULL;
}

void FBrightmapTexture::Unload ()
{
}

int FBrightmapTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	SourcePic->CopyTrueColorTranslated(bmp, x, y, rotate, &GlobalBrightmap);
	return 0;
}


//==========================================================================
//
// Parses a brightmap definition
//
//==========================================================================

void gl_ParseBrightmap(FScanner &sc, int deflump)
{
	int type = FTexture::TEX_Any;
	bool disable_fullbright=false;
	bool thiswad = false;
	bool iwad = false;
	FTexture *bmtex = NULL;

	sc.MustGetString();
	if (sc.Compare("texture")) type = FTexture::TEX_Wall;
	else if (sc.Compare("flat")) type = FTexture::TEX_Flat;
	else if (sc.Compare("sprite")) type = FTexture::TEX_Sprite;
	else sc.UnGet();

	sc.MustGetString();
	FTextureID no = TexMan.CheckForTexture(sc.String, type);
	FTexture *tex = TexMan[no];

	sc.MustGetToken('{');
	while (!sc.CheckToken('}'))
	{
		sc.MustGetString();
		if (sc.Compare("disablefullbright"))
		{
			// This can also be used without a brightness map to disable
			// fullbright in rotations that only use brightness maps on
			// other angles.
			disable_fullbright = true;
		}
		else if (sc.Compare("thiswad"))
		{
			// only affects textures defined in the WAD containing the definition file.
			thiswad = true;
		}
		else if (sc.Compare ("iwad"))
		{
			// only affects textures defined in the IWAD.
			iwad = true;
		}
		else if (sc.Compare ("map"))
		{
			sc.MustGetString();

			if (bmtex != NULL)
			{
				Printf("Multiple brightmap definitions in texture %s\n", tex? tex->Name.GetChars() : "(null)");
			}

			bmtex = TexMan.FindTexture(sc.String, FTexture::TEX_Any, FTextureManager::TEXMAN_TryAny);

			if (bmtex == NULL) 
				Printf("Brightmap '%s' not found in texture '%s'\n", sc.String, tex? tex->Name.GetChars() : "(null)");
		}
	}
	if (!tex)
	{
		return;
	}
	if (thiswad || iwad)
	{
		bool useme = false;
		int lumpnum = tex->GetSourceLump();

		if (lumpnum != -1)
		{
			if (iwad && Wads.GetLumpFile(lumpnum) <= FWadCollection::IWAD_FILENUM) useme = true;
			if (thiswad && Wads.GetLumpFile(lumpnum) == deflump) useme = true;
		}
		if (!useme) return;
	}

	if (bmtex != NULL)
	{
		if (tex->bWarped != 0)
		{
			Printf("Cannot combine warping with brightmap on texture '%s'\n", tex->Name.GetChars());
			return;
		}

		bmtex->bMasked = false;
		tex->gl_info.Brightmap = bmtex;
	}	
	tex->gl_info.bDisableFullbright = disable_fullbright;
}

//==========================================================================
//
// Parses a GLBoom+ detail texture definition
//
// Syntax is this:
//	detail
//	{
//		(walls | flats) [default_detail_name [width [height [offset_x [offset_y]]]]]
//		{
//			texture_name [detail_name [width [height [offset_x [offset_y]]]]]
//		}
//	}
// This merely parses the block and returns no error if valid. The feature
// is not actually implemented, so nothing else happens.
//==========================================================================

void gl_ParseDetailTexture(FScanner &sc)
{
	while (!sc.CheckToken('}'))
	{
		sc.MustGetString();
		if (sc.Compare("walls") || sc.Compare("flats"))
		{
			if (!sc.CheckToken('{'))
			{
				sc.MustGetString();  // Default detail texture
				if (sc.CheckFloat()) // Width
				if (sc.CheckFloat()) // Height
				if (sc.CheckFloat()) // OffsX
				if (sc.CheckFloat()) // OffsY
				{
					// Nothing
				}
			}
			else sc.UnGet();
			sc.MustGetToken('{');
			while (!sc.CheckToken('}'))
			{
				sc.MustGetString();  // Texture
				if (sc.GetString())	 // Detail texture
				{
					if (sc.CheckFloat()) // Width
					if (sc.CheckFloat()) // Height
					if (sc.CheckFloat()) // OffsX
					if (sc.CheckFloat()) // OffsY
					{
						// Nothing
					}
				}
				else sc.UnGet();
			}
		}
	}
}


//==========================================================================
//
// Prints some texture info
//
//==========================================================================

CCMD(textureinfo)
{
	int cntt = 0;
	for (int i = 0; i < TexMan.NumTextures(); i++)
	{
		FTexture *tex = TexMan.ByIndex(i);
		if (tex->gl_info.SystemTexture[0] || tex->gl_info.SystemTexture[1] || tex->gl_info.Material[0] || tex->gl_info.Material[1])
		{
			int lump = tex->GetSourceLump();
			Printf(PRINT_LOG, "Texture '%s' (Index %d, Lump %d, Name '%s'):\n", tex->Name.GetChars(), i, lump, Wads.GetLumpFullName(lump));
			if (tex->gl_info.Material[0])
			{
				Printf(PRINT_LOG, "in use (normal)\n");
			}
			else if (tex->gl_info.SystemTexture[0])
			{
				Printf(PRINT_LOG, "referenced (normal)\n");
			}
			if (tex->gl_info.Material[1])
			{
				Printf(PRINT_LOG, "in use (expanded)\n");
			}
			else if (tex->gl_info.SystemTexture[1])
			{
				Printf(PRINT_LOG, "referenced (normal)\n");
			}
			cntt++;
		}
	}
	Printf(PRINT_LOG, "%d system textures\n", cntt);
}

