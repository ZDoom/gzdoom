#include "info.h"
#include "a_pickups.h"
#include "gstrings.h"
#include "a_keys.h"
#include "tarray.h"

IMPLEMENT_STATELESS_ACTOR (AHexenKey, Hexen, -1, 0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (20)
	PROP_Flags (MF_SPECIAL)
END_DEFAULTS

#define MAKEKEY(num,n1,name,ednum, spawn) \
	class AKey##n1 : public AHexenKey { \
		DECLARE_ACTOR (AKey##n1, AHexenKey) \
	public: \
		const char *PickupMessage () { return GStrings("TXT_KEY_" #name); } \
	}; \
	FState AKey##n1::States[] = { S_NORMAL (KEY##num, 'A', -1, NULL, NULL) }; \
	IMPLEMENT_ACTOR (AKey##n1, Hexen, ednum, spawn) PROP_SpawnState (0) \
	PROP_Inventory_Icon ("KEYSLOT" #num) END_DEFAULTS

MAKEKEY (1, Steel, STEEL, 8030, 85)
MAKEKEY (2, Cave, CAVE, 8031, 86)
MAKEKEY (3, Axe, AXE, 8032, 87)
MAKEKEY (4, Fire, FIRE, 8033, 88)
MAKEKEY (5, Emerald, EMERALD, 8034, 89)
MAKEKEY (6, Dungeon, DUNGEON, 8035, 90)
MAKEKEY (7, Silver, SILVER, 8036, 91)
MAKEKEY (8, Rusted, RUSTED, 8037, 92)
MAKEKEY (9, Horn, HORN, 8038, 93)
MAKEKEY (A, Swamp, SWAMP, 8039, 94)
MAKEKEY (B, Castle, CASTLE, 8200, 0)
