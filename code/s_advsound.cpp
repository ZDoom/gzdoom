//**************************************************************************
//**
//** s_advsound.cpp
//**
//** [RH] Sound routines for managing SNDINFO lumps and ambient sounds.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

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

// MACROS ------------------------------------------------------------------

#define RANDOM		1
#define PERIODIC	2
#define CONTINUOUS	3
#define POSITIONAL	4
#define SURROUND	16

// TYPES -------------------------------------------------------------------

static struct AmbientSound
{
	unsigned	type;		// type of ambient sound
	int			periodmin;	// # of tics between repeats
	int			periodmax;	// max # of tics for random ambients
	float		volume;		// relative volume of sound
	float		attenuation;
	char		sound[MAX_SNDNAME+1]; // Logical name of sound to play
} Ambients[256];

enum SICommands
{
	SI_Ambient,
	SI_Map,
	SI_Registered,
	SI_ArchivePath,
	SI_Alias,
	SI_IfDoom,
	SI_IfHeretic,
	SI_IfHexen
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void S_StartNamedSound (AActor *ent, fixed_t *pt, int channel, 
						const char *name, float volume, float attenuation, BOOL looping);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static const char *SICommandStrings[] =
{
	"$ambient",
	"$map",
	"$registered",
	"$archivepath",
	"$alias",
	"$ifdoom",
	"$ifheretic",
	"$ifhexen",
	NULL
};

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int sfx_sawup, sfx_sawidl, sfx_sawful, sfx_sawhit;
extern int sfx_itemup, sfx_tink;
extern int sfx_plasma, sfx_chngun, sfx_chainguy, sfx_empty;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TArray<sfxinfo_t> S_sfx (128);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

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
		S_sfx[i].index = ~0;

	// Now set up the chains
	for (i = 0; i < size; i++)
	{
		j = MakeKey (S_sfx[i].name) % size;
		S_sfx[i].next = S_sfx[j].index;
		S_sfx[j].index = i;
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

	if (logicalname)
	{
		i = S_sfx[MakeKey (logicalname) % S_sfx.Size ()].index;

		while ((i != -1) && strnicmp (S_sfx[i].name, logicalname, MAX_SNDNAME))
			i = S_sfx[i].next;

		return i;
	}
	else
	{
		return -1;
	}
}

//==========================================================================
//
// S_FindSkinnedSound
//
// Like S_FindSound, except names beginning with '*' will be looked for
// as player/*/*
//==========================================================================

int S_FindSkinnedSound (AActor *actor, const char *name)
{
	int sfx_id;

	if (*name == '*')
	{
		// Sexed sound
		char nametemp[128];
		static const char templat[] = "player/%s/%s";
		static const char *genders[] = { "male", "female", "cyborg" };
		player_t *player;

		sfx_id = -1;
		if ( (player = actor->player) )
		{
			sprintf (nametemp, templat, skins[player->userinfo.skin].name, name + 1);
			sfx_id = S_FindSound (nametemp);
			if (sfx_id == -1)
			{
				const char *base = player->mo->BaseSoundName ();

				if (base)
				{
					sprintf (nametemp, templat, base, name + 1);
					sfx_id = S_FindSound (nametemp);
				}
				if (sfx_id == -1)
				{
					sprintf (nametemp, templat, genders[player->userinfo.gender], name + 1);
					sfx_id = S_FindSound (nametemp);
				}
			}
		}
		if (sfx_id == -1)
		{
			sprintf (nametemp, templat, "male", name + 1);
			sfx_id = S_FindSound (nametemp);
		}
	}
	else
	{
		sfx_id = S_FindSound (name);
	}
	return sfx_id;
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

	for (i = 0; i < S_sfx.Size (); i++)
	{
		if (strnicmp (S_sfx[i].name, logicalname, MAX_SNDNAME) == 0)
		{
			return i;
		}
	}
	return -1;
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

		for (i = 0; i < S_sfx.Size (); i++)
			if (S_sfx[i].lumpnum == lump)
				return i;
	}
	return -1;
}

//==========================================================================
//
// S_AddSoundLump
//
// Adds a new sound mapping to S_sfx.
//==========================================================================

int S_AddSoundLump (char *logicalname, int lump)
{
	sfxinfo_t newsfx;

	memset (&newsfx, 0, sizeof(newsfx));
	// logicalname MUST be < MAX_SNDNAME chars long
	strcpy (newsfx.name, logicalname);
	newsfx.lumpnum = lump;
	newsfx.link = -1;
	return S_sfx.Push (newsfx);
}

//==========================================================================
//
// S_AddSound
//
// If logical name is already in S_sfx, updates it to use the new sound
// lump. Otherwise, adds the new mapping with S_AddSoundLump
//==========================================================================

int S_AddSound (char *logicalname, char *lumpname)
{
	int sfxid;

	sfxid = S_FindSoundNoHash (logicalname);

	if (sfxid >= 0)
	{ // If the sound has already been defined, change the old definition
		S_sfx[sfxid].link = -1;
		S_sfx[sfxid].lumpnum = W_CheckNumForName (lumpname);
	}
	else
	{ // Otherwise, create a new definition.
		sfxid = S_AddSoundLump (logicalname, W_CheckNumForName (lumpname));
	}

	return sfxid;
}

//==========================================================================
//
// S_ParseSndInfo
//
// Parses all loaded SNDINFO lumps.
//==========================================================================

void S_ParseSndInfo ()
{
	int lastlump, lump;
	bool skipToEndIf;

	lastlump = 0;
	while ((lump = W_FindLump ("SNDINFO", &lastlump)) != -1)
	{
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
					// $ambient <num> <logical name> [point [atten]|surround] <type> [secs] <relative volume>
					struct AmbientSound *ambient, dummy;

					SC_MustGetNumber ();
					if (sc_Number < 0 || sc_Number > 255)
					{
						Printf (PRINT_HIGH, "Bad ambient index (%d)\n", sc_Number);
						ambient = &dummy;
					}
					else
					{
						ambient = Ambients + sc_Number;
					}
					memset (ambient, 0, sizeof(struct AmbientSound));

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
						attenuation = (float)atof (sc_String);
						if (attenuation > 0)
						{
							ambient->attenuation = attenuation;
							SC_MustGetString ();
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
						Printf (PRINT_HIGH, "Unknown ambient type (%s)\n", sc_String);
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
					break;

				case SI_ArchivePath:
					SC_MustGetString ();	// Unused for now
					break;

				case SI_Alias: {
					int sfxfrom, sfxto;

					SC_MustGetString ();
					sfxfrom = S_AddSound (sc_String, "__nocare");
					SC_MustGetString ();
					sfxto = S_FindSoundNoHash (sc_String);
					if (sfxto == -1)
					{
						sfxto = S_AddSoundLump (sc_String, -1);
					}
					S_sfx[sfxfrom].link = sfxto;
					}
					break;

				case SI_IfDoom:
					if (gameinfo.gametype != GAME_Doom)
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
	}
	S_HashSounds ();

	// [RH] Hack for pitch varying
	sfx_sawup = S_FindSound ("weapons/sawup");
	sfx_sawidl = S_FindSound ("weapons/sawidle");
	sfx_sawful = S_FindSound ("weapons/sawfull");
	sfx_sawhit = S_FindSound ("weapons/sawhit");
	sfx_itemup = S_FindSound ("misc/i_pkup");
	sfx_tink = S_FindSound ("misc/chat2");

	sfx_plasma = S_FindSound ("weapons/plasmaf");
	sfx_chngun = S_FindSound ("weapons/chngun");
	sfx_chainguy = S_FindSound ("chainguy/attack");
	sfx_empty = W_CheckNumForName ("dsempty");
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
		if (S_sfx[i].lumpnum != -1)
		{
			strncpy (lumpname, lumpinfo[S_sfx[i].lumpnum].name, 8);
			Printf (PRINT_HIGH, "%3d. %s (%s)\n", i+1, S_sfx[i].name, lumpname);
		}
		else
		{
			Printf (PRINT_HIGH, "%3d. %s **not present**\n", i+1, S_sfx[i].name);
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
		if (S_sfx[i].link >= 0)
		{
			Printf (PRINT_HIGH, "%s -> %s\n", S_sfx[i].name,
				S_sfx[S_sfx[i].link].name);
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
	arc << bActive << NextCheck;
}

void AAmbientSound::RunThink ()
{
	Super::RunThink ();

	if (!bActive || gametic < NextCheck)
		return;

	struct AmbientSound *ambient = &Ambients[args[0]];

	if ((ambient->type & CONTINUOUS) == CONTINUOUS)
	{
		if (S_GetSoundPlayingInfo (this, S_FindSound (ambient->sound)))
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

void AAmbientSound::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	Activate (NULL);
}

void AAmbientSound::Activate (AActor *activator)
{
	Super::Activate (activator);

	struct AmbientSound *amb = &Ambients[args[0]];

	if (!(amb->type & 3) && !amb->periodmin)
	{
		int sndnum = S_FindSound (amb->sound);
		if (sndnum == -1)
		{
			Destroy ();
			return;
		}
		sfxinfo_t *sfx = &S_sfx[sndnum];

		// Make sure the sound has been loaded so we know how long it is
		if (!sfx->data)
			I_LoadSound (sfx);
		amb->periodmin = (sfx->ms * TICRATE) / 1000;
	}

	NextCheck = gametic;
	if (amb->type & (RANDOM|PERIODIC))
		SetTicker (amb);

	bActive = true;
}

void AAmbientSound::Deactivate (AActor *activator)
{
	Super::Deactivate (activator);
	bActive = false;
}
