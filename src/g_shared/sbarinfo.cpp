#include "doomtype.h"
#include "doomstat.h"
#include "v_font.h"
#include "v_video.h"
#include "sbar.h"
#include "r_defs.h"
#include "w_wad.h"
#include "m_random.h"
#include "d_player.h"
#include "st_stuff.h"
#include "r_local.h"
#include "m_swap.h"
#include "a_keys.h"
#include "templates.h"
#include "i_system.h"
#include "sbarinfo.h"
#include "sc_man.h"
#include "gi.h"
#include "r_translate.h"

static FRandom pr_chainwiggle; //use the same method of chain wiggling as heretic.

#define ST_FACETIME		(TICRATE/2)
#define ST_PAINTIME		(TICRATE)
#define ST_GRINTIME		(TICRATE*2)
#define ST_RAMPAGETIME	(TICRATE*2)
#define ST_XDTHTIME		(TICRATE*(3/2))
#define ST_NUMFACES		82 //9 levels with 8 faces each, 3 god, 1 death, 6 xdeath
#define ARTIFLASH_OFFSET (invBarOffset+6)

EXTERN_CVAR(Int, fraglimit)

SBarInfo *SBarInfoScript;

enum //statusbar flags
{
	STATUSBARFLAG_FORCESCALED = 1,
};

enum //gametype flags
{
	GAMETYPE_SINGLEPLAYER = 1,
	GAMETYPE_COOPERATIVE = 2,
	GAMETYPE_DEATHMATCH = 4,
	GAMETYPE_TEAMGAME = 8,
};

enum //drawimage flags
{
	DRAWIMAGE_PLAYERICON = 1,
	DRAWIMAGE_AMMO1 = 2,
	DRAWIMAGE_AMMO2 = 4,
	DRAWIMAGE_INVENTORYICON = 8,
	DRAWIMAGE_TRANSLATABLE = 16,
	DRAWIMAGE_WEAPONSLOT = 32,
	DRAWIMAGE_SWITCHABLE_AND = 64,
	DRAWIMAGE_INVULNERABILITY = 128,
	DRAWIMAGE_OFFSET_CENTER = 256,
	DRAWIMAGE_ARMOR = 512,
	DRAWIMAGE_WEAPONICON = 1024,
};

enum //drawnumber flags
{
	DRAWNUMBER_HEALTH = 1,
	DRAWNUMBER_ARMOR = 2,
	DRAWNUMBER_AMMO1 = 4,
	DRAWNUMBER_AMMO2 = 8,
	DRAWNUMBER_AMMO = 16,
	DRAWNUMBER_AMMOCAPACITY = 32,
	DRAWNUMBER_FRAGS = 64,
	DRAWNUMBER_INVENTORY = 128,
	DRAWNUMBER_KILLS = 256,
	DRAWNUMBER_MONSTERS = 512,
	DRAWNUMBER_ITEMS = 1024,
	DRAWNUMBER_TOTALITEMS = 2048,
	DRAWNUMBER_SECRETS = 4096,
	DRAWNUMBER_TOTALSECRETS = 8192,
};

enum //drawbar flags (will go into special2)
{
	DRAWBAR_HORIZONTAL = 1,
	DRAWBAR_REVERSE = 2,
	DRAWBAR_COMPAREDEFAULTS = 4,
};

enum //drawselectedinventory flags
{
	DRAWSELECTEDINVENTORY_ALTERNATEONEMPTY = 1,
	DRAWSELECTEDINVENTORY_ARTIFLASH = 2,
	DRAWSELECTEDINVENTORY_ALWAYSSHOWCOUNTER = 4,
};

enum //drawinventorybar flags
{
	DRAWINVENTORYBAR_ALWAYSSHOW = 1,
	DRAWINVENTORYBAR_NOARTIBOX = 2,
	DRAWINVENTORYBAR_NOARROWS = 4,
	DRAWINVENTORYBAR_ALWAYSSHOWCOUNTER = 8,
};

enum //drawgem flags
{
	DRAWGEM_WIGGLE = 1,
	DRAWGEM_TRANSLATABLE = 2,
};

enum //drawshader flags
{
	DRAWSHADER_VERTICAL = 1,
	DRAWSHADER_REVERSE = 2,
};

enum //drawmugshot flags
{
	DRAWMUGSHOT_XDEATHFACE = 1,
	DRAWMUGSHOT_ANIMATEDGODMODE = 2,
};

static const char *SBarInfoTopLevel[] =
{
	"base",
	"height",
	"interpolatehealth",
	"statusbar",
	NULL
};
enum
{
	SBARINFO_BASE,
	SBARINFO_HEIGHT,
	SBARINFO_INTERPOLATEHEALTH,
	SBARINFO_STATUSBAR,
};

static const char *StatusBars[] =
{
	"none",
	"fullscreen",
	"normal",
	"automap",
	"inventory",
	"inventoryfullscreen",
	NULL
};
enum
{
	STBAR_NONE,
	STBAR_FULLSCREEN,
	STBAR_NORMAL,
	STBAR_AUTOMAP,
	STBAR_INVENTORY,
	STBAR_INVENTORYFULLSCREEN,
};

static const char *SBarInfoRoutineLevel[] =
{
	"drawimage",
	"drawnumber",
	"drawswitchableimage",
	"drawmugshot",
	"drawselectedinventory",
	"drawinventorybar",
	"drawbar",
	"drawgem",
	"drawshader",
	"drawstring",
	"gamemode",
	"playerclass",
	NULL
};
enum
{
	SBARINFO_DRAWIMAGE,
	SBARINFO_DRAWNUMBER,
	SBARINFO_DRAWSWITCHABLEIMAGE,
	SBARINFO_DRAWMUGSHOT,
	SBARINFO_DRAWSELECTEDINVENTORY,
	SBARINFO_DRAWINVENTORYBAR,
	SBARINFO_DRAWBAR,
	SBARINFO_DRAWGEM,
	SBARINFO_DRAWSHADER,
	SBARINFO_DRAWSTRING,
	SBARINFO_GAMEMODE,
	SBARINFO_PLAYERCLASS,
};

void FreeSBarInfoScript()
{
	if (SBarInfoScript != NULL)
	{
		delete SBarInfoScript;
		SBarInfoScript = NULL;
	}
}

//Laz Bar Script Reader
void SBarInfo::ParseSBarInfo(int lump)
{
	gameType = GAME_Any;
	SC_OpenLumpNum(lump, Wads.GetLumpFullName(lump));
	SC_SetCMode(true);
	while(SC_CheckToken(TK_Identifier) || SC_CheckToken(TK_Include))
	{
		if(sc_TokenType == TK_Include)
		{
			SC_MustGetToken(TK_StringConst);
			int lump = Wads.CheckNumForFullName(sc_String); //zip/pk3
			//Do a normal wad lookup.
			if (lump==-1 && strlen(sc_String) <= 8 && !strchr(sc_String, '/')) 
				lump = Wads.CheckNumForName(sc_String);
			if (lump==-1) 
				SC_ScriptError("Lump '%s' not found", sc_String);
			SC_SaveScriptState();
			ParseSBarInfo(lump);
			SC_RestoreScriptState();
			continue;
		}
		switch(SC_MustMatchString(SBarInfoTopLevel))
		{
			case SBARINFO_BASE:
				SC_MustGetToken(TK_Identifier);
				if(SC_Compare("Doom"))
					gameType = GAME_Doom;
				else if(SC_Compare("Heretic"))
					gameType = GAME_Heretic;
				else if(SC_Compare("Hexen"))
					gameType = GAME_Hexen;
				else if(SC_Compare("Strife"))
					gameType = GAME_Strife;
				else if(SC_Compare("None"))
					gameType = GAME_Any;
				else
					SC_ScriptError("Bad game name: %s", sc_String);
				SC_MustGetToken(';');
				break;
			case SBARINFO_HEIGHT:
				SC_MustGetToken(TK_IntConst);
				this->height = sc_Number;
				SC_MustGetToken(';');
				break;
			case SBARINFO_INTERPOLATEHEALTH: //mimics heretics interpolated health values.
				if(SC_CheckToken(TK_True))
				{
					interpolateHealth = true;
				}
				else
				{
					SC_TokenMustBe(TK_False);
					interpolateHealth = false;
				}
				if(SC_CheckToken(',')) //speed param
				{
					SC_MustGetToken(TK_IntConst);
					this->interpolationSpeed = sc_Number;
				}
				SC_MustGetToken(';');
				break;
			case SBARINFO_STATUSBAR:
			{
				SC_MustGetToken(TK_Identifier);
				int barNum = SC_MustMatchString(StatusBars);
				while(SC_CheckToken(','))
				{
					SC_MustGetToken(TK_Identifier);
					if(SC_Compare("forcescaled"))
					{
						this->huds[barNum].forceScaled = true;
					}
					else
					{
						SC_ScriptError("Unkown flag '%s'.", sc_String);
					}
				}
				SC_MustGetToken('{');
				if(barNum == STBAR_AUTOMAP)
				{
					automapbar = true;
				}
				ParseSBarInfoBlock(this->huds[barNum]);
				break;
			}
		}
	}
	SC_SetCMode(false);
	SC_Close();
}

void SBarInfo::ParseSBarInfoBlock(SBarInfoBlock &block)
{
	while(SC_CheckToken(TK_Identifier))
	{
		SBarInfoCommand cmd;

		switch(cmd.type = SC_MustMatchString(SBarInfoRoutineLevel))
		{
			case SBARINFO_DRAWSWITCHABLEIMAGE:
				SC_MustGetToken(TK_Identifier);
				if(SC_Compare("weaponslot"))
				{
					cmd.flags = DRAWIMAGE_WEAPONSLOT;
					SC_MustGetToken(TK_IntConst);
					cmd.value = sc_Number;
				}
				else if(SC_Compare("invulnerable"))
				{
					cmd.flags = DRAWIMAGE_INVULNERABILITY;
				}
				else
				{
					cmd.setString(sc_String, 0);
					const PClass* item = PClass::FindClass(sc_String);
					if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
					{
						SC_ScriptError("'%s' is not a type of inventory item.", sc_String);
					}
				}
				if(SC_CheckToken(TK_AndAnd))
				{
					cmd.flags += DRAWIMAGE_SWITCHABLE_AND;
					SC_MustGetToken(TK_Identifier);
					cmd.setString(sc_String, 1);
					const PClass* item = PClass::FindClass(sc_String);
					if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
					{
						SC_ScriptError("'%s' is not a type of inventory item.", sc_String);
					}
					SC_MustGetToken(',');
					SC_MustGetToken(TK_StringConst);
					cmd.special = newImage(sc_String);
					SC_MustGetToken(',');
					SC_MustGetToken(TK_StringConst);
					cmd.special2 = newImage(sc_String);
					SC_MustGetToken(',');
					SC_MustGetToken(TK_StringConst);
					cmd.special3 = newImage(sc_String);
					SC_MustGetToken(',');
				}
				else
				{
					SC_MustGetToken(',');
					SC_MustGetToken(TK_StringConst);
					cmd.special = newImage(sc_String);
					SC_MustGetToken(',');
				}
			case SBARINFO_DRAWIMAGE:
			{
				bool getImage = true;
				if(SC_CheckToken(TK_Identifier))
				{
					getImage = false;
					if(SC_Compare("playericon"))
						cmd.flags += DRAWIMAGE_PLAYERICON;
					else if(SC_Compare("ammoicon1"))
						cmd.flags += DRAWIMAGE_AMMO1;
					else if(SC_Compare("ammoicon2"))
						cmd.flags += DRAWIMAGE_AMMO2;
					else if(SC_Compare("armoricon"))
						cmd.flags += DRAWIMAGE_ARMOR;
					else if(SC_Compare("weaponicon"))
						cmd.flags += DRAWIMAGE_WEAPONICON;
					else if(SC_Compare("translatable"))
					{
						cmd.flags += DRAWIMAGE_TRANSLATABLE;
						getImage = true;
					}
					else
					{
						cmd.flags += DRAWIMAGE_INVENTORYICON;
						const PClass* item = PClass::FindClass(sc_String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
						{
							SC_ScriptError("'%s' is not a type of inventory item.", sc_String);
						}
						cmd.sprite = ((AInventory *)GetDefaultByType(item))->Icon;
					}
				}
				if(getImage)
				{
					SC_MustGetToken(TK_StringConst);
					cmd.sprite = newImage(sc_String);
				}
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.x = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.y = sc_Number - (200 - this->height); //the position should be absolute on the screen.
				if(SC_CheckToken(','))
				{
					SC_MustGetToken(TK_Identifier);
					if(SC_Compare("center"))
						cmd.flags += DRAWIMAGE_OFFSET_CENTER;
					else
						SC_ScriptError("Expected 'center' got '%s' instead.", sc_String);
				}
				SC_MustGetToken(';');
				break;
			}
			case SBARINFO_DRAWNUMBER:
				cmd.special4 = cmd.special3 = -1;
				SC_MustGetToken(TK_IntConst);
				cmd.special = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_Identifier);
				cmd.font = V_GetFont(sc_String);
				if(cmd.font == NULL)
					SC_ScriptError("Unknown font '%s'.", sc_String);
				SC_MustGetToken(',');
				SC_MustGetToken(TK_Identifier);
				cmd.translation = this->GetTranslation(sc_String);
				SC_MustGetToken(',');
				if(SC_CheckToken(TK_IntConst))
				{
					cmd.value = sc_Number;
					SC_MustGetToken(',');
				}
				else
				{
					SC_MustGetToken(TK_Identifier);
					if(SC_Compare("health"))
						cmd.flags = DRAWNUMBER_HEALTH;
					else if(SC_Compare("armor"))
						cmd.flags = DRAWNUMBER_ARMOR;
					else if(SC_Compare("ammo1"))
						cmd.flags = DRAWNUMBER_AMMO1;
					else if(SC_Compare("ammo2"))
						cmd.flags = DRAWNUMBER_AMMO2;
					else if(SC_Compare("ammo")) //request the next string to be an ammo type
					{
						SC_MustGetToken(TK_Identifier);
						cmd.setString(sc_String, 0);
						cmd.flags = DRAWNUMBER_AMMO;
						const PClass* ammo = PClass::FindClass(sc_String);
						if(ammo == NULL || !PClass::FindClass("Ammo")->IsAncestorOf(ammo)) //must be a kind of ammo
						{
							SC_ScriptError("'%s' is not a type of ammo.", sc_String);
						}
					}
					else if(SC_Compare("ammocapacity"))
					{
						SC_MustGetToken(TK_Identifier);
						cmd.setString(sc_String, 0);
						cmd.flags = DRAWNUMBER_AMMOCAPACITY;
						const PClass* ammo = PClass::FindClass(sc_String);
						if(ammo == NULL || !PClass::FindClass("Ammo")->IsAncestorOf(ammo)) //must be a kind of ammo
						{
							SC_ScriptError("'%s' is not a type of ammo.", sc_String);
						}
					}
					else if(SC_Compare("frags"))
						cmd.flags = DRAWNUMBER_FRAGS;
					else if(SC_Compare("kills"))
						cmd.flags += DRAWNUMBER_KILLS;
					else if(SC_Compare("monsters"))
						cmd.flags += DRAWNUMBER_MONSTERS;
					else if(SC_Compare("items"))
						cmd.flags += DRAWNUMBER_ITEMS;
					else if(SC_Compare("totalitems"))
						cmd.flags += DRAWNUMBER_TOTALITEMS;
					else if(SC_Compare("secrets"))
						cmd.flags += DRAWNUMBER_SECRETS;
					else if(SC_Compare("totalsecrets"))
						cmd.flags += DRAWNUMBER_TOTALSECRETS;
					else
					{
						cmd.flags = DRAWNUMBER_INVENTORY;
						SC_MustGetToken(TK_Identifier);
						cmd.setString(sc_String, 0);
						const PClass* item = PClass::FindClass(sc_String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of ammo
						{
							SC_ScriptError("'%s' is not a type of inventory item.", sc_String);
						}
					}
					SC_MustGetToken(',');
				}
				SC_MustGetToken(TK_IntConst);
				cmd.x = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.y = sc_Number - (200 - this->height);
				if(SC_CheckToken(','))
				{
					bool needsComma = false;
					if(SC_CheckToken(TK_IntConst)) //font spacing
					{
						cmd.special2 = sc_Number;
						needsComma = true;
					}
					if(!needsComma || SC_CheckToken(',')) //2nd coloring for "low-on" value
					{
						SC_MustGetToken(TK_Identifier);
						cmd.translation2 = this->GetTranslation(sc_String);
						SC_MustGetToken(',');
						SC_MustGetToken(TK_IntConst);
						cmd.special3 = sc_Number;
						if(SC_CheckToken(',')) //3rd coloring for "high-on" value
						{
							SC_MustGetToken(TK_Identifier);
							cmd.translation3 = this->GetTranslation(sc_String);
							SC_MustGetToken(',');
							SC_MustGetToken(TK_IntConst);
							cmd.special4 = sc_Number;
						}
					}
				}
				SC_MustGetToken(';');
				break;
			case SBARINFO_DRAWMUGSHOT:
				SC_MustGetToken(TK_StringConst);
				cmd.setString(sc_String, 0, 3, true);
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst); //accuracy
				if(sc_Number < 1 || sc_Number > 9)
					SC_ScriptError("Exspected a number between 1 and 9, got %d instead.", sc_Number);
				cmd.special = sc_Number;
				SC_MustGetToken(',');
				while(SC_CheckToken(TK_Identifier))
				{
					if(SC_Compare("xdeathface"))
						cmd.flags += DRAWMUGSHOT_XDEATHFACE;
					else if(SC_Compare("animatedgodmode"))
						cmd.flags += DRAWMUGSHOT_ANIMATEDGODMODE;
					else
						SC_ScriptError("Unknown flag '%s'.", sc_String);
					SC_MustGetToken(',');
				}
				SC_MustGetToken(TK_IntConst);
				cmd.x = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.y = sc_Number - (200 - this->height);
				if(SC_CheckToken(',')) //hmm I guess we had a numberic flag in there.
				{
					cmd.flags = cmd.x;
					cmd.x = cmd.y + (200 - this->height);
					SC_MustGetToken(TK_IntConst);
					cmd.y = sc_Number - (200 - this->height);
				}
				SC_MustGetToken(';');
				break;
			case SBARINFO_DRAWSELECTEDINVENTORY:
			{
				bool alternateonempty = false;
				while(true) //go until we get a font (non-flag)
				{
					SC_MustGetToken(TK_Identifier);
					if(SC_Compare("alternateonempty"))
					{
						alternateonempty = true;
						cmd.flags += DRAWSELECTEDINVENTORY_ALTERNATEONEMPTY;
					}
					else if(SC_Compare("artiflash"))
					{
						cmd.flags += DRAWSELECTEDINVENTORY_ARTIFLASH;
					}
					else if(SC_Compare("alwaysshowcounter"))
					{
						cmd.flags += DRAWSELECTEDINVENTORY_ALWAYSSHOWCOUNTER;
					}
					else
					{
						cmd.font = V_GetFont(sc_String);
						if(cmd.font == NULL)
							SC_ScriptError("Unknown font '%s'.", sc_String);
						SC_MustGetToken(',');
						break;
					}
					SC_MustGetToken(',');
				}
				SC_MustGetToken(TK_IntConst);
				cmd.x = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.y = sc_Number - (200 - this->height);
				cmd.special2 = cmd.x + 30;
				cmd.special3 = cmd.y + 24;
				cmd.translation = CR_GOLD;
				if(SC_CheckToken(',')) //more font information
				{
					SC_MustGetToken(TK_IntConst);
					cmd.special2 = sc_Number;
					SC_MustGetToken(',');
					SC_MustGetToken(TK_IntConst);
					cmd.special3 = sc_Number - (200 - this->height);
					if(SC_CheckToken(','))
					{
						SC_MustGetToken(TK_Identifier);
						cmd.translation = this->GetTranslation(sc_String);
						if(SC_CheckToken(','))
						{
							SC_MustGetToken(TK_IntConst);
							cmd.special4 = sc_Number;
						}
					}
				}
				if(alternateonempty)
				{
					SC_MustGetToken('{');
					this->ParseSBarInfoBlock(cmd.subBlock);
				}
				else
				{
					SC_MustGetToken(';');
				}
				break;
			}
			case SBARINFO_DRAWINVENTORYBAR:
				SC_MustGetToken(TK_Identifier);
				if(SC_Compare("Heretic"))
				{
					cmd.special = GAME_Heretic;
				}
				if(SC_Compare("Doom") || SC_Compare("Heretic"))
				{
					SC_MustGetToken(',');
					while(SC_CheckToken(TK_Identifier))
					{
						if(SC_Compare("alwaysshow"))
						{
							cmd.flags += DRAWINVENTORYBAR_ALWAYSSHOW;
						}
						else if(SC_Compare("noartibox"))
						{
							cmd.flags += DRAWINVENTORYBAR_NOARTIBOX;
						}
						else if(SC_Compare("noarrows"))
						{
							cmd.flags += DRAWINVENTORYBAR_NOARROWS;
						}
						else if(SC_Compare("alwaysshowcounter"))
						{
							cmd.flags += DRAWINVENTORYBAR_ALWAYSSHOWCOUNTER;
						}
						else
						{
							SC_ScriptError("Unknown flag '%s'.", sc_String);
						}
						SC_MustGetToken(',');
					}
					SC_MustGetToken(TK_IntConst);
					cmd.value = sc_Number;
					SC_MustGetToken(',');
					SC_MustGetToken(TK_Identifier);
					cmd.font = V_GetFont(sc_String);
					if(cmd.font == NULL)
						SC_ScriptError("Unknown font '%s'.", sc_String);
				}
				else
				{
					SC_ScriptError("Unkown style '%s'.", sc_String);
				}
				SC_MustGetToken(',');
				SC_MustGetNumber();
				cmd.x = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetNumber();
				cmd.y = sc_Number - (200 - this->height);
				cmd.special2 = cmd.x + 26;
				cmd.special3 = cmd.y + 22;
				cmd.translation = CR_GOLD;
				if(SC_CheckToken(',')) //more font information
				{
					SC_MustGetToken(TK_IntConst);
					cmd.special2 = sc_Number;
					SC_MustGetToken(',');
					SC_MustGetToken(TK_IntConst);
					cmd.special3 = sc_Number - (200 - this->height);
					if(SC_CheckToken(','))
					{
						SC_MustGetToken(TK_Identifier);
						cmd.translation = this->GetTranslation(sc_String);
						if(SC_CheckToken(','))
						{
							SC_MustGetToken(TK_IntConst);
							cmd.special4 = sc_Number;
						}
					}
				}
				SC_MustGetToken(';');
				break;
			case SBARINFO_DRAWBAR:
				SC_MustGetToken(TK_StringConst);
				cmd.sprite = newImage(sc_String);
				SC_MustGetToken(',');
				SC_MustGetToken(TK_StringConst);
				cmd.special = newImage(sc_String);
				SC_MustGetToken(',');
				SC_MustGetToken(TK_Identifier); //yeah, this is the same as drawnumber, there might be a better way to copy it...
				if(SC_Compare("health"))
				{
					cmd.flags = DRAWNUMBER_HEALTH;
					if(SC_CheckToken(TK_Identifier)) //comparing reference
					{
						cmd.setString(sc_String, 0);
						const PClass* item = PClass::FindClass(sc_String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of inventory
						{
							SC_ScriptError("'%s' is not a type of inventory item.", sc_String);
						}
					}
					else
						cmd.special2 = DRAWBAR_COMPAREDEFAULTS;
				}
				else if(SC_Compare("armor"))
				{
					cmd.flags = DRAWNUMBER_ARMOR;
					if(SC_CheckToken(TK_Identifier))
					{
						cmd.setString(sc_String, 0);
						const PClass* item = PClass::FindClass(sc_String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of inventory
						{
							SC_ScriptError("'%s' is not a type of inventory item.", sc_String);
						}
					}
					else
						cmd.special2 = DRAWBAR_COMPAREDEFAULTS;
				}
				else if(SC_Compare("ammo1"))
					cmd.flags = DRAWNUMBER_AMMO1;
				else if(SC_Compare("ammo2"))
					cmd.flags = DRAWNUMBER_AMMO2;
				else if(SC_Compare("ammo")) //request the next string to be an ammo type
				{
					SC_MustGetToken(TK_Identifier);
					cmd.setString(sc_String, 0);
					cmd.flags = DRAWNUMBER_AMMO;
					const PClass* ammo = PClass::FindClass(sc_String);
					if(ammo == NULL || !PClass::FindClass("Ammo")->IsAncestorOf(ammo)) //must be a kind of ammo
					{
						SC_ScriptError("'%s' is not a type of ammo.", sc_String);
					}
				}
				else if(SC_Compare("frags"))
					cmd.flags = DRAWNUMBER_FRAGS;
				else if(SC_Compare("kills"))
					cmd.flags = DRAWNUMBER_KILLS;
				else if(SC_Compare("items"))
					cmd.flags = DRAWNUMBER_ITEMS;
				else if(SC_Compare("secrets"))
					cmd.flags = DRAWNUMBER_SECRETS;
				else
				{
					cmd.flags = DRAWNUMBER_INVENTORY;
					cmd.setString(sc_String, 0);
					const PClass* item = PClass::FindClass(sc_String);
					if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of ammo
					{
						SC_ScriptError("'%s' is not a type of inventory item.", sc_String);
					}
				}
				SC_MustGetToken(',');
				SC_MustGetToken(TK_Identifier);
				if(SC_Compare("horizontal"))
					cmd.special2 += DRAWBAR_HORIZONTAL;
				else if(!SC_Compare("vertical"))
					SC_ScriptError("Unknown direction '%s'.", sc_String);
				SC_MustGetToken(',');
				if(SC_CheckToken(TK_Identifier))
				{
					if(!SC_Compare("reverse"))
					{
						SC_ScriptError("Exspected 'reverse', got '%s' instead.", sc_String);
					}
					cmd.special2 += DRAWBAR_REVERSE;
					SC_MustGetToken(',');
				}
				SC_MustGetToken(TK_IntConst);
				cmd.x = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.y = sc_Number - (200 - this->height);
				SC_MustGetToken(';');
				break;
			case SBARINFO_DRAWGEM:
				while(SC_CheckToken(TK_Identifier))
				{
					if(SC_Compare("wiggle"))
						cmd.flags += DRAWGEM_WIGGLE;
					else if(SC_Compare("translatable"))
						cmd.flags += DRAWGEM_TRANSLATABLE;
					else
						SC_ScriptError("Unkown drawgem flag '%s'.", sc_String);
					SC_MustGetToken(',');
				}
				SC_MustGetToken(TK_StringConst); //chain
				cmd.special = newImage(sc_String);
				SC_MustGetToken(',');
				SC_MustGetToken(TK_StringConst); //gem
				cmd.sprite = newImage(sc_String);
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				if(sc_Number < 0)
					SC_ScriptError("Left padding must be a positive number.");
				cmd.special2 = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				if(sc_Number < 0)
					SC_ScriptError("Right padding must be a positive number.");
				cmd.special3 = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				if(sc_Number < 0)
					SC_ScriptError("Chain size must be a positive number.");
				cmd.special4 = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.x = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.y = sc_Number - (200 - this->height);
				SC_MustGetToken(';');
				break;
			case SBARINFO_DRAWSHADER:
				SC_MustGetToken(TK_IntConst);
				cmd.special = sc_Number;
				if(sc_Number < 1)
					SC_ScriptError("Width must be greater than 1.");
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.special2 = sc_Number;
				if(sc_Number < 1)
					SC_ScriptError("Height must be greater than 1.");
				SC_MustGetToken(',');
				SC_MustGetToken(TK_Identifier);
				if(SC_Compare("vertical"))
					cmd.flags += DRAWSHADER_VERTICAL;
				else if(!SC_Compare("horizontal"))
					SC_ScriptError("Unknown direction '%s'.", sc_String);
				SC_MustGetToken(',');
				if(SC_CheckToken(TK_Identifier))
				{
					if(!SC_Compare("reverse"))
					{
						SC_ScriptError("Exspected 'reverse', got '%s' instead.", sc_String);
					}
					cmd.flags += DRAWSHADER_REVERSE;
					SC_MustGetToken(',');
				}
				SC_MustGetToken(TK_IntConst);
				cmd.x = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.y = sc_Number - (200 - this->height);
				SC_MustGetToken(';');
				break;
			case SBARINFO_DRAWSTRING:
				SC_MustGetToken(TK_Identifier);
				cmd.font = V_GetFont(sc_String);
				if(cmd.font == NULL)
					SC_ScriptError("Unknown font '%s'.", sc_String);
				SC_MustGetToken(',');
				SC_MustGetToken(TK_Identifier);
				cmd.translation = this->GetTranslation(sc_String);
				SC_MustGetToken(',');
				SC_MustGetToken(TK_StringConst);
				cmd.setString(sc_String, 0, -1, false);
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.x = sc_Number;
				SC_MustGetToken(',');
				SC_MustGetToken(TK_IntConst);
				cmd.y = sc_Number - (200 - this->height);
				SC_MustGetToken(';');
				break;
			case SBARINFO_GAMEMODE:
				while(SC_CheckToken(TK_Identifier))
				{
					if(SC_Compare("singleplayer"))
						cmd.flags += GAMETYPE_SINGLEPLAYER;
					else if(SC_Compare("cooperative"))
						cmd.flags += GAMETYPE_COOPERATIVE;
					else if(SC_Compare("deathmatch"))
						cmd.flags += GAMETYPE_DEATHMATCH;
					else if(SC_Compare("teamgame"))
						cmd.flags += GAMETYPE_TEAMGAME;
					else
						SC_ScriptError("Unknown gamemode: %s", sc_String);
					if(SC_CheckToken('{'))
						break;
					SC_MustGetToken(',');
				}
				this->ParseSBarInfoBlock(cmd.subBlock);
				break;
			case SBARINFO_PLAYERCLASS:
				cmd.special = cmd.special2 = cmd.special3 = -1;
				for(int i = 0;i < 3 && SC_CheckToken(TK_Identifier);i++) //up to 3 classes
				{
					bool foundClass = false;
					for(unsigned int c = 0;c < PlayerClasses.Size();c++)
					{
						if(stricmp(sc_String, PlayerClasses[c].Type->Meta.GetMetaString(APMETA_DisplayName)) == 0)
						{
							foundClass = true;
							if(i == 0)
								cmd.special = PlayerClasses[c].Type->ClassIndex;
							else if(i == 1)
								cmd.special2 = PlayerClasses[c].Type->ClassIndex;
							else //should be 2
								cmd.special3 = PlayerClasses[c].Type->ClassIndex;
							break;
						}
					}
					if(!foundClass)
						SC_ScriptError("Unkown PlayerClass '%s'.", sc_String);
					if(SC_CheckToken('{') || i == 2)
						goto FinishPlayerClass;
					SC_MustGetToken(',');
				}
			FinishPlayerClass:
				this->ParseSBarInfoBlock(cmd.subBlock);
				break;
		}
		block.commands.Push(cmd);
	}
	SC_MustGetToken('}');
}

int SBarInfo::newImage(const char* patchname)
{
	if(stricmp(patchname, "nullimage") == 0)
	{
		return -1;
	}
//	if(strlen(patchname) > 8)
//	{
//		SC_ScriptError("Graphic names can not be greater then 8 characters long.");
//	}
	for(unsigned int i = 0;i < this->Images.Size();i++) //did we already load it?
	{
		if(stricmp(this->Images[i], patchname) == 0)
		{
			return i;
		}
	}
	return this->Images.Push(patchname);
}

//converts a string into a tranlation.
EColorRange SBarInfo::GetTranslation(char* translation)
{
	EColorRange returnVal = CR_UNTRANSLATED;
	FString namedTranslation; //we must send in "[translation]"
	const BYTE *trans_ptr;
	namedTranslation.Format("[%s]", translation);
	trans_ptr = (const BYTE *)(&namedTranslation[0]);
	if((returnVal = V_ParseFontColor(trans_ptr, CR_UNTRANSLATED, CR_UNTRANSLATED)) == CR_UNDEFINED)
	{
		SC_ScriptError("Missing definition for color %s.", translation);
	}
	return returnVal;
}

SBarInfo::SBarInfo() //make results more predicable
{
	Init();
}

SBarInfo::SBarInfo(int lumpnum)
{
	Init();
	ParseSBarInfo(lumpnum);
}

void SBarInfo::Init()
{
	automapbar = false;
	interpolateHealth = false;
	interpolationSpeed = 8;
	height = 0;
}

SBarInfo::~SBarInfo()
{
	for (size_t i = 0; i < countof(huds); ++i)
	{
		huds[i].commands.Clear();
	}
}

void SBarInfoCommand::setString(const char* source, int strnum, int maxlength, bool exact)
{
	if(!exact)
	{
		if(maxlength != -1 && strlen(source) > (unsigned int) maxlength)
		{
			SC_ScriptError("%s is greater than %d characters.", source, maxlength);
			return;
		}
	}
	else
	{
		if(maxlength != -1 && strlen(source) != (unsigned int) maxlength)
		{
			SC_ScriptError("%s must be %d characters.", source, maxlength);
			return;
		}
	}
	string[strnum] = source;
}

enum
{
	ST_FACENORMALRIGHT,
	ST_FACENORMAL,
	ST_FACENORMALLEFT,
	ST_FACEPAINRIGHT,
	ST_FACEPAINLEFT,
	ST_FACEOUCH,
	ST_FACEGRIN,
	ST_FACEPAIN,
	ST_FACEGOD = 72,
	ST_FACEGODRIGHT = 73, //switch the roles of 0 and 1 because 0 is taken by Doom.
	ST_FACEGODLEFT = 74,
	ST_FACEDEAD = 75,
	ST_FACEXDEAD = 76,
};

enum
{
	imgARTIBOX,
	imgSELECTBOX,
	imgINVLFGEM1,
	imgINVLFGEM2,
	imgINVRTGEM1,
	imgINVRTGEM2,
};

//The next class allows us to draw bars
class FBarTexture : public FTexture
{
public:
	~FBarTexture()
	{
		delete Pixels;
	}

	void Unload()
	{
		if(image != NULL)
		{
			image->Unload ();
		}
	}

	const BYTE *GetColumn(unsigned int column, const Span **spans_out)
	{
		if (column > (unsigned int) Width)
		{
			column = Width;
		}
		image->GetColumn(column, spans_out);
		return Pixels + column*Height;
	}

	const BYTE *GetPixels()
	{
		return Pixels;
	}

	FBarTexture(FTexture* bar, FTexture* bg, int value, bool horizontal, bool reverse)
	{
		value = clamp(value, 0, 100);
		image = bar;
		//width and height are supposed to be the end result, Width and Height are the input image.  If that makes sense.
		int width = Width = bar->GetWidth();
		int height = Height = bar->GetHeight();
		if(horizontal)
		{
			width = (int) (((double) width/100)*value);
		}
		Pixels = new BYTE[Width*Height];
		memset(Pixels, 0, Width*Height); //Prevent garbage when using transparent images
		bar->CopyToBlock(Pixels, Width, Height, 0, 0); //draw the bar
		int run = bar->GetHeight() - (int) (((double) height/100)*value);
		int visible = bar->GetHeight() - run;
		if(bg == NULL || bg->GetWidth() != bar->GetWidth() || bg->GetHeight() != bar->GetHeight())
		{
			BYTE color0 = GPalette.Remap[0];
			if(!horizontal)
			{
				if(!reverse) //remove offset if we are not reversing the direction.
				{
					visible = 0;
				}
				for(int i = 0;i < Width;i++)
				{
					memset(Pixels + i*Height + visible, color0, run);
				}
			}
			else
			{
				for(int i = reverse ? 0 : width;(reverse && i < Width - width) || (!reverse && i < Width);i++)
				{
					memset(Pixels + i*Height, color0, Height);
				}
			}
		}
		else
		{
			BYTE* PixelData = (BYTE*) bg->GetPixels();
			if(!horizontal)
			{
				if(!reverse)
				{
					visible = 0;
				}
				for(int i = 0;i < width;i++)
				{
					memcpy(Pixels + i*Height + visible, PixelData + i*Height, run);
				}
			}
			else
			{
				for(int i = reverse ? 0 : width;(reverse && i < Width - width) || (!reverse && i < Width);i++)
				{
					memcpy(Pixels + i*Height, PixelData + i*Height, Height);
				}
			}
		}
	}
protected:
	BYTE* Pixels;
	FTexture* image;
};

//Used for shading
class FBarShader : public FTexture
{
public:
	FBarShader(bool vertical, bool reverse) //make an alpha map
	{
		int i;

		Width = vertical ? 2 : 256;
		Height = vertical ? 256 : 2;
		CalcBitSize();

		// Fill the column/row with shading values.
		// Vertical shaders have have minimum alpha at the top
		// and maximum alpha at the bottom, unless flipped by
		// setting reverse to true. Horizontal shaders are just
		// the opposite.
		if (vertical)
		{
			if (!reverse)
			{
				for (i = 0; i < 256; ++i)
				{
					Pixels[i] = i;
					Pixels[256+i] = i;
				}
			}
			else
			{
				for (i = 0; i < 256; ++i)
				{
					Pixels[i] = 255 - i;
					Pixels[256+i] = 255 -i;
				}
			}
		}
		else
		{
			if (!reverse)
			{
				for (i = 0; i < 256; ++i)
				{
					Pixels[i*2] = 255 - i;
					Pixels[i*2+1] = 255 - i;
				}
			}
			else
			{
				for (i = 0; i < 256; ++i)
				{
					Pixels[i*2] = i;
					Pixels[i*2+1] = i;
				}
			}
		}
		DummySpan[0].TopOffset = 0;
		DummySpan[0].Length = vertical ? 256 : 1;
		DummySpan[1].TopOffset = 0;
		DummySpan[1].Length = 0;
	}

	const BYTE *GetColumn(unsigned int column, const Span **spans_out)
	{
		if (spans_out != NULL)
		{
			*spans_out = DummySpan;
		}
		return Pixels + (column & WidthMask) * 256;
	}

	const BYTE *GetPixels()
	{
		return Pixels;
	}

	void Unload()
	{
	}

private:
	BYTE Pixels[512];
	Span DummySpan[2];
};

SBarInfoCommand::SBarInfoCommand() //sets the default values for more predicable behavior
{
	type = 0;
	special = 0;
	special2 = 0;
	special3 = 0;
	special4 = 0;
	flags = 0;
	x = 0;
	y = 0;
	value = 0;
	sprite = 0;
	translation = CR_UNTRANSLATED;
	translation2 = CR_UNTRANSLATED;
	translation3 = CR_UNTRANSLATED;
	font = V_GetFont("CONFONT");
	bar = NULL;
}

SBarInfoCommand::~SBarInfoCommand()
{
	if (bar != NULL)
	{
		delete bar;
	}
	subBlock.commands.Clear();
}

SBarInfoBlock::SBarInfoBlock()
{
	forceScaled = false;
}

//SBarInfo Display
class FSBarInfo : public FBaseStatusBar
{
public:
	FSBarInfo () : FBaseStatusBar (SBarInfoScript->height),
		shader_horz_normal(false, false),
		shader_horz_reverse(false, true),
		shader_vert_normal(true, false),
		shader_vert_reverse(true, true)
	{
		static const char *InventoryBarLumps[] =
		{
			"ARTIBOX",	"SELECTBO",	"INVGEML1",
			"INVGEML2",	"INVGEMR1",	"INVGEMR2",
			"USEARTIA", "USEARTIB", "USEARTIC", "USEARTID",
		};
		TArray<const char *> patchnames;
		patchnames.Resize(SBarInfoScript->Images.Size()+10);
		unsigned int i = 0;
		for(i = 0;i < SBarInfoScript->Images.Size();i++)
		{
			patchnames[i] = SBarInfoScript->Images[i];
		}
		for(i = 0;i < 10;i++)
		{
			patchnames[i+SBarInfoScript->Images.Size()] = InventoryBarLumps[i];
		}
		invBarOffset = SBarInfoScript->Images.Size();
		Images.Init(&patchnames[0], patchnames.Size());
		drawingFont = V_GetFont("ConFont");
		faceTimer = ST_FACETIME;
		rampageTimer = 0;
		faceIndex = 0;
		if(SBarInfoScript->interpolateHealth)
		{
			oldHealth = 0;
		}
		mugshotHealth = -1;
		lastPrefix = "";
		weaponGrin = false;
		chainWiggle = 0;
		artiflash = 4;
	}

	~FSBarInfo ()
	{
		Images.Uninit();
		Faces.Uninit();
	}

	void Draw (EHudState state)
	{
		FBaseStatusBar::Draw(state);
		int hud = 2;
		if(state == HUD_StatusBar)
		{
			if(SBarInfoScript->automapbar && automapactive)
			{
				hud = 3;
			}
			else
			{
				hud = 2;
			}
		}
		else if(state == HUD_Fullscreen)
		{
			hud = 1;
		}
		else
		{
			hud = 0;
		}
		if(SBarInfoScript->huds[hud].forceScaled) //scale the statusbar
		{
			SetScaled(true);
			setsizeneeded = true;
		}
		doCommands(SBarInfoScript->huds[hud]);
		if(CPlayer->inventorytics > 0 && !(level.flags & LEVEL_NOINVENTORYBAR))
		{
			if(state == HUD_StatusBar)
				doCommands(SBarInfoScript->huds[4]);
			else if(state == HUD_Fullscreen)
				doCommands(SBarInfoScript->huds[5]);
		}
	}

	void NewGame ()
	{
		if (CPlayer != NULL)
		{
			AttachToPlayer (CPlayer);
		}
	}

	void AttachToPlayer (player_t *player)
	{
		player_t *oldplayer = CPlayer;
		FBaseStatusBar::AttachToPlayer(player);
		if (oldplayer != CPlayer)
		{
			SetFace(&skins[CPlayer->userinfo.skin], "STF");
		}
	}

	void Tick ()
	{
		FBaseStatusBar::Tick();
		if(level.time & 1)
			chainWiggle = pr_chainwiggle() & 1;
		getNewFace(M_Random());
		if(!SBarInfoScript->interpolateHealth)
		{
			oldHealth = CPlayer->health;
		}
		else
		{
			if(oldHealth > CPlayer->health)
			{
				oldHealth -= clamp((oldHealth - CPlayer->health) >> 2, 1, 8);
			}
			else if(oldHealth < CPlayer->health)
			{
				oldHealth += clamp((CPlayer->health - oldHealth) >> 2, 1, 8);
			}
		}
		if(artiflash)
		{
			artiflash--;
		}
	}

	void SetFace (void *skn)
	{
		SetFace((FPlayerSkin *)skn, "STF");
	}

	void ReceivedWeapon (AWeapon *weapon)
	{
		weaponGrin = true;
	}

	void FlashItem(const PClass *itemtype)
	{
		artiflash = 4;
	}
private:
	//code from doom_sbar.cpp but it should do fine.
	void SetFace (FPlayerSkin *skin, const char* defPrefix)
	{
		oldSkin = skin;
		const char *nameptrs[ST_NUMFACES]; 
		char names[ST_NUMFACES][9];
		char prefix[4];
		int i, j;
		int namespc;
		int facenum;

		for (i = 0; i < ST_NUMFACES; i++)
		{
			nameptrs[i] = names[i];
		}

		if (skin->face[0] != 0)
		{
			prefix[0] = skin->face[0];
			prefix[1] = skin->face[1];
			prefix[2] = skin->face[2];
			prefix[3] = 0;
			namespc = skin->namespc;
		}
		else
		{
			prefix[0] = defPrefix[0];
			prefix[1] = defPrefix[1];
			prefix[2] = defPrefix[2];
			prefix[3] = 0;
			namespc = ns_global;
		}
		if(stricmp(prefix, lastPrefix) == 0)
		{
			return;
		}
		lastPrefix = prefix;

		facenum = 0;

		for (i = 0; i < 9; i++) //levels
		{
			for (j = 0; j < 3; j++)
			{
				sprintf (names[facenum++], "%sST%d%d", prefix, i, j);
			}
			sprintf (names[facenum++], "%sTR%d0", prefix, i);  // turn right
			sprintf (names[facenum++], "%sTL%d0", prefix, i);  // turn left
			sprintf (names[facenum++], "%sOUCH%d", prefix, i); // ouch!
			sprintf (names[facenum++], "%sEVL%d", prefix, i);  // evil grin ;)
			sprintf (names[facenum++], "%sKILL%d", prefix, i); // pissed off
		}
		for (i = 0; i < 3; i++)
			sprintf (names[facenum++], "%sGOD%d", prefix, i);
		sprintf (names[facenum++], "%sDEAD0", prefix);
		for(i = 0;i < 6;i++) //xdeath
		{
			sprintf (names[facenum++], "%sXDTH%d", prefix, i);
		}

		Faces.Uninit ();
		Faces.Init (nameptrs, ST_NUMFACES, namespc);

		faceIndex = ST_FACENORMAL;
		faceTimer = 0;
	}
	void doCommands(SBarInfoBlock &block)
	{
		//prepare ammo counts
		AAmmo *ammo1, *ammo2;
		int ammocount1, ammocount2;
		GetCurrentAmmo(ammo1, ammo2, ammocount1, ammocount2);
		int health = CPlayer->mo->health;
		if(SBarInfoScript->interpolateHealth)
		{
			health = oldHealth;
		}
		for(unsigned int i = 0;i < block.commands.Size();i++)
		{
			SBarInfoCommand& cmd = block.commands[i];
			switch(cmd.type) //read and execute all the commands
			{
				case SBARINFO_DRAWSWITCHABLEIMAGE: //draw the alt image if we don't have the item else this is like a normal drawimage
				{
					int drawAlt = 0;
					if((cmd.flags & DRAWIMAGE_WEAPONSLOT)) //weaponslots
					{
						drawAlt = 1; //draw off state until we know we have something.
						for (int i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
						{
							const PClass *weap = LocalWeapons.Slots[cmd.value].GetWeapon(i);
							if(weap == NULL)
							{
								continue;
							}
							else if(CPlayer->mo->FindInventory(weap) != NULL)
							{
								drawAlt = 0;
								break;
							}
						}
					}
					else if((cmd.flags & DRAWIMAGE_INVULNERABILITY))
					{
						if(CPlayer->cheats&CF_GODMODE)
						{
							drawAlt = 1;
						}
					}
					else //check the inventory items and draw selected sprite
					{
						AInventory* item = CPlayer->mo->FindInventory(PClass::FindClass(cmd.string[0]));
						if(item == NULL || item->Amount == 0)
							drawAlt = 1;
						if((cmd.flags & DRAWIMAGE_SWITCHABLE_AND))
						{
							item = CPlayer->mo->FindInventory(PClass::FindClass(cmd.string[1]));
							if((item != NULL && item->Amount != 0) && drawAlt == 0) //both
							{
								drawAlt = 0;
							}
							else if((item != NULL && item->Amount != 0) && drawAlt == 1) //2nd
							{
								drawAlt = 3;
							}
							else if((item == NULL || item->Amount == 0) && drawAlt == 0) //1st
							{
								drawAlt = 2;
							}
						}
					}
					if(drawAlt != 0) //draw 'off' image
					{
						if(cmd.special != -1 && drawAlt == 1)
							DrawGraphic(Images[cmd.special], cmd.x, cmd.y, cmd.flags);
						else if(cmd.special2 != -1 && drawAlt == 2)
							DrawGraphic(Images[cmd.special2], cmd.x, cmd.y, cmd.flags);
						else if(cmd.special3 != -1 && drawAlt == 3)
							DrawGraphic(Images[cmd.special3], cmd.x, cmd.y, cmd.flags);
						break;
					}
				}
				case SBARINFO_DRAWIMAGE:
					if((cmd.flags & DRAWIMAGE_PLAYERICON))
						DrawGraphic(TexMan[CPlayer->mo->ScoreIcon], cmd.x, cmd.y, cmd.flags);
					else if((cmd.flags & DRAWIMAGE_AMMO1))
					{
						if(ammo1 != NULL)
							DrawGraphic(TexMan[ammo1->Icon], cmd.x, cmd.y, cmd.flags);
					}
					else if((cmd.flags & DRAWIMAGE_AMMO2))
					{
						if(ammo2 != NULL)
							DrawGraphic(TexMan[ammo2->Icon], cmd.x, cmd.y, cmd.flags);
					}
					else if((cmd.flags & DRAWIMAGE_ARMOR))
					{
						ABasicArmor *armor = CPlayer->mo->FindInventory<ABasicArmor>();
						if(armor != NULL && armor->Amount != 0)
							DrawGraphic(TexMan(armor->Icon), cmd.x, cmd.y, cmd.flags);
					}
					else if((cmd.flags & DRAWIMAGE_WEAPONICON))
					{
						AWeapon *weapon = CPlayer->ReadyWeapon;
						if(weapon != NULL && weapon->Icon > 0)
						{
							DrawGraphic(TexMan[weapon->Icon], cmd.x, cmd.y, cmd.flags);
						}
					}
					else if((cmd.flags & DRAWIMAGE_INVENTORYICON))
					{
						DrawGraphic(TexMan[cmd.sprite], cmd.x, cmd.y, cmd.flags);
					}
					else
					{
						DrawGraphic(Images[cmd.sprite], cmd.x, cmd.y, cmd.flags);
					}
					break;
				case SBARINFO_DRAWNUMBER:
					if(drawingFont != cmd.font)
					{
						drawingFont = cmd.font;
					}
					if(cmd.flags == DRAWNUMBER_HEALTH)
					{
						cmd.value = health;
						if(cmd.value < 0) //health shouldn't display negatives
						{
							cmd.value = 0;
						}
					}
					else if(cmd.flags == DRAWNUMBER_ARMOR)
					{
						AInventory *armor = CPlayer->mo->FindInventory<ABasicArmor>();
						cmd.value = armor != NULL ? armor->Amount : 0;
					}
					else if(cmd.flags == DRAWNUMBER_AMMO1)
					{
						cmd.value = ammocount1;
						if(ammo1 == NULL) //no ammo, do not draw
						{
							continue;
						}
					}
					else if(cmd.flags == DRAWNUMBER_AMMO2)
					{
						cmd.value = ammocount2;
						if(ammo2 == NULL) //no ammo, do not draw
						{
							continue;
						}
					}
					else if(cmd.flags == DRAWNUMBER_AMMO)
					{
						const PClass* ammo = PClass::FindClass(cmd.string[0]);
						AInventory* item = CPlayer->mo->FindInventory(ammo);
						if(item != NULL)
						{
							cmd.value = item->Amount;
						}
						else
						{
							cmd.value = 0;
						}
					}
					else if(cmd.flags == DRAWNUMBER_AMMOCAPACITY)
					{
						const PClass* ammo = PClass::FindClass(cmd.string[0]);
						AInventory* item = CPlayer->mo->FindInventory(ammo);
						if(item != NULL)
						{
							cmd.value = item->MaxAmount;
						}
						else
						{
							cmd.value = ((AInventory *)GetDefaultByType(ammo))->MaxAmount;
						}
					}
					else if(cmd.flags == DRAWNUMBER_FRAGS)
						cmd.value = CPlayer->fragcount;
					else if(cmd.flags == DRAWNUMBER_KILLS)
						cmd.value = level.killed_monsters;
					else if(cmd.flags == DRAWNUMBER_MONSTERS)
						cmd.value = level.total_monsters;
					else if(cmd.flags == DRAWNUMBER_ITEMS)
						cmd.value = level.found_items;
					else if(cmd.flags == DRAWNUMBER_TOTALITEMS)
						cmd.value = level.total_items;
					else if(cmd.flags == DRAWNUMBER_SECRETS)
						cmd.value = level.found_secrets;
					else if(cmd.flags == DRAWNUMBER_TOTALSECRETS)
						cmd.value = level.total_secrets;
					else if(cmd.flags == DRAWNUMBER_INVENTORY)
					{
						AInventory* item = CPlayer->mo->FindInventory(PClass::FindClass(cmd.string[0]));
						if(item != NULL)
						{
							cmd.value = item->Amount;
						}
						else
						{
							cmd.value = 0;
						}
					}
					if(cmd.special3 != -1 && cmd.value <= cmd.special3) //low
						DrawNumber(cmd.value, cmd.special, cmd.x, cmd.y, cmd.translation2, cmd.special2);
					else if(cmd.special4 != -1 && cmd.value >= cmd.special4) //high
						DrawNumber(cmd.value, cmd.special, cmd.x, cmd.y, cmd.translation3, cmd.special2);
					else
						DrawNumber(cmd.value, cmd.special, cmd.x, cmd.y, cmd.translation, cmd.special2);
					break;
				case SBARINFO_DRAWMUGSHOT:
				{
					bool xdth = false;
					bool animatedgodmode = false;
					if(cmd.flags & DRAWMUGSHOT_XDEATHFACE)
						xdth = true;
					if(cmd.flags & DRAWMUGSHOT_ANIMATEDGODMODE)
						animatedgodmode = true;
					SetFace(oldSkin, cmd.string[0].GetChars());
					DrawFace(cmd.special, xdth, animatedgodmode, cmd.x, cmd.y);
					break;
				}
				case SBARINFO_DRAWSELECTEDINVENTORY:
					if(CPlayer->mo->InvSel != NULL && !(level.flags & LEVEL_NOINVENTORYBAR))
					{
						if((cmd.flags & DRAWSELECTEDINVENTORY_ARTIFLASH) && artiflash)
						{
							DrawDimImage(Images[ARTIFLASH_OFFSET+(4-artiflash)], cmd.x, cmd.y, CPlayer->mo->InvSel->Amount <= 0);
						}
						else
						{
							DrawDimImage(TexMan(CPlayer->mo->InvSel->Icon), cmd.x, cmd.y, CPlayer->mo->InvSel->Amount <= 0);
						}
						if((cmd.flags & DRAWSELECTEDINVENTORY_ALWAYSSHOWCOUNTER) || CPlayer->mo->InvSel->Amount != 1)
						{
							if(drawingFont != cmd.font)
							{
								drawingFont = cmd.font;
							}
							DrawNumber(CPlayer->mo->InvSel->Amount, 3, cmd.special2, cmd.special3, cmd.translation, cmd.special4);
						}
					}
					else if((cmd.flags & DRAWSELECTEDINVENTORY_ALTERNATEONEMPTY))
					{
						doCommands(cmd.subBlock);
					}
					break;
				case SBARINFO_DRAWINVENTORYBAR:
				{
					bool alwaysshow = false;
					bool artibox = true;
					bool noarrows = false;
					bool alwaysshowcounter = false;
					if((cmd.flags & DRAWINVENTORYBAR_ALWAYSSHOW))
						alwaysshow = true;
					if((cmd.flags & DRAWINVENTORYBAR_NOARTIBOX))
						artibox = false;
					if((cmd.flags & DRAWINVENTORYBAR_NOARROWS))
						noarrows = true;
					if((cmd.flags & DRAWINVENTORYBAR_ALWAYSSHOWCOUNTER))
						alwaysshowcounter = true;
					if(drawingFont != cmd.font)
					{
						drawingFont = cmd.font;
					}
					DrawInventoryBar(cmd.special, cmd.value, cmd.x, cmd.y, alwaysshow, cmd.special2, cmd.special3, cmd.translation, artibox, noarrows, alwaysshowcounter);
					break;
				}
				case SBARINFO_DRAWBAR:
				{
					if(cmd.sprite == -1) break; //don't draw anything.
					bool horizontal = !!((cmd.special2 & DRAWBAR_HORIZONTAL));
					bool reverse = !!((cmd.special2 & DRAWBAR_REVERSE));
					int value = 0;
					int max = 0;
					if(cmd.flags == DRAWNUMBER_HEALTH)
					{
						value = health;
						if(cmd.value < 0) //health shouldn't display negatives
						{
							value = 0;
						}
						if(!(cmd.special2 & DRAWBAR_COMPAREDEFAULTS))
						{
							AInventory* item = CPlayer->mo->FindInventory(PClass::FindClass(cmd.string[0])); //max comparer
							if(item != NULL)
							{
								max = item->Amount;
							}
							else
							{
								max = 0;
							}
						}
						else //default to the classes health
						{
							max = CPlayer->mo->GetDefault()->health;
						}
					}
					else if(cmd.flags == DRAWNUMBER_ARMOR)
					{
						AInventory *armor = CPlayer->mo->FindInventory<ABasicArmor>();
						value = armor != NULL ? armor->Amount : 0;
						if(!((cmd.special2 & DRAWBAR_COMPAREDEFAULTS) == DRAWBAR_COMPAREDEFAULTS))
						{
							AInventory* item = CPlayer->mo->FindInventory(PClass::FindClass(cmd.string[0])); //max comparer
							if(item != NULL)
							{
								max = item->Amount;
							}
							else
							{
								max = 0;
							}
						}
						else //default to 100
						{
							max = 100;
						}
					}
					else if(cmd.flags == DRAWNUMBER_AMMO1)
					{
						value = ammocount1;
						if(ammo1 == NULL) //no ammo, draw as empty
						{
							value = 0;
							max = 1;
						}
						else
							max = ammo1->MaxAmount;
					}
					else if(cmd.flags == DRAWNUMBER_AMMO2)
					{
						value = ammocount2;
						if(ammo2 == NULL) //no ammo, draw as empty
						{
							value = 0;
							max = 1;
						}
						else
							max = ammo2->MaxAmount;
					}
					else if(cmd.flags == DRAWNUMBER_AMMO)
					{
						const PClass* ammo = PClass::FindClass(cmd.string[0]);
						AInventory* item = CPlayer->mo->FindInventory(ammo);
						if(item != NULL)
						{
							value = item->Amount;
							max = item->MaxAmount;
						}
						else
						{
							value = 0;
						}
					}
					else if(cmd.flags == DRAWNUMBER_FRAGS)
					{
						value = CPlayer->fragcount;
						max = fraglimit;
					}
					else if(cmd.flags == DRAWNUMBER_KILLS)
					{
						value = level.killed_monsters;
						max = level.total_monsters;
					}
					else if(cmd.flags == DRAWNUMBER_ITEMS)
					{
						value = level.found_items;
						max = level.total_items;
					}
					else if(cmd.flags == DRAWNUMBER_SECRETS)
					{
						value = level.found_secrets;
						max = level.total_secrets;
					}
					else if(cmd.flags == DRAWNUMBER_INVENTORY)
					{
						AInventory* item = CPlayer->mo->FindInventory(PClass::FindClass(cmd.string[0]));
						if(item != NULL)
						{
							value = item->Amount;
							max = item->MaxAmount;
						}
						else
						{
							value = 0;
						}
					}
					if(max != 0 || value < 0)
					{
						value = (value*100)/max;
						if(value > 100)
							value = 100;
					}
					else
					{
						value = 0;
					}
					if(cmd.bar != NULL)
						delete cmd.bar;
					if(cmd.special != -1)
						cmd.bar = new FBarTexture(Images[cmd.sprite], Images[cmd.special], value, horizontal, reverse);
					else
						cmd.bar = new FBarTexture(Images[cmd.sprite], NULL, value, horizontal, reverse);
					DrawImage(cmd.bar, cmd.x, cmd.y);
					//delete cmd.bar;
					break;
				}
				case SBARINFO_DRAWGEM:
				{
					int value = health;
					int max = 100;
					bool wiggle = false;
					bool translate = !!(cmd.flags & DRAWGEM_TRANSLATABLE);
					if(max != 0 || value < 0)
					{
						value = (value*100)/max;
						if(value > 100)
							value = 100;
					}
					else
					{
						value = 0;
					}
					if(health != CPlayer->health)
					{
						wiggle = !!(cmd.flags & DRAWGEM_WIGGLE);
					}
					DrawGem(Images[cmd.special], Images[cmd.sprite], value, cmd.x, cmd.y, cmd.special2, cmd.special3, cmd.special4+1, wiggle, translate);
					break;
				}
				case SBARINFO_DRAWSHADER:
				{
					FBarShader *const shaders[4] =
					{
						&shader_horz_normal, &shader_horz_reverse,
						&shader_vert_normal, &shader_vert_reverse
					};
					bool vertical = !!(cmd.flags & DRAWSHADER_VERTICAL);
					bool reverse = !!(cmd.flags & DRAWSHADER_REVERSE);
					screen->DrawTexture (shaders[(vertical << 1) + reverse], ST_X+cmd.x, ST_Y+cmd.y,
						DTA_DestWidth, cmd.special,
						DTA_DestHeight, cmd.special2,
						DTA_Bottom320x200, Scaled,
						DTA_AlphaChannel, true,
						DTA_FillColor, 0,
						TAG_DONE);
					break;
				}
				case SBARINFO_DRAWSTRING:
					if(drawingFont != cmd.font)
					{
						drawingFont = cmd.font;
					}
					DrawString(cmd.string[0], cmd.x - drawingFont->StringWidth(cmd.string[0]), cmd.y, cmd.translation);
					break;
				case SBARINFO_GAMEMODE:
					if(((cmd.flags & GAMETYPE_SINGLEPLAYER) && !multiplayer) ||
						((cmd.flags & GAMETYPE_DEATHMATCH) && deathmatch) ||
						((cmd.flags & GAMETYPE_COOPERATIVE) && multiplayer && !deathmatch) ||
						((cmd.flags & GAMETYPE_TEAMGAME) && teamplay))
					{
						doCommands(cmd.subBlock);
					}
					break;
				case SBARINFO_PLAYERCLASS:
					int spawnClass = CPlayer->cls->ClassIndex;
					if(cmd.special == spawnClass || cmd.special2 == spawnClass || cmd.special3 == spawnClass)
					{
						doCommands(cmd.subBlock);
					}
					break;
			}
		}
	}

	//draws an image with the specified flags
	void DrawGraphic(FTexture* texture, int x, int y, int flags)
	{
		if((flags & DRAWIMAGE_OFFSET_CENTER))
		{
			x -= (texture->GetWidth()/2)-texture->LeftOffset;
			y -= (texture->GetHeight()/2)-texture->TopOffset;
		}
		if((flags & DRAWIMAGE_TRANSLATABLE))
			DrawImage(texture, x, y, getTranslation());
		else
			DrawImage(texture, x, y);
	}

	void DrawString(const char* str, int x, int y, EColorRange translation, int spacing=0)
	{
		x += spacing;
		while(*str != '\0')
		{
			if(*str == ' ')
			{
				x += drawingFont->GetSpaceWidth();
				str++;
				continue;
			}
			int width = drawingFont->GetCharWidth((int) *str);
			FTexture* character = drawingFont->GetChar((int) *str, &width);
			if(character == NULL) //missing character.
			{
				str++;
				continue;
			}
			x += (character->LeftOffset+1); //ignore x offsets since we adapt to character size
			DrawImage(character, x, y, drawingFont->GetColorTranslation(translation));
			x += width + spacing - (character->LeftOffset+1);
			str++;
		}
	}

	//draws the specified number up to len digits
	void DrawNumber(int num, int len, int x, int y, EColorRange translation, int spacing=0)
	{
		FString value;
		int maxval = (int) pow(10., len);
		num = clamp(num, -maxval+1, maxval-1);
		value.Format("%d", num);
		x -= int(drawingFont->StringWidth(value)+(spacing * value.Len()));
		DrawString(value, x, y, translation, spacing);
	}

	//draws the mug shot
	void DrawFace(int accuracy, bool xdth, bool animatedgodmode, int x, int y)
	{
		if(CPlayer->health > 0)
		{
			if(faceIndex == ST_FACEGOD || faceIndex == ST_FACEGODLEFT || faceIndex == ST_FACEGODRIGHT) //nothing fancy to do here
			{
				if(animatedgodmode)
					DrawImage(Faces[faceIndex], x, y);
				else
					DrawImage(Faces[ST_FACEGOD], x, y);
			}
			else
			{
				int level = 0;
				for(level = 0;CPlayer->health < (accuracy-level-1)*(100/accuracy);level++);
				int face = faceIndex + level*8;
				DrawImage(Faces[face], x, y);
			}
		}
		else //dead
		{
			if(!xdth || CPlayer->mo->health > -CPlayer->mo->GetDefault()->health)
			{
				DrawImage(Faces[ST_FACEDEAD], x, y);
			}
			else
			{
				if(faceIndex != ST_FACEXDEAD+5 && faceIndex >= ST_FACEXDEAD) //animate
				{
					if(faceTimer == 0)
					{
						faceIndex++;
						faceTimer = ST_XDTHTIME;
					}
					faceTimer--;
				}
				else if(faceIndex < ST_FACEXDEAD) //set to xdeath
				{
					faceIndex = ST_FACEXDEAD;
					faceTimer = ST_XDTHTIME;
				}
				DrawImage(Faces[faceIndex], x, y);
			}
		}
	}

	//Does some face drawing logic.
	void getNewFace(int number)
	{
		int 		i;
		angle_t 	badguyangle;
		angle_t 	diffang;

		if(CPlayer->health > 0)
		{
			if(weaponGrin)
			{
				weaponGrin = false;
				if(CPlayer->bonuscount)
				{
					faceTimer = ST_GRINTIME;
					faceIndex = ST_FACEGRIN;
					return;
				}
			}
			// getting hurt
			if (CPlayer->damagecount)
			{
				int damageAngle = ST_FACEPAIN;
				if(CPlayer->attacker && CPlayer->attacker != CPlayer->mo)
				{
					if(CPlayer->mo != NULL)
					{
						//some more doom_sbar C&P
						badguyangle = R_PointToAngle2(CPlayer->mo->x, CPlayer->mo->y, CPlayer->attacker->x, CPlayer->attacker->y);
						if (badguyangle > CPlayer->mo->angle)
						{
							// whether right or left
							diffang = badguyangle - CPlayer->mo->angle;
							i = diffang > ANG180; 
						}
						else
						{
							// whether left or right
							diffang = CPlayer->mo->angle - badguyangle;
							i = diffang <= ANG180; 
						} // confusing, aint it?
						if(i && diffang >= ANG45)
						{
							damageAngle = ST_FACEPAINRIGHT;
						}
						else if(!i && diffang >= ANG45)
						{
							damageAngle = ST_FACEPAINLEFT;
						}
					}
				}
				faceTimer = ST_PAINTIME;
				if (mugshotHealth != -1 && CPlayer->health - mugshotHealth > 20)
				{
					faceIndex = ST_FACEOUCH;
				}
				else
				{
					faceIndex = damageAngle;
				}
				return;
			}
			if((CPlayer->cmd.ucmd.buttons & (BT_ATTACK|BT_ALTATTACK)) && !(CPlayer->cheats & (CF_FROZEN | CF_TOTALLYFROZEN)))
			{
				if(rampageTimer == ST_RAMPAGETIME)
				{
					faceIndex = ST_FACEPAIN;
					faceTimer = 1;
				}
				else
				{
					rampageTimer++;
				}
			}
			else
			{
				rampageTimer = 0;
			}
			// invulnerability
			if (((CPlayer->cheats & CF_GODMODE) || (CPlayer->mo != NULL && CPlayer->mo->flags2 & MF2_INVULNERABLE)) && 
				(faceTimer == 0 || faceIndex <= 2))
			{
				faceIndex = ST_FACEGOD + (number % 3);
				faceTimer = ST_FACETIME;
			}

			if (faceTimer == 0)
			{
				faceIndex = (number % 3); //should generate 0, 1, or 2
				faceTimer = ST_FACETIME;
			}
			faceTimer--;
			mugshotHealth = CPlayer->health;
		}
	}

	void DrawInventoryBar(int type, int num, int x, int y, bool alwaysshow, 
		int counterx, int countery, EColorRange translation, bool drawArtiboxes, bool noArrows, bool alwaysshowcounter)
	{ //yes, there is some Copy & Paste here too
		const AInventory *item;
		int i;

		// If the player has no artifacts, don't draw the bar
		CPlayer->mo->InvFirst = ValidateInvFirst(num);
		if(CPlayer->mo->InvFirst != NULL || alwaysshow)
		{
			for(item = CPlayer->mo->InvFirst, i = 0; item != NULL && i < num; item = item->NextInv(), ++i)
			{
				if(drawArtiboxes)
				{
					DrawImage (Images[invBarOffset + imgARTIBOX], x+i*31, y);
				}
				DrawDimImage (TexMan(item->Icon), x+i*31, y, item->Amount <= 0);
				if(alwaysshowcounter || item->Amount != 1)
				{
					DrawNumber(item->Amount, 3, counterx+i*31, countery, translation);
				}
				if(item == CPlayer->mo->InvSel)
				{
					if(type == GAME_Heretic)
					{
						DrawImage(Images[invBarOffset + imgSELECTBOX], x+i*31, y+29);
					}
					else
					{
						DrawImage(Images[invBarOffset + imgSELECTBOX], x+i*31, y);
					}
				}
			}
			for (; i < num && drawArtiboxes; ++i)
			{
				DrawImage (Images[invBarOffset + imgARTIBOX], x+i*31, y);
			}
			// Is there something to the left?
			if (!noArrows && CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
			{
				DrawImage (Images[!(gametic & 4) ?
					invBarOffset + imgINVLFGEM1 : invBarOffset + imgINVLFGEM2], x-12, y);
			}
			// Is there something to the right?
			if (!noArrows && item != NULL)
			{
				DrawImage (Images[!(gametic & 4) ?
					invBarOffset + imgINVRTGEM1 : invBarOffset + imgINVRTGEM2], x+num*31+2, y);
			}
		}
	}

	//draws heretic/hexen style life gems
	void DrawGem(FTexture* chain, FTexture* gem, int value, int x, int y, int padleft, int padright, int chainsize, 
				bool wiggle, bool translate)
	{
		if(value > 100)
			value = 100;
		else if(value < 0)
			value = 0;
		if(wiggle)
			y += chainWiggle;
		int gemWidth = gem->GetWidth();
		int chainWidth = chain->GetWidth();
		int offset = (int) (((double) (chainWidth-padleft-padright)/100)*value);
		if(chain != NULL)
		{
			DrawImage(chain, x+(offset%chainsize), y);
		}
		if(gem != NULL)
			DrawImage(gem, x+padleft+offset, y, translate ? getTranslation() : NULL);
	}

	FRemapTable* getTranslation()
	{
		if(gameinfo.gametype & GAME_Raven)
			return translationtables[TRANSLATION_PlayersExtra][int(CPlayer - players)];
		return translationtables[TRANSLATION_Players][int(CPlayer - players)];
	}

	FImageCollection Images;
	FImageCollection Faces;
	FPlayerSkin *oldSkin;
	FFont *drawingFont;
	FString lastPrefix;
	bool weaponGrin;
	int faceTimer;
	int rampageTimer;
	int faceIndex;
	int oldHealth;
	int mugshotHealth;
	int chainWiggle;
	int artiflash;
	unsigned int invBarOffset;
	FBarShader shader_horz_normal;
	FBarShader shader_horz_reverse;
	FBarShader shader_vert_normal;
	FBarShader shader_vert_reverse;
};

FBaseStatusBar *CreateCustomStatusBar ()
{
	return new FSBarInfo;
}
