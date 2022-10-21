//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Cheat code. See *_sbar.cpp for status bars.
//
//-----------------------------------------------------------------------------

#include "d_protocol.h"
#include "gstrings.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "d_event.h"
#include "gi.h"
#include "d_net.h"
#include "doomstat.h"
#include "g_level.h"
#include "g_levellocals.h"
#include "d_main.h"

EXTERN_CVAR (Bool, ticker);
EXTERN_CVAR (Int, am_cheat);
EXTERN_CVAR (Int, cl_blockcheats);

struct cheatseq_t
{
	uint8_t *Sequence;
	uint8_t *Pos;
	uint8_t DontCheck;
	uint8_t CurrentArg;
	uint8_t Args[2];
	bool (*Handler)(cheatseq_t *);
};

static bool CheatCheckList (event_t *ev, cheatseq_t *cheats, int numcheats);
static bool CheatAddKey (cheatseq_t *cheat, uint8_t key, bool *eat);
static bool Cht_Generic (cheatseq_t *);
static bool Cht_Music (cheatseq_t *);
static bool Cht_BeholdMenu (cheatseq_t *);
static bool Cht_PumpupMenu (cheatseq_t *);
static bool Cht_AutoMap (cheatseq_t *);
static bool Cht_ChangeLevel (cheatseq_t *);
static bool Cht_ChangeStartSpot (cheatseq_t *);
static bool Cht_WarpTransLevel (cheatseq_t *);
static bool Cht_MyPos (cheatseq_t *);
static bool Cht_Sound (cheatseq_t *);
static bool Cht_Ticker (cheatseq_t *);

uint8_t CheatPowerup[7][10] =
{
	{ 'i','d','b','e','h','o','l','d','v', 255 },
	{ 'i','d','b','e','h','o','l','d','s', 255 },
	{ 'i','d','b','e','h','o','l','d','i', 255 },
	{ 'i','d','b','e','h','o','l','d','r', 255 },
	{ 'i','d','b','e','h','o','l','d','a', 255 },
	{ 'i','d','b','e','h','o','l','d','l', 255 },
	{ 'i','d','b','e','h','o','l','d', 255 },
};
uint8_t CheatPowerup1[11][7] =
{
	{ 'g','i','m','m','e','a',255 },
	{ 'g','i','m','m','e','b',255 },
	{ 'g','i','m','m','e','c',255 },
	{ 'g','i','m','m','e','d',255 },
	{ 'g','i','m','m','e','e',255 },
	{ 'g','i','m','m','e','f',255 },
	{ 'g','i','m','m','e','g',255 },
	{ 'g','i','m','m','e','h',255 },
	{ 'g','i','m','m','e','i',255 },
	{ 'g','i','m','m','e','j',255 },
	{ 'g','i','m','m','e','z',255 },
};
uint8_t CheatPowerup2[8][10] =
{
	{ 'p','u','m','p','u','p','b',255 },
	{ 'p','u','m','p','u','p','i',255 },
	{ 'p','u','m','p','u','p','m',255 },
	{ 'p','u','m','p','u','p','h',255 },
	{ 'p','u','m','p','u','p','p',255 },
	{ 'p','u','m','p','u','p','s',255 },
	{ 'p','u','m','p','u','p','t',255 },
	{ 'p','u','m','p','u','p',255 },
};

// Smashing Pumpkins Into Small Piles Of Putrid Debris. 
static uint8_t CheatNoclip[] =		{ 'i','d','s','p','i','s','p','o','p','d',255 };
static uint8_t CheatNoclip2[] =	{ 'i','d','c','l','i','p',255 };
static uint8_t CheatMus[] =		{ 'i','d','m','u','s',0,0,255 };
static uint8_t CheatChoppers[] =	{ 'i','d','c','h','o','p','p','e','r','s',255 };
static uint8_t CheatGod[] =		{ 'i','d','d','q','d',255 };
static uint8_t CheatAmmo[] =		{ 'i','d','k','f','a',255 };
static uint8_t CheatAmmoNoKey[] =	{ 'i','d','f','a',255 };
static uint8_t CheatClev[] =		{ 'i','d','c','l','e','v',0,0,255 };
static uint8_t CheatMypos[] =		{ 'i','d','m','y','p','o','s',255 };
static uint8_t CheatAmap[] =		{ 'i','d','d','t',255 };

static uint8_t CheatQuicken[] =	{ 'q','u','i','c','k','e','n',255 };
static uint8_t CheatKitty[] =		{ 'k','i','t','t','y',255 };
static uint8_t CheatRambo[] =		{ 'r','a','m','b','o',255 };
static uint8_t CheatShazam[] =		{ 's','h','a','z','a','m',255 };
static uint8_t CheatPonce[] =		{ 'p','o','n','c','e',255 };
static uint8_t CheatSkel[] =		{ 's','k','e','l',255 };
static uint8_t CheatNoise[] =		{ 'n','o','i','s','e',255 };
static uint8_t CheatTicker[] =		{ 't','i','c','k','e','r',255 };
static uint8_t CheatEngage[] =		{ 'e','n','g','a','g','e',0,0,255 };
static uint8_t CheatChicken[] =	{ 'c','o','c','k','a','d','o','o','d','l','e','d','o','o',255 };
static uint8_t CheatMassacre[] =	{ 'm','a','s','s','a','c','r','e',255 };
static uint8_t CheatRavMap[] =		{ 'r','a','v','m','a','p',255 };

static uint8_t CheatSatan[] =		{ 's','a','t','a','n',255 };
static uint8_t CheatCasper[] =		{ 'c','a','s','p','e','r',255 };
static uint8_t CheatNRA[] =		{ 'n','r','a',255 };
static uint8_t CheatClubMed[] =	{ 'c','l','u','b','m','e','d',255 };
static uint8_t CheatLocksmith[] =	{ 'l','o','c','k','s','m','i','t','h',255 };
static uint8_t CheatIndiana[] =	{ 'i','n','d','i','a','n','a',255 };
static uint8_t CheatSherlock[] =	{ 's','h','e','r','l','o','c','k',255 };
static uint8_t CheatVisit[] =		{ 'v','i','s','i','t',0,0,255 };
static uint8_t CheatPig[] =		{ 'd','e','l','i','v','e','r','a','n','c','e',255 };
static uint8_t CheatButcher[] =	{ 'b','u','t','c','h','e','r',255 };
static uint8_t CheatConan[] =		{ 'c','o','n','a','n',255 };
static uint8_t CheatMapsco[] =		{ 'm','a','p','s','c','o',255 };
static uint8_t CheatWhere[] =		{ 'w','h','e','r','e',255 };
#if 0
static uint8_t CheatClass1[] =		{ 's','h','a','d','o','w','c','a','s','t','e','r',255 };
static uint8_t CheatClass2[] =		{ 's','h','a','d','o','w','c','a','s','t','e','r',0,255 };
static uint8_t CheatInit[] =		{ 'i','n','i','t',255 };
static uint8_t CheatScript1[] =	{ 'p','u','k','e',255 };
static uint8_t CheatScript2[] =	{ 'p','u','k','e',0,255 };
static uint8_t CheatScript3[] =	{ 'p','u','k','e',0,0,255 };
#endif

static uint8_t CheatSpin[] =		{ 's','p','i','n',0,0,255 };
static uint8_t CheatRift[] =		{ 'r','i','f','t',0,0,255 };
static uint8_t CheatGPS[] =		{ 'g','p','s',255 };
static uint8_t CheatGripper[] =	{ 'g','r','i','p','p','e','r',255 };
static uint8_t CheatLego[] =		{ 'l','e','g','o',255 };
static uint8_t CheatDots[] =		{ 'd','o','t','s',255 };
static uint8_t CheatScoot[] =		{ 's','c','o','o','t',0,255 };
static uint8_t CheatDonnyTrump[] =	{ 'd','o','n','n','y','t','r','u','m','p',255 };
static uint8_t CheatOmnipotent[] =	{ 'o','m','n','i','p','o','t','e','n','t',255 };
static uint8_t CheatJimmy[] =		{ 'j','i','m','m','y',255 };
static uint8_t CheatBoomstix[] =	{ 'b','o','o','m','s','t','i','x',255 };
static uint8_t CheatStoneCold[] =	{ 's','t','o','n','e','c','o','l','d',255 };
static uint8_t CheatElvis[] =		{ 'e','l','v','i','s',255 };
static uint8_t CheatTopo[] =		{ 't','o','p','o',255 };

//[BL] Graf will probably get rid of this
static uint8_t CheatJoelKoenigs[] =	{ 'j','o','e','l','k','o','e','n','i','g','s',255 };
static uint8_t CheatDavidBrus[] =		{ 'd','a','v','i','d','b','r','u','s',255 };
static uint8_t CheatScottHolman[] =	{ 's','c','o','t','t','h','o','l','m','a','n',255 };
static uint8_t CheatMikeKoenigs[] =	{ 'm','i','k','e','k','o','e','n','i','g','s',255 };
static uint8_t CheatCharlesJacobi[] =	{ 'c','h','a','r','l','e','s','j','a','c','o','b','i',255 };
static uint8_t CheatAndrewBenson[] =	{ 'a','n','d','r','e','w','b','e','n','s','o','n',255 };
static uint8_t CheatDeanHyers[] =		{ 'd','e','a','n','h','y','e','r','s',255 };
static uint8_t CheatMaryBregi[] =		{ 'm','a','r','y','b','r','e','g','i',255 };
static uint8_t CheatAllen[] =			{ 'a','l','l','e','n',255 };
static uint8_t CheatDigitalCafe[] =	{ 'd','i','g','i','t','a','l','c','a','f','e',255 };
static uint8_t CheatJoshuaStorms[] =	{ 'j','o','s','h','u','a','s','t','o','r','m','s',255 };
static uint8_t CheatLeeSnyder[] =		{ 'l','e','e','s','n','y','d','e','r',0,0,255 };
static uint8_t CheatKimHyers[] =		{ 'k','i','m','h','y','e','r','s',255 };
static uint8_t CheatShrrill[] =		{ 's','h','e','r','r','i','l','l',255 };

static uint8_t CheatTNTem[] =		{ 't','n','t','e','m',255 };

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
	{ CheatClev,			0, 1, 0, {0,0},				Cht_ChangeLevel }
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
	{ CheatMassacre,		0, 0, 0, {CHT_MASSACRE,0},	Cht_Generic },
	{ CheatEngage,			0, 1, 0, {0,0},				Cht_ChangeLevel },
	{ CheatPowerup1[0],		0, 0, 0, {CHT_GIMMIEA,0},	Cht_Generic },
	{ CheatPowerup1[1],		0, 0, 0, {CHT_GIMMIEB,0},	Cht_Generic },
	{ CheatPowerup1[2],		0, 0, 0, {CHT_GIMMIEC,0},	Cht_Generic },
	{ CheatPowerup1[3],		0, 0, 0, {CHT_GIMMIED,0},	Cht_Generic },
	{ CheatPowerup1[4],		0, 0, 0, {CHT_GIMMIEE,0},	Cht_Generic },
	{ CheatPowerup1[5],		0, 0, 0, {CHT_GIMMIEF,0},	Cht_Generic },
	{ CheatPowerup1[6],		0, 0, 0, {CHT_GIMMIEG,0},	Cht_Generic },
	{ CheatPowerup1[7],		0, 0, 0, {CHT_GIMMIEH,0},	Cht_Generic },
	{ CheatPowerup1[8],		0, 0, 0, {CHT_GIMMIEI,0},	Cht_Generic },
	{ CheatPowerup1[9],		0, 0, 0, {CHT_GIMMIEJ,0},	Cht_Generic },
	{ CheatPowerup1[10],	0, 0, 0, {CHT_GIMMIEZ,0},	Cht_Generic },
	{ CheatAmmo,			0, 0, 0, {CHT_TAKEWEAPS,0},	Cht_Generic },
	{ CheatGod,				0, 0, 0, {CHT_NOWUDIE,0},	Cht_Generic },
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
	{ CheatVisit,			0, 0, 0, {0,0},				Cht_WarpTransLevel },
	{ CheatPig,				0, 0, 0, {CHT_MORPH,0},		Cht_Generic },
	{ CheatButcher,			0, 0, 0, {CHT_MASSACRE,0},	Cht_Generic },
	{ CheatConan,			0, 0, 0, {CHT_TAKEWEAPS,0},	Cht_Generic },
	{ CheatWhere,			0, 1, 0, {0,0},				Cht_MyPos },
	{ CheatMapsco,			0, 0, 0, {0,0},				Cht_AutoMap }
};

static cheatseq_t StrifeCheats[] =
{
	{ CheatSpin,			0, 1, 0, {0,0},				Cht_Music },
	{ CheatGPS,				0, 1, 0, {0,0},				Cht_MyPos },
	{ CheatTopo,			0, 0, 0, {0,0},				Cht_AutoMap },
	{ CheatDots,			0, 1, 0, {0,0},				Cht_Ticker },
	{ CheatOmnipotent,		0, 0, 0, {CHT_IDDQD,0},		Cht_Generic },
	{ CheatGripper,			0, 0, 0, {CHT_NOVELOCITY,0},Cht_Generic },
	{ CheatJimmy,			0, 0, 0, {CHT_KEYS,0},		Cht_Generic },
	{ CheatBoomstix,		0, 0, 0, {CHT_IDFA,0},		Cht_Generic },
	{ CheatElvis,			0, 0, 0, {CHT_NOCLIP,0},	Cht_Generic },
	{ CheatStoneCold,		0, 0, 0, {CHT_MASSACRE,0},	Cht_Generic },
	{ CheatPowerup2[7],		0, 1, 0, {0,0},				Cht_PumpupMenu },
	{ CheatPowerup2[0],		0, 0, 0, {CHT_BEHOLDS,0},	Cht_Generic },
	{ CheatPowerup2[1],		0, 0, 0, {CHT_PUMPUPI,0},	Cht_Generic },
	{ CheatPowerup2[2],		0, 0, 0, {CHT_PUMPUPM,0},	Cht_Generic },
	{ CheatPowerup2[3],		0, 0, 0, {CHT_PUMPUPH,0},	Cht_Generic },
	{ CheatPowerup2[4],		0, 0, 0, {CHT_PUMPUPP,0},	Cht_Generic },
	{ CheatPowerup2[5],		0, 0, 0, {CHT_PUMPUPS,0},	Cht_Generic },
	{ CheatPowerup2[6],		0, 0, 0, {CHT_PUMPUPT,0},	Cht_Generic },
	{ CheatRift,			0, 1, 0, {0,0},				Cht_ChangeLevel },
	{ CheatDonnyTrump,		0, 0, 0, {CHT_DONNYTRUMP,0},Cht_Generic },
	{ CheatScoot,			0, 0, 0, {0,0},				Cht_ChangeStartSpot },
	{ CheatLego,			0, 0, 0, {CHT_LEGO,0},		Cht_Generic },
};

static cheatseq_t ChexCheats[] =
{
	{ CheatKimHyers,		0, 1, 0, {0,0},				Cht_MyPos },
	{ CheatShrrill,			0, 0, 0, {0,0},				Cht_AutoMap },
	{ CheatDavidBrus,		0, 0, 0, {CHT_IDDQD,0},		Cht_Generic },
	{ CheatScottHolman,		0, 0, 0, {CHT_IDKFA,0},		Cht_Generic },
	{ CheatMikeKoenigs,		0, 0, 0, {CHT_IDFA,0},		Cht_Generic },
	{ CheatCharlesJacobi,	0, 0, 0, {CHT_NOCLIP,0},	Cht_Generic },
	{ CheatAndrewBenson,	0, 0, 0, {CHT_BEHOLDV,0},	Cht_Generic },
	{ CheatDeanHyers,		0, 0, 0, {CHT_BEHOLDS,0},	Cht_Generic },
	{ CheatMaryBregi,		0, 0, 0, {CHT_BEHOLDI,0},	Cht_Generic },
	{ CheatAllen,			0, 0, 0, {CHT_BEHOLDR,0},	Cht_Generic },
	{ CheatDigitalCafe,		0, 0, 0, {CHT_BEHOLDA,0},	Cht_Generic },
	{ CheatJoshuaStorms,	0, 0, 0, {CHT_BEHOLDL,0},	Cht_Generic },
	{ CheatJoelKoenigs,		0, 0, 0, {CHT_CHAINSAW,0},	Cht_Generic },
	{ CheatLeeSnyder,		0, 1, 0, {0,0},				Cht_ChangeLevel },
	{ CheatMus,				0, 1, 0, {0,0},				Cht_Music },
};

static cheatseq_t SpecialCheats[] =
{
	{ CheatTNTem,		0, 0, 0, {CHT_MASSACRE,0},	Cht_Generic }
};



CVAR(Bool, allcheats, false, CVAR_ARCHIVE)
CVAR(Bool, nocheats, false, CVAR_ARCHIVE)

// Respond to keyboard input events, intercept cheats.
// [RH] Cheats eat the last keypress used to trigger them
bool ST_Responder (event_t *ev)
{
	bool eat = false;

	if (nocheats || !!cl_blockcheats || (gameinfo.nokeyboardcheats && !allcheats))
	{
		return false;
	}
	else if (!allcheats)
	{
		cheatseq_t *cheats;
		int numcheats;

		switch (gameinfo.gametype)
		{
		case GAME_Doom:
			cheats = DoomCheats;
			numcheats = countof(DoomCheats);
			break;

		case GAME_Heretic:
			cheats = HereticCheats;
			numcheats = countof(HereticCheats);
			break;

		case GAME_Hexen:
			cheats = HexenCheats;
			numcheats = countof(HexenCheats);
			break;

		case GAME_Strife:
			cheats = StrifeCheats;
			numcheats = countof(StrifeCheats);
			break;

		case GAME_Chex:
			cheats = ChexCheats;
			numcheats = countof(ChexCheats);
			break;

		default:
			return false;
		}
		return CheatCheckList(ev, cheats, numcheats);
	}
	else
	{
		static cheatseq_t *cheatlists[] = { DoomCheats, HereticCheats, HexenCheats, StrifeCheats, ChexCheats, SpecialCheats };
		static int counts[] = { countof(DoomCheats), countof(HereticCheats)-2, countof(HexenCheats), 
								countof(StrifeCheats), countof(ChexCheats)-1, countof(SpecialCheats) };

		for (size_t i=0; i<countof(cheatlists); i++)
		{
			if (CheatCheckList(ev, cheatlists[i], counts[i])) return true;
		}
	}
	return false;
}

static bool CheatCheckList (event_t *ev, cheatseq_t *cheats, int numcheats)
{
	bool eat = false;

	if (ev->type == EV_KeyDown)
	{
		int i;

		for (i = 0; i < numcheats; i++, cheats++)
		{
			if (CheatAddKey (cheats, (uint8_t)ev->data2, &eat))
			{
				if (cheats->DontCheck || !CheckCheatmode ())
				{
					eat |= cheats->Handler (cheats);
				}
			}
			else if (cheats->Pos - cheats->Sequence > 2)
			{ // If more than two characters into the sequence,
			  // eat the keypress, just so that the Hexen cheats
			  // with T in them will work without unbinding T.
				eat = true;
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

static bool CheatAddKey (cheatseq_t *cheat, uint8_t key, bool *eat)
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

static bool Cht_Generic (cheatseq_t *cheat)
{
	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (cheat->Args[0]);
	return true;
}

static bool Cht_Music (cheatseq_t *cheat)
{
	char buf[9] = "idmus xx";

	buf[6] = cheat->Args[0];
	buf[7] = cheat->Args[1];
	C_DoCommand (buf);
	return true;
}

static bool Cht_BeholdMenu (cheatseq_t *cheat)
{
	Printf ("%s\n", GStrings("STSTR_BEHOLD"));
	return false;
}

static bool Cht_PumpupMenu (cheatseq_t *cheat)
{
	// How many people knew about the PUMPUPT cheat, since
	// it isn't printed in the list?
	Printf ("Bzrk, Inviso, Mask, Health, Pack, Stats\n");
	return false;
}

static bool Cht_AutoMap (cheatseq_t *cheat)
{
	if (automapactive)
	{
		am_cheat = (am_cheat + 1) % 3;
		return true;
	}
	else
	{
		return false;
	}
}

static bool Cht_ChangeLevel (cheatseq_t *cheat)
{
	char cmd[10] = "idclev xx";

	cmd[7] = cheat->Args[0];
	cmd[8] = cheat->Args[1];
	C_DoCommand (cmd);
	return true;
}

static bool Cht_ChangeStartSpot (cheatseq_t *cheat)
{
	char cmd[64];

	mysnprintf (cmd, countof(cmd), "changemap %s %c", primaryLevel->MapName.GetChars(), cheat->Args[0]);
	C_DoCommand (cmd);
	return true;
}

static bool Cht_WarpTransLevel (cheatseq_t *cheat)
{
	char cmd[11] = "hxvisit xx";
	cmd[8] = cheat->Args[0];
	cmd[9] = cheat->Args[1];
	C_DoCommand (cmd);
	return true;
}

static bool Cht_MyPos (cheatseq_t *cheat)
{
	C_DoCommand ("toggle idmypos");
	return true;
}

static bool Cht_Ticker (cheatseq_t *cheat)
{
	ticker = !ticker;
	Printf ("%s\n", GStrings(ticker ? "TXT_CHEATTICKERON" : "TXT_CHEATTICKEROFF"));
	return true;
}

static bool Cht_Sound (cheatseq_t *cheat)
{
	AddCommandString("stat sounddebug");
	return true;
}
