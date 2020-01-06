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

#include "templates.h"
#include "actor.h"
#include "c_dispatch.h"
#include "w_wad.h"
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
public:
	FPlayerSoundHashTable();
	FPlayerSoundHashTable(const FPlayerSoundHashTable &other);
	~FPlayerSoundHashTable();

	void AddSound (int player_sound_id, int sfx_id);
	int LookupSound (int player_sound_id);
	FPlayerSoundHashTable &operator= (const FPlayerSoundHashTable &other);
	void MarkUsed();

protected:
	struct Entry
	{
		Entry *Next;
		int PlayerSoundID;
		int SfxID;
	};
	enum { NUM_BUCKETS = 23 };
	Entry *Buckets[NUM_BUCKETS];

	void Init ();
	void Free ();
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

// music volume multipliers
struct FMusicVolume
{
	FMusicVolume *Next;
	float Volume;
	char MusicName[1];
};

// This is used to recreate the skin sounds after reloading SNDINFO due to a changed local one.
struct FSavedPlayerSoundInfo
{
	FName pclass;
	int gender;
	int refid;
	int lumpnum;
	bool alias;
};

// This specifies whether Timidity or Windows playback is preferred for a certain song (only useful for Windows.)
MusicAliasMap MusicAliases;
MidiDeviceMap MidiDevices;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern bool IsFloat (const char *str);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int SortPlayerClasses (const void *a, const void *b);
static int S_DupPlayerSound (const char *pclass, int gender, int refid, int aliasref);
static void S_SavePlayerSound (const char *pclass, int gender, int refid, int lumpnum, bool alias);
static void S_RestorePlayerSounds();
static int S_AddPlayerClass (const char *name);
static int S_AddPlayerGender (int classnum, int gender);
static int S_FindPlayerClass (const char *name);
static int S_LookupPlayerSound (int classidx, int gender, FSoundID refid);
static void S_ParsePlayerSoundCommon (FScanner &sc, FString &pclass, int &gender, int &refid);
static void S_AddSNDINFO (int lumpnum);
static void S_AddBloodSFX (int lumpnum);
static void S_AddStrifeVoice (int lumpnum);
static int S_AddSound (const char *logicalname, int lumpnum, FScanner *sc=NULL);

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
	NULL
};

static FMusicVolume *MusicVolumes;
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
// S_GetMusicVolume
//
// Gets the relative volume for the given music track
//==========================================================================

float S_GetMusicVolume (const char *music)
{
	FMusicVolume *musvol = MusicVolumes;

	while (musvol != NULL)
	{
		if (!stricmp (music, musvol->MusicName))
		{
			return musvol->Volume;
		}
		musvol = musvol->Next;
	}
	return 1.f;
}

//==========================================================================
//
// S_CheckIntegrity
//
// Scans the entire sound list and looks for recursive definitions.
//==========================================================================

static bool S_CheckSound(sfxinfo_t *startsfx, sfxinfo_t *sfx, TArray<sfxinfo_t *> &chain)
{
	auto &S_sfx = soundEngine->GetSounds();
	sfxinfo_t *me = sfx;
	bool success = true;
	unsigned siz = chain.Size();

	if (sfx->bPlayerReserve)
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
			auto rsfx = &S_sfx[list->Choices[i]];
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
		me = &S_sfx[me->link];
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

	auto &S_sfx = soundEngine->GetSounds();
	broken.Resize(S_sfx.Size());
	memset(&broken[0], 0, sizeof(bool)*S_sfx.Size());
	for (unsigned i = 0; i < S_sfx.Size(); i++)
	{
		auto &sfx = S_sfx[i];
		broken[i] = !S_CheckSound(&sfx, &sfx, chain);
	}
	for (unsigned i = 0; i < S_sfx.Size(); i++)
	{
		if (broken[i])
		{
			auto &sfx = S_sfx[i];
			Printf(TEXTCOLOR_RED "Sound %s has been disabled\n", sfx.name.GetChars());
			sfx.bRandomHeader = false;
			sfx.link = 0;	// link to the empty sound.
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
	auto &S_sfx = soundEngine->GetSounds();
	if ((unsigned int)sound >= S_sfx.Size())
	{
		return 0;
	}

	sfxinfo_t *sfx = &S_sfx[sound];

	// Resolve player sounds, random sounds, and aliases
	if (sfx->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->bPlayerReserve)
		{
			sfx = &S_sfx[S_FindSkinnedSound (NULL, sound)];
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
			sfx = &S_sfx[sfx->link];
		}
	}

	sfx = soundEngine->LoadSound(sfx, nullptr);
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

int S_AddSound (const char *logicalname, const char *lumpname, FScanner *sc)
{
	int lump = Wads.CheckNumForFullName (lumpname, true, ns_sounds);
	return S_AddSound (logicalname, lump);
}

static int S_AddSound (const char *logicalname, int lumpnum, FScanner *sc)
{
	auto &S_sfx = soundEngine->GetSounds();
	int sfxid;

	sfxid = soundEngine->FindSoundNoHash (logicalname);

	if (sfxid > 0)
	{ // If the sound has already been defined, change the old definition
		sfxinfo_t *sfx = &S_sfx[sfxid];

		if (sfx->bPlayerReserve)
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
		if (sfx->bPlayerCompat)
		{
			sfx = &S_sfx[sfx->link];
		}
		if (sfx->bRandomHeader)
		{
			FRandomSoundList* rnd = soundEngine->ResolveRandomSound(sfx);
			rnd->Choices.Reset();
			rnd->Owner = 0;
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

int S_AddPlayerSound (const char *pclass, int gender, int refid,
	const char *lumpname)
{
	int lump=-1;
	
	if (lumpname)
	{
		lump = Wads.CheckNumForFullName (lumpname, true, ns_sounds);
	}

	return S_AddPlayerSound (pclass, gender, refid, lump);
}

int S_AddPlayerSound (const char *pclass, int gender, int refid, int lumpnum, bool fromskin)
{
	auto &S_sfx = soundEngine->GetSounds();
	FString fakename;
	int id;

	fakename = pclass;
	fakename += '"';
	fakename += '0' + gender;
	fakename += '"';
	fakename += S_sfx[refid].name;

	id = soundEngine->AddSoundLump (fakename, lumpnum, CurrentPitchMask);
	int classnum = S_AddPlayerClass (pclass);
	int soundlist = S_AddPlayerGender (classnum, gender);

	PlayerSounds[soundlist].AddSound (S_sfx[refid].link, id);

	if (fromskin) S_SavePlayerSound(pclass, gender, refid, lumpnum, false);

	return id;
}

//==========================================================================
//
// S_AddPlayerSoundExisting
//
// Adds the player sound as an alias to an existing sound.
//==========================================================================

int S_AddPlayerSoundExisting (const char *pclass, int gender, int refid,
	int aliasto, bool fromskin)
{
	int classnum = S_AddPlayerClass (pclass);
	int soundlist = S_AddPlayerGender (classnum, gender);

	auto &S_sfx = soundEngine->GetSounds();
	PlayerSounds[soundlist].AddSound (S_sfx[refid].link, aliasto);

	if (fromskin) S_SavePlayerSound(pclass, gender, refid, aliasto, true);

	return aliasto;
}

//==========================================================================
//
// S_DupPlayerSound
//
// Adds a player sound that uses the same sound as an existing player sound.
//==========================================================================

int S_DupPlayerSound (const char *pclass, int gender, int refid, int aliasref)
{
	int aliasto = S_LookupPlayerSound (pclass, gender, aliasref);
	return S_AddPlayerSoundExisting (pclass, gender, refid, aliasto);
}

//==========================================================================
//
// FPlayerSoundHashTable constructor
//
//==========================================================================

FPlayerSoundHashTable::FPlayerSoundHashTable ()
{
	Init();
}

//==========================================================================
//
// FPlayerSoundHashTable copy constructor
//
//==========================================================================

FPlayerSoundHashTable::FPlayerSoundHashTable (const FPlayerSoundHashTable &other)
{
	Init();
	*this = other;
}

//==========================================================================
//
// FPlayerSoundHashTable destructor
//
//==========================================================================

FPlayerSoundHashTable::~FPlayerSoundHashTable ()
{
	Free ();
}

//==========================================================================
//
// FPlayerSoundHashTable :: Init
//
//==========================================================================

void FPlayerSoundHashTable::Init ()
{
	for (int i = 0; i < NUM_BUCKETS; ++i)
	{
		Buckets[i] = NULL;
	}
}

//==========================================================================
//
// FPlayerSoundHashTable :: Free
//
//==========================================================================

void FPlayerSoundHashTable::Free ()
{
	for (int i = 0; i < NUM_BUCKETS; ++i)
	{
		Entry *entry, *next;

		for (entry = Buckets[i]; entry != NULL; )
		{
			next = entry->Next;
			delete entry;
			entry = next;
		}
		Buckets[i] = NULL;
	}
}

//==========================================================================
//
// FPlayerSoundHashTable :: operator=
//
//==========================================================================

FPlayerSoundHashTable &FPlayerSoundHashTable::operator= (const FPlayerSoundHashTable &other)
{
	Free ();
	for (int i = 0; i < NUM_BUCKETS; ++i)
	{
		Entry *entry;

		for (entry = other.Buckets[i]; entry != NULL; entry = entry->Next)
		{
			AddSound (entry->PlayerSoundID, entry->SfxID);
		}
	}
	return *this;
}

//==========================================================================
//
// FPlayerSoundHashTable :: AddSound
//
//==========================================================================

void FPlayerSoundHashTable::AddSound (int player_sound_id, int sfx_id)
{
	Entry *entry;
	unsigned bucket_num = (unsigned)player_sound_id % NUM_BUCKETS;

	// See if the entry exists already.
	for (entry = Buckets[bucket_num];
		 entry != NULL && entry->PlayerSoundID != player_sound_id;
		 entry = entry->Next)
	{ }

	if (entry != NULL)
	{ // If the player sound is already present, redefine it.
		entry->SfxID = sfx_id;
	}
	else
	{ // Otherwise, add it to the start of its bucket.
		entry = new Entry;
		entry->Next = Buckets[bucket_num];
		entry->PlayerSoundID = player_sound_id;
		entry->SfxID = sfx_id;
		Buckets[bucket_num] = entry;
	}
}

//==========================================================================
//
// FPlayerSoundHashTable :: LookupSound
//
//==========================================================================

int FPlayerSoundHashTable::LookupSound (int player_sound_id)
{
	Entry *entry;
	unsigned bucket_num = (unsigned)player_sound_id % NUM_BUCKETS;

	// See if the entry exists already.
	for (entry = Buckets[bucket_num];
		 entry != NULL && entry->PlayerSoundID != player_sound_id;
		 entry = entry->Next)
	{ }

	return entry != NULL ? entry->SfxID : 0;
}

//==========================================================================
//
// FPlayerSoundHashTable :: Mark
//
// Marks all sounds defined for this class/gender as used.
//
//==========================================================================

void FPlayerSoundHashTable::MarkUsed()
{
	for (size_t i = 0; i < NUM_BUCKETS; ++i)
	{
		for (Entry *probe = Buckets[i]; probe != NULL; probe = probe->Next)
		{
			soundEngine->MarkUsed(probe->SfxID);
		}
	}
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
	while (MusicVolumes != NULL)
	{
		FMusicVolume *me = MusicVolumes;
		MusicVolumes = me->Next;
		M_Free(me);
	}

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
	auto &S_sfx = soundEngine->GetSounds();
	int lump;

	if (!redefine) SavedPlayerSounds.Clear();	// clear skin sounds only for initial parsing.
	S_ClearSoundData();	// remove old sound data first!

	CurrentPitchMask = 0;
	S_AddSound ("{ no sound }", "DSEMPTY");	// Sound 0 is no sound at all
	for (lump = 0; lump < Wads.GetNumLumps(); ++lump)
	{
		switch (Wads.GetLumpNamespace (lump))
		{
		case ns_global:
			if (Wads.CheckLumpName (lump, "SNDINFO"))
			{
				S_AddSNDINFO (lump);
			}
			break;

		case ns_bloodsfx:
			S_AddBloodSFX (lump);
			break;

		case ns_strifevoices:
			S_AddStrifeVoice (lump);
			break;
		}
	}
	S_RestorePlayerSounds();
	soundEngine->HashSounds ();

	S_ShrinkPlayerSoundLists ();

	sfx_empty = Wads.CheckNumForName ("dsempty", ns_sounds);
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
	auto &S_sfx = soundEngine->GetSounds();
	bool skipToEndIf;
	TArray<uint32_t> list;

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
				ambient->sound = 0;

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
				int gender, refid, sfxnum;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				sfxnum = S_AddPlayerSound (pclass, gender, refid, sc.String);
				if (0 == stricmp(sc.String, "dsempty"))
				{
					S_sfx[sfxnum].bPlayerSilent = true;
				}
				}
				break;

			case SI_PlayerSoundDup: {
				// $playersounddup <player class> <gender> <logical name> <target sound name>
				FString pclass;
				int gender, refid, targid;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				targid = soundEngine->FindSoundNoHash (sc.String);
				if (!S_sfx[targid].bPlayerReserve)
				{
					sc.ScriptError ("%s is not a player sound", sc.String);
				}
				S_DupPlayerSound (pclass, gender, refid, targid);
				}
				break;

			case SI_PlayerCompat: {
				// $playercompat <player class> <gender> <logical name> <compat sound name>
				FString pclass;
				int gender, refid;
				int sfxfrom, aliasto;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				sfxfrom = S_AddSound (sc.String, -1, &sc);
				aliasto = S_LookupPlayerSound (pclass, gender, refid);
				S_sfx[sfxfrom].link = aliasto;
				S_sfx[sfxfrom].bPlayerCompat = true;
				}
				break;

			case SI_PlayerAlias: {
				// $playeralias <player class> <gender> <logical name> <logical name of existing sound>
				FString pclass;
				int gender, refid;
				int soundnum;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				soundnum = soundEngine->FindSoundTentative (sc.String);
				S_AddPlayerSoundExisting (pclass, gender, refid, soundnum);
				}
				break;

			case SI_Alias: {
				// $alias <name of alias> <name of real sound>
				int sfxfrom;

				sc.MustGetString ();
				sfxfrom = S_AddSound (sc.String, -1, &sc);
				sc.MustGetString ();
				if (S_sfx[sfxfrom].bPlayerCompat)
				{
					sfxfrom = S_sfx[sfxfrom].link;
				}
				S_sfx[sfxfrom].link = soundEngine->FindSoundTentative (sc.String);
				S_sfx[sfxfrom].NearLimit = -1;	// Aliases must use the original sound's limit.
				}
				break;

			case SI_Limit: {
				// $limit <logical name> <max channels> [<distance>]
				int sfx;

				sc.MustGetString ();
				sfx = soundEngine->FindSoundTentative (sc.String);
				sc.MustGetNumber ();
				S_sfx[sfx].NearLimit = MIN(MAX(sc.Number, 0), 255);
				if (sc.CheckFloat())
				{
					S_sfx[sfx].LimitRange = float(sc.Float * sc.Float);
				}
				}
				break;

			case SI_Singular: {
				// $singular <logical name>
				int sfx;

				sc.MustGetString ();
				sfx = soundEngine->FindSoundTentative (sc.String);
				S_sfx[sfx].bSingular = true;
				}
				break;

			case SI_PitchShift: {
				// $pitchshift <logical name> <pitch shift amount>
				int sfx;

				sc.MustGetString ();
				sfx = soundEngine->FindSoundTentative (sc.String);
				sc.MustGetNumber ();
				S_sfx[sfx].PitchMask = (1 << clamp (sc.Number, 0, 7)) - 1;
				}
				break;

			case SI_PitchShiftRange:
				// $pitchshiftrange <pitch shift amount>
				sc.MustGetNumber ();
				CurrentPitchMask = (1 << clamp (sc.Number, 0, 7)) - 1;
				break;

			case SI_Volume: {
				// $volume <logical name> <volume>
				int sfx;

				sc.MustGetString();
				sfx = soundEngine->FindSoundTentative(sc.String);
				sc.MustGetFloat();
				S_sfx[sfx].Volume = (float)sc.Float;
				}
				break;

			case SI_Attenuation: {
				// $attenuation <logical name> <attenuation>
				int sfx;

				sc.MustGetString();
				sfx = soundEngine->FindSoundTentative(sc.String);
				sc.MustGetFloat();
				S_sfx[sfx].Attenuation = (float)sc.Float;
				}
				break;

			case SI_Rolloff: {
				// $rolloff *|<logical name> [linear|log|custom] <min dist> <max dist/rolloff factor>
				// Using * for the name makes it the default for sounds that don't specify otherwise.
				FRolloffInfo *rolloff;
				int type;
				int sfx;

				sc.MustGetString();
				if (sc.Compare("*"))
				{
					sfx = -1;
					rolloff = &soundEngine->GlobalRolloff();
				}
				else
				{
					sfx = soundEngine->FindSoundTentative(sc.String);
					rolloff = &S_sfx[sfx].Rolloff;
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
				uint32_t Owner = S_AddSound (sc.String, -1, &sc);
				sc.MustGetStringName ("{");
				while (sc.GetString () && !sc.Compare ("}"))
				{
					uint32_t sfxto = soundEngine->FindSoundTentative (sc.String);
					if (sfxto == random.Owner)
					{
						Printf("Definition of random sound '%s' refers to itself recursively.\n", sc.String);
						continue;
					}
					list.Push (sfxto);
				}
				if (list.Size() == 1)
				{ // Only one sound: treat as $alias
					S_sfx[Owner].link = list[0];
					S_sfx[Owner].NearLimit = -1;
				}
				else if (list.Size() > 1)
				{ // Only add non-empty random lists
					soundEngine->AddRandomSound(Owner, list);
				}
				}
				break;

			case SI_MusicVolume: {
				sc.MustGetString();
				FString musname (sc.String);
				sc.MustGetFloat();
				FMusicVolume *mv = (FMusicVolume *)M_Malloc (sizeof(*mv) + musname.Len());
				mv->Volume = (float)sc.Float;
				strcpy (mv->MusicName, musname);
				mv->Next = MusicVolumes;
				MusicVolumes = mv;
				}
				break;

			case SI_MusicAlias: {
				sc.MustGetString();
				int lump = Wads.CheckNumForName(sc.String, ns_music);
				if (lump >= 0)
				{
					// do not set the alias if a later WAD defines its own music of this name
					int file = Wads.GetLumpFile(lump);
					int sndifile = Wads.GetLumpFile(sc.LumpNum);
					if (file > sndifile)
					{
						sc.MustGetString();
						continue;
					}
				}
				FName alias = sc.String;
				sc.MustGetString();
				FName mapped = sc.String;

				// only set the alias if the lump it maps to exists.
				if (mapped == NAME_None || Wads.CheckNumForFullName(sc.String, true, ns_music) >= 0)
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
			sc.MustGetString ();
			S_AddSound (name, sc.String, &sc);
		}
	}
}

//==========================================================================
//
// S_AddBloodSFX
//
// Registers a new sound with the name "<lumpname>.sfx"
// Actual sound data is searched for in the ns_bloodraw namespace.
//
//==========================================================================

static void S_AddBloodSFX (int lumpnum)
{
	FMemLump sfxlump = Wads.ReadLump(lumpnum);
	const FBloodSFX *sfx = (FBloodSFX *)sfxlump.GetMem();
	int rawlump = Wads.CheckNumForName(sfx->RawName, ns_bloodraw);
	int sfxnum;

	if (rawlump != -1)
	{
		auto &S_sfx = soundEngine->GetSounds();
		const char *name = Wads.GetLumpFullName(lumpnum);
		sfxnum = S_AddSound(name, rawlump);
		if (sfx->Format < 5 || sfx->Format > 12)
		{	// [0..4] + invalid formats
			S_sfx[sfxnum].RawRate = 11025;
		}
		else if (sfx->Format < 9)
		{	// [5..8]
			S_sfx[sfxnum].RawRate = 22050;
		}
		else
		{	// [9..12]
			S_sfx[sfxnum].RawRate = 44100;
		}
		S_sfx[sfxnum].bLoadRAW = true;
		S_sfx[sfxnum].LoopStart = LittleLong(sfx->LoopStart);
		// Make an ambient sound out of it, whether it has a loop point
		// defined or not. (Because none of the standard Blood ambient
		// sounds are explicitly defined as looping.)
		FAmbientSound *ambient = &Ambients[Wads.GetLumpIndexNum(lumpnum)];
		ambient->type = CONTINUOUS;
		ambient->periodmin = 0;
		ambient->periodmax = 0;
		ambient->volume = 1;
		ambient->attenuation = 1;
		ambient->sound = FSoundID(sfxnum);
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
	Wads.GetLumpName (name+5, lumpnum);
	S_AddSound (name, lumpnum);
}

//==========================================================================
//
// S_ParsePlayerSoundCommon
//
// Parses the common part of playersound commands in SNDINFO
//	(player class, gender, and ref id)
//==========================================================================

static void S_ParsePlayerSoundCommon (FScanner &sc, FString &pclass, int &gender, int &refid)
{
	sc.MustGetString ();
	pclass = sc.String;
	sc.MustGetString ();
	gender = D_GenderToInt (sc.String);
	sc.MustGetString ();
	refid = soundEngine->FindSoundNoHash (sc.String);
	auto &S_sfx = soundEngine->GetSounds();
	if (refid != 0 && !S_sfx[refid].bPlayerReserve && !S_sfx[refid].bTentative)
	{
		sc.ScriptError ("%s has already been used for a non-player sound.", sc.String);
	}
	if (refid == 0)
	{
		refid = S_AddSound (sc.String, -1, &sc);
		S_sfx[refid].bTentative = true;
	}
	if (S_sfx[refid].bTentative)
	{
		S_sfx[refid].link = NumPlayerReserves++;
		S_sfx[refid].bTentative = false;
		S_sfx[refid].bPlayerReserve = true;
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

int S_LookupPlayerSound (const char *pclass, int gender, const char *name)
{
	int refid = S_FindSound (name);
	if (refid != 0)
	{
		refid = S_LookupPlayerSound (pclass, gender, refid);
	}
	return refid;
}

int S_LookupPlayerSound (const char *pclass, int gender, FSoundID refid)
{
	auto &S_sfx = soundEngine->GetSounds();
	if (!S_sfx[refid].bPlayerReserve)
	{ // Not a player sound, so just return this sound
		return refid;
	}

	return S_LookupPlayerSound (S_FindPlayerClass (pclass), gender, refid);
}

static int S_LookupPlayerSound (int classidx, int gender, FSoundID refid)
{
	auto &S_sfx = soundEngine->GetSounds();
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
			return 0;
		}
		gender = g;
	}

	int sndnum = PlayerSounds[listidx].LookupSound (S_sfx[refid].link);

	// If we're not done parsing SNDINFO yet, assume that the target sound is valid
	if (PlayerClassesIsSorted &&
		(sndnum == 0 ||
		((S_sfx[sndnum].lumpnum == -1 || S_sfx[sndnum].lumpnum == sfx_empty) &&
		 S_sfx[sndnum].link == sfxinfo_t::NO_LINK &&
		 !S_sfx[sndnum].bPlayerSilent)))
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

static void S_SavePlayerSound (const char *pclass, int gender, int refid, int lumpnum, bool alias)
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
			S_AddPlayerSoundExisting(spi->pclass, spi->gender, spi->refid, spi->lumpnum);
		}
		else
		{
			S_AddPlayerSound(spi->pclass, spi->gender, spi->refid, spi->lumpnum);
		}
	}
}

//==========================================================================
//
// S_AreSoundsEquivalent
//
// Returns true if two sounds are essentially the same thing
//==========================================================================

bool S_AreSoundsEquivalent (AActor *actor, const char *name1, const char *name2)
{
	return S_AreSoundsEquivalent (actor, S_FindSound (name1), S_FindSound (name2));
}

bool S_AreSoundsEquivalent (AActor *actor, int id1, int id2)
{
	sfxinfo_t *sfx;

	auto &S_sfx = soundEngine->GetSounds();
	if (id1 == id2)
	{
		return true;
	}
	if (id1 == 0 || id2 == 0)
	{
		return false;
	}
	// Dereference aliases, but not random or player sounds
	while ((sfx = &S_sfx[id1])->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->bPlayerReserve)
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
	while ((sfx = &S_sfx[id2])->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->bPlayerReserve)
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

static const char *GetSoundClass(AActor *pp)
{
	auto player = pp->player;
	if (player != nullptr &&
		(player->mo == nullptr || !(player->mo->flags4 &MF4_NOSKIN)) &&
		(unsigned int)player->userinfo.GetSkin() >= PlayerClasses.Size() &&
		(unsigned)player->userinfo.GetSkin() < Skins.Size())
	{
		return Skins[player->userinfo.GetSkin()].Name.GetChars();
	}
	auto sclass = player? pp->NameVar(NAME_SoundClass) : NAME_None;

	return sclass != NAME_None ? sclass.GetChars() : "player";
}

//==========================================================================
//
// S_FindSkinnedSound
//
// Calls S_LookupPlayerSound, deducing the class and gender from actor.
//==========================================================================

int S_FindSkinnedSound (AActor *actor, FSoundID refid)
{
	const char *pclass;
	int gender = 0;

	if (actor != nullptr)
	{
		pclass = GetSoundClass (actor);
		if (actor->player != nullptr) gender = actor->player->userinfo.GetGender();
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

int S_FindSkinnedSoundEx (AActor *actor, const char *name, const char *extendedname)
{
	FString fullname;

	// Look for "name-extendedname";
	fullname = name;
	fullname += '-';
	fullname += extendedname;
	FSoundID id = fullname;

	if (id == 0)
	{ // Look for "name"
		id = name;
	}
	return S_FindSkinnedSound (actor, id);
}

//==========================================================================
//
// sfxinfo_t :: MarkUsed
//
// Marks this sound for precaching.
//
//==========================================================================

void sfxinfo_t::MarkUsed()
{
	bUsed = true;
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
	const char *playerclass = GetSoundClass(player);
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
	auto &S_sfx = soundEngine->GetSounds();
	unsigned int i;

	for (i = 0; i < S_sfx.Size (); i++)
	{
		const sfxinfo_t *sfx = &S_sfx[i];

		if (sfx->link != sfxinfo_t::NO_LINK &&
			!sfx->bRandomHeader &&
			!sfx->bPlayerReserve)
		{
			Printf ("%s -> %s\n", sfx->name.GetChars(), S_sfx[sfx->link].name.GetChars());
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
	auto &S_sfx = soundEngine->GetSounds();
	const char *reserveNames[256];
	unsigned int i;
	int j, k, l;

	// Find names for the player sounds
	memset (reserveNames, 0, sizeof(reserveNames));
	for (i = j = 0; j < NumPlayerReserves && i < S_sfx.Size(); ++i)
	{
		if (S_sfx[i].bPlayerReserve)
		{
			++j;
			reserveNames[S_sfx[i].link] = S_sfx[i].name;
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
					Printf (" %-16s%s\n", reserveNames[k], S_sfx[PlayerSounds[l].LookupSound (k)].name.GetChars());
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

	if (ambient->sound != FSoundID(0))
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
			if (amb->sound == 0)
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

	while ((lump = Wads.FindLump ("MUSINFO", &lastlump)) != -1)
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

