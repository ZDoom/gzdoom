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
#include "basics.h"
#include "doomstat.h"
#include "v_font.h"
#include "v_video.h"
#include "sbar.h"
#include "filesystem.h"
#include "d_player.h"
#include "a_keys.h"
#include "sbarinfo.h"
#include "gi.h"
#include "p_acs.h"
#include "gstrings.h"
#include "g_levellocals.h"
#include "vm.h"
#include "i_system.h"
#include "utf8.h"
#include "texturemanager.h"
#include "v_palette.h"
#include "v_draw.h"

#define ARTIFLASH_OFFSET (statusBar->invBarOffset+6)
enum
{
	imgARTIBOX,
	imgSELECTBOX,
	imgCURSOR,
	imgINVLFGEM1,
	imgINVRTGEM1,
};

EXTERN_CVAR(Int, fraglimit)
EXTERN_CVAR(Int, screenblocks)

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
			if (!sc.CheckToken(TK_Null)) sc.MustGetToken(TK_Identifier);
			EColorRange returnVal = CR_UNTRANSLATED;
			FString namedTranslation; //we must send in "[translation]"
			const uint8_t *trans_ptr;
			namedTranslation.Format("[%s]", sc.String);
			trans_ptr = (const uint8_t *)(&namedTranslation[0]);
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
			std::swap(commands[0], commands[1]);
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
	SBARINFO_PROTRUSION,
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
	"protrusion",
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

void FreeSBarInfoScript()
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
		int lump = fileSystem.CheckNumForFullName(gameinfo.statusbar, true);
		if(lump != -1)
		{
			if (!batchrun) Printf ("ParseSBarInfo: Loading default status bar definition.\n");
			if(SBarInfoScript[SCRIPT_DEFAULT] == NULL)
				SBarInfoScript[SCRIPT_DEFAULT] = new SBarInfo(lump);
			else
				SBarInfoScript[SCRIPT_DEFAULT]->ParseSBarInfo(lump);
		}
	}

	if(fileSystem.CheckNumForName("SBARINFO") != -1)
	{
		if (!batchrun) Printf ("ParseSBarInfo: Loading custom status bar definition.\n");
		int lastlump, lump;
		lastlump = 0;
		while((lump = fileSystem.FindLump("SBARINFO", &lastlump)) != -1)
		{
			if(SBarInfoScript[SCRIPT_CUSTOM] == NULL)
				SBarInfoScript[SCRIPT_CUSTOM] = new SBarInfo(lump);
			else //We now have to load multiple SBarInfo Lumps so the 2nd time we need to use this method instead.
				SBarInfoScript[SCRIPT_CUSTOM]->ParseSBarInfo(lump);
		}
	}
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
			int lump = fileSystem.CheckNumForFullName(sc.String, true);
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
					baselump = fileSystem.CheckNumForFullName("sbarinfo/doom.txt", true);
				}
				else if(sc.Compare("Heretic"))
				{
					baselump = fileSystem.CheckNumForFullName("sbarinfo/heretic.txt", true);
				}
				else if(sc.Compare("Hexen"))
				{
					baselump = fileSystem.CheckNumForFullName("sbarinfo/hexen.txt", true);
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
					else if (fileSystem.GetFileContainer(baselump) > 0)
					{
						I_FatalError("File %s is overriding core lump sbarinfo/%s.txt.",
							fileSystem.GetResourceFileFullName(fileSystem.GetFileContainer(baselump)), sc.String);
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
				_resW = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				_resH = sc.Number;
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
						popup.speed = 1.0 / (TICRATE * sc.Float);
						sc.MustGetToken(',');
						sc.MustGetToken(TK_FloatConst);
						popup.speed2 = 1.0 / (TICRATE * sc.Float);
					}
					else
						sc.ScriptError("Unkown transition type: '%s'", sc.String);
				}
				popup.init();
				sc.MustGetToken(';');
				break;
			}
			case SBARINFO_PROTRUSION:
			{
				double lastvalue = -DBL_EPSILON;
				do
				{
					sc.MustGetToken(TK_FloatConst);
					if (sc.Float <= lastvalue) sc.ScriptError("Protrusion factors must be in ascending order");
					lastvalue = sc.Float;
					sc.MustGetToken(',');
					sc.MustGetToken(TK_IntConst);
					protrusions.Push({ lastvalue, sc.Number });
				} while (sc.CheckToken(','));
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
	_resW = 320;
	_resH = 200;

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

inline void adjustRelCenter(bool relX, bool relY, const double &x, const double &y, double &outX, double &outY, double ScaleX, double ScaleY)
{
	if(relX)
		outX = x + (twod->GetWidth()/(ScaleX*2));
	else
		outX = x;
	if(relY)
		outY = y + (twod->GetHeight()/(ScaleY*2));
	else
		outY = y;
}

class DSBarInfo
{
public:
	DSBarInfo (DBaseStatusBar *wrapper, SBarInfo *script=NULL) :
		ammo1(NULL), ammo2(NULL), ammocount1(0), ammocount2(0), armor(NULL),
		pendingPopup(DBaseStatusBar::POP_None), currentPopup(DBaseStatusBar::POP_None), lastHud(-1),
		lastInventoryBar(NULL), lastPopup(NULL)
	{
		this->script = script;
		this->wrapper = wrapper;

		static const char *InventoryBarLumps[] =
		{
			"ARTIBOX",	"SELECTBO", "INVCURS",	"INVGEML1", "INVGEMR1",
			"USEARTIA", "USEARTIB", "USEARTIC", "USEARTID",
		};
		TArray<const char *> patchnames;
		patchnames.Resize(script->Images.Size()+9);
		unsigned int i = 0;
		for(i = 0;i < script->Images.Size();i++)
		{
			patchnames[i] = script->Images[i];
		}
		for(i = 0;i < 9;i++)
		{
			patchnames[i+script->Images.Size()] = InventoryBarLumps[i];
		}
		invBarOffset = script->Images.Size();
		Images.Init(&patchnames[0], patchnames.Size());
	}

	~DSBarInfo ()
	{
		Images.Uninit();
	}

	void _AttachToPlayer(player_t *player)
	{
		CPlayer = player;
	}

	void SetReferences()
	{
		if (CPlayer->ReadyWeapon != nullptr)
		{
			ammo1 = CPlayer->ReadyWeapon->PointerVar<AActor>(NAME_Ammo1);
			ammo2 = CPlayer->ReadyWeapon->PointerVar<AActor>(NAME_Ammo2);
			if (ammo1 == nullptr)
			{
				ammo1 = ammo2;
				ammo2 = nullptr;
			}
		}
		else
		{
			ammo1 = ammo2 = nullptr;
		}
		ammocount1 = ammo1 != nullptr ? ammo1->IntVar(NAME_Amount) : 0;
		ammocount2 = ammo2 != nullptr ? ammo2->IntVar(NAME_Amount) : 0;

		//prepare ammo counts
		armor = CPlayer->mo->FindInventory(NAME_BasicArmor);
	}

	void _Draw (EHudState state)
	{
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
		wrapper->ForceHUDScale(script->huds[hud]->ForceScaled());

		SetReferences();

		if(state != HUD_AltHud)
		{
			if(hud != lastHud)
			{
				script->huds[hud]->Tick(NULL, this, true);
				// Restore scaling if need be.
			}
			wrapper->ForceHUDScale(script->huds[hud]->ForceScaled());

			if(currentPopup != DBaseStatusBar::POP_None && !script->huds[hud]->FullScreenOffsets())
				script->huds[hud]->Draw(NULL, this, script->popups[currentPopup-1].getXDisplacement(), script->popups[currentPopup-1].getYDisplacement(), 1.);
			else
				script->huds[hud]->Draw(NULL, this, 0, 0, 1.);
			lastHud = hud;

			// Handle inventory bar drawing
			if(CPlayer->inventorytics > 0 && !(primaryLevel->flags & LEVEL_NOINVENTORYBAR) && (state == HUD_StatusBar || state == HUD_Fullscreen))
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
			// Reset hud scale
			wrapper->ForceHUDScale(false);
		}

		// Handle popups
		if(currentPopup != DBaseStatusBar::POP_None)
		{
			int popbar = 0;
			if(currentPopup == DBaseStatusBar::POP_Log)
				popbar = STBAR_POPUPLOG;
			else if(currentPopup == DBaseStatusBar::POP_Keys)
				popbar = STBAR_POPUPKEYS;
			else if(currentPopup == DBaseStatusBar::POP_Status)
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

		// These may not live any longer than beyond here!
		ammo1 = ammo2 = nullptr;
		armor = nullptr;
	}

	void _NewGame ()
	{
		// Reset the huds
		script->ResetHuds();
		lastHud = -1; // Reset
	}

	bool _MustDrawLog (EHudState state)
	{
		return script->huds[STBAR_POPUPLOG]->NumCommands() == 0;
	}

	void _Tick ()
	{
		SetReferences();
		if(currentPopup != DBaseStatusBar::POP_None)
		{
			script->popups[currentPopup-1].tick();
			if(script->popups[currentPopup-1].opened == false && script->popups[currentPopup-1].isDoneMoving())
			{
				currentPopup = pendingPopup;
				if(currentPopup != DBaseStatusBar::POP_None)
					script->popups[currentPopup-1].open();
			}

			if (lastPopup != NULL) lastPopup->Tick(NULL, this, false);
		}

		if(lastHud != -1)
			script->huds[lastHud]->Tick(NULL, this, false);
		if(lastInventoryBar != NULL && CPlayer->inventorytics > 0)
			lastInventoryBar->Tick(NULL, this, false);

		// These may not live any longer than beyond here!
		ammo1 = ammo2 = nullptr;
		armor = nullptr;
	}

	void _ShowPop(int popnum)
	{
		if(popnum != currentPopup)
		{
			pendingPopup = popnum;
		}
		else
			pendingPopup = DBaseStatusBar::POP_None;
		if(currentPopup != DBaseStatusBar::POP_None)
			script->popups[currentPopup-1].close();
		else
		{
			currentPopup = pendingPopup;
			pendingPopup = DBaseStatusBar::POP_None;
			if(currentPopup != DBaseStatusBar::POP_None)
				script->popups[currentPopup-1].open();
		}
	}

	int _GetProtrusion(double scalefac)
	{
		int returnval = 0;
		for (auto &prot : script->protrusions)
		{
			if (prot.first > scalefac) break;
			returnval = prot.second;
		}
		return returnval;
	}

	//draws an image with the specified flags
	void DrawGraphic(FGameTexture* texture, SBarInfoCoordinate x, SBarInfoCoordinate y, int xOffset, int yOffset, double Alpha, bool fullScreenOffsets, bool translate=false, bool dim=false, int offsetflags=0, bool alphaMap=false, int forceWidth=-1, int forceHeight=-1, const double *clip = nulclip, bool clearDontDraw=false) const
	{
		if (texture == NULL)
			return;

		double dx = *x;
		double dy = *y;

		if((offsetflags & SBarInfoCommand::CENTER) == SBarInfoCommand::CENTER)
		{
			if (forceWidth < 0)	dx -= (texture->GetDisplayWidth()/2.0)-texture->GetDisplayLeftOffset();
			else	dx -= forceWidth*(0.5-(texture->GetDisplayLeftOffset()/texture->GetDisplayWidth()));
			
			if (forceHeight < 0)	dy -= (texture->GetDisplayHeight()/2.0)-texture->GetDisplayTopOffset();
			else	dy -= forceHeight*(0.5-(texture->GetDisplayTopOffset()/texture->GetDisplayHeight()));
		}

		dx += xOffset;
		dy += yOffset;
		double w, h;
		if(!fullScreenOffsets)
		{
			double tmp = 0;
			w = forceWidth < 0 ? texture->GetDisplayWidth() : forceWidth;
			h = forceHeight < 0 ? texture->GetDisplayHeight() : forceHeight;
			double dcx = clip[0] == 0 ? 0 : dx + clip[0] - texture->GetDisplayLeftOffset();
			double dcy = clip[1] == 0 ? 0 : dy + clip[1] - texture->GetDisplayTopOffset();
			double dcr = clip[2] == 0 ? INT_MAX : dx + w - clip[2] - texture->GetDisplayLeftOffset();
			double dcb = clip[3] == 0 ? INT_MAX : dy + h - clip[3] - texture->GetDisplayTopOffset();

			if(clip[0] != 0 || clip[1] != 0)
			{
				wrapper->StatusbarToRealCoords(dcx, dcy, tmp, tmp);
				if (clip[0] == 0) dcx = 0;
				if (clip[1] == 0) dcy = 0;
			}
			if (clip[2] != 0 || clip[3] != 0 || clearDontDraw)
			{
				wrapper->StatusbarToRealCoords(dcr, dcb, tmp, tmp);
			}
			wrapper->StatusbarToRealCoords(dx, dy, w, h);

			if(clearDontDraw)
				ClearRect(twod, static_cast<int>(max<double>(dx, dcx)), static_cast<int>(max<double>(dy, dcy)), static_cast<int>(min<double>(dcr,w+max<double>(dx, dcx))), static_cast<int>(min<double>(dcb,max<double>(dy, dcy)+h)), GPalette.BlackIndex, 0);
			else
			{
				if(alphaMap)
				{
					DrawTexture(twod, texture, dx, dy,
						DTA_DestWidthF, w,
						DTA_DestHeightF, h,
						DTA_ClipLeft, static_cast<int>(dcx),
						DTA_ClipTop, static_cast<int>(dcy),
						DTA_ClipRight, static_cast<int>(min<double>(INT_MAX, dcr)),
						DTA_ClipBottom, static_cast<int>(min<double>(INT_MAX, dcb)),
						DTA_TranslationIndex, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_Alpha, Alpha,
						DTA_AlphaChannel, alphaMap,
						DTA_FillColor, 0,
						TAG_DONE);
				}
				else
				{
					DrawTexture(twod, texture, dx, dy,
						DTA_DestWidthF, w,
						DTA_DestHeightF, h,
						DTA_ClipLeft, static_cast<int>(dcx),
						DTA_ClipTop, static_cast<int>(dcy),
						DTA_ClipRight, static_cast<int>(min<double>(INT_MAX, dcr)),
						DTA_ClipBottom, static_cast<int>(min<double>(INT_MAX, dcb)),
						DTA_TranslationIndex, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_Alpha, Alpha,
						TAG_DONE);
				}
			}
		}
		else
		{
			double rx, ry, rcx=0, rcy=0, rcr=INT_MAX, rcb=INT_MAX;

			DVector2 Scale = wrapper->GetHUDScale();

			adjustRelCenter(x.RelCenter(), y.RelCenter(), dx, dy, rx, ry, Scale.X, Scale.Y);

			// Translation: No high res.
			bool xright = *x < 0 && !x.RelCenter();
			bool ybot = *y < 0 && !y.RelCenter();

			w = (forceWidth < 0 ? texture->GetDisplayWidth() : forceWidth);
			h = (forceHeight < 0 ? texture->GetDisplayHeight() : forceHeight);

			rx *= Scale.X;
			ry *= Scale.Y;
			w *= Scale.X;
			h *= Scale.Y;

			if(xright)
				rx = twod->GetWidth() + rx;
			if(ybot)
				ry = twod->GetHeight() + ry;

			// Check for clipping
			if(clip[0] != 0 || clip[1] != 0 || clip[2] != 0 || clip[3] != 0)
			{
				rcx = clip[0] == 0 ? 0 : rx+((clip[0] - texture->GetDisplayLeftOffset())*Scale.X);
				rcy = clip[1] == 0 ? 0 : ry+((clip[1] - texture->GetDisplayTopOffset())*Scale.Y);
				rcr = clip[2] == 0 ? INT_MAX : rx+w-((clip[2] + texture->GetDisplayLeftOffset())*Scale.X);
				rcb = clip[3] == 0 ? INT_MAX : ry+h-((clip[3] + texture->GetDisplayTopOffset())*Scale.Y);
			}

			if(clearDontDraw)
				ClearRect(twod, static_cast<int>(rcx), static_cast<int>(rcy), static_cast<int>(min<double>(rcr, rcx+w)), static_cast<int>(min<double>(rcb, rcy+h)), GPalette.BlackIndex, 0);
			else
			{
				if(alphaMap)
				{
					DrawTexture(twod, texture, rx, ry,
						DTA_DestWidthF, w,
						DTA_DestHeightF, h,
						DTA_ClipLeft, static_cast<int>(rcx),
						DTA_ClipTop, static_cast<int>(rcy),
						DTA_ClipRight, static_cast<int>(rcr),
						DTA_ClipBottom, static_cast<int>(rcb),
						DTA_TranslationIndex, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_Alpha, Alpha,
						DTA_AlphaChannel, alphaMap,
						DTA_FillColor, 0,
						TAG_DONE);
				}
				else
				{
					DrawTexture(twod, texture, rx, ry,
						DTA_DestWidthF, w,
						DTA_DestHeightF, h,
						DTA_ClipLeft, static_cast<int>(rcx),
						DTA_ClipTop, static_cast<int>(rcy),
						DTA_ClipRight, static_cast<int>(rcr),
						DTA_ClipBottom, static_cast<int>(rcb),
						DTA_TranslationIndex, translate ? GetTranslation() : 0,
						DTA_ColorOverlay, dim ? DIM_OVERLAY : 0,
						DTA_CenterBottomOffset, (offsetflags & SBarInfoCommand::CENTER_BOTTOM) == SBarInfoCommand::CENTER_BOTTOM,
						DTA_Alpha, Alpha,
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

		DVector2 Scale;

		const uint8_t* str = (const uint8_t*) cstring;
		const EColorRange boldTranslation = EColorRange(translation ? translation - 1 : NumTextColors - 1);
		int fontcolor = translation;

		if(fullScreenOffsets)
		{
			Scale = wrapper->GetHUDScale();
			adjustRelCenter(x.RelCenter(), y.RelCenter(), *x, *y, ax, ay, Scale.X, Scale.Y);
		}
		else
		{
			Scale = { 1.,1. };
		}
		int ch;
		while (ch = GetCharFromString(str), ch != '\0')
		{
			if(ch == ' ')
			{
				if(script->spacingCharacter == '\0')
					ax += font->GetSpaceWidth();
				else
					ax += font->GetCharWidth((unsigned char) script->spacingCharacter);
				continue;
			}
			else if(ch == TEXTCOLOR_ESCAPE)
			{
				EColorRange newColor = V_ParseFontColor(str, translation, boldTranslation);
				if(newColor != CR_UNDEFINED)
					fontcolor = newColor;
				continue;
			}

			int width;
			if(script->spacingCharacter == '\0') //No monospace?
				width = font->GetCharWidth(ch);
			else
				width = font->GetCharWidth((unsigned char) script->spacingCharacter);
			bool redirected = false;
			auto c = font->GetChar(ch, fontcolor, &width);
			if(c == NULL) //missing character.
			{
				continue;
			}

			if (script->spacingCharacter == '\0') //If we are monospaced lets use the offset
				ax += (c->GetDisplayLeftOffset() + 1); //ignore x offsets since we adapt to character size

			double rx, ry, rw, rh;
			rx = ax + xOffset;
			ry = ay + yOffset;
			rw = c->GetDisplayWidth();
			rh = c->GetDisplayHeight();

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
				wrapper->StatusbarToRealCoords(rx, ry, rw, rh);
			}
			else
			{

				bool xright = rx < 0;
				bool ybot = ry < 0;

				rx *= Scale.X;
				ry *= Scale.Y;
				rw *= Scale.X;
				rh *= Scale.Y;

				if(xright)
					rx = twod->GetWidth() + rx;
				if(ybot)
					ry = twod->GetHeight() + ry;
			}
			if(drawshadow)
			{
				double salpha = (Alpha *HR_SHADOW);
				double srx = rx + (shadowX*Scale.X);
				double sry = ry + (shadowY*Scale.Y);
				DrawChar(twod, font, CR_UNTRANSLATED, srx, sry, ch,
					DTA_DestWidthF, rw,
					DTA_DestHeightF, rh,
					DTA_Alpha, salpha,
					DTA_FillColor, 0,
					TAG_DONE);
			}
			DrawChar(twod, font, fontcolor, rx, ry, ch,
				DTA_DestWidthF, rw,
				DTA_DestHeightF, rh,
				DTA_Alpha, Alpha,
				TAG_DONE);
			if (script->spacingCharacter == '\0')
				ax += width + spacing - (c->GetDisplayLeftOffset() + 1);
			else //width gets changed at the call to GetChar()
				ax += font->GetCharWidth((unsigned char) script->spacingCharacter) + spacing;
		}
	}

	uint32_t GetTranslation() const
	{
		if(gameinfo.gametype & GAME_Raven)
			return TRANSLATION(TRANSLATION_PlayersExtra, int(CPlayer - players));
		return TRANSLATION(TRANSLATION_Players, int(CPlayer - players));
	}

	PClassActor *AmmoType(int no) const
	{
		auto w = StatusBar->CPlayer->ReadyWeapon;
		return w == nullptr ? nullptr : (w->PointerVar<PClassActor>(no == 1 ? NAME_AmmoType1 : NAME_AmmoType2));
	}

	AActor *ammo1, *ammo2;
	int ammocount1, ammocount2;
	AActor *armor;
	FImageCollection Images;
	unsigned int invBarOffset;
	player_t *CPlayer = nullptr;
	DBaseStatusBar *wrapper;

private:
	SBarInfo *script;
	int pendingPopup;
	int currentPopup;
	int lastHud;
	SBarInfoMainBlock *lastInventoryBar;
	SBarInfoMainBlock *lastPopup;
};


void SBarInfoMainBlock::DrawAux(const SBarInfoMainBlock *block, DSBarInfo *statusBar, int xOffset, int yOffset, double alpha)
{
	// Popups can also be forced to scale
	bool old = statusBar->wrapper->ForceHUDScale(ForceScaled());
	Draw(block, statusBar, xOffset, yOffset, alpha);
	statusBar->wrapper->ForceHUDScale(old);
}

#include "sbarinfo_commands.cpp"


//==========================================================================
//
// This routes all access through a scripted class because a native
// side class of an entire scripted hierarchy would be a major obstacle.
//
//==========================================================================

static void SBarInfo_Destroy(DSBarInfo *self)
{
	delete self;
}

DEFINE_ACTION_FUNCTION_NATIVE(DSBarInfo, Destroy, SBarInfo_Destroy)
{
	PARAM_SELF_STRUCT_PROLOGUE(DSBarInfo);
	delete self;
	return 0;
}

static void SBarInfo_AttachToPlayer(DSBarInfo *self, player_t *player)
{
	self->_AttachToPlayer(player);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSBarInfo, AttachToPlayer, SBarInfo_AttachToPlayer)
{
	PARAM_SELF_STRUCT_PROLOGUE(DSBarInfo);
	PARAM_POINTER(player, player_t);
	self->_AttachToPlayer(player);
	return 0;
}

static void SBarInfo_Draw(DSBarInfo *self, int state)
{
	self->_Draw((EHudState)state);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSBarInfo, Draw, SBarInfo_Draw)
{
	PARAM_SELF_STRUCT_PROLOGUE(DSBarInfo);
	PARAM_INT(State);
	self->_Draw((EHudState)State);
	return 0;
}

static void SBarInfo_NewGame(DSBarInfo *self)
{
	self->_NewGame();
}

DEFINE_ACTION_FUNCTION_NATIVE(DSBarInfo, NewGame, SBarInfo_NewGame)
{
	PARAM_SELF_STRUCT_PROLOGUE(DSBarInfo);
	self->_NewGame();
	return 0;
}

static int SBarInfo_MustDrawLog(DSBarInfo *self, int state)
{
	return self->_MustDrawLog((EHudState)state);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSBarInfo, MustDrawLog, SBarInfo_MustDrawLog)
{
	PARAM_SELF_STRUCT_PROLOGUE(DSBarInfo);
	PARAM_INT(State);
	ACTION_RETURN_BOOL(self->_MustDrawLog((EHudState)State));
}

static void SBarInfo_Tick(DSBarInfo *self)
{
	self->_Tick();
}

DEFINE_ACTION_FUNCTION_NATIVE(DSBarInfo, Tick, SBarInfo_Tick)
{
	PARAM_SELF_STRUCT_PROLOGUE(DSBarInfo);
	self->_Tick();
	return 0;
}

static void SBarInfo_ShowPop(DSBarInfo *self, int state)
{
	self->_ShowPop(state);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSBarInfo, ShowPop, SBarInfo_ShowPop)
{
	PARAM_SELF_STRUCT_PROLOGUE(DSBarInfo);
	PARAM_INT(State);
	self->_ShowPop(State);
	return 0;
}

static int SBarInfo_GetProtrusion(DSBarInfo *self, double scale)
{
	return self->_GetProtrusion(scale);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSBarInfo, GetProtrusion, SBarInfo_GetProtrusion)
{
	PARAM_SELF_STRUCT_PROLOGUE(DSBarInfo);
	PARAM_FLOAT(scalefac);
	ACTION_RETURN_INT(self->_GetProtrusion(scalefac));
}

DBaseStatusBar *CreateCustomStatusBar(int scriptno)
{
	auto script = SBarInfoScript[scriptno];
	if (script == nullptr) return nullptr;

	PClass *sbarclass = PClass::FindClass("SBarInfoWrapper");
	assert(sbarclass != nullptr);
	assert(sbarclass->IsDescendantOf(RUNTIME_CLASS(DBaseStatusBar)));
	auto sbar = (DBaseStatusBar*)sbarclass->CreateNew();

	auto core = new DSBarInfo(sbar, script);
	sbar->PointerVar<DSBarInfo>("core") = core;
	sbar->SetSize(script->height, script->_resW, script->_resH);
	sbar->CompleteBorder = script->completeBorder;

	return sbar;
}

