/*
** sbarinfo_display.cpp
**
** Contains all functions required for the display of custom statusbars.
**
**---------------------------------------------------------------------------
** Copyright 2008 Braden Obrzut
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

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
#include "gi.h"
#include "r_translate.h"
#include "r_main.h"
#include "a_weaponpiece.h"
#include "a_strifeglobal.h"

static FRandom pr_chainwiggle; //use the same method of chain wiggling as heretic.

#define ST_RAMPAGETIME	(TICRATE*2)
#define ARTIFLASH_OFFSET (invBarOffset+6)

EXTERN_CVAR(Int, fraglimit)
EXTERN_CVAR(Int, screenblocks)

enum
{
	imgARTIBOX,
	imgSELECTBOX,
	imgCURSOR,
	imgINVLFGEM1,
	imgINVLFGEM2,
	imgINVRTGEM1,
	imgINVRTGEM2,
};

//Used for shading
FBarShader::FBarShader(bool vertical, bool reverse) //make an alpha map
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
	DummySpan[0].Length = vertical ? 256 : 2;
	DummySpan[1].TopOffset = 0;
	DummySpan[1].Length = 0;
}

const BYTE *FBarShader::GetColumn(unsigned int column, const Span **spans_out)
{
	if (spans_out != NULL)
	{
		*spans_out = DummySpan;
	}
	return Pixels + ((column & WidthMask) << HeightBits);
}

const BYTE *FBarShader::GetPixels()
{
	return Pixels;
}

void FBarShader::Unload()
{
}

//SBarInfo Display
DSBarInfo::DSBarInfo () : DBaseStatusBar (SBarInfoScript->height),
	shader_horz_normal(false, false),
	shader_horz_reverse(false, true),
	shader_vert_normal(true, false),
	shader_vert_reverse(true, true)
{
	static const char *InventoryBarLumps[] =
	{
		"ARTIBOX",	"SELECTBO", "INVCURS",	"INVGEML1",
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
	for (i = 0;i < numskins;i++)
	{
		AddFaceToImageCollection (&skins[i], &Images);
	}
	invBarOffset = SBarInfoScript->Images.Size();
	Images.Init(&patchnames[0], patchnames.Size());
	drawingFont = V_GetFont("ConFont");
	oldHealth = 0;
	oldArmor = 0;
	chainWiggle = 0;
	artiflash = 4;
	currentPopup = POP_None;
	pendingPopup = POP_None;
}

DSBarInfo::~DSBarInfo ()
{
	Images.Uninit();
}

void DSBarInfo::Draw (EHudState state)
{
	DBaseStatusBar::Draw(state);
	int hud = STBAR_NORMAL;
	if(state == HUD_StatusBar)
	{
		if(SBarInfoScript->completeBorder) //Fill the statusbar with the border before we draw.
		{
			FTexture *b = TexMan[gameinfo.border->b];
			R_DrawBorder(viewwindowx, viewwindowy + realviewheight + b->GetHeight(), viewwindowx + realviewwidth, SCREENHEIGHT);
			if(screenblocks == 10)
				screen->FlatFill(viewwindowx, viewwindowy + realviewheight, viewwindowx + realviewwidth, viewwindowy + realviewheight + b->GetHeight(), b, true);
		}
		if(SBarInfoScript->automapbar && automapactive)
		{
			hud = STBAR_AUTOMAP;
		}
		else
		{
			hud = STBAR_NORMAL;
		}
	}
	else if(state == HUD_Fullscreen)
	{
		hud = STBAR_FULLSCREEN;
	}
	else
	{
		hud = STBAR_NONE;
	}
	if(SBarInfoScript->huds[hud].forceScaled) //scale the statusbar
	{
		SetScaled(true);
		setsizeneeded = true;
	}
	doCommands(SBarInfoScript->huds[hud], 0, 0, SBarInfoScript->huds[hud].alpha);
	if(CPlayer->inventorytics > 0 && !(level.flags & LEVEL_NOINVENTORYBAR))
	{
		if(state == HUD_StatusBar)
			doCommands(SBarInfoScript->huds[STBAR_INVENTORY], 0, 0, SBarInfoScript->huds[STBAR_INVENTORY].alpha);
		else if(state == HUD_Fullscreen)
			doCommands(SBarInfoScript->huds[STBAR_INVENTORYFULLSCREEN], 0, 0, SBarInfoScript->huds[STBAR_INVENTORYFULLSCREEN].alpha);
	}
	if(currentPopup != POP_None)
	{
		int popbar = 0;
		if(currentPopup == POP_Log)
			popbar = STBAR_POPUPLOG;
		else if(currentPopup == POP_Keys)
			popbar = STBAR_POPUPKEYS;
		else if(currentPopup == POP_Status)
			popbar = STBAR_POPUPSTATUS;
		doCommands(SBarInfoScript->huds[popbar], SBarInfoScript->popups[currentPopup-1].getXOffset(), SBarInfoScript->popups[currentPopup-1].getYOffset(),
			SBarInfoScript->popups[currentPopup-1].getAlpha(SBarInfoScript->huds[popbar].alpha));
	}
}

void DSBarInfo::NewGame ()
{
	if (CPlayer != NULL)
	{
		AttachToPlayer (CPlayer);
	}
}

void DSBarInfo::AttachToPlayer (player_t *player)
{
	DBaseStatusBar::AttachToPlayer(player);
	MugShot.CurrentState = NULL;
}

void DSBarInfo::SetMugShotState (const char *state_name, bool wait_till_done, bool reset)
{
	MugShot.SetState(state_name, wait_till_done, reset);
}

void DSBarInfo::Tick ()
{
	DBaseStatusBar::Tick();
	if(level.time & 1)
		chainWiggle = pr_chainwiggle() & 1;
	if(!SBarInfoScript->interpolateHealth)
	{
		oldHealth = CPlayer->health;
	}
	else
	{
		if(oldHealth > CPlayer->health)
		{
			oldHealth -= clamp((oldHealth - CPlayer->health) >> 2, 1, SBarInfoScript->interpolationSpeed);
		}
		else if(oldHealth < CPlayer->health)
		{
			oldHealth += clamp((CPlayer->health - oldHealth) >> 2, 1, SBarInfoScript->interpolationSpeed);
		}
	}
	AInventory *armor = CPlayer->mo->FindInventory<ABasicArmor>();
	if(armor == NULL)
	{
		oldArmor = 0;
	}
	else
	{
		if(!SBarInfoScript->interpolateArmor)
		{
			oldArmor = armor->Amount;
		}
		else
		{
			if(oldArmor > armor->Amount)
			{
				oldArmor -= clamp((oldArmor - armor->Amount) >> 2, 1, SBarInfoScript->armorInterpolationSpeed);
			}
			else if(oldArmor < armor->Amount)
			{
				oldArmor += clamp((armor->Amount - oldArmor) >> 2, 1, SBarInfoScript->armorInterpolationSpeed);
			}
		}
	}
	if(artiflash)
	{
		artiflash--;
	}

	MugShot.Tick(CPlayer);
	if(currentPopup != POP_None)
	{
		SBarInfoScript->popups[currentPopup-1].tick();
		if(SBarInfoScript->popups[currentPopup-1].opened == false && SBarInfoScript->popups[currentPopup-1].isDoneMoving())
		{
			currentPopup = pendingPopup;
			if(currentPopup != POP_None)
				SBarInfoScript->popups[currentPopup-1].open();
		}
	}
}

void DSBarInfo::ReceivedWeapon(AWeapon *weapon)
{
	MugShot.bEvilGrin = true;
}

void DSBarInfo::FlashItem(const PClass *itemtype)
{
	artiflash = 4;
}

void DSBarInfo::ShowPop(int popnum)
{
	DBaseStatusBar::ShowPop(popnum);
	if(popnum != currentPopup)
	{
		pendingPopup = popnum;
	}
	else
		pendingPopup = POP_None;
	if(currentPopup != POP_None)
		SBarInfoScript->popups[currentPopup-1].close();
	else
	{
		currentPopup = pendingPopup;
		pendingPopup = POP_None;
		if(currentPopup != POP_None)
			SBarInfoScript->popups[currentPopup-1].open();
	}
}

void DSBarInfo::doCommands(SBarInfoBlock &block, int xOffset, int yOffset, int alpha)
{
	//prepare ammo counts
	AAmmo *ammo1, *ammo2;
	int ammocount1, ammocount2;
	GetCurrentAmmo(ammo1, ammo2, ammocount1, ammocount2);
	ABasicArmor *armor = CPlayer->mo->FindInventory<ABasicArmor>();
	int health = CPlayer->mo->health;
	int armorAmount = armor != NULL ? armor->Amount : 0;
	if(SBarInfoScript->interpolateHealth)
	{
		health = oldHealth;
	}
	if(SBarInfoScript->interpolateArmor)
	{
		armorAmount = oldArmor;
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
						DrawGraphic(Images[cmd.special], cmd.x, cmd.y, xOffset, yOffset, alpha, !!(cmd.flags & DRAWIMAGE_TRANSLATABLE), false, !!(cmd.flags & DRAWIMAGE_OFFSET_CENTER));
					else if(cmd.special2 != -1 && drawAlt == 2)
						DrawGraphic(Images[cmd.special2], cmd.x, cmd.y, xOffset, yOffset, alpha, !!(cmd.flags & DRAWIMAGE_TRANSLATABLE), false, !!(cmd.flags & DRAWIMAGE_OFFSET_CENTER));
					else if(cmd.special3 != -1 && drawAlt == 3)
						DrawGraphic(Images[cmd.special3], cmd.x, cmd.y, xOffset, yOffset, alpha, !!(cmd.flags & DRAWIMAGE_TRANSLATABLE), false, !!(cmd.flags & DRAWIMAGE_OFFSET_CENTER));
					break;
				}
			}
			case SBARINFO_DRAWIMAGE:
			{
				FTexture *texture = NULL;
				if((cmd.flags & DRAWIMAGE_PLAYERICON))
					texture = TexMan[CPlayer->mo->ScoreIcon];
				else if((cmd.flags & DRAWIMAGE_AMMO1))
				{
					if(ammo1 != NULL)
						texture = TexMan[ammo1->Icon];
				}
				else if((cmd.flags & DRAWIMAGE_AMMO2))
				{
					if(ammo2 != NULL)
						texture = TexMan[ammo2->Icon];
				}
				else if((cmd.flags & DRAWIMAGE_ARMOR))
				{
					if(armor != NULL && armor->Amount != 0)
						texture = TexMan(armor->Icon);
				}
				else if((cmd.flags & DRAWIMAGE_WEAPONICON))
				{
					AWeapon *weapon = CPlayer->ReadyWeapon;
					if(weapon != NULL && weapon->Icon.isValid())
					{
						texture = TexMan[weapon->Icon];
					}
				}
				else if(cmd.flags & DRAWIMAGE_SIGIL)
				{
					AInventory *item = CPlayer->mo->FindInventory<ASigil>();
					if (item != NULL)
						texture = TexMan[item->Icon];
				}
				else if((cmd.flags & DRAWIMAGE_INVENTORYICON))
					texture = TexMan[cmd.sprite_index];
				else if(cmd.image_index >= 0)
					texture = Images[cmd.image_index];

				DrawGraphic(texture, cmd.x, cmd.y, xOffset, yOffset, alpha, !!(cmd.flags & DRAWIMAGE_TRANSLATABLE), false, !!(cmd.flags & DRAWIMAGE_OFFSET_CENTER));
				break;
			}
			case SBARINFO_DRAWNUMBER:
			{
				int value = cmd.value;
				if(drawingFont != cmd.font)
				{
					drawingFont = cmd.font;
				}
				if(cmd.flags & DRAWNUMBER_HEALTH)
				{
					value = health;
					if(SBarInfoScript->lowerHealthCap && cmd.value < 0) //health shouldn't display negatives
					{
						value = 0;
					}
				}
				else if(cmd.flags & DRAWNUMBER_ARMOR)
				{
					value = armorAmount;
				}
				else if(cmd.flags & DRAWNUMBER_AMMO1)
				{
					value = ammocount1;
					if(ammo1 == NULL) //no ammo, do not draw
					{
						continue;
					}
				}
				else if(cmd.flags & DRAWNUMBER_AMMO2)
				{
					value = ammocount2;
					if(ammo2 == NULL) //no ammo, do not draw
					{
						continue;
					}
				}
				else if(cmd.flags & DRAWNUMBER_AMMO)
				{
					const PClass* ammo = PClass::FindClass(cmd.string[0]);
					AInventory* item = CPlayer->mo->FindInventory(ammo);
					if(item != NULL)
					{
						value = item->Amount;
					}
					else
					{
						value = 0;
					}
				}
				else if(cmd.flags & DRAWNUMBER_AMMOCAPACITY)
				{
					const PClass* ammo = PClass::FindClass(cmd.string[0]);
					AInventory* item = CPlayer->mo->FindInventory(ammo);
					if(item != NULL)
					{
						value = item->MaxAmount;
					}
					else
					{
						value = ((AInventory *)GetDefaultByType(ammo))->MaxAmount;
					}
				}
				else if(cmd.flags & DRAWNUMBER_FRAGS)
					value = CPlayer->fragcount;
				else if(cmd.flags & DRAWNUMBER_KILLS)
					value = level.killed_monsters;
				else if(cmd.flags & DRAWNUMBER_MONSTERS)
					value = level.total_monsters;
				else if(cmd.flags & DRAWNUMBER_ITEMS)
					value = level.found_items;
				else if(cmd.flags & DRAWNUMBER_TOTALITEMS)
					value = level.total_items;
				else if(cmd.flags & DRAWNUMBER_SECRETS)
					value = level.found_secrets;
				else if(cmd.flags & DRAWNUMBER_TOTALSECRETS)
					value = level.total_secrets;
				else if(cmd.flags & DRAWNUMBER_ARMORCLASS)
				{
					AHexenArmor *harmor = CPlayer->mo->FindInventory<AHexenArmor>();
					if(harmor != NULL)
					{
						value = harmor->Slots[0] + harmor->Slots[1] +
							harmor->Slots[2] + harmor->Slots[3] + harmor->Slots[4];
					}
					//Hexen counts basic armor also so we should too.
					if(armor != NULL)
					{
						value += armor->SavePercent;
					}
					value /= (5*FRACUNIT);
				}
				else if(cmd.flags & DRAWNUMBER_GLOBALVAR)
					value = ACS_GlobalVars[cmd.value];
				else if(cmd.flags & DRAWNUMBER_GLOBALARRAY)
					value = ACS_GlobalArrays[cmd.value][consoleplayer];
				else if(cmd.flags & DRAWNUMBER_POWERUPTIME)
				{
					//Get the PowerupType and check to see if the player has any in inventory.
					const PClass* powerupType = ((APowerupGiver*) GetDefaultByType(PClass::FindClass(cmd.string[0])))->PowerupType;
					APowerup* powerup = (APowerup*) CPlayer->mo->FindInventory(powerupType);
					if(powerup != NULL)
					{
						value = powerup->EffectTics / TICRATE + 1;
					}
				}
				else if(cmd.flags & DRAWNUMBER_INVENTORY)
				{
					AInventory* item = CPlayer->mo->FindInventory(PClass::FindClass(cmd.string[0]));
					if(item != NULL)
					{
						value = item->Amount;
					}
					else
					{
						value = 0;
					}
				}
				bool fillzeros = !!(cmd.flags & DRAWNUMBER_FILLZEROS);
				EColorRange translation = cmd.translation;
				if(cmd.special3 != -1 && value <= cmd.special3) //low
					translation = cmd.translation2;
				else if(cmd.special4 != -1 && value >= cmd.special4) //high
					translation = cmd.translation3;
				if((cmd.flags & DRAWNUMBER_WHENNOTZERO) && value == 0)
					break;
				DrawNumber(value, cmd.special, cmd.x, cmd.y, xOffset, yOffset, alpha, translation, cmd.special2, fillzeros);
				break;
			}
			case SBARINFO_DRAWMUGSHOT:
			{
				bool xdth = false;
				bool animatedgodmode = false;
				if(cmd.flags & DRAWMUGSHOT_XDEATHFACE)
					xdth = true;
				if(cmd.flags & DRAWMUGSHOT_ANIMATEDGODMODE)
					animatedgodmode = true;
				DrawFace(cmd.string[0], cmd.special, xdth, animatedgodmode, cmd.x, cmd.y, xOffset, yOffset, alpha);
				break;
			}
			case SBARINFO_DRAWSELECTEDINVENTORY:
				if(CPlayer->mo->InvSel != NULL && !(level.flags & LEVEL_NOINVENTORYBAR))
				{
					if((cmd.flags & DRAWSELECTEDINVENTORY_ARTIFLASH) && artiflash)
					{
						DrawGraphic(Images[ARTIFLASH_OFFSET+(4-artiflash)], cmd.x, cmd.y, xOffset, yOffset, alpha, false, CPlayer->mo->InvSel->Amount <= 0);
					}
					else
					{
						DrawGraphic(TexMan(CPlayer->mo->InvSel->Icon), cmd.x, cmd.y, xOffset, yOffset, alpha, false, CPlayer->mo->InvSel->Amount <= 0);
					}
					if((cmd.flags & DRAWSELECTEDINVENTORY_ALWAYSSHOWCOUNTER) || CPlayer->mo->InvSel->Amount != 1)
					{
						if(drawingFont != cmd.font)
						{
							drawingFont = cmd.font;
						}
						DrawNumber(CPlayer->mo->InvSel->Amount, 3, cmd.special2, cmd.special3, xOffset, yOffset, alpha, cmd.translation, cmd.special4);
					}
				}
				else if((cmd.flags & DRAWSELECTEDINVENTORY_ALTERNATEONEMPTY))
				{
					doCommands(cmd.subBlock, xOffset, yOffset, alpha);
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
				DrawInventoryBar(cmd.special, cmd.value, cmd.x, cmd.y, xOffset, yOffset, alpha, alwaysshow, cmd.special2, cmd.special3, cmd.translation, artibox, noarrows, alwaysshowcounter);
				break;
			}
			case SBARINFO_DRAWBAR:
			{
				if(cmd.image_index == -1 || Images[cmd.image_index] == NULL)
					break; //don't draw anything.
				bool horizontal = !!((cmd.special2 & DRAWBAR_HORIZONTAL));
				bool reverse = !!((cmd.special2 & DRAWBAR_REVERSE));
				fixed_t value = 0;
				int max = 0;
				if(cmd.flags == DRAWNUMBER_HEALTH)
				{
					value = health;
					if(value < 0) //health shouldn't display negatives
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
					else //default to the class's health
					{
						max = CPlayer->mo->GetDefault()->health;
					}
				}
				else if(cmd.flags == DRAWNUMBER_ARMOR)
				{
					value = armorAmount;
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
				else if(cmd.flags & DRAWNUMBER_POWERUPTIME)
				{
					//Get the PowerupType and check to see if the player has any in inventory.
					APowerupGiver* powerupGiver = (APowerupGiver*) GetDefaultByType(PClass::FindClass(cmd.string[0]));
					const PClass* powerupType = powerupGiver->PowerupType;
					APowerup* powerup = (APowerup*) CPlayer->mo->FindInventory(powerupType);
					if(powerup != NULL && powerupType != NULL && powerupGiver != NULL)
					{
						value = powerup->EffectTics + 1;
						if(powerupGiver->EffectTics == 0) //if 0 we need to get the default from the powerup
							max = ((APowerup*) GetDefaultByType(powerupType))->EffectTics + 1;
						else
							max = powerupGiver->EffectTics + 1;
					}
				}
				if(cmd.special3 != 0)
					value = max - value; //invert since the new drawing method requires drawing the bg on the fg.
				if(max != 0 && value > 0)
				{
					value = (value << FRACBITS) / max;
					if(value > FRACUNIT)
						value = FRACUNIT;
				}
				else if(cmd.special3 != 0 && max == 0 && value <= 0)
				{
					value = FRACUNIT;
				}
				else
				{
					value = 0;
				}
				assert(Images[cmd.image_index] != NULL);

				FTexture *fg = Images[cmd.image_index];
				FTexture *bg = (cmd.special != -1) ? Images[cmd.special] : NULL;
				int x, y, w, h;
				int cx, cy, cw, ch, cr, cb;

				// Calc real screen coordinates for bar
				x = cmd.x + ST_X + xOffset;
				y = cmd.y + ST_Y + yOffset;
				w = fg->GetScaledWidth();
				h = fg->GetScaledHeight();
				if (Scaled)
				{
					screen->VirtualToRealCoordsInt(x, y, w, h, 320, 200, true);
				}

				if(cmd.special3 != 0)
				{
					//Draw the whole foreground
					screen->DrawTexture(fg, x, y,
						DTA_DestWidth, w,
						DTA_DestHeight, h,
						DTA_Alpha, alpha,
						TAG_DONE);
				}
				else
				{
					// Draw background
					if (bg != NULL && bg->GetScaledWidth() == fg->GetScaledWidth() && bg->GetScaledHeight() == fg->GetScaledHeight())
					{
						screen->DrawTexture(bg, x, y,
							DTA_DestWidth, w,
							DTA_DestHeight, h,
							DTA_Alpha, alpha,
							TAG_DONE);
					}
					else
					{
						screen->Clear(x, y, x + w, y + h, GPalette.BlackIndex, 0);
					}
				}

				// Calc clipping rect for background
				cx = cmd.x + ST_X + cmd.special3 + xOffset;
				cy = cmd.y + ST_Y + cmd.special3 + yOffset;
				cw = fg->GetScaledWidth() - fg->GetScaledLeftOffset() - cmd.special3 * 2;
				ch = fg->GetScaledHeight() - fg->GetScaledTopOffset() - cmd.special3 * 2;
				if (Scaled)
				{
					screen->VirtualToRealCoordsInt(cx, cy, cw, ch, 320, 200, true);
				}

				if (horizontal)
				{
					if ((cmd.special3 != 0 && reverse) || (cmd.special3 == 0 && !reverse))
					{ // left to right
						cr = cx + FixedMul(cw, value);
					}
					else
					{ // right to left
						cr = cx + cw;
						cx += FixedMul(cw, FRACUNIT - value);
					}
					cb = cy + ch;
				}
				else
				{
					if ((cmd.special3 != 0 && reverse) || (cmd.special3 == 0 && !reverse))
					{ // bottom to top
						cb = cy + ch;
						cy += FixedMul(ch, FRACUNIT - value);
					}
					else
					{ // top to bottom
						cb = cy + FixedMul(ch, value);
					}
					cr = cx + cw;
				}

				// Draw background
				if(cmd.special3 != 0)
				{
					if (bg != NULL && bg->GetScaledWidth() == fg->GetScaledWidth() && bg->GetScaledHeight() == fg->GetScaledHeight())
					{
						screen->DrawTexture(bg, x, y,
							DTA_DestWidth, w,
							DTA_DestHeight, h,
							DTA_ClipLeft, cx,
							DTA_ClipTop, cy,
							DTA_ClipRight, cr,
							DTA_ClipBottom, cb,
							DTA_Alpha, alpha,
							TAG_DONE);
					}
					else
					{
						screen->Clear(cx, cy, cr, cb, GPalette.BlackIndex, 0);
					}
				}
				else
				{
					screen->DrawTexture(fg, x, y,
						DTA_DestWidth, w,
						DTA_DestHeight, h,
						DTA_ClipLeft, cx,
						DTA_ClipTop, cy,
						DTA_ClipRight, cr,
						DTA_ClipBottom, cb,
						DTA_Alpha, alpha,
						TAG_DONE);
				}
				break;
			}
			case SBARINFO_DRAWGEM:
			{
				int value = (cmd.flags & DRAWGEM_ARMOR) ? armorAmount : health;
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
				value = (cmd.flags & DRAWGEM_REVERSE) ? 100 - value : value;
				if(health != CPlayer->health)
				{
					wiggle = !!(cmd.flags & DRAWGEM_WIGGLE);
				}
				DrawGem(Images[cmd.special], Images[cmd.image_index], value, cmd.x, cmd.y, xOffset, yOffset, alpha, cmd.special2, cmd.special3, cmd.special4+1, wiggle, translate);
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
				screen->DrawTexture (shaders[(vertical << 1) + reverse], ST_X+cmd.x+xOffset, ST_Y+cmd.y+yOffset,
					DTA_DestWidth, cmd.special,
					DTA_DestHeight, cmd.special2,
					DTA_Bottom320x200, Scaled,
					DTA_Alpha, alpha,
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
				DrawString(cmd.string[0], cmd.x - drawingFont->StringWidth(cmd.string[0]), cmd.y, xOffset, yOffset, alpha, cmd.translation, cmd.special);
				break;
			case SBARINFO_DRAWKEYBAR:
			{
				bool vertical = !!(cmd.flags & DRAWKEYBAR_VERTICAL);
				AInventory *item = CPlayer->mo->Inventory;
				if(item == NULL)
					break;
				for(int i = 0;i < cmd.value;i++)
				{
					while(!item->Icon.isValid() || !item->IsKindOf(RUNTIME_CLASS(AKey)))
					{
						item = item->Inventory;
						if(item == NULL)
							goto FinishDrawKeyBar;
					}
					if(!vertical)
						DrawGraphic(TexMan[item->Icon], cmd.x+(cmd.special*i), cmd.y, xOffset, yOffset, alpha);
					else
						DrawGraphic(TexMan[item->Icon], cmd.x, cmd.y+(cmd.special*i), xOffset, yOffset, alpha);
					item = item->Inventory;
					if(item == NULL)
						break;
				}
			FinishDrawKeyBar:
				break;
			}
			case SBARINFO_GAMEMODE:
				if(((cmd.flags & GAMETYPE_SINGLEPLAYER) && !multiplayer) ||
					((cmd.flags & GAMETYPE_DEATHMATCH) && deathmatch) ||
					((cmd.flags & GAMETYPE_COOPERATIVE) && multiplayer && !deathmatch) ||
					((cmd.flags & GAMETYPE_TEAMGAME) && teamplay))
				{
					doCommands(cmd.subBlock, xOffset, yOffset, alpha);
				}
				break;
			case SBARINFO_PLAYERCLASS:
			{
				if(CPlayer->cls == NULL) break; //No class so we can not continue
				int spawnClass = CPlayer->cls->ClassIndex;
				if(cmd.special == spawnClass || cmd.special2 == spawnClass || cmd.special3 == spawnClass)
				{
					doCommands(cmd.subBlock, xOffset, yOffset, alpha);
				}
				break;
			}
			case SBARINFO_ASPECTRATIO:
				if(CheckRatio(screen->GetWidth(), screen->GetHeight()) == cmd.value)
				{
					doCommands(cmd.subBlock, xOffset, yOffset, alpha);
				}
				break;
			case SBARINFO_ISSELECTED:
				if(CPlayer->ReadyWeapon != NULL)
				{
					const PClass *weapon1 = PClass::FindClass(cmd.string[0]);
					const PClass *weapon2 = PClass::FindClass(cmd.string[1]);
					if(weapon2 != NULL)
					{
						if((cmd.flags & SBARINFOEVENT_NOT) && (weapon1 != CPlayer->ReadyWeapon->GetSpecies() && weapon2 != CPlayer->ReadyWeapon->GetSpecies()))
							doCommands(cmd.subBlock, xOffset, yOffset, alpha);
						else if(!(cmd.flags & SBARINFOEVENT_NOT) && (weapon1 == CPlayer->ReadyWeapon->GetSpecies() || weapon2 == CPlayer->ReadyWeapon->GetSpecies()))
							doCommands(cmd.subBlock, xOffset, yOffset, alpha);
					}
					else
					{
						if(!(cmd.flags & SBARINFOEVENT_NOT) && weapon1 == CPlayer->ReadyWeapon->GetSpecies())
							doCommands(cmd.subBlock, xOffset, yOffset, alpha);
						else if((cmd.flags & SBARINFOEVENT_NOT) && weapon1 != CPlayer->ReadyWeapon->GetSpecies())
							doCommands(cmd.subBlock, xOffset, yOffset, alpha);
					}
				}
				break;
			case SBARINFO_USESSECONDARYAMMO:
				if((CPlayer->ReadyWeapon->AmmoType2 != NULL && !(cmd.flags & SBARINFOEVENT_NOT)) ||
					(CPlayer->ReadyWeapon->AmmoType2 == NULL && cmd.flags & SBARINFOEVENT_NOT))
				{
					doCommands(cmd.subBlock, xOffset, yOffset, alpha);
				}
				break;
			case SBARINFO_HASWEAPONPIECE:
			{
				AInventory *inv;
				AWeaponHolder *hold;
				const PClass *weapon = PClass::FindClass(cmd.string[0]);
				for(inv = CPlayer->mo->Inventory;inv != NULL;inv=inv->Inventory)
				{
					if(inv->IsKindOf(RUNTIME_CLASS(AWeaponHolder)))
					{
						hold = static_cast<AWeaponHolder*>(inv);
						if(hold->PieceWeapon == weapon)
						{
							if(hold->PieceMask & (1 << (cmd.value-1)))
								doCommands(cmd.subBlock, xOffset, yOffset, alpha);
							break;
						}
					}
				}
				break;
			}
			case SBARINFO_WEAPONAMMO:
				if(CPlayer->ReadyWeapon != NULL)
				{
					const PClass *AmmoType1 = CPlayer->ReadyWeapon->AmmoType1;
					const PClass *AmmoType2 = CPlayer->ReadyWeapon->AmmoType2;
					const PClass *IfAmmo1 = PClass::FindClass(cmd.string[0]);
					const PClass *IfAmmo2 = (cmd.flags & SBARINFOEVENT_OR) || (cmd.flags & SBARINFOEVENT_AND) ?
											PClass::FindClass(cmd.string[1]) : NULL;
					bool usesammo1 = (AmmoType1 != NULL);
					bool usesammo2 = (AmmoType2 != NULL);
					if(!(cmd.flags & SBARINFOEVENT_NOT) && !usesammo1 && !usesammo2) //if the weapon doesn't use ammo don't go though the trouble.
					{
						doCommands(cmd.subBlock, xOffset, yOffset, alpha);
						break;
					}
					//Or means only 1 ammo type needs to match and means both need to match.
					if((cmd.flags & SBARINFOEVENT_OR) || (cmd.flags & SBARINFOEVENT_AND))
					{
						bool match1 = ((usesammo1 && (AmmoType1 == IfAmmo1 || AmmoType1 == IfAmmo2)) || !usesammo1);
						bool match2 = ((usesammo2 && (AmmoType2 == IfAmmo1 || AmmoType2 == IfAmmo2)) || !usesammo2);
						if(((cmd.flags & SBARINFOEVENT_OR) && (match1 || match2)) || ((cmd.flags & SBARINFOEVENT_AND) && (match1 && match2)))
						{
							if(!(cmd.flags & SBARINFOEVENT_NOT))
								doCommands(cmd.subBlock, xOffset, yOffset, alpha);
						}
						else if(cmd.flags & SBARINFOEVENT_NOT)
						{
							doCommands(cmd.subBlock, xOffset, yOffset, alpha);
						}
					}
					else //Every thing here could probably be one long if statement but then it would be more confusing.
					{
						if((usesammo1 && (AmmoType1 == IfAmmo1)) || (usesammo2 && (AmmoType2 == IfAmmo1)))
						{
							if(!(cmd.flags & SBARINFOEVENT_NOT))
								doCommands(cmd.subBlock, xOffset, yOffset, alpha);
						}
						else if(cmd.flags & SBARINFOEVENT_NOT)
						{
							doCommands(cmd.subBlock, xOffset, yOffset, alpha);
						}
					}
				}
				break;
			case SBARINFO_ININVENTORY:
			{
				AInventory *item1 = CPlayer->mo->FindInventory(PClass::FindClass(cmd.string[0]));
				AInventory *item2 = CPlayer->mo->FindInventory(PClass::FindClass(cmd.string[1]));
				if(cmd.flags & SBARINFOEVENT_AND)
				{
					if((item1 != NULL && item2 != NULL) && !(cmd.flags & SBARINFOEVENT_NOT))
						doCommands(cmd.subBlock, xOffset, yOffset, alpha);
					else if((item1 == NULL || item2 == NULL) && (cmd.flags & SBARINFOEVENT_NOT))
						doCommands(cmd.subBlock, xOffset, yOffset, alpha);
				}
				else if(cmd.flags & SBARINFOEVENT_OR)
				{
					if((item1 != NULL || item2 != NULL) && !(cmd.flags & SBARINFOEVENT_NOT))
						doCommands(cmd.subBlock, xOffset, yOffset, alpha);
					else if((item1 == NULL && item2 == NULL) && (cmd.flags & SBARINFOEVENT_NOT))
						doCommands(cmd.subBlock, xOffset, yOffset, alpha);
				}
				else if((item1 != NULL) && !(cmd.flags & SBARINFOEVENT_NOT))
					doCommands(cmd.subBlock, xOffset, yOffset, alpha);
				else if((item1 == NULL) && (cmd.flags & SBARINFOEVENT_NOT))
					doCommands(cmd.subBlock, xOffset, yOffset, alpha);
				break;
			}
		}
	}
}

//draws an image with the specified flags
void DSBarInfo::DrawGraphic(FTexture* texture, int x, int y, int xOffset, int yOffset, int alpha,
	bool translate, bool dim, bool center) //flags
{
	if (texture == NULL)
		return;

	if(center)
	{
		x -= (texture->GetWidth()/2)-texture->LeftOffset;
		y -= (texture->GetHeight()/2)-texture->TopOffset;
	}

	x += ST_X + xOffset;
	y += ST_Y + yOffset;
	int w = texture->GetScaledWidth();
	int h = texture->GetScaledHeight() + y;
	if(Scaled)
		screen->VirtualToRealCoordsInt(x, y, w, h, 320, 200, true);
	h -= y;
	screen->DrawTexture(texture, x, y,
		DTA_DestWidth, w,
		DTA_DestHeight, h,
		DTA_Translation, translate ? getTranslation() : 0,
		DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
		DTA_Alpha, alpha,
		TAG_DONE);
}

void DSBarInfo::DrawString(const char* str, int x, int y, int xOffset, int yOffset, int alpha, EColorRange translation, int spacing)
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
		int width;
		if(SBarInfoScript->spacingCharacter == '\0') //No monospace?
			width = drawingFont->GetCharWidth((int) *str);
		else
			width = drawingFont->GetCharWidth((int) SBarInfoScript->spacingCharacter);
		FTexture* character = drawingFont->GetChar((int) *str, &width);
		if(character == NULL) //missing character.
		{
			str++;
			continue;
		}
		if(SBarInfoScript->spacingCharacter == '\0') //If we are monospaced lets use the offset
			x += (character->LeftOffset+1); //ignore x offsets since we adapt to character size
		int rx = x + ST_X + xOffset;
		int ry = y + ST_Y + yOffset;
		int rw = character->GetScaledWidth();
		int rh = character->GetScaledHeight();
		if(Scaled)
			screen->VirtualToRealCoordsInt(rx, ry, rw, rh, 320, 200, true);
		screen->DrawTexture(character, rx, ry,
			DTA_DestWidth, rw,
			DTA_DestHeight, rh,
			DTA_Translation, drawingFont->GetColorTranslation(translation),
			DTA_Alpha, alpha,
			TAG_DONE);
		if(SBarInfoScript->spacingCharacter == '\0')
			x += width + spacing - (character->LeftOffset+1);
		else //width gets changed at the call to GetChar()
			x += drawingFont->GetCharWidth((int) SBarInfoScript->spacingCharacter) + spacing;
		str++;
	}
}

//draws the specified number up to len digits
void DSBarInfo::DrawNumber(int num, int len, int x, int y, int xOffset, int yOffset, int alpha, EColorRange translation, int spacing, bool fillzeros)
{
	FString value;
	int maxval = (int) ceil(pow(10., len))-1;
	if(!fillzeros || len == 1)
		num = clamp(num, -maxval, maxval);
	else //The community wanted negatives to take the last digit, but we can only do this if there is room
		num = clamp(num, (int) -(ceil(pow(10., len-1))-1), maxval);
	value.Format("%d", num);
	if(fillzeros)
	{
		if(num < 0) //We don't want the negative just yet
			value.Format("%d", -num);
		while(fillzeros && value.Len() < (unsigned int) len)
		{
			if(num < 0 && value.Len() == (unsigned int) (len-1))
				value.Insert(0, "-");
			else
				value.Insert(0, "0");
		}
	}
	if(SBarInfoScript->spacingCharacter == '\0')
		x -= int(drawingFont->StringWidth(value)+(spacing * value.Len()));
	else //monospaced, so just multiplay the character size
		x -= int((drawingFont->GetCharWidth((int) SBarInfoScript->spacingCharacter) + spacing) * value.Len());
	DrawString(value, x, y, xOffset, yOffset, alpha, translation, spacing);
}

//draws the mug shot
void DSBarInfo::DrawFace(const char *defaultFace, int accuracy, bool xdth, bool animatedgodmode, int x, int y, int xOffset, int yOffset, int alpha)
{
	FTexture *face = MugShot.GetFace(CPlayer, defaultFace, accuracy, xdth, animatedgodmode);
	if (face != NULL)
	{
		DrawGraphic(face, x, y, xOffset, yOffset, alpha);
	}
}

void DSBarInfo::DrawInventoryBar(int type, int num, int x, int y, int xOffset, int yOffset, int alpha, bool alwaysshow,
	int counterx, int countery, EColorRange translation, bool drawArtiboxes, bool noArrows, bool alwaysshowcounter)
{ //yes, there is some Copy & Paste here too
	AInventory *item;
	int i;
	int spacing = (type != GAME_Strife) ? 31 : 35;

	// If the player has no artifacts, don't draw the bar
	CPlayer->mo->InvFirst = ValidateInvFirst(num);
	if(CPlayer->mo->InvFirst != NULL || alwaysshow)
	{
		for(item = CPlayer->mo->InvFirst, i = 0; item != NULL && i < num; item = item->NextInv(), ++i)
		{
			if(drawArtiboxes)
			{
				DrawGraphic(Images[invBarOffset + imgARTIBOX], x+i*spacing, y, xOffset, yOffset, alpha);
			}
			if(type != GAME_Strife) //Strife draws the cursor before the icons
				DrawGraphic(TexMan(item->Icon), x+i*spacing, y, xOffset, yOffset, alpha, false, item->Amount <= 0);
			if(item == CPlayer->mo->InvSel)
			{
				if(type == GAME_Heretic)
				{
					DrawGraphic(Images[invBarOffset + imgSELECTBOX], x+i*31, y+29, xOffset, yOffset, alpha);
				}
				else if(type == GAME_Hexen)
				{
					DrawGraphic(Images[invBarOffset + imgSELECTBOX], x+i*31, y-1, xOffset, yOffset, alpha);
				}
				else if(type == GAME_Strife)
				{
					DrawGraphic(Images[invBarOffset + imgCURSOR], x+i*35-6, y-2, xOffset, yOffset, alpha);
				}
				else
				{
					DrawGraphic(Images[invBarOffset + imgSELECTBOX], x+i*31, y, xOffset, yOffset, alpha);
				}
			}
			if(type == GAME_Strife)
				DrawGraphic(TexMan(item->Icon), x+i*spacing, y, xOffset, yOffset, alpha, false, item->Amount <= 0);
			if(alwaysshowcounter || item->Amount != 1)
			{
				DrawNumber(item->Amount, 3, counterx+i*spacing, countery, xOffset, yOffset, alpha, translation);
			}
		}
		for (; i < num && drawArtiboxes; ++i)
		{
			DrawGraphic(Images[invBarOffset + imgARTIBOX], x+i*spacing, y, xOffset, yOffset, alpha);
		}
		// Is there something to the left?
		if (!noArrows && CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
		{
			DrawGraphic(Images[!(gametic & 4) ?
				invBarOffset + imgINVLFGEM1 : invBarOffset + imgINVLFGEM2], (type != GAME_Strife) ? x-12 : x-14, y, xOffset, yOffset, alpha);
		}
		// Is there something to the right?
		if (!noArrows && item != NULL)
		{
			DrawGraphic(Images[!(gametic & 4) ?
				invBarOffset + imgINVRTGEM1 : invBarOffset + imgINVRTGEM2], (type != GAME_Strife) ? x+num*31+2 : x+num*35-4, y, xOffset, yOffset, alpha);
		}
	}
}

//draws heretic/hexen style life gems
void DSBarInfo::DrawGem(FTexture* chain, FTexture* gem, int value, int x, int y, int xOffset, int yOffset, int alpha, int padleft, int padright, int chainsize,
	bool wiggle, bool translate)
{
	if(chain == NULL)
		return;
	if(value > 100)
		value = 100;
	else if(value < 0)
		value = 0;
	if(wiggle)
		y += chainWiggle;
	int chainWidth = chain->GetWidth();
	int offset = (int) (((double) (chainWidth-padleft-padright)/100)*value);
	DrawGraphic(chain, x+(offset%chainsize), y, xOffset, yOffset, alpha);
	if(gem != NULL)
		DrawGraphic(gem, x+padleft+offset, y, xOffset, yOffset, alpha, translate);
}

FRemapTable* DSBarInfo::getTranslation()
{
	if(gameinfo.gametype & GAME_Raven)
		return translationtables[TRANSLATION_PlayersExtra][int(CPlayer - players)];
	return translationtables[TRANSLATION_Players][int(CPlayer - players)];
}

IMPLEMENT_CLASS(DSBarInfo);

DBaseStatusBar *CreateCustomStatusBar ()
{
	return new DSBarInfo;
}
