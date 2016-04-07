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
#include "m_swap.h"
#include "a_keys.h"
#include "templates.h"
#include "i_system.h"
#include "sbarinfo.h"
#include "gi.h"
#include "r_data/r_translate.h"
#include "a_weaponpiece.h"
#include "a_strifeglobal.h"
#include "g_level.h"
#include "v_palette.h"
#include "p_acs.h"
#include "gstrings.h"
#include "version.h"

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
static double nulclip[] = { 0,0,0,0 };

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
		virtual void	Reset() {}
		virtual void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged) {}

		SBarInfo *GetScript() { return script; }

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

			//if(!fullScreenOffsets)
			//	y.SetCoord((negative ? -sc.Number : sc.Number) - (200 - script->height));
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
		SBarInfoCommandFlowControl(SBarInfo *script) : SBarInfoCommand(script), truth(false) {}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			for(auto command : commands[truth])
				command->Draw(block, statusBar);
		}
		int		NumCommands() const { return commands[truth].Size(); }
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			ParseBlock(commands[1], sc, fullScreenOffsets);
			if(sc.CheckToken(TK_Else))
				ParseBlock(commands[0], sc, fullScreenOffsets);
		}
		void	Reset()
		{
			for(unsigned int i = 0;i < 2;i++)
			{
				for(auto command : commands[i])
					command->Reset();
			}
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			for(auto command : commands[truth])
				command->Tick(block, statusBar, hudChanged);
		}

	protected:
		void	SetTruth(bool truth, const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			// If there is no change we don't need to do anything.  Do note
			// that this should not change more than once per tick.  If it does
			// there may be cosmetic problems.
			if(this->truth == truth)
				return;

			this->truth = truth;
			if(block != NULL)
				Tick(block, statusBar, true);
		}

		void Negate()
		{
			swapvalues(commands[0], commands[1]);
		}

	private:
		void ParseBlock(TDeletingArray<SBarInfoCommand *> &commands, FScanner &sc, bool fullScreenOffsets)
		{
			if(sc.CheckToken('{'))
			{
				while(SBarInfoCommand *cmd = NextCommand(sc))
				{
					cmd->Parse(sc, fullScreenOffsets);
					commands.Push(cmd);
				}
			}
			else
			{
				if(SBarInfoCommand *cmd = NextCommand(sc))
				{
					cmd->Parse(sc, fullScreenOffsets);
					commands.Push(cmd);
				}
				else
					sc.ScriptError("Missing command for flow control statement.");
			}
		}

		SBarInfoCommand	*NextCommand(FScanner &sc);

		TDeletingArray<SBarInfoCommand *> commands[2];
		bool truth;
};

class SBarInfoNegatableFlowControl : public SBarInfoCommandFlowControl
{
	public:
		SBarInfoNegatableFlowControl(SBarInfo *script) : SBarInfoCommandFlowControl(script) {}

		void Parse(FScanner &sc, bool fullScreenOffsets)
		{
			bool negate = false;
			if(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("not"))
					negate = true;
				else
					sc.UnGet();
			}

			ParseNegatable(sc, fullScreenOffsets);

			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);

			if(negate)
				Negate();
		}

		virtual void ParseNegatable(FScanner &sc, bool fullScreenOffsets) {}
};

class SBarInfoMainBlock : public SBarInfoCommandFlowControl
{
	public:
		SBarInfoMainBlock(SBarInfo *script) : SBarInfoCommandFlowControl(script),
			alpha(1.), currentAlpha(1.), forceScaled(false),
			fullScreenOffsets(false)
		{
			SetTruth(true, NULL, NULL);
		}

		double	Alpha() const { return currentAlpha; }
		// Same as Draw but takes into account ForceScaled and temporarily sets the scaling if needed.
		void	DrawAux(const SBarInfoMainBlock *block, DSBarInfo *statusBar, int xOffset, int yOffset, double alpha);
		// Silence hidden overload warning since this is a special use class.
		using SBarInfoCommandFlowControl::Draw;
		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, int xOffset, int yOffset, double alpha)
		{
			this->xOffset = xOffset;
			this->yOffset = yOffset;
			this->currentAlpha = this->alpha * alpha;
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
				alpha = sc.Float;
			}
			SBarInfoCommandFlowControl::Parse(sc, this->fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged) { SBarInfoCommandFlowControl::Tick(this, statusBar, hudChanged); }
		int		XOffset() const { return xOffset; }
		int		YOffset() const { return yOffset; }

	protected:
		double	alpha;
		double	currentAlpha;
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
	SBARINFO_RESOLUTION,
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
	"resolution",
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
	FreeSBarInfoScript();
	MugShotStates.Clear();
	if(gameinfo.statusbar.IsNotEmpty())
	{
		int lump = Wads.CheckNumForFullName(gameinfo.statusbar, true);
		if(lump != -1)
		{
			if (!batchrun) Printf ("ParseSBarInfo: Loading default status bar definition.\n");
			if(SBarInfoScript[SCRIPT_DEFAULT] == NULL)
				SBarInfoScript[SCRIPT_DEFAULT] = new SBarInfo(lump);
			else
				SBarInfoScript[SCRIPT_DEFAULT]->ParseSBarInfo(lump);
		}
	}

	if(Wads.CheckNumForName("SBARINFO") != -1)
	{
		if (!batchrun) Printf ("ParseSBarInfo: Loading custom status bar definition.\n");
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
		int baselump = -2;
		switch(sc.MustMatchString(SBarInfoTopLevel))
		{
			case SBARINFO_BASE:
				baseSet = true;
				if(!sc.CheckToken(TK_None))
					sc.MustGetToken(TK_Identifier);
				if(sc.Compare("Doom"))
				{
					baselump = Wads.CheckNumForFullName("sbarinfo/doom.txt", true);
				}
				else if(sc.Compare("Heretic"))
				{
					baselump = Wads.CheckNumForFullName("sbarinfo/heretic.txt", true);
				}
				else if(sc.Compare("Hexen"))
				{
					baselump = Wads.CheckNumForFullName("sbarinfo/hexen.txt", true);
				}
				else if(sc.Compare("Strife"))
					gameType = GAME_Strife;
				else if(sc.Compare("None"))
					gameType = GAME_Any;
				else
					sc.ScriptError("Bad game name: %s", sc.String);
				// If one of the standard status bar should be loaded, baselump has been set to a different value.
				if (baselump != -2)
				{
					if(baselump == -1)
					{
						sc.ScriptError("Standard %s status bar not found.", sc.String);
					}
					else if (Wads.GetLumpFile(baselump) > 0)
					{
						I_FatalError("File %s is overriding core lump sbarinfo/%s.txt.",
							Wads.GetWadFullName(Wads.GetLumpFile(baselump)), sc.String);
					}
					ParseSBarInfo(baselump);
				}
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
				if(sc.CheckToken(','))
				{
					// Character alignment
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("left"))
						spacingAlignment = ALIGN_LEFT;
					else if(sc.Compare("center"))
						spacingAlignment = ALIGN_CENTER;
					else if(sc.Compare("right"))
						spacingAlignment = ALIGN_RIGHT;
					else
						sc.ScriptError("Unknown alignment '%s'.", sc.String);
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
			case SBARINFO_RESOLUTION:
				sc.MustGetToken(TK_IntConst);
				resW = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				resH = sc.Number;
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
				if (this->huds[barNum] != NULL)
				{
					delete this->huds[barNum];
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
						popup.ispeed = sc.Number;
					}
					else if(sc.Compare("pushup"))
					{
						popup.transition = Popup::TRANSITION_PUSHUP;
						sc.MustGetToken(',');
						sc.MustGetToken(TK_IntConst);
						popup.ispeed = sc.Number;
					}
					else if(sc.Compare("fade"))
					{
						popup.transition = Popup::TRANSITION_FADE;
						sc.MustGetToken(',');
						sc.MustGetToken(TK_FloatConst);
						popup.speed = 1.0 / (35.0 * sc.Float);
						sc.MustGetToken(',');
						sc.MustGetToken(TK_FloatConst);
						popup.speed2 = 1.0 / (35.0 * sc.Float);
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

void SBarInfo::ResetHuds()
{
	for(int i = 0;i < NUMHUDS;i++)
		huds[i]->Reset();
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
	spacingAlignment = ALIGN_CENTER;
	resW = 320;
	resH = 200;
	cleanX = -1;
	cleanY = -1;

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
Popup::Popup() : transition(TRANSITION_NONE), opened(false), moving(false),
	height(320), width(200), ispeed(0), speed(0), speed2(0), alpha(1.), x(320),
	y(200), displacementX(0), displacementY(0)
{
}

void Popup::init()
{
	x = width;
	y = height;
	switch(transition)
	{
		case TRANSITION_SLIDEINBOTTOM:
		case TRANSITION_PUSHUP:
			x = 0;
			break;
		case TRANSITION_FADE:
			alpha = 0;
			x = 0;
			y = 0;
			break;
		default:
			break;
	}
}

void Popup::tick()
{
	switch(transition)
	{
		case TRANSITION_SLIDEINBOTTOM:
		case TRANSITION_PUSHUP:
			if(moving)
			{
				int oldY = y;
				if(opened)
					y -= clamp(height + (y - height), 1, ispeed);
				else
					y += clamp(height - y, 1, ispeed);
				if(transition == TRANSITION_PUSHUP)
					displacementY += y - oldY;
			}
			if(y != 0 && y != height)
				moving = true;
			else
				moving = false;
			break;
		case TRANSITION_FADE:
			if(moving)
			{
				if(opened)
					alpha = clamp(alpha + speed, 0., 1.);
				else
					alpha = clamp(alpha - speed2, 0., 1.);
			}
			if(alpha == 0 || alpha == 1.)
				moving = false;
			else
				moving = true;
			break;
		default:
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
			break;
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

double Popup::getAlpha(double maxAlpha)
{
	return alpha * maxAlpha;
}

int Popup::getXDisplacement()
{
	return displacementX;
}

int Popup::getYDisplacement()
{
	return displacementY;
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

inline void adjustRelCenter(bool relX, bool relY, const double &x, const double &y, double &outX, double &outY, const double &xScale, const double &yScale)
{
	if(relX)
		outX = x + (SCREENWIDTH/(hud_scale ? xScale*2 : 2));
	else
		outX = x;
	if(relY)
		outY = y + (SCREENHEIGHT/(hud_scale ? yScale*2 : 2));
	else
		outY = y;
}

class DSBarInfo : public DBaseStatusBar
{
	DECLARE_CLASS(DSBarInfo, DBaseStatusBar)
	HAS_OBJECT_POINTERS
public:
	DSBarInfo (SBarInfo *script=NULL) : DBaseStatusBar(script->height, script->resW, script->resH),
		ammo1(NULL), ammo2(NULL), ammocount1(0), ammocount2(0), armor(NULL),
		pendingPopup(POP_None), currentPopup(POP_None), lastHud(-1),
		scalingWasForced(false), lastInventoryBar(NULL), lastPopup(NULL)
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
		invBarOffset = script->Images.Size();
		Images.Init(&patchnames[0], patchnames.Size());

		CompleteBorder = script->completeBorder;
	}

	~DSBarInfo ()
	{
		Images.Uninit();
	}

	void ScreenSizeChanged()
	{
		Super::ScreenSizeChanged();
		V_CalcCleanFacs(script->resW, script->resH, SCREENWIDTH, SCREENHEIGHT, &script->cleanX, &script->cleanY);
	}

	void Draw (EHudState state)
	{
		DBaseStatusBar::Draw(state);
		if (script->cleanX <= 0)
		{ // Calculate cleanX and cleanY
			ScreenSizeChanged();
		}
		int hud = STBAR_NORMAL;
		if(state == HUD_StatusBar)
		{
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
			if(script->huds[hud]->FullScreenOffsets())
				hud_scale = true;
			else if(!Scaled)
			{
				scalingWasForced = true;
				SetScaled(true, true);
				setsizeneeded = true;
			}
		}

		//prepare ammo counts
		GetCurrentAmmo(ammo1, ammo2, ammocount1, ammocount2);
		armor = CPlayer->mo->FindInventory<ABasicArmor>();

		if(state != HUD_AltHud)
		{
			if(hud != lastHud)
			{
				script->huds[hud]->Tick(NULL, this, true);

				// Restore scaling if need be.
				if(scalingWasForced)
				{
					scalingWasForced = false;
					SetScaled(false);
					setsizeneeded = true;
				}
			}

			if(currentPopup != POP_None && !script->huds[hud]->FullScreenOffsets())
				script->huds[hud]->Draw(NULL, this, script->popups[currentPopup-1].getXDisplacement(), script->popups[currentPopup-1].getYDisplacement(), 1.);
			else
				script->huds[hud]->Draw(NULL, this, 0, 0, 1.);
			lastHud = hud;

			// Handle inventory bar drawing
			if(CPlayer->inventorytics > 0 && !(level.flags & LEVEL_NOINVENTORYBAR) && (state == HUD_StatusBar || state == HUD_Fullscreen))
			{
				SBarInfoMainBlock *inventoryBar = state == HUD_StatusBar ? script->huds[STBAR_INVENTORY] : script->huds[STBAR_INVENTORYFULLSCREEN];
				if(inventoryBar != lastInventoryBar)
					inventoryBar->Tick(NULL, this, true);
		
				// No overlay?  Lets cancel it.
				if(inventoryBar->NumCommands() == 0)
					CPlayer->inventorytics = 0;
				else
					inventoryBar->DrawAux(NULL, this, 0, 0, 1.);
			}
		}

		// Handle popups
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

			script->huds[popbar]->DrawAux(NULL, this, script->popups[currentPopup-1].getXOffset(), script->popups[currentPopup-1].getYOffset(), script->popups[currentPopup-1].getAlpha());
		}
		else
			lastPopup = NULL;

		// Reset hud_scale
		hud_scale = oldhud_scale;
	}

	void NewGame ()
	{
		if (CPlayer != NULL)
		{
			AttachToPlayer (CPlayer);

			// Reset the huds
			script->ResetHuds();
			lastHud = -1; // Reset
		}
	}

	bool MustDrawLog (EHudState state)
	{
		return script->huds[STBAR_POPUPLOG]->NumCommands() == 0;
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

		if(lastHud != -1)
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
	void DrawGraphic(FTexture* texture, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, double Alpha, bool fullScreenOffsets, bool translate=false, bool dim=false, int offsetflags=0, bool alphaMap=false, int forceWidth=-1, int forceHeight=-1, const double *clip = nulclip, bool clearDontDraw=false) const
	{
		if (texture == NULL)
			return;

		double dx = *x;
		double dy = *y;

		if((offsetflags & SBarInfoCommand::CENTER) == SBarInfoCommand::CENTER)
		{
			if (forceWidth < 0)	dx -= (texture->GetScaledWidthDouble()/2.0)-texture->GetScaledLeftOffsetDouble();
			else	dx -= forceWidth*(0.5-(texture->GetScaledLeftOffsetDouble()/texture->GetScaledWidthDouble()));
			//Unoptimalized formula is dx -= forceWidth/2.0-(texture->GetScaledLeftOffsetDouble()*forceWidth/texture->GetScaledWidthDouble());
			
			if (forceHeight < 0)	dy -= (texture->GetScaledHeightDouble()/2.0)-texture->GetScaledTopOffsetDouble();
			else	dy -= forceHeight*(0.5-(texture->GetScaledTopOffsetDouble()/texture->GetScaledHeightDouble()));
		}

		dx += xOffset;
		dy += yOffset;
		double w, h;
		if(!fullScreenOffsets)
		{
			double tmp = 0;
			dx += ST_X;
			dy += ST_Y - (Scaled ? script->resH : 200) + script->height;
			w = forceWidth < 0 ? texture->GetScaledWidthDouble() : forceWidth;
			h = forceHeight < 0 ? texture->GetScaledHeightDouble() : forceHeight;
			double dcx = clip[0] == 0 ? 0 : dx + clip[0] - texture->GetScaledLeftOffsetDouble();
			double dcy = clip[1] == 0 ? 0 : dy + clip[1] - texture->GetScaledTopOffsetDouble();
			double dcr = clip[2] == 0 ? INT_MAX : dx + w - clip[2] - texture->GetScaledLeftOffsetDouble();
			double dcb = clip[3] == 0 ? INT_MAX : dy + h - clip[3] - texture->GetScaledTopOffsetDouble();

			if(Scaled)
			{
				if(clip[0] != 0 || clip[1] != 0)
				{
					screen->VirtualToRealCoords(dcx, dcy, tmp, tmp, script->resW, script->resH, true);
					if (clip[0] == 0) dcx = 0;
					if (clip[1] == 0) dcy = 0;
				}
				if(clip[2] != 0 || clip[3] != 0 || clearDontDraw)
					screen->VirtualToRealCoords(dcr, dcb, tmp, tmp, script->resW, script->resH, true);
				screen->VirtualToRealCoords(dx, dy, w, h, script->resW, script->resH, true);
			}
			else
			{
				dy += 200 - script->resH;
				dcy += 200 - script->resH;
				dcb += 200 - script->resH;
			}

			if(clearDontDraw)
				screen->Clear(static_cast<int>(MAX<double>(dx, dcx)), static_cast<int>(MAX<double>(dy, dcy)), static_cast<int>(MIN<double>(dcr,w+MAX<double>(dx, dcx))), static_cast<int>(MIN<double>(dcb,MAX<double>(dy, dcy)+h)), GPalette.BlackIndex, 0);
			else
			{
				if(alphaMap)
				{
					screen->DrawTexture(texture, dx, dy,
						DTA_DestWidthF, w,
						DTA_DestHeightF, h,
						DTA_ClipLeft, static_cast<int>(dcx),
						DTA_ClipTop, static_cast<int>(dcy),
						DTA_ClipRight, static_cast<int>(MIN<double>(INT_MAX, dcr)),
						DTA_ClipBottom, static_cast<int>(MIN<double>(INT_MAX, dcb)),
						DTA_Translation, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_AlphaF, Alpha,
						DTA_AlphaChannel, alphaMap,
						DTA_FillColor, 0,
						TAG_DONE);
				}
				else
				{
					screen->DrawTexture(texture, dx, dy,
						DTA_DestWidthF, w,
						DTA_DestHeightF, h,
						DTA_ClipLeft, static_cast<int>(dcx),
						DTA_ClipTop, static_cast<int>(dcy),
						DTA_ClipRight, static_cast<int>(MIN<double>(INT_MAX, dcr)),
						DTA_ClipBottom, static_cast<int>(MIN<double>(INT_MAX, dcb)),
						DTA_Translation, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_AlphaF, Alpha,
						TAG_DONE);
				}
			}
		}
		else
		{
			double rx, ry, rcx=0, rcy=0, rcr=INT_MAX, rcb=INT_MAX;

			double xScale = !hud_scale ? 1 : script->cleanX;
			double yScale = !hud_scale ? 1 : script->cleanY;

			adjustRelCenter(x.RelCenter(), y.RelCenter(), dx, dy, rx, ry, xScale, yScale);

			// We can't use DTA_HUDRules since it forces a width and height.
			// Translation: No high res.
			bool xright = *x < 0 && !x.RelCenter();
			bool ybot = *y < 0 && !y.RelCenter();

			w = (forceWidth < 0 ? texture->GetScaledWidthDouble() : forceWidth);
			h = (forceHeight < 0 ? texture->GetScaledHeightDouble() : forceHeight);
			if(vid_fps && rx < 0 && ry >= 0)
				ry += 10;
			if(hud_scale)
			{
				rx *= xScale;
				ry *= yScale;
				w *= xScale;
				h *= yScale;
			}
			if(xright)
				rx = SCREENWIDTH + rx;
			if(ybot)
				ry = SCREENHEIGHT + ry;

			// Check for clipping
			if(clip[0] != 0 || clip[1] != 0 || clip[2] != 0 || clip[3] != 0)
			{
				rcx = clip[0] == 0 ? 0 : rx+((clip[0] - texture->GetScaledLeftOffsetDouble())*xScale);
				rcy = clip[1] == 0 ? 0 : ry+((clip[1] - texture->GetScaledTopOffsetDouble())*yScale);
				rcr = clip[2] == 0 ? INT_MAX : rx+w-((clip[2] + texture->GetScaledLeftOffsetDouble())*xScale);
				rcb = clip[3] == 0 ? INT_MAX : ry+h-((clip[3] + texture->GetScaledTopOffsetDouble())*yScale);
			}

			if(clearDontDraw)
				screen->Clear(static_cast<int>(rcx), static_cast<int>(rcy), static_cast<int>(MIN<double>(rcr, rcx+w)), static_cast<int>(MIN<double>(rcb, rcy+h)), GPalette.BlackIndex, 0);
			else
			{
				if(alphaMap)
				{
					screen->DrawTexture(texture, rx, ry,
						DTA_DestWidthF, w,
						DTA_DestHeightF, h,
						DTA_ClipLeft, static_cast<int>(rcx),
						DTA_ClipTop, static_cast<int>(rcy),
						DTA_ClipRight, static_cast<int>(rcr),
						DTA_ClipBottom, static_cast<int>(rcb),
						DTA_Translation, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_AlphaF, Alpha,
						DTA_AlphaChannel, alphaMap,
						DTA_FillColor, 0,
						TAG_DONE);
				}
				else
				{
					screen->DrawTexture(texture, rx, ry,
						DTA_DestWidthF, w,
						DTA_DestHeightF, h,
						DTA_ClipLeft, static_cast<int>(rcx),
						DTA_ClipTop, static_cast<int>(rcy),
						DTA_ClipRight, static_cast<int>(rcr),
						DTA_ClipBottom, static_cast<int>(rcb),
						DTA_Translation, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_AlphaF, Alpha,
						TAG_DONE);
				}
			}
		}
	}

	void DrawString(FFont *font, const char* cstring, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, double Alpha, bool fullScreenOffsets, EColorRange translation, int spacing=0, bool drawshadow=false, int shadowX=2, int shadowY=2) const
	{
		x += spacing;
		double ax = *x;
		double ay = *y;

		double xScale = 1.0;
		double yScale = 1.0;

		const BYTE* str = (const BYTE*) cstring;
		const EColorRange boldTranslation = EColorRange(translation ? translation - 1 : NumTextColors - 1);
		FRemapTable *remap = font->GetColorTranslation(translation);

		if(fullScreenOffsets)
		{
			if(hud_scale)
			{
				xScale = script->cleanX;
				yScale = script->cleanY;
			}
			adjustRelCenter(x.RelCenter(), y.RelCenter(), *x, *y, ax, ay, xScale, yScale);
		}
		while(*str != '\0')
		{
			if(*str == ' ')
			{
				if(script->spacingCharacter == '\0')
					ax += font->GetSpaceWidth();
				else
					ax += font->GetCharWidth((unsigned char) script->spacingCharacter);
				str++;
				continue;
			}
			else if(*str == TEXTCOLOR_ESCAPE)
			{
				EColorRange newColor = V_ParseFontColor(++str, translation, boldTranslation);
				if(newColor != CR_UNDEFINED)
					remap = font->GetColorTranslation(newColor);
				continue;
			}

			int width;
			if(script->spacingCharacter == '\0') //No monospace?
				width = font->GetCharWidth((unsigned char) *str);
			else
				width = font->GetCharWidth((unsigned char) script->spacingCharacter);
			FTexture* character = font->GetChar((unsigned char) *str, &width);
			if(character == NULL) //missing character.
			{
				str++;
				continue;
			}
			if(script->spacingCharacter == '\0') //If we are monospaced lets use the offset
				ax += (character->LeftOffset+1); //ignore x offsets since we adapt to character size

			double rx, ry, rw, rh;
			rx = ax + xOffset;
			ry = ay + yOffset;
			rw = character->GetScaledWidthDouble();
			rh = character->GetScaledHeightDouble();

			if(script->spacingCharacter != '\0')
			{
				double spacingSize = font->GetCharWidth((unsigned char) script->spacingCharacter);
				switch(script->spacingAlignment)
				{
					default:
						break;
					case SBarInfo::ALIGN_CENTER:
						rx += (spacingSize/2)-(rw/2);
						break;
					case SBarInfo::ALIGN_RIGHT:
						rx += spacingSize-rw;
						break;
				}
			}

			if(!fullScreenOffsets)
			{
				rx += ST_X;
				ry += ST_Y - (Scaled ? script->resH : 200) + script->height;
				if(Scaled)
					screen->VirtualToRealCoords(rx, ry, rw, rh, script->resW, script->resH, true);
				else
				{
					ry += (200 - script->resH);
				}
			}
			else
			{
				if(vid_fps && ax < 0 && ay >= 0)
					ry += 10;

				bool xright = rx < 0;
				bool ybot = ry < 0;

				if(hud_scale)
				{
					rx *= xScale;
					ry *= yScale;
					rw *= xScale;
					rh *= yScale;
				}
				if(xright)
					rx = SCREENWIDTH + rx;
				if(ybot)
					ry = SCREENHEIGHT + ry;
			}
			if(drawshadow)
			{
				double salpha = (Alpha *HR_SHADOW);
				double srx = rx + (shadowX*xScale);
				double sry = ry + (shadowY*yScale);
				screen->DrawTexture(character, srx, sry,
					DTA_DestWidthF, rw,
					DTA_DestHeightF, rh,
					DTA_AlphaF, salpha,
					DTA_FillColor, 0,
					TAG_DONE);
			}
			screen->DrawTexture(character, rx, ry,
				DTA_DestWidthF, rw,
				DTA_DestHeightF, rh,
				DTA_Translation, remap,
				DTA_AlphaF, Alpha,
				TAG_DONE);
			if(script->spacingCharacter == '\0')
				ax += width + spacing - (character->LeftOffset+1);
			else //width gets changed at the call to GetChar()
				ax += font->GetCharWidth((unsigned char) script->spacingCharacter) + spacing;
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
	bool scalingWasForced;
	SBarInfoMainBlock *lastInventoryBar;
	SBarInfoMainBlock *lastPopup;
};

IMPLEMENT_POINTY_CLASS(DSBarInfo)
 DECLARE_POINTER(ammo1)
 DECLARE_POINTER(ammo2)
 DECLARE_POINTER(armor)
END_POINTERS

DBaseStatusBar *CreateCustomStatusBar (int script)
{
	if(SBarInfoScript[script] == NULL)
		I_FatalError("Tried to create a status bar with no script!");
	return new DSBarInfo(SBarInfoScript[script]);
}

void SBarInfoMainBlock::DrawAux(const SBarInfoMainBlock *block, DSBarInfo *statusBar, int xOffset, int yOffset, double alpha)
{
	// Popups can also be forced to scale
	bool rescale = false;
	if(ForceScaled())
	{
		if(FullScreenOffsets())
		{
			if(!hud_scale)
			{
				rescale = true;
				hud_scale = true;
			}
		}
		else if(!statusBar->Scaled)
		{
			rescale = true;
			statusBar->SetScaled(true, true);
		}
	}

	Draw(block, statusBar, xOffset, yOffset, alpha);

	if(rescale)
	{
		if(FullScreenOffsets())
			hud_scale = false;
		else
			statusBar->SetScaled(false);
	}
}

#include "sbarinfo_commands.cpp"
