#ifndef __GL_TRANSLATE__
#define __GL_TRANSLATE__

#include "doomtype.h"
#include "v_video.h"

enum
{
	TRANSLATION_ICE = -1,
	TRANSLATION_INTENSITY = -2,
	TRANSLATION_SHADE = -3,
};


class GLTranslationPalette : public FNativePalette
{
	struct PalData
	{
		int crc32;
		PalEntry pe[256];
	};
	static TArray<PalData> AllPalettes;

	int Index;
	FRemapTable *remap;

	GLTranslationPalette(FRemapTable *r) { remap=r; Index=-1; }

public:

	static GLTranslationPalette *CreatePalette(FRemapTable *remap);
	static int GetInternalTranslation(int trans);
	static PalEntry *GetPalette(unsigned int index)
	{
		return index > 0 && index <= AllPalettes.Size()? AllPalettes[index-1].pe : NULL;
	}
	bool Update();
	int GetIndex() const { return Index; }
};


#endif
