#include "doomtype.h"
#include "doomstat.h"
#include "sc_man.h"
#include "v_font.h"
#include "w_wad.h"
#include "sbar.h"
#include "d_player.h"
#include "sbarinfo.h"
#include "templates.h"
#include "m_random.h"
#include "gi.h"
#include "i_system.h"

SBarInfo *SBarInfoScript;
TArray<MugShotState> MugShotStates;

static const char *SBarInfoTopLevel[] =
{
	"base",
	"height",
	"interpolatehealth",
	"interpolatearmor",
	"completeborder",
	"statusbar",
	"mugshot",
	NULL
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
	"drawkeybar",
	"gamemode",
	"playerclass",
	"weaponammo", //event
	NULL
};

static void FreeSBarInfoScript()
{
	if (SBarInfoScript != NULL)
	{
		delete SBarInfoScript;
		SBarInfoScript = NULL;
	}
}

void SBarInfo::Load()
{
	if(Wads.CheckNumForName("SBARINFO") != -1)
	{
		Printf ("ParseSBarInfo: Loading custom status bar definition.\n");
		int lastlump, lump;
		lastlump = 0;
		while((lump = Wads.FindLump("SBARINFO", &lastlump)) != -1)
		{
			if(SBarInfoScript == NULL)
				SBarInfoScript = new SBarInfo(lump);
			else //We now have to load multiple SBarInfo Lumps so the 2nd time we need to use this method instead.
				SBarInfoScript->ParseSBarInfo(lump);
		}
		atterm(FreeSBarInfoScript);
	}
}

//SBarInfo Script Reader
void SBarInfo::ParseSBarInfo(int lump)
{
	gameType = gameinfo.gametype;
	Printf("%d=%d", gameinfo.gametype, GAME_Any);
	bool baseSet = false;
	FScanner sc(lump, Wads.GetLumpFullName(lump));
	sc.SetCMode(true);
	while(sc.CheckToken(TK_Identifier) || sc.CheckToken(TK_Include))
	{
		if(sc.TokenType == TK_Include)
		{
			sc.MustGetToken(TK_StringConst);
			int lump = Wads.CheckNumForFullName(sc.String); //zip/pk3
			//Do a normal wad lookup.
			if (lump == -1 && sc.StringLen <= 8 && !strchr(sc.String, '/')) 
				lump = Wads.CheckNumForName(sc.String);
			if (lump == -1) 
				sc.ScriptError("Lump '%s' not found", sc.String);
			ParseSBarInfo(lump);
			continue;
		}
		switch(sc.MustMatchString(SBarInfoTopLevel))
		{
			case SBARINFO_BASE:
				baseSet = true;
				if(!sc.CheckToken(TK_None))
					sc.MustGetToken(TK_Identifier);
				if(sc.Compare("Doom"))
					gameType = GAME_Doom;
				else if(sc.Compare("Heretic"))
					gameType = GAME_Heretic;
				else if(sc.Compare("Hexen"))
					gameType = GAME_Hexen;
				else if(sc.Compare("Strife"))
					gameType = GAME_Strife;
				else if(sc.Compare("None"))
					gameType = GAME_Any;
				else
					sc.ScriptError("Bad game name: %s", sc.String);
				sc.MustGetToken(';');
				break;
			case SBARINFO_HEIGHT:
				sc.MustGetToken(TK_IntConst);
				this->height = sc.Number;
				sc.MustGetToken(';');
				break;
			case SBARINFO_INTERPOLATEHEALTH: //mimics heretics interpolated health values.
				if(sc.CheckToken(TK_True))
				{
					interpolateHealth = true;
				}
				else
				{
					sc.MustGetToken(TK_False);
					interpolateHealth = false;
				}
				if(sc.CheckToken(',')) //speed param
				{
					sc.MustGetToken(TK_IntConst);
					this->interpolationSpeed = sc.Number;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_INTERPOLATEARMOR: //Since interpolatehealth is such a popular command
				if(sc.CheckToken(TK_True))
				{
					interpolateArmor = true;
				}
				else
				{
					sc.MustGetToken(TK_False);
					interpolateArmor = false;
				}
				if(sc.CheckToken(',')) //speed
				{
					sc.MustGetToken(TK_IntConst);
					this->armorInterpolationSpeed = sc.Number;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_COMPLETEBORDER: //draws the border instead of an HOM
				if(sc.CheckToken(TK_True))
				{
					completeBorder = true;
				}
				else
				{
					sc.MustGetToken(TK_False);
					completeBorder = false;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_STATUSBAR:
			{
				if(!baseSet) //If the user didn't explicitly define a base, do so now.
					gameType = GAME_Any;
				int barNum = 0;
				if(!sc.CheckToken(TK_None))
				{
					sc.MustGetToken(TK_Identifier);
					barNum = sc.MustMatchString(StatusBars);
				}
				this->huds[barNum] = SBarInfoBlock();
				while(sc.CheckToken(','))
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("forcescaled"))
					{
						this->huds[barNum].forceScaled = true;
					}
					else
					{
						sc.ScriptError("Unkown flag '%s'.", sc.String);
					}
				}
				sc.MustGetToken('{');
				if(barNum == STBAR_AUTOMAP)
				{
					automapbar = true;
				}
				ParseSBarInfoBlock(sc, this->huds[barNum]);
				break;
			}
			case SBARINFO_MUGSHOT:
			{
				sc.MustGetToken(TK_StringConst);
				MugShotState state(sc.String);
				if(sc.CheckToken(',')) //first loop must be a comma
				{
					do
					{
						sc.MustGetToken(TK_Identifier);
						if(sc.Compare("health"))
							state.usesLevels = true;
						else if(sc.Compare("health2"))
							state.usesLevels = state.health2 = true;
						else if(sc.Compare("healthspecial"))
							state.usesLevels = state.healthspecial = true;
						else if(sc.Compare("directional"))
							state.directional = true;
						else
							sc.ScriptError("Unknown MugShot state flag '%s'.", sc.String);
					}
					while(sc.CheckToken(',') || sc.CheckToken('|'));
				}
				ParseMugShotBlock(sc, state);
				int index = 0;
				if((index = FindMugShotStateIndex(state.state)) != -1) //We already had this state, remove the old one.
				{
					MugShotStates.Delete(index);
				}
				MugShotStates.Push(state);
				break;
			}
		}
	}
}

void SBarInfo::ParseSBarInfoBlock(FScanner &sc, SBarInfoBlock &block)
{
	while(sc.CheckToken(TK_Identifier))
	{
		SBarInfoCommand cmd;

		switch(cmd.type = sc.MustMatchString(SBarInfoRoutineLevel))
		{
			case SBARINFO_DRAWSWITCHABLEIMAGE:
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("weaponslot"))
				{
					cmd.flags = DRAWIMAGE_WEAPONSLOT;
					sc.MustGetToken(TK_IntConst);
					cmd.value = sc.Number;
				}
				else if(sc.Compare("invulnerable"))
				{
					cmd.flags = DRAWIMAGE_INVULNERABILITY;
				}
				else
				{
					cmd.setString(sc, sc.String, 0);
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
					{
						sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
					}
				}
				if(sc.CheckToken(TK_AndAnd))
				{
					cmd.flags += DRAWIMAGE_SWITCHABLE_AND;
					sc.MustGetToken(TK_Identifier);
					cmd.setString(sc, sc.String, 1);
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
					{
						sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
					}
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					cmd.special = newImage(sc.String);
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					cmd.special2 = newImage(sc.String);
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					cmd.special3 = newImage(sc.String);
					sc.MustGetToken(',');
				}
				else
				{
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					cmd.special = newImage(sc.String);
					sc.MustGetToken(',');
				}
			case SBARINFO_DRAWIMAGE:
			{
				bool getImage = true;
				if(sc.CheckToken(TK_Identifier))
				{
					getImage = false;
					if(sc.Compare("playericon"))
						cmd.flags += DRAWIMAGE_PLAYERICON;
					else if(sc.Compare("ammoicon1"))
						cmd.flags += DRAWIMAGE_AMMO1;
					else if(sc.Compare("ammoicon2"))
						cmd.flags += DRAWIMAGE_AMMO2;
					else if(sc.Compare("armoricon"))
						cmd.flags += DRAWIMAGE_ARMOR;
					else if(sc.Compare("weaponicon"))
						cmd.flags += DRAWIMAGE_WEAPONICON;
					else if(sc.Compare("translatable"))
					{
						cmd.flags += DRAWIMAGE_TRANSLATABLE;
						getImage = true;
					}
					else
					{
						cmd.flags += DRAWIMAGE_INVENTORYICON;
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
						{
							sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
						}
						cmd.sprite = ((AInventory *)GetDefaultByType(item))->Icon;
					}
				}
				if(getImage)
				{
					sc.MustGetToken(TK_StringConst);
					cmd.sprite = newImage(sc.String);
				}
				sc.MustGetToken(',');
				this->getCoordinates(sc, cmd);
				if(sc.CheckToken(','))
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("center"))
						cmd.flags += DRAWIMAGE_OFFSET_CENTER;
					else
						sc.ScriptError("Expected 'center' got '%s' instead.", sc.String);
				}
				sc.MustGetToken(';');
				break;
			}
			case SBARINFO_DRAWNUMBER:
				cmd.special4 = cmd.special3 = -1;
				sc.MustGetToken(TK_IntConst);
				cmd.special = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				cmd.font = V_GetFont(sc.String);
				if(cmd.font == NULL)
					sc.ScriptError("Unknown font '%s'.", sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				cmd.translation = this->GetTranslation(sc, sc.String);
				sc.MustGetToken(',');
				if(sc.CheckToken(TK_IntConst))
				{
					cmd.value = sc.Number;
					sc.MustGetToken(',');
				}
				else
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("health"))
						cmd.flags = DRAWNUMBER_HEALTH;
					else if(sc.Compare("armor"))
						cmd.flags = DRAWNUMBER_ARMOR;
					else if(sc.Compare("ammo1"))
						cmd.flags = DRAWNUMBER_AMMO1;
					else if(sc.Compare("ammo2"))
						cmd.flags = DRAWNUMBER_AMMO2;
					else if(sc.Compare("ammo")) //request the next string to be an ammo type
					{
						sc.MustGetToken(TK_Identifier);
						cmd.setString(sc, sc.String, 0);
						cmd.flags = DRAWNUMBER_AMMO;
						const PClass* ammo = PClass::FindClass(sc.String);
						if(ammo == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(ammo)) //must be a kind of ammo
						{
							sc.ScriptError("'%s' is not a type of ammo.", sc.String);
						}
					}
					else if(sc.Compare("ammocapacity"))
					{
						sc.MustGetToken(TK_Identifier);
						cmd.setString(sc, sc.String, 0);
						cmd.flags = DRAWNUMBER_AMMOCAPACITY;
						const PClass* ammo = PClass::FindClass(sc.String);
						if(ammo == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(ammo)) //must be a kind of ammo
						{
							sc.ScriptError("'%s' is not a type of ammo.", sc.String);
						}
					}
					else if(sc.Compare("frags"))
						cmd.flags = DRAWNUMBER_FRAGS;
					else if(sc.Compare("kills"))
						cmd.flags += DRAWNUMBER_KILLS;
					else if(sc.Compare("monsters"))
						cmd.flags += DRAWNUMBER_MONSTERS;
					else if(sc.Compare("items"))
						cmd.flags += DRAWNUMBER_ITEMS;
					else if(sc.Compare("totalitems"))
						cmd.flags += DRAWNUMBER_TOTALITEMS;
					else if(sc.Compare("secrets"))
						cmd.flags += DRAWNUMBER_SECRETS;
					else if(sc.Compare("totalsecrets"))
						cmd.flags += DRAWNUMBER_TOTALSECRETS;
					else if(sc.Compare("armorclass"))
						cmd.flags += DRAWNUMBER_ARMORCLASS;
					else
					{
						cmd.flags = DRAWNUMBER_INVENTORY;
						sc.MustGetToken(TK_Identifier);
						cmd.setString(sc, sc.String, 0);
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of ammo
						{
							sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
						}
					}
					sc.MustGetToken(',');
				}
				this->getCoordinates(sc, cmd);
				if(sc.CheckToken(','))
				{
					bool needsComma = false;
					if(sc.CheckToken(TK_IntConst)) //font spacing
					{
						cmd.special2 = sc.Number;
						needsComma = true;
					}
					if(!needsComma || sc.CheckToken(',')) //2nd coloring for "low-on" value
					{
						sc.MustGetToken(TK_Identifier);
						cmd.translation2 = this->GetTranslation(sc, sc.String);
						sc.MustGetToken(',');
						sc.MustGetToken(TK_IntConst);
						cmd.special3 = sc.Number;
						if(sc.CheckToken(',')) //3rd coloring for "high-on" value
						{
							sc.MustGetToken(TK_Identifier);
							cmd.translation3 = this->GetTranslation(sc, sc.String);
							sc.MustGetToken(',');
							sc.MustGetToken(TK_IntConst);
							cmd.special4 = sc.Number;
						}
					}
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWMUGSHOT:
				sc.MustGetToken(TK_StringConst);
				cmd.setString(sc, sc.String, 0, 3, true);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst); //accuracy
				if(sc.Number < 1 || sc.Number > 9)
					sc.ScriptError("Expected a number between 1 and 9, got %d instead.", sc.Number);
				cmd.special = sc.Number;
				sc.MustGetToken(',');
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("xdeathface"))
						cmd.flags += DRAWMUGSHOT_XDEATHFACE;
					else if(sc.Compare("animatedgodmode"))
						cmd.flags += DRAWMUGSHOT_ANIMATEDGODMODE;
					else
						sc.ScriptError("Unknown flag '%s'.", sc.String);
					if(!sc.CheckToken('|'))
						sc.MustGetToken(',');
				}
				this->getCoordinates(sc, cmd);
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWSELECTEDINVENTORY:
			{
				bool alternateonempty = false;
				while(true) //go until we get a font (non-flag)
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("alternateonempty"))
					{
						alternateonempty = true;
						cmd.flags += DRAWSELECTEDINVENTORY_ALTERNATEONEMPTY;
					}
					else if(sc.Compare("artiflash"))
					{
						cmd.flags += DRAWSELECTEDINVENTORY_ARTIFLASH;
					}
					else if(sc.Compare("alwaysshowcounter"))
					{
						cmd.flags += DRAWSELECTEDINVENTORY_ALWAYSSHOWCOUNTER;
					}
					else
					{
						cmd.font = V_GetFont(sc.String);
						if(cmd.font == NULL)
							sc.ScriptError("Unknown font '%s'.", sc.String);
						sc.MustGetToken(',');
						break;
					}
					if(!sc.CheckToken('|'))
						sc.MustGetToken(',');
				}
				sc.MustGetToken(TK_IntConst);
				cmd.x = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				cmd.y = sc.Number - (200 - this->height);
				cmd.special2 = cmd.x + 30;
				cmd.special3 = cmd.y + 24;
				cmd.translation = CR_GOLD;
				if(sc.CheckToken(',')) //more font information
				{
					sc.MustGetToken(TK_IntConst);
					cmd.special2 = sc.Number;
					sc.MustGetToken(',');
					sc.MustGetToken(TK_IntConst);
					cmd.special3 = sc.Number - (200 - this->height);
					if(sc.CheckToken(','))
					{
						sc.MustGetToken(TK_Identifier);
						cmd.translation = this->GetTranslation(sc, sc.String);
						if(sc.CheckToken(','))
						{
							sc.MustGetToken(TK_IntConst);
							cmd.special4 = sc.Number;
						}
					}
				}
				if(alternateonempty)
				{
					sc.MustGetToken('{');
					this->ParseSBarInfoBlock(sc, cmd.subBlock);
				}
				else
				{
					sc.MustGetToken(';');
				}
				break;
			}
			case SBARINFO_DRAWINVENTORYBAR:
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("Heretic"))
				{
					cmd.special = GAME_Heretic;
				}
				if(sc.Compare("Doom") || sc.Compare("Heretic"))
				{
					sc.MustGetToken(',');
					while(sc.CheckToken(TK_Identifier))
					{
						if(sc.Compare("alwaysshow"))
						{
							cmd.flags += DRAWINVENTORYBAR_ALWAYSSHOW;
						}
						else if(sc.Compare("noartibox"))
						{
							cmd.flags += DRAWINVENTORYBAR_NOARTIBOX;
						}
						else if(sc.Compare("noarrows"))
						{
							cmd.flags += DRAWINVENTORYBAR_NOARROWS;
						}
						else if(sc.Compare("alwaysshowcounter"))
						{
							cmd.flags += DRAWINVENTORYBAR_ALWAYSSHOWCOUNTER;
						}
						else
						{
							sc.ScriptError("Unknown flag '%s'.", sc.String);
						}
						if(!sc.CheckToken('|'))
							sc.MustGetToken(',');
					}
					sc.MustGetToken(TK_IntConst);
					cmd.value = sc.Number;
					sc.MustGetToken(',');
					sc.MustGetToken(TK_Identifier);
					cmd.font = V_GetFont(sc.String);
					if(cmd.font == NULL)
						sc.ScriptError("Unknown font '%s'.", sc.String);
				}
				else
				{
					sc.ScriptError("Unkown style '%s'.", sc.String);
				}
				sc.MustGetToken(',');
				this->getCoordinates(sc, cmd);
				cmd.special2 = cmd.x + 26;
				cmd.special3 = cmd.y + 22;
				cmd.translation = CR_GOLD;
				if(sc.CheckToken(',')) //more font information
				{
					sc.MustGetToken(TK_IntConst);
					cmd.special2 = sc.Number;
					sc.MustGetToken(',');
					sc.MustGetToken(TK_IntConst);
					cmd.special3 = sc.Number - (200 - this->height);
					if(sc.CheckToken(','))
					{
						sc.MustGetToken(TK_Identifier);
						cmd.translation = this->GetTranslation(sc, sc.String);
						if(sc.CheckToken(','))
						{
							sc.MustGetToken(TK_IntConst);
							cmd.special4 = sc.Number;
						}
					}
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWBAR:
				sc.MustGetToken(TK_StringConst);
				cmd.sprite = newImage(sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_StringConst);
				cmd.special = newImage(sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier); //yeah, this is the same as drawnumber, there might be a better way to copy it...
				if(sc.Compare("health"))
				{
					cmd.flags = DRAWNUMBER_HEALTH;
					if(sc.CheckToken(TK_Identifier)) //comparing reference
					{
						cmd.setString(sc, sc.String, 0);
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of inventory
						{
							sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
						}
					}
					else
						cmd.special2 = DRAWBAR_COMPAREDEFAULTS;
				}
				else if(sc.Compare("armor"))
				{
					cmd.flags = DRAWNUMBER_ARMOR;
					if(sc.CheckToken(TK_Identifier))
					{
						cmd.setString(sc, sc.String, 0);
						const PClass* item = PClass::FindClass(sc.String);
						if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of inventory
						{
							sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
						}
					}
					else
						cmd.special2 = DRAWBAR_COMPAREDEFAULTS;
				}
				else if(sc.Compare("ammo1"))
					cmd.flags = DRAWNUMBER_AMMO1;
				else if(sc.Compare("ammo2"))
					cmd.flags = DRAWNUMBER_AMMO2;
				else if(sc.Compare("ammo")) //request the next string to be an ammo type
				{
					sc.MustGetToken(TK_Identifier);
					cmd.setString(sc, sc.String, 0);
					cmd.flags = DRAWNUMBER_AMMO;
					const PClass* ammo = PClass::FindClass(sc.String);
					if(ammo == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(ammo)) //must be a kind of ammo
					{
						sc.ScriptError("'%s' is not a type of ammo.", sc.String);
					}
				}
				else if(sc.Compare("frags"))
					cmd.flags = DRAWNUMBER_FRAGS;
				else if(sc.Compare("kills"))
					cmd.flags = DRAWNUMBER_KILLS;
				else if(sc.Compare("items"))
					cmd.flags = DRAWNUMBER_ITEMS;
				else if(sc.Compare("secrets"))
					cmd.flags = DRAWNUMBER_SECRETS;
				else
				{
					cmd.flags = DRAWNUMBER_INVENTORY;
					cmd.setString(sc, sc.String, 0);
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !RUNTIME_CLASS(AInventory)->IsAncestorOf(item)) //must be a kind of ammo
					{
						sc.ScriptError("'%s' is not a type of inventory item.", sc.String);
					}
				}
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("horizontal"))
					cmd.special2 += DRAWBAR_HORIZONTAL;
				else if(!sc.Compare("vertical"))
					sc.ScriptError("Unknown direction '%s'.", sc.String);
				sc.MustGetToken(',');
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("reverse"))
						cmd.special2 += DRAWBAR_REVERSE;
					else if(sc.Compare("keepoffsets"))
						cmd.special2 += DRAWBAR_KEEPOFFSETS;
					else
						sc.ScriptError("Unkown flag '%s'.", sc.String);
					if(!sc.CheckToken('|'))
						sc.MustGetToken(',');
				}
				this->getCoordinates(sc, cmd);
				if(sc.CheckToken(',')) //border
				{
					sc.MustGetToken(TK_IntConst);
					cmd.special3 = sc.Number;
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWGEM:
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("wiggle"))
						cmd.flags += DRAWGEM_WIGGLE;
					else if(sc.Compare("translatable"))
						cmd.flags += DRAWGEM_TRANSLATABLE;
					else if(sc.Compare("armor"))
						cmd.flags += DRAWGEM_ARMOR;
					else if(sc.Compare("reverse"))
						cmd.flags += DRAWGEM_REVERSE;
					else
						sc.ScriptError("Unknown drawgem flag '%s'.", sc.String);
					if(!sc.CheckToken('|'))
							sc.MustGetToken(',');
				}
				sc.MustGetToken(TK_StringConst); //chain
				cmd.special = newImage(sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_StringConst); //gem
				cmd.sprite = newImage(sc.String);
				sc.MustGetToken(',');
				cmd.special2 = this->getSignedInteger(sc);
				sc.MustGetToken(',');
				cmd.special3 = this->getSignedInteger(sc);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				if(sc.Number < 0)
					sc.ScriptError("Chain size must be a positive number.");
				cmd.special4 = sc.Number;
				sc.MustGetToken(',');
				this->getCoordinates(sc, cmd);
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWSHADER:
				sc.MustGetToken(TK_IntConst);
				cmd.special = sc.Number;
				if(sc.Number < 1)
					sc.ScriptError("Width must be greater than 1.");
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				cmd.special2 = sc.Number;
				if(sc.Number < 1)
					sc.ScriptError("Height must be greater than 1.");
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("vertical"))
					cmd.flags += DRAWSHADER_VERTICAL;
				else if(!sc.Compare("horizontal"))
					sc.ScriptError("Unknown direction '%s'.", sc.String);
				sc.MustGetToken(',');
				if(sc.CheckToken(TK_Identifier))
				{
					if(!sc.Compare("reverse"))
					{
						sc.ScriptError("Exspected 'reverse', got '%s' instead.", sc.String);
					}
					cmd.flags += DRAWSHADER_REVERSE;
					sc.MustGetToken(',');
				}
				this->getCoordinates(sc, cmd);
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWSTRING:
				sc.MustGetToken(TK_Identifier);
				cmd.font = V_GetFont(sc.String);
				if(cmd.font == NULL)
					sc.ScriptError("Unknown font '%s'.", sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				cmd.translation = this->GetTranslation(sc, sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_StringConst);
				cmd.setString(sc, sc.String, 0, -1, false);
				sc.MustGetToken(',');
				this->getCoordinates(sc, cmd);
				sc.MustGetToken(';');
				break;
			case SBARINFO_DRAWKEYBAR:
				sc.MustGetToken(TK_IntConst);
				cmd.value = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("vertical"))
					cmd.flags += DRAWKEYBAR_VERTICAL;
				else if(!sc.Compare("horizontal"))
					sc.ScriptError("Unknown direction '%s'.", sc.String);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				cmd.special = sc.Number;
				sc.MustGetToken(',');
				this->getCoordinates(sc, cmd);
				sc.MustGetToken(';');
				break;
			case SBARINFO_GAMEMODE:
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("singleplayer"))
						cmd.flags += GAMETYPE_SINGLEPLAYER;
					else if(sc.Compare("cooperative"))
						cmd.flags += GAMETYPE_COOPERATIVE;
					else if(sc.Compare("deathmatch"))
						cmd.flags += GAMETYPE_DEATHMATCH;
					else if(sc.Compare("teamgame"))
						cmd.flags += GAMETYPE_TEAMGAME;
					else
						sc.ScriptError("Unknown gamemode: %s", sc.String);
					if(sc.CheckToken('{'))
						break;
					sc.MustGetToken(',');
				}
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			case SBARINFO_PLAYERCLASS:
				cmd.special = cmd.special2 = cmd.special3 = -1;
				for(int i = 0;i < 3 && sc.CheckToken(TK_Identifier);i++) //up to 3 classes
				{
					bool foundClass = false;
					for(unsigned int c = 0;c < PlayerClasses.Size();c++)
					{
						if(stricmp(sc.String, PlayerClasses[c].Type->Meta.GetMetaString(APMETA_DisplayName)) == 0)
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
						sc.ScriptError("Unkown PlayerClass '%s'.", sc.String);
					if(sc.CheckToken('{') || i == 2)
						goto FinishPlayerClass;
					sc.MustGetToken(',');
				}
			FinishPlayerClass:
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
			case SBARINFO_WEAPONAMMO:
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("not"))
				{
					cmd.flags += SBARINFOEVENT_NOT;
					sc.MustGetToken(TK_Identifier);
				}
				for(int i = 0;i < 2;i++)
				{
					cmd.setString(sc, sc.String, i);
					const PClass* ammo = PClass::FindClass(sc.String);
					if(ammo == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(ammo)) //must be a kind of ammo
					{
						sc.ScriptError("'%s' is not a type of ammo.", sc.String);
					}
					if(sc.CheckToken(TK_OrOr))
					{
						cmd.flags += SBARINFOEVENT_OR;
						sc.MustGetToken(TK_Identifier);
					}
					else if(sc.CheckToken(TK_AndAnd))
					{
						cmd.flags += SBARINFOEVENT_AND;
						sc.MustGetToken(TK_Identifier);
					}
					else
						break;
				}
				sc.MustGetToken('{');
				this->ParseSBarInfoBlock(sc, cmd.subBlock);
				break;
		}
		block.commands.Push(cmd);
	}
	sc.MustGetToken('}');
}

void SBarInfo::ParseMugShotBlock(FScanner &sc, MugShotState &state)
{
	sc.MustGetToken('{');
	while(!sc.CheckToken('}'))
	{
		MugShotFrame frame;
		bool multiframe = false;
		if(sc.CheckToken('{'))
			multiframe = true;
		do
		{
			sc.MustGetToken(TK_Identifier);
			if(strlen(sc.String) > 5)
				sc.ScriptError("MugShot frames can not exceed 5 characters.");
			frame.graphic.Push(sc.String);
		}
		while(multiframe && sc.CheckToken(','));
		if(multiframe)
			sc.MustGetToken('}');
		frame.delay = getSignedInteger(sc);
		sc.MustGetToken(';');
		state.frames.Push(frame);
	}
}

void SBarInfo::getCoordinates(FScanner &sc, SBarInfoCommand &cmd)
{
	bool negative = false;
	negative = sc.CheckToken('-');
	sc.MustGetToken(TK_IntConst);
	cmd.x = negative ? -sc.Number : sc.Number;
	sc.MustGetToken(',');
	negative = sc.CheckToken('-');
	sc.MustGetToken(TK_IntConst);
	cmd.y = (negative ? -sc.Number : sc.Number) - (200 - this->height);
}

int SBarInfo::getSignedInteger(FScanner &sc)
{
	if(sc.CheckToken('-'))
	{
		sc.MustGetToken(TK_IntConst);
		return -sc.Number;
	}
	else
	{
		sc.MustGetToken(TK_IntConst);
		return sc.Number;
	}
}

int SBarInfo::newImage(const char* patchname)
{
	if(stricmp(patchname, "nullimage") == 0)
	{
		return -1;
	}
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
EColorRange SBarInfo::GetTranslation(FScanner &sc, char* translation)
{
	EColorRange returnVal = CR_UNTRANSLATED;
	FString namedTranslation; //we must send in "[translation]"
	const BYTE *trans_ptr;
	namedTranslation.Format("[%s]", translation);
	trans_ptr = (const BYTE *)(&namedTranslation[0]);
	if((returnVal = V_ParseFontColor(trans_ptr, CR_UNTRANSLATED, CR_UNTRANSLATED)) == CR_UNDEFINED)
	{
		sc.ScriptError("Missing definition for color %s.", translation);
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
	interpolateArmor = false;
	completeBorder = false;
	interpolationSpeed = 8;
	armorInterpolationSpeed = 8;
	height = 0;
}

SBarInfo::~SBarInfo()
{
	for (size_t i = 0; i < countof(huds); ++i)
	{
		huds[i].commands.Clear();
	}
}

void SBarInfoCommand::setString(FScanner &sc, const char* source, int strnum, int maxlength, bool exact)
{
	if(!exact)
	{
		if(maxlength != -1 && strlen(source) > (unsigned int) maxlength)
		{
			sc.ScriptError("%s is greater than %d characters.", source, maxlength);
			return;
		}
	}
	else
	{
		if(maxlength != -1 && strlen(source) != (unsigned int) maxlength)
		{
			sc.ScriptError("%s must be %d characters.", source, maxlength);
			return;
		}
	}
	string[strnum] = source;
}

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
}

SBarInfoCommand::~SBarInfoCommand()
{
}

SBarInfoBlock::SBarInfoBlock()
{
	forceScaled = false;
}

const MugShotState *FindMugShotState(FString state)
{
	state.ToLower();
	for(unsigned int i = 0;i < MugShotStates.Size();i++)
	{
		if(MugShotStates[i].state == state)
			return &MugShotStates[i];
	}
	return NULL;
}

//Used to allow replacements of states
int FindMugShotStateIndex(FName state)
{
	for(unsigned int i = 0;i < MugShotStates.Size();i++)
	{
		if(MugShotStates[i].state == state)
			return i;
	}
	return -1;
}
