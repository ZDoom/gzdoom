#pragma once

#include "tarray.h"

struct FTextureBuffer;

class FHardwareTextureContainer
{
public:
	enum
	{
		MAX_TEXTURES = 16
	};

private:
	struct TranslatedTexture
	{
		IHardwareTexture *hwTexture = nullptr;
		int translation = 0;

		void Delete()
		{
			if (hwTexture) delete hwTexture;
			hwTexture = nullptr;
		}
		
		~TranslatedTexture()
		{
			Delete();
		}
	};

private:

	TranslatedTexture hwDefTex[2];
	TArray<TranslatedTexture> hwTex_Translated;
	
	TranslatedTexture * GetTexID(int translation, bool expanded)
	{
		translation = TranslationToIndex(translation);
		if (translation == 0)
		{
			return &hwDefTex[expanded];
		}

		if (expanded) translation = -translation;
		// normally there aren't more than very few different 
		// translations here so this isn't performance critical.
		unsigned index = hwTex_Translated.FindEx([=](auto &element)
		{
			return element.translation == translation;
		}
		if (index < hwTex_Translated.Size())
		{
			return &hwTex_Translated[i];
		}

		int add = hwTex_Translated.Reserve(1);
		auto item = &hwTex_Translated[add];
		item->translation = translation;
		return item;
	}

public:

	void Clean(bool all)
	{
		if (all) hwDefTex.Delete();
		hwTex_Translated.Clear();
			
	}
	
	IHardwareTexture * GetHardwareTexture(int translation, bool expanded)
	{
		auto tt = GetTexID(translation, expanded);
		return tt->hwTexture;
	}
	
	void AddHardwareTexture(int translation, bool expanded, IHardwareTexture *tex)
	{
		auto tt = GetTexID(translation, expanded);
		tt->Delete();
		tt->hwTexture =tex;
	}
	
	//===========================================================================
	// 
	// Deletes all allocated resources and considers translations
	// This will only be called for sprites
	//
	//===========================================================================

	void FHardwareTexture::CleanUnused(SpriteHits &usedtranslations, bool expanded)
	{
		if (usedtranslations.CheckKey(0) == nullptr)
		{
			hwDefTex[expanded].Delete();
		}
		for (int i = hwTex_Translated.Size()-1; i>= 0; i--)
		{
			if (usedtranslations.CheckKey(hwTex_Translated[i].translation) == nullptr)
			{
				hwTex_Translated.Delete(i);
			}
		}
	}
	
	static int TranslationToIndex(int translation)
	{
		if (translation <= 0)
		{
			return -translation;
		}
		else
		{
			auto remap = TranslationToTable(translation);
			return remap == nullptr ? 0 : remap->GetUniqueIndex();
		}
	}

	
};

