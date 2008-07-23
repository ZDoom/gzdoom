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
#include "a_sharedglobal.h"
#include "s_sound.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "sc_man.h"
#include "g_level.h"
#include "cmdlib.h"
#include "gi.h"
#include "doomstat.h"
#include "i_sound.h"
#include "r_data.h"
#include "m_random.h"
#include "d_netinf.h"
#include "i_system.h"

// MACROS ------------------------------------------------------------------

#define RANDOM		1
#define PERIODIC	2
#define CONTINUOUS	3
#define POSITIONAL	4
#define SURROUND	16

// TYPES -------------------------------------------------------------------

struct FRandomSoundList
{
	FRandomSoundList()
	: Sounds(0), SfxHead(0), NumSounds(0)
	{
	}
	~FRandomSoundList()
	{
		if (Sounds != NULL)
		{
			delete[] Sounds;
			Sounds = NULL;
		}
	}

	WORD		*Sounds;	// A list of sounds that can result for the following id
	WORD		SfxHead;	// The sound id used to reference this list
	WORD		NumSounds;
};

struct FPlayerClassLookup
{
	FString		Name;
	WORD		ListIndex[3];	// indices into PlayerSounds (0xffff means empty)
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

static struct AmbientSound
{
	unsigned	type;		// type of ambient sound
	int			periodmin;	// # of tics between repeats
	int			periodmax;	// max # of tics for random ambients
	float		volume;		// relative volume of sound
	float		attenuation;
	FString		sound;		// Logical name of sound to play
} *Ambients[256];

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
};

// Blood was a cool game. If Monolith ever releases the source for it,
// you can bet I'll port it.

struct FBloodSFX
{
	DWORD	RelVol;		// volume, 0-255
	fixed_t	Pitch;		// pitch change
	fixed_t	PitchRange;	// range of random pitch
	DWORD	Format;		// format of audio 1=11025 5=22050
	SDWORD	LoopStart;	// loop position (-1 means no looping)
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
MidiDeviceMap MidiDevices;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern bool IsFloat (const char *str);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int STACK_ARGS SortPlayerClasses (const void *a, const void *b);
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

extern int sfx_sawup, sfx_sawidl, sfx_sawful, sfx_sawhit;
extern int sfx_itemup, sfx_tink;
extern int sfx_empty;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TArray<sfxinfo_t> S_sfx (128);

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
	NULL
};

static TArray<FRandomSoundList> S_rnd;
static FMusicVolume *MusicVolumes;
static TArray<FSavedPlayerSoundInfo> SavedPlayerSounds;

static int NumPlayerReserves;
static bool PlayerClassesIsSorted;

static TArray<FPlayerClassLookup> PlayerClassLookups;
static TArray<FPlayerSoundHashTable> PlayerSounds;

static FString DefPlayerClassName;
static int DefPlayerClass;

static BYTE CurrentPitchMask;

static FRandom pr_randsound ("RandSound");

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
// S_HashSounds
//
// Fills in the next and index fields of S_sfx to form a working hash table.
//==========================================================================

void S_HashSounds ()
{
	unsigned int i;
	unsigned int j;
	unsigned int size;

	S_sfx.ShrinkToFit ();
	size = S_sfx.Size ();

	// Mark all buckets as empty
	for (i = 0; i < size; i++)
		S_sfx[i].index = 0;

	// Now set up the chains
	for (i = 1; i < size; i++)
	{
		j = MakeKey (S_sfx[i].name) % size;
		S_sfx[i].next = S_sfx[j].index;
		S_sfx[j].index = i;
	}
}

//==========================================================================
//
// S_PickReplacement
//
// Picks a replacement sound from the associated random list. If this sound
// is not the head of a random list, then the sound passed is returned.
//==========================================================================

int S_PickReplacement (int refid)
{
	if (S_sfx[refid].bRandomHeader)
	{
		const FRandomSoundList *list = &S_rnd[S_sfx[refid].link];

		return list->Sounds[pr_randsound() % list->NumSounds];
	}
	return refid;
}

//==========================================================================
//
// S_CacheRandomSound
//
// Loads all sounds a random sound might play.
//
//==========================================================================

void S_CacheRandomSound (sfxinfo_t *sfx)
{
	if (sfx->bRandomHeader)
	{
		const FRandomSoundList *list = &S_rnd[sfx->link];
		for (int i = 0; i < list->NumSounds; ++i)
		{
			sfx = &S_sfx[list->Sounds[i]];
			sfx->bUsed = true;
			S_CacheSound (&S_sfx[list->Sounds[i]]);
		}
	}
}

//==========================================================================
//
// S_FindSound
//
// Given a logical name, find the sound's index in S_sfx.
//==========================================================================

int S_FindSound (const char *logicalname)
{
	int i;

	if (logicalname != NULL)
	{
		i = S_sfx[MakeKey (logicalname) % S_sfx.Size ()].index;

		while ((i != 0) && stricmp (S_sfx[i].name, logicalname))
			i = S_sfx[i].next;

		return i;
	}
	else
	{
		return 0;
	}
}

//==========================================================================
//
// S_FindSoundNoHash
//
// Given a logical name, find the sound's index in S_sfx without
// using the hash table.
//==========================================================================

int S_FindSoundNoHash (const char *logicalname)
{
	unsigned int i;

	for (i = 1; i < S_sfx.Size (); i++)
	{
		if (stricmp (S_sfx[i].name, logicalname) == 0)
		{
			return i;
		}
	}
	return 0;
}

//==========================================================================
//
// S_FindSoundByLump
//
// Given a sound lump, find the sound's index in S_sfx.
//==========================================================================

int S_FindSoundByLump (int lump)
{
	if (lump != -1)
	{
		unsigned int i;

		for (i = 1; i < S_sfx.Size (); i++)
			if (S_sfx[i].lumpnum == lump)
				return i;
	}
	return 0;
}

//==========================================================================
//
// S_AddSoundLump
//
// Adds a new sound mapping to S_sfx.
//==========================================================================

int S_AddSoundLump (const char *logicalname, int lump)
{
	sfxinfo_t newsfx;

	newsfx.data = NULL;
	newsfx.name = logicalname;
	newsfx.lumpnum = lump;
	newsfx.next = 0;
	newsfx.index = 0;
	newsfx.Volume = 1;
	newsfx.PitchMask = CurrentPitchMask;
	newsfx.NearLimit = 2;
	newsfx.bRandomHeader = false;
	newsfx.bPlayerReserve = false;
	newsfx.bForce11025 = false;
	newsfx.bForce22050 = false;
	newsfx.bLoadRAW = false;
	newsfx.bPlayerCompat = false;
	newsfx.b16bit = false;
	newsfx.bUsed = false;
	newsfx.bSingular = false;
	newsfx.bTentative = false;
	newsfx.RolloffType = ROLLOFF_Doom;
	newsfx.link = sfxinfo_t::NO_LINK;
	newsfx.MinDistance = 0;
	newsfx.MaxDistance = 0;

	return (int)S_sfx.Push (newsfx);
}

//==========================================================================
//
// S_FindSoundTentative
//
// Given a logical name, find the sound's index in S_sfx without
// using the hash table. If it does not exist, a new sound without
// an associated lump is created.
//==========================================================================

int S_FindSoundTentative (const char *name)
{
	int id = S_FindSoundNoHash (name);
	if (id == 0)
	{
		id = S_AddSoundLump (name, -1);
		S_sfx[id].bTentative = true;
	}
	return id;
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
	int sfxid;

	sfxid = S_FindSoundNoHash (logicalname);

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
			FRandomSoundList *rnd = &S_rnd[sfx->link];
			delete[] rnd->Sounds;
			rnd->Sounds = NULL;
			rnd->NumSounds = 0;
			rnd->SfxHead = 0;
		}
		sfx->lumpnum = lumpnum;
		sfx->bRandomHeader = false;
		sfx->link = sfxinfo_t::NO_LINK;
		sfx->bTentative = false;
		if (sfx->NearLimit == -1) sfx->NearLimit = 2;
		//sfx->PitchMask = CurrentPitchMask;
	}
	else
	{ // Otherwise, create a new definition.
		sfxid = S_AddSoundLump (logicalname, lumpnum);
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
	FString fakename;
	int id;

	fakename = pclass;
	fakename += '"';
	fakename += '0' + gender;
	fakename += '"';
	fakename += S_sfx[refid].name;

	id = S_AddSoundLump (fakename, lumpnum);
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
// S_ClearSoundData
//
// clears all sound tables
// When we want to allow level specific SNDINFO lumps this has to
// be cleared for each level
//==========================================================================

static void S_ClearSoundData()
{
	unsigned int i;

	S_StopAllChannels();
	for (i = 0; i < S_sfx.Size(); ++i)
	{
		GSnd->UnloadSound(&S_sfx[i]);
	}
	S_sfx.Clear();

	for(i = 0; i < countof(Ambients); i++)
	{
		if (Ambients[i] != NULL)
		{
			delete Ambients[i];
			Ambients[i] = NULL;
		}
	}
	while (MusicVolumes != NULL)
	{
		FMusicVolume *me = MusicVolumes;
		MusicVolumes = me->Next;
		M_Free(me);
	}
	S_rnd.Clear();

	NumPlayerReserves = 0;
	PlayerClassesIsSorted = false;
	PlayerClassLookups.Clear();
	PlayerSounds.Clear();
	DefPlayerClass = 0;
	DefPlayerClassName = "";
}

//==========================================================================
//
// S_ParseSndInfo
//
// Parses all loaded SNDINFO lumps.
// Also registers Blood SFX files and Strife's voices.
//==========================================================================

void S_ParseSndInfo ()
{
	int lump;

	atterm (S_ClearSoundData);
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
	S_HashSounds ();
	S_sfx.ShrinkToFit ();

	if (S_rnd.Size() > 0)
	{
		S_rnd.ShrinkToFit ();
	}

	S_ShrinkPlayerSoundLists ();

	sfx_empty = Wads.CheckNumForName ("dsempty", ns_sounds);
}

//==========================================================================
//
// Adds a level specific SNDINFO lump
//
//==========================================================================

void S_AddLocalSndInfo(int lump)
{
	S_AddSNDINFO(lump);
	S_HashSounds ();
	S_sfx.ShrinkToFit ();

	if (S_rnd.Size() > 0)
	{
		S_rnd.ShrinkToFit ();
	}

	S_ShrinkPlayerSoundLists ();
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
	TArray<WORD> list;

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
				AmbientSound *ambient, dummy;

				sc.MustGetNumber ();
				if (sc.Number < 0 || sc.Number > 255)
				{
					Printf ("Bad ambient index (%d)\n", sc.Number);
					ambient = &dummy;
				}
				else if (Ambients[sc.Number] == NULL)
				{
					ambient = new AmbientSound;
					Ambients[sc.Number] = ambient;
				}
				else
				{
					ambient = Ambients[sc.Number];
				}
				ambient->type = 0;
				ambient->periodmin = 0;
				ambient->periodmax = 0;
				ambient->volume = 0;
				ambient->attenuation = 0;
				ambient->sound = "";

				sc.MustGetString ();
				ambient->sound = sc.String;
				ambient->attenuation = 0;

				sc.MustGetString ();
				if (sc.Compare ("point"))
				{
					float attenuation;

					ambient->type = POSITIONAL;
					sc.MustGetString ();

					if (IsFloat (sc.String))
					{
						attenuation = atof (sc.String);
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
				ambient->volume = sc.Float;
				if (ambient->volume > 1)
					ambient->volume = 1;
				else if (ambient->volume < 0)
					ambient->volume = 0;
				}
				break;

			case SI_Map: {
				// Hexen-style $MAP command
				level_info_t *info;
				char temp[16];

				sc.MustGetNumber ();
				mysnprintf (temp, countof(temp), "MAP%02d", sc.Number);
				info = FindLevelInfo (temp);
				sc.MustGetString ();
				if (info->mapname[0] && (!(info->flags & LEVEL_MUSICDEFINED)))
				{
					ReplaceString (&info->music, sc.String);
				}
				}
				break;

			case SI_Registered:
				// I don't think Hexen even pays attention to the $registered command.
				break;

			case SI_ArchivePath:
				sc.MustGetString ();	// Unused for now
				break;

			case SI_PlayerSound: {
				// $playersound <player class> <gender> <logical name> <lump name>
				FString pclass;
				int gender, refid;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				S_AddPlayerSound (pclass, gender, refid, sc.String);
				}
				break;

			case SI_PlayerSoundDup: {
				// $playersounddup <player class> <gender> <logical name> <target sound name>
				FString pclass;
				int gender, refid, targid;

				S_ParsePlayerSoundCommon (sc, pclass, gender, refid);
				targid = S_FindSoundNoHash (sc.String);
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
				soundnum = S_FindSoundTentative (sc.String);
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
				S_sfx[sfxfrom].link = S_FindSoundTentative (sc.String);
				S_sfx[sfxfrom].NearLimit = -1;	// Aliases must use the original sound's limit.
				}
				break;

			case SI_Limit: {
				// $limit <logical name> <max channels>
				int sfx;

				sc.MustGetString ();
				sfx = S_FindSoundTentative (sc.String);
				sc.MustGetNumber ();
				S_sfx[sfx].NearLimit = MIN(MAX(sc.Number, 0), 255);
				}
				break;

			case SI_Singular: {
				// $singular <logical name>
				int sfx;

				sc.MustGetString ();
				sfx = S_FindSoundTentative (sc.String);
				S_sfx[sfx].bSingular = true;
				}
				break;

			case SI_PitchShift: {
				// $pitchshift <logical name> <pitch shift amount>
				int sfx;

				sc.MustGetString ();
				sfx = S_FindSoundTentative (sc.String);
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
				sfx = S_FindSoundTentative(sc.String);
				sc.MustGetFloat();
				S_sfx[sfx].Volume = sc.Float;
				}
				break;

			case SI_Rolloff: {
				// $rolloff *|<logical name> [linear|log|custom] <min dist> <max dist/rolloff factor>
				// Using * for the name makes it the default for sounds that don't specify otherwise.
				float *min, *max;
				int type;
				int sfx;

				sc.MustGetString();
				if (sc.Compare("*"))
				{
					sfx = -1;
					min = &S_MinDistance;
					max = &S_MaxDistanceOrRolloffFactor;
				}
				else
				{
					sfx = S_FindSoundTentative(sc.String);
					min = &S_sfx[sfx].MinDistance;
					max = &S_sfx[sfx].MaxDistance;
				}
				type = ROLLOFF_Doom;
				if (!sc.CheckFloat())
				{
					sc.MustGetString();
					if (sc.Compare("linear"))
					{
						type = ROLLOFF_Linear;
					}
					else if (sc.Compare("log"))
					{
						type = ROLLOFF_Log;
					}
					else if (sc.Compare("custom"))
					{
						type = ROLLOFF_Custom;
					}
					else
					{
						sc.ScriptError("Unknown rolloff type '%s'", sc.String);
					}
					sc.MustGetFloat();
				}
				if (sfx < 0)
				{
					S_RolloffType = type;
				}
				else
				{
					S_sfx[sfx].RolloffType = type;
				}
				*min = sc.Float;
				sc.MustGetFloat();
				*max = sc.Float;
				break;
			  }

			case SI_Random: {
				// $random <logical name> { <logical name> ... }
				FRandomSoundList random;

				list.Clear ();
				sc.MustGetString ();
				random.SfxHead = S_AddSound (sc.String, -1, &sc);
				sc.MustGetStringName ("{");
				while (sc.GetString () && !sc.Compare ("}"))
				{
					WORD sfxto = S_FindSoundTentative (sc.String);
					if (sfxto == random.SfxHead)
					{
						Printf("Definition of random sound '%s' refers to itself recursively.", sc.String);
						continue;
					}
					list.Push (sfxto);
				}
				if (list.Size() == 1)
				{ // Only one sound: treat as $alias
					S_sfx[random.SfxHead].link = list[0];
					S_sfx[random.SfxHead].NearLimit = -1;
				}
				else if (list.Size() > 1)
				{ // Only add non-empty random lists
					random.NumSounds = (WORD)list.Size();
					S_sfx[random.SfxHead].link = (WORD)S_rnd.Push (random);
					S_sfx[random.SfxHead].bRandomHeader = true;
					S_rnd[S_sfx[random.SfxHead].link].Sounds = new WORD[random.NumSounds];
					memcpy (S_rnd[S_sfx[random.SfxHead].link].Sounds, &list[0], sizeof(WORD)*random.NumSounds);
					S_sfx[random.SfxHead].NearLimit = -1;
				}
				}
				break;

			case SI_MusicVolume: {
				sc.MustGetString();
				FString musname (sc.String);
				sc.MustGetFloat();
				FMusicVolume *mv = (FMusicVolume *)M_Malloc (sizeof(*mv) + musname.Len());
				mv->Volume = sc.Float;
				strcpy (mv->MusicName, musname);
				mv->Next = MusicVolumes;
				MusicVolumes = mv;
				}
				break;

			case SI_MidiDevice: {
				sc.MustGetString();
				FName nm = sc.String;
				sc.MustGetString();
				if (sc.Compare("timidity")) MidiDevices[nm] = MDEV_TIMIDITY;
				else if (sc.Compare("fmod")) MidiDevices[nm] = MDEV_FMOD;
				else if (sc.Compare("standard")) MidiDevices[nm] = MDEV_MMAPI;
				else if (sc.Compare("opl")) MidiDevices[nm] = MDEV_OPL;
				else if (sc.Compare("default")) MidiDevices[nm] = MDEV_DEFAULT;
				else sc.ScriptError("Unknown MIDI device %s\n", sc.String);
				}
				break;

			case SI_IfDoom:
				if (gameinfo.gametype != GAME_Doom)
				{
					skipToEndIf = true;
				}
				break;

			case SI_IfStrife:
				if (gameinfo.gametype != GAME_Strife)
				{
					skipToEndIf = true;
				}
				break;

			case SI_IfHeretic:
				if (gameinfo.gametype != GAME_Heretic)
				{
					skipToEndIf = true;
				}
				break;

			case SI_IfHexen:
				if (gameinfo.gametype != GAME_Hexen)
				{
					skipToEndIf = true;
				}
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
	char name[13];
	FMemLump sfxlump = Wads.ReadLump (lumpnum);
	const FBloodSFX *sfx = (FBloodSFX *)sfxlump.GetMem();
	int rawlump = Wads.CheckNumForName (sfx->RawName, ns_bloodraw);
	int sfxnum;

	if (rawlump != -1)
	{
		Wads.GetLumpName (name, lumpnum);
		name[8] = 0;
		strcat (name, ".SFX");
		sfxnum = S_AddSound (name, rawlump);
		if (sfx->Format == 5)
		{
			S_sfx[sfxnum].bForce22050 = true;
		}
		else // I don't know any other formats for this
		{
			S_sfx[sfxnum].bForce11025 = true;
		}
		S_sfx[sfxnum].bLoadRAW = true;
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
	refid = S_FindSoundNoHash (sc.String);
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
		lookup.ListIndex[2] = lookup.ListIndex[1] = lookup.ListIndex[0] = 0xffff;
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
		PlayerClassLookups[classnum].ListIndex[gender] = (WORD)index;
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

static int STACK_ARGS SortPlayerClasses (const void *a, const void *b)
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
	if (!S_sfx[refid].bPlayerReserve)
	{ // Not a player sound, so just return this sound
		return refid;
	}

	return S_LookupPlayerSound (S_FindPlayerClass (pclass), gender, refid);
}

static int S_LookupPlayerSound (int classidx, int gender, FSoundID refid)
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

		for (g = 0; g < 3 && listidx == 0xffff; ++g)
		{
			listidx = PlayerClassLookups[classidx].ListIndex[g];
		}
		if (g == 3)
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
		((S_sfx[sndnum].lumpnum == -1 || S_sfx[sndnum].lumpnum == sfx_empty) && S_sfx[sndnum].link == sfxinfo_t::NO_LINK)))
	{ // This sound is unavailable.
		if (ingender != 0)
		{ // Try "male"
			return S_LookupPlayerSound (classidx, 0, refid);
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

//==========================================================================
//
// S_FindSkinnedSound
//
// Calls S_LookupPlayerSound, deducing the class and gender from actor.
//==========================================================================

int S_FindSkinnedSound (AActor *actor, FSoundID refid)
{
	const char *pclass;
	int gender = GENDER_MALE;

	if (actor != NULL && actor->IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{
		pclass = static_cast<APlayerPawn*>(actor)->GetSoundClass ();
		if (actor->player != NULL) gender = actor->player->userinfo.gender;
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
	FSoundID id;

	// Look for "name-extendedname";
	fullname = name;
	fullname += '-';
	fullname += extendedname;
	id = fullname;

	if (id == 0)
	{ // Look for "name"
		id = name;
	}
	return S_FindSkinnedSound (actor, id);
}

//==========================================================================
//
// CCMD soundlist
//
//==========================================================================

CCMD (soundlist)
{
	char lumpname[9];
	unsigned int i;

	lumpname[8] = 0;
	for (i = 0; i < S_sfx.Size (); i++)
	{
		const sfxinfo_t *sfx = &S_sfx[i];
		if (sfx->bRandomHeader)
		{
			Printf ("%3d. %s -> #%d {", i, sfx->name.GetChars(), sfx->link);
			const FRandomSoundList *list = &S_rnd[sfx->link];
			for (size_t j = 0; j < list->NumSounds; ++j)
			{
				Printf (" %s ", S_sfx[list->Sounds[j]].name.GetChars());
			}
			Printf ("}\n");
		}
		else if (sfx->bPlayerReserve)
		{
			Printf ("%3d. %s <<player sound %d>>\n", i, sfx->name.GetChars(), sfx->link);
		}
		else if (S_sfx[i].lumpnum != -1)
		{
			Wads.GetLumpName (lumpname, sfx->lumpnum);
			Printf ("%3d. %s (%s)\n", i, sfx->name.GetChars(), lumpname);
		}
		else if (S_sfx[i].link != sfxinfo_t::NO_LINK)
		{
			Printf ("%3d. %s -> %s\n", i, sfx->name.GetChars(), S_sfx[sfx->link].name.GetChars());
		}
		else
		{
			Printf ("%3d. %s **not present**\n", i, sfx->name.GetChars());
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
		for (j = 0; j < 3; ++j)
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

// AAmbientSound implementation ---------------------------------------------

IMPLEMENT_STATELESS_ACTOR (AAmbientSound, Any, 14065, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)
END_DEFAULTS

void AAmbientSound::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << bActive;
	
	int checktime = NextCheck - gametic;
	arc << checktime;
	if (arc.IsLoading ())
	{
		NextCheck = checktime + gametic;
	}
}


void AAmbientSound::Tick ()
{
	Super::Tick ();

	if (!bActive || gametic < NextCheck)
		return;

	AmbientSound *ambient = Ambients[args[0]];
	int loop = 0;

	if ((ambient->type & CONTINUOUS) == CONTINUOUS)
	{
		loop = CHAN_LOOP;
	}

	if (ambient->sound[0])
	{
		S_Sound(this, CHAN_BODY | loop, ambient->sound, ambient->volume, ambient->attenuation);
		if (!loop)
		{
			SetTicker (ambient);
		}
		else
		{
			NextCheck = INT_MAX;
		}
	}
	else
	{
		Destroy ();
	}
}


void AAmbientSound::SetTicker (struct AmbientSound *ambient)
{
	if ((ambient->type & CONTINUOUS) == CONTINUOUS)
	{
		NextCheck += 1;
	}
	else if (ambient->type & RANDOM)
	{
		NextCheck += (int)(((float)rand() / (float)RAND_MAX) *
				(float)(ambient->periodmax - ambient->periodmin)) +
				ambient->periodmin;
	}
	else
	{
		NextCheck += ambient->periodmin;
	}
}

void AAmbientSound::BeginPlay ()
{
	Super::BeginPlay ();
	Activate (NULL);
}

void AAmbientSound::Activate (AActor *activator)
{
	Super::Activate (activator);

	AmbientSound *amb = Ambients[args[0]];

	if (amb == NULL)
	{
		Destroy ();
		return;
	}

	if (!bActive)
	{
		if ((amb->type & 3) == 0 && amb->periodmin == 0)
		{
			int sndnum = S_FindSound(amb->sound);
			if (sndnum == 0)
			{
				Destroy ();
				return;
			}
			amb->periodmin = Scale(GSnd->GetMSLength(&S_sfx[sndnum]), TICRATE, 1000);
		}

		NextCheck = gametic;
		if (amb->type & (RANDOM|PERIODIC))
			SetTicker (amb);

		bActive = true;
	}
}

void AAmbientSound::Deactivate (AActor *activator)
{
	Super::Deactivate (activator);
	if (bActive)
	{
		bActive = false;
		if ((Ambients[args[0]]->type & CONTINUOUS) == CONTINUOUS)
		{
			S_StopSound (this, CHAN_BODY);
		}
	}
}
