/*
** decallib.cpp
** Parses DECALDEFs and creates a "library" of decals
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

#include "decallib.h"
#include "sc_man.h"
#include "w_wad.h"
#include "v_video.h"
#include "v_palette.h"
#include "cmdlib.h"
#include "m_random.h"
#include "weightedlist.h"
#include "statnums.h"
#include "templates.h"
#include "r_draw.h"
#include "a_sharedglobal.h"
#include "r_translate.h"

FDecalLib DecalLibrary;

static fixed_t ReadScale ();
static TArray<BYTE> DecalTranslations;

// A decal group holds multiple decals and returns one randomly
// when GetDecal() is called.

// Sometimes two machines in a game will disagree on the state of
// decals. I do not know why.

static FRandom pr_decalchoice ("DecalChoice");
static FRandom pr_decal ("Decal");

class FDecalGroup : public FDecalBase
{
	friend class FDecalLib;

public:
	FDecalGroup () : Choices (pr_decalchoice) {}
	const FDecalTemplate *GetDecal () const;
	void ReplaceDecalRef (FDecalBase *from, FDecalBase *to)
	{
		Choices.ReplaceValues(from, to);
	}
	void AddDecal (FDecalBase *decal, WORD weight)
	{
		Choices.AddEntry (decal, weight);
	}

private:
	TWeightedList<FDecalBase *> Choices;

	FDecalGroup &operator= (const FDecalGroup &) { return *this; }
};

struct FDecalLib::FTranslation
{
	FTranslation (DWORD start, DWORD end);
	FTranslation *LocateTranslation (DWORD start, DWORD end);

	DWORD StartColor, EndColor;
	FTranslation *Next;
	WORD Index;
};

struct FDecalAnimator
{
	FDecalAnimator (const char *name);
	virtual ~FDecalAnimator ();
	virtual DThinker *CreateThinker (DBaseDecal *actor, side_t *wall) const = 0;

	FName Name;
};

class FDecalAnimatorArray : public TArray<FDecalAnimator *>
{
public:
	~FDecalAnimatorArray()
	{
		for (unsigned int i = 0; i < Size(); ++i)
		{
			delete (*this)[i];
		}
	}
};

FDecalAnimatorArray Animators;

struct DDecalThinker : public DThinker
{
	DECLARE_CLASS (DDecalThinker, DThinker)
	HAS_OBJECT_POINTERS
public:
	DDecalThinker (DBaseDecal *decal) : DThinker (STAT_DECALTHINKER), TheDecal (decal) {}
	void Serialize (FArchive &arc);
	DBaseDecal *TheDecal;
protected:
	DDecalThinker () : DThinker (STAT_DECALTHINKER) {}
};

IMPLEMENT_POINTY_CLASS (DDecalThinker)
 DECLARE_POINTER (TheDecal)
END_POINTERS

void DDecalThinker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << TheDecal;
}

struct FDecalFaderAnim : public FDecalAnimator
{
	FDecalFaderAnim (const char *name) : FDecalAnimator (name) {}
	DThinker *CreateThinker (DBaseDecal *actor, side_t *wall) const;

	int DecayStart;
	int DecayTime;
};

class DDecalFader : public DDecalThinker
{
	DECLARE_CLASS (DDecalFader, DDecalThinker)
public:
	DDecalFader (DBaseDecal *decal) : DDecalThinker (decal) {}
	void Serialize (FArchive &arc);
	void Tick ();

	int TimeToStartDecay;
	int TimeToEndDecay;
	fixed_t StartTrans;
private:
	DDecalFader () {}
};

struct FDecalColorerAnim : public FDecalAnimator
{
	FDecalColorerAnim (const char *name) : FDecalAnimator (name) {}
	DThinker *CreateThinker (DBaseDecal *actor, side_t *wall) const;

	int DecayStart;
	int DecayTime;
	PalEntry GoalColor;
};

class DDecalColorer : public DDecalThinker
{
	DECLARE_CLASS (DDecalColorer, DDecalThinker)
public:
	DDecalColorer (DBaseDecal *decal) : DDecalThinker (decal) {}
	void Serialize (FArchive &arc);
	void Tick ();

	int TimeToStartDecay;
	int TimeToEndDecay;
	PalEntry StartColor;
	PalEntry GoalColor;
private:
	DDecalColorer () {}
};

struct FDecalStretcherAnim : public FDecalAnimator
{
	FDecalStretcherAnim (const char *name) : FDecalAnimator (name) {}
	DThinker *CreateThinker (DBaseDecal *actor, side_t *wall) const;

	int StretchStart;
	int StretchTime;
	fixed_t GoalX, GoalY;
};

class DDecalStretcher : public DDecalThinker
{
	DECLARE_CLASS (DDecalStretcher, DDecalThinker)
public:
	DDecalStretcher (DBaseDecal *decal) : DDecalThinker (decal) {}
	void Serialize (FArchive &arc);
	void Tick ();

	int TimeToStart;
	int TimeToStop;
	fixed_t GoalX;
	fixed_t StartX;
	fixed_t GoalY;
	fixed_t StartY;
	bool bStretchX;
	bool bStretchY;
	bool bStarted;
private:
	DDecalStretcher () {}
};

struct FDecalSliderAnim : public FDecalAnimator
{
	FDecalSliderAnim (const char *name) : FDecalAnimator (name) {}
	DThinker *CreateThinker (DBaseDecal *actor, side_t *wall) const;

	int SlideStart;
	int SlideTime;
	fixed_t /*DistX,*/ DistY;
};

class DDecalSlider : public DDecalThinker
{
	DECLARE_CLASS (DDecalSlider, DDecalThinker)
public:
	DDecalSlider (DBaseDecal *decal) : DDecalThinker (decal) {}
	void Serialize (FArchive &arc);
	void Tick ();

	int TimeToStart;
	int TimeToStop;
/*	fixed_t DistX; */
	fixed_t DistY;
	fixed_t StartX;
	fixed_t StartY;
	bool bStarted;
private:
	DDecalSlider () {}
};

struct FDecalCombinerAnim : public FDecalAnimator
{
	FDecalCombinerAnim (const char *name) : FDecalAnimator (name) {}
	DThinker *CreateThinker (DBaseDecal *actor, side_t *wall) const;

	int FirstAnimator;
	int NumAnimators;

	static TArray<FDecalAnimator *> AnimatorList;
};

TArray<FDecalAnimator *> FDecalCombinerAnim::AnimatorList;

static const char *DecalKeywords[] =
{
	"x-scale",
	"y-scale",
	"pic",
	"solid",
	"add",
	"translucent",
	"flipx",
	"flipy",
	"randomflipx",
	"randomflipy",
	"fullbright",
	"fuzzy",
	"shade",
	"colors",
	"animator",
	"lowerdecal",
	NULL
};

enum
{
	DECAL_XSCALE,
	DECAL_YSCALE,
	DECAL_PIC,
	DECAL_SOLID,
	DECAL_ADD,
	DECAL_TRANSLUCENT,
	DECAL_FLIPX,
	DECAL_FLIPY,
	DECAL_RANDOMFLIPX,
	DECAL_RANDOMFLIPY,
	DECAL_FULLBRIGHT,
	DECAL_FUZZY,
	DECAL_SHADE,
	DECAL_COLORS,
	DECAL_ANIMATOR,
	DECAL_LOWERDECAL
};

const FDecalTemplate *FDecalBase::GetDecal () const
{
	return NULL;
}

void FDecalTemplate::ReplaceDecalRef(FDecalBase *from, FDecalBase *to)
{
	if (LowerDecal == from)
	{
		LowerDecal = to;
	}
}

FDecalLib::FDecalLib ()
{
	Root = NULL;
	Translations = NULL;
}

FDecalLib::~FDecalLib ()
{
	Clear ();
}

void FDecalLib::Clear ()
{
	FTranslation *trans;

	DelTree (Root);
	Root = NULL;
	
	trans = Translations;
	while (trans != NULL)
	{
		FTranslation *next = trans->Next;
		delete trans;
		trans = next;
	}
}

void FDecalLib::DelTree (FDecalBase *root)
{
	if (root != NULL)
	{
		DelTree (root->Left);
		DelTree (root->Right);
		delete root;
	}
}

void FDecalLib::ReadAllDecals ()
{
	int lump, lastlump = 0;
	unsigned int i;

	while ((lump = Wads.FindLump ("DECALDEF", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, "DECALDEF");
		ReadDecals ();
		SC_Close ();
	}
	// Supporting code to allow specifying decals directly in the DECORATE lump
	for (i = 0; i < PClass::m_RuntimeActors.Size(); i++)
	{
		AActor *def = (AActor*)GetDefaultByType (PClass::m_RuntimeActors[i]);

		FName v = ENamedName(intptr_t(def->DecalGenerator));
		if (v.IsValidName())
		{
			def->DecalGenerator = ScanTreeForName (v, Root);
		}
	}
}

void FDecalLib::ReadDecals ()
{
	while (SC_GetString ())
	{
		if (SC_Compare ("decal"))
		{
			ParseDecal ();
		}
		else if (SC_Compare ("decalgroup"))
		{
			ParseDecalGroup ();
		}
		else if (SC_Compare ("generator"))
		{
			ParseGenerator ();
		}
		else if (SC_Compare ("fader"))
		{
			ParseFader ();
		}
		else if (SC_Compare ("stretcher"))
		{
			ParseStretcher ();
		}
		else if (SC_Compare ("slider"))
		{
			ParseSlider ();
		}
		else if (SC_Compare ("combiner"))
		{
			ParseCombiner ();
		}
		else if (SC_Compare ("colorchanger"))
		{
			ParseColorchanger ();
		}
		else
		{
			SC_ScriptError (NULL);
		}
	}
}

BYTE FDecalLib::GetDecalID ()
{
	SC_MustGetString ();
	if (!IsNum (sc_String))
	{
		SC_UnGet ();
		return 0;
	}
	else
	{
		unsigned long num = strtoul (sc_String, NULL, 10);
		if (num < 1 || num > 255)
		{
			SC_ScriptError ("Decal ID must be between 1 and 255");
		}
		return (BYTE)num;
	}
}

void FDecalLib::ParseDecal ()
{
	char decalName[64];
	BYTE decalNum;
	FDecalTemplate newdecal;
	int code, picnum;

	SC_MustGetString ();
	strcpy (decalName, sc_String);
	decalNum = GetDecalID ();
	SC_MustGetStringName ("{");

	memset (&newdecal, 0, sizeof(newdecal));
	newdecal.PicNum = 0xffff;
	newdecal.ScaleX = newdecal.ScaleY = FRACUNIT;
	newdecal.RenderFlags = RF_WALLSPRITE;
	newdecal.RenderStyle = STYLE_Normal;
	newdecal.Alpha = 0x8000;

	for (;;)
	{
		SC_MustGetString ();
		if (SC_Compare ("}"))
		{
			AddDecal (decalName, decalNum, newdecal);
			break;
		}
		switch ((code = SC_MustMatchString (DecalKeywords)))
		{
		case DECAL_XSCALE:
			newdecal.ScaleX = ReadScale ();
			break;

		case DECAL_YSCALE:
			newdecal.ScaleY = ReadScale ();
			break;

		case DECAL_PIC:
			SC_MustGetString ();
			picnum = TexMan.CheckForTexture (sc_String, FTexture::TEX_Any);
			if (picnum < 0 && (picnum = Wads.CheckNumForName (sc_String, ns_graphics)) >= 0)
			{
				picnum = TexMan.CreateTexture (picnum, FTexture::TEX_Decal);
			}
			newdecal.PicNum = picnum;
			break;

		case DECAL_SOLID:
			newdecal.RenderStyle = STYLE_Normal;
			break;

		case DECAL_ADD:
			SC_MustGetFloat ();
			newdecal.Alpha = (WORD)(32768.f * sc_Float);
			newdecal.RenderStyle = STYLE_Add;
			break;

		case DECAL_TRANSLUCENT:
			SC_MustGetFloat ();
			newdecal.Alpha = (WORD)(32768.f * sc_Float);
			newdecal.RenderStyle = STYLE_Translucent;
			break;

		case DECAL_FLIPX:
			newdecal.RenderFlags |= RF_XFLIP;
			break;

		case DECAL_FLIPY:
			newdecal.RenderFlags |= RF_YFLIP;
			break;

		case DECAL_RANDOMFLIPX:
			newdecal.RenderFlags |= FDecalTemplate::DECAL_RandomFlipX;
			break;

		case DECAL_RANDOMFLIPY:
			newdecal.RenderFlags |= FDecalTemplate::DECAL_RandomFlipY;
			break;

		case DECAL_FULLBRIGHT:
			newdecal.RenderFlags |= RF_FULLBRIGHT;
			break;

		case DECAL_FUZZY:
			newdecal.RenderStyle = STYLE_Fuzzy;
			break;

		case DECAL_SHADE:
			SC_MustGetString ();
			newdecal.RenderStyle = STYLE_Shaded;
			newdecal.ShadeColor = V_GetColor (NULL, sc_String);
			newdecal.ShadeColor |=
				ColorMatcher.Pick (RPART(newdecal.ShadeColor),
					GPART(newdecal.ShadeColor), BPART(newdecal.ShadeColor)) << 24;
			break;

		case DECAL_COLORS:
			DWORD startcolor, endcolor;

			SC_MustGetString (); startcolor = V_GetColor (NULL, sc_String);
			SC_MustGetString (); endcolor   = V_GetColor (NULL, sc_String);
			newdecal.Translation = GenerateTranslation (startcolor, endcolor)->Index;
			break;

		case DECAL_ANIMATOR:
			SC_MustGetString ();
			newdecal.Animator = FindAnimator (sc_String);
			break;

		case DECAL_LOWERDECAL:
			SC_MustGetString ();
			newdecal.LowerDecal = GetDecalByName (sc_String);
			break;
		}
	}
}

void FDecalLib::ParseDecalGroup ()
{
	char groupName[64];
	BYTE decalNum;
	FDecalBase *targetDecal;
	FDecalGroup *group;

	SC_MustGetString ();
	strcpy (groupName, sc_String);
	decalNum = GetDecalID ();
	SC_MustGetStringName ("{");

	group = new FDecalGroup;

	for (;;)
	{
		SC_MustGetString ();
		if (SC_Compare ("}"))
		{
			group->Name = groupName;
			group->SpawnID = decalNum;
			AddDecal (group);
			break;
		}

		targetDecal = ScanTreeForName (sc_String, Root);
		if (targetDecal == NULL)
		{
			SC_ScriptError ("%s has not been defined", sc_String);
		}
		SC_MustGetNumber ();

		group->AddDecal (targetDecal, sc_Number);
	}
}

void FDecalLib::ParseGenerator ()
{
	const PClass *type;
	FDecalBase *decal;
	AActor *actor;

	// Get name of generator (actor)
	SC_MustGetString ();
	type = PClass::FindClass (sc_String);
	if (type == NULL || type->ActorInfo == NULL)
	{
		SC_ScriptError ("%s is not an actor.", sc_String);
	}
	actor = (AActor *)type->Defaults;

	// Get name of generated decal
	SC_MustGetString ();
	if (stricmp (sc_String, "None") == 0)
	{
		decal = NULL;
	}
	else
	{
		decal = ScanTreeForName (sc_String, Root);
		if (decal == NULL)
		{
			SC_ScriptError ("%s has not been defined.", sc_String);
		}
	}

	actor->DecalGenerator = decal;
	decal->Users.Push (type);
}

void FDecalLib::ParseFader ()
{
	char faderName[64];
	int startTime = 0, decayTime = 0;

	SC_MustGetString ();
	strcpy (faderName, sc_String);
	SC_MustGetStringName ("{");
	
	for (;;)
	{
		SC_MustGetString ();
		if (SC_Compare ("}"))
		{
			FDecalFaderAnim *fader = new FDecalFaderAnim (faderName);
			fader->DecayStart = startTime;
			fader->DecayTime = decayTime;
			Animators.Push (fader);
			break;
		}
		else if (SC_Compare ("DecayStart"))
		{
			SC_MustGetFloat ();
			startTime = (int)(sc_Float * TICRATE);
		}
		else if (SC_Compare ("DecayTime"))
		{
			SC_MustGetFloat ();
			decayTime = (int)(sc_Float * TICRATE);
		}
		else
		{
			SC_ScriptError ("Unknown fader parameter '%s'", sc_String);
		}
	}
}

void FDecalLib::ParseStretcher ()
{
	char stretcherName[64];
	fixed_t goalX = -1, goalY = -1;
	int startTime = 0, takeTime = 0;

	SC_MustGetString ();
	strcpy (stretcherName, sc_String);
	SC_MustGetStringName ("{");
	
	for (;;)
	{
		SC_MustGetString ();
		if (SC_Compare ("}"))
		{
			if (goalX >= 0 || goalY >= 0)
			{
				FDecalStretcherAnim *stretcher = new FDecalStretcherAnim (stretcherName);
				stretcher->StretchStart = startTime;
				stretcher->StretchTime = takeTime;
				stretcher->GoalX = goalX;
				stretcher->GoalY = goalY;
				Animators.Push (stretcher);
			}
			break;
		}
		else if (SC_Compare ("StretchStart"))
		{
			SC_MustGetFloat ();
			startTime = (int)(sc_Float * TICRATE);
		}
		else if (SC_Compare ("StretchTime"))
		{
			SC_MustGetFloat ();
			takeTime = (int)(sc_Float * TICRATE);
		}
		else if (SC_Compare ("GoalX"))
		{
			goalX = ReadScale ();
		}
		else if (SC_Compare ("GoalY"))
		{
			goalY = ReadScale ();
		}
		else
		{
			SC_ScriptError ("Unknown stretcher parameter '%s'", sc_String);
		}
	}
}

void FDecalLib::ParseSlider ()
{
	char sliderName[64];
	fixed_t distX = 0, distY = 0;
	int startTime = 0, takeTime = 0;

	SC_MustGetString ();
	strcpy (sliderName, sc_String);
	SC_MustGetStringName ("{");
	
	for (;;)
	{
		SC_MustGetString ();
		if (SC_Compare ("}"))
		{
			if ((/*distX |*/ distY) != 0)
			{
				FDecalSliderAnim *slider = new FDecalSliderAnim (sliderName);
				slider->SlideStart = startTime;
				slider->SlideTime = takeTime;
				/*slider->DistX = distX;*/
				slider->DistY = distY;
				Animators.Push (slider);
			}
			break;
		}
		else if (SC_Compare ("SlideStart"))
		{
			SC_MustGetFloat ();
			startTime = (int)(sc_Float * TICRATE);
		}
		else if (SC_Compare ("SlideTime"))
		{
			SC_MustGetFloat ();
			takeTime = (int)(sc_Float * TICRATE);
		}
		else if (SC_Compare ("DistX"))
		{
			SC_MustGetFloat ();
			distX = (fixed_t)(sc_Float * FRACUNIT);
			Printf ("DistX in slider decal %s is unsupported\n", sliderName);
		}
		else if (SC_Compare ("DistY"))
		{
			SC_MustGetFloat ();
			distY = (fixed_t)(sc_Float * FRACUNIT);
		}
		else
		{
			SC_ScriptError ("Unknown slider parameter '%s'", sc_String);
		}
	}
}

void FDecalLib::ParseColorchanger ()
{
	char faderName[64];
	int startTime = 0, decayTime = 0;
	PalEntry goal = 0;

	SC_MustGetString ();
	strcpy (faderName, sc_String);
	SC_MustGetStringName ("{");
	
	for (;;)
	{
		SC_MustGetString ();
		if (SC_Compare ("}"))
		{
			FDecalColorerAnim *fader = new FDecalColorerAnim (faderName);
			fader->DecayStart = startTime;
			fader->DecayTime = decayTime;
			fader->GoalColor = goal;
			Animators.Push (fader);
			break;
		}
		else if (SC_Compare ("FadeStart"))
		{
			SC_MustGetFloat ();
			startTime = (int)(sc_Float * TICRATE);
		}
		else if (SC_Compare ("FadeTime"))
		{
			SC_MustGetFloat ();
			decayTime = (int)(sc_Float * TICRATE);
		}
		else if (SC_Compare ("Color"))
		{
			SC_MustGetString ();
			goal = V_GetColor (NULL, sc_String);
		}
		else
		{
			SC_ScriptError ("Unknown color changer parameter '%s'", sc_String);
		}
	}
}

void FDecalLib::ParseCombiner ()
{
	char combinerName[64];
	size_t first = FDecalCombinerAnim::AnimatorList.Size ();

	SC_MustGetString ();
	strcpy (combinerName, sc_String);
	SC_MustGetStringName ("{");
	SC_MustGetString ();
	while (!SC_Compare ("}"))
	{
		FDecalAnimator *anim = FindAnimator (sc_String);
		if (anim == NULL)
		{
			SC_ScriptError ("Undefined animator %s", sc_String);
		}
		FDecalCombinerAnim::AnimatorList.Push (anim);
		SC_MustGetString ();
	}

	size_t last = FDecalCombinerAnim::AnimatorList.Size ();

	if (last > first)
	{
		FDecalCombinerAnim *combiner = new FDecalCombinerAnim (combinerName);
		combiner->FirstAnimator = (int)first;
		combiner->NumAnimators = (int)(last - first);
		Animators.Push (combiner);
	}
}

void FDecalLib::ReplaceDecalRef (FDecalBase *from, FDecalBase *to, FDecalBase *root)
{
	if (root == NULL)
	{
		return;
	}
	ReplaceDecalRef (from, to, root->Left);
	ReplaceDecalRef (from, to, root->Right);
	root->ReplaceDecalRef (from, to);
}

void FDecalLib::AddDecal (const char *name, BYTE num, const FDecalTemplate &decal)
{
	FDecalTemplate *newDecal = new FDecalTemplate;

	*newDecal = decal;
	newDecal->Name = name;
	newDecal->SpawnID = num;
	AddDecal (newDecal);
}

void FDecalLib::AddDecal (FDecalBase *decal)
{
	FDecalBase *node = Root, **prev = &Root;
	int num = decal->SpawnID;

	decal->SpawnID = 0;

	// Check if this decal already exists.
	while (node != NULL)
	{
		int lexx = stricmp (decal->Name, node->Name);
		if (lexx == 0)
		{
			break;
		}
		else if (lexx < 0)
		{
			prev = &node->Left;
			node = node->Left;
		}
		else
		{
			prev = &node->Right;
			node = node->Right;
		}
	}
	if (node == NULL)
	{ // No, add it.
		decal->SpawnID = 0;
		*prev = decal;
		decal->Left = NULL;
		decal->Right = NULL;
	}
	else
	{ // Yes, replace the old one.
		// If this decal has been used as the lowerdecal for another decal,
		// be sure and update the lowerdecal to use the new decal.
		ReplaceDecalRef(node, decal, Root);

		decal->Left = node->Left;
		decal->Right = node->Right;
		*prev = decal;

		// Fix references to the old decal so that they use the new one instead.
		for (unsigned int i = 0; i < node->Users.Size(); ++i)
		{
			((AActor *)node->Users[i]->Defaults)->DecalGenerator = decal;
		}
		decal->Users = node->Users;
		delete node;
	}
	// If this decal has an ID, make sure no existing decals have the same ID.
	if (num != 0)
	{
		FDecalBase *spawner = ScanTreeForNum (num, Root);
		if (spawner != NULL)
		{
			spawner->SpawnID = 0;
		}
		decal->SpawnID = num;
	}
}

const FDecalTemplate *FDecalLib::GetDecalByNum (BYTE num) const
{
	if (num == 0)
	{
		return NULL;
	}
	FDecalBase *base = ScanTreeForNum (num, Root);
	if (base != NULL)
	{
		return base->GetDecal ();
	}
	return NULL;
}

const FDecalTemplate *FDecalLib::GetDecalByName (const char *name) const
{
	if (name == NULL)
	{
		return NULL;
	}
	FDecalBase *base = ScanTreeForName (name, Root);
	if (base != NULL)
	{
		return static_cast<FDecalTemplate *>(base);
	}
	return NULL;
}

FDecalBase *FDecalLib::ScanTreeForNum (const BYTE num, FDecalBase *root)
{
	while (root != NULL)
	{
		if (root->SpawnID == num)
		{
			break;
		}
		FDecalBase *leftres = ScanTreeForNum (num, root->Left);
		if (leftres != NULL)
			return leftres;
		root = root->Right;		// Avoid tail-recursion
	}
	return root;
}

FDecalBase *FDecalLib::ScanTreeForName (const char *name, FDecalBase *root)
{
	while (root != NULL)
	{
		int lexx = stricmp (name, root->Name);
		if (lexx == 0)
		{
			break;
		}
		else if (lexx < 0)
		{
			root = root->Left;
		}
		else
		{
			root = root->Right;
		}
	}
	return root;
}

FDecalLib::FTranslation *FDecalLib::GenerateTranslation (DWORD start, DWORD end)
{
	FTranslation *trans;

	if (Translations != NULL)
	{
		trans = Translations->LocateTranslation (start, end);
	}
	else
	{
		trans = NULL;
	}
	if (trans == NULL)
	{
		trans = new FTranslation (start, end);
		trans->Next = Translations;
		Translations = trans;
	}
	return trans;
}

FDecalBase::FDecalBase ()
{
	Name = NAME_None;
}

FDecalBase::~FDecalBase ()
{
}

void FDecalTemplate::ApplyToDecal (DBaseDecal *decal, side_t *wall) const
{
	if (RenderStyle == STYLE_Shaded)
	{
		decal->SetShade (ShadeColor);
	}
	decal->Translation = Translation;
	decal->ScaleX = ScaleX;
	decal->ScaleY = ScaleY;
	decal->PicNum = PicNum;
	decal->Alpha = Alpha << 1;
	decal->RenderStyle = RenderStyle;
	decal->RenderFlags = (RenderFlags & ~(DECAL_RandomFlipX|DECAL_RandomFlipY)) |
		(decal->RenderFlags & (RF_RELMASK|RF_CLIPMASK|RF_INVISIBLE|RF_ONESIDED));
	if (RenderFlags & (DECAL_RandomFlipX|DECAL_RandomFlipY))
	{
		decal->RenderFlags ^= pr_decal() &
			((RenderFlags & (DECAL_RandomFlipX|DECAL_RandomFlipY)) >> 8);
	}
	if (Animator != NULL)
	{
		Animator->CreateThinker (decal, wall);
	}
}

const FDecalTemplate *FDecalTemplate::GetDecal () const
{
	return this;
}

FDecalLib::FTranslation::FTranslation (DWORD start, DWORD end)
{
	DWORD ri, gi, bi, rs, gs, bs;
	PalEntry *first, *last;
	BYTE *table;
	unsigned int i, tablei;

	StartColor = start;
	EndColor = end;
	Next = NULL;

	if (DecalTranslations.Size() == 256*256)
	{
		Printf ("Too many decal translations defined\n");
		Index = 0;
		return;
	}

	first = (PalEntry *)&StartColor;
	last = (PalEntry *)&EndColor;

	ri = first->r << 24;
	gi = first->g << 24;
	bi = first->b << 24;
	rs = last->r << 24;
	gs = last->g << 24;
	bs = last->b << 24;

	rs = (rs - ri) / 255;
	gs = (gs - ri) / 255;
	bs = (bs - bi) / 255;

	tablei = DecalTranslations.Reserve(256);
	table = &DecalTranslations[tablei];

	for (i = 1; i < 256; i++, ri += rs, gi += gs, bi += bs)
	{
		table[i] = ColorMatcher.Pick (ri >> 24, gi >> 24, bi >> 24);
	}
	table[0] = table[1];
	Index = (WORD)TRANSLATION(TRANSLATION_Decals, tablei >> 8);
}

FDecalLib::FTranslation *FDecalLib::FTranslation::LocateTranslation (DWORD start, DWORD end)
{
	FTranslation *trans = this;

	do
	{
		if (start == trans->StartColor && end == trans->EndColor)
		{
			return trans;
		}
		trans = trans->Next;
	} while (trans != NULL);
	return trans;
}

const FDecalTemplate *FDecalGroup::GetDecal () const
{
	const FDecalBase *decal = Choices.PickEntry ();
	const FDecalBase *remember;

	// Repeatedly GetDecal() until the result is constant, since
	// the choice might be another FDecalGroup.
	do
	{
		remember = decal;
		decal = decal->GetDecal ();
	} while (decal != remember);
	return static_cast<const FDecalTemplate *>(decal);
}

FDecalAnimator::FDecalAnimator (const char *name)
{
	Name = name;
}

FDecalAnimator::~FDecalAnimator ()
{
}

IMPLEMENT_CLASS (DDecalFader)

void DDecalFader::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << TimeToStartDecay << TimeToEndDecay << StartTrans;
}

void DDecalFader::Tick ()
{
	if (TheDecal == NULL)
	{
		Destroy ();
	}
	else
	{
		if (level.maptime < TimeToStartDecay || bglobal.freeze)
		{
			return;
		}
		else if (level.maptime >= TimeToEndDecay)
		{
			TheDecal->Destroy ();		// remove the decal
			Destroy ();					// remove myself
		}
		if (StartTrans == -1)
		{
			StartTrans = TheDecal->Alpha;
		}

		int distanceToEnd = TimeToEndDecay - level.maptime;
		int fadeDistance = TimeToEndDecay - TimeToStartDecay;
		TheDecal->Alpha = Scale (StartTrans, distanceToEnd, fadeDistance);

		if (TheDecal->RenderStyle < STYLE_Translucent)
		{
			TheDecal->RenderStyle = STYLE_Translucent;
		}
	}
}

DThinker *FDecalFaderAnim::CreateThinker (DBaseDecal *actor, side_t *wall) const
{
	DDecalFader *fader = new DDecalFader (actor);

	fader->TimeToStartDecay = level.maptime + DecayStart;
	fader->TimeToEndDecay = fader->TimeToStartDecay + DecayTime;
	fader->StartTrans = -1;
	return fader;
}

IMPLEMENT_CLASS (DDecalStretcher)

void DDecalStretcher::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << TimeToStart
		<< TimeToStop
		<< GoalX
		<< StartX
		<< bStretchX
		<< GoalY
		<< StartY
		<< bStretchY
		<< bStarted;
}

DThinker *FDecalStretcherAnim::CreateThinker (DBaseDecal *actor, side_t *wall) const
{
	DDecalStretcher *thinker = new DDecalStretcher (actor);

	thinker->TimeToStart = level.maptime + StretchStart;
	thinker->TimeToStop = thinker->TimeToStart + StretchTime;

	if (GoalX >= 0)
	{
		thinker->GoalX = GoalX;
		thinker->bStretchX = true;
	}
	else
	{
		thinker->bStretchX = false;
	}
	if (GoalY >= 0)
	{
		thinker->GoalY = GoalY;
		thinker->bStretchY = true;
	}
	else
	{
		thinker->bStretchY = false;
	}
	thinker->bStarted = false;
	return thinker;
}

void DDecalStretcher::Tick ()
{
	if (TheDecal == NULL)
	{
		Destroy ();
		return;
	}
	if (level.maptime < TimeToStart || bglobal.freeze)
	{
		return;
	}
	if (level.maptime >= TimeToStop)
	{
		if (bStretchX)
		{
			TheDecal->ScaleX = GoalX;
		}
		if (bStretchY)
		{
			TheDecal->ScaleY = GoalY;
		}
		Destroy ();
		return;
	}
	if (!bStarted)
	{
		bStarted = true;
		StartX = TheDecal->ScaleX;
		StartY = TheDecal->ScaleY;
	}

	int distance = level.maptime - TimeToStart;
	int maxDistance = TimeToStop - TimeToStart;
	if (bStretchX)
	{
		TheDecal->ScaleX = StartX + Scale (GoalX - StartX, distance, maxDistance);
	}
	if (bStretchY)
	{
		TheDecal->ScaleY = StartY + Scale (GoalY - StartY, distance, maxDistance);
	}
}

IMPLEMENT_CLASS (DDecalSlider)

void DDecalSlider::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << TimeToStart
		<< TimeToStop
		/*<< DistX*/
		<< DistY
		/*<< StartX*/
		<< StartY
		<< bStarted;
}

DThinker *FDecalSliderAnim::CreateThinker (DBaseDecal *actor, side_t *wall) const
{
	DDecalSlider *thinker = new DDecalSlider (actor);

	thinker->TimeToStart = level.maptime + SlideStart;
	thinker->TimeToStop = thinker->TimeToStart + SlideTime;
	/*thinker->DistX = DistX;*/
	thinker->DistY = DistY;
	thinker->bStarted = false;
	return thinker;
}

void DDecalSlider::Tick ()
{
	if (TheDecal == NULL)
	{
		Destroy ();
		return;
	}
	if (level.maptime < TimeToStart || bglobal.freeze)
	{
		return;
	}
	if (!bStarted)
	{
		bStarted = true;
		/*StartX = TheDecal->LeftDistance;*/
		StartY = TheDecal->Z;
	}
	if (level.maptime >= TimeToStop)
	{
		/*TheDecal->LeftDistance = StartX + DistX;*/
		TheDecal->Z = StartY + DistY;
		Destroy ();
		return;
	}

	int distance = level.maptime - TimeToStart;
	int maxDistance = TimeToStop - TimeToStart;
	/*TheDecal->LeftDistance = StartX + Scale (DistX, distance, maxDistance);*/
	TheDecal->Z = StartY + Scale (DistY, distance, maxDistance);
}

DThinker *FDecalCombinerAnim::CreateThinker (DBaseDecal *actor, side_t *wall) const
{
	DThinker *thinker = NULL;

	for (int i = 0; i < NumAnimators; ++i)
	{
		thinker = AnimatorList[FirstAnimator+i]->CreateThinker (actor, wall);
	}
	return thinker;
}

FDecalAnimator *FDecalLib::FindAnimator (const char *name)
{
	int i;

	for (i = (int)Animators.Size ()-1; i >= 0; --i)
	{
		if (stricmp (name, Animators[i]->Name) == 0)
		{
			return Animators[i];
		}
	}
	return NULL;
}


IMPLEMENT_CLASS (DDecalColorer)

void DDecalColorer::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << TimeToStartDecay << TimeToEndDecay << StartColor << GoalColor;
}

void DDecalColorer::Tick ()
{
	if (TheDecal == NULL || TheDecal->RenderStyle != STYLE_Shaded)
	{
		Destroy ();
	}
	else
	{
		if (level.maptime < TimeToStartDecay || bglobal.freeze)
		{
			return;
		}
		else if (level.maptime >= TimeToEndDecay)
		{
			TheDecal->SetShade (GoalColor);
			Destroy ();					// remove myself
		}
		if (StartColor.a == 255)
		{
			StartColor = TheDecal->AlphaColor & 0xffffff;
			if (StartColor == GoalColor)
			{
				Destroy ();
				return;
			}
		}
		if (level.maptime & 0)
		{ // Changing the shade can be expensive, so don't do it too often.
			return;
		}

		int distance = level.maptime - TimeToStartDecay;
		int maxDistance = TimeToEndDecay - TimeToStartDecay;
		int r = StartColor.r + Scale (GoalColor.r - StartColor.r, distance, maxDistance);
		int g = StartColor.g + Scale (GoalColor.g - StartColor.g, distance, maxDistance);
		int b = StartColor.b + Scale (GoalColor.b - StartColor.b, distance, maxDistance);
		TheDecal->SetShade (r, g, b);
	}
}

DThinker *FDecalColorerAnim::CreateThinker (DBaseDecal *actor, side_t *wall) const
{
	DDecalColorer *Colorer = new DDecalColorer (actor);

	Colorer->TimeToStartDecay = level.maptime + DecayStart;
	Colorer->TimeToEndDecay = Colorer->TimeToStartDecay + DecayTime;
	Colorer->StartColor = 0xff000000;
	Colorer->GoalColor = GoalColor;
	return Colorer;
}

static fixed_t ReadScale ()
{
	SC_MustGetFloat ();
	return fixed_t(clamp (sc_Float * FRACUNIT, 256.0, 256.0*FRACUNIT));
}
