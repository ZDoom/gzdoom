#ifndef __DECALLIB_H__
#define __DECALLIB_H__

#include <string.h>

#include "doomtype.h"

class AActor;
class FDecal;

class FDecalBase
{
	friend class FDecalLib;
public:
	virtual FDecal *GetDecal ();
	
protected:
	FDecalBase ();
	virtual ~FDecalBase ();

	FDecalBase *Left, *Right;
	char *Name;
	BYTE SpawnID;
};

class FDecal : public FDecalBase
{
	friend class FDecalLib;
public:
	FDecal () : Translation (NULL) {}

	void ApplyToActor (AActor *actor) const;
	FDecal *GetDecal ();

	DWORD ShadeColor;
	BYTE *Translation;
	BYTE ScaleX, ScaleY;
	BYTE RenderStyle;
	WORD PicNum;
	WORD RenderFlags;
	WORD Alpha;				// same as (actor->alpha >> 1)

	enum { DECAL_RandomFlipX = 0x100, DECAL_RandomFlipY = 0x200 };
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

private:
	struct FTranslation;

	static void DelTree (FDecalBase *root);
	static FDecalBase *ScanTreeForNum (const BYTE num, FDecalBase *root);
	static FDecalBase *ScanTreeForName (const char *name, FDecalBase *root);
	FTranslation *GenerateTranslation (DWORD start, DWORD end);
	void AddDecal (const char *name, byte num, const FDecal &decal);
	void AddDecal (FDecalBase *decal);

	BYTE GetDecalID ();
	void ParseDecal ();
	void ParseDecalGroup ();
	void ParseGenerator ();

	FDecalBase *Root;
	FTranslation *Translations;
};

extern FDecalLib DecalLibrary;

#endif //__DECALLIB_H__
