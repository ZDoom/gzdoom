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
#include "g_level.h"
#include "v_palette.h"
#include "p_acs.h"

#define ADJUST_RELCENTER(x, y, outX, outY) \
	if(x.RelCenter()) \
		outX = *x + SCREENWIDTH/(hud_scale ? CleanXfac*2 : 2); \
	else \
		outX = *x; \
	if(y.RelCenter()) \
		outY = *y + SCREENHEIGHT/(hud_scale ? CleanYfac*2 : 2); \
	else \
		outY = *y;

#define ARTIFLASH_OFFSET (statusBar->invBarOffset+6)
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

EXTERN_CVAR(Int, fraglimit)
EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Bool, vid_fps)
EXTERN_CVAR(Bool, hud_scale)

class DSBarInfo;

////////////////////////////////////////////////////////////////////////////////

/**
 * This class is used to help prevent errors that may occur from adding or 
 * subtracting from coordinates.
 *
 * In order to provide the maximum flexibility, coordinates are packed into
 * an int with one bit reserved for relCenter.
 */
class SBarInfoCoordinate
{
	public:
		SBarInfoCoordinate	&Add(int add)
		{
			value += add;
			return *this;
		}
		int					Coordinate() const { return value; }
		bool				RelCenter() const { return relCenter; }
		void				Set(int coord, bool center) { value = coord; relCenter = center; }
		void				SetCoord(int coord) { value = coord; }
		void				SetRelCenter(bool center) { relCenter = center; }

		int					operator*	() const { return Coordinate(); }
		SBarInfoCoordinate	operator+	(int add) const { return SBarInfoCoordinate(*this).Add(add); }
		SBarInfoCoordinate	operator+	(const SBarInfoCoordinate &other) const { return SBarInfoCoordinate(*this).Add(other.Coordinate()); }
		SBarInfoCoordinate	operator-	(int sub) const { return SBarInfoCoordinate(*this).Add(-sub); }
		SBarInfoCoordinate	operator-	(const SBarInfoCoordinate &other) const { return SBarInfoCoordinate(*this).Add(-other.Coordinate()); }
		void				operator+=	(int add) { Add(add); }
		void				operator-=	(int sub) { Add(-sub); }

	protected:
		unsigned relCenter:1;
		int		 value:31;
};

class SBarInfoMainBlock;

////////////////////////////////////////////////////////////////////////////////
/* There are three major classes here.  The SBarInfoCommand is our root class.
 * SBarInfoCommandFlowControl would be the base class for command which
 * implements a sub-block.  And SBarInfoMainBlock which is the root for a
 * single hud (so all commands are held inside a MainBlock at some point).
 *
 * A MainBlock can be passed NULL for the first argument of the Draw function.
 */

class SBarInfoCommand
{
	public:
		enum Offset
		{
			TOP = 0x1,
			VMIDDLE = 0x2,
			BOTTOM = 0x4,

			LEFT = 0x10,
			RIGHT = 0x20,
			HMIDDLE = 0x40,

			CENTER = VMIDDLE|HMIDDLE,
			CENTER_BOTTOM = BOTTOM|HMIDDLE
		};

		SBarInfoCommand(SBarInfo *script) : script(script) {}
		virtual ~SBarInfoCommand() {}

		virtual void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)=0;
		virtual void	Parse(FScanner &sc, bool fullScreenOffsets)=0;
		virtual void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged) {}

	protected:
		void		GetCoordinates(FScanner &sc, bool fullScreenOffsets, SBarInfoCoordinate &x, SBarInfoCoordinate &y)
		{
			bool negative = false;
			bool relCenter = false;
			SBarInfoCoordinate *coords[2] = {&x, &y};
			for(int i = 0;i < 2;i++)
			{
				negative = false;
				relCenter = false;
				if(i > 0)
					sc.MustGetToken(',');
			
				// [-]INT center
				negative = sc.CheckToken('-');
				sc.MustGetToken(TK_IntConst);
				coords[i]->Set(negative ? -sc.Number : sc.Number, false);
				if(sc.CheckToken('+'))
				{
					sc.MustGetToken(TK_Identifier);
					if(!sc.Compare("center"))
						sc.ScriptError("Expected 'center' but got '%s' instead.", sc.String);
					relCenter = true;
				}
				if(fullScreenOffsets)
				{
					coords[i]->SetRelCenter(relCenter);
				}
			}

			if(!fullScreenOffsets)
				y.SetCoord((negative ? -sc.Number : sc.Number) - (200 - script->height));
		}
		EColorRange	GetTranslation(FScanner &sc)
		{
			sc.MustGetToken(TK_Identifier);
			EColorRange returnVal = CR_UNTRANSLATED;
			FString namedTranslation; //we must send in "[translation]"
			const BYTE *trans_ptr;
			namedTranslation.Format("[%s]", sc.String);
			trans_ptr = (const BYTE *)(&namedTranslation[0]);
			if((returnVal = V_ParseFontColor(trans_ptr, CR_UNTRANSLATED, CR_UNTRANSLATED)) == CR_UNDEFINED)
			{
				sc.ScriptError("Missing definition for color %s.", sc.String);
			}
			return returnVal;
		}

		SBarInfo	*script;
};

class SBarInfoCommandFlowControl : public SBarInfoCommand
{
	public:
		SBarInfoCommandFlowControl(SBarInfo *script) : SBarInfoCommand(script) {}
		~SBarInfoCommandFlowControl()
		{
			for(unsigned int i = 0;i < commands.Size();i++)
				delete commands[i];
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			for(unsigned int i = 0;i < commands.Size();i++)
				commands[i]->Draw(block, statusBar);
		}
		int		NumCommands() const { return commands.Size(); }
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			sc.MustGetToken('{');
			SBarInfoCommand *cmd = NULL;
			while((cmd = NextCommand(sc)) != NULL)
			{
				cmd->Parse(sc, fullScreenOffsets);
				commands.Push(cmd);
			}
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			for(unsigned int i = 0;i < commands.Size();i++)
				commands[i]->Tick(block, statusBar, hudChanged);
		}

	private:
		SBarInfoCommand	*NextCommand(FScanner &sc);

		TArray<SBarInfoCommand *>	commands;
};

class SBarInfoMainBlock : public SBarInfoCommandFlowControl
{
	public:
		SBarInfoMainBlock(SBarInfo *script) : SBarInfoCommandFlowControl(script),
			alpha(FRACUNIT), forceScaled(false), fullScreenOffsets(false)
		{
		}

		int		Alpha() const { return alpha; }
		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, int xOffset, int yOffset, int alpha)
		{
			this->xOffset = xOffset;
			this->yOffset = yOffset;
			this->alpha = alpha;
			SBarInfoCommandFlowControl::Draw(this, statusBar);
		}
		bool	ForceScaled() const { return forceScaled; }
		bool	FullScreenOffsets() const { return fullScreenOffsets; }
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			this->fullScreenOffsets = fullScreenOffsets;
			if(sc.CheckToken(','))
			{
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("forcescaled"))
						forceScaled = true;
					else if(sc.Compare("fullscreenoffsets"))
						this->fullScreenOffsets = true;
					else
						sc.ScriptError("Unkown flag '%s'.", sc.String);
					if(!sc.CheckToken('|') && !sc.CheckToken(','))
					{
						SBarInfoCommandFlowControl::Parse(sc, this->fullScreenOffsets);
						return;
					}
				}
				sc.MustGetToken(TK_FloatConst);
				alpha = fixed_t(FRACUNIT * sc.Float);
			}
			SBarInfoCommandFlowControl::Parse(sc, this->fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged) { SBarInfoCommandFlowControl::Tick(this, statusBar, hudChanged); }
		int		XOffset() const { return xOffset; }
		int		YOffset() const { return yOffset; }

	private:
		int		alpha;
		bool	forceScaled;
		bool	fullScreenOffsets;
		int		xOffset;
		int		yOffset;
};

////////////////////////////////////////////////////////////////////////////////

SBarInfo *SBarInfoScript[2] = {NULL,NULL};

enum //Key words
{
	SBARINFO_BASE,
	SBARINFO_HEIGHT,
	SBARINFO_INTERPOLATEHEALTH,
	SBARINFO_INTERPOLATEARMOR,
	SBARINFO_COMPLETEBORDER,
	SBARINFO_MONOSPACEFONTS,
	SBARINFO_LOWERHEALTHCAP,
	SBARINFO_STATUSBAR,
	SBARINFO_MUGSHOT,
	SBARINFO_CREATEPOPUP,
};

enum //Bar types
{
	STBAR_NONE,
	STBAR_FULLSCREEN,
	STBAR_NORMAL,
	STBAR_AUTOMAP,
	STBAR_INVENTORY,
	STBAR_INVENTORYFULLSCREEN,
	STBAR_POPUPLOG,
	STBAR_POPUPKEYS,
	STBAR_POPUPSTATUS,
};

static const char *SBarInfoTopLevel[] =
{
	"base",
	"height",
	"interpolatehealth",
	"interpolatearmor",
	"completeborder",
	"monospacefonts",
	"lowerhealthcap",
	"statusbar",
	"mugshot",
	"createpopup",
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
	"popuplog",
	"popupkeys",
	"popupstatus",
	NULL
};

static void FreeSBarInfoScript()
{
	for(int i = 0;i < 2;i++)
	{
		if (SBarInfoScript[i] != NULL)
		{
			delete SBarInfoScript[i];
			SBarInfoScript[i] = NULL;
		}
	}
}

void SBarInfo::Load()
{
	if(gameinfo.statusbar.IsNotEmpty())
	{
		int lump = Wads.CheckNumForFullName(gameinfo.statusbar, true);
		if(lump != -1)
		{
			Printf ("ParseSBarInfo: Loading default status bar definition.\n");
			if(SBarInfoScript[SCRIPT_DEFAULT] == NULL)
				SBarInfoScript[SCRIPT_DEFAULT] = new SBarInfo(lump);
			else
				SBarInfoScript[SCRIPT_DEFAULT]->ParseSBarInfo(lump);
		}
	}

	if(Wads.CheckNumForName("SBARINFO") != -1)
	{
		Printf ("ParseSBarInfo: Loading custom status bar definition.\n");
		int lastlump, lump;
		lastlump = 0;
		while((lump = Wads.FindLump("SBARINFO", &lastlump)) != -1)
		{
			if(SBarInfoScript[SCRIPT_CUSTOM] == NULL)
				SBarInfoScript[SCRIPT_CUSTOM] = new SBarInfo(lump);
			else //We now have to load multiple SBarInfo Lumps so the 2nd time we need to use this method instead.
				SBarInfoScript[SCRIPT_CUSTOM]->ParseSBarInfo(lump);
		}
	}
	atterm(FreeSBarInfoScript);
}

//SBarInfo Script Reader
void SBarInfo::ParseSBarInfo(int lump)
{
	gameType = gameinfo.gametype;
	bool baseSet = false;
	FScanner sc(lump);
	sc.SetCMode(true);
	while(sc.CheckToken(TK_Identifier) || sc.CheckToken(TK_Include))
	{
		if(sc.TokenType == TK_Include)
		{
			sc.MustGetToken(TK_StringConst);
			int lump = Wads.CheckNumForFullName(sc.String, true);
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
				{
					int lump = Wads.CheckNumForFullName("sbarinfo/doom.txt", true);
					if(lump == -1)
						sc.ScriptError("Standard Doom Status Bar not found.");
					ParseSBarInfo(lump);
				}
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
			case SBARINFO_MONOSPACEFONTS:
				if(sc.CheckToken(TK_True))
				{
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					spacingCharacter = sc.String[0];
				}
				else
				{
					sc.MustGetToken(TK_False);
					spacingCharacter = '\0';
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst); //Don't tell anyone we're just ignoring this ;)
				}
				sc.MustGetToken(';');
				break;
			case SBARINFO_LOWERHEALTHCAP:
				if(sc.CheckToken(TK_False))
				{
					lowerHealthCap = false;
				}
				else
				{
					sc.MustGetToken(TK_True);
					lowerHealthCap = true;
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
				this->huds[barNum] = new SBarInfoMainBlock(this);
				if(barNum == STBAR_AUTOMAP)
				{
					automapbar = true;
				}
				this->huds[barNum]->Parse(sc, false);
				break;
			}
			case SBARINFO_MUGSHOT:
			{
				sc.MustGetToken(TK_StringConst);
				FMugShotState state(sc.String);
				if(sc.CheckToken(',')) //first loop must be a comma
				{
					do
					{
						sc.MustGetToken(TK_Identifier);
						if(sc.Compare("health"))
							state.bUsesLevels = true;
						else if(sc.Compare("health2"))
							state.bUsesLevels = state.bHealth2 = true;
						else if(sc.Compare("healthspecial"))
							state.bUsesLevels = state.bHealthSpecial = true;
						else if(sc.Compare("directional"))
							state.bDirectional = true;
						else
							sc.ScriptError("Unknown MugShot state flag '%s'.", sc.String);
					}
					while(sc.CheckToken(',') || sc.CheckToken('|'));
				}
				ParseMugShotBlock(sc, state);
				int index;
				if((index = FindMugShotStateIndex(state.State)) != -1) //We already had this state, remove the old one.
				{
					MugShotStates.Delete(index);
				}
				MugShotStates.Push(state);
				break;
			}
			case SBARINFO_CREATEPOPUP:
			{
				int pop = 0;
				sc.MustGetToken(TK_Identifier);
				if(sc.Compare("log"))
					pop = 0;
				else if(sc.Compare("keys"))
					pop = 1;
				else if(sc.Compare("status"))
					pop = 2;
				else
					sc.ScriptError("Unkown popup: '%s'", sc.String);
				Popup &popup = popups[pop];
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				popup.width = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				popup.height = sc.Number;
				sc.MustGetToken(',');
				if(!sc.CheckToken(TK_None))
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("slideinbottom"))
					{
						popup.transition = Popup::TRANSITION_SLIDEINBOTTOM;
						sc.MustGetToken(',');
						sc.MustGetToken(TK_IntConst);
						popup.speed = sc.Number;
					}
					else if(sc.Compare("fade"))
					{
						popup.transition = Popup::TRANSITION_FADE;
						sc.MustGetToken(',');
						sc.MustGetToken(TK_FloatConst);
						popup.speed = fixed_t(FRACUNIT * (1.0 / (35.0 * sc.Float)));
						sc.MustGetToken(',');
						sc.MustGetToken(TK_FloatConst);
						popup.speed2 = fixed_t(FRACUNIT * (1.0 / (35.0 * sc.Float)));
					}
					else
						sc.ScriptError("Unkown transition type: '%s'", sc.String);
				}
				popup.init();
				sc.MustGetToken(';');
				break;
			}
		}
	}
}

void SBarInfo::ParseMugShotBlock(FScanner &sc, FMugShotState &state)
{
	sc.MustGetToken('{');
	while(!sc.CheckToken('}'))
	{
		FMugShotFrame frame;
		bool multiframe = false;
		if(sc.CheckToken('{'))
			multiframe = true;
		do
		{
			sc.MustGetToken(TK_Identifier);
			if(strlen(sc.String) > 5)
				sc.ScriptError("MugShot frames cannot exceed 5 characters.");
			frame.Graphic.Push(sc.String);
		}
		while(multiframe && sc.CheckToken(','));
		if(multiframe)
			sc.MustGetToken('}');
		bool negative = sc.CheckToken('-');
		sc.MustGetToken(TK_IntConst);
		frame.Delay = (negative ? -1 : 1)*sc.Number;
		sc.MustGetToken(';');
		state.Frames.Push(frame);
	}
}

int SBarInfo::newImage(const char *patchname)
{
	if(patchname[0] == '\0' || stricmp(patchname, "nullimage") == 0)
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
	lowerHealthCap = true;
	interpolationSpeed = 8;
	armorInterpolationSpeed = 8;
	height = 0;
	spacingCharacter = '\0';

	for(unsigned int i = 0;i < NUMHUDS;i++)
		huds[i] = new SBarInfoMainBlock(this);
}

SBarInfo::~SBarInfo()
{
	for (size_t i = 0; i < NUMHUDS; ++i)
	{
		delete huds[i];
//		huds[i].commands.Clear();
	}
}

//Popup
Popup::Popup()
{
	transition = TRANSITION_NONE;
	height = 320;
	width = 200;
	speed = 0;
	x = 320;
	y = 200;
	alpha = FRACUNIT;
	opened = false;
	moving = false;
}

void Popup::init()
{
	x = width;
	y = height;
	if(transition == TRANSITION_SLIDEINBOTTOM)
	{
		x = 0;
	}
	else if(transition == TRANSITION_FADE)
	{
		alpha = 0;
		x = 0;
		y = 0;
	}
}

void Popup::tick()
{
	if(transition == TRANSITION_SLIDEINBOTTOM)
	{
		if(moving)
		{
			if(opened)
				y -= clamp(height + (y - height), 1, speed);
			else
				y += clamp(height - y, 1, speed);
		}
		if(y != 0 && y != height)
			moving = true;
		else
			moving = false;
	}
	else if(transition == TRANSITION_FADE)
	{
		if(moving)
		{
			if(opened)
				alpha = clamp(alpha + speed, 0, FRACUNIT);
			else
				alpha = clamp(alpha - speed2, 0, FRACUNIT);
		}
		if(alpha == 0 || alpha == FRACUNIT)
			moving = false;
		else
			moving = true;
	}
	else
	{
		if(opened)
		{
			y = 0;
			x = 0;
		}
		else
		{
			y = height;
			x = width;
		}
		moving = false;
	}
}

bool Popup::isDoneMoving()
{
	return !moving;
}

int Popup::getXOffset()
{
	return x;
}

int Popup::getYOffset()
{
	return y;
}

int Popup::getAlpha(int maxAlpha)
{
	double a = (double) alpha / (double) FRACUNIT;
	double b = (double) maxAlpha / (double) FRACUNIT;
	return fixed_t((a * b) * FRACUNIT);
}

void Popup::open()
{
	opened = true;
	moving = true;
}

void Popup::close()
{
	opened = false;
	moving = true;
}

////////////////////////////////////////////////////////////////////////////////

class DSBarInfo : public DBaseStatusBar
{
	DECLARE_CLASS(DSBarInfo, DBaseStatusBar)
public:
	DSBarInfo (SBarInfo *script=NULL) : DBaseStatusBar(script->height),
		ammo1(NULL), ammo2(NULL), ammocount1(0), ammocount2(0), armor(NULL),
		pendingPopup(POP_None), currentPopup(POP_None), lastHud(0),
		lastInventoryBar(NULL), lastPopup(NULL)
	{
		this->script = script;

		static const char *InventoryBarLumps[] =
		{
			"ARTIBOX",	"SELECTBO", "INVCURS",	"INVGEML1",
			"INVGEML2",	"INVGEMR1",	"INVGEMR2",
			"USEARTIA", "USEARTIB", "USEARTIC", "USEARTID",
		};
		TArray<const char *> patchnames;
		patchnames.Resize(script->Images.Size()+10);
		unsigned int i = 0;
		for(i = 0;i < script->Images.Size();i++)
		{
			patchnames[i] = script->Images[i];
		}
		for(i = 0;i < 10;i++)
		{
			patchnames[i+script->Images.Size()] = InventoryBarLumps[i];
		}
		for (i = 0;i < numskins;i++)
		{
			AddFaceToImageCollection (&skins[i], &Images);
		}
		invBarOffset = script->Images.Size();
		Images.Init(&patchnames[0], patchnames.Size());
	}

	~DSBarInfo ()
	{
		Images.Uninit();
	}

	void Draw (EHudState state)
	{
		DBaseStatusBar::Draw(state);
		int hud = STBAR_NORMAL;
		if(state == HUD_StatusBar)
		{
			if(script->completeBorder) //Fill the statusbar with the border before we draw.
			{
				FTexture *b = TexMan[gameinfo.border->b];
				R_DrawBorder(viewwindowx, viewwindowy + viewheight + b->GetHeight(), viewwindowx + viewwidth, SCREENHEIGHT);
				if(screenblocks == 10)
					screen->FlatFill(viewwindowx, viewwindowy + viewheight, viewwindowx + viewwidth, viewwindowy + viewheight + b->GetHeight(), b, true);
			}
			if(script->automapbar && automapactive)
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
		bool oldhud_scale = hud_scale;
		if(script->huds[hud]->ForceScaled()) //scale the statusbar
		{
			SetScaled(true, true);
			setsizeneeded = true;
			if(script->huds[hud]->FullScreenOffsets())
				hud_scale = true;
		}

		//prepare ammo counts
		GetCurrentAmmo(ammo1, ammo2, ammocount1, ammocount2);
		armor = CPlayer->mo->FindInventory<ABasicArmor>();
		if(hud != lastHud)
			script->huds[hud]->Tick(NULL, this, true);
		script->huds[hud]->Draw(NULL, this, 0, 0, FRACUNIT);
		lastHud = hud;
		if(CPlayer->inventorytics > 0 && !(level.flags & LEVEL_NOINVENTORYBAR) && (state == HUD_StatusBar || state == HUD_Fullscreen))
		{
			SBarInfoMainBlock *inventoryBar = state == HUD_StatusBar ? script->huds[STBAR_INVENTORY] : script->huds[STBAR_INVENTORYFULLSCREEN];
			if(inventoryBar != lastInventoryBar)
				inventoryBar->Tick(NULL, this, true);
	
			// No overlay?  Lets cancel it.
			if(inventoryBar->NumCommands() == 0)
				CPlayer->inventorytics = 0;
			else
				inventoryBar->Draw(NULL, this, 0, 0, FRACUNIT);
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
			if(script->huds[popbar] != lastPopup)
			{
				lastPopup = script->huds[popbar];
				lastPopup->Tick(NULL, this, true);
			}

			script->huds[popbar]->Draw(NULL, this, script->popups[currentPopup-1].getXOffset(), script->popups[currentPopup-1].getYOffset(), script->popups[currentPopup-1].getAlpha());
		}
		else
			lastPopup = NULL;
		if(script->huds[hud]->ForceScaled() && script->huds[hud]->FullScreenOffsets())
			hud_scale = oldhud_scale;
	}

	void NewGame ()
	{
		if (CPlayer != NULL)
		{
			AttachToPlayer (CPlayer);
		}
	}

	void SetMugShotState (const char *state_name, bool wait_till_done, bool reset)
	{
		script->MugShot.SetState(state_name, wait_till_done, reset);
	}

	void Tick ()
	{
		DBaseStatusBar::Tick();

		script->MugShot.Tick(CPlayer);
		if(currentPopup != POP_None)
		{
			script->popups[currentPopup-1].tick();
			if(script->popups[currentPopup-1].opened == false && script->popups[currentPopup-1].isDoneMoving())
			{
				currentPopup = pendingPopup;
				if(currentPopup != POP_None)
					script->popups[currentPopup-1].open();
			}

			if (lastPopup != NULL) lastPopup->Tick(NULL, this, false);
		}

		script->huds[lastHud]->Tick(NULL, this, false);
		if(lastInventoryBar != NULL && CPlayer->inventorytics > 0)
			lastInventoryBar->Tick(NULL, this, false);
	}

	void ReceivedWeapon(AWeapon *weapon)
	{
		script->MugShot.Grin();
	}

	// void DSBarInfo::FlashItem(const PClass *itemtype) - Is defined with CommandDrawSelectedInventory
	void FlashItem(const PClass *itemtype);

	void ShowPop(int popnum)
	{
		DBaseStatusBar::ShowPop(popnum);
		if(popnum != currentPopup)
		{
			pendingPopup = popnum;
		}
		else
			pendingPopup = POP_None;
		if(currentPopup != POP_None)
			script->popups[currentPopup-1].close();
		else
		{
			currentPopup = pendingPopup;
			pendingPopup = POP_None;
			if(currentPopup != POP_None)
				script->popups[currentPopup-1].open();
		}
	}

	//draws an image with the specified flags
	void DrawGraphic(FTexture* texture, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, int alpha, bool fullScreenOffsets, bool translate=false, bool dim=false, int offsetflags=0, bool alphaMap=false, int forceWidth=-1, int forceHeight=-1, fixed_t cx=0, fixed_t cy=0, fixed_t cr=0, fixed_t cb=0, bool clearDontDraw=false) const
	{
		if (texture == NULL)
			return;

		if((offsetflags & SBarInfoCommand::CENTER) == SBarInfoCommand::CENTER)
		{
			x -= (texture->GetScaledWidth()/2)-texture->LeftOffset;
			y -= (texture->GetScaledHeight()/2)-texture->TopOffset;
		}

		x += xOffset;
		y += yOffset;
		int w, h;
		if(!fullScreenOffsets)
		{
			fixed_t tmp = 0;
			// I'll handle the conversion from fixed to int myself for more control
			fixed_t fx = (x + ST_X).Coordinate() << FRACBITS;
			fixed_t fy = (y + ST_Y).Coordinate() << FRACBITS;
			fixed_t fw = (forceWidth <= -1 ? texture->GetScaledWidth() : forceWidth) << FRACBITS;
			fixed_t fh = (forceHeight <= -1 ? texture->GetScaledHeight() : forceHeight) << FRACBITS;
			fixed_t fcx = cx == 0 ? 0 : fx + cx - (texture->GetScaledLeftOffset() << FRACBITS);
			fixed_t fcy = cy == 0 ? 0 : fy + cy - (texture->GetScaledTopOffset() << FRACBITS);
			fixed_t fcr = fx + fw - cr;
			fixed_t fcb = fy + fh - cb;
			if(Scaled)
			{
				if(cx != 0 || cy != 0)
					screen->VirtualToRealCoordsFixed(fcx, fcy, tmp, tmp, 320, 200, true);
				if(cr != 0 || cb != 0 || clearDontDraw)
					screen->VirtualToRealCoordsFixed(fcr, fcb, tmp, tmp, 320, 200, true);
				screen->VirtualToRealCoordsFixed(fx, fy, fw, fh, 320, 200, true);
			}
			// Round to nearest
			w = (fw + (FRACUNIT>>1)) >> FRACBITS;
			h = (fh + (FRACUNIT>>1)) >> FRACBITS;
			cr = cr != 0 ? fcr >> FRACBITS : INT_MAX;
			cb = cb != 0 ? fcb >> FRACBITS : INT_MAX;
			if(clearDontDraw)
				screen->Clear(MAX<fixed_t>(fx, fcx)>>FRACBITS, MAX<fixed_t>(fy, fcy)>>FRACBITS, fcr>>FRACBITS, fcb>>FRACBITS, GPalette.BlackIndex, 0);
			else
			{
				if(alphaMap)
				{
					screen->DrawTexture(texture, (fx >> FRACBITS), (fy >> FRACBITS),
						DTA_DestWidth, w,
						DTA_DestHeight, h,
						DTA_ClipLeft, fcx>>FRACBITS,
						DTA_ClipTop, fcy>>FRACBITS,
						DTA_ClipRight, cr,
						DTA_ClipBottom, cb,
						DTA_Translation, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_Alpha, alpha,
						DTA_AlphaChannel, alphaMap,
						DTA_FillColor, 0,
						TAG_DONE);
				}
				else
				{
					screen->DrawTexture(texture, (fx >> FRACBITS), (fy >> FRACBITS),
						DTA_DestWidth, w,
						DTA_DestHeight, h,
						DTA_ClipLeft, fcx>>FRACBITS,
						DTA_ClipTop, fcy>>FRACBITS,
						DTA_ClipRight, cr,
						DTA_ClipBottom, cb,
						DTA_Translation, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_Alpha, alpha,
						TAG_DONE);
				}
			}
		}
		else
		{
			int rx, ry, rcx=0, rcy=0, rcr=INT_MAX, rcb=INT_MAX;
			ADJUST_RELCENTER(x,y,rx,ry)

			w = (forceWidth <= -1 ? texture->GetScaledWidth() : forceWidth);
			h = (forceHeight <= -1 ? texture->GetScaledHeight() : forceHeight);
			if(vid_fps && rx < 0 && ry >= 0)
				ry += 10;

			// Check for clipping
			if(cx != 0 || cy != 0 || cr != 0 || cb != 0)
			{
				rcx = cx == 0 ? 0 : rx+(cx>>FRACBITS);
				rcy = cy == 0 ? 0 : ry+(cy>>FRACBITS);
				rcr = cr == 0 ? INT_MAX : rx+w-(cr>>FRACBITS);
				rcb = cb == 0 ? INT_MAX : ry+h-(cb>>FRACBITS);
				// Fix the clipping for fullscreenoffsets.
				if(ry < 0)
				{
					if(rcy != 0)
						rcy = hud_scale ? SCREENHEIGHT + (rcy*CleanYfac) : SCREENHEIGHT + rcy;
					if(rcb != INT_MAX)
						rcb = hud_scale ? SCREENHEIGHT + (rcb*CleanYfac) : SCREENHEIGHT + rcb;
				}
				else if(hud_scale)
				{
					rcy *= CleanYfac;
					if(rcb != INT_MAX)
						rcb *= CleanYfac;
				}
				if(rx < 0)
				{
					if(rcx != 0)
						rcx = hud_scale ? SCREENWIDTH + (rcx*CleanXfac) : SCREENWIDTH + rcx;
					if(rcr != INT_MAX)
						rcr = hud_scale ? SCREENWIDTH + (rcr*CleanXfac) : SCREENWIDTH + rcr;
				}
				else if(hud_scale)
				{
					rcx *= CleanXfac;
					if(rcr != INT_MAX)
						rcr *= CleanXfac;
				}
			}

			if(clearDontDraw)
			{
				screen->Clear(rcx, rcy, MIN<int>(rcr, w*(hud_scale ? CleanXfac : 1)), MIN<int>(rcb, h*(hud_scale ? CleanYfac : 1)), GPalette.BlackIndex, 0);
			}
			else
			{
				if(alphaMap)
				{
					screen->DrawTexture(texture, rx, ry,
						DTA_DestWidth, w,
						DTA_DestHeight, h,
						DTA_ClipLeft, rcx,
						DTA_ClipTop, rcy,
						DTA_ClipRight, rcr,
						DTA_ClipBottom, rcb,
						DTA_Translation, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_HUDRules, HUD_Normal,
						DTA_Alpha, alpha,
						DTA_AlphaChannel, alphaMap,
						DTA_FillColor, 0,
						TAG_DONE);
				}
				else
				{
					screen->DrawTexture(texture, rx, ry,
						DTA_DestWidth, w,
						DTA_DestHeight, h,
						DTA_ClipLeft, rcx,
						DTA_ClipTop, rcy,
						DTA_ClipRight, rcr,
						DTA_ClipBottom, rcb,
						DTA_Translation, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_HUDRules, HUD_Normal,
						DTA_Alpha, alpha,
						TAG_DONE);
				}
			}
		}
	}

	void DrawString(FFont *font, const char* str, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, int alpha, bool fullScreenOffsets, EColorRange translation, int spacing=0, bool drawshadow=false) const
	{
		x += spacing;
		int ax = *x;
		int ay = *y;
		if(fullScreenOffsets)
		{
			ADJUST_RELCENTER(x,y,ax,ay)
		}
		while(*str != '\0')
		{
			if(*str == ' ')
			{
				ax += font->GetSpaceWidth();
				str++;
				continue;
			}
			int width;
			if(script->spacingCharacter == '\0') //No monospace?
				width = font->GetCharWidth((int) *str);
			else
				width = font->GetCharWidth((int) script->spacingCharacter);
			FTexture* character = font->GetChar((int) *str, &width);
			if(character == NULL) //missing character.
			{
				str++;
				continue;
			}
			if(script->spacingCharacter == '\0') //If we are monospaced lets use the offset
				ax += (character->LeftOffset+1); //ignore x offsets since we adapt to character size

			int rx, ry, rw, rh;
			rx = ax + xOffset;
			ry = ay + yOffset;
			rw = character->GetScaledWidth();
			rh = character->GetScaledHeight();
			if(!fullScreenOffsets)
			{
				rx += ST_X;
				ry += ST_Y;
				if(Scaled)
					screen->VirtualToRealCoordsInt(rx, ry, rw, rh, 320, 200, true);
			}
			else
			{
				if(vid_fps && ax < 0 && ay >= 0)
					ry += 10;
			}
			if(drawshadow)
			{
				int salpha = fixed_t(((double) alpha / (double) FRACUNIT) * ((double) HR_SHADOW / (double) FRACUNIT) * FRACUNIT);
				if(!fullScreenOffsets)
				{
					screen->DrawTexture(character, rx+2, ry+2,
						DTA_DestWidth, rw,
						DTA_DestHeight, rh,
						DTA_Alpha, salpha,
						DTA_FillColor, 0,
						TAG_DONE);
				}
				else
				{
					screen->DrawTexture(character, rx+2, ry+2,
						DTA_DestWidth, rw,
						DTA_DestHeight, rh,
						DTA_Alpha, salpha,
						DTA_HUDRules, HUD_Normal,
						DTA_FillColor, 0,
						TAG_DONE);
				}
			}
			if(!fullScreenOffsets)
			{
				screen->DrawTexture(character, rx, ry,
					DTA_DestWidth, rw,
					DTA_DestHeight, rh,
					DTA_Translation, font->GetColorTranslation(translation),
					DTA_Alpha, alpha,
					TAG_DONE);
			}
			else
			{
				screen->DrawTexture(character, rx, ry,
					DTA_DestWidth, rw,
					DTA_DestHeight, rh,
					DTA_Translation, font->GetColorTranslation(translation),
					DTA_Alpha, alpha,
					DTA_HUDRules, HUD_Normal,
					TAG_DONE);
			}
			if(script->spacingCharacter == '\0')
				ax += width + spacing - (character->LeftOffset+1);
			else //width gets changed at the call to GetChar()
				ax += font->GetCharWidth((int) script->spacingCharacter) + spacing;
			str++;
		}
	}

	FRemapTable* GetTranslation() const
	{
		if(gameinfo.gametype & GAME_Raven)
			return translationtables[TRANSLATION_PlayersExtra][int(CPlayer - players)];
		return translationtables[TRANSLATION_Players][int(CPlayer - players)];
	}

	AAmmo *ammo1, *ammo2;
	int ammocount1, ammocount2;
	ABasicArmor *armor;
	FImageCollection Images;
	unsigned int invBarOffset;

private:
	SBarInfo *script;
	int pendingPopup;
	int currentPopup;
	int lastHud;
	SBarInfoMainBlock *lastInventoryBar;
	SBarInfoMainBlock *lastPopup;
};

IMPLEMENT_CLASS(DSBarInfo);

DBaseStatusBar *CreateCustomStatusBar (int script)
{
	if(SBarInfoScript[script] == NULL)
		I_FatalError("Tried to create a status bar with no script!");
	return new DSBarInfo(SBarInfoScript[script]);
}

#include "sbarinfo_commands.cpp"
