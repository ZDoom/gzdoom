/*
** decallib.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __DECALLIB_H__
#define __DECALLIB_H__

#include <string.h>

#include "doomtype.h"
#include "renderstyle.h"

class FScanner;
class FDecalTemplate;
struct FDecalAnimator;
class PClass;
class DBaseDecal;
struct side_t;

class FDecalBase
{
	friend class FDecalLib;
public:
	virtual const FDecalTemplate *GetDecal () const;
	virtual void ReplaceDecalRef (FDecalBase *from, FDecalBase *to) = 0;
	
protected:
	FDecalBase ();
	virtual ~FDecalBase ();

	FDecalBase *Left, *Right;
	FName Name;
	uint16_t SpawnID;
	TArray<const PClass *> Users;	// Which actors generate this decal
};

class FDecalTemplate : public FDecalBase
{
	friend class FDecalLib;
public:
	FDecalTemplate () : Translation (0) {}

	void ApplyToDecal (DBaseDecal *actor, side_t *wall) const;
	const FDecalTemplate *GetDecal () const;
	void ReplaceDecalRef (FDecalBase *from, FDecalBase *to);

	double ScaleX, ScaleY;
	uint32_t ShadeColor;
	uint32_t Translation;
	FRenderStyle RenderStyle;
	FTextureID PicNum;
	uint16_t RenderFlags;
	bool opaqueBlood;
	double Alpha;				// same as actor->alpha
	const FDecalAnimator *Animator;
	const FDecalBase *LowerDecal;

	enum { DECAL_RandomFlipX = 0x100, DECAL_RandomFlipY = 0x200 };
};

class FDecalLib
{
public:
	FDecalLib ();
	~FDecalLib ();

	void Clear ();
	void ReadDecals (FScanner &sc);
	void ReadAllDecals ();

	const FDecalTemplate *GetDecalByNum (uint16_t num) const;
	const FDecalTemplate *GetDecalByName (const char *name) const;

private:
	struct FTranslation;

	static void DelTree (FDecalBase *root);
	static FDecalBase *ScanTreeForNum (const uint16_t num, FDecalBase *root);
	static FDecalBase *ScanTreeForName (const char *name, FDecalBase *root);
	static void ReplaceDecalRef (FDecalBase *from, FDecalBase *to, FDecalBase *root);
	FTranslation *GenerateTranslation (uint32_t start, uint32_t end);
	void AddDecal (const char *name, uint16_t num, const FDecalTemplate &decal);
	void AddDecal (FDecalBase *decal);
	FDecalAnimator *FindAnimator (const char *name);

	uint16_t GetDecalID (FScanner &sc);
	void ParseDecal (FScanner &sc);
	void ParseDecalGroup (FScanner &sc);
	void ParseGenerator (FScanner &sc);
	void ParseFader (FScanner &sc);
	void ParseStretcher (FScanner &sc);
	void ParseSlider (FScanner &sc);
	void ParseCombiner (FScanner &sc);
	void ParseColorchanger (FScanner &sc);

	FDecalBase *Root;
	FTranslation *Translations;
};

extern FDecalLib DecalLibrary;

#endif //__DECALLIB_H__
