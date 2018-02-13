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
** Global texture data
**
*/

#include "gl/system/gl_system.h"
#include "c_cvars.h"
#include "w_wad.h"
#include "templates.h"
#include "colormatcher.h"
#include "r_data/r_translate.h"
#include "c_dispatch.h"
#include "r_state.h"
#include "actor.h"
#include "cmdlib.h"
#ifdef _WIN32
#include "win32gliface.h"
#endif
#include "v_palette.h"
#include "sc_man.h"
#include "textures/skyboxtexture.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_material.h"
#include "gl/textures/gl_samplers.h"
#include "gl/textures/gl_translate.h"
#include "gl/models/gl_models.h"

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
	uint8_t palbuffer[768];
	ReadPalette(Wads.CheckNumForName("PLAYPAL"), palbuffer);

	const unsigned char *cmapdata = (const unsigned char *)cmap.GetMem();
	const uint8_t *paldata = palbuffer;

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
		if (GlobalBrightmap.Remap[i] == white) DPrintf(DMSG_NOTIFY, "Marked color %d as fullbright\n",i);
	}
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
	bFullbright = false;
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
			const uint8_t *texbuf = GetPixels();
			const int white = ColorMatcher.Pick(255,255,255);

			int size = GetWidth() * GetHeight();
			for(int i=0;i<size;i++)
			{
				if (GlobalBrightmap.Remap[texbuf[i]] == white)
				{
					// Create a brightmap
					DPrintf(DMSG_NOTIFY, "brightmap created for texture '%s'\n", Name.GetChars());
					gl_info.Brightmap = new FBrightmapTexture(this);
					gl_info.bBrightmapChecked = 1;
					TexMan.AddTexture(gl_info.Brightmap);
					return;
				}
			}
			// No bright pixels found
			DPrintf(DMSG_SPAMMY, "No bright pixels found in texture '%s'\n", Name.GetChars());
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
			gl_info.GlowColor = averageColor((uint32_t *) buffer, w*h, 153);
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

	if (gapc > 0)
	{
		FloatRect * rcs = new FloatRect[gapc];

		for (x = 0; x < gapc; x++)
		{
			// gaps are stored as texture (u/v) coordinates
			rcs[x].width = rcs[x].left = -1.0f;
			rcs[x].top = (float)gaps[x][0] / (float)h;
			rcs[x].height = (float)gaps[x][1] / (float)h;
		}
		gl_info.areas = rcs;
	}
	else gl_info.areas = nullptr;
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
			uint32_t * dwbuf = (uint32_t*)buffer;
			for(int i=0;i<size;i++)
			{
				uint32_t alpha = dwbuf[i]>>24;

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

#define CHKPIX(ofs) (l1[(ofs)*4+MSB]==255 ? (( ((uint32_t*)l1)[0] = ((uint32_t*)l1)[ofs]&SOME_MASK), trans=true ) : false)

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

const uint8_t *FBrightmapTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	// not needed
	return NULL;
}

const uint8_t *FBrightmapTexture::GetPixels ()
{
	// not needed
	return NULL;
}

void FBrightmapTexture::Unload ()
{
}

int FBrightmapTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	SourcePic->CopyTrueColorTranslated(bmp, x, y, rotate, GlobalBrightmap.Palette);
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
	FTextureID no = TexMan.CheckForTexture(sc.String, type, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_Overridable);
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
			if (iwad && Wads.GetLumpFile(lumpnum) <= Wads.GetIwadNum()) useme = true;
			if (thiswad && Wads.GetLumpFile(lumpnum) == deflump) useme = true;
		}
		if (!useme) return;
	}

	if (bmtex != NULL)
	{
		/* I do not think this is needed any longer
		if (tex->bWarped != 0)
		{
			Printf("Cannot combine warping with brightmap on texture '%s'\n", tex->Name.GetChars());
			return;
		}
		*/

		bmtex->bMasked = false;
		tex->gl_info.Brightmap = bmtex;
	}	
	tex->gl_info.bDisableFullbright = disable_fullbright;
}

//==========================================================================
//
//
//
//==========================================================================

void AddAutoBrightmaps()
{
	int num = Wads.GetNumLumps();
	for (int i = 0; i < num; i++)
	{
		const char *name = Wads.GetLumpFullName(i);
		if (strstr(name, "brightmaps/auto/") == name)
		{
			TArray<FTextureID> list;
			FString texname = ExtractFileBase(name, false);
			TexMan.ListTextures(texname, list);
			auto bmtex = TexMan.FindTexture(name, FTexture::TEX_Any, FTextureManager::TEXMAN_TryAny);
			for (auto texid : list)
			{
				bmtex->bMasked = false;
				TexMan[texid]->gl_info.Brightmap = bmtex;
			}
		}
	}
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
// DFrameBuffer :: PrecacheTexture
//
//==========================================================================

static void PrecacheTexture(FTexture *tex, int cache)
{
	if (cache & (FTextureManager::HIT_Wall | FTextureManager::HIT_Flat | FTextureManager::HIT_Sky))
	{
		FMaterial * gltex = FMaterial::ValidateTexture(tex, false);
		if (gltex) gltex->Precache();
	}
	else
	{
		// make sure that software pixel buffers do not stick around for unneeded textures.
		tex->Unload();
	}
}

//==========================================================================
//
// DFrameBuffer :: PrecacheSprite
//
//==========================================================================

static void PrecacheSprite(FTexture *tex, SpriteHits &hits)
{
	FMaterial * gltex = FMaterial::ValidateTexture(tex, true);
	if (gltex) gltex->PrecacheList(hits);
}

//==========================================================================
//
// DFrameBuffer :: Precache
//
//==========================================================================

void gl_PrecacheTexture(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist)
{
	SpriteHits *spritelist = new SpriteHits[sprites.Size()];
	SpriteHits **spritehitlist = new SpriteHits*[TexMan.NumTextures()];
	TMap<PClassActor*, bool>::Iterator it(actorhitlist);
	TMap<PClassActor*, bool>::Pair *pair;
	uint8_t *modellist = new uint8_t[Models.Size()];
	memset(modellist, 0, Models.Size());
	memset(spritehitlist, 0, sizeof(SpriteHits**) * TexMan.NumTextures());

	// this isn't done by the main code so it needs to be done here first:
	// check skybox textures and mark the separate faces as used
	for (int i = 0; i<TexMan.NumTextures(); i++)
	{
		// HIT_Wall must be checked for MBF-style sky transfers. 
		if (texhitlist[i] & (FTextureManager::HIT_Sky | FTextureManager::HIT_Wall))
		{
			FTexture *tex = TexMan.ByIndex(i);
			if (tex->gl_info.bSkybox)
			{
				FSkyBox *sb = static_cast<FSkyBox*>(tex);
				for (int i = 0; i<6; i++)
				{
					if (sb->faces[i])
					{
						int index = sb->faces[i]->id.GetIndex();
						texhitlist[index] |= FTextureManager::HIT_Flat;
					}
				}
			}
		}
	}

	// Check all used actors.
	// 1. mark all sprites associated with its states
	// 2. mark all model data and skins associated with its states
	while (it.NextPair(pair))
	{
		PClassActor *cls = pair->Key;
		int gltrans = GLTranslationPalette::GetInternalTranslation(GetDefaultByType(cls)->Translation);

		for (unsigned i = 0; i < cls->GetStateCount(); i++)
		{
			auto &state = cls->GetStates()[i];
			spritelist[state.sprite].Insert(gltrans, true);
			FSpriteModelFrame * smf = gl_FindModelFrame(cls, state.sprite, state.Frame, false);
			if (smf != NULL)
			{
				for (int i = 0; i < MAX_MODELS_PER_FRAME; i++)
				{
					if (smf->skinIDs[i].isValid())
					{
						texhitlist[smf->skinIDs[i].GetIndex()] |= FTexture::TEX_Flat;
					}
					else if (smf->modelIDs[i] != -1)
					{
						Models[smf->modelIDs[i]]->PushSpriteMDLFrame(smf, i);
						Models[smf->modelIDs[i]]->AddSkins(texhitlist);
					}
					if (smf->modelIDs[i] != -1)
					{
						modellist[smf->modelIDs[i]] = 1;
					}
				}
			}
		}
	}

	// mark all sprite textures belonging to the marked sprites.
	for (int i = (int)(sprites.Size() - 1); i >= 0; i--)
	{
		if (spritelist[i].CountUsed())
		{
			int j, k;
			for (j = 0; j < sprites[i].numframes; j++)
			{
				const spriteframe_t *frame = &SpriteFrames[sprites[i].spriteframes + j];

				for (k = 0; k < 16; k++)
				{
					FTextureID pic = frame->Texture[k];
					if (pic.isValid())
					{
						spritehitlist[pic.GetIndex()] = &spritelist[i];
					}
				}
			}
		}
	}

	// delete everything unused before creating any new resources to avoid memory usage peaks.

	// delete unused models
	for (unsigned i = 0; i < Models.Size(); i++)
	{
		if (!modellist[i]) Models[i]->DestroyVertexBuffer();
	}

	// delete unused textures
	int cnt = TexMan.NumTextures();
	for (int i = cnt - 1; i >= 0; i--)
	{
		FTexture *tex = TexMan.ByIndex(i);
		if (tex != nullptr)
		{
			if (!texhitlist[i])
			{
				if (tex->gl_info.Material[0]) tex->gl_info.Material[0]->Clean(true);
			}
			if (spritehitlist[i] == nullptr || (*spritehitlist[i]).CountUsed() == 0)
			{
				if (tex->gl_info.Material[1]) tex->gl_info.Material[1]->Clean(true);
			}
		}
	}

	if (gl_precache)
	{
		// cache all used textures
		for (int i = cnt - 1; i >= 0; i--)
		{
			FTexture *tex = TexMan.ByIndex(i);
			if (tex != nullptr)
			{
				PrecacheTexture(tex, texhitlist[i]);
				if (spritehitlist[i] != nullptr && (*spritehitlist[i]).CountUsed() > 0)
				{
					PrecacheSprite(tex, *spritehitlist[i]);
				}
			}
		}

		// cache all used models
		FGLModelRenderer renderer;
		for (unsigned i = 0; i < Models.Size(); i++)
		{
			if (modellist[i]) 
				Models[i]->BuildVertexBuffer(&renderer);
		}
	}

	delete[] spritehitlist;
	delete[] spritelist;
	delete[] modellist;
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

