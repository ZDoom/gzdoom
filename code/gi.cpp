#include <stdlib.h>
#include "info.h"
#include "gi.h"

gameinfo_t gameinfo;

static char *quitsounds[8] =
{
	"player/male/death1",
	"demon/pain",
	"grunt/pain",
	"misc/gibbed",
	"misc/teleport",
	"grunt/sight1",
	"grunt/sight3",
	"demon/melee"
};

static char *quitsounds2[8] =
{
	"vile/active",
	"misc/p_pkup",
	"brain/cube",
	"misc/gibbed",
	"skeleton/swing",
	"knight/death",
	"baby/active",
	"demon/melee"
};

static gameborder_t DoomBorder =
{
	8, 8,
	"brdr_tl", "brdr_t", "brdr_tr",
	"brdr_l",			 "brdr_r",
	"brdr_bl", "brdr_b", "brdr_br"
};

static gameborder_t HereticBorder =
{
	4, 16,
	"bordtl", "bordt", "bordtr",
	"bordl",           "bordr",
	"bordbl", "bordb", "bordbr"
};

gameinfo_t HexenGameInfo =
{
	GI_PAGESARERAW | GI_MAPxx | GI_NOLOOPFINALEMUSIC | GI_INFOINDEXED,
	"TITLE",
	"CREDIT",
	"CREDIT",
	"HEXEN",
	280/35,
	210/35,
	200/35,
	"Chat",
	"hall",
	{ '-','N','O','F','L','A','T','-' },
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "TITLE", 4 } },
	NULL,
	33,
	"F_022",
	&HereticBorder
};

gameinfo_t HereticGameInfo =
{
	GI_PAGESARERAW | GI_INFOINDEXED,
	"TITLE",
	"CREDIT",
	"CREDIT",
	{ 'M','U','S','_','T','I','T','L' },
	280/35,
	210/35,
	200/35,
	"misc/chat",
	{ 'M','U','S','_','C','P','T','D' },
	"FLOOR25",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "TITLE", 4 } },
	NULL,
	17,
	"FLAT513",
	&HereticBorder
};

gameinfo_t HereticSWGameInfo =
{
	GI_PAGESARERAW | GI_SHAREWARE | GI_INFOINDEXED,
	"TITLE",
	"CREDIT",
	"ORDER",
	{ 'M','U','S','_','T','I','T','L' },
	280/35,
	210/35,
	200/35,
	"misc/chat",
	{ 'M','U','S','_','C','P','T','D' },
	"FLOOR25",
	"ORDER",
	"CREDIT",
	"CREDIT",
	{ { "TITLE", 5 } },
	NULL,
	17,
	"FLOOR04",
	&HereticBorder
};

gameinfo_t SharewareGameInfo =
{
	GI_SHAREWARE | GI_NOCRAZYDEATH,
	{ 'T','I','T','L','E','P','I','C' },
	"CREDIT",
	"HELP2",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	{ 'D','_','V','I','C','T','O','R' },
	{ 'F','L','O','O','R','4','_','8' },
	"HELP2",
	{ 'V','I','C','T','O','R','Y','2' },
	"ENDPIC",
	{ { "HELP1", "HELP2" } },
	quitsounds,
	1,
	{ 'F','L','O','O','R','7','_','2' },
	&DoomBorder
};

gameinfo_t RegisteredGameInfo =
{
	GI_NOCRAZYDEATH,
	{ 'T','I','T','L','E','P','I','C' },
	"CREDIT",
	"HELP2",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	{ 'D','_','V','I','C','T','O','R' },
	{ 'F','L','O','O','R','4','_','8' },
	"HELP2",
	{ 'V','I','C','T','O','R','Y','2' },
	"ENDPIC",
	{ { "HELP1", "HELP2" } },
	quitsounds,
	2,
	{ 'F','L','O','O','R','7','_','2' },
	&DoomBorder
};

gameinfo_t RetailGameInfo =
{
	GI_MENUHACK_RETAIL | GI_NOCRAZYDEATH,
	{ 'T','I','T','L','E','P','I','C' },
	"CREDIT",
	"CREDIT",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	{ 'D','_','V','I','C','T','O','R' },
	{ 'F','L','O','O','R','4','_','8' },
	"CREDIT",
	{ 'V','I','C','T','O','R','Y','2' },
	"ENDPIC",
	{ { "HELP1", "CREDIT" } },
	quitsounds,
	2,
	{ 'F','L','O','O','R','7','_','2' },
	&DoomBorder
};

gameinfo_t CommercialGameInfo =
{
	GI_MAPxx | GI_MENUHACK_COMMERCIAL,
	{ 'T','I','T','L','E','P','I','C' },
	"CREDIT",
	"CREDIT",
	{ 'D','_','D','M','2','T','T','L' },
	11,
	0,
	200/35,
	"misc/chat",
	{ 'D','_','R','E','A','D','_','M' },
	"SLIME16",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "HELP", "CREDIT" } },
	quitsounds2,
	3,
	"GRNROCK",
	&DoomBorder
};
