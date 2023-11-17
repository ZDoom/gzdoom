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
#include "filesystem.h"
#include "r_data/r_translate.h"
#include "c_dispatch.h"
#include "r_state.h"
#include "actor.h"
#include "models.h"
#include "skyboxtexture.h"
#include "hw_material.h"
#include "image.h"
#include "v_video.h"
#include "v_font.h"
#include "texturemanager.h"
#include "modelrenderer.h"
#include "hw_models.h"
#include "d_main.h"

EXTERN_CVAR(Bool, gl_precache)

//==========================================================================
//
// DFrameBuffer :: PrecacheTexture
//
//==========================================================================

static void PrecacheTexture(FGameTexture *tex, int cache)
{
	if (cache & (FTextureManager::HIT_Wall | FTextureManager::HIT_Flat | FTextureManager::HIT_Sky))
	{
		int scaleflags = 0;
		if (shouldUpscale(tex, UF_Texture)) scaleflags |= CTF_Upscale;

		FMaterial * gltex = FMaterial::ValidateTexture(tex, scaleflags);
		if (gltex) screen->PrecacheMaterial(gltex, 0);
	}
}

//===========================================================================
//
//
//
//===========================================================================
static void PrecacheList(FMaterial *gltex, SpriteHits& translations)
{
	SpriteHits::Iterator it(translations);
	SpriteHits::Pair* pair;
	while (it.NextPair(pair)) screen->PrecacheMaterial(gltex, pair->Key);
}

//==========================================================================
//
// DFrameBuffer :: PrecacheSprite
//
//==========================================================================

static void PrecacheSprite(FGameTexture *tex, SpriteHits &hits)
{
	int scaleflags = CTF_Expand;
	if (shouldUpscale(tex, UF_Sprite)) scaleflags |= CTF_Upscale;

	FMaterial * gltex = FMaterial::ValidateTexture(tex, scaleflags);
	if (gltex) PrecacheList(gltex, hits);
}


//==========================================================================
//
// DFrameBuffer :: Precache
//
//==========================================================================

void hw_PrecacheTexture(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist)
{
	TMap<FTexture*, bool> allTextures;
	TArray<FTexture*> layers;

	// First collect the potential max. texture set 
	for (int i = 1; i < TexMan.NumTextures(); i++)
	{
		auto gametex = TexMan.GameByIndex(i);
		if (gametex && gametex->isValid() &&
			gametex->GetTexture()->GetImage() &&	// only image textures are subject to precaching
			gametex->GetUseType() != ETextureType::FontChar &&	// We do not want to delete font characters here as they are very likely to be needed constantly.
			gametex->GetUseType() < ETextureType::Special)		// Any texture marked as 'special' is also out.
		{
			gametex->GetLayers(layers);
			for (auto layer : layers)
			{
				allTextures.Insert(layer, true);
				layer->CleanPrecacheMarker();
			}
		}

		// Mark the faces of a skybox as used.
		// This isn't done by the main code so it needs to be done here.
		// MBF sky transfers are being checked by the calling code to add HIT_Sky for them.
		if (texhitlist[i] & (FTextureManager::HIT_Sky))
		{
			auto tex = TexMan.GameByIndex(i);
			auto sb = dynamic_cast<FSkyBox*>(tex->GetTexture());
			if (sb)
			{
				for (int i = 0; i < 6; i++)
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

	SpriteHits *spritelist = new SpriteHits[sprites.Size()];
	SpriteHits **spritehitlist = new SpriteHits*[TexMan.NumTextures()];
	TMap<PClassActor*, bool>::Iterator it(actorhitlist);
	TMap<PClassActor*, bool>::Pair *pair;
	uint8_t *modellist = new uint8_t[Models.Size()];
	memset(modellist, 0, Models.Size());
	memset(spritehitlist, 0, sizeof(SpriteHits**) * TexMan.NumTextures());

	// Check all used actors.
	// 1. mark all sprites associated with its states
	// 2. mark all model data and skins associated with its states
	while (it.NextPair(pair))
	{
		PClassActor *cls = pair->Key;
		auto remap = GPalette.TranslationToTable(GetDefaultByType(cls)->Translation.index());
		int gltrans = remap == nullptr ? 0 : remap->Index;

		for (unsigned i = 0; i < cls->GetStateCount(); i++)
		{
			auto &state = cls->GetStates()[i];
			spritelist[state.sprite].Insert(gltrans, true);
			FSpriteModelFrame * smf = FindModelFrameRaw(cls, state.sprite, state.Frame, false);
			if (smf != NULL)
			{
				for (int i = 0; i < smf->modelsAmount; i++)
				{
					if (smf->skinIDs[i].isValid())
					{
						texhitlist[smf->skinIDs[i].GetIndex()] |= FTextureManager::HIT_Flat;
					}
					else if (smf->modelIDs[i] != -1)
					{
						Models[smf->modelIDs[i]]->AddSkins(texhitlist, (unsigned)(i * MD3_MAX_SURFACES) < smf->surfaceskinIDs.Size()? &smf->surfaceskinIDs[i * MD3_MAX_SURFACES] : nullptr);
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

	// prepare the textures for precaching. First mark all used layer textures so that we know which ones should not be deleted.
	for (int i = cnt - 1; i >= 0; i--)
	{
		auto tex = TexMan.GameByIndex(i);
		if (tex != nullptr)
		{
			if (texhitlist[i] & (FTextureManager::HIT_Wall | FTextureManager::HIT_Flat | FTextureManager::HIT_Sky))
			{
				int scaleflags = 0;
				if (shouldUpscale(tex, UF_Texture)) scaleflags |= CTF_Upscale;

				FMaterial* mat = FMaterial::ValidateTexture(tex, scaleflags, true);
				if (mat != nullptr)
				{
					for (auto &layer : mat->GetLayerArray())
					{
						if (layer.layerTexture) layer.layerTexture->MarkForPrecache(0, layer.scaleFlags);
					}
				}
			}
			if (spritehitlist[i] != nullptr && (*spritehitlist[i]).CountUsed() > 0)
			{
				int scaleflags = CTF_Expand;
				if (shouldUpscale(tex, UF_Sprite)) scaleflags |= CTF_Upscale;

				FMaterial *mat = FMaterial::ValidateTexture(tex, true, true);
				if (mat != nullptr)
				{
					SpriteHits::Iterator it(*spritehitlist[i]);
					SpriteHits::Pair* pair;
					while (it.NextPair(pair))
					{
						for (auto& layer : mat->GetLayerArray())
						{
							if (layer.layerTexture) layer.layerTexture->MarkForPrecache(pair->Key, layer.scaleFlags);
						}
					}
				}
			}
		}
	}

	// delete unused hardware textures (i.e. those which didn't get referenced by any material in the cache list.)
	decltype(allTextures)::Iterator ita(allTextures);
	decltype(allTextures)::Pair* paira;
	while (ita.NextPair(paira))
	{
		paira->Key->CleanUnused();
	}

	if (gl_precache)
	{
		cycle_t precache;
		precache.Reset();
		precache.Clock();

		FImageSource::BeginPrecaching();

		// cache all used images
		for (int i = cnt - 1; i >= 0; i--)
		{
			auto gtex = TexMan.GameByIndex(i);
			auto tex = gtex->GetTexture();
			if (tex != nullptr && tex->GetImage() != nullptr)
			{
				if (texhitlist[i] & (FTextureManager::HIT_Wall | FTextureManager::HIT_Flat | FTextureManager::HIT_Sky))
				{
					int flags = shouldUpscale(gtex, UF_Texture);
					if (tex->GetImage() && tex->GetHardwareTexture(0, flags) == nullptr)
					{
						FImageSource::RegisterForPrecache(tex->GetImage(), V_IsTrueColor());
					}
				}

				// Only register untranslated sprite images. Translated ones are very unlikely to require data that can be reused so they can just be created on demand.
				if (spritehitlist[i] != nullptr && (*spritehitlist[i]).CheckKey(0))
				{
					FImageSource::RegisterForPrecache(tex->GetImage(), V_IsTrueColor());
				}
			}
		}

		// cache all used textures
		for (int i = cnt - 1; i >= 0; i--)
		{
			auto gtex = TexMan.GameByIndex(i);
			if (gtex != nullptr)
			{
				PrecacheTexture(gtex, texhitlist[i]);
				if (spritehitlist[i] != nullptr && (*spritehitlist[i]).CountUsed() > 0)
				{
					PrecacheSprite(gtex, *spritehitlist[i]);
				}
			}
		}


		FImageSource::EndPrecaching();

		// cache all used models
		FModelRenderer* renderer = new FHWModelRenderer(nullptr, *screen->RenderState(), -1);
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

