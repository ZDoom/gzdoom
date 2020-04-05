class StrifeKey : Key
{
	Default
	{
		Radius 20;
		Height 16;
		+NOTDMATCH
		+FLOORCLIP
	}
}

// Base Key -----------------------------------------------------------------

class BaseKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_FUSL";
		Tag "$TAG_BASEKEY";
		Inventory.PickupMessage "$TXT_BASEKEY";
	}
	States
	{
	Spawn:
		FUSL A -1;
		Stop;
	}
}


// Govs Key -----------------------------------------------------------------

class GovsKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_REBL";
		Tag "$TAG_GOVSKEY";
		Inventory.PickupMessage "$TXT_GOVSKEY";
	}
	States
	{
	Spawn:
		REBL A -1;
		Stop;
	}
}
	

// Passcard -----------------------------------------------------------------

class Passcard : StrifeKey
{
	Default
	{
		Inventory.Icon "I_TPAS";
		Tag "$TAG_PASSCARD";
		Inventory.PickupMessage "$TXT_PASSCARD";
	}
	States
	{
	Spawn:
		TPAS A -1;
		Stop;
	}
}


// ID Badge -----------------------------------------------------------------

class IDBadge : StrifeKey
{
	Default
	{
		Inventory.Icon "I_CRD1";
		Tag "$TAG_IDBADGE";
		Inventory.PickupMessage "$TXT_IDBADGE";
	}
	States
	{
	Spawn:
		CRD1 A -1;
		Stop;
	}
}


// Prison Key ---------------------------------------------------------------

class PrisonKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_PRIS";
		Tag "$TAG_PRISONKEY";
		Inventory.GiveQuest 11;
		Inventory.PickupMessage "$TXT_PRISONKEY";
	}
	States
	{
	Spawn:
		PRIS A -1;
		Stop;
	}
}


// Severed Hand -------------------------------------------------------------

class SeveredHand : StrifeKey
{
	Default
	{
		Inventory.Icon "I_HAND";
		Tag "$TAG_SEVEREDHAND";
		Inventory.GiveQuest 12;
		Inventory.PickupMessage "$TXT_SEVEREDHAND";
	}
	States
	{
	Spawn:
		HAND A -1;
		Stop;
	}
}


// Power1 Key ---------------------------------------------------------------

class Power1Key : StrifeKey
{
	Default
	{
		Inventory.Icon "I_PWR1";
		Tag "$TAG_POWER1KEY";
		Inventory.PickupMessage "$TXT_POWER1KEY";
	}
	States
	{
	Spawn:
		PWR1 A -1;
		Stop;
	}
}


// Power2 Key ---------------------------------------------------------------

class Power2Key : StrifeKey
{
	Default
	{
		Inventory.Icon "I_PWR2";
		Tag "$TAG_POWER2KEY";
		Inventory.PickupMessage "$TXT_POWER2KEY";
	}
	States
	{
	Spawn:
		PWR2 A -1;
		Stop;
	}
}


// Power3 Key ---------------------------------------------------------------

class Power3Key : StrifeKey
{
	Default
	{
		Inventory.Icon "I_PWR3";
		Tag "$TAG_POWER3KEY";
		Inventory.PickupMessage "$TXT_POWER3KEY";
	}
	States
	{
	Spawn:
		PWR3 A -1;
		Stop;
	}
}


// Gold Key -----------------------------------------------------------------

class GoldKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_KY1G";
		Tag "$TAG_GOLDKEY";
		Inventory.PickupMessage "$TXT_GOLDKEY";
	}
	States
	{
	Spawn:
		KY1G A -1;
		Stop;
	}
}


// ID Card ------------------------------------------------------------------

class IDCard : StrifeKey
{
	Default
	{
		Inventory.Icon "I_CRD2";
		Tag "$TAG_IDCARD";
		Inventory.PickupMessage "$TXT_IDCARD";
	}
	States
	{
	Spawn:
		CRD2 A -1;
		Stop;
	}
}


// Silver Key ---------------------------------------------------------------

class SilverKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_KY2S";
		Tag "$TAG_SILVERKEY";
		Inventory.PickupMessage "$TXT_SILVERKEY";
	}
	States
	{
	Spawn:
		KY2S A -1;
		Stop;
	}
}


// Oracle Key ---------------------------------------------------------------

class OracleKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_ORAC";
		Tag "$TAG_ORACLEKEY";
		Inventory.PickupMessage "$TXT_ORACLEKEY";
	}
	States
	{
	Spawn:
		ORAC A -1;
		Stop;
	}
}


// Military ID --------------------------------------------------------------

class MilitaryID : StrifeKey
{
	Default
	{
		Inventory.Icon "I_GYID";
		Tag "$TAG_MILITARYID";
		Inventory.PickupMessage "$TXT_MILITARYID";
	}
	States
	{
	Spawn:
		GYID A -1;
		Stop;
	}
}


// Order Key ----------------------------------------------------------------

class OrderKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_FUBR";
		Tag "$TAG_ORDERKEY";
		Inventory.PickupMessage "$TXT_ORDERKEY";
	}
	States
	{
	Spawn:
		FUBR A -1;
		Stop;
	}
}


// Warehouse Key ------------------------------------------------------------

class WarehouseKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_WARE";
		Tag "$TAG_WAREHOUSEKEY";
		Inventory.PickupMessage "$TXT_WAREHOUSEKEY";
	}
	States
	{
	Spawn:
		WARE A -1;
		Stop;
	}
}


// Brass Key ----------------------------------------------------------------

class BrassKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_KY3B";
		Tag "$TAG_BRASSKEY";
		Inventory.PickupMessage "$TXT_BRASSKEY";
	}
	States
	{
	Spawn:
		KY3B A -1;
		Stop;
	}
}


// Red Crystal Key ----------------------------------------------------------

class RedCrystalKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_RCRY";
		Tag "$TAG_REDCRYSTALKEY";
		Inventory.PickupMessage "$TXT_REDCRYSTAL";
	}
	States
	{
	Spawn:
		RCRY A -1 Bright;
		Stop;
	}
}


// Blue Crystal Key ---------------------------------------------------------

class BlueCrystalKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_BCRY";
		Tag "$TAG_BLUECRYSTALKEY";
		Inventory.PickupMessage "$TXT_BLUECRYSTAL";
	}
	States
	{
	Spawn:
		BCRY A -1 Bright;
		Stop;
	}
}


// Chapel Key ---------------------------------------------------------------

class ChapelKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_CHAP";
		Tag "$TAG_CHAPELKEY";
		Inventory.PickupMessage "$TXT_CHAPELKEY";
	}
	States
	{
	Spawn:
		CHAP A -1;
		Stop;
	}
}


// Catacomb Key -------------------------------------------------------------

class CatacombKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_TUNL";
		Tag "$TAG_CATACOMBKEY";
		Inventory.GiveQuest 28;
		Inventory.PickupMessage "$TXT_CATACOMBKEY";
	}
	States
	{
	Spawn:
		TUNL A -1;
		Stop;
	}
}


// Security Key -------------------------------------------------------------

class SecurityKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_SECK";
		Tag "$TAG_SECURITYKEY";
		Inventory.PickupMessage "$TXT_SECURITYKEY";
	}
	States
	{
	Spawn:
		SECK A -1;
		Stop;
	}
}


// Core Key -----------------------------------------------------------------

class CoreKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_GOID";
		Tag "$TAG_COREKEY";
		Inventory.PickupMessage "$TXT_COREKEY";
	}
	States
	{
	Spawn:
		GOID A -1;
		Stop;
	}
}


// Mauler Key ---------------------------------------------------------------

class MaulerKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_BLTK";
		Tag "$TAG_MAULERKEY";
		Inventory.PickupMessage "$TXT_MAULERKEY";
	}
	States
	{
	Spawn:
		BLTK A -1;
		Stop;
	}
}


// Factory Key --------------------------------------------------------------

class FactoryKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_PROC";
		Tag "$TAG_FACTORYKEY";
		Inventory.PickupMessage "$TXT_FACTORYKEY";
	}
	States
	{
	Spawn:
		PROC A -1;
		Stop;
	}
}


// Mine Key -----------------------------------------------------------------

class MineKey : StrifeKey
{
	Default
	{
		Inventory.Icon "I_MINE";
		Tag "$TAG_MINEKEY";
		Inventory.PickupMessage "$TXT_MINEKEY";
	}
	States
	{
	Spawn:
		MINE A -1;
		Stop;
	}
}


// New Key5 -----------------------------------------------------------------

class NewKey5 : StrifeKey
{
	Default
	{
		Inventory.Icon "I_BLTK";
		Tag "$TAG_NEWKEY5";
		Inventory.PickupMessage "$TXT_NEWKEY5";
	}
	States
	{
	Spawn:
		BLTK A -1;
		Stop;
	}
}


// Oracle Pass --------------------------------------------------------------

class OraclePass : Inventory
{
	Default
	{
		+INVENTORY.INVBAR
		Inventory.Icon "I_OTOK";
		Inventory.GiveQuest 18;
		Inventory.PickupMessage "$TXT_ORACLEPASS";
		Tag "$TAG_ORACLEPASS";
	}
	States
	{
	Spawn:
		OTOK A -1;
		Stop;
	}
}

