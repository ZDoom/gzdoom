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
	"-NOFLAT-",
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
	"MUS_TITL",
	280/35,
	210/35,
	200/35,
	"misc/chat",
	"MUS_CPTD",
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
	"MUS_TITL",
	280/35,
	210/35,
	200/35,
	"misc/chat",
	"MUS_CPTD",
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
	"TITLEPIC",
	"CREDIT",
	"HELP2",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	"D_VICTOR",
	"FLOOR4_8",
	"HELP2",
	"VICTORY2",
	"ENDPIC",
	{ { "HELP1", "HELP2" } },
	quitsounds,
	1,
	"FLOOR7_2",
	&DoomBorder
};

gameinfo_t RegisteredGameInfo =
{
	GI_NOCRAZYDEATH,
	"TITLEPIC",
	"CREDIT",
	"HELP2",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	"D_VICTOR",
	"FLOOR4_8",
	"HELP2",
	"VICTORY2",
	"ENDPIC",
	{ { "HELP1", "HELP2" } },
	quitsounds,
	2,
	"FLOOR7_2",
	&DoomBorder
};

gameinfo_t RetailGameInfo =
{
	GI_MENUHACK_RETAIL | GI_NOCRAZYDEATH,
	"TITLEPIC",
	"CREDIT",
	"CREDIT",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	"D_VICTOR",
	"FLOOR4_8",
	"CREDIT",
	"VICTORY2",
	"ENDPIC",
	{ { "HELP1", "CREDIT" } },
	quitsounds,
	2,
	"FLOOR7_2",
	&DoomBorder
};

gameinfo_t CommercialGameInfo =
{
	GI_MAPxx | GI_MENUHACK_COMMERCIAL,
	"TITLEPIC",
	"CREDIT",
	"CREDIT",
	"D_DM2TTL",
	11,
	0,
	200/35,
	"misc/chat",
	"D_READ_M",
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
