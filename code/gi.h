#ifndef __GI_H__
#define __GI_H__

#include "doomtype.h"

#define GI_MAPxx				0x00000001
#define GI_PAGESARERAW			0x00000002
#define GI_SHAREWARE			0x00000004
#define GI_NOLOOPFINALEMUSIC	0x00000008
#define GI_INFOINDEXED			0x00000010
#define GI_MENUHACK				0x00000060
#define GI_MENUHACK_RETAIL		0x00000020
#define GI_MENUHACK_EXTENDED	0x00000040	// (Heretic)
#define GI_MENUHACK_COMMERCIAL	0x00000060
#define GI_NOCRAZYDEATH			0x00000080

#ifndef EGAMETYPE
#define EGAMETYPE
enum EGameType
{
	GAME_Any	 = 0,
	GAME_Doom	 = 1,
	GAME_Heretic = 2,
	GAME_Hexen	 = 4,
	GAME_Raven	 = 6
};
#endif

extern const char *GameNames[5];

typedef struct
{
	byte offset;
	byte size;
	char tl[8];
	char t[8];
	char tr[8];
	char l[8];
	char r[8];
	char bl[8];
	char b[8];
	char br[8];
} gameborder_t;

typedef struct
{
	int flags;
	char titlePage[8];
	char creditPage1[8];
	char creditPage2[8];
	char titleMusic[8];
	float titleTime;
	float advisoryTime;
	float pageTime;
	char chatSound[16];
	char finaleMusic[8];
	char finaleFlat[8];
	char finalePage1[8];
	char finalePage2[8];
	char finalePage3[8];
	union
	{
		char infoPage[2][8];
		struct
		{
			char basePage[8];
			int numPages;
		} indexed;
	} info;
	char **quitSounds;
	int maxSwitch;
	char borderFlat[8];
	gameborder_t *border;
	int telefogheight;
	EGameType gametype;
	int defKickback;
} gameinfo_t;

extern gameinfo_t gameinfo;

#endif //__GI_H__