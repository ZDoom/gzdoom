#include <string.h>
#include <stdio.h>

#include "doomtype.h"
#include "doomstat.h"
#include "sc_man.h"
#include "m_alloc.h"
#include "m_random.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "w_wad.h"
#include "z_zone.h"
#include "i_system.h"
#include "cmdlib.h"

// MACROS ------------------------------------------------------------------

#define MAX_SEQSIZE			256
#define GetCommand(a)		((a)>>24)
#define GetData(a)			( ((a) & 0xffffff) == 0xffffff ? -1 : ((a) & 0xffffff) )
#define MakeCommand(a,b)	(((a) << 24)|((b) & 0xffffff))
#define HexenPlatSeq(a)		(a)
#define HexenDoorSeq(a)		((a) | 0x40)
#define HexenEnvSeq(a)		((a) | 0x80)
#define HexenLastSeq		(0xff)

// TYPES -------------------------------------------------------------------

typedef enum
{
	SS_STRING_PLAY,
	SS_STRING_PLAYUNTILDONE,
	SS_STRING_PLAYTIME,
	SS_STRING_PLAYREPEAT,
	SS_STRING_PLAYLOOP,
	SS_STRING_DELAY,
	SS_STRING_DELAYRAND,
	SS_STRING_VOLUME,
	SS_STRING_END,
	SS_STRING_STOPSOUND,
	SS_STRING_ATTENUATION,
	SS_STRING_DOOR,
	SS_STRING_PLATFORM,
	SS_STRING_ENVIRONMENT,
	SS_STRING_NOSTOPCUTOFF
} ssstrings_t;

typedef enum
{
	SS_CMD_NONE,
	SS_CMD_PLAY,
	SS_CMD_WAITUNTILDONE, // used by PLAYUNTILDONE
	SS_CMD_PLAYTIME,
	SS_CMD_PLAYREPEAT,
	SS_CMD_PLAYLOOP,
	SS_CMD_DELAY,
	SS_CMD_DELAYRAND,
	SS_CMD_VOLUME,
	SS_CMD_STOPSOUND,
	SS_CMD_ATTENUATION,
	SS_CMD_END
} sscmds_t;

typedef struct {
	const char		*name;
	byte			seqs[4];
} hexenseq_t;

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void VerifySeqPtr (int pos, int need);
static void AssignTranslations (int seq, seqtype_t type);
static void AssignHexenTranslations (void);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *SSStrings[] = {
	"play",
	"playuntildone",
	"playtime",
	"playrepeat",
	"playloop",
	"delay",
	"delayrand",
	"volume",
	"end",
	"stopsound",
	"attenuation",
	"door",
	"platform",
	"environment",
	"nostopcutoff",
	NULL
};
static const char *Attenuations[] = {
	"none",
	"normal",
	"idle",
	"static",
	"surround",
	NULL
};
static const hexenseq_t HexenSequences[] = {
	{ "Platform",		{ HexenPlatSeq(0), HexenPlatSeq(1), HexenPlatSeq(3), HexenLastSeq } },
	{ "PlatformMetal",	{ HexenPlatSeq(2), HexenLastSeq } },
	{ "Silence",		{ HexenPlatSeq(4), HexenDoorSeq(4), HexenLastSeq } },
	{ "Lava",			{ HexenPlatSeq(5), HexenDoorSeq(5), HexenLastSeq } },
	{ "Water",			{ HexenPlatSeq(6), HexenDoorSeq(6), HexenLastSeq } },
	{ "Ice",			{ HexenPlatSeq(7), HexenDoorSeq(7), HexenLastSeq } },
	{ "Earth",			{ HexenPlatSeq(8), HexenDoorSeq(8), HexenLastSeq } },
	{ "PlatformMetal2",	{ HexenPlatSeq(9), HexenLastSeq } },
	{ "DoorNormal",		{ HexenDoorSeq(0), HexenLastSeq } },
	{ "DoorHeavy",		{ HexenDoorSeq(1), HexenLastSeq } },
	{ "DoorMetal",		{ HexenDoorSeq(2), HexenLastSeq } },
	{ "DoorCreak",		{ HexenDoorSeq(3), HexenLastSeq } },
	{ "DoorMetal2",		{ HexenDoorSeq(9), HexenLastSeq } },
	{ "Wind",			{ HexenEnvSeq(10), HexenLastSeq } },
	{ NULL, }
};

static int SeqTrans[64*3];
static unsigned int *ScriptTemp;
static int ScriptTempSize;

sndseq_t **Sequences;
int NumSequences;
int MaxSequences;
int ActiveSequences;
seqnode_t *SequenceListHead;


// CODE --------------------------------------------------------------------

//==========================================================================
//
// VerifySequencePtr
//
//==========================================================================

static void VerifySeqPtr (int pos, int need)
{
	if (pos + need > ScriptTempSize)
	{
		ScriptTempSize *= 2;
		ScriptTemp = Realloc (ScriptTemp, ScriptTempSize * sizeof(*ScriptTemp));
	}
}

//==========================================================================
//
// AssignTranslations
//
//==========================================================================

static void AssignTranslations (int seq, seqtype_t type)
{
	sc_Crossed = false;

	while (SC_GetString () && !sc_Crossed)
	{
		if (IsNum (sc_String))
		{
			SeqTrans[(atoi(sc_String) & 63) + type * 64] = seq;
		}
	}

	SC_UnGet ();
}

//==========================================================================
//
// AssignHexenTranslations
//
//==========================================================================

static void AssignHexenTranslations (void)
{
	int i, j, seq;

	for (i = 0; HexenSequences[i].name; i++)
	{
		for (seq = 0; seq < NumSequences; seq++)
		{
			if (!stricmp (HexenSequences[i].name, Sequences[seq]->name))
				break;
		}
		if (seq == NumSequences)
			continue;

		for (j = 0; j < 4 && HexenSequences[i].seqs[j] != HexenLastSeq; j++)
		{
			int trans;

			if (HexenSequences[i].seqs[j] & 0x40)
				trans = 64;
			else if (HexenSequences[i].seqs[j] & 0x80)
				trans = 64*2;
			else
				trans = 0;

			SeqTrans[trans + (HexenSequences[i].seqs[j] & 0x3f)] = seq;
		}
	}
}

//==========================================================================
//
// S_ParseSndSeq
//
//==========================================================================

void S_ParseSndSeq (void)
{
	int lastlump, lump;
	char name[MAX_SNDNAME+1];
	int stopsound;
	int cursize;
	int curseq = -1;

	// be gone, compiler warnings
	cursize = 0;
	stopsound = -1;

	memset (SeqTrans, -1, sizeof(SeqTrans));
	lastlump = 0;
	name[MAX_SNDNAME] = 0;
	ScriptTemp = Malloc (MAX_SEQSIZE * sizeof(*ScriptTemp));
	ScriptTempSize = MAX_SEQSIZE;

	while ((lump = W_FindLump ("SNDSEQ", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, "SNDSEQ");
		while (SC_GetString ())
		{
			if (*sc_String == ':')
			{
				if (curseq != -1)
				{
					SC_ScriptError ("S_ParseSndSeq: Nested Script Error");
				}
				strncpy (name, sc_String + 1, MAX_SNDNAME);
				for (curseq = 0; curseq < NumSequences; curseq++)
				{
					if (stricmp (Sequences[curseq]->name, name) == 0)
					{
						Z_Free (Sequences[curseq]);
						Sequences[curseq] = NULL;
						break;
					}
				}
				if (curseq == NumSequences)
					NumSequences++;
				if (NumSequences > MaxSequences)
				{
					MaxSequences = MaxSequences ? MaxSequences * 2 : 64;
					Sequences = Realloc (Sequences, MaxSequences * sizeof(*Sequences));
				}
				memset (ScriptTemp, 0, sizeof(*ScriptTemp) * ScriptTempSize);
				stopsound = -1;
				cursize = 0;
				continue;
			}
			if (curseq == -1)
			{
				continue;
			}
			switch (SC_MustMatchString (SSStrings))
			{
				case SS_STRING_PLAYUNTILDONE:
					VerifySeqPtr (cursize, 2);
					SC_MustGetString ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_PLAY, S_FindSound (sc_String));
					ScriptTemp[cursize++] = SS_CMD_WAITUNTILDONE << 24;
					break;

				case SS_STRING_PLAY:
					VerifySeqPtr (cursize, 1);
					SC_MustGetString ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_PLAY, S_FindSound (sc_String));
					break;

				case SS_STRING_PLAYTIME:
					VerifySeqPtr (cursize, 2);
					SC_MustGetString ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_PLAY, S_FindSound (sc_String));
					SC_MustGetNumber ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_DELAY, sc_Number);
					break;

				case SS_STRING_PLAYREPEAT:
					VerifySeqPtr (cursize, 1);
					SC_MustGetString ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_PLAYREPEAT, S_FindSound (sc_String));
					break;

				case SS_STRING_PLAYLOOP:
					VerifySeqPtr (cursize, 2);
					SC_MustGetString ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_PLAYLOOP, S_FindSound (sc_String));
					SC_MustGetNumber ();
					ScriptTemp[cursize++] = sc_Number;
					break;

				case SS_STRING_DELAY:
					VerifySeqPtr (cursize, 1);
					SC_MustGetNumber ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_DELAY, sc_Number);
					break;

				case SS_STRING_DELAYRAND:
					VerifySeqPtr (cursize, 2);
					SC_MustGetNumber ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_DELAYRAND, sc_Number);
					SC_MustGetNumber ();
					ScriptTemp[cursize++] = sc_Number;
					break;

				case SS_STRING_VOLUME:
					VerifySeqPtr (cursize, 1);
					SC_MustGetNumber ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_VOLUME, sc_Number);
					break;

				case SS_STRING_STOPSOUND:
					VerifySeqPtr (cursize, 1);
					SC_MustGetString ();
					stopsound = S_FindSound (sc_String);
					ScriptTemp[cursize++] = SS_CMD_STOPSOUND << 24;
					break;

				case SS_STRING_NOSTOPCUTOFF:
					VerifySeqPtr (cursize, 1);
					stopsound = -2;
					ScriptTemp[cursize++] = SS_CMD_STOPSOUND << 24;
					break;

				case SS_STRING_ATTENUATION:
					VerifySeqPtr (cursize, 1);
					SC_MustGetString ();
					ScriptTemp[cursize++] = MakeCommand (SS_CMD_ATTENUATION,
											SC_MustMatchString (Attenuations));
					break;

				case SS_STRING_END:
					Sequences[curseq] = Z_Malloc (sizeof(sndseq_t) + sizeof(int)*cursize, PU_STATIC, 0);
					strcpy (Sequences[curseq]->name, name);
					memcpy (Sequences[curseq]->script, ScriptTemp, sizeof(int)*cursize);
					Sequences[curseq]->script[cursize] = SS_CMD_END;
					Sequences[curseq]->stopsound = stopsound;
					curseq = -1;
					break;

				case SS_STRING_PLATFORM:
					AssignTranslations (curseq, SEQ_PLATFORM);
					break;

				case SS_STRING_DOOR:
					AssignTranslations (curseq, SEQ_DOOR);
					break;

				case SS_STRING_ENVIRONMENT:
					AssignTranslations (curseq, SEQ_ENVIRONMENT);
					break;
			}
		}
		SC_Close ();
	}

	free (ScriptTemp);

	if (HexenHack)
		AssignHexenTranslations ();
}

//==========================================================================
//
//  SN_StartSequence
//
//==========================================================================

void SN_StartSequence (mobj_t *mobj, int sequence, seqtype_t type)
{
	seqnode_t *node;
	
	SN_StopSequence (mobj); // Stop any previous sequence

	if (type < SEQ_NUMSEQTYPES)
		sequence = SeqTrans[sequence + type * 64];

	if (sequence == -1 || Sequences[sequence] == NULL)
		return;

	node = (seqnode_t *)Z_Malloc (sizeof(seqnode_t), PU_STATIC, NULL);
	node->sequencePtr = Sequences[sequence]->script;
	node->sequence = sequence;
	node->mobj = mobj;
	node->delayTics = 0;
	node->stopSound = Sequences[sequence]->stopsound;
	node->currentSoundID = -1;
	node->volume = 1; // Start at max volume...
	node->atten = ATTN_IDLE; // ...and idle attenuation

	if (!SequenceListHead)
	{
		SequenceListHead = node;
		node->next = node->prev = NULL;
	}
	else
	{
		SequenceListHead->prev = node;
		node->next = SequenceListHead;
		node->prev = NULL;
		SequenceListHead = node;
	}
	ActiveSequences++;
	return;
}

//==========================================================================
//
//  SN_StartSequenceName
//
//==========================================================================

void SN_StartSequenceName (mobj_t *mobj, const char *name)
{
	int i;

	for (i = 0; i < NumSequences; i++)
	{
		if (!stricmp (name, Sequences[i]->name))
		{
			SN_StartSequence (mobj, i, SEQ_NOTRANS);
			return;
		}
	}
}
//==========================================================================
//
//  SN_StopSequence
//
//==========================================================================

void SN_StopSequence (mobj_t *mobj)
{
	seqnode_t *node;

	for (node = SequenceListHead; node; node = node->next)
	{
		if (node->mobj == mobj)
		{
			if (node->stopSound >= 0)
			{
				S_SoundID (mobj, CHAN_BODY, node->stopSound, node->volume, node->atten);
			}
			else if (node->stopSound == -1)
			{
				S_StopSound (mobj, CHAN_BODY);
			}
			if (SequenceListHead == node)
			{
				SequenceListHead = node->next;
			}
			if (node->prev)
			{
				node->prev->next = node->next;
			}
			if (node->next)
			{
				node->next->prev = node->prev;
			}
			Z_Free (node);
			ActiveSequences--;
		}
	}
}

//==========================================================================
//
//  SN_UpdateActiveSequences
//
//==========================================================================

void SN_UpdateActiveSequences (void)
{
	seqnode_t *node;
	BOOL sndPlaying;

	if (!ActiveSequences || paused)
	{ // No sequences currently playing/game is paused
		return;
	}
	for (node = SequenceListHead; node; node = node->next)
	{
		if (node->delayTics > 0)
		{
			node->delayTics--;
			continue;
		}
		sndPlaying = S_GetSoundPlayingInfo (node->mobj, node->currentSoundID);
		if (node->delayTics < 0 && sndPlaying)
		{
			node->delayTics++;
			continue;
		}
		switch (GetCommand(*node->sequencePtr))
		{
			case SS_CMD_PLAY:
				if (!sndPlaying)
				{
					node->currentSoundID = GetData(*node->sequencePtr);
					S_SoundID (node->mobj, CHAN_BODY, node->currentSoundID,
						node->volume, node->atten);
				}
				node->sequencePtr++;
				break;

			case SS_CMD_WAITUNTILDONE:
				if (!sndPlaying)
				{
					node->sequencePtr++;
					node->currentSoundID = -1;
				}
				break;

			case SS_CMD_PLAYREPEAT:
				if (!sndPlaying)
				{
					// Does not advance sequencePtr, so it will repeat
					// as necessary
					node->currentSoundID = GetData(*node->sequencePtr);
					S_LoopedSoundID (node->mobj, CHAN_BODY, node->currentSoundID,
						node->volume, node->atten);
				}
				break;

			case SS_CMD_PLAYLOOP:
				node->currentSoundID = GetData(*node->sequencePtr);
				S_LoopedSoundID (node->mobj, CHAN_BODY, node->currentSoundID,
					node->volume, node->atten);
				node->delayTics = -(signed)GetData(*(node->sequencePtr+1));
				break;

			case SS_CMD_DELAY:
				node->delayTics = GetData(*node->sequencePtr);
				node->sequencePtr++;
				node->currentSoundID = -1;
				break;

			case SS_CMD_DELAYRAND:
				node->delayTics = GetData(*node->sequencePtr)+
					M_Random()%(*(node->sequencePtr+1)-GetData(*node->sequencePtr));
				node->sequencePtr += 2;
				node->currentSoundID = -1;
				break;

			case SS_CMD_VOLUME:
				// volume is in range 0..100
				node->volume = GetData(*node->sequencePtr)/100;
				node->sequencePtr++;
				break;

			case SS_CMD_STOPSOUND:
				// Wait until something else stops the sequence
				break;

			case SS_CMD_ATTENUATION:
				node->atten = GetData(*node->sequencePtr);
				node->sequencePtr++;
				break;

			case SS_CMD_END:
				SN_StopSequence (node->mobj);
				break;

			default:	
				break;
		}
	}
}

//==========================================================================
//
//  SN_StopAllSequences
//
//==========================================================================

void SN_StopAllSequences (void)
{
	seqnode_t *node;

	for (node = SequenceListHead; node; node = node->next)
	{
		node->stopSound = -1; // don't play any stop sounds
		SN_StopSequence (node->mobj);
	}
}
	
//==========================================================================
//
//  SN_GetSequenceOffset
//
//==========================================================================

ptrdiff_t SN_GetSequenceOffset (int sequence, unsigned int *sequencePtr)
{
	return sequencePtr - Sequences[sequence]->script;
}

//==========================================================================
//
//  SN_ChangeNodeData
//
// 	nodeNum zero is the first node
//==========================================================================

void SN_ChangeNodeData (int nodeNum, int seqOffset, int delayTics, float volume,
	int currentSoundID)
{
	int i;
	seqnode_t *node;

	i = 0;
	node = SequenceListHead;
	while (node && i < nodeNum)
	{
		node = node->next;
		i++;
	}
	if (!node)
	{ // reach the end of the list before finding the nodeNum-th node
		return;
	}
	node->delayTics = delayTics;
	node->volume = volume;
	node->sequencePtr += seqOffset;
	node->currentSoundID = currentSoundID;
}
