/*
** s_advsound.cpp
** Routines for managing SNDINFO lumps and ambient sounds
**
**---------------------------------------------------------------------------
** Copyright 1998-2005 Randy Heit
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

// MACROS ------------------------------------------------------------------

#define RANDOM		1
#define PERIODIC	2
#define CONTINUOUS	3
#define POSITIONAL	4
#define SURROUND	16

// TYPES -------------------------------------------------------------------

struct FRandomSoundList
{
	WORD		*Sounds;	// A list of sounds that can result for the following id
	WORD		SfxHead;	// The sound id used to reference this list
	WORD		NumSounds;
};

struct FPlayerClassLookup
{
	char		Name[MAX_SNDNAME+1];
	WORD		ListIndex[3];	// indices into PlayerSounds (0xffff means empty)
};

static struct AmbientSound
{
	unsigned	type;		// type of ambient sound
	int			periodmin;	// # of tics between repeats
	int			periodmax;	// max # of tics for random ambients
	float		volume;		// relative volume of sound
	float		attenuation;
	char		sound[MAX_SNDNAME+1]; // Logical name of sound to play
} *Ambients[256];

enum SICommands
{
	SI_Ambient,
	SI_Random,
	SI_PlayerReserve,
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
	SI_IfDoom,
	SI_IfHeretic,
	SI_IfHexen,
	SI_IfStrife
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

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void S_StartNamedSound (AActor *ent, fixed_t *pt, int channel, 
	const char *name, float volume, float attenuation, bool looping);
extern bool IsFloat (const char *str);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int STACK_ARGS SortPlayerClasses (const void *a, const void *b);
static int S_DupPlayerSound (const char *pclass, int gender, int refid, int aliasref);
static int S_AddPlayerClass (const char *name);
static int S_AddPlayerGender (int classnum, int gender);
static int S_FindPlayerClass (const char *name);
static int S_LookupPlayerSound (int classidx, int gender, int refid);
static void S_ParsePlayerSoundCommon (char pclass[MAX_SNDNAME+1], int &gender, int &refid);
static void S_AddSNDINFO (int lumpnum);
static void S_AddBloodSFX (int lumpnum);
static void S_AddStrifeVoice (int lumpnum);
static int S_AddSound (const char *logicalname, int lumpnum);

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
	"$playerreserve",
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
	"$ifdoom",
	"$ifheretic",
	"$ifhexen",
	"$ifstrife",
	NULL
};

static TArray<FRandomSoundList> S_rnd;
static FMusicVolume *MusicVolumes;

static int NumPlayerReserves;
static bool DoneReserving;
static bool PlayerClassesIsSorted;

static TArray<FPlayerClassLookup> PlayerClasses;
static TArray<WORD> PlayerSounds;

static char DefPlayerClassName[MAX_SNDNAME+1];
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
	size_t size;

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

		while ((i != 0) && strnicmp (S_sfx[i].name, logicalname, MAX_SNDNAME))
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
		if (strnicmp (S_sfx[i].name, logicalname, MAX_SNDNAME) == 0)
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

	memset (&newsfx.lumpnum, 0, sizeof(newsfx)-sizeof(newsfx.name));
	// logicalname MUST be < MAX_SNDNAME chars long
	strcpy (newsfx.name, logicalname);
	newsfx.lumpnum = lump;
	newsfx.link = sfxinfo_t::NO_LINK;
	newsfx.PitchMask = CurrentPitchMask;
	newsfx.MaxChannels = 2;
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
	return id != 0 ? id : S_AddSoundLump (name, -1);
}

//==========================================================================
//
// S_AddSound
//
// If logical name is already in S_sfx, updates it to use the new sound
// lump. Otherwise, adds the new mapping by using S_AddSoundLump().
//==========================================================================

int S_AddSound (const char *logicalname, const char *lumpname)
{
	return S_AddSound (logicalname,
		lumpname ? Wads.CheckNumForName (lumpname) : -1);
}

static int S_AddSound (const char *logicalname, int lumpnum)
{
	int sfxid;

	sfxid = S_FindSoundNoHash (logicalname);

	if (sfxid > 0)
	{ // If the sound has already been defined, change the old definition
		sfxinfo_t *sfx = &S_sfx[sfxid];

		if (sfx->bPlayerReserve)
		{
			SC_ScriptError ("Sounds that are reserved for players cannot be reassigned");
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
	return S_AddPlayerSound (pclass, gender, refid,
		lumpname ? Wads.CheckNumForName (lumpname) : -1);
}

int S_AddPlayerSound (const char *pclass, int gender, int refid, int lumpnum)
{
	char fakename[MAX_SNDNAME+1];
	size_t len;
	int id;

	len = strlen (pclass);
	memcpy (fakename, pclass, len);
	fakename[len] = '|';
	fakename[len+1] = gender + '0';
	strcpy (&fakename[len+2], S_sfx[refid].name);

	id = S_AddSoundLump (fakename, lumpnum);
	int classnum = S_AddPlayerClass (pclass);
	int soundlist = S_AddPlayerGender (classnum, gender);

	PlayerSounds[soundlist + S_sfx[refid].link] = id;
	return id;
}

//==========================================================================
//
// S_AddPlayerSoundExisting
//
// Adds the player sound as an alias to an existing sound.
//==========================================================================

int S_AddPlayerSoundExisting (const char *pclass, int gender, int refid,
	int aliasto)
{
	int classnum = S_AddPlayerClass (pclass);
	int soundlist = S_AddPlayerGender (classnum, gender);

	PlayerSounds[soundlist + S_sfx[refid].link] = aliasto;
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
// S_ClearSoundData
//
// clears all sound tables
// When we want to allow level specific SNDINFO lumps this has to
// be cleared for each level
//==========================================================================

static void S_ClearSoundData()
{
	int i;

	S_sfx.Clear();

	for(i=0;i<256;i++)
	{
		if (Ambients[i]) delete Ambients[i];
		Ambients[i]=NULL;
	}
	for(i=0;i<S_rnd.Size();i++)
	{
		delete[] S_rnd[i].Sounds;
	}
	while (MusicVolumes != NULL)
	{
		FMusicVolume * me = MusicVolumes;
		MusicVolumes = me->Next;
		delete me;
	}
	S_rnd.Clear();

	DoneReserving=false;
	NumPlayerReserves=0;
	PlayerClassesIsSorted=false;
	PlayerClasses.Clear();
	PlayerSounds.Clear();
	DefPlayerClass=0;
	*DefPlayerClassName=0;
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

	S_HashSounds ();
	S_sfx.ShrinkToFit ();

	if (S_rnd.Size() > 0)
	{
		S_rnd.ShrinkToFit ();
	}

	S_ShrinkPlayerSoundLists ();

	// [RH] Hack for pitch varying
	sfx_sawup = S_FindSound ("weapons/sawup");
	sfx_sawidl = S_FindSound ("weapons/sawidle");
	sfx_sawful = S_FindSound ("weapons/sawfull");
	sfx_sawhit = S_FindSound ("weapons/sawhit");
	sfx_itemup = S_FindSound ("misc/i_pkup");
	sfx_tink = S_FindSound ("misc/chat2");

	sfx_empty = Wads.CheckNumForName ("dsempty");
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

	SC_OpenLumpNum (lump, "SNDINFO");
	skipToEndIf = false;

	while (SC_GetString ())
	{
		if (skipToEndIf)
		{
			if (SC_Compare ("$endif"))
			{
				skipToEndIf = false;
			}
			continue;
		}

		if (sc_String[0] == '$')
		{ // Got a command
			switch (SC_MatchString (SICommandStrings))
			{
			case SI_Ambient: {
				// $ambient <num> <logical name> [point [atten] | surround | [world]]
				//			<continuous | random <minsecs> <maxsecs> | periodic <secs>>
				//			<volume>
				AmbientSound *ambient, dummy;

				SC_MustGetNumber ();
				if (sc_Number < 0 || sc_Number > 255)
				{
					Printf ("Bad ambient index (%d)\n", sc_Number);
					ambient = &dummy;
				}
				else if (Ambients[sc_Number] == NULL)
				{
					ambient = new AmbientSound;
					Ambients[sc_Number] = ambient;
				}
				else
				{
					ambient = Ambients[sc_Number];
				}
				memset (ambient, 0, sizeof(AmbientSound));

				SC_MustGetString ();
				strncpy (ambient->sound, sc_String, MAX_SNDNAME);
				ambient->sound[MAX_SNDNAME] = 0;
				ambient->attenuation = 0;

				SC_MustGetString ();
				if (SC_Compare ("point"))
				{
					float attenuation;

					ambient->type = POSITIONAL;
					SC_MustGetString ();

					if (IsFloat (sc_String))
					{
						attenuation = atof (sc_String);
						SC_MustGetString ();
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
				else if (SC_Compare ("surround"))
				{
					ambient->type = SURROUND;
					SC_MustGetString ();
					ambient->attenuation = -1;
				}
				else
				{ // World is an optional keyword
					if (SC_Compare ("world"))
					{
						SC_MustGetString ();
					}
				}

				if (SC_Compare ("continuous"))
				{
					ambient->type |= CONTINUOUS;
				}
				else if (SC_Compare ("random"))
				{
					ambient->type |= RANDOM;
					SC_MustGetFloat ();
					ambient->periodmin = (int)(sc_Float * TICRATE);
					SC_MustGetFloat ();
					ambient->periodmax = (int)(sc_Float * TICRATE);
				}
				else if (SC_Compare ("periodic"))
				{
					ambient->type |= PERIODIC;
					SC_MustGetFloat ();
					ambient->periodmin = (int)(sc_Float * TICRATE);
				}
				else
				{
					Printf ("Unknown ambient type (%s)\n", sc_String);
				}

				SC_MustGetFloat ();
				ambient->volume = sc_Float;
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

				SC_MustGetNumber ();
				sprintf (temp, "MAP%02d", sc_Number);
				info = FindLevelInfo (temp);
				SC_MustGetString ();
				if (info->mapname[0])
				{
					ReplaceString (&info->music, sc_String);
				}
				}
				break;

			case SI_Registered:
				// I don't think Hexen even pays attention to the $registered command.
				break;

			case SI_ArchivePath:
				SC_MustGetString ();	// Unused for now
				break;

			case SI_PlayerReserve:
				// $playerreserve <logical name>
				if (DoneReserving)
				{
					SC_ScriptError ("All $playerreserves must come before any $playersounds or $playeraliases");
				}
				else
				{
					SC_MustGetString ();
					int id = S_AddSound (sc_String, -1);
					S_sfx[id].link = NumPlayerReserves++;
					S_sfx[id].bPlayerReserve = true;
				}
				break;

			case SI_PlayerSound: {
				// $playersound <player class> <gender> <logical name> <lump name>
				char pclass[MAX_SNDNAME+1];
				int gender, refid;

				S_ParsePlayerSoundCommon (pclass, gender, refid);
				S_AddPlayerSound (pclass, gender, refid, sc_String);
				}
				break;

			case SI_PlayerSoundDup: {
				// $playersounddup <player class> <gender> <logical name> <target sound name>
				char pclass[MAX_SNDNAME+1];
				int gender, refid, targid;

				S_ParsePlayerSoundCommon (pclass, gender, refid);
				targid = S_FindSoundNoHash (sc_String);
				if (!S_sfx[targid].bPlayerReserve)
				{
					SC_ScriptError ("%s is not a player sound", sc_String);
				}
				S_DupPlayerSound (pclass, gender, refid, targid);
				}
				break;

			case SI_PlayerCompat: {
				// $playercompat <player class> <gender> <logical name> <compat sound name>
				char pclass[MAX_SNDNAME+1];
				int gender, refid;
				int sfxfrom, aliasto;

				S_ParsePlayerSoundCommon (pclass, gender, refid);
				sfxfrom = S_AddSound (sc_String, -1);
				aliasto = S_LookupPlayerSound (pclass, gender, refid);
				S_sfx[sfxfrom].link = aliasto;
				S_sfx[sfxfrom].bPlayerCompat = true;
				}
				break;

			case SI_PlayerAlias: {
				// $playeralias <player class> <gender> <logical name> <logical name of existing sound>
				char pclass[MAX_SNDNAME+1];
				int gender, refid;

				S_ParsePlayerSoundCommon (pclass, gender, refid);
				S_AddPlayerSoundExisting (pclass, gender, refid,
					S_FindSoundTentative (sc_String));
				}
				break;

			case SI_Alias: {
				// $alias <name of alias> <name of real sound>
				int sfxfrom;

				SC_MustGetString ();
				sfxfrom = S_AddSound (sc_String, -1);
				SC_MustGetString ();
				if (S_sfx[sfxfrom].bPlayerCompat)
				{
					sfxfrom = S_sfx[sfxfrom].link;
				}
				S_sfx[sfxfrom].link = S_FindSoundTentative (sc_String);
				}
				break;

			case SI_Limit: {
				// $limit <logical name> <max channels>
				int sfx;

				SC_MustGetString ();
				sfx = S_FindSoundTentative (sc_String);
				SC_MustGetNumber ();
				//S_sfx[sfx].MaxChannels = clamp<BYTE> (sc_Number, 0, 255);
				//Can't use clamp because of GCC bugs
				S_sfx[sfx].MaxChannels = MIN (MAX (sc_Number, 0), 255);
				}
				break;

			case SI_Singular: {
				// $singular <logical name>
				int sfx;

				SC_MustGetString ();
				sfx = S_FindSoundTentative (sc_String);
				S_sfx[sfx].bSingular = true;
				}
				break;

			case SI_PitchShift: {
				// $pitchshift <logical name> <pitch shift amount>
				int sfx;

				SC_MustGetString ();
				sfx = S_FindSoundTentative (sc_String);
				SC_MustGetNumber ();
				S_sfx[sfx].PitchMask = (1 << clamp (sc_Number, 0, 7)) - 1;
				}
				break;

			case SI_PitchShiftRange:
				// $pitchshiftrange <pitch shift amount>
				SC_MustGetNumber ();
				CurrentPitchMask = (1 << clamp (sc_Number, 0, 7)) - 1;
				break;

			case SI_Random: {
				// $random <logical name> { <logical name> ... }
				FRandomSoundList random;

				list.Clear ();
				SC_MustGetString ();
				random.SfxHead = S_AddSound (sc_String, -1);
				SC_MustGetStringName ("{");
				while (SC_GetString () && !SC_Compare ("}"))
				{
					WORD sfxto = S_FindSoundTentative (sc_String);
					list.Push (sfxto);
				}
				if (list.Size() == 1)
				{ // Only one sound: treat as $alias
					S_sfx[random.SfxHead].link = list[0];
				}
				else if (list.Size() > 1)
				{ // Only add non-empty random lists
					random.NumSounds = (WORD)list.Size();
					random.Sounds = new WORD[random.NumSounds];
					memcpy (random.Sounds, &list[0], sizeof(WORD)*random.NumSounds);
					S_sfx[random.SfxHead].link = (WORD)S_rnd.Push (random);
					S_sfx[random.SfxHead].bRandomHeader = true;
				}
				}
				break;

			case SI_MusicVolume: {
				SC_MustGetString();
				string musname (sc_String);
				SC_MustGetFloat();
				FMusicVolume *mv = (FMusicVolume *)Malloc (sizeof(*mv) + musname.Len());
				mv->Volume = sc_Float;
				strcpy (mv->MusicName, musname.GetChars());
				mv->Next = MusicVolumes;
				MusicVolumes = mv;
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
			char name[MAX_SNDNAME+1];

			strncpy (name, sc_String, MAX_SNDNAME);
			name[MAX_SNDNAME] = 0;
			SC_MustGetString ();
			S_AddSound (name, sc_String);
		}
	}
	SC_Close ();
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

static void S_ParsePlayerSoundCommon (char pclass[MAX_SNDNAME+1], int &gender, int &refid)
{
	DoneReserving = true;
	SC_MustGetString ();
	strcpy (pclass, sc_String);
	SC_MustGetString ();
	gender = D_GenderToInt (sc_String);
	SC_MustGetString ();
	refid = S_FindSoundNoHash (sc_String);
	if (!S_sfx[refid].bPlayerReserve)
	{
		SC_ScriptError ("%s has not been reserved for a player sound", sc_String);
	}
	SC_MustGetString ();
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
		size_t namelen = strlen (name) + 1;

		memcpy (lookup.Name, name, namelen);
		lookup.ListIndex[2] = lookup.ListIndex[1] = lookup.ListIndex[0] = 0xffff;
		cnum = (int)PlayerClasses.Push (lookup);
		PlayerClassesIsSorted = false;

		// The default player class is the first one added
		if (DefPlayerClassName[0] == 0)
		{
			memcpy (DefPlayerClassName, name, namelen);
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

		for (i = 0; i < PlayerClasses.Size(); ++i)
		{
			if (stricmp (name, PlayerClasses[i].Name) == 0)
			{
				return (int)i;
			}
		}
	}
	else
	{
		int min = 0;
		int max = (int)(PlayerClasses.Size() - 1);

		while (min <= max)
		{
			int mid = (min + max) / 2;
			int lexx = stricmp (PlayerClasses[mid].Name, name);
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
	int index;

	index = PlayerClasses[classnum].ListIndex[gender];
	if (index == 0xffff)
	{
		WORD pushee = 0;

		index = (int)PlayerSounds.Size ();
		PlayerClasses[classnum].ListIndex[gender] = (WORD)index;
		for (int i = NumPlayerReserves; i != 0; --i)
		{
			PlayerSounds.Push (pushee);
		}
	}
	return index;
}

//==========================================================================
//
// S_ShrinkPlayerSoundLists
//
// Shrinks the arrays used by the player sounds to be just large enough
// and also sorts the PlayerClasses array.
//==========================================================================

void S_ShrinkPlayerSoundLists ()
{
	PlayerSounds.ShrinkToFit ();
	PlayerClasses.ShrinkToFit ();

	qsort (&PlayerClasses[0], PlayerClasses.Size(),
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

int S_LookupPlayerSound (const char *pclass, int gender, int refid)
{
	if (!S_sfx[refid].bPlayerReserve)
	{ // Not a player sound, so just return this sound
		return refid;
	}

	return S_LookupPlayerSound (S_FindPlayerClass (pclass), gender, refid);
}

static int S_LookupPlayerSound (int classidx, int gender, int refid)
{
	int ingender = gender;

	if (classidx == -1)
	{
		classidx = DefPlayerClass;
	}

	int listidx = PlayerClasses[classidx].ListIndex[gender];
	if (listidx == 0xffff)
	{
		int g;

		for (g = 0; g < 3 && listidx == 0xffff; ++g)
		{
			listidx = PlayerClasses[classidx].ListIndex[g];
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

	int sndnum = PlayerSounds[listidx + S_sfx[refid].link];

	// If we're not done parsing SNDINFO yet, assume that the target sound is valid
	if (PlayerClassesIsSorted &&
		(sndnum == 0 ||
		((S_sfx[sndnum].lumpnum == -1 || S_sfx[sndnum].lumpnum == sfx_empty) && S_sfx[sndnum].link == 0xffff)))
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

int S_FindSkinnedSound (AActor *actor, const char *name)
{
	return S_FindSkinnedSound (actor, S_FindSound (name));
}

int S_FindSkinnedSound (AActor *actor, int refid)
{
	const char *pclass;
	int gender;

	if (actor != NULL && actor->player != NULL)
	{
		pclass = actor->player->mo->GetSoundClass ();
		gender = actor->player->userinfo.gender;
	}
	else
	{
		pclass = gameinfo.gametype != GAME_Hexen
			? "player" : "fighter";
		gender = GENDER_MALE;
	}

	return S_LookupPlayerSound (pclass, gender, refid);
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
			Printf ("%3d. %s -> #%d {", i, sfx->name, sfx->link);
			const FRandomSoundList *list = &S_rnd[sfx->link];
			for (size_t j = 0; j < list->NumSounds; ++j)
			{
				Printf (" %s ", S_sfx[list->Sounds[j]].name);
			}
			Printf ("}\n");
		}
		else if (sfx->bPlayerReserve)
		{
			Printf ("%3d. %s <<player sound %d>>\n", i, sfx->name, sfx->link);
		}
		else if (S_sfx[i].lumpnum != -1)
		{
			Wads.GetLumpName (lumpname, sfx->lumpnum);
			Printf ("%3d. %s (%s)\n", i, sfx->name, lumpname);
		}
		else if (S_sfx[i].link != sfxinfo_t::NO_LINK)
		{
			Printf ("%3d. %s -> %s\n", i, sfx->name, S_sfx[sfx->link].name);
		}
		else
		{
			Printf ("%3d. %s **not present**\n", i, sfx->name);
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
			Printf ("%s -> %s\n", sfx->name, S_sfx[sfx->link].name);
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

	for (i = 0; i < PlayerClasses.Size(); ++i)
	{
		for (j = 0; j < 3; ++j)
		{
			if ((l = PlayerClasses[i].ListIndex[j]) != 0xffff)
			{
				Printf ("\n%s, %s:\n", PlayerClasses[i].Name, GenderNames[j]);
				for (k = 0; k < NumPlayerReserves; ++l, ++k)
				{
					Printf (" %-16s%s\n", reserveNames[k], S_sfx[PlayerSounds[l]].name);
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

	if ((ambient->type & CONTINUOUS) == CONTINUOUS)
	{
		if (S_IsActorPlayingSomething (this, CHAN_BODY))
			return;

		if (ambient->sound[0])
		{
			S_StartNamedSound (this, NULL, CHAN_BODY, ambient->sound,
				ambient->volume, ambient->attenuation, true);
			SetTicker (ambient);
		}
		else
		{
			Destroy ();
		}
	}
	else
	{
		if (ambient->sound[0])
		{
			S_StartNamedSound (this, NULL, CHAN_BODY, ambient->sound,
				ambient->volume, ambient->attenuation, false);
			SetTicker (ambient);
		}
		else
		{
			Destroy ();
		}
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
		if (!(amb->type & 3) && !amb->periodmin)
		{
			int sndnum = S_FindSound (amb->sound);
			if (sndnum == 0)
			{
				Destroy ();
				return;
			}
			sfxinfo_t *sfx = &S_sfx[sndnum];

			// Make sure the sound has been loaded so we know how long it is
			if (!sfx->data && GSnd != NULL)
				GSnd->LoadSound (sfx);
			amb->periodmin = (sfx->ms * TICRATE) / 1000;
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
