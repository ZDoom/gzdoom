#include "decallib.h"
#include "sc_man.h"
#include "w_wad.h"
#include "v_video.h"
#include "v_palette.h"
#include "cmdlib.h"
#include "m_random.h"
#include "weightedlist.h"

FDecalLib DecalLibrary;

// A decal group holds multiple decals and returns one randomly
// when GetDecal() is called.

class FDecalGroup : public FDecalBase
{
	friend class FDecalLib;

public:
	FDecalGroup () : Choices (pr_decalchoice) {}
	FDecal *GetDecal ();
	void AddDecal (FDecalBase *decal, WORD weight)
	{
		Choices.AddEntry (decal, weight);
	}

private:
	TWeightedList<FDecalBase *> Choices;
};

struct FDecalLib::FTranslation
{
	FTranslation (DWORD start, DWORD end);
	FTranslation *LocateTranslation (DWORD start, DWORD end);

	DWORD StartColor, EndColor;
	FTranslation *Next;
	BYTE PalRemap[256];
};

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
	DECAL_COLORS
};

FDecal *FDecalBase::GetDecal ()
{
	return NULL;
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

	while ((lump = W_FindLump ("DECALDEF", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, "DECALDEF");
		ReadDecals ();
		SC_Close ();
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
	byte decalNum;
	FDecal newdecal;
	int code;

	SC_MustGetString ();
	strcpy (decalName, sc_String);
	decalNum = GetDecalID ();
	SC_MustGetStringName ("{");

	memset (&newdecal, 0, sizeof(newdecal));
	newdecal.PicNum = 0xffff;
	newdecal.ScaleX = newdecal.ScaleY = 63;
	newdecal.RenderFlags = RF_WALLSPRITE;
	newdecal.RenderStyle = STYLE_Normal;
	newdecal.Alpha = 0x8000;

	for (;;)
	{
		SC_GetString ();
		if (SC_Compare ("}"))
		{
			AddDecal (decalName, decalNum, newdecal);
			break;
		}
		switch ((code = SC_MustMatchString (DecalKeywords)))
		{
		case DECAL_XSCALE:
		case DECAL_YSCALE:
			SC_MustGetNumber ();
			if (--sc_Number < 0)
				sc_Number = 0;
			if (sc_Number > 255)
				sc_Number = 255;
			if (code == DECAL_XSCALE)
				newdecal.ScaleX = sc_Number;
			else
				newdecal.ScaleY = sc_Number;
			break;

		case DECAL_PIC:
			SC_MustGetString ();
			newdecal.PicNum = R_CheckTileNumForName (sc_String, TILE_Patch);
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
			newdecal.RenderFlags |= FDecal::DECAL_RandomFlipX;
			break;

		case DECAL_RANDOMFLIPY:
			newdecal.RenderFlags |= FDecal::DECAL_RandomFlipY;
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
			newdecal.Translation = GenerateTranslation (startcolor, endcolor)->PalRemap;
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
		SC_GetString ();
		if (SC_Compare ("}"))
		{
			group->Name = copystring (groupName);
			AddDecal (group);
			break;
		}

		targetDecal = ScanTreeForName (sc_String, Root);
		if (targetDecal == NULL)
		{
			const char *arg = sc_String;
			SC_ScriptError ("%s has not been defined", &arg);
		}
		SC_MustGetNumber ();

		group->AddDecal (targetDecal, sc_Number);
	}
}

void FDecalLib::ParseGenerator ()
{
	const TypeInfo *type;
	FDecalBase *decal;
	AActor *actor;

	// Get name of generator (actor)
	SC_MustGetString ();
	type = TypeInfo::FindType (sc_String);
	if (type == NULL || type->ActorInfo == NULL)
	{
		const char *eekMsg = "%s is not an actor.";
		const char *args[2];

		args[0] = sc_String;
		if (type == NULL)
		{
			type = TypeInfo::IFindType (sc_String);
			if (type != NULL)
			{
				args[1] = type->Name + 1;
				eekMsg = "%s is not an actor.\nDid you mean %s?";
			}
		}
		SC_ScriptError (eekMsg, args);
	}
	actor = (AActor *)type->ActorInfo->Defaults;

	// Get name of generated decal
	SC_MustGetString ();
	decal = ScanTreeForName (sc_String, Root);
	if (decal == NULL)
	{
		const char *arg = sc_String;
		SC_ScriptError ("%s has not been defined.", &arg);
	}

	actor->DecalGenerator = decal;
}

void FDecalLib::AddDecal (const char *name, byte num, const FDecal &decal)
{
	FDecal *newDecal = new FDecal;

	*newDecal = decal;
	newDecal->Name = copystring (name);
	newDecal->SpawnID = num;
	AddDecal (newDecal);
}

void FDecalLib::AddDecal (FDecalBase *decal)
{
	FDecalBase *node = Root, **prev = &Root;
	int num = decal->SpawnID;

	decal->SpawnID = 0;

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
	{
		decal->SpawnID = 0;
		*prev = decal;
		decal->Left = NULL;
		decal->Right = NULL;
	}
	else
	{
		decal->Left = node->Left;
		decal->Right = node->Right;
		*prev = decal;
		delete node;
	}
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

const FDecal *FDecalLib::GetDecalByNum (byte num) const
{
	if (num == 0)
	{
		return NULL;
	}
	return ScanTreeForNum (num, Root)->GetDecal ();
}

const FDecal *FDecalLib::GetDecalByName (const char *name) const
{
	if (name == NULL)
	{
		return NULL;
	}
	return ScanTreeForName (name, Root)->GetDecal ();
}

FDecalBase *FDecalLib::ScanTreeForNum (const BYTE num, FDecalBase *root)
{
	while (root != NULL)
	{
		if (root->SpawnID == num)
		{
			break;
		}
		ScanTreeForNum (num, root->Left);
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
	Name = NULL;
}

FDecalBase::~FDecalBase ()
{
	if (Name != NULL)
		delete[] Name;
}

void FDecal::ApplyToActor (AActor *actor) const
{
	if (RenderStyle == STYLE_Shaded)
	{
		actor->SetShade (ShadeColor);
	}
	actor->translation = Translation;
	actor->xscale = ScaleX;
	actor->yscale = ScaleY;
	actor->picnum = PicNum;
	actor->alpha = Alpha << 1;
	actor->RenderStyle = RenderStyle;
	actor->renderflags = (RenderFlags & ~(DECAL_RandomFlipX|DECAL_RandomFlipY)) |
		(actor->renderflags & (RF_RELMASK|RF_CLIPMASK|RF_INVISIBLE|RF_ONESIDED));
	if (RenderFlags & (DECAL_RandomFlipX|DECAL_RandomFlipY))
	{
		actor->renderflags ^= P_Random (pr_decal) &
			((RenderFlags & (DECAL_RandomFlipX|DECAL_RandomFlipY)) >> 8);
	}
}

FDecal *FDecal::GetDecal ()
{
	return this;
}

FDecalLib::FTranslation::FTranslation (DWORD start, DWORD end)
{
	DWORD ri, gi, bi, rs, gs, bs;
	PalEntry *first, *last;
	int i;

	StartColor = start;
	EndColor = end;
	Next = NULL;

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

	for (i = 1; i < 256; i++, ri += rs, gi += gs, bi += bs)
	{
		PalRemap[i] = ColorMatcher.Pick (ri >> 24, gi >> 24, bi >> 24);
	}
	PalRemap[0] = PalRemap[1];
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

FDecal *FDecalGroup::GetDecal ()
{
	FDecalBase *decal = Choices.PickEntry ();
	FDecalBase *remember;

	// Repeatedly GetDecal() until the result is the same, since
	// the choice might be another FDecalGroup.
	do
	{
		remember = decal;
		decal = decal->GetDecal ();
	} while (decal != remember);
	return static_cast<FDecal *>(decal);
}
