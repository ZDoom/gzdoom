#ifndef __DECALLIB_H__
#define __DECALLIB_H__

#include <string.h>

#include "doomtype.h"

class AActor;

class FDecal
{
	friend class FDecalLib;

private:
	FDecal ();
	~FDecal ();

	FDecal *Left, *Right;

public:
	void ApplyToActor (AActor *actor) const;

	DWORD ShadeColor;
	BYTE *Translation;
	char *Name;
	BYTE ScaleX, ScaleY;
	BYTE SpawnID, RenderStyle;
	WORD PicNum;
	WORD RenderFlags;
	WORD Alpha;				// same as (actor->alpha >> 1)
};

class FDecalLib
{
public:
	FDecalLib ();
	~FDecalLib ();

	void Clear ();
	void ReadDecals ();		// SC_Open() should have just been called
	void ReadAllDecals ();

	const FDecal *GetDecalByNum (byte num) const;
	const FDecal *GetDecalByName (const char *name) const;
	void AddDecal (const char *name, byte num, const FDecal &decal);

private:
	struct FTranslation;

	static void DelTree (FDecal *root);
	static FDecal *ScanTreeForNum (const BYTE num, FDecal *root);
	static FDecal *ScanTreeForName (const char *name, FDecal *root);
	FTranslation *GenerateTranslation (DWORD start, DWORD end);

	FDecal *Root;
	FTranslation *Translations;
};

extern FDecalLib DecalLibrary;

#endif //__DECALLIB_H__