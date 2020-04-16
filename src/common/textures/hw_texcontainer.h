#pragma once

#include "tarray.h"
#include "hw_ihwtexture.h"
#include "palettecontainer.h"

struct FTextureBuffer;
class IHardwareTexture;

enum ECreateTexBufferFlags
{
	CTF_Expand = 1,			// create buffer with a one-pixel wide border
	CTF_Upscale = 2,		// Upscale the texture
	CTF_CreateMask = 3,		// Flags that are relevant for hardware texture creation.
	CTF_ProcessData = 4,	// run postprocessing on the generated buffer. This is only needed when using the data for a hardware texture.
	CTF_CheckOnly = 8,		// Only runs the code to get a content ID but does not create a texture. Can be used to access a caching system for the hardware textures.
};

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

		void DeleteDescriptors()
		{
			if (hwTexture) hwTexture->DeleteDescriptors();
		}
		
		~TranslatedTexture()
		{
			Delete();
		}
	};

private:

	TranslatedTexture hwDefTex[4];
	TArray<TranslatedTexture> hwTex_Translated;
	
 	TranslatedTexture * GetTexID(int translation, int scaleflags)
	{
		auto remap = GPalette.TranslationToTable(translation);
		translation = remap == nullptr ? 0 : remap->Index;

		if (translation == 0 && !(scaleflags & CTF_Upscale))
		{
			return &hwDefTex[scaleflags];
		}

		translation |= (scaleflags << 24);
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

	void Clean(bool reallyclean)
	{
		hwDefTex[0].DeleteDescriptors();
		hwDefTex[1].DeleteDescriptors();
		for (unsigned int j = 0; j < hwTex_Translated.Size(); j++)
			hwTex_Translated[j].DeleteDescriptors();

		if (!reallyclean) return;
		hwDefTex[0].Delete();
		hwDefTex[1].Delete();
		hwTex_Translated.Clear();
	}
	
	IHardwareTexture * GetHardwareTexture(int translation, int scaleflags)
	{
		auto tt = GetTexID(translation, scaleflags);
		return tt->hwTexture;
	}
	
	void AddHardwareTexture(int translation, int scaleflags, IHardwareTexture *tex)
	{
		auto tt = GetTexID(translation, scaleflags);
		tt->Delete();
		tt->hwTexture =tex;
	}

	//===========================================================================
	// 
	// Deletes all allocated resources and considers translations
	// This will only be called for sprites
	//
	//===========================================================================

	void CleanUnused(SpriteHits &usedtranslations, int scaleflags)
	{
		if (usedtranslations.CheckKey(0) == nullptr)
		{
			hwDefTex[scaleflags].Delete();
		}
		for (int i = hwTex_Translated.Size()-1; i>= 0; i--)
		{
			if (usedtranslations.CheckKey(hwTex_Translated[i].translation & 0xffffff) == nullptr)
			{
				hwTex_Translated.Delete(i);
			}
		}
	}
	
	template<class T>
	void Iterate(T callback)
	{
		for (auto & t : hwDefTex) if (t.hwTexture) callback(t.hwTexture);
		for (auto & t : hwTex_Translated) if (t.hwTexture) callback(t.hwTexture);
	}

	
};

