// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Cheat code. See *_sbar.cpp for status bars.
//
//-----------------------------------------------------------------------------

//#include "am_map.h"
#include "d_protocol.h"
#include "gstrings.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "d_event.h"
#include "gi.h"

#define COUNT_CHEATS(l)		(sizeof(l)/sizeof(l[0]))

EXTERN_CVAR (Bool, ticker);
EXTERN_CVAR (Bool, noisedebug);

extern int AutoMapCheat;

struct cheatseq_t
{
	BYTE *Sequence;
	BYTE *Pos;
	BYTE DontCheck;
	BYTE CurrentArg;
	BYTE Args[2];
	BOOL (*Handler)(cheatseq_t *);
};

static bool CheatAddKey (cheatseq_t *cheat, byte key, BOOL *eat);
static BOOL Cht_Generic (cheatseq_t *);
static BOOL Cht_Music (cheatseq_t *);
static BOOL Cht_BeholdMenu (cheatseq_t *);
static BOOL Cht_AutoMap (cheatseq_t *);
static BOOL Cht_ChangeLevel (cheatseq_t *);
static BOOL Cht_MyPos (cheatseq_t *);
static BOOL Cht_Sound (cheatseq_t *);
static BOOL Cht_Ticker (cheatseq_t *);

BYTE CheatPowerup[7][10] =
{
	{ 'i','d','b','e','h','o','l','d','v', 255 },
	{ 'i','d','b','e','h','o','l','d','s', 255 },
	{ 'i','d','b','e','h','o','l','d','i', 255 },
	{ 'i','d','b','e','h','o','l','d','r', 255 },
	{ 'i','d','b','e','h','o','l','d','a', 255 },
	{ 'i','d','b','e','h','o','l','d','l', 255 },
	{ 'i','d','b','e','h','o','l','d', 255 }
};

// Smashing Pumpkins Into Small Piles Of Putrid Debris. 
static BYTE CheatNoclip[] =		{ 'i','d','s','p','i','s','p','o','p','d',255 };
static BYTE CheatNoclip2[] =	{ 'i','d','c','l','i','p',255 };
static BYTE CheatMus[] =		{ 'i','d','m','u','s',0,0,255 };
static BYTE CheatChoppers[] =	{ 'i','d','c','h','o','p','p','e','r','s',255 };
static BYTE CheatGod[] =		{ 'i','d','d','q','d',255 };
static BYTE CheatAmmo[] =		{ 'i','d','k','f','a',255 };
static BYTE CheatAmmoNoKey[] =	{ 'i','d','f','a',255 };
static BYTE CheatClev[] =		{ 'i','d','c','l','e','v',0,0,255 };
static BYTE CheatMypos[] =		{ 'i','d','m','y','p','o','s',255 };
static BYTE CheatAmap[] =		{ 'i','d','d','t',255 };

static BYTE CheatQuicken[] =	{ 'q','u','i','c','k','e','n',255 };
static BYTE CheatKitty[] =		{ 'k','i','t','t','y',255 };
static BYTE CheatRambo[] =		{ 'r','a','m','b','o',255 };
static BYTE CheatShazam[] =		{ 's','h','a','z','a','m',255 };
static BYTE CheatPonce[] =		{ 'p','o','n','c','e',255 };
static BYTE CheatSkel[] =		{ 's','k','e','l',255 };
static BYTE CheatNoise[] =		{ 'n','o','i','s','e',255 };
static BYTE CheatTicker[] =		{ 't','i','c','k','e','r',255 };
static BYTE CheatEngage[] =		{ 'e','n','g','a','g','e',0,0,255 };
static BYTE CheatChicken[] =	{ 'c','o','c','k','a','d','o','o','d','l','e','d','o','o',255 };
static BYTE CheatMassacre[] =	{ 'm','a','s','s','a','c','r','e',255 };
static BYTE CheatRavMap[] =		{ 'r','a','v','m','a','p',255 };

static BYTE CheatSatan[] =		{ 's','a','t','a','n',255 };
static BYTE CheatCasper[] =		{ 'c','a','s','p','e','r',255 };
static BYTE CheatNRA[] =		{ 'n','r','a',255 };
static BYTE CheatClubMed[] =	{ 'c','l','u','b','m','e','d',255 };
static BYTE CheatLocksmith[] =	{ 'l','o','c','k','s','m','i','t','h',255 };
static BYTE CheatIndiana[] =	{ 'i','n','d','i','a','n','a',255 };
static BYTE CheatSherlock[] =	{ 's','h','e','r','l','o','c','k',255 };
static BYTE CheatVisit[] =		{ 'v','i','s','i','t',0,0,255 };
static BYTE CheatPig[] =		{ 'd','e','l','i','v','e','r','a','n','c','e',255 };
static BYTE CheatButcher[] =	{ 'b','u','t','c','h','e','r',255 };
static BYTE CheatConan[] =		{ 'c','o','n','a','n',255 };
static BYTE CheatMapsco[] =		{ 'm','a','p','s','c','o',255 };
static BYTE CheatWhere[] =		{ 'w','h','e','r','e',255 };
#if 0
static BYTE CheatClass1[] =		{ 's','h','a','d','o','w','c','a','s','t','e','r',255 };
static BYTE CheatClass2[] =		{ 's','h','a','d','o','w','c','a','s','t','e','r',0,255 };
static BYTE CheatInit[] =		{ 'i','n','i','t',255 };
static BYTE CheatScript1[] =	{ 'p','u','k','e',255 };
static BYTE CheatScript2[] =	{ 'p','u','k','e',0,255 };
static BYTE CheatScript3[] =	{ 'p','u','k','e',0,0,255 };
#endif

static cheatseq_t DoomCheats[] =
{
	{ CheatMus,				0, 1, 0, {0,0},				Cht_Music },
	{ CheatPowerup[6],		0, 1, 0, {0,0},				Cht_BeholdMenu },
	{ CheatMypos,			0, 1, 0, {0,0},				Cht_MyPos },
	{ CheatAmap,			0, 0, 0, {0,0},				Cht_AutoMap },
	{ CheatGod,				0, 0, 0, {CHT_IDDQD,0},		Cht_Generic },
	{ CheatAmmo,			0, 0, 0, {CHT_IDKFA,0},		Cht_Generic },
	{ CheatAmmoNoKey,		0, 0, 0, {CHT_IDFA,0},		Cht_Generic },
	{ CheatNoclip,			0, 0, 0, {CHT_NOCLIP,0},	Cht_Generic },
	{ CheatNoclip2,			0, 0, 0, {CHT_NOCLIP,0},	Cht_Generic },
	{ CheatPowerup[0],		0, 0, 0, {CHT_BEHOLDV,0},	Cht_Generic },
	{ CheatPowerup[1],		0, 0, 0, {CHT_BEHOLDS,0},	Cht_Generic },
	{ CheatPowerup[2],		0, 0, 0, {CHT_BEHOLDI,0},	Cht_Generic },
	{ CheatPowerup[3],		0, 0, 0, {CHT_BEHOLDR,0},	Cht_Generic },
	{ CheatPowerup[4],		0, 0, 0, {CHT_BEHOLDA,0},	Cht_Generic },
	{ CheatPowerup[5],		0, 0, 0, {CHT_BEHOLDL,0},	Cht_Generic },
	{ CheatChoppers,		0, 0, 0, {CHT_CHAINSAW,0},	Cht_Generic },
	{ CheatClev,			0, 0, 0, {0,0},				Cht_ChangeLevel }
};

static cheatseq_t HereticCheats[] =
{
	{ CheatNoise,			0, 1, 0, {0,0},				Cht_Sound },
	{ CheatTicker,			0, 1, 0, {0,0},				Cht_Ticker },
	{ CheatRavMap,			0, 0, 0, {0,0},				Cht_AutoMap },
	{ CheatQuicken,			0, 0, 0, {CHT_GOD,0},		Cht_Generic },
	{ CheatKitty,			0, 0, 0, {CHT_NOCLIP,0},	Cht_Generic },
	{ CheatRambo,			0, 0, 0, {CHT_IDFA,0},		Cht_Generic },
	{ CheatShazam,			0, 0, 0, {CHT_POWER,0},		Cht_Generic },
	{ CheatPonce,			0, 0, 0, {CHT_HEALTH,0},	Cht_Generic },
	{ CheatSkel,			0, 0, 0, {CHT_KEYS,0},		Cht_Generic },
	{ CheatChicken,			0, 0, 0, {CHT_MORPH,0},		Cht_Generic },
	{ CheatAmmo,			0, 0, 0, {CHT_TAKEWEAPS,0},	Cht_Generic },
	{ CheatGod,				0, 0, 0, {CHT_NOWUDIE,0},	Cht_Generic },
	{ CheatMassacre,		0, 0, 0, {CHT_MASSACRE,0},	Cht_Generic },
	{ CheatEngage,			0, 0, 0, {0,0},				Cht_ChangeLevel }
};

static cheatseq_t HexenCheats[] =
{
	{ CheatSatan,			0, 0, 0, {CHT_GOD,0},		Cht_Generic },
	{ CheatCasper,			0, 0, 0, {CHT_NOCLIP,0},	Cht_Generic },
	{ CheatNRA,				0, 0, 0, {CHT_IDFA,0},		Cht_Generic },
	{ CheatClubMed,			0, 0, 0, {CHT_HEALTH,0},	Cht_Generic },
	{ CheatLocksmith,		0, 0, 0, {CHT_KEYS,0},		Cht_Generic },
	{ CheatIndiana,			0, 0, 0, {CHT_ALLARTI,0},	Cht_Generic },
	{ CheatSherlock,		0, 0, 0, {CHT_PUZZLE,0},	Cht_Generic },
	{ CheatVisit,			0, 0, 0, {0,0},				Cht_ChangeLevel },
	{ CheatPig,				0, 0, 0, {CHT_MORPH,0},		Cht_Generic },
	{ CheatButcher,			0, 0, 0, {CHT_MASSACRE,0},	Cht_Generic },
	{ CheatConan,			0, 0, 0, {CHT_TAKEWEAPS,0},	Cht_Generic },
	{ CheatWhere,			0, 1, 0, {0,0},				Cht_MyPos },
	{ CheatMapsco,			0, 0, 0, {0,0},				Cht_AutoMap }
};

extern BOOL CheckCheatmode ();

// Respond to keyboard input events, intercept cheats.
// [RH] Cheats eat the last keypress used to trigger them
BOOL ST_Responder (event_t *ev)
{
	BOOL eat = false;

#if 0
	// Filter automap on/off.
	if (ev->type == EV_KeyUp && ((ev->data1 & 0xffff0000) == AM_MSGHEADER))
	{
		switch (ev->data1 & 0xffff0000)
		{
		case AM_MSGENTERED:
			break;
			
		case AM_MSGEXITED:
			break;
		}
	}
	else
#endif

	if (ev->type == EV_KeyDown)
	{
		cheatseq_t *cheats;
		int numcheats;
		int i;

		if (gameinfo.gametype == GAME_Doom)
		{
			cheats = DoomCheats;
			numcheats = COUNT_CHEATS(DoomCheats);
		}
		else if (gameinfo.gametype == GAME_Heretic)
		{
			cheats = HereticCheats;
			numcheats = COUNT_CHEATS(HereticCheats);
		}
		else if (gameinfo.gametype == GAME_Hexen)
		{
			cheats = HexenCheats;
			numcheats = COUNT_CHEATS(HexenCheats);
		}
		else
		{
			return false;
		}

		for (i = 0; i < numcheats; i++, cheats++)
		{
			if (CheatAddKey (cheats, (byte)ev->data2, &eat))
			{
				if (cheats->DontCheck || !CheckCheatmode ())
				{
					eat |= cheats->Handler (cheats);
				}
			}
		}
	}
	return eat;
}

//--------------------------------------------------------------------------
//
// FUNC CheatAddkey
//
// Returns true if the added key completed the cheat, false otherwise.
//
//--------------------------------------------------------------------------

static bool CheatAddKey (cheatseq_t *cheat, byte key, BOOL *eat)
{
	if (cheat->Pos == NULL)
	{
		cheat->Pos = cheat->Sequence;
		cheat->CurrentArg = 0;
	}
	if (*cheat->Pos == 0)
	{
		*eat = true;
		cheat->Args[cheat->CurrentArg++] = key;
		cheat->Pos++;
	}
	else if (key == *cheat->Pos)
	{
		cheat->Pos++;
	}
	else
	{
		cheat->Pos = cheat->Sequence;
		cheat->CurrentArg = 0;
	}
	if (*cheat->Pos == 0xff)
	{
		cheat->Pos = cheat->Sequence;
		cheat->CurrentArg = 0;
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------
//
// CHEAT FUNCTIONS
//
//--------------------------------------------------------------------------

static BOOL Cht_Generic (cheatseq_t *cheat)
{
	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (cheat->Args[0]);
	return true;
}

static BOOL Cht_Music (cheatseq_t *cheat)
{
	char buf[9] = "idmus xx";

	buf[6] = cheat->Args[0];
	buf[7] = cheat->Args[1];
	C_DoCommand (buf);
	return true;
}

static BOOL Cht_BeholdMenu (cheatseq_t *cheat)
{
	Printf ("%s\n", GStrings(STSTR_BEHOLD));
	return false;
}

static BOOL Cht_AutoMap (cheatseq_t *cheat)
{
	if (automapactive)
	{
		AutoMapCheat = (AutoMapCheat + 1) % 3;
		return true;
	}
	else
	{
		return false;
	}
}

static BOOL Cht_ChangeLevel (cheatseq_t *cheat)
{
	char cmd[10] = "idclev xx";

	cmd[7] = cheat->Args[0];
	cmd[8] = cheat->Args[1];
	C_DoCommand (cmd);
	return true;
}

static BOOL Cht_MyPos (cheatseq_t *cheat)
{
	C_DoCommand ("toggle idmypos");
	return true;
}

static BOOL Cht_Ticker (cheatseq_t *cheat)
{
	ticker = !ticker;
	Printf ("%s\n", GStrings (ticker ? TXT_CHEATTICKERON : TXT_CHEATTICKEROFF));
	return true;
}

static BOOL Cht_Sound (cheatseq_t *cheat)
{
	noisedebug = !noisedebug;
	Printf ("%s\n", GStrings (noisedebug ? TXT_CHEATSOUNDON : TXT_CHEATSOUNDOFF));
	return true;
}
