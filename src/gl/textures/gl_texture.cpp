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
#include "v_palette.h"
#include "sc_man.h"
#include "textures/skyboxtexture.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/textures/gl_material.h"
#include "gl/textures/gl_samplers.h"
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



FTexture::GLTexInfo::~GLTexInfo()
{
	for (int i = 0; i < 2; i++)
	{
		if (Material[i] != NULL) delete Material[i];
		Material[i] = NULL;

		if (SystemTexture[i] != NULL) delete SystemTexture[i];
		SystemTexture[i] = NULL;
	}
}

//===========================================================================
// 
// Sprite adjust has changed.
// This needs to alter the material's sprite rect.
//
//===========================================================================

void FTexture::SetSpriteAdjust()
{
	for(auto mat : gl_info.Material)
	{
		if (mat != nullptr) mat->SetSpriteRect();
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
			if (tex->bSkybox)
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
		auto remap = TranslationToTable(GetDefaultByType(cls)->Translation);
		int gltrans = remap == nullptr ? 0 : remap->GetUniqueIndex();

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
						texhitlist[smf->skinIDs[i].GetIndex()] |= FTextureManager::HIT_Flat;
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
		FGLModelRenderer renderer(-1);
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

