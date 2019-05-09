//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------

/* For code that originates from ZDoom the following applies:
**
**---------------------------------------------------------------------------
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

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "i_system.h"
#include "i_sound.h"
#include "i_music.h"
#include "i_cd.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "s_playlist.h"
#include "c_dispatch.h"
#include "m_random.h"
#include "w_wad.h"
#include "p_local.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "v_video.h"
#include "v_text.h"
#include "a_sharedglobal.h"
#include "gstrings.h"
#include "gi.h"
#include "po_man.h"
#include "serializer.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "vm.h"
#include "g_game.h"

// MACROS ------------------------------------------------------------------

#ifdef NeXT
// NeXT doesn't need a binary flag in open call
#define O_BINARY 0
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define NORM_PITCH				128
#define NORM_PRIORITY			64
#define NORM_SEP				0

#define S_PITCH_PERTURB 		1
#define S_STEREO_SWING			0.75

// TYPES -------------------------------------------------------------------

enum
{
	SOURCE_None,		// Sound is always on top of the listener.
	SOURCE_Actor,		// Sound is coming from an actor.
	SOURCE_Sector,		// Sound is coming from a sector.
	SOURCE_Polyobj,		// Sound is coming from a polyobject.
	SOURCE_Unattached,	// Sound is not attached to any particular emitter.
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern float S_GetMusicVolume (const char *music);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void S_LoadSound3D(sfxinfo_t *sfx, FSoundLoadBuffer *pBuffer);
static bool S_CheckSoundLimit(sfxinfo_t *sfx, const FVector3 &pos, int near_limit, float limit_range, AActor *actor, int channel);
static bool S_IsChannelUsed(AActor *actor, int channel, int *seen);
static void S_ActivatePlayList(bool goBack);
static void CalcPosVel(FSoundChan *chan, FVector3 *pos, FVector3 *vel);
static void CalcPosVel(int type, const AActor *actor, const sector_t *sector, const FPolyObj *poly,
	const float pt[3], int channel, int chanflags, FVector3 *pos, FVector3 *vel);
static void CalcSectorSoundOrg(const DVector3 &listenpos, const sector_t *sec, int channum, FVector3 &res);
static void CalcPolyobjSoundOrg(const DVector3 &listenpos, const FPolyObj *poly, FVector3 &res);
static FSoundChan *S_StartSound(AActor *mover, const sector_t *sec, const FPolyObj *poly,
	const FVector3 *pt, int channel, FSoundID sound_id, float volume, float attenuation, FRolloffInfo *rolloff);
static void S_SetListener(SoundListener &listener, AActor *listenactor);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static bool		SoundPaused;		// whether sound is paused
static bool		MusicPaused;		// whether music is paused
MusPlayingInfo mus_playing;	// music currently being played
static FString	 LastSong;			// last music that was played
static FPlayList *PlayList;
static int		RestartEvictionsAt;	// do not restart evicted channels before this time

DEFINE_GLOBAL_NAMED(mus_playing, musplaying);
DEFINE_FIELD_X(MusPlayingInfo, MusPlayingInfo, name);
DEFINE_FIELD_X(MusPlayingInfo, MusPlayingInfo, baseorder);
DEFINE_FIELD_X(MusPlayingInfo, MusPlayingInfo, loop);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int sfx_empty;

FSoundChan *Channels;
FSoundChan *FreeChannels;

FRolloffInfo S_Rolloff;
TArray<uint8_t> S_SoundCurve;

FBoolCVar noisedebug ("noise", false, 0);	// [RH] Print sound debugging info?
CUSTOM_CVAR (Int, snd_channels, 128, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// number of channels available
{
	if (self < 64) self = 64;
}
CVAR(Bool, snd_waterreverb, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// CODE --------------------------------------------------------------------

//==========================================================================
//
// S_NoiseDebug
//
// [RH] Print sound debug info. Called by status bar.
//==========================================================================

void S_NoiseDebug (void)
{
	FSoundChan *chan;
	FVector3 listener;
	FVector3 origin;
	int y, color;

	y = 32 * CleanYfac;
	screen->DrawText (NewConsoleFont, CR_YELLOW, 0, y, "*** SOUND DEBUG INFO ***", TAG_DONE);
	y += NewConsoleFont->GetHeight();

	screen->DrawText (NewConsoleFont, CR_GOLD, 0, y, "name", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 70, y, "x", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 120, y, "y", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 170, y, "z", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 220, y, "vol", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 260, y, "dist", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 300, y, "chan", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 340, y, "pri", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 380, y, "flags", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 460, y, "aud", TAG_DONE);
	screen->DrawText (NewConsoleFont, CR_GOLD, 520, y, "pos", TAG_DONE);
	y += NewConsoleFont->GetHeight();

	if (Channels == NULL)
	{
		return;
	}


	listener = players[consoleplayer].camera->SoundPos();

	// Display the oldest channel first.
	for (chan = Channels; chan->NextChan != NULL; chan = chan->NextChan)
	{ }
	while (y < SCREENHEIGHT - 16)
	{
		char temp[32];

		CalcPosVel(chan, &origin, NULL);
		color = (chan->ChanFlags & CHAN_LOOP) ? CR_BROWN : CR_GREY;

		// Name
		Wads.GetLumpName (temp, S_sfx[chan->SoundID].lumpnum);
		temp[8] = 0;
		screen->DrawText (NewConsoleFont, color, 0, y, temp, TAG_DONE);

		if (!(chan->ChanFlags & CHAN_IS3D))
		{
			screen->DrawText(NewConsoleFont, color, 70, y, "---", TAG_DONE);		// X
			screen->DrawText(NewConsoleFont, color, 120, y, "---", TAG_DONE);	// Y
			screen->DrawText(NewConsoleFont, color, 170, y, "---", TAG_DONE);	// Z
			screen->DrawText(NewConsoleFont, color, 260, y, "---", TAG_DONE);	// Distance
		}
		else
		{
			// X coordinate
			mysnprintf(temp, countof(temp), "%.0f", origin.X);
			screen->DrawText(NewConsoleFont, color, 70, y, temp, TAG_DONE);

			// Y coordinate
			mysnprintf(temp, countof(temp), "%.0f", origin.Z);
			screen->DrawText(NewConsoleFont, color, 120, y, temp, TAG_DONE);

			// Z coordinate
			mysnprintf(temp, countof(temp), "%.0f", origin.Y);
			screen->DrawText(NewConsoleFont, color, 170, y, temp, TAG_DONE);

			// Distance
			if (chan->DistanceScale > 0)
			{
				mysnprintf(temp, countof(temp), "%.0f", (origin - listener).Length());
				screen->DrawText(NewConsoleFont, color, 260, y, temp, TAG_DONE);
			}
			else
			{
				screen->DrawText(NewConsoleFont, color, 260, y, "---", TAG_DONE);
			}
		}

		// Volume
		mysnprintf(temp, countof(temp), "%.2g", chan->Volume);
		screen->DrawText(NewConsoleFont, color, 220, y, temp, TAG_DONE);

		// Channel
		mysnprintf(temp, countof(temp), "%d", chan->EntChannel);
		screen->DrawText(NewConsoleFont, color, 300, y, temp, TAG_DONE);

		// Priority
		mysnprintf(temp, countof(temp), "%d", chan->Priority);
		screen->DrawText(NewConsoleFont, color, 340, y, temp, TAG_DONE);

		// Flags
		mysnprintf(temp, countof(temp), "%s3%sZ%sU%sM%sN%sA%sL%sE%sV",
			(chan->ChanFlags & CHAN_IS3D)			? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_LISTENERZ)		? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_UI)				? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_MAYBE_LOCAL)	? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_NOPAUSE)		? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_AREA)			? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_LOOP)			? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_EVICTED)		? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHAN_VIRTUAL)		? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK);
		screen->DrawText(NewConsoleFont, color, 380, y, temp, TAG_DONE);

		// Audibility
		mysnprintf(temp, countof(temp), "%.4f", GSnd->GetAudibility(chan));
		screen->DrawText(NewConsoleFont, color, 460, y, temp, TAG_DONE);

		// Position
		mysnprintf(temp, countof(temp), "%u", GSnd->GetPosition(chan));
		screen->DrawText(NewConsoleFont, color, 520, y, temp, TAG_DONE);


		y += NewConsoleFont->GetHeight();
		if (chan->PrevChan == &Channels)
		{
			break;
		}
		chan = (FSoundChan *)((size_t)chan->PrevChan - myoffsetof(FSoundChan, NextChan));
	}
}

static FString LastLocalSndInfo;
static FString LastLocalSndSeq;
void S_AddLocalSndInfo(int lump);

//==========================================================================
//
// S_Init
//
// Initializes sound stuff, including volume. Sets channels, SFX and
// music volume, allocates channel buffer, and sets S_sfx lookup.
//==========================================================================

void S_Init ()
{
	int curvelump;

	atterm (S_Shutdown);

	// Heretic and Hexen have sound curve lookup tables. Doom does not.
	curvelump = Wads.CheckNumForName ("SNDCURVE");
	if (curvelump >= 0)
	{
		S_SoundCurve.Resize(Wads.LumpLength (curvelump));
		Wads.ReadLump(curvelump, S_SoundCurve.Data());
	}
	else
	{
		S_SoundCurve.Clear();
	}

	// Free all channels for use.
	while (Channels != NULL)
	{
		S_ReturnChannel(Channels);
	}

	// no sounds are playing, and they are not paused
	MusicPaused = false;
}

//==========================================================================
//
// S_InitData
//
//==========================================================================

void S_InitData ()
{
	LastLocalSndInfo = LastLocalSndSeq = "";
	S_ParseSndInfo (false);
	S_ParseSndSeq (-1);
}

//==========================================================================
//
// S_Shutdown
//
//==========================================================================

void S_Shutdown ()
{
	FSoundChan *chan, *next;

	chan = Channels;
	while (chan != NULL)
	{
		next = chan->NextChan;
		S_StopChannel(chan);
		chan = next;
	}

	GSnd->UpdateSounds();
	for (chan = FreeChannels; chan != NULL; chan = next)
	{
		next = chan->NextChan;
		delete chan;
	}
	FreeChannels = NULL;

	if (PlayList != NULL)
	{
		delete PlayList;
		PlayList = NULL;
	}
	S_StopMusic (true);
	mus_playing.name = "";
	LastSong = "";
}

//==========================================================================
//
// S_Start
//
// Per level startup code. Kills playing sounds at start of level
// and starts new music.
//==========================================================================

void S_Start ()
{
	if (GSnd)
	{
		// kill all playing sounds at start of level (trust me - a good idea)
		S_StopAllChannels();

		// Check for local sound definitions. Only reload if they differ
		// from the previous ones.
		FString LocalSndInfo;
		FString LocalSndSeq;
		
		// To be certain better check whether level is valid!
		if (primaryLevel->info)
		{
			LocalSndInfo = primaryLevel->info->SoundInfo;
			LocalSndSeq = primaryLevel->info->SndSeq;
		}

			bool parse_ss = false;

			// This level uses a different local SNDINFO
		if (LastLocalSndInfo.CompareNoCase(LocalSndInfo) != 0 || !primaryLevel->info)
			{
				// First delete the old sound list
			for(unsigned i = 1; i < S_sfx.Size(); i++) 
				{
					S_UnloadSound(&S_sfx[i]);
				}
				
				// Parse the global SNDINFO
				S_ParseSndInfo(true);
				
				if (LocalSndInfo.IsNotEmpty())
				{
					// Now parse the local SNDINFO
					int j = Wads.CheckNumForFullName(LocalSndInfo, true);
					if (j>=0) S_AddLocalSndInfo(j);
				}

				// Also reload the SNDSEQ if the SNDINFO was replaced!
				parse_ss = true;
			}
			else if (LastLocalSndSeq.CompareNoCase(LocalSndSeq) != 0)
			{
				parse_ss = true;
			}

			if (parse_ss)
			{
				S_ParseSndSeq(LocalSndSeq.IsNotEmpty()? Wads.CheckNumForFullName(LocalSndSeq, true) : -1);
			}
			
			LastLocalSndInfo = LocalSndInfo;
			LastLocalSndSeq = LocalSndSeq;
		}

	// stop the old music if it has been paused.
	// This ensures that the new music is started from the beginning
	// if it's the same as the last one and it has been paused.
	if (MusicPaused) S_StopMusic(true);

	// start new music for the level
	MusicPaused = false;

	// Don't start the music if loading a savegame, because the music is stored there.
	// Don't start the music if revisiting a level in a hub for the same reason.
	if (!primaryLevel->IsReentering())
	{
		primaryLevel->SetMusic();
	}
}

//==========================================================================
//
// S_PrecacheLevel
//
// Like R_PrecacheLevel, but for sounds.
//
//==========================================================================

void S_PrecacheLevel (FLevelLocals *Level)
{
	unsigned int i;

	if (GSnd && Level == primaryLevel)
	{
		for (i = 0; i < S_sfx.Size(); ++i)
		{
			S_sfx[i].bUsed = false;
		}

		AActor *actor;
		auto iterator = Level->GetThinkerIterator<AActor>();

		// Precache all sounds known to be used by the currently spawned actors.
		while ( (actor = iterator.Next()) != NULL )
		{
			IFVIRTUALPTR(actor, AActor, MarkPrecacheSounds)
			{
				VMValue params[1] = { actor };
				VMCall(func, params, 1, nullptr, 0);
			}
		}
		for (auto snd : gameinfo.PrecachedSounds)
		{
			FSoundID(snd).MarkUsed();
		}
		// Precache all extra sounds requested by this map.
		for (auto snd : primaryLevel->info->PrecacheSounds)
		{
			FSoundID(snd).MarkUsed();
		}
		// Don't unload sounds that are playing right now.
		for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			chan->SoundID.MarkUsed();
		}

		for (i = 1; i < S_sfx.Size(); ++i)
		{
			if (S_sfx[i].bUsed)
			{
				S_CacheSound (&S_sfx[i]);
			}
		}
		for (i = 1; i < S_sfx.Size(); ++i)
		{
			if (!S_sfx[i].bUsed && S_sfx[i].link == sfxinfo_t::NO_LINK)
			{
				S_UnloadSound (&S_sfx[i]);
			}
		}
	}
}

//==========================================================================
//
// S_CacheSound
//
//==========================================================================

void S_CacheSound (sfxinfo_t *sfx)
{
	if (GSnd)
	{
		if (sfx->bPlayerReserve)
		{
			return;
		}
		sfxinfo_t *orig = sfx;
		while (!sfx->bRandomHeader && sfx->link != sfxinfo_t::NO_LINK)
		{
			sfx = &S_sfx[sfx->link];
		}
		if (sfx->bRandomHeader)
		{
			S_CacheRandomSound(sfx);
		}
		else
		{
			// Since we do not know in what format the sound will be used, we have to cache both.
			FSoundLoadBuffer SoundBuffer;
			S_LoadSound(sfx, &SoundBuffer);
			S_LoadSound3D(sfx, &SoundBuffer);
			sfx->bUsed = true;
		}
	}
}

//==========================================================================
//
// S_UnloadSound
//
//==========================================================================

void S_UnloadSound (sfxinfo_t *sfx)
{
	if (sfx->data3d.isValid() && sfx->data != sfx->data3d)
		GSnd->UnloadSound(sfx->data3d);
	if (sfx->data.isValid())
		GSnd->UnloadSound(sfx->data);
	if (sfx->data.isValid() || sfx->data3d.isValid())
		DPrintf(DMSG_NOTIFY, "Unloaded sound \"%s\" (%td)\n", sfx->name.GetChars(), sfx - &S_sfx[0]);
	sfx->data.Clear();
	sfx->data3d.Clear();
}

//==========================================================================
//
// S_GetChannel
//
// Returns a free channel for the system sound interface.
//
//==========================================================================

FISoundChannel *S_GetChannel(void *syschan)
{
	FSoundChan *chan;

	if (FreeChannels != NULL)
	{
		chan = FreeChannels;
		S_UnlinkChannel(chan);
	}
	else
	{
		chan = new FSoundChan;
		memset(chan, 0, sizeof(*chan));
	}
	S_LinkChannel(chan, &Channels);
	chan->SysChannel = syschan;
	return chan;
}

//==========================================================================
//
// S_ReturnChannel
//
// Returns a channel to the free pool.
//
//==========================================================================

void S_ReturnChannel(FSoundChan *chan)
{
	S_UnlinkChannel(chan);
	memset(chan, 0, sizeof(*chan));
	S_LinkChannel(chan, &FreeChannels);
}

//==========================================================================
//
// S_UnlinkChannel
//
//==========================================================================

void S_UnlinkChannel(FSoundChan *chan)
{
	*(chan->PrevChan) = chan->NextChan;
	if (chan->NextChan != NULL)
	{
		chan->NextChan->PrevChan = chan->PrevChan;
	}
}

//==========================================================================
//
// S_LinkChannel
//
//==========================================================================

void S_LinkChannel(FSoundChan *chan, FSoundChan **head)
{
	chan->NextChan = *head;
	if (chan->NextChan != NULL)
	{
		chan->NextChan->PrevChan = &chan->NextChan;
	}
	*head = chan;
	chan->PrevChan = head;
}

// [RH] Split S_StartSoundAtVolume into multiple parts so that sounds can
//		be specified both by id and by name. Also borrowed some stuff from
//		Hexen and parameters from Quake.

//==========================================================================
//
// CalcPosVel
//
// Retrieves a sound's position and velocity for 3D sounds. This version
// is for an already playing sound.
//
//=========================================================================

static void CalcPosVel(FSoundChan *chan, FVector3 *pos, FVector3 *vel)
{
	CalcPosVel(chan->SourceType, chan->Actor, chan->Sector, chan->Poly, chan->Point,
		chan->EntChannel, chan->ChanFlags, pos, vel);
}

//=========================================================================
//
// CalcPosVel
//
// This version is for sounds that haven't started yet so have no channel.
//
//=========================================================================

static void CalcPosVel(int type, const AActor *actor, const sector_t *sector,
	const FPolyObj *poly, const float pt[3], int channum, int chanflags, FVector3 *pos, FVector3 *vel)
{
	if (pos != NULL)
	{
		DVector3 listenpos;
		int pgroup;
		AActor *listener = players[consoleplayer].camera;

		if (listener != NULL)
		{
			listenpos = listener->Pos();
			*pos = listener->SoundPos();
			pgroup = listener->Sector->PortalGroup;
		}
		else
		{
			listenpos.Zero();
			pos->Zero();
			pgroup = 0;
		}

		// [BL] Moved this case out of the switch statement to make code easier
		//      on static analysis.
		if(type == SOURCE_Unattached)
		{		
			sector_t *sec = primaryLevel->PointInSector(pt[0], pt[2]);
			DVector2 disp = primaryLevel->Displacements.getOffset(pgroup, sec->PortalGroup);
			pos->X = pt[0] - (float)disp.X;
			pos->Y = !(chanflags & CHAN_LISTENERZ) ? pt[1] : (float)listenpos.Z;
			pos->Z = pt[2] - (float)disp.Y;
		}
		else
		{
			switch (type)
			{
			case SOURCE_None:
			default:
				break;

			case SOURCE_Actor:
				//assert(actor != NULL);
				if (actor != NULL)
				{
					DVector2 disp = primaryLevel->Displacements.getOffset(pgroup, actor->Sector->PortalGroup);
					DVector3 posi = actor->Pos() - disp;
					*pos = { (float)posi.X, (float)posi.Z, (float)posi.Y };
				}
				break;

			case SOURCE_Sector:
				assert(sector != NULL);
				if (sector != NULL)
				{
					DVector2 disp = primaryLevel->Displacements.getOffset(pgroup, sector->PortalGroup);
					if (chanflags & CHAN_AREA)
					{
						// listener must be reversely offset to calculate the proper sound origin.
						CalcSectorSoundOrg(listenpos + disp, sector, channum, *pos);
						pos->X -= (float)disp.X;
						pos->Z -= (float)disp.Y;
					}
					else
					{
						
						pos->X = (float)(sector->centerspot.X - disp.X);
						pos->Z = (float)(sector->centerspot.Y - disp.Y);
						chanflags |= CHAN_LISTENERZ;
					}
				}
				break;

			case SOURCE_Polyobj:
				assert(poly != NULL);
				if (poly != NULL)
				{
					DVector2 disp = primaryLevel->Displacements.getOffset(pgroup, poly->CenterSubsector->sector->PortalGroup);
					CalcPolyobjSoundOrg(listenpos + disp, poly, *pos);
					pos->X -= (float)disp.X;
					pos->Z -= (float)disp.Y;
				}
				break;
			}

			if ((chanflags & CHAN_LISTENERZ) && players[consoleplayer].camera != NULL)
			{
				pos->Y = (float)listenpos.Z;
			}
		}
	}
	if (vel != NULL)
	{
		// Only actors maintain velocity information.
		if (type == SOURCE_Actor && actor != NULL)
		{
			vel->X = float(actor->Vel.X * TICRATE);
			vel->Y = float(actor->Vel.Z * TICRATE);
			vel->Z = float(actor->Vel.Y * TICRATE);
		}
		else
		{
			vel->Zero();
		}
	}
}

//==========================================================================
//
// ValidatePosVel
//
//==========================================================================

inline bool Validate(const float value, const float limit)
{
	return value >= -limit && value <= limit;
}

static bool Validate(const FVector3 &value, const float limit, const char *const name, const AActor *const actor)
{
	const bool valid = 
		   Validate(value.X, limit)
		&& Validate(value.Y, limit)
		&& Validate(value.Z, limit);

	if (!valid)
	{
		// Sound position and velocity have Y and Z axes swapped comparing to map coordinate system
		Printf(TEXTCOLOR_RED "Invalid sound %s " TEXTCOLOR_WHITE "(%f, %f, %f)", name, value.X, value.Z, value.Y);

		if (actor == nullptr)
		{
			Printf("\n");
		}
		else
		{
			Printf(TEXTCOLOR_RED " for actor of class " TEXTCOLOR_WHITE "%s\n", actor->GetClass()->TypeName.GetChars());
		}
	}

	return valid;
}

static bool ValidatePosVel(const AActor *actor, const FVector3 &pos, const FVector3 &vel)
{
	// The actual limit for map coordinates
	static const float POSITION_LIMIT = 1024.f * 1024.f;
	const bool valid = Validate(pos, POSITION_LIMIT, "position", actor);

	// The maximum velocity is enough to travel through entire map in one tic
	static const float VELOCITY_LIMIT = 2 * POSITION_LIMIT * TICRATE;
	return Validate(vel, VELOCITY_LIMIT, "velocity", actor) && valid;
}

static bool ValidatePosVel(const FSoundChan *const chan, const FVector3 &pos, const FVector3 &vel)
{
	return ValidatePosVel(chan->SourceType == SOURCE_Actor ? chan->Actor : nullptr, pos, vel);
}

//==========================================================================
//
// CalcSectorSoundOrg
//
// Returns the perceived sound origin for a sector. If the listener is
// inside the sector, then the origin is their location. Otherwise, the
// origin is from the nearest wall on the sector.
//
//==========================================================================

static void CalcSectorSoundOrg(const DVector3 &listenpos, const sector_t *sec, int channum, FVector3 &pos)
{
	if (!(sec->Level->i_compatflags & COMPATF_SECTORSOUNDS))
	{
		// Are we inside the sector? If yes, the closest point is the one we're on.
		if (primaryLevel->PointInSector(listenpos.X, listenpos.Y) == sec)
		{
			pos.X = (float)listenpos.X;
			pos.Z = (float)listenpos.Y;
		}
		else
		{
			// Find the closest point on the sector's boundary lines and use
			// that as the perceived origin of the sound.
			DVector2 xy;
			sec->ClosestPoint(listenpos, xy);
			pos.X = (float)xy.X;
			pos.Z = (float)xy.Y;
		}
	}
	else
	{
		pos.X = float(sec->centerspot.X);
		pos.Z = float(sec->centerspot.Y);
	}

	// Set sound vertical position based on channel.
	if (channum == CHAN_FLOOR)
	{
		pos.Y = (float)MIN<double>(sec->floorplane.ZatPoint(listenpos), listenpos.Z);
	}
	else if (channum == CHAN_CEILING)
	{
		pos.Y = (float)MAX<double>(sec->ceilingplane.ZatPoint(listenpos), listenpos.Z);
	}
	else if (channum == CHAN_INTERIOR)
	{
		pos.Y = (float)clamp<double>(listenpos.Z, sec->floorplane.ZatPoint(listenpos), sec->ceilingplane.ZatPoint(listenpos));
	}
}

//==========================================================================
//
// CalcPolySoundOrg
//
// Returns the perceived sound origin for a polyobject. This is similar to
// CalcSectorSoundOrg, except there is no special case for being "inside"
// a polyobject, so the sound literally comes from the polyobject's walls.
// Vertical position of the sound always comes from the visible wall.
//
//==========================================================================

static void CalcPolyobjSoundOrg(const DVector3 &listenpos, const FPolyObj *poly, FVector3 &pos)
{
	side_t *side;
	sector_t *sec;

	DVector2 ppos;
	poly->ClosestPoint(listenpos, ppos, &side);
	pos.X = (float)ppos.X;
	pos.Z = (float)ppos.Y;
	sec = side->sector;
	pos.Y = (float)clamp<double>(listenpos.Z, sec->floorplane.ZatPoint(listenpos), sec->ceilingplane.ZatPoint(listenpos));
}

//==========================================================================
//
// S_StartSound
//
// 0 attenuation means full volume over whole primaryLevel->
// 0 < attenuation means to scale the distance by that amount when
//		calculating volume.
//
//==========================================================================

static FSoundChan *S_StartSound(AActor *actor, const sector_t *sec, const FPolyObj *poly,
	const FVector3 *pt, int channel, FSoundID sound_id, float volume, float attenuation,
	FRolloffInfo *forcedrolloff=NULL)
{
	sfxinfo_t *sfx;
	int chanflags;
	int basepriority;
	int org_id;
	int pitch;
	FSoundChan *chan;
	FVector3 pos, vel;
	FRolloffInfo *rolloff;
	FSoundLoadBuffer SoundBuffer;

	if (sound_id <= 0 || volume <= 0 || nosfx || nosound )
		return NULL;

	int type;

	if (actor != NULL)
	{
		type = SOURCE_Actor;
	}
	else if (sec != NULL)
	{
		type = SOURCE_Sector;
	}
	else if (poly != NULL)
	{
		type = SOURCE_Polyobj;
	}
	else if (pt != NULL)
	{
		type = SOURCE_Unattached;
	}
	else
	{
		type = SOURCE_None;
	}

	org_id = sound_id;
	chanflags = channel & ~7;
	channel &= 7;

	CalcPosVel(type, actor, sec, poly, &pt->X, channel, chanflags, &pos, &vel);

	if (!ValidatePosVel(type == SOURCE_Actor ? actor : nullptr, pos, vel))
	{
		return nullptr;
	}
	
	if (compatflags & COMPATF_MAGICSILENCE)
	{ // For people who just can't play without a silent BFG.
		channel = CHAN_WEAPON;
	}
	else if ((chanflags & CHAN_MAYBE_LOCAL) && (compatflags & COMPATF_SILENTPICKUP))
	{
		if (actor != nullptr && actor != players[consoleplayer].camera)
		{
			return nullptr;
		}
	}
	
	sfx = &S_sfx[sound_id];

	// Scale volume according to SNDINFO data.
	volume = MIN(volume * sfx->Volume, 1.f);
	if (volume <= 0)
		return NULL;

	// When resolving a link we do not want to get the NearLimit of
	// the referenced sound so some additional checks are required
	int near_limit = sfx->NearLimit;
	float limit_range = sfx->LimitRange;
	auto pitchmask = sfx->PitchMask;
	rolloff = &sfx->Rolloff;

	// Resolve player sounds, random sounds, and aliases
	while (sfx->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->bPlayerReserve)
		{
			sound_id = FSoundID(S_FindSkinnedSound (actor, sound_id));
			near_limit = S_sfx[sound_id].NearLimit;
			limit_range = S_sfx[sound_id].LimitRange;
			rolloff = &S_sfx[sound_id].Rolloff;
		}
		else if (sfx->bRandomHeader)
		{
			// Random sounds attenuate based on the original (random) sound as well as the chosen one.
			attenuation *= sfx->Attenuation;
			sound_id = FSoundID(S_PickReplacement (sound_id));
			if (near_limit < 0) 
			{
				near_limit = S_sfx[sound_id].NearLimit;
				limit_range = S_sfx[sound_id].LimitRange;
			}
			if (rolloff->MinDistance == 0)
			{
				rolloff = &S_sfx[sound_id].Rolloff;
			}
		}
		else
		{
			sound_id = FSoundID(sfx->link);
			if (near_limit < 0) 
			{
				near_limit = S_sfx[sound_id].NearLimit;
				limit_range = S_sfx[sound_id].LimitRange;
			}
			if (rolloff->MinDistance == 0)
			{
				rolloff = &S_sfx[sound_id].Rolloff;
			}
		}
		sfx = &S_sfx[sound_id];
	}

	// Attenuate the attenuation based on the sound.
	attenuation *= sfx->Attenuation;

	// The passed rolloff overrides any sound-specific rolloff.
	if (forcedrolloff != NULL && forcedrolloff->MinDistance != 0)
	{
		rolloff = forcedrolloff;
	}

	// If no valid rolloff was set, use the global default.
	if (rolloff->MinDistance == 0)
	{
		rolloff = &S_Rolloff;
	}

	// If this is a singular sound, don't play it if it's already playing.
	if (sfx->bSingular && S_CheckSingular(sound_id))
	{
		chanflags |= CHAN_EVICTED;
	}

	// If the sound is unpositioned or comes from the listener, it is
	// never limited.
	if (type == SOURCE_None || actor == players[consoleplayer].camera)
	{
		near_limit = 0;
	}

	// If this sound doesn't like playing near itself, don't play it if
	// that's what would happen.
	if (near_limit > 0 && S_CheckSoundLimit(sfx, pos, near_limit, limit_range, actor, channel))
	{
		chanflags |= CHAN_EVICTED;
	}

	// If the sound is blocked and not looped, return now. If the sound
	// is blocked and looped, pretend to play it so that it can
	// eventually play for real.
	if ((chanflags & (CHAN_EVICTED | CHAN_LOOP)) == CHAN_EVICTED)
	{
		return NULL;
	}

	// Make sure the sound is loaded.
	sfx = S_LoadSound(sfx, &SoundBuffer);

	// The empty sound never plays.
	if (sfx->lumpnum == sfx_empty)
	{
		return NULL;
	}

	// Select priority.
	if (type == SOURCE_None || actor == players[consoleplayer].camera)
	{
		basepriority = 80;
	}
	else
	{
		basepriority = 0;
	}

	int seen = 0;
	if (actor != NULL && channel == CHAN_AUTO)
	{
		// Select a channel that isn't already playing something.
		// Try channel 0 first, then travel from channel 7 down.
		if (!S_IsChannelUsed(actor, 0, &seen))
		{
			channel = 0;
		}
		else
		{
			for (channel = 7; channel > 0; --channel)
			{
				if (!S_IsChannelUsed(actor, channel, &seen))
				{
					break;
				}
			}
			if (channel == 0)
			{ // Crap. No free channels.
				return NULL;
			}
		}
	}

	// If this actor is already playing something on the selected channel, stop it.
	if (type != SOURCE_None && ((actor == NULL && channel != CHAN_AUTO) || (actor != NULL && S_IsChannelUsed(actor, channel, &seen))))
	{
		for (chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->SourceType == type && chan->EntChannel == channel)
			{
				bool foundit;

				switch (type)
				{
				case SOURCE_Actor:		foundit = (chan->Actor == actor);	break;
				case SOURCE_Sector:		foundit = (chan->Sector == sec);	break;
				case SOURCE_Polyobj:	foundit = (chan->Poly == poly);		break;
				case SOURCE_Unattached:	foundit = (chan->Point[0] == pt->X && chan->Point[2] == pt->Z && chan->Point[1] == pt->Y);		break;
				default:				foundit = false;					break;
				}
				if (foundit)
				{
					S_StopChannel(chan);
					break;
				}
			}
		}
	}

	// sound is paused and a non-looped sound is being started.
	// Such a sound would play right after unpausing which wouldn't sound right.
	if (!(chanflags & CHAN_LOOP) && !(chanflags & (CHAN_UI|CHAN_NOPAUSE)) && SoundPaused)
	{
		return NULL;
	}

	// Vary the sfx pitches.
	if (pitchmask != 0)
	{
		pitch = NORM_PITCH - (M_Random() & pitchmask) + (M_Random() & pitchmask);
	}
	else
	{
		pitch = NORM_PITCH;
	}

	if (chanflags & CHAN_EVICTED)
	{
		chan = NULL;
	}
	else 
	{
		int startflags = 0;
		if (chanflags & CHAN_LOOP) startflags |= SNDF_LOOP;
		if (chanflags & CHAN_AREA) startflags |= SNDF_AREA;
		if (chanflags & (CHAN_UI|CHAN_NOPAUSE)) startflags |= SNDF_NOPAUSE;
		if (chanflags & CHAN_UI) startflags |= SNDF_NOREVERB;

		if (attenuation > 0)
		{
            S_LoadSound3D(sfx, &SoundBuffer);
			SoundListener listener;
			S_SetListener(listener, players[consoleplayer].camera);
            chan = (FSoundChan*)GSnd->StartSound3D (sfx->data3d, &listener, float(volume), rolloff, float(attenuation), pitch, basepriority, pos, vel, channel, startflags, NULL);
		}
		else
		{
			chan = (FSoundChan*)GSnd->StartSound (sfx->data, float(volume), pitch, startflags, NULL);
		}
	}
	if (chan == NULL && (chanflags & CHAN_LOOP))
	{
		chan = (FSoundChan*)S_GetChannel(NULL);
		GSnd->MarkStartTime(chan);
		chanflags |= CHAN_EVICTED;
	}
	if (attenuation > 0)
	{
		chanflags |= CHAN_IS3D | CHAN_JUSTSTARTED;
	}
	else
	{
		chanflags |= CHAN_LISTENERZ | CHAN_JUSTSTARTED;
	}
	if (chan != NULL)
	{
		chan->SoundID = sound_id;
		chan->OrgID = FSoundID(org_id);
		chan->EntChannel = channel;
		chan->Volume = float(volume);
		chan->ChanFlags |= chanflags;
		chan->NearLimit = near_limit;
		chan->LimitRange = limit_range;
		chan->Pitch = pitch;
		chan->Priority = basepriority;
		chan->DistanceScale = float(attenuation);
		chan->SourceType = type;
		switch (type)
		{
		case SOURCE_Actor:		chan->Actor = actor;	break;
		case SOURCE_Sector:		chan->Sector = sec;		break;
		case SOURCE_Polyobj:	chan->Poly = poly;		break;
		case SOURCE_Unattached:	chan->Point[0] = pt->X; chan->Point[1] = pt->Y; chan->Point[2] = pt->Z;	break;
		default:										break;
		}
	}
	return chan;
}

//==========================================================================
//
// S_RestartSound
//
// Attempts to restart looping sounds that were evicted from their channels.
//
//==========================================================================

void S_RestartSound(FSoundChan *chan)
{
	assert(chan->ChanFlags & CHAN_EVICTED);

	FSoundChan *ochan;
	sfxinfo_t *sfx = &S_sfx[chan->SoundID];
	FSoundLoadBuffer SoundBuffer;

	// If this is a singular sound, don't play it if it's already playing.
	if (sfx->bSingular && S_CheckSingular(chan->SoundID))
		return;

	sfx = S_LoadSound(sfx, &SoundBuffer);

	// The empty sound never plays.
	if (sfx->lumpnum == sfx_empty)
	{
		return;
	}

	int oldflags = chan->ChanFlags;

	int startflags = 0;
	if (chan->ChanFlags & CHAN_LOOP) startflags |= SNDF_LOOP;
	if (chan->ChanFlags & CHAN_AREA) startflags |= SNDF_AREA;
	if (chan->ChanFlags & (CHAN_UI|CHAN_NOPAUSE)) startflags |= SNDF_NOPAUSE;
	if (chan->ChanFlags & CHAN_ABSTIME) startflags |= SNDF_ABSTIME;

	if (chan->ChanFlags & CHAN_IS3D)
	{
		FVector3 pos, vel;

		CalcPosVel(chan, &pos, &vel);

		if (!ValidatePosVel(chan, pos, vel))
		{
			return;
		}

		// If this sound doesn't like playing near itself, don't play it if
		// that's what would happen.
		if (chan->NearLimit > 0 && S_CheckSoundLimit(&S_sfx[chan->SoundID], pos, chan->NearLimit, chan->LimitRange, NULL, 0))
		{
			return;
		}

        S_LoadSound3D(sfx, &SoundBuffer);
		SoundListener listener;
		S_SetListener(listener, players[consoleplayer].camera);

		chan->ChanFlags &= ~(CHAN_EVICTED|CHAN_ABSTIME);
        ochan = (FSoundChan*)GSnd->StartSound3D(sfx->data3d, &listener, chan->Volume, &chan->Rolloff, chan->DistanceScale, chan->Pitch,
            chan->Priority, pos, vel, chan->EntChannel, startflags, chan);
	}
	else
	{
		chan->ChanFlags &= ~(CHAN_EVICTED|CHAN_ABSTIME);
		ochan = (FSoundChan*)GSnd->StartSound(sfx->data, chan->Volume, chan->Pitch, startflags, chan);
	}
	assert(ochan == NULL || ochan == chan);
	if (ochan == NULL)
	{
		chan->ChanFlags = oldflags;
	}
}

//==========================================================================
//
// S_Sound - Unpositioned version
//
//==========================================================================

void S_Sound (int channel, FSoundID sound_id, float volume, float attenuation)
{
	S_StartSound (NULL, NULL, NULL, NULL, channel, sound_id, volume, attenuation);
}

DEFINE_ACTION_FUNCTION(DObject, S_Sound)
{
	PARAM_PROLOGUE;
	PARAM_SOUND(id);
	PARAM_INT(channel);
	PARAM_FLOAT(volume);
	PARAM_FLOAT(attn);
	S_Sound(channel, id, static_cast<float>(volume), static_cast<float>(attn));
	return 0;
}

//==========================================================================
//
// S_Sound - An actor is source
//
//==========================================================================

void S_Sound (AActor *ent, int channel, FSoundID sound_id, float volume, float attenuation)
{
	if (ent == nullptr || ent->Sector->Flags & SECF_SILENT || ent->Level != primaryLevel)
		return;
	S_StartSound (ent, nullptr, nullptr, nullptr, channel, sound_id, volume, attenuation);
}

//==========================================================================
//
// S_SoundMinMaxDist - An actor is source
//
// Attenuation is specified as min and max distances, rather than a scalar.
//
//==========================================================================

void S_SoundMinMaxDist(AActor *ent, int channel, FSoundID sound_id, float volume, float mindist, float maxdist)
{
	if (ent == nullptr || ent->Sector->Flags & SECF_SILENT || ent->Level != primaryLevel)
		return;

	FRolloffInfo rolloff;

	rolloff.RolloffType = ROLLOFF_Linear;
	rolloff.MinDistance = mindist;
	rolloff.MaxDistance = maxdist;
	S_StartSound(ent, nullptr, nullptr, nullptr, channel, sound_id, volume, 1, &rolloff);
}

//==========================================================================
//
// S_Sound - A polyobject is source
//
//==========================================================================

void S_Sound (const FPolyObj *poly, int channel, FSoundID sound_id, float volume, float attenuation)
{
	if (poly->Level != primaryLevel) return;
	S_StartSound (nullptr, nullptr, poly, nullptr, channel, sound_id, volume, attenuation);
}

//==========================================================================
//
// S_Sound - A point is source
//
//==========================================================================

void S_Sound(FLevelLocals *Level, const DVector3 &pos, int channel, FSoundID sound_id, float volume, float attenuation)
{
	if (Level != primaryLevel) return;
	// The sound system switches Y and Z around.
	FVector3 p((float)pos.X, (float)pos.Z, (float)pos.Y);
	S_StartSound (nullptr, nullptr, nullptr, &p, channel, sound_id, volume, attenuation);
}

//==========================================================================
//
// S_Sound - An entire sector is source
//
//==========================================================================

void S_Sound (const sector_t *sec, int channel, FSoundID sfxid, float volume, float attenuation)
{
	if (sec->Level != primaryLevel) return;
	S_StartSound (nullptr, sec, nullptr, nullptr, channel, sfxid, volume, attenuation);
}

//==========================================================================
//
// S_PlaySound - Subfunction used by ACS and DECORATE
//
// Has a local parameter to make the sound audible only to the source
//
//==========================================================================

void S_PlaySound(AActor *a, int chan, FSoundID sid, float vol, float atten, bool local)
{
	if (a == nullptr || a->Sector->Flags & SECF_SILENT || a->Level != primaryLevel)
		return;

	if (!local)
	{
		S_Sound(a, chan, sid, vol, atten);
	}
	else
	{
		if (a->CheckLocalView())
		{
			S_Sound(chan, sid, vol, ATTN_NONE);
		}
	}
}

void A_PlaySound(AActor *self, int soundid, int channel, double volume, int looping, double attenuation, int local)
{
	if (!looping)
	{
		if (!(channel & CHAN_NOSTOP) || !S_IsActorPlayingSomething(self, channel & 7, soundid))
		{
			S_PlaySound(self, channel, soundid, (float)volume, (float)attenuation, local);
		}
	}
	else
	{
		if (!S_IsActorPlayingSomething(self, channel & 7, soundid))
		{
			S_PlaySound(self, channel | CHAN_LOOP, soundid, (float)volume, (float)attenuation, local);
		}
	}
}

//==========================================================================
//
// S_LoadSound
//
// Returns a pointer to the sfxinfo with the actual sound data.
//
//==========================================================================

sfxinfo_t *S_LoadSound(sfxinfo_t *sfx, FSoundLoadBuffer *pBuffer)
{
	if (GSnd->IsNull()) return sfx;

	while (!sfx->data.isValid())
	{
		unsigned int i;

		// If the sound doesn't exist, replace it with the empty sound.
		if (sfx->lumpnum == -1)
		{
			sfx->lumpnum = sfx_empty;
		}
		
		// See if there is another sound already initialized with this lump. If so,
		// then set this one up as a link, and don't load the sound again.
		for (i = 0; i < S_sfx.Size(); i++)
		{
			if (S_sfx[i].data.isValid() && S_sfx[i].link == sfxinfo_t::NO_LINK && S_sfx[i].lumpnum == sfx->lumpnum)
			{
				DPrintf (DMSG_NOTIFY, "Linked %s to %s (%d)\n", sfx->name.GetChars(), S_sfx[i].name.GetChars(), i);
				sfx->link = i;
				// This is necessary to avoid using the rolloff settings of the linked sound if its
				// settings are different.
				if (sfx->Rolloff.MinDistance == 0) sfx->Rolloff = S_Rolloff;
				return &S_sfx[i];
			}
		}

		DPrintf(DMSG_NOTIFY, "Loading sound \"%s\" (%td)\n", sfx->name.GetChars(), sfx - &S_sfx[0]);

		int size = Wads.LumpLength(sfx->lumpnum);
		if (size > 8)
		{
			auto wlump = Wads.OpenLumpReader(sfx->lumpnum);
			auto sfxdata = wlump.Read(size);
			int32_t dmxlen = LittleLong(((int32_t *)sfxdata.Data())[1]);
            std::pair<SoundHandle,bool> snd;

			// If the sound is voc, use the custom loader.
			if (strncmp ((const char *)sfxdata.Data(), "Creative Voice File", 19) == 0)
			{
				snd = GSnd->LoadSoundVoc(sfxdata.Data(), size);
			}
			// If the sound is raw, just load it as such.
			else if (sfx->bLoadRAW)
			{
				snd = GSnd->LoadSoundRaw(sfxdata.Data(), size, sfx->RawRate, 1, 8, sfx->LoopStart);
			}
			// Otherwise, try the sound as DMX format.
			else if (((uint8_t *)sfxdata.Data())[0] == 3 && ((uint8_t *)sfxdata.Data())[1] == 0 && dmxlen <= size - 8)
			{
				int frequency = LittleShort(((uint16_t *)sfxdata.Data())[1]);
				if (frequency == 0) frequency = 11025;
				snd = GSnd->LoadSoundRaw(sfxdata.Data()+8, dmxlen, frequency, 1, 8, sfx->LoopStart);
			}
			// If that fails, let the sound system try and figure it out.
			else
			{
				snd = GSnd->LoadSound(sfxdata.Data(), size, false, pBuffer);
			}

            sfx->data = snd.first;
            if(snd.second)
                sfx->data3d = sfx->data;
		}

		if (!sfx->data.isValid())
		{
			if (sfx->lumpnum != sfx_empty)
			{
				sfx->lumpnum = sfx_empty;
				continue;
			}
		}
		break;
	}
	return sfx;
}

static void S_LoadSound3D(sfxinfo_t *sfx, FSoundLoadBuffer *pBuffer)
{
    if (GSnd->IsNull()) return;

    if(sfx->data3d.isValid())
        return;

    DPrintf(DMSG_NOTIFY, "Loading monoized sound \"%s\" (%td)\n", sfx->name.GetChars(), sfx - &S_sfx[0]);

	std::pair<SoundHandle, bool> snd;

	if (pBuffer->mBuffer.Size() > 0)
	{
		snd = GSnd->LoadSoundBuffered(pBuffer, true);
	}
	else
	{
		int size = Wads.LumpLength(sfx->lumpnum);
		if (size <= 8) return;

		auto wlump = Wads.OpenLumpReader(sfx->lumpnum);
		auto sfxdata = wlump.Read(size);
		int32_t dmxlen = LittleLong(((int32_t *)sfxdata.Data())[1]);

		// If the sound is voc, use the custom loader.
		if (strncmp((const char *)sfxdata.Data(), "Creative Voice File", 19) == 0)
		{
			snd = GSnd->LoadSoundVoc(sfxdata.Data(), size, true);
		}
		// If the sound is raw, just load it as such.
		else if (sfx->bLoadRAW)
		{
			snd = GSnd->LoadSoundRaw(sfxdata.Data(), size, sfx->RawRate, 1, 8, sfx->LoopStart, true);
		}
		// Otherwise, try the sound as DMX format.
		else if (((uint8_t *)sfxdata.Data())[0] == 3 && ((uint8_t *)sfxdata.Data())[1] == 0 && dmxlen <= size - 8)
		{
			int frequency = LittleShort(((uint16_t *)sfxdata.Data())[1]);
			if (frequency == 0) frequency = 11025;
			snd = GSnd->LoadSoundRaw(sfxdata.Data() + 8, dmxlen, frequency, 1, 8, sfx->LoopStart, -1, true);
		}
		// If that fails, let the sound system try and figure it out.
		else
		{
			snd = GSnd->LoadSound(sfxdata.Data(), size, true, pBuffer);
		}
	}

	sfx->data3d = snd.first;
}

//==========================================================================
//
// S_CheckSingular
//
// Returns true if a copy of this sound is already playing.
//
//==========================================================================

bool S_CheckSingular(int sound_id)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->OrgID == sound_id)
		{
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// S_CheckSoundLimit
//
// Limits the number of nearby copies of a sound that can play near
// each other. If there are NearLimit instances of this sound already
// playing within sqrt(limit_range) (typically 256 units) of the new sound, the
// new sound will not start.
//
// If an actor is specified, and it is already playing the same sound on
// the same channel, this sound will not be limited. In this case, we're
// restarting an already playing sound, so there's no need to limit it.
//
// Returns true if the sound should not play.
//
//==========================================================================

bool S_CheckSoundLimit(sfxinfo_t *sfx, const FVector3 &pos, int near_limit, float limit_range,
	AActor *actor, int channel)
{
	FSoundChan *chan;
	int count;
	
	for (chan = Channels, count = 0; chan != NULL && count < near_limit; chan = chan->NextChan)
	{
		if (!(chan->ChanFlags & CHAN_EVICTED) && &S_sfx[chan->SoundID] == sfx)
		{
			FVector3 chanorigin;

			if (actor != NULL && chan->EntChannel == channel &&
				chan->SourceType == SOURCE_Actor && chan->Actor == actor)
			{ // We are restarting a playing sound. Always let it play.
				return false;
			}

			CalcPosVel(chan, &chanorigin, NULL);
			if ((chanorigin - pos).LengthSquared() <= limit_range)
			{
				count++;
			}
		}
	}
	return count >= near_limit;
}

//==========================================================================
//
// S_StopSound
//
// Stops an unpositioned sound from playing on a specific channel.
//
//==========================================================================

void S_StopSound (int channel)
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_None &&
			(chan->EntChannel == channel || (compatflags & COMPATF_MAGICSILENCE)))
		{
			S_StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single actor from playing on a specific channel.
//
//==========================================================================

void S_StopSound (AActor *actor, int channel)
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_Actor &&
			chan->Actor == actor &&
			(chan->EntChannel == channel || (compatflags & COMPATF_MAGICSILENCE)))
		{
			S_StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single sector from playing on a specific channel.
//
//==========================================================================

void S_StopSound (const sector_t *sec, int channel)
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_Sector &&
			chan->Sector == sec &&
			(chan->EntChannel == channel || (compatflags & COMPATF_MAGICSILENCE)))
		{
			S_StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single polyobject from playing on a specific channel.
//
//==========================================================================

void S_StopSound (const FPolyObj *poly, int channel)
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_Polyobj &&
			chan->Poly == poly &&
			(chan->EntChannel == channel || (compatflags & COMPATF_MAGICSILENCE)))
		{
			S_StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopAllChannels
//
//==========================================================================

void S_StopAllChannels ()
{
	for (auto Level : AllLevels())
	{
		SN_StopAllSequences(Level);
	}

	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		S_StopChannel(chan);
		chan = next;
	}
	GSnd->UpdateSounds();
}

//==========================================================================
//
// S_RelinkSound
//
// Moves all the sounds from one thing to another. If the destination is
// NULL, then the sound becomes a positioned sound.
//==========================================================================

void S_RelinkSound (AActor *from, AActor *to)
{
	if (from == NULL)
		return;

	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == SOURCE_Actor && chan->Actor == from)
		{
			if (to != NULL)
			{
				chan->Actor = to;
			}
			else if (!(chan->ChanFlags & CHAN_LOOP) && !(compatflags2 & COMPATF2_SOUNDCUTOFF))
			{
				chan->Actor = NULL;
				chan->SourceType = SOURCE_Unattached;
				FVector3 p = from->SoundPos();
				chan->Point[0] = p.X;
				chan->Point[1] = p.Y;
				chan->Point[2] = p.Z;
			}
			else
			{
				S_StopChannel(chan);
			}
		}
		chan = next;
	}
}


//==========================================================================
//
// S_ChangeSoundVolume
//
//==========================================================================

void S_ChangeSoundVolume(AActor *actor, int channel, double dvolume)
{
	float volume = float(dvolume);
	// don't let volume get out of bounds
	if (volume < 0.0)
		volume = 0.0;
	else if (volume > 1.0)
		volume = 1.0;

	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == SOURCE_Actor &&
			chan->Actor == actor &&
			(chan->EntChannel == channel || (compatflags & COMPATF_MAGICSILENCE)))
		{
			GSnd->ChannelVolume(chan, volume);
			chan->Volume = volume;
			return;
		}
	}
	return;
}

//==========================================================================
//
// S_GetSoundPlayingInfo
//
// Is a sound being played by a specific emitter?
//==========================================================================

bool S_GetSoundPlayingInfo (const AActor *actor, int sound_id)
{
	if (sound_id > 0)
	{
		for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->OrgID == sound_id &&
				chan->SourceType == SOURCE_Actor &&
				chan->Actor == actor)
			{
				return true;
			}
		}
	}
	return false;
}

bool S_GetSoundPlayingInfo (const sector_t *sec, int sound_id)
{
	if (sound_id > 0)
	{
		for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->OrgID == sound_id &&
				chan->SourceType == SOURCE_Sector &&
				chan->Sector == sec)
			{
				return true;
			}
		}
	}
	return false;
}

bool S_GetSoundPlayingInfo (const FPolyObj *poly, int sound_id)
{
	if (sound_id > 0)
	{
		for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->OrgID == sound_id &&
				chan->SourceType == SOURCE_Polyobj &&
				chan->Poly == poly)
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_IsChannelUsed
//
// Returns true if the channel is in use. Also fills in a bitmask of
// channels seen while scanning for this one, to make searching for unused
// channels faster. Initialize seen to 0 for the first call.
//
//==========================================================================

static bool S_IsChannelUsed(AActor *actor, int channel, int *seen)
{
	if (*seen & (1 << channel))
	{
		return true;
	}
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == SOURCE_Actor && chan->Actor == actor)
		{
			*seen |= 1 << chan->EntChannel;
			if (chan->EntChannel == channel)
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_IsActorPlayingSomething
//
//==========================================================================

bool S_IsActorPlayingSomething (AActor *actor, int channel, int sound_id)
{
	if (compatflags & COMPATF_MAGICSILENCE)
	{
		channel = 0;
	}

	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == SOURCE_Actor && chan->Actor == actor)
		{
			if (channel == 0 || chan->EntChannel == channel)
			{
				return sound_id <= 0 || chan->OrgID == sound_id;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_PauseSound
//
// Stop music and sound effects, during game PAUSE.
//==========================================================================

void S_PauseSound (bool notmusic, bool notsfx)
{
	if (!notmusic && mus_playing.handle && !MusicPaused)
	{
		mus_playing.handle->Pause();
		MusicPaused = true;
	}
	if (!notsfx)
	{
		SoundPaused = true;
		GSnd->SetSfxPaused (true, 0);
	}
}

DEFINE_ACTION_FUNCTION(DObject, S_PauseSound)
{
	PARAM_PROLOGUE;
	PARAM_BOOL(notmusic);
	PARAM_BOOL(notsfx);
	S_PauseSound(notmusic, notsfx);
	return 0;
}

//==========================================================================
//
// S_ResumeSound
//
// Resume music and sound effects, after game PAUSE.
//==========================================================================

void S_ResumeSound (bool notsfx)
{
	if (mus_playing.handle && MusicPaused)
	{
		mus_playing.handle->Resume();
		MusicPaused = false;
	}
	if (!notsfx)
	{
		SoundPaused = false;
		GSnd->SetSfxPaused (false, 0);
	}
}

DEFINE_ACTION_FUNCTION(DObject, S_ResumeSound)
{
	PARAM_PROLOGUE;
	PARAM_BOOL(notsfx);
	S_ResumeSound(notsfx);
	return 0;
}

//==========================================================================
//
// S_SetSoundPaused
//
// Called with state non-zero when the app is active, zero when it isn't.
//
//==========================================================================

void S_SetSoundPaused (int state)
{
	if (state)
	{
		if (paused == 0)
		{
			S_ResumeSound(true);
			if (GSnd != NULL)
			{
				GSnd->SetInactive(SoundRenderer::INACTIVE_Active);
			}
		}
	}
	else
	{
		if (paused == 0)
		{
			S_PauseSound(false, true);
			if (GSnd !=  NULL)
			{
				GSnd->SetInactive(gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL ?
					SoundRenderer::INACTIVE_Complete :
					SoundRenderer::INACTIVE_Mute);
			}
		}
	}
	if (!netgame
#ifdef _DEBUG
		&& !demoplayback
#endif
		)
	{
		pauseext = !state;
	}
}

//==========================================================================
//
// S_EvictAllChannels
//
// Forcibly evicts all channels so that there are none playing, but all
// information needed to restart them is retained.
//
//==========================================================================

void S_EvictAllChannels()
{
	FSoundChan *chan, *next;

	for (chan = Channels; chan != NULL; chan = next)
	{
		next = chan->NextChan;

		if (!(chan->ChanFlags & CHAN_EVICTED))
		{
			chan->ChanFlags |= CHAN_EVICTED;
			if (chan->SysChannel != NULL)
			{
				if (!(chan->ChanFlags & CHAN_ABSTIME))
				{
					chan->StartTime.AsOne = GSnd ? GSnd->GetPosition(chan) : 0;
					chan->ChanFlags |= CHAN_ABSTIME;
				}
				S_StopChannel(chan);
			}
//			assert(chan->NextChan == next);
		}
	}
}

//==========================================================================
//
// S_RestoreEvictedChannel
//
// Recursive helper for S_RestoreEvictedChannels().
//
//==========================================================================

void S_RestoreEvictedChannel(FSoundChan *chan)
{
	if (chan == NULL)
	{
		return;
	}
	S_RestoreEvictedChannel(chan->NextChan);
	if (chan->ChanFlags & CHAN_EVICTED)
	{
		S_RestartSound(chan);
		if (!(chan->ChanFlags & CHAN_LOOP))
		{
			if (chan->ChanFlags & CHAN_EVICTED)
			{ // Still evicted and not looping? Forget about it.
				S_ReturnChannel(chan);
			}
			else if (!(chan->ChanFlags & CHAN_JUSTSTARTED))
			{ // Should this sound become evicted again, it's okay to forget about it.
				chan->ChanFlags |= CHAN_FORGETTABLE;
			}
		}
	}
	else if (chan->SysChannel == NULL && (chan->ChanFlags & (CHAN_FORGETTABLE | CHAN_LOOP)) == CHAN_FORGETTABLE)
	{
		S_ReturnChannel(chan);
	}
}

//==========================================================================
//
// S_RestoreEvictedChannels
//
// Restarts as many evicted channels as possible. Any channels that could
// not be started and are not looping are moved to the free pool.
//
//==========================================================================

void S_RestoreEvictedChannels()
{
	// Restart channels in the same order they were originally played.
	S_RestoreEvictedChannel(Channels);
}

//==========================================================================
//
// S_UpdateSounds
//
// Updates music & sounds
//==========================================================================

void S_UpdateSounds (AActor *listenactor)
{
	FVector3 pos, vel;
	SoundListener listener;

	I_UpdateMusic();

	// [RH] Update music and/or playlist. IsPlaying() must be called
	// to attempt to reconnect to broken net streams and to advance the
	// playlist when the current song finishes.
	if (mus_playing.handle != NULL &&
		!mus_playing.handle->IsPlaying() &&
		PlayList)
	{
		PlayList->Advance();
		S_ActivatePlayList(false);
	}

	// should never happen
	S_SetListener(listener, listenactor);

	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if ((chan->ChanFlags & (CHAN_EVICTED | CHAN_IS3D)) == CHAN_IS3D)
		{
			CalcPosVel(chan, &pos, &vel);

			if (ValidatePosVel(chan, pos, vel))
			{
				GSnd->UpdateSoundParams3D(&listener, chan, !!(chan->ChanFlags & CHAN_AREA), pos, vel);
			}
		}
		chan->ChanFlags &= ~CHAN_JUSTSTARTED;
	}

	for (auto Level : AllLevels())
	{
		SN_UpdateActiveSequences(Level);
	}

	GSnd->UpdateListener(&listener);
	GSnd->UpdateSounds();

	if (primaryLevel->time >= RestartEvictionsAt)
	{
		RestartEvictionsAt = 0;
		S_RestoreEvictedChannels();
	}
}

//==========================================================================
//
// Sets the internal listener structure
//
//==========================================================================

static void S_SetListener(SoundListener &listener, AActor *listenactor)
{
	if (listenactor != NULL)
	{
		listener.angle = (float)listenactor->Angles.Yaw.Radians();
		/*
		listener.velocity.X = listenactor->vel.x * (TICRATE/65536.f);
		listener.velocity.Y = listenactor->vel.z * (TICRATE/65536.f);
		listener.velocity.Z = listenactor->vel.y * (TICRATE/65536.f);
		*/
		listener.velocity.Zero();
		listener.position = listenactor->SoundPos();
		listener.underwater = listenactor->waterlevel == 3;
		assert(primaryLevel->Zones.Size() > listenactor->Sector->ZoneNumber);
		listener.Environment = primaryLevel->Zones[listenactor->Sector->ZoneNumber].Environment;
		listener.valid = true;
	}
	else
	{
		listener.angle = 0;
		listener.position.Zero();
		listener.velocity.Zero();
		listener.underwater = false;
		listener.Environment = NULL;
		listener.valid = false;
	}
}



//==========================================================================
//
// S_GetRolloff
//
//==========================================================================

float S_GetRolloff(FRolloffInfo *rolloff, float distance, bool logarithmic)
{
	if (rolloff == NULL)
	{
		return 0;
	}

	if (distance <= rolloff->MinDistance)
	{
		return 1;
	}
	if (rolloff->RolloffType == ROLLOFF_Log)
	{ // Logarithmic rolloff has no max distance where it goes silent.
		return rolloff->MinDistance / (rolloff->MinDistance + rolloff->RolloffFactor * (distance - rolloff->MinDistance));
	}
	if (distance >= rolloff->MaxDistance)
	{
		return 0;
	}

	float volume = (rolloff->MaxDistance - distance) / (rolloff->MaxDistance - rolloff->MinDistance);
	if (rolloff->RolloffType == ROLLOFF_Custom && S_SoundCurve.Size() > 0)
	{
		volume = S_SoundCurve[int(S_SoundCurve.Size() * (1 - volume))] / 127.f;
	}
	if (logarithmic)
	{
		if (rolloff->RolloffType == ROLLOFF_Linear)
		{
			return volume;
		}
		else
		{
			return float((powf(10.f, volume) - 1.) / 9.);
		}
	}
	else
	{
		if (rolloff->RolloffType == ROLLOFF_Linear)
		{
			return float(log10(9. * volume + 1.));
		}
		else
		{
			return volume;
		}
	}
}


//==========================================================================
//
// S_ChannelEnded (callback for sound interface code)
//
//==========================================================================

void S_ChannelEnded(FISoundChannel *ichan)
{
	FSoundChan *schan = static_cast<FSoundChan*>(ichan);
	bool evicted;

	if (schan != NULL)
	{
		// If the sound was stopped with GSnd->StopSound(), then we know
		// it wasn't evicted. Otherwise, if it's looping, it must have
		// been evicted. If it's not looping, then it was evicted if it
		// didn't reach the end of its playback.
		if (schan->ChanFlags & CHAN_FORGETTABLE)
		{
			evicted = false;
		}
		else if (schan->ChanFlags & (CHAN_LOOP | CHAN_EVICTED))
		{
			evicted = true;
		}
		else
		{ 
			unsigned int pos = GSnd->GetPosition(schan);
			unsigned int len = GSnd->GetSampleLength(S_sfx[schan->SoundID].data);
			if (pos == 0)
			{
				evicted = !!(schan->ChanFlags & CHAN_JUSTSTARTED);
			}
			else
			{
				evicted = (pos < len);
			}
		}
		if (!evicted)
		{
			S_ReturnChannel(schan);
		}
		else
		{
			schan->ChanFlags |= CHAN_EVICTED;
			schan->SysChannel = NULL;
		}
	}
}

//==========================================================================
//
// S_ChannelVirtualChanged (callback for sound interface code)
//
//==========================================================================

void S_ChannelVirtualChanged(FISoundChannel *ichan, bool is_virtual)
{
	FSoundChan *schan = static_cast<FSoundChan*>(ichan);
	if (is_virtual)
	{
		schan->ChanFlags |= CHAN_VIRTUAL;
	}
	else
	{
		schan->ChanFlags &= ~CHAN_VIRTUAL;
	}
}

//==========================================================================
//
// S_StopChannel
//
//==========================================================================

void S_StopChannel(FSoundChan *chan)
{
	if (chan == NULL)
		return;

	if (chan->SysChannel != NULL)
	{
		// S_EvictAllChannels() will set the CHAN_EVICTED flag to indicate
		// that it wants to keep all the channel information around.
		if (!(chan->ChanFlags & CHAN_EVICTED))
		{
			chan->ChanFlags |= CHAN_FORGETTABLE;
			if (chan->SourceType == SOURCE_Actor)
			{
				chan->Actor = NULL;
			}
		}
		GSnd->StopChannel(chan);
	}
	else
	{
		S_ReturnChannel(chan);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

static FSerializer &Serialize(FSerializer &arc, const char *key, FSoundChan &chan, FSoundChan *def)
{
	if (arc.BeginObject(key))
	{
		arc("sourcetype", chan.SourceType)
			("soundid", chan.SoundID)
			("orgid", chan.OrgID)
			("volume", chan.Volume)
			("distancescale", chan.DistanceScale)
			("pitch", chan.Pitch)
			("chanflags", chan.ChanFlags)
			("entchannel", chan.EntChannel)
			("priority", chan.Priority)
			("nearlimit", chan.NearLimit)
			("starttime", chan.StartTime.AsOne)
			("rolloftype", chan.Rolloff.RolloffType)
			("rolloffmin", chan.Rolloff.MinDistance)
			("rolloffmax", chan.Rolloff.MaxDistance)
			("limitrange", chan.LimitRange);

		switch (chan.SourceType)
		{
		case SOURCE_None:										break;
		case SOURCE_Actor:		arc("actor", chan.Actor);		break;
		case SOURCE_Sector:		arc("sector", chan.Sector);		break;
		case SOURCE_Polyobj:	arc("poly", chan.Poly);			break;
		case SOURCE_Unattached:	arc.Array("point", chan.Point, 3); break;
		default:				I_Error("Unknown sound source type %d\n", chan.SourceType);	break;
		}
		arc.EndObject();
	}
	return arc;
}

//==========================================================================
//
// S_SerializeSounds
//
//==========================================================================

void S_SerializeSounds(FSerializer &arc)
{
	FSoundChan *chan;

	GSnd->Sync(true);

	if (arc.isWriting())
	{
		TArray<FSoundChan *> chans;

		// Count channels and accumulate them so we can store them in
		// reverse order. That way, they will be in the same order when
		// reloaded later as they are now.
		for (chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			// If the sound is forgettable, this is as good a time as
			// any to forget about it. And if it's a UI sound, it shouldn't
			// be stored in the savegame.
			if (!(chan->ChanFlags & (CHAN_FORGETTABLE | CHAN_UI)))
			{
				chans.Push(chan);
			}
		}
		if (chans.Size() > 0 && arc.BeginArray("sounds"))
		{
			for (unsigned int i = chans.Size(); i-- != 0; )
			{
				// Replace start time with sample position.
				uint64_t start = chans[i]->StartTime.AsOne;
				chans[i]->StartTime.AsOne = GSnd ? GSnd->GetPosition(chans[i]) : 0;
				arc(nullptr, *chans[i]);
				chans[i]->StartTime.AsOne = start;
			}
			arc.EndArray();
		}
	}
	else
	{
		unsigned int count;

		S_StopAllChannels();
		if (arc.BeginArray("sounds"))
		{
			count = arc.ArraySize();
			for (unsigned int i = 0; i < count; ++i)
			{
				chan = (FSoundChan*)S_GetChannel(NULL);
				arc(nullptr, *chan);
				// Sounds always start out evicted when restored from a save.
				chan->ChanFlags |= CHAN_EVICTED | CHAN_ABSTIME;
			}
			arc.EndArray();
		}
		// The two tic delay is to make sure any screenwipes have finished.
		// This needs to be two because the game is run for one tic before
		// the wipe so that it can produce a screen to wipe to. So if we
		// only waited one tic to restart the sounds, they would start
		// playing before the wipe, and depending on the synchronization
		// between the main thread and the mixer thread at the time, the
		// sounds might be heard briefly before pausing for the wipe.
		RestartEvictionsAt = primaryLevel->time + 2;
	}
	GSnd->Sync(false);
	GSnd->UpdateSounds();
}

//==========================================================================
//
// S_ActivatePlayList
//
// Plays the next song in the playlist. If no songs in the playlist can be
// played, then it is deleted.
//==========================================================================

void S_ActivatePlayList (bool goBack)
{
	int startpos, pos;

	startpos = pos = PlayList->GetPosition ();
	S_StopMusic (true);
	while (!S_ChangeMusic (PlayList->GetSong (pos), 0, false, true))
	{
		pos = goBack ? PlayList->Backup () : PlayList->Advance ();
		if (pos == startpos)
		{
			delete PlayList;
			PlayList = NULL;
			Printf ("Cannot play anything in the playlist.\n");
			return;
		}
	}
}

//==========================================================================
//
// S_ChangeCDMusic
//
// Starts a CD track as music.
//==========================================================================

bool S_ChangeCDMusic (int track, unsigned int id, bool looping)
{
	char temp[32];

	if (id != 0)
	{
		mysnprintf (temp, countof(temp), ",CD,%d,%x", track, id);
	}
	else
	{
		mysnprintf (temp, countof(temp), ",CD,%d", track);
	}
	return S_ChangeMusic (temp, 0, looping);
}

//==========================================================================
//
// S_StartMusic
//
// Starts some music with the given name.
//==========================================================================

bool S_StartMusic (const char *m_id)
{
	return S_ChangeMusic (m_id, 0, false);
}

//==========================================================================
//
// S_ChangeMusic
//
// Starts playing a music, possibly looping.
//
// [RH] If music is a MOD, starts it at position order. If name is of the
// format ",CD,<track>,[cd id]" song is a CD track, and if [cd id] is
// specified, it will only be played if the specified CD is in a drive.
//==========================================================================

bool S_ChangeMusic (const char *musicname, int order, bool looping, bool force)
{
	if (!force && PlayList)
	{ // Don't change if a playlist is active
		return false;
	}

	// allow specifying "*" as a placeholder to play the level's default music.
	if (musicname != NULL && !strcmp(musicname, "*"))
	{
		if (gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL)
		{
			musicname = primaryLevel->Music;
			order = primaryLevel->musicorder;
		}
		else
		{
			musicname = NULL;
		}
	}

	if (musicname == NULL || musicname[0] == 0)
	{
		// Don't choke if the map doesn't have a song attached
		S_StopMusic (true);
		mus_playing.name = "";
		LastSong = "";
		return true;
	}

	FString DEH_Music;
	if (musicname[0] == '$')
	{
		// handle dehacked replacement.
		// Any music name defined this way needs to be prefixed with 'D_' because
		// Doom.exe does not contain the prefix so these strings don't either.
		const char * mus_string = GStrings[musicname+1];
		if (mus_string != NULL)
		{
			DEH_Music << "D_" << mus_string;
			musicname = DEH_Music;
		}
	}

	FName *aliasp = MusicAliases.CheckKey(musicname);
	if (aliasp != NULL) 
	{
		if (*aliasp == NAME_None)
		{
			return true;	// flagged to be ignored
		}
		musicname = aliasp->GetChars();
	}

	if (!mus_playing.name.IsEmpty() &&
		mus_playing.handle != NULL &&
		stricmp (mus_playing.name, musicname) == 0 &&
		mus_playing.handle->m_Looping == looping)
	{
		if (order != mus_playing.baseorder)
		{
			if (mus_playing.handle->SetSubsong(order))
			{
				mus_playing.baseorder = order;
			}
		}
		else if (!mus_playing.handle->IsPlaying())
		{
			mus_playing.handle->Play(looping, order);
		}
		return true;
	}

	if (strnicmp (musicname, ",CD,", 4) == 0)
	{
		int track = strtoul (musicname+4, NULL, 0);
		const char *more = strchr (musicname+4, ',');
		unsigned int id = 0;

		if (more != NULL)
		{
			id = strtoul (more+1, NULL, 16);
		}
		S_StopMusic (true);
		mus_playing.handle = I_RegisterCDSong (track, id);
	}
	else
	{
		int lumpnum = -1;
		int length = 0;
		MusInfo *handle = NULL;
		MidiDeviceSetting *devp = MidiDevices.CheckKey(musicname);

		// Strip off any leading file:// component.
		if (strncmp(musicname, "file://", 7) == 0)
		{
			musicname += 7;
		}

		FileReader reader;
		if (!FileExists (musicname))
		{
			if ((lumpnum = Wads.CheckNumForFullName (musicname, true, ns_music)) == -1)
			{
				Printf ("Music \"%s\" not found\n", musicname);
				return false;
			}
			if (handle == NULL)
			{
				if (Wads.LumpLength (lumpnum) == 0)
				{
					return false;
				}
				reader = Wads.ReopenLumpReader(lumpnum);
			}
		}
		else
		{
			// Load an external file.
			if (!reader.OpenFile(musicname))
			{
				return false;
			}
		}

		// shutdown old music
		S_StopMusic (true);

		// Just record it if volume is 0
		if (snd_musicvolume <= 0)
		{
			mus_playing.loop = looping;
			mus_playing.name = musicname;
			mus_playing.baseorder = order;
			LastSong = musicname;
			return true;
		}

		// load & register it
		if (handle != NULL)
		{
			mus_playing.handle = handle;
		}
		else
		{
			mus_playing.handle = I_RegisterSong (reader, devp);
		}
	}

	mus_playing.loop = looping;
	mus_playing.name = musicname;
	mus_playing.baseorder = 0;
	LastSong = "";

	if (mus_playing.handle != 0)
	{ // play it
		mus_playing.handle->Start(looping, S_GetMusicVolume (musicname), order);
		mus_playing.baseorder = order;
		return true;
	}
	return false;
}

DEFINE_ACTION_FUNCTION(DObject, S_ChangeMusic)
{
	PARAM_PROLOGUE;
	PARAM_STRING(music);
	PARAM_INT(order);
	PARAM_BOOL(looping);
	PARAM_BOOL(force);
	ACTION_RETURN_BOOL(S_ChangeMusic(music, order, looping, force));
}


//==========================================================================
//
// S_RestartMusic
//
// Must only be called from snd_reset in i_sound.cpp!
//==========================================================================

void S_RestartMusic ()
{
	if (!LastSong.IsEmpty())
	{
		FString song = LastSong;
		LastSong = "";
		S_ChangeMusic (song, mus_playing.baseorder, mus_playing.loop, true);
	}
}

//==========================================================================
//
// S_MIDIDeviceChanged
//
//==========================================================================

void S_MIDIDeviceChanged()
{
	if (mus_playing.handle != NULL && mus_playing.handle->IsMIDI())
	{
		mus_playing.handle->Stop();
		mus_playing.handle->Start(mus_playing.loop, -1, mus_playing.baseorder);
	}
}

//==========================================================================
//
// S_GetMusic
//
//==========================================================================

int S_GetMusic (const char **name)
{
	int order;

	if (mus_playing.name.IsNotEmpty())
	{
		*name = mus_playing.name;
		order = mus_playing.baseorder;
	}
	else
	{
		*name = NULL;
		order = 0;
	}
	return order;
}

//==========================================================================
//
// S_StopMusic
//
//==========================================================================

void S_StopMusic (bool force)
{
	// [RH] Don't stop if a playlist is active.
	if ((force || PlayList == NULL) && !mus_playing.name.IsEmpty())
	{
		if (mus_playing.handle != NULL)
		{
			if (MusicPaused)
				mus_playing.handle->Resume();

			mus_playing.handle->Stop();
			delete mus_playing.handle;
			mus_playing.handle = NULL;
		}
		LastSong = mus_playing.name;
		mus_playing.name = "";
	}
}

//==========================================================================
//
// CCMD playsound
//
//==========================================================================

CCMD (playsound)
{
	if (argv.argc() > 1)
	{
		FSoundID id = argv[1];
		if (id == 0)
		{
			Printf("'%s' is not a sound\n", argv[1]);
		}
		else
		{
			S_Sound (CHAN_AUTO | CHAN_UI, id, 1.f, ATTN_NONE);
		}
	}
}

//==========================================================================
//
// CCMD loopsound
//
//==========================================================================

CCMD (loopsound)
{
	if (players[consoleplayer].mo != NULL && !netgame && argv.argc() > 1)
	{
		FSoundID id = argv[1];
		if (id == 0)
		{
			Printf("'%s' is not a sound\n", argv[1]);
		}
		else
		{
			AActor *icon = Spawn(primaryLevel, "SpeakerIcon", players[consoleplayer].mo->PosPlusZ(32.), ALLOW_REPLACE);
			if (icon != NULL)
			{
				S_Sound(icon, CHAN_BODY | CHAN_LOOP, id, 1.f, ATTN_IDLE);
			}
		}
	}
}

//==========================================================================
//
// CCMD idmus
//
//==========================================================================

CCMD (idmus)
{
	level_info_t *info;
	FString map;
	int l;

	if (!nomusic)
	{
		if (argv.argc() > 1)
		{
			if (gameinfo.flags & GI_MAPxx)
			{
				l = atoi (argv[1]);
			if (l <= 99)
				{
					map = CalcMapName (0, l);
				}
				else
				{
					Printf ("%s\n", GStrings("STSTR_NOMUS"));
					return;
				}
			}
			else
			{
				map = CalcMapName (argv[1][0] - '0', argv[1][1] - '0');
			}

			if ( (info = FindLevelInfo (map)) )
			{
				if (info->Music.IsNotEmpty())
				{
					S_ChangeMusic (info->Music, info->musicorder);
					Printf ("%s\n", GStrings("STSTR_MUS"));
				}
			}
			else
			{
				Printf ("%s\n", GStrings("STSTR_NOMUS"));
			}
		}
	}
}

//==========================================================================
//
// CCMD changemus
//
//==========================================================================

CCMD (changemus)
{
	if (!nomusic)
	{
		if (argv.argc() > 1)
		{
			if (PlayList)
			{
				delete PlayList;
				PlayList = NULL;
			}
			S_ChangeMusic (argv[1], argv.argc() > 2 ? atoi (argv[2]) : 0);
		}
		else
		{
			const char *currentmus = mus_playing.name.GetChars();
			if(currentmus != NULL && *currentmus != 0)
			{
				Printf ("currently playing %s\n", currentmus);
			}
			else
			{
				Printf ("no music playing\n");
			}
		}
	}
}

//==========================================================================
//
// CCMD stopmus
//
//==========================================================================

CCMD (stopmus)
{
	if (PlayList)
	{
		delete PlayList;
		PlayList = NULL;
	}
	S_StopMusic (false);
	LastSong = "";	// forget the last played song so that it won't get restarted if some volume changes occur
}

//==========================================================================
//
// CCMD cd_play
//
// Plays a specified track, or the entire CD if no track is specified.
//==========================================================================

CCMD (cd_play)
{
	char musname[16];

	if (argv.argc() == 1)
	{
		strcpy (musname, ",CD,");
	}
	else
	{
		mysnprintf (musname, countof(musname), ",CD,%d", atoi(argv[1]));
	}
	S_ChangeMusic (musname, 0, true);
}

//==========================================================================
//
// CCMD cd_stop
//
//==========================================================================

CCMD (cd_stop)
{
	CD_Stop ();
}

//==========================================================================
//
// CCMD cd_eject
//
//==========================================================================

CCMD (cd_eject)
{
	CD_Eject ();
}

//==========================================================================
//
// CCMD cd_close
//
//==========================================================================

CCMD (cd_close)
{
	CD_UnEject ();
}

//==========================================================================
//
// CCMD cd_pause
//
//==========================================================================

CCMD (cd_pause)
{
	CD_Pause ();
}

//==========================================================================
//
// CCMD cd_resume
//
//==========================================================================

CCMD (cd_resume)
{
	CD_Resume ();
}

//==========================================================================
//
// CCMD playlist
//
//==========================================================================

UNSAFE_CCMD (playlist)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 3)
	{
		Printf ("playlist <playlist.m3u> [<position>|shuffle]\n");
	}
	else
	{
		if (PlayList != NULL)
		{
			PlayList->ChangeList (argv[1]);
		}
		else
		{
			PlayList = new FPlayList (argv[1]);
		}
		if (PlayList->GetNumSongs () == 0)
		{
			delete PlayList;
			PlayList = NULL;
		}
		else
		{
			if (argc == 3)
			{
				if (stricmp (argv[2], "shuffle") == 0)
				{
					PlayList->Shuffle ();
				}
				else
				{
					PlayList->SetPosition (atoi (argv[2]));
				}
			}
			S_ActivatePlayList (false);
		}
	}
}

//==========================================================================
//
// CCMD playlistpos
//
//==========================================================================

static bool CheckForPlaylist ()
{
	if (PlayList == NULL)
	{
		Printf ("No playlist is playing.\n");
		return false;
	}
	return true;
}

CCMD (playlistpos)
{
	if (CheckForPlaylist() && argv.argc() > 1)
	{
		PlayList->SetPosition (atoi (argv[1]) - 1);
		S_ActivatePlayList (false);
	}
}

//==========================================================================
//
// CCMD playlistnext
//
//==========================================================================

CCMD (playlistnext)
{
	if (CheckForPlaylist())
	{
		PlayList->Advance ();
		S_ActivatePlayList (false);
	}
}

//==========================================================================
//
// CCMD playlistprev
//
//==========================================================================

CCMD (playlistprev)
{
	if (CheckForPlaylist())
	{
		PlayList->Backup ();
		S_ActivatePlayList (true);
	}
}

//==========================================================================
//
// CCMD playliststatus
//
//==========================================================================

CCMD (playliststatus)
{
	if (CheckForPlaylist ())
	{
		Printf ("Song %d of %d:\n%s\n",
			PlayList->GetPosition () + 1,
			PlayList->GetNumSongs (),
			PlayList->GetSong (PlayList->GetPosition ()));
	}
}

//==========================================================================
//
// CCMD cachesound <sound name>
//
//==========================================================================

CCMD (cachesound)
{
	if (argv.argc() < 2)
	{
		Printf ("Usage: cachesound <sound> ...\n");
		return;
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		FSoundID sfxnum = argv[i];
		if (sfxnum != FSoundID(0))
		{
			S_CacheSound (&S_sfx[sfxnum]);
		}
	}
}


CCMD(listsoundchannels)
{	
	FSoundChan *chan;
	int count = 0;
	for (chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (!(chan->ChanFlags & CHAN_EVICTED))
		{
			FVector3 chanorigin;

			CalcPosVel(chan, &chanorigin, NULL);

			Printf("%s at (%1.5f, %1.5f, %1.5f)\n", (const char*)chan->SoundID, chanorigin.X, chanorigin.Y, chanorigin.Z);
			count++;
		}
	}
	Printf("%d sounds playing\n", count);
}

CCMD(currentmusic)
{
	if (mus_playing.name.IsNotEmpty())
	{
		Printf("Currently playing music '%s'\n", mus_playing.name.GetChars());
	}
	else
	{
		Printf("Currently no music playing\n");
	}
}
