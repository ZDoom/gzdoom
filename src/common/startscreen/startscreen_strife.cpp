/*
** st_start.cpp
** Handles the startup screen.
**
**---------------------------------------------------------------------------
** Copyright 2006-2007 Randy Heit
** Copyright 2006-2022 Christoph Oelckers
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

#include "startscreen.h"
#include "filesystem.h"
#include "printf.h"
#include "image.h"
#include "textures.h"
#include "palettecontainer.h"
#include "cmdlib.h"

// Strife startup screen
#define PEASANT_INDEX			0
#define LASER_INDEX				4
#define BOT_INDEX				6

#define ST_LASERSPACE_X			60
#define ST_LASERSPACE_Y			156
#define ST_LASERSPACE_WIDTH		200
#define ST_LASER_WIDTH			16
#define ST_LASER_HEIGHT			16

#define ST_BOT_X				14
#define ST_BOT_Y				138
#define ST_BOT_WIDTH			48
#define ST_BOT_HEIGHT			48

#define ST_PEASANT_X			262
#define ST_PEASANT_Y			136
#define ST_PEASANT_WIDTH		32
#define ST_PEASANT_HEIGHT		64

static const char* StrifeStartupPicNames[] =
{
	"STRTPA1", "STRTPB1", "STRTPC1", "STRTPD1",
	"STRTLZ1", "STRTLZ2",
	"STRTBOT",
	"STARTUP0"
};



class FStrifeStartScreen : public FStartScreen
{
public:
	FStrifeStartScreen(int max_progress);

	bool DoProgress(int) override;
protected:
	void DrawStuff(int old_laser, int new_laser);

	FBitmap StartupPics[4+2+1+1];
	int NotchPos = 0;
};


//==========================================================================
//
// FStrifeStartScreen Constructor
//
// Shows the Strife startup screen. If the screen doesn't appear to be
// valid, it returns a failure code in hr.
//
// The startup background is a raw 320x200 image, however Strife only
// actually uses 95 rows from it, starting at row 57. The rest of the image
// is discarded. (What a shame.)
//
// The peasants are raw 32x64 images. The laser dots are raw 16x16 images.
// The bot is a raw 48x48 image. All use the standard PLAYPAL.
//
//==========================================================================

FStrifeStartScreen::FStrifeStartScreen(int max_progress)
	: FStartScreen(max_progress)
{
	StartupBitmap.Create(320, 200);

	// at this point we do not have a working texture manager yet, so we have to do the lookup via the file system
	// Load the background and animated overlays.
	for (size_t i = 0; i < countof(StrifeStartupPicNames); ++i)
	{
		int lumpnum = fileSystem.CheckNumForName(StrifeStartupPicNames[i], ns_graphics);
		if (lumpnum < 0) lumpnum = fileSystem.CheckNumForName(StrifeStartupPicNames[i]);

		if (lumpnum >= 0)
		{
			auto lumpr1 = FImageSource::GetImage(lumpnum, false);
			if (lumpr1) StartupPics[i] = lumpr1->GetCachedBitmap(nullptr, FImageSource::normal);
		}
	}
	if (StartupPics[7].GetWidth() != 320 || StartupPics[7].GetHeight() != 200)
	{
		I_Error("bad startscreen assets");
	}

	// Make the startup image appear.
	DrawStuff(0, 0);
	Scale = 2;
	CreateHeader();
}

//==========================================================================
//
// FStrifeStartScreen :: Progress
//
// Bumps the progress meter one notch.
//
//==========================================================================

bool FStrifeStartScreen::DoProgress(int advance)
{
	int notch_pos;

	if (CurPos < MaxPos)
	{
		notch_pos = ((CurPos + 1) * (ST_LASERSPACE_WIDTH - ST_LASER_WIDTH)) / MaxPos;
		if (notch_pos != NotchPos && !(notch_pos & 1))
		{ // Time to update.
			DrawStuff(NotchPos, notch_pos);
			NotchPos = notch_pos;
			StartupTexture->CleanHardwareData(true);
		}
	}
	return FStartScreen::DoProgress(advance);
}

//==========================================================================
//
// FStrifeStartScreen :: DrawStuff
//
// Draws all the moving parts of Strife's startup screen. If you're
// running off a slow drive, it can look kind of good. Otherwise, it
// borders on crazy insane fast.
//
//==========================================================================

void FStrifeStartScreen::DrawStuff(int old_laser, int new_laser)
{
	int y;

	// Clear old laser
	StartupBitmap.Blit(0, 0, StartupPics[7]);

	// Draw new laser
	auto& lp = StartupPics[LASER_INDEX + (new_laser & 1)];
	StartupBitmap.Blit(ST_LASERSPACE_X + new_laser, ST_LASERSPACE_Y, lp);

	// The bot jumps up and down like crazy.
	y = max(0, (new_laser >> 1) % 5 - 2);

	StartupBitmap.Blit(ST_BOT_X, ST_BOT_Y + y, StartupPics[BOT_INDEX]);

	// The peasant desperately runs in place, trying to get away from the laser.
	// Yet, despite all his limb flailing, he never manages to get anywhere.
	auto& pp = StartupPics[PEASANT_INDEX + ((new_laser >> 1) & 3)];
	StartupBitmap.Blit(ST_PEASANT_X, ST_PEASANT_Y, pp);
}


FStartScreen* CreateStrifeStartScreen(int max_progress)
{
	return new FStrifeStartScreen(max_progress);
}
