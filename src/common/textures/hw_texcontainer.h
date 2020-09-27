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
	CTF_Indexed = 4,		// Tell the backend to create an indexed texture.
	CTF_CheckOnly = 8,		// Only runs the code to get a content ID but does not create a texture. Can be used to access a caching system for the hardware textures.
	CTF_ProcessData = 16,	// run postprocessing on the generated buffer. This is only needed when using the data for a hardware texture.
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
		bool precacheMarker;	// This is used to check whether a texture has been hit by the precacher, so that the cleanup code can delete the unneeded ones.

		void Delete()
		{
			if (hwTexture) delete hwTexture;
			hwTexture = nullptr;
		}

		~TranslatedTexture()
		{
			Delete();
		}

		void MarkForPrecache(bool on)
		{
			precacheMarker = on;
		}

		bool isMarkedForPreache() const
		{
			return precacheMarker;
		}
	};

private:

	TranslatedTexture hwDefTex[4];
	TArray<TranslatedTexture> hwTex_Translated;
	
 	TranslatedTexture * GetTexID(int translation, int scaleflags)
	{
		// Allow negative indices to pass through unchanged. 
		// This is needed for allowing the client to allocate slots that aren't matched to a palette, e.g. Build's indexed variants.
		if (translation >= 0)
		{
			auto remap = GPalette.TranslationToTable(translation);
			translation = remap == nullptr ? 0 : remap->Index;
		}
		else translation &= ~0x7fffffff;

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
	void Clean()
	{
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
	//
	//===========================================================================

	void CleanUnused()
	{
		for (auto& tt : hwDefTex)
		{
			if (!tt.isMarkedForPreache()) tt.Delete();
		}
		for (int i = hwTex_Translated.Size()-1; i>= 0; i--)
		{
			auto& tt = hwTex_Translated[i];
			if (!tt.isMarkedForPreache()) 
			{
				hwTex_Translated.Delete(i);
			}
		}
	}

	void UnmarkAll()
	{
		for (auto& tt : hwDefTex)
		{
			if (!tt.isMarkedForPreache()) tt.MarkForPrecache(false);
		}
		for (auto& tt : hwTex_Translated)
		{
			if (!tt.isMarkedForPreache()) tt.MarkForPrecache(false);
		}
	}

	void MarkForPrecache(int translation, int scaleflags)
	{
		auto tt = GetTexID(translation, scaleflags);
		tt->MarkForPrecache(true);
	}


	template<class T>
	void Iterate(T callback)
	{
		for (auto & t : hwDefTex) if (t.hwTexture) callback(t.hwTexture);
		for (auto & t : hwTex_Translated) if (t.hwTexture) callback(t.hwTexture);
	}

	
};

