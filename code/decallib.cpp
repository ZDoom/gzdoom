#include "decallib.h"
#include "sc_man.h"
#include "w_wad.h"
#include "v_video.h"
#include "v_palette.h"
#include "cmdlib.h"

FDecalLib DecalLibrary;

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
	"translucency",
	"flipx",
	"flipy",
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
	DECAL_TRANSLUCENCY,
	DECAL_FLIPX,
	DECAL_FLIPY,
	DECAL_FULLBRIGHT,
	DECAL_FUZZY,
	DECAL_SHADE,
	DECAL_COLORS
};

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

void FDecalLib::DelTree (FDecal *root)
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
	char decalName[64];
	byte decalNum;
	FDecal newdecal;
	int code;

	while (SC_GetString ())
	{
		if (!SC_Compare ("decal"))
		{
			SC_ScriptError (NULL);
		}
		SC_MustGetString ();
		strcpy (decalName, sc_String);
		SC_MustGetString ();
		if (!IsNum (sc_String))
		{
			SC_UnGet ();
			decalNum = 0;
		}
		else
		{
			unsigned long num = strtoul (sc_String, NULL, 10);
			if (num < 1 || num > 255)
			{
				SC_ScriptError ("Decal ID must be between 1 and 255");
			}
			decalNum = (byte)num;
		}
		SC_MustGetStringName ("{");

		memset (&newdecal, 0, sizeof(newdecal));
		newdecal.PicNum = 0xffff;
		newdecal.ScaleX = newdecal.ScaleY = 63;
		newdecal.RenderFlags = RF_WALLSPRITE;
		newdecal.RenderStyle = STYLE_Translucent;
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

			case DECAL_TRANSLUCENCY:
				SC_MustGetFloat ();
				newdecal.Alpha = (WORD)(32768.f * sc_Float);
				break;

			case DECAL_FLIPX:
				newdecal.RenderFlags |= RF_XFLIP;
				break;

			case DECAL_FLIPY:
				newdecal.RenderFlags |= RF_YFLIP;
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
}

void FDecalLib::AddDecal (const char *name, byte num, const FDecal &decal)
{
	FDecal *node = Root, **prev = &Root;

	while (node != NULL)
	{
		int lexx = stricmp (name, node->Name);
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
		node = new FDecal;
		*node = decal;
		node->Name = copystring (name);
		node->SpawnID = num;
		node->Left = node->Right = NULL;
		*prev = node;
	}
	else
	{
		char *savename = node->Name;
		*node = decal;
		node->Name = savename;
		node->SpawnID = num;
	}
	if (num != 0)
	{
		FDecal *spawner = ScanTreeForNum (num, Root);
		if (spawner != node)
		{
			spawner->SpawnID = 0;
		}
	}
}

const FDecal *FDecalLib::GetDecalByNum (byte num) const
{
	if (num == 0)
	{
		return NULL;
	}
	FDecal *node = ScanTreeForNum (num, Root);
	return node;
}

const FDecal *FDecalLib::GetDecalByName (const char *name) const
{
	if (name == NULL)
	{
		return NULL;
	}
	FDecal *node = ScanTreeForName (name, Root);
	return node;
}

FDecal *FDecalLib::ScanTreeForNum (const BYTE num, FDecal *root)
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

FDecal *FDecalLib::ScanTreeForName (const char *name, FDecal *root)
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

FDecal::FDecal ()
{
	Name = NULL;
	Translation = NULL;
}

FDecal::~FDecal ()
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
	actor->renderflags = RenderFlags |
		(actor->renderflags & (RF_RELMASK|RF_CLIPMASK|RF_INVISIBLE|RF_ONESIDED));
	actor->alpha = Alpha << 1;
	actor->RenderStyle = RenderStyle;
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
