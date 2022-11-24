/*
** s_advsound.cpp
** Routines for managing SNDINFO lumps and ambient sounds
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

// HEADER FILES ------------------------------------------------------------


#include "actor.h"
#include "c_dispatch.h"
#include "filesystem.h"
#include "gi.h"
#include "i_sound.h"
#include "d_netinf.h"
#include "d_player.h"
#include "serializer.h"
#include "v_text.h"
#include "g_levellocals.h"
#include "r_data/sprites.h"
#include "vm.h"
#include "i_system.h"
#include "s_music.h"

// MACROS ------------------------------------------------------------------

#define RANDOM		1
#define PERIODIC	2
#define CONTINUOUS	3
#define POSITIONAL	4
#define SURROUND	16

// TYPES -------------------------------------------------------------------

struct FPlayerClassLookup
{
	FString		Name;
	uint16_t		ListIndex[GENDER_MAX];	// indices into PlayerSounds (0xffff means empty)
};

// Used to lookup a sound like "*grunt". This contains all player sounds for
// a particular class and gender.
class FPlayerSoundHashTable
{
	TMap<int, FSoundID> map;
public:

	void AddSound(FSoundID player_sound_id, FSoundID sfx_id)
	{
		map.Insert(player_sound_id.index(), sfx_id);
	}
	FSoundID LookupSound(FSoundID player_sound_id)
	{
		auto v = map.CheckKey(player_sound_id.index());
		return v ? *v : NO_SOUND;
	}
	void MarkUsed()
	{
		decltype(map)::Iterator it(map);
		decltype(map)::Pair* pair;

		while (it.NextPair(pair))
		{
			soundEngine->MarkUsed(pair->Value);
		}
	}

protected:
	struct Entry
	{
		Entry* Next;
		FSoundID PlayerSoundID;
		FSoundID SfxID;
	};
	enum { NUM_BUCKETS = 23 };
	Entry* Buckets[NUM_BUCKETS];
};

struct FAmbientSound
{
	unsigned	type;		// type of ambient sound
	int			periodmin;	// # of tics between repeats
	int			periodmax;	// max # of tics for random ambients
	float		volume;		// relative volume of sound
	float		attenuation;
	FSoundID	sound;		// Sound to play
};
TMap<int, FAmbientSound> Ambients;

enum SICommands
{
	SI_Ambient,
	SI_Random,
	SI_PlayerSound,
	SI_PlayerSoundDup,
	SI_PlayerCompat,
	SI_PlayerAlias,
	SI_Alias,
	SI_Limit,
	SI_Singular,
	SI_PitchShift,
	SI_PitchShiftRange,
	SI_Map,
	SI_Registered,
	SI_ArchivePath,
	SI_MusicVolume,
	SI_MidiDevice,
	SI_IfDoom,
	SI_IfHeretic,
	SI_IfHexen,
	SI_IfStrife,
	SI_Rolloff,
	SI_Volume,
	SI_MusicAlias,
	SI_EDFOverride,
	SI_Attenuation,
	SI_PitchSet,
};

// Blood was a cool game. If Monolith ever releases the source for it,
// you can bet I'll port it.

struct FBloodSFX
{
	uint32_t	RelVol;		// volume, 0-255
	int		Pitch;		// pitch change
	int		PitchRange;	// range of random pitch
	uint32_t	Format;		// format of audio 1=11025 5=22050
	int32_t	LoopStart;	// loop position (-1 means no looping)
	char	RawName[9];	// name of RAW resource
};

// This is used to recreate the skin sounds after reloading SNDINFO due to a changed local one.
struct FSavedPlayerSoundInfo
{
	FName pclass;
	int gender;
	FSoundID refid;
	int lumpnum;
	bool alias;
};

// This specifies whether Timidity or Windows playback is preferred for a certain song (only useful for Windows.)
MusicAliasMap MusicAliases;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern bool IsFloat (const char *str);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int SortPlayerClasses (const void *a, const void *b);
static FSoundID S_DupPlayerSound (const char *pclass, int gender, FSoundID refid, FSoundID aliasref);
static void S_SavePlayerSound (const char *pclass, int gender, FSoundID refid, int lumpnum, bool alias);
static void S_RestorePlayerSounds();
static int S_AddPlayerClass (const char *name);
static int S_AddPlayerGender (int classnum, int gender);
static int S_FindPlayerClass (const char *name);
static FSoundID S_LookupPlayerSound (int classidx, int gender, FSoundID refid);
static void S_ParsePlayerSoundCommon (FScanner &sc, FString &pclass, int &gender, FSoundID &refid);
static void S_AddSNDINFO (int lumpnum);
static void S_AddBloodSFX (int lumpnum);
static void S_AddStrifeVoice (int lumpnum);
static FSoundID S_AddSound (const char *logicalname, int lumpnum, FScanner *sc=NULL);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//TArray<sfxinfo_t> S_sfx (128);
TMap<int, FString> HexenMusic;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *SICommandStrings[] =
{
	"$ambient",
	"$random",
	"$playersound",
	"$playersounddup",
	"$playercompat",
	"$playeralias",
	"$alias",
	"$limit",
	"$singular",
	"$pitchshift",
	"$pitchshiftrange",
	"$map",
	"$registered",
	"$archivepath",
	"$musicvolume",
	"$mididevice",
	"$ifdoom",
	"$ifheretic",
	"$ifhexen",
	"$ifstrife",
	"$rolloff",
	"$volume",
	"$musicalias",
	"$edfoverride",
	"$attenuation",
	"$pitchset",
	NULL
};

static TArray<FSavedPlayerSoundInfo> SavedPlayerSounds;

static int NumPlayerReserves;
static bool PlayerClassesIsSorted;

static TArray<FPlayerClassLookup> PlayerClassLookups;
static TArray<FPlayerSoundHashTable> PlayerSounds;


static FString DefPlayerClassName;
static int DefPlayerClass;

static uint8_t CurrentPitchMask;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// S_CheckIntegrity
//
// Scans the entire sound list and looks for recursive definitions.
//==========================================================================

static bool S_CheckSound(sfxinfo_t *startsfx, sfxinfo_t *sfx, TArray<sfxinfo_t *> &chain)
{
	sfxinfo_t *me = sfx;
	bool success = true;
	unsigned siz = chain.Size();

	if (sfx->UserData[0] & SND_PlayerReserve)
	{
		return true;
	}

	// There is a bad link in here, but let's report it only for the sound that contains the broken definition.
	// Once that sound has been disabled this one will work again.
	if (chain.Find(sfx) < chain.Size())
	{
		return true;
	}
	chain.Push(sfx);

	if (me->bRandomHeader)
	{
		const FRandomSoundList* list = soundEngine->ResolveRandomSound(me);
		for (unsigned i = 0; i < list->Choices.Size(); ++i)
		{
			auto rsfx = soundEngine->GetWritableSfx(list->Choices[i]);
			if (rsfx == startsfx)
			{
				Printf(TEXTCOLOR_RED "recursive sound $random found for %s:\n", startsfx->name.GetChars());
				success = false;
				for (unsigned i = 1; i<chain.Size(); i++)
				{
					Printf(TEXTCOLOR_ORANGE "    -> %s\n", chain[i]->name.GetChars());
				}
			}
			else
			{
				success &= S_CheckSound(startsfx, rsfx, chain);
			}
		}
	}
	else if (me->link != sfxinfo_t::NO_LINK)
	{
		me = soundEngine->GetWritableSfx(me->link);
		if (me == startsfx)
		{
			Printf(TEXTCOLOR_RED "recursive sound $alias found for %s:\n", startsfx->name.GetChars());
			success = false;
			for (unsigned i = 1; i<chain.Size(); i++)
			{
				Printf(TEXTCOLOR_ORANGE "    -> %s\n", chain[i]->name.GetChars());
			}
			chain.Resize(siz);
		}
		else
		{
			success &= S_CheckSound(startsfx, me, chain);
		}
	}
	chain.Pop();
	return success;
}

void S_CheckIntegrity()
{
	TArray<sfxinfo_t *> chain;
	TArray<bool> broken;

	broken.Resize(soundEngine->GetNumSounds());
	memset(&broken[0], 0, sizeof(bool) * soundEngine->GetNumSounds());
	for (unsigned i = 0; i < soundEngine->GetNumSounds(); i++)
	{
		auto &sfx = *soundEngine->GetWritableSfx(FSoundID::fromInt(i));
		broken[i] = !S_CheckSound(&sfx, &sfx, chain);
	}
	for (unsigned i = 0; i < soundEngine->GetNumSounds(); i++)
	{
		if (broken[i])
		{
			auto& sfx = *soundEngine->GetWritableSfx(FSoundID::fromInt(i));
			Printf(TEXTCOLOR_RED "Sound %s has been disabled\n", sfx.name.GetChars());
			sfx.bRandomHeader = false;
			sfx.link = NO_SOUND;	// link to the empty sound.
		}
	}
}

//==========================================================================
//
// S_GetSoundMSLength
//
// Returns duration of sound
// This cannot be made a sound engine function due to the player sounds.
//
//==========================================================================

unsigned int S_GetMSLength(FSoundID sound)
{
	sfxinfo_t* sfx = soundEngine->GetWritableSfx(sound);
	if (!sfx) return 0;

	// Resolve player sounds, random sounds, and aliases
	if (sfx->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->UserData[0] & SND_PlayerReserve)
		{
			sfx = soundEngine->GetWritableSfx(S_FindSkinnedSound(NULL, sound));
		}
		else if (sfx->bRandomHeader)
		{
			// Hm... What should we do here?
			// Pick the longest or the shortest sound?
			// I think the longest one makes more sense.

			int length = 0;
			const FRandomSoundList* list = soundEngine->ResolveRandomSound(sfx);

			for (auto &me : list->Choices)
			{
				// unfortunately we must load all sounds to find the longest one... :(
				int thislen = S_GetMSLength(me);
				if (thislen > length) length = thislen;
			}
			return length;
		}
		else
		{
			sfx = soundEngine->GetWritableSfx(sfx->link);
		}
	}

	sfx = soundEngine->LoadSound(sfx);
	if (sfx != NULL) return GSnd->GetMSLength(sfx->data);
	else return 0;
}

DEFINE_ACTION_FUNCTION(DObject,S_GetLength)
{
	PARAM_PROLOGUE;
	PARAM_SOUND(sound_id);
	ACTION_RETURN_FLOAT(S_GetMSLength(sound_id)/1000.0);
}

//==========================================================================
//
// S_AddSound
//
// If logical name is already in S_sfx, updates it to use the new sound
// lump. Otherwise, adds the new mapping by using S_AddSoundLump().
//==========================================================================

FSoundID S_AddSound (const char *logicalname, const char *lumpname, FScanner *sc)
{
	int lump = fileSystem.CheckNumForFullName (lumpname, true, ns_sounds);
	return S_AddSound (logicalname, lump);
}

static FSoundID S_AddSound (const char *logicalname, int lumpnum, FScanner *sc)
{
	FSoundID sfxid = soundEngine->FindSoundNoHash (logicalname);

	if (sfxid.isvalid())
	{ // If the sound has already been defined, change the old definition
		auto sfx = soundEngine->GetWritableSfx(sfxid);

		if (sfx->UserData[0] & SND_PlayerReserve)
		{
			if (sc != NULL)
			{
				sc->ScriptError ("Sounds that are reserved for players cannot be reassigned");
			}
			else
			{
				I_Error ("Sounds that are reserved for players cannot be reassigned");
			}
		}
		// Redefining a player compatibility sound will redefine the target instead.
		if (sfx->UserData[0] & SND_PlayerCompat)
		{
			sfx = soundEngine->GetWritableSfx(sfx->link);
		}
		if (sfx->bRandomHeader)
		{
			FRandomSoundList* rnd = soundEngine->ResolveRandomSound(sfx);
			rnd->Choices.Reset();
			rnd->Owner = NO_SOUND;
		}
		sfx->lumpnum = lumpnum;
		sfx->bRandomHeader = false;
		sfx->link = sfxinfo_t::NO_LINK;
		sfx->bTentative = false;
		if (sfx->NearLimit == -1) 
		{
			sfx->NearLimit = 2;
			sfx->LimitRange = 256*256;
		}
		//sfx->PitchMask = CurrentPitchMask;
	}
	else
	{ // Otherwise, create a new definition.
		sfxid = soundEngine->AddSoundLump (logicalname, lumpnum, CurrentPitchMask);
	}

	return sfxid;
}

//==========================================================================
//
// S_AddPlayerSound
//
// Adds the given sound lump to the player sound lists.
//==========================================================================

FSoundID S_AddPlayerSound (const char *pclass, int gender, FSoundID refid, const char *lumpname)
{
	int lump=-1;
	
	if (lumpname)
	{
		lump = fileSystem.CheckNumForFullName (lumpname, true, ns_sounds);
	}

	return S_AddPlayerSound (pclass, gender, refid, lump);
}

FSoundID S_AddPlayerSound (const char *pclass, int gender, FSoundID refid, int lumpnum, bool fromskin)
{
	FString fakename;
	FSoundID id;

	auto sfx = soundEngine->GetSfx(refid);
	if (refid == NO_SOUND || !sfx) return NO_SOUND;

	fakename = pclass;
	fakename += '"';
	fakename += '0' + gender;
	fakename += '"';
	fakename += sfx->name;

	id = soundEngine->AddSoundLump (fakename, lumpnum, CurrentPitchMask);
	int classnum = S_AddPlayerClass (pclass);
	int soundlist = S_AddPlayerGender (classnum, gender);

	PlayerSounds[soundlist].AddSound (sfx->link, id);

	if (fromskin) S_SavePlayerSound(pclass, gender, refid, lumpnum, false);

	return id;
}

//==========================================================================
//
// S_AddPlayerSoundExisting
//
// Adds the player sound as an alias to an existing sound.
//==========================================================================

FSoundID S_AddPlayerSoundExisting (const char *pclass, int gender, FSoundID refid, FSoundID aliasto, bool fromskin)
{
	int classnum = S_AddPlayerClass (pclass);
	int soundlist = S_AddPlayerGender (classnum, gender);
	auto sfx = soundEngine->GetSfx(refid);
	if (refid == NO_SOUND || !sfx) return NO_SOUND;

	PlayerSounds[soundlist].AddSound (sfx->link, aliasto);

	if (fromskin) S_SavePlayerSound(pclass, gender, refid, aliasto.index(), true);

	return aliasto;
}

//==========================================================================
//
// S_DupPlayerSound
//
// Adds a player sound that uses the same sound as an existing player sound.
//==========================================================================

FSoundID S_DupPlayerSound (const char *pclass, int gender, FSoundID refid, FSoundID aliasref)
{
	auto aliasto = S_LookupPlayerSound (pclass, gender, aliasref);
	return S_AddPlayerSoundExisting (pclass, gender, refid, aliasto);
}

//==========================================================================
//
// S_ClearSoundData
//
// clears all sound tables
// When we want to allow level specific SNDINFO lumps this has to
// be cleared for each level
//==========================================================================

void S_ClearSoundData()
{
	if (soundEngine)
		soundEngine->Clear();

	Ambients.Clear();
	MusicVolumes.Clear();

	NumPlayerReserves = 0;
	PlayerClassesIsSorted = false;
	PlayerClassLookups.Clear();
	PlayerSounds.Clear();
	DefPlayerClass = 0;
	DefPlayerClassName = "";
	MusicAliases.Clear();
	MidiDevices.Clear();
	HexenMusic.Clear();
}

//==========================================================================
//
// S_ParseSndInfo
//
// Parses all loaded SNDINFO lumps.
// Also registers Blood SFX files and Strife's voices.
//==========================================================================

void S_ParseSndInfo (bool redefine)
{
	int lump;

	if (!redefine) SavedPlayerSounds.Clear();	// clear skin sounds only for initial parsing.
	S_ClearSoundData();	// remove old sound data first!

	CurrentPitchMask = 0;
	S_AddSound ("{ no sound }", "DSEMPTY");	// Sound 0 is no sound at all
	for (lump = 0; lump < fileSystem.GetNumEntries(); ++lump)
	{
		switch (fileSystem.GetFileNamespace (lump))
		{
		case ns_global:
			if (fileSystem.CheckFileName (lump, "SNDINFO"))
			{
				S_AddSNDINFO (lump);
			}
			break;

		case ns_strifevoices:
			S_AddStrifeVoice (lump);
			break;
		}
	}
	S_RestorePlayerSounds();
	soundEngine->HashSounds ();

	S_ShrinkPlayerSoundLists ();

	S_CheckIntegrity();
}

//==========================================================================
//
// Adds a level specific SNDINFO lump
//
//==========================================================================

void S_AddLocalSndInfo(int lump)
{
	S_AddSNDINFO(lump);
	soundEngine->HashSounds ();

	S_ShrinkPlayerSoundLists ();
	S_CheckIntegrity();
}

//==========================================================================
//
// S_AddSNDINFO
//
// Reads a SNDINFO and does what it says.
//
//==========================================================================

static void S_AddSNDINFO (int lump)
{
	bool skipToEndIf;
	TArray<FSoundID> list;
	int wantassigns = -1;

	FScanner sc(lump);
	skipToEndIf = false;

	while (sc.GetString ())
	{
		if (skipToEndIf)
		{
			if (sc.Compare ("$endif"))
			{
				skipToEndIf = false;
			}
			continue;
		}

		if (sc.String[0] == '$')
		{ // Got a command
			switch (sc.MatchString (SICommandStrings))
			{
			case SI_Ambient: {
				// $ambient <num> <logical name> [point [atten] | surround | [world]]
				//			<continuous | random <minsecs> <maxsecs> | periodic <secs>>
				//			<volume>
				FAmbientSound *ambient;

				sc.MustGetNumber ();
				ambient = &Ambients[sc.Number];
				ambient->type = 0;
				ambient->periodmin = 0;
				ambient->periodmax = 0;
				ambient->volume = 0;
				ambient->attenuation = 0;
				ambient->sound = NO_SOUND;

				sc.MustGetString ();
				ambient->sound = FSoundID(soundEngine->FindSoundTentative(sc.String));
				ambient->attenuation = 0;

				sc.MustGetString ();
				if (sc.Compare ("point"))
				{
					float attenuation;

					ambient->type = POSITIONAL;
					sc.MustGetString ();

					if (IsFloat (sc.String))
					{
						attenuation = (float)atof (sc.String);
						sc.MustGetString ();
						if (attenuation > 0)
						{
							ambient->attenuation = attenuation;
						}
						else
						{
							ambient->attenuation = 1;
						}
					}
					else
					{
						ambient->attenuation = 1;
					}
				}
				else if (sc.Compare ("surround"))
				{
					ambient->type = SURROUND;
					sc.MustGetString ();
					ambient->attenuation = -1;
				}
				else
				{ // World is an optional keyword
					if (sc.Compare ("world"))
					{
						sc.MustGetString ();
					}
				}

				if (sc.Compare ("continuous"))
				{
					ambient->type |= CONTINUOUS;
				}
				else if (sc.Compare ("random"))
				{
					ambient->type |= RANDOM;
					sc.MustGetFloat ();
					ambient->periodmin = (int)(sc.Float * TICRATE);
					sc.MustGetFloat ();
					ambient->periodmax = (int)(sc.Float * TICRATE);
				}
				else if (sc.Compare ("periodic"))
				{
					ambient->type |= PERIODIC;
					sc.MustGetFloat ();
					ambient->periodmin = (int)(sc.Float * TICRATE);
				}
				else
				{
					Printf ("Unknown ambient type (%s)\n", sc.String);
				}

				sc.MustGetFloat ();
				ambient->volume = (float)sc.Float;
				if (ambient->volume > 1)
					ambient->volume = 1;
				else if (ambient->volume < 0)
					ambient->volume = 0;
				}
				break;

			case SI_Map: {
				// Hexen-style $MAP command
				int mapnum;

				sc.MustGetNumber();
				mapnum = sc.Number;
				sc.MustGetString();
				if (mapnum != 0)
				{
					HexenMusic[mapnum] = sc.String;
				}
				}
				break;

			case SI_Registered:
				// I don't think Hexen even pays attention to the $registered command.
			case SI_EDFOverride:
				break;

			case SI_ArchivePath:
				sc.MustGetString ();	// Unused for now
				break;

			case SI_PlayerSound: {
				// $playersound <player class> <gender> <logical name> <lump name>
				FString pclass;
				int gender;
				FSoundID refid, sfxnum;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				sfxnum = S_AddPlayerSound (pclass, gender, refid, sc.String);
				if (0 == stricmp(sc.String, "dsempty"))
				{
					soundEngine->GetWritableSfx(sfxnum)->UserData[0] |= SND_PlayerSilent;
				}
				}
				break;

			case SI_PlayerSoundDup: {
				// $playersounddup <player class> <gender> <logical name> <target sound name>
				FString pclass;
				int gender;
				FSoundID refid, targid;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				targid = soundEngine->FindSoundNoHash (sc.String);
				auto sfx = soundEngine->GetWritableSfx(targid);
				if (!sfx || !(sfx->UserData[0] & SND_PlayerReserve))
				{
					sc.ScriptError("%s is not a player sound", sc.String);
				}
				S_DupPlayerSound (pclass, gender, refid, targid);
				}
				break;

			case SI_PlayerCompat: {
				// $playercompat <player class> <gender> <logical name> <compat sound name>
				FString pclass;
				int gender;
				FSoundID refid;
				FSoundID sfxfrom, aliasto;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				sfxfrom = S_AddSound (sc.String, -1, &sc);
				aliasto = S_LookupPlayerSound (pclass, gender, refid);
				auto sfx = soundEngine->GetWritableSfx(sfxfrom);
				sfx->link = aliasto;
				sfx->UserData[0] |= SND_PlayerCompat;
				}
				break;

			case SI_PlayerAlias: {
				// $playeralias <player class> <gender> <logical name> <logical name of existing sound>
				FString pclass;
				int gender;
				FSoundID refid, soundnum;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				soundnum = soundEngine->FindSoundTentative (sc.String);
				S_AddPlayerSoundExisting (pclass, gender, refid, soundnum);
				}
				break;

			case SI_Alias: {
				// $alias <name of alias> <name of real sound>
				FSoundID sfxfrom;

				sc.MustGetString ();
				sfxfrom = S_AddSound (sc.String, -1, &sc);
				sc.MustGetString ();
				auto sfx = soundEngine->GetWritableSfx(sfxfrom);
				if (sfx->UserData[0] & SND_PlayerCompat)
				{
					sfxfrom = sfx->link;
				}
				sfx->link = soundEngine->FindSoundTentative (sc.String);
				sfx->NearLimit = -1;	// Aliases must use the original sound's limit.
				}
				break;

			case SI_Limit: {
				// $limit <logical name> <max channels> [<distance>]
				FSoundID sfxfrom;

				sc.MustGetString ();
				sfxfrom = soundEngine->FindSoundTentative (sc.String);
				sc.MustGetNumber ();
				auto sfx = soundEngine->GetWritableSfx(sfxfrom);
				sfx->NearLimit = min(max(sc.Number, 0), 255);
				if (sc.CheckFloat())
				{
					sfx->LimitRange = float(sc.Float * sc.Float);
				}
				}
				break;

			case SI_Singular: {
				// $singular <logical name>
				FSoundID sfx;

				sc.MustGetString ();
				sfx = soundEngine->FindSoundTentative (sc.String);
				auto sfxp = soundEngine->GetWritableSfx(sfx);
				sfxp->bSingular = true;
				}
				break;

			case SI_PitchShift: {
				// $pitchshift <logical name> <pitch shift amount>
				FSoundID sfx;

				sc.MustGetString ();
				sfx = soundEngine->FindSoundTentative (sc.String);
				sc.MustGetNumber ();
				auto sfxp = soundEngine->GetWritableSfx(sfx);
				sfxp->PitchMask = (1 << clamp (sc.Number, 0, 7)) - 1;
				}
				break;

			case SI_PitchSet: {
				// $pitchset <logical name> <pitch amount as float> [range maximum]
				FSoundID sfx;

				sc.MustGetString();
				sfx = soundEngine->FindSoundTentative(sc.String);
				sc.MustGetFloat();
				auto sfxp = soundEngine->GetWritableSfx(sfx);
				sfxp->DefPitch = (float)sc.Float;
				if (sc.CheckFloat())
				{
					sfxp->DefPitchMax = (float)sc.Float;
				}
				else
				{
					sfxp->DefPitchMax = 0;
				}
				}
				break;

			case SI_PitchShiftRange:
				// $pitchshiftrange <pitch shift amount>
				sc.MustGetNumber ();
				CurrentPitchMask = (1 << clamp (sc.Number, 0, 7)) - 1;
				break;

			case SI_Volume: {
				// $volume <logical name> <volume>
				FSoundID sfx;

				sc.MustGetString();
				sfx = soundEngine->FindSoundTentative(sc.String);
				sc.MustGetFloat();
				auto sfxp = soundEngine->GetWritableSfx(sfx);
				sfxp->Volume = (float)sc.Float;
				}
				break;

			case SI_Attenuation: {
				// $attenuation <logical name> <attenuation>
				FSoundID sfx;

				sc.MustGetString();
				sfx = soundEngine->FindSoundTentative(sc.String);
				sc.MustGetFloat();
				auto sfxp = soundEngine->GetWritableSfx(sfx);
				sfxp->Attenuation = (float)sc.Float;
				}
				break;

			case SI_Rolloff: {
				// $rolloff *|<logical name> [linear|log|custom] <min dist> <max dist/rolloff factor>
				// Using * for the name makes it the default for sounds that don't specify otherwise.
				FRolloffInfo *rolloff;
				int type;
				FSoundID sfx;

				sc.MustGetString();
				if (sc.Compare("*"))
				{
					sfx = INVALID_SOUND;
					rolloff = &soundEngine->GlobalRolloff();
				}
				else
				{
					sfx = soundEngine->FindSoundTentative(sc.String);
					auto sfxp = soundEngine->GetWritableSfx(sfx);
					sfxp->Rolloff;
				}
				type = ROLLOFF_Doom;
				if (!sc.CheckFloat())
				{
					sc.MustGetString();
					if (sc.Compare("linear"))
					{
						rolloff->RolloffType = ROLLOFF_Linear;
					}
					else if (sc.Compare("log"))
					{
						rolloff->RolloffType = ROLLOFF_Log;
					}
					else if (sc.Compare("custom"))
					{
						rolloff->RolloffType = ROLLOFF_Custom;
					}
					else
					{
						sc.ScriptError("Unknown rolloff type '%s'", sc.String);
					}
					sc.MustGetFloat();
				}
				rolloff->MinDistance = (float)sc.Float;
				sc.MustGetFloat();
				rolloff->MaxDistance = (float)sc.Float;
				break;
			  }

			case SI_Random: {
				// $random <logical name> { <logical name> ... }
				FRandomSoundList random;

				list.Clear ();
				sc.MustGetString ();
				FSoundID Owner = S_AddSound (sc.String, -1, &sc);
				sc.MustGetStringName ("{");
				while (sc.GetString () && !sc.Compare ("}"))
				{
					FSoundID sfxto = soundEngine->FindSoundTentative (sc.String);
					if (sfxto == random.Owner)
					{
						Printf("Definition of random sound '%s' refers to itself recursively.\n", sc.String);
						continue;
					}
					list.Push (sfxto);
				}
				if (list.Size() == 1)
				{ // Only one sound: treat as $alias
					auto sfxp = soundEngine->GetWritableSfx(Owner);
					sfxp->link = list[0];
					sfxp->NearLimit = -1;
				}
				else if (list.Size() > 1)
				{ // Only add non-empty random lists
					soundEngine->AddRandomSound(Owner, list);
				}
				}
				break;

			case SI_MusicVolume: {
				sc.MustGetString();
				FName musname (sc.String);
				sc.MustGetFloat();
				MusicVolumes[musname] = (float)sc.Float;
				}
				break;

			case SI_MusicAlias: {
				sc.MustGetString();
				int lump = fileSystem.CheckNumForName(sc.String, ns_music);
				if (lump >= 0)
				{
					// do not set the alias if a later WAD defines its own music of this name
					// internal files (up to and including iwads) can always be set for musicalias
					int file = fileSystem.GetFileContainer(lump);
					int sndifile = fileSystem.GetFileContainer(sc.LumpNum);
					if ( (file > sndifile) &&
						!(sndifile <= fileSystem.GetIwadNum() && file <= fileSystem.GetIwadNum()) )
					{
						sc.MustGetString();
						continue;
					}
				}
				FName alias = sc.String;
				sc.MustGetString();
				FName mapped = sc.String;

				// only set the alias if the lump it maps to exists.
				if (mapped == NAME_None || fileSystem.CheckNumForFullName(sc.String, true, ns_music) >= 0)
				{
					MusicAliases[alias] = mapped;
				}
				}
				break;

			case SI_MidiDevice: {
				sc.MustGetString();
				FName nm = sc.String;
				FScanner::SavedPos save = sc.SavePos();
				
				sc.SetCMode(true);
				sc.MustGetString();
				MidiDeviceSetting devset;
				if (sc.Compare("timidity")) devset.device = MDEV_TIMIDITY;
				else if (sc.Compare("fmod") || sc.Compare("sndsys")) devset.device = MDEV_SNDSYS;
				else if (sc.Compare("standard")) devset.device = MDEV_STANDARD;
				else if (sc.Compare("opl")) devset.device = MDEV_OPL;
				else if (sc.Compare("default")) devset.device = MDEV_DEFAULT;
				else if (sc.Compare("fluidsynth")) devset.device = MDEV_FLUIDSYNTH;
				else if (sc.Compare("gus")) devset.device = MDEV_GUS;
				else if (sc.Compare("wildmidi")) devset.device = MDEV_WILDMIDI;
				else if (sc.Compare("adl")) devset.device = MDEV_ADL;
				else if (sc.Compare("opn")) devset.device = MDEV_OPN;
				else sc.ScriptError("Unknown MIDI device %s\n", sc.String);

				if (sc.CheckString(","))
				{
					sc.SetCMode(false);
					sc.MustGetString();
					devset.args = sc.String;
				}
				else
				{
					// This does not really do what one might expect, because the next token has already been parsed and can be a '$'.
					// So in order to continue parsing without C-Mode, we need to reset and parse the last token again.
					sc.SetCMode(false);
					sc.RestorePos(save);
					sc.MustGetString();
				}
				MidiDevices[nm] = devset;
				}
				break;

			case SI_IfDoom: //also Chex
			case SI_IfStrife:
			case SI_IfHeretic:
			case SI_IfHexen:
				skipToEndIf = !CheckGame(sc.String+3, true);
				break;
			}
		}
		else
		{ // Got a logical sound mapping
			FString name (sc.String);
			if (wantassigns == -1)
			{
				wantassigns = sc.CheckString("=");
			}
			else if (wantassigns)
			{
				sc.MustGetStringName("=");
			}

			sc.MustGetString ();
			S_AddSound (name, sc.String, &sc);
		}
	}
}

//==========================================================================
//
// S_AddStrifeVoice
//
// Registers a new sound with the name "svox/<lumpname>"
//
//==========================================================================

static void S_AddStrifeVoice (int lumpnum)
{
	char name[16] = "svox/";
	fileSystem.GetFileShortName (name+5, lumpnum);
	S_AddSound (name, lumpnum);
}

//==========================================================================
//
// S_ParsePlayerSoundCommon
//
// Parses the common part of playersound commands in SNDINFO
//	(player class, gender, and ref id)
//==========================================================================

static void S_ParsePlayerSoundCommon (FScanner &sc, FString &pclass, int &gender, FSoundID &refid)
{
	sc.MustGetString ();
	pclass = sc.String;
	sc.MustGetString ();
	gender = D_GenderToInt (sc.String);
	sc.MustGetString ();
	refid = soundEngine->FindSoundNoHash (sc.String);
	auto sfx = soundEngine->GetWritableSfx(refid);
	if (refid.isvalid() && sfx && !(sfx->UserData[0] & SND_PlayerReserve) && !sfx->bTentative)
	{
		sc.ScriptError ("%s has already been used for a non-player sound.", sc.String);
	}
	if (refid == NO_SOUND)
	{
		refid = S_AddSound (sc.String, -1, &sc);
		sfx = soundEngine->GetWritableSfx(refid);
		sfx->bTentative = true;
	}
	if (sfx->bTentative)
	{
		sfx->link = FSoundID::fromInt(NumPlayerReserves++);
		sfx->bTentative = false;
		sfx->UserData[0] |= SND_PlayerReserve;
	}
	sc.MustGetString ();
}

//==========================================================================
//
// S_AddPlayerClass
//
// Adds the new player sound class if it doesn't exist. If it does, then
// the existing class is returned.
//==========================================================================

static int S_AddPlayerClass (const char *name)
{
	int cnum = S_FindPlayerClass (name);
	if (cnum == -1)
	{
		FPlayerClassLookup lookup;

		lookup.Name = name;
		for(int i = 0; i < GENDER_MAX; i++)
			lookup.ListIndex[i] = 0xffff;
		cnum = (int)PlayerClassLookups.Push (lookup);
		PlayerClassesIsSorted = false;

		// The default player class is the first one added
		if (DefPlayerClassName.IsEmpty())
		{
			DefPlayerClassName = lookup.Name;
			DefPlayerClass = cnum;
		}
	}
	return cnum;
}

//==========================================================================
//
// S_FindPlayerClass
//
// Finds the given player class. Returns -1 if not found.
//==========================================================================

static int S_FindPlayerClass (const char *name)
{
	if (!PlayerClassesIsSorted)
	{
		unsigned int i;

		for (i = 0; i < PlayerClassLookups.Size(); ++i)
		{
			if (stricmp (name, PlayerClassLookups[i].Name) == 0)
			{
				return (int)i;
			}
		}
	}
	else
	{
		int min = 0;
		int max = (int)(PlayerClassLookups.Size() - 1);

		while (min <= max)
		{
			int mid = (min + max) / 2;
			int lexx = stricmp (PlayerClassLookups[mid].Name, name);
			if (lexx == 0)
			{
				return mid;
			}
			else if (lexx < 0)
			{
				min = mid + 1;
			}
			else
			{
				max = mid - 1;
			}
		}
	}
	return -1;
}

//==========================================================================
//
// S_AddPlayerGender
//
// Adds a list of sounds for the given class and gender or just returns
// an existing list if one is present.
//==========================================================================

static int S_AddPlayerGender (int classnum, int gender)
{
	unsigned int index;

	index = PlayerClassLookups[classnum].ListIndex[gender];
	if (index == 0xffff)
	{
		index = PlayerSounds.Reserve (1);
		PlayerClassLookups[classnum].ListIndex[gender] = (uint16_t)index;
	}
	return index;
}

//==========================================================================
//
// S_ShrinkPlayerSoundLists
//
// Shrinks the arrays used by the player sounds to be just large enough
// and also sorts the PlayerClassLookups array.
//==========================================================================

void S_ShrinkPlayerSoundLists ()
{
	PlayerSounds.ShrinkToFit ();
	PlayerClassLookups.ShrinkToFit ();

	qsort (&PlayerClassLookups[0], PlayerClassLookups.Size(),
		sizeof(FPlayerClassLookup), SortPlayerClasses);
	PlayerClassesIsSorted = true;
	DefPlayerClass = S_FindPlayerClass (DefPlayerClassName);
}

static int SortPlayerClasses (const void *a, const void *b)
{
	return stricmp (((const FPlayerClassLookup *)a)->Name,
					((const FPlayerClassLookup *)b)->Name);
}

//==========================================================================
//
// S_LookupPlayerSound
//
// Returns the sound for the given player class, gender, and sound name.
//==========================================================================

FSoundID S_LookupPlayerSound (const char *pclass, int gender, FSoundID refid)
{
	auto sfxp = soundEngine->GetWritableSfx(refid);

	if (sfxp && !(sfxp->UserData[0] & SND_PlayerReserve))
	{ // Not a player sound, so just return this sound
		return refid;
	}

	return S_LookupPlayerSound (S_FindPlayerClass (pclass), gender, refid);
}

static FSoundID S_LookupPlayerSound (int classidx, int gender, FSoundID refid)
{
	int ingender = gender;

	if (classidx == -1)
	{
		classidx = DefPlayerClass;
	}

	int listidx = PlayerClassLookups[classidx].ListIndex[gender];
	if (listidx == 0xffff)
	{
		int g;

		for (g = 0; g < GENDER_MAX; ++g)
		{
			listidx = PlayerClassLookups[classidx].ListIndex[g];
			if (listidx != 0xffff) break;
		}
		if (g == GENDER_MAX)
		{ // No sounds defined at all for this class (can this happen?)
			if (classidx != DefPlayerClass)
			{
				return S_LookupPlayerSound (DefPlayerClass, gender, refid);
			}
			return NO_SOUND;
		}
		gender = g;
	}
	auto sfxp = soundEngine->GetWritableSfx(refid);
	if (!sfxp) return NO_SOUND;

	FSoundID sndnum = PlayerSounds[listidx].LookupSound (sfxp->link);
	sfxp = soundEngine->GetWritableSfx(sndnum);

	// If we're not done parsing SNDINFO yet, assume that the target sound is valid
	if (PlayerClassesIsSorted &&
		(!sfxp || sndnum == NO_SOUND ||
		((sfxp->lumpnum == -1 || sfxp->lumpnum == sfx_empty) &&
		 sfxp->link == sfxinfo_t::NO_LINK &&
		 !(sfxp->UserData[0] & SND_PlayerSilent))))
	{ // This sound is unavailable.
		if (ingender != 0)
		{ // Try "male"
			return S_LookupPlayerSound (classidx, GENDER_MALE, refid);
		}
		if (classidx != DefPlayerClass)
		{ // Try the default class.
			return S_LookupPlayerSound (DefPlayerClass, gender, refid);
		}
	}
	return sndnum;
}


//==========================================================================
//
// S_SavePlayerSound / S_RestorePlayerSounds
//
// Restores all skin-based player sounds after changing the local SNDINFO
// which forces a reload of the global one as well
//
//==========================================================================

static void S_SavePlayerSound (const char *pclass, int gender, FSoundID refid, int lumpnum, bool alias)
{
	FSavedPlayerSoundInfo spi;

	spi.pclass = pclass;
	spi.gender = gender;
	spi.refid = refid;
	spi.lumpnum = lumpnum;
	spi.alias = alias;
	SavedPlayerSounds.Push(spi);
}

static void S_RestorePlayerSounds()
{
	for(unsigned int i = 0; i < SavedPlayerSounds.Size(); i++)
	{
		FSavedPlayerSoundInfo * spi = &SavedPlayerSounds[i];
		if (spi->alias)
		{
			S_AddPlayerSoundExisting(spi->pclass.GetChars(), spi->gender, spi->refid, FSoundID::fromInt(spi->lumpnum));
		}
		else
		{
			S_AddPlayerSound(spi->pclass.GetChars(), spi->gender, spi->refid, spi->lumpnum);
		}
	}
}

//==========================================================================
//
// S_AreSoundsEquivalent
//
// Returns true if two sounds are essentially the same thing
//==========================================================================

bool S_AreSoundsEquivalent (AActor *actor, FSoundID id1, FSoundID id2)
{
	sfxinfo_t *sfx;

	if (id1 == id2)
	{
		return true;
	}
	if (!id1.isvalid() || !id2.isvalid())
	{
		return false;
	}
	// Dereference aliases, but not random or player sounds
	while (sfx = soundEngine->GetWritableSfx(id1), sfx->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->UserData[0] & SND_PlayerReserve)
		{
			id1 = S_FindSkinnedSound (actor, id1);
		}
		else if (sfx->bRandomHeader)
		{
			break;
		}
		else
		{
			id1 = sfx->link;
		}
	}
	while (sfx = soundEngine->GetWritableSfx(id2), sfx->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->UserData[0] & SND_PlayerReserve)
		{
			id2 = S_FindSkinnedSound (actor, id2);
		}
		else if (sfx->bRandomHeader)
		{
			break;
		}
		else
		{
			id2 = sfx->link;
		}
	}
	return id1 == id2;
}

//===========================================================================
//
//PlayerPawn :: GetSoundClass
//
//===========================================================================

const char *S_GetSoundClass(AActor *pp)
{
	auto player = pp->player;
	const char *defaultsoundclass = pp->NameVar(NAME_SoundClass) == NAME_None ? "player" : pp->NameVar(NAME_SoundClass).GetChars();
	if (player != nullptr &&
		(player->mo == nullptr || !(player->mo->flags4 &MF4_NOSKIN)) &&
		(unsigned int)player->userinfo.GetSkin() >= PlayerClasses.Size() &&
		(unsigned)player->userinfo.GetSkin() < Skins.Size() &&
		player->SoundClass.IsEmpty())
	{
		return Skins[player->userinfo.GetSkin()].Name.GetChars();
	}
		
	return (!player || player->SoundClass.IsEmpty()) ? defaultsoundclass : player->SoundClass.GetChars();
}

//==========================================================================
//
// S_FindSkinnedSound
//
// Calls S_LookupPlayerSound, deducing the class and gender from actor.
//==========================================================================

FSoundID S_FindSkinnedSound (AActor *actor, FSoundID refid)
{
	const char *pclass;
	int gender = 0;

	if (actor != nullptr && actor->player != nullptr) 
	{
		pclass = S_GetSoundClass(actor);
		gender = actor->player->userinfo.GetGender();
	}
	else
	{
		pclass = gameinfo.gametype == GAME_Hexen? "fighter" : "player";
	}

	return S_LookupPlayerSound (pclass, gender, refid);
}

//==========================================================================
//
// S_FindSkinnedSoundEx
//
// Tries looking for both "name-extendedname" and "name" in that order.
//==========================================================================

FSoundID S_FindSkinnedSoundEx (AActor *actor, const char *name, const char *extendedname)
{
	FString fullname;

	// Look for "name-extendedname";
	fullname = name;
	fullname += '-';
	fullname += extendedname;
	FSoundID id = S_FindSound(fullname);

	if (!id.isvalid())
	{ // Look for "name"
		id = S_FindSound(name);
	}
	return S_FindSkinnedSound (actor, id);
}

//==========================================================================
//
// S_MarkPlayerSounds
//
// Marks all sounds from a particular player class for precaching.
//
//==========================================================================

void S_MarkPlayerSounds (AActor *player)
{
	const char *playerclass = S_GetSoundClass(player);
	int classidx = S_FindPlayerClass(playerclass);
	if (classidx < 0)
	{
		classidx = DefPlayerClass;
	}
	for (int g = 0; g < GENDER_MAX; ++g)
	{
		int listidx = PlayerClassLookups[classidx].ListIndex[0];
		if (listidx != 0xffff)
		{
			PlayerSounds[listidx].MarkUsed();
		}
	}
	
}

//==========================================================================
//
// CCMD soundlinks
//
//==========================================================================

CCMD (soundlinks)
{
	unsigned int i;

	for (i = 0; i < soundEngine->GetNumSounds(); i++)
	{
		const sfxinfo_t* sfx = soundEngine->GetSfx(FSoundID::fromInt(i));

		if (sfx->link != sfxinfo_t::NO_LINK &&
			!sfx->bRandomHeader &&
			!(sfx->UserData[0] & SND_PlayerReserve))
		{
			Printf ("%s -> %s\n", sfx->name.GetChars(), soundEngine->GetSfx(sfx->link)->name.GetChars());
		}
	}
}

//==========================================================================
//
// CCMD playersounds
//
//==========================================================================

CCMD (playersounds)
{
	const char *reserveNames[256];
	unsigned int i;
	int j, k, l;

	// Find names for the player sounds
	memset (reserveNames, 0, sizeof(reserveNames));
	for (i = j = 0; j < NumPlayerReserves && i < soundEngine->GetNumSounds(); ++i)
	{
		auto sfx = soundEngine->GetSfx(FSoundID::fromInt(i));
		if (sfx->UserData[0] & SND_PlayerReserve)
		{
			++j;
			reserveNames[sfx->link.index()] = sfx->name;
		}
	}

	for (i = 0; i < PlayerClassLookups.Size(); ++i)
	{
		for (j = 0; j < GENDER_MAX; ++j)
		{
			if ((l = PlayerClassLookups[i].ListIndex[j]) != 0xffff)
			{
				Printf ("\n%s, %s:\n", PlayerClassLookups[i].Name.GetChars(), GenderNames[j]);
				for (k = 0; k < NumPlayerReserves; ++k)
				{
					auto sndid = PlayerSounds[l].LookupSound(FSoundID::fromInt(k));
					auto sfx = soundEngine->GetSfx(sndid);
					Printf (" %-16s%s\n", reserveNames[k], sfx->name.GetChars());
				}
			}
		}
	}
}

//==========================================================================
//
// AmbientSound :: MarkAmbientSounds
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AAmbientSound, MarkAmbientSounds)
{
	PARAM_SELF_PROLOGUE(AActor);

	FAmbientSound *ambient = Ambients.CheckKey(self->args[0]);
	if (ambient != NULL)
	{
		soundEngine->MarkUsed(ambient->sound);
	}
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

int GetTicker(struct FAmbientSound *ambient)
{
	if ((ambient->type & CONTINUOUS) == CONTINUOUS)
	{
		return 1;
	}
	else if (ambient->type & RANDOM)
	{
		return (int)(((float)rand() / (float)RAND_MAX) *
			(float)(ambient->periodmax - ambient->periodmin)) +
			ambient->periodmin;
	}
	else
	{
		return ambient->periodmin;
	}
}

//==========================================================================
//
// AmbientSound :: Tick
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AAmbientSound, Tick)
{
	PARAM_SELF_PROLOGUE(AActor);

	self->Tick();
	
	if (self->special1 > 0)
	{
		if (--self->special1 > 0) return 0;
	}

	if (!self->special2)
		return 0;

	FAmbientSound *ambient;
	EChanFlags loop = 0;

	ambient = Ambients.CheckKey(self->args[0]);
	if (ambient == NULL)
	{
		return 0;
	}

	if ((ambient->type & CONTINUOUS) == CONTINUOUS)
	{
		loop = CHANF_LOOP;
	}

	if (ambient->sound != NO_SOUND)
	{
		// The second argument scales the ambient sound's volume.
		// 0 and 100 are normal volume. The maximum volume level
		// possible is always 1.
		float volscale = self->args[1] == 0 ? 1 : self->args[1] / 100.f;
		float usevol = clamp(ambient->volume * volscale, 0.f, 1.f);

		// The third argument is the minimum distance for audible fading, and
		// the fourth argument is the maximum distance for audibility. Setting
		// either of these to 0 or setting  min distance > max distance will
		// use the standard rolloff.
		if ((self->args[2] | self->args[3]) == 0 || self->args[2] > self->args[3])
		{
			S_Sound(self, CHAN_BODY, loop, ambient->sound, usevol, ambient->attenuation);
		}
		else
		{
			float min = float(self->args[2]), max = float(self->args[3]);
			// The fifth argument acts as a scalar for the preceding two, if it's non-zero.
			if (self->args[4] > 0)
			{
				min *= self->args[4];
				max *= self->args[4];
			}
			S_SoundMinMaxDist(self, CHAN_BODY, loop, ambient->sound, usevol, min, max);
		}
		if (!loop)
		{
			self->special1 += GetTicker (ambient);
		}
		else
		{
			self->special1 = INT_MAX;
		}
	}
	else
	{
		self->Destroy ();
	}
	return 0;
}

//==========================================================================
//
// AmbientSound :: Activate
//
// Starts playing a sound (or does nothing if the sound is already playing).
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AAmbientSound, Activate)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT(activator, AActor);
		
	self->Activate(activator);
	FAmbientSound *amb = Ambients.CheckKey(self->args[0]);

	if (amb == NULL)
	{
		self->Destroy ();
		return 0;
	}

	if (!self->special2)
	{
		if ((amb->type & 3) == 0 && amb->periodmin == 0)
		{
			if (!amb->sound.isvalid())
			{
				self->Destroy ();
				return 0;
			}
			amb->periodmin = ::Scale(S_GetMSLength(amb->sound), TICRATE, 1000);
		}

		self->special1 = 0;
		if (amb->type & (RANDOM|PERIODIC))
			self->special1 += GetTicker (amb);

		self->special2 = true;
	}
	return 0;
}

//==========================================================================
//
// AmbientSound :: Deactivate
//
// Stops playing CONTINUOUS sounds immediately. Also prevents further
// occurrences of repeated sounds.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AAmbientSound, Deactivate)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT(activator, AActor);

	self->Deactivate(activator);
	if (self->special2)
	{
		self->special2 = false;
		FAmbientSound *ambient = Ambients.CheckKey(self->args[0]);
		if (ambient != NULL && (ambient->type & CONTINUOUS) == CONTINUOUS)
		{
			S_StopSound (self, CHAN_BODY);
		}
	}
	return 0;
}


//==========================================================================
//
// S_ParseMusInfo
// Parses MUSINFO lump.
//
//==========================================================================

void S_ParseMusInfo()
{
	int lastlump = 0, lump;

	while ((lump = fileSystem.FindLump ("MUSINFO", &lastlump)) != -1)
	{
		FScanner sc(lump);

		while (sc.GetString())
		{
			level_info_t *map = FindLevelInfo(sc.String);

			if (map == NULL)
			{
				// Don't abort for invalid maps
				sc.ScriptMessage("Unknown map '%s'", sc.String);
			}
			while (sc.CheckNumber())
			{
				int index = sc.Number;
				sc.MustGetString();
				if (index > 0)
				{
					FName music = sc.String;
					if (map != NULL)
					{
						map->MusicMap[index] = music;
					}
				}
			}
		}
	}
}


DEFINE_ACTION_FUNCTION(DObject, MarkSound)
{
	PARAM_PROLOGUE;
	PARAM_SOUND(sound_id);
	soundEngine->MarkUsed(sound_id);
	return 0;
}

