/*
** sbarinfo.h
**
** Header for custom status bar definitions.
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

#ifndef __SBarInfo_SBAR_H__
#define __SBarInfo_SBAR_H__

#include "tarray.h"

#define NUMHUDS 9
#define NUMPOPUPS 3

class FScanner;

class SBarInfoMainBlock;

//Popups!
struct Popup
{
	enum PopupTransition
	{
		TRANSITION_NONE,
		TRANSITION_SLIDEINBOTTOM,
		TRANSITION_PUSHUP,
		TRANSITION_FADE,
	};

	PopupTransition transition;
	bool opened;
	bool moving;
	int height;
	int width;
	int speed;
	int speed2;
	int alpha;
	int x;
	int y;
	int displacementX;
	int displacementY;

	Popup();
	void init();
	void tick();
	void open();
	void close();
	bool isDoneMoving();
	int getXOffset();
	int getYOffset();
	int getAlpha(int maxAlpha=FRACUNIT);
	int getXDisplacement();
	int getYDisplacement();
};

struct SBarInfo
{
	enum MonospaceAlignment
	{
		ALIGN_LEFT,
		ALIGN_CENTER,
		ALIGN_RIGHT
	};

	TArray<FString> Images;
	SBarInfoMainBlock *huds[NUMHUDS];
	Popup popups[NUMPOPUPS];
	bool automapbar;
	bool interpolateHealth;
	bool interpolateArmor;
	bool completeBorder;
	bool lowerHealthCap;
	char spacingCharacter;
	MonospaceAlignment spacingAlignment;
	int interpolationSpeed;
	int armorInterpolationSpeed;
	int height;
	int gameType;
	FMugShot MugShot;
	int resW;
	int resH;
	int cleanX;
	int cleanY;

	int GetGameType() { return gameType; }
	void ParseSBarInfo(int lump);
	void ParseMugShotBlock(FScanner &sc, FMugShotState &state);
	void ResetHuds();
	int newImage(const char* patchname);
	void Init();
	SBarInfo();
	SBarInfo(int lumpnum);
	~SBarInfo();

	static void	Load();
};

#define SCRIPT_CUSTOM	0
#define SCRIPT_DEFAULT	1
extern SBarInfo *SBarInfoScript[2];

#endif //__SBarInfo_SBAR_H__
