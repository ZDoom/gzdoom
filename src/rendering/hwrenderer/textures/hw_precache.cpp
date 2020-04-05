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
** Texture precaching
**
*/

#include "c_cvars.h"
#include "w_wad.h"
#include "r_data/r_translate.h"
#include "c_dispatch.h"
#include "r_state.h"
#include "actor.h"
#include "r_data/models/models.h"
#include "textures/skyboxtexture.h"
#include "hwrenderer/textures/hw_material.h"
#include "image.h"
#include "v_video.h"
#include "v_font.h"


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

void hw_PrecacheTexture(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist)
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
			if (tex->isSkybox())
			{
				FSkyBox *sb = static_cast<FSkyBox*>(tex);
				for (int i = 0; i<6; i++)
				{
					if (sb->faces[i])
					{
						int index = sb->faces[i]->GetID().GetIndex();
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
			FSpriteModelFrame * smf = FindModelFrame(cls, state.sprite, state.Frame, false);
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

	TMap<FTexture *, bool> usedTextures, usedSprites;

	screen->StartPrecaching();
	int cnt = TexMan.NumTextures();

	// prepare the textures for precaching. First collect all used layer textures so that we know which ones should not be deleted.
	for (int i = cnt - 1; i >= 0; i--)
	{
		FTexture *tex = TexMan.ByIndex(i);
		if (tex != nullptr)
		{
			FMaterial *mat = FMaterial::ValidateTexture(tex, false, false);
			if (mat != nullptr)
			{
				for (auto ftex : mat->GetLayerArray())
				{
					usedTextures.Insert(ftex, true);
				}
			}
			if (spritehitlist[i] != nullptr && (*spritehitlist[i]).CountUsed() > 0)
			{
				FMaterial *mat = FMaterial::ValidateTexture(tex, true, false);
				if (mat != nullptr)
				{
					for (auto ftex : mat->GetLayerArray())
					{
						usedSprites.Insert(ftex, true);
					}
				}
			}
		}
	}

	// delete unused textures (i.e. those which didn't get referenced by any material in the cache list.
	for (int i = cnt - 1; i >= 0; i--)
	{
		FTexture *tex = TexMan.ByIndex(i);
		if (tex != nullptr && tex->GetUseType() != ETextureType::FontChar)
		{
			if (usedTextures.CheckKey(tex) == nullptr)
			{
				tex->SystemTextures.Clean(true, false);
			}
			if (usedSprites.CheckKey(tex) == nullptr)
			{
				tex->SystemTextures.Clean(false, true);
			}
		}
	}

	if (gl_precache)
	{
		cycle_t precache;
		precache.Reset();
		precache.Clock();

		FImageSource::BeginPrecaching();

		// cache all used textures
		for (int i = cnt - 1; i >= 0; i--)
		{
			FTexture *tex = TexMan.ByIndex(i);
			if (tex != nullptr && tex->GetImage() != nullptr)
			{
				if (texhitlist[i] & (FTextureManager::HIT_Wall | FTextureManager::HIT_Flat | FTextureManager::HIT_Sky))
				{
					if (tex->GetImage() && tex->SystemTextures.GetHardwareTexture(0, false) == nullptr)
					{
						FImageSource::RegisterForPrecache(tex->GetImage());
					}
				}

				// Only register untranslated sprites. Translated ones are very unlikely to require data that can be reused.
				if (spritehitlist[i] != nullptr && (*spritehitlist[i]).CheckKey(0))
				{
					FImageSource::RegisterForPrecache(tex->GetImage());
				}
			}
		}

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


		FImageSource::EndPrecaching();

		// cache all used models
		FModelRenderer *renderer = screen->CreateModelRenderer(-1);
		for (unsigned i = 0; i < Models.Size(); i++)
		{
			if (modellist[i]) 
				Models[i]->BuildVertexBuffer(renderer);
		}
		delete renderer;

		precache.Unclock();
		DPrintf(DMSG_NOTIFY, "Textures precached in %.3f ms\n", precache.TimeMS());
	}

	delete[] spritehitlist;
	delete[] spritelist;
	delete[] modellist;
}

