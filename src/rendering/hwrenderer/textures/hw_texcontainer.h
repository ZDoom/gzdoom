#pragma once

#include "tarray.h"
#include "hwrenderer/textures/hw_ihwtexture.h"

struct FTextureBuffer;
class IHardwareTexture;

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
		});
		if (index < hwTex_Translated.Size())
		{
			return &hwTex_Translated[index];
		}

		int add = hwTex_Translated.Reserve(1);
		auto item = &hwTex_Translated[add];
		item->translation = translation;
		return item;
	}

public:

	void Clean(bool cleannormal, bool cleanexpanded)
	{
		if (cleannormal) hwDefTex[0].Delete();
		if (cleanexpanded) hwDefTex[1].Delete();
		for (int i = hwTex_Translated.Size() - 1; i >= 0; i--)
		{
			if (cleannormal && hwTex_Translated[i].translation > 0) hwTex_Translated.Delete(i);
			else if (cleanexpanded && hwTex_Translated[i].translation < 0) hwTex_Translated.Delete(i);
		}
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

	void CleanUnused(SpriteHits &usedtranslations, bool expanded)
	{
		if (usedtranslations.CheckKey(0) == nullptr)
		{
			hwDefTex[expanded].Delete();
		}
		int fac = expanded ? -1 : 1;
		for (int i = hwTex_Translated.Size()-1; i>= 0; i--)
		{
			if (usedtranslations.CheckKey(hwTex_Translated[i].translation * fac) == nullptr)
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

	template<class T>
	void Iterate(T callback)
	{
		for (auto & t : hwDefTex) if (t.hwTexture) callback(t.hwTexture);
		for (auto & t : hwTex_Translated) if (t.hwTexture) callback(t.hwTexture);
	}

	
};

