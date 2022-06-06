//-----------------------------------------------------------------------------
//
// Copyright 1994-1996 Raven Software
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

#include <string.h>

#include "doomtype.h"
#include "doomstat.h"
#include "sc_man.h"
#include "m_random.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "filesystem.h"
#include "cmdlib.h"
#include "p_local.h"
#include "po_man.h"
#include "gi.h"
#include "c_dispatch.h"

#include "g_level.h"
#include "serializer_doom.h"
#include "serialize_obj.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "vm.h"

// MACROS ------------------------------------------------------------------

#define GetCommand(a)		((a) & 255)
#define GetData(a)			(int32_t(a) >> 8 )
#define GetFloatData(a)		float((int32_t(a) >> 8 )/65536.f)
#define MakeCommand(a,b)	((a) | ((b) << 8))
#define HexenPlatSeqStart	(0)
#define HexenDoorSeqStart	(MAX_SNDSEQS)
#define HexenEnvSeqStart	(MAX_SNDSEQS << 1)
#define HexenPlatSeq(a)		((a) | (HexenPlatSeqStart))
#define HexenDoorSeq(a)		((a) | (HexenDoorSeqStart))
#define HexenEnvSeq(a)		((a) | (HexenEnvSeqStart))
#define HexenLastSeq		((MAX_SNDSEQS << 2) - 1)

// TYPES -------------------------------------------------------------------

typedef enum
{
	SS_STRING_PLAY,
	SS_STRING_PLAYUNTILDONE,
	SS_STRING_PLAYTIME,
	SS_STRING_PLAYREPEAT,
	SS_STRING_PLAYLOOP,
	SS_STRING_DELAY,
	SS_STRING_DELAYONCE,
	SS_STRING_DELAYRAND,
	SS_STRING_VOLUME,
	SS_STRING_VOLUMEREL,
	SS_STRING_VOLUMERAND,
	SS_STRING_END,
	SS_STRING_STOPSOUND,
	SS_STRING_ATTENUATION,
	SS_STRING_NOSTOPCUTOFF,
	SS_STRING_SLOT,
	SS_STRING_RANDOMSEQUENCE,
	SS_STRING_RESTART,
	// These must be last and in the same order as they appear in seqtype_t
	SS_STRING_PLATFORM,
	SS_STRING_DOOR,
	SS_STRING_ENVIRONMENT
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
	SS_CMD_VOLUMEREL,
	SS_CMD_VOLUMERAND,
	SS_CMD_STOPSOUND,
	SS_CMD_ATTENUATION,
	SS_CMD_RANDOMSEQUENCE,
	SS_CMD_BRANCH,
	SS_CMD_LAST2NOP,
	SS_CMD_SELECT,
	SS_CMD_END
} sscmds_t;

struct hexenseq_t
{
	ENamedName	Name;
	uint16_t	Seqs[4];
};

class DSeqActorNode : public DSeqNode
{
	DECLARE_CLASS(DSeqActorNode, DSeqNode)
	HAS_OBJECT_POINTERS
public:
	DSeqActorNode(AActor *actor, int sequence, int modenum);
	void OnDestroy() override;
	void Serialize(FSerializer &arc);
	void MakeSound(int loop, FSoundID id)
	{
		S_Sound(m_Actor, CHAN_BODY, EChanFlags::FromInt(loop), id, clamp(m_Volume, 0.f, 1.f), m_Atten);
	}
	bool IsPlaying()
	{
		return S_IsActorPlayingSomething (m_Actor, CHAN_BODY, m_CurrentSoundID);
	}
	void *Source()
	{
		return m_Actor;
	}
	DSeqNode *SpawnChild(int seqnum)
	{
		return SN_StartSequence (m_Actor, seqnum, SEQ_NOTRANS, m_ModeNum, true);
	}
private:
	DSeqActorNode() {}
	TObjPtr<AActor*> m_Actor;
};

class DSeqPolyNode : public DSeqNode
{
	DECLARE_CLASS(DSeqPolyNode, DSeqNode)
public:
	DSeqPolyNode(FPolyObj *poly, int sequence, int modenum);
	void OnDestroy() override;
	void Serialize(FSerializer &arc);
	void MakeSound(int loop, FSoundID id)
	{
		S_Sound (m_Poly, CHAN_BODY, EChanFlags::FromInt(loop), id, clamp(m_Volume, 0.f, 1.f), m_Atten);
	}
	bool IsPlaying()
	{
		return m_CurrentSoundID != 0 && S_GetSoundPlayingInfo (m_Poly, m_CurrentSoundID);
	}
	void *Source()
	{
		return m_Poly;
	}
	DSeqNode *SpawnChild (int seqnum)
	{
		return SN_StartSequence (m_Poly, seqnum, SEQ_NOTRANS, m_ModeNum, true);
	}
private:
	DSeqPolyNode () {}
	FPolyObj *m_Poly;
};

class DSeqSectorNode : public DSeqNode
{
	DECLARE_CLASS(DSeqSectorNode, DSeqNode)
public:
	DSeqSectorNode(sector_t *sec, int chan, int sequence, int modenum);
	void OnDestroy() override;
	void Serialize(FSerializer &arc);
	void MakeSound(int loop, FSoundID id)
	{
		// The Channel here may have CHANF_LOOP encoded into it.
		S_Sound(m_Sector, Channel & 7, CHANF_AREA | EChanFlags::FromInt(loop| (Channel & ~7)), id, clamp(m_Volume, 0.f, 1.f), m_Atten);
	}
	bool IsPlaying()
	{
		return m_CurrentSoundID != 0 && S_GetSoundPlayingInfo (m_Sector, m_CurrentSoundID);
	}
	void *Source()
	{
		return m_Sector;
	}
	DSeqNode *SpawnChild(int seqnum)
	{
		return SN_StartSequence (m_Sector, Channel, seqnum, SEQ_NOTRANS, m_ModeNum, true);
	}
	int Channel;
private:
	DSeqSectorNode() {}
	sector_t *m_Sector;
};

// When destroyed, destroy the sound sequences too.
struct FSoundSequencePtrArray : public TArray<FSoundSequence *>
{
	~FSoundSequencePtrArray()
	{
		for (unsigned int i = 0; i < Size(); ++i)
		{
			M_Free((*this)[i]);
		}
	}
};

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void AssignTranslations (FScanner &sc, int seq, seqtype_t type);
static void AssignHexenTranslations (void);
static void AddSequence (int curseq, FName seqname, FName slot, int stopsound, const TArray<uint32_t> &ScriptTemp);
static int FindSequence (const char *searchname);
static int FindSequence (FName seqname);
static bool TwiddleSeqNum (int &sequence, seqtype_t type);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FSoundSequencePtrArray Sequences;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *SSStrings[] = {
	"play",
	"playuntildone",
	"playtime",
	"playrepeat",
	"playloop",
	"delay",
	"delayonce",
	"delayrand",
	"volume",
	"volumerel",
	"volumerand",
	"end",
	"stopsound",
	"attenuation",
	"nostopcutoff",
	"slot",
	"randomsequence",
	"restart",
	// These must be last and in the same order as they appear in seqtype_t
	"platform",
	"door",
	"environment",
	NULL
};

struct SSAttenuation
{
	const char *name;
	float value;
};

static const SSAttenuation Attenuations[] = {
	{ "none", ATTN_NONE },
	{ "normal", ATTN_NORM }, 
	{ "idle", ATTN_IDLE },
	{ "static", ATTN_STATIC },
	{ "surround", ATTN_NONE },
	{ NULL, 0}
};

static const hexenseq_t HexenSequences[] = {
	{ NAME_Platform,		{ HexenPlatSeq(0), HexenPlatSeq(1), HexenPlatSeq(3), HexenLastSeq } },
	{ NAME_PlatformMetal,	{ HexenPlatSeq(2), HexenLastSeq } },
	{ NAME_Silence,			{ HexenPlatSeq(4), HexenDoorSeq(4), HexenLastSeq } },
	{ NAME_Lava,			{ HexenPlatSeq(5), HexenDoorSeq(5), HexenLastSeq } },
	{ NAME_Water,			{ HexenPlatSeq(6), HexenDoorSeq(6), HexenLastSeq } },
	{ NAME_Ice,				{ HexenPlatSeq(7), HexenDoorSeq(7), HexenLastSeq } },
	{ NAME_Earth,			{ HexenPlatSeq(8), HexenDoorSeq(8), HexenLastSeq } },
	{ NAME_PlatformMetal2,	{ HexenPlatSeq(9), HexenLastSeq } },
	{ NAME_DoorNormal,		{ HexenDoorSeq(0), HexenLastSeq } },
	{ NAME_DoorHeavy,		{ HexenDoorSeq(1), HexenLastSeq } },
	{ NAME_DoorMetal,		{ HexenDoorSeq(2), HexenLastSeq } },
	{ NAME_DoorCreak,		{ HexenDoorSeq(3), HexenLastSeq } },
	{ NAME_DoorMetal2,		{ HexenDoorSeq(9), HexenLastSeq } },
	{ NAME_Wind,			{ HexenEnvSeq(10), HexenLastSeq } },
	{ NAME_None, {0} }
};

static int SeqTrans[MAX_SNDSEQS*3];

static FRandom pr_sndseq ("SndSeq");

// CODE --------------------------------------------------------------------

IMPLEMENT_CLASS(DSeqNode, false, true)

IMPLEMENT_POINTERS_START(DSeqNode)
	IMPLEMENT_POINTER(m_ChildSeqNode)
	IMPLEMENT_POINTER(m_ParentSeqNode)
	IMPLEMENT_POINTER(m_Next)
	IMPLEMENT_POINTER(m_Prev)
IMPLEMENT_POINTERS_END

DSeqNode::DSeqNode ()
: m_SequenceChoices(0)
{
	m_Next = m_Prev = m_ChildSeqNode = m_ParentSeqNode = nullptr;
}

void DSeqNode::Serialize(FSerializer &arc)
{
	int seqOffset;
	unsigned int i;
	FName seqName = NAME_None;
	int delayTics = 0;
	FSoundID id = 0;
	float volume;
	float atten = ATTN_NORM;
	int seqnum;
	unsigned int numchoices;

	// copy these to local variables so that the actual serialization code does not need to be duplicated for saving and loading.
	if (arc.isWriting())
	{
		seqOffset = (int)SN_GetSequenceOffset(m_Sequence, m_SequencePtr);
		delayTics = m_DelayTics;
		volume = m_Volume;
		atten = m_Atten;
		id = m_CurrentSoundID;
		seqName = Sequences[m_Sequence]->SeqName;
		numchoices = m_SequenceChoices.Size();
	}
	Super::Serialize(arc);

	arc("seqoffset", seqOffset)
		("delaytics", delayTics)
		("volume", volume)
		("atten", atten)
		("modelnum", m_ModeNum)
		("next", m_Next)
		("prev", m_Prev)
		("childseqnode", m_ChildSeqNode)
		("parentseqnode", m_ParentSeqNode)
		("id", id)
		("seqname", seqName)
		("numchoices", numchoices)
		("level", Level);

	// The way this is saved makes it hard to encapsulate so just do it the hard way...
	if (arc.isWriting())
	{
		if (numchoices > 0 && arc.BeginArray("choices"))
		{
			for (i = 0; i < m_SequenceChoices.Size(); ++i)
			{
				arc(nullptr, Sequences[m_SequenceChoices[i]]->SeqName);
			}
			arc.EndArray();
		}
	}
	else
	{
		seqnum = FindSequence (seqName);
		if (seqnum >= 0)
		{
			ActivateSequence (seqnum);
		}
		else
		{
			I_Error ("Unknown sound sequence '%s'\n", seqName.GetChars());
			// Can I just Destroy() here instead of erroring out?
		}

		ChangeData (seqOffset, delayTics, volume, id);

		m_SequenceChoices.Resize(numchoices);
		if (numchoices > 0 && arc.BeginArray("choices"))
		{
			for (i = 0; i < numchoices; ++i)
			{
				arc(nullptr, seqName);
				m_SequenceChoices[i] = FindSequence(seqName);
			}
			arc.EndArray();
		}
	}
}

void DSeqNode::OnDestroy()
{
	// If this sequence was launched by a parent sequence, advance that
	// sequence now.
	if (m_ParentSeqNode != NULL && m_ParentSeqNode->m_ChildSeqNode == this)
	{
		m_ParentSeqNode->m_SequencePtr++;
		m_ParentSeqNode->m_ChildSeqNode = nullptr;
		m_ParentSeqNode = nullptr;
	}
	if (Level && Level->SequenceListHead == this)
	{
		Level->SequenceListHead = m_Next;
		GC::WriteBarrier(m_Next);
	}
	if (m_Prev)
	{
		m_Prev->m_Next = m_Next;
		GC::WriteBarrier(m_Prev, m_Next);
	}
	if (m_Next)
	{
		m_Next->m_Prev = m_Prev;
		GC::WriteBarrier(m_Next, m_Prev);
	}
	Super::OnDestroy();
}

void DSeqNode::StopAndDestroy ()
{
	if (m_ChildSeqNode != NULL)
	{
		m_ChildSeqNode->StopAndDestroy();
	}
	Destroy();
}

void DSeqNode::AddChoice (int seqnum, seqtype_t type)
{
	if (TwiddleSeqNum (seqnum, type))
	{
		m_SequenceChoices.Push (seqnum);
	}
}

DEFINE_ACTION_FUNCTION(DSeqNode, AddChoice)
{
	PARAM_SELF_PROLOGUE(DSeqNode);
	PARAM_INT(seq);
	PARAM_INT(mode);
	self->AddChoice(seq, seqtype_t(mode));
	return 0;
}


FName DSeqNode::GetSequenceName () const
{
	return Sequences[m_Sequence]->SeqName;
}

DEFINE_ACTION_FUNCTION(DSeqNode, GetSequenceName)
{
	PARAM_SELF_PROLOGUE(DSeqNode);
	ACTION_RETURN_INT(self->GetSequenceName().GetIndex());
}

IMPLEMENT_CLASS(DSeqActorNode, false, true)

IMPLEMENT_POINTERS_START(DSeqActorNode)
	IMPLEMENT_POINTER(m_Actor)
IMPLEMENT_POINTERS_END

void DSeqActorNode::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("actor", m_Actor);
}

IMPLEMENT_CLASS(DSeqPolyNode, false, false)

void DSeqPolyNode::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("poly", m_Poly);
}

IMPLEMENT_CLASS(DSeqSectorNode, false, false)

void DSeqSectorNode::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("sector",m_Sector)
		("channel", Channel);
}

//==========================================================================
//
// AssignTranslations
//
//==========================================================================

static void AssignTranslations (FScanner &sc, int seq, seqtype_t type)
{
	sc.Crossed = false;

	while (sc.GetString() && !sc.Crossed)
	{
		if (IsNum(sc.String))
		{
			SeqTrans[(atoi(sc.String) & (MAX_SNDSEQS - 1)) + type * MAX_SNDSEQS] = seq;
		}
	}
	sc.UnGet();
}

//==========================================================================
//
// AssignHexenTranslations
//
//==========================================================================

static void AssignHexenTranslations (void)
{
	unsigned int i, j, seq;

	for (i = 0; HexenSequences[i].Name != NAME_None; i++)
	{
		for (seq = 0; seq < Sequences.Size(); seq++)
		{
			if (Sequences[seq] != NULL && Sequences[seq]->SeqName == HexenSequences[i].Name)
				break;
		}
		if (seq == Sequences.Size())
			continue;

		for (j = 0; j < 4 && HexenSequences[i].Seqs[j] != HexenLastSeq; j++)
		{
			int trans;

			if (HexenSequences[i].Seqs[j] & HexenDoorSeqStart)
			{
				trans = MAX_SNDSEQS * SEQ_DOOR;
			}
			else if (HexenSequences[i].Seqs[j] & HexenEnvSeqStart)
				trans = MAX_SNDSEQS * SEQ_ENVIRONMENT;
			else
				trans = MAX_SNDSEQS * SEQ_PLATFORM;

			SeqTrans[trans + (HexenSequences[i].Seqs[j] & (MAX_SNDSEQS - 1))] = seq;
		}
	}
}

//==========================================================================
//
// S_ClearSndSeq
//
//==========================================================================

void S_ClearSndSeq()
{
	for (unsigned int i = 0; i < Sequences.Size(); i++)
	{
		if (Sequences[i])
		{
			M_Free(Sequences[i]);
		}
	}
	Sequences.Clear();
}

//==========================================================================
//
// S_ParseSndSeq
//
//==========================================================================

void S_ParseSndSeq (int levellump)
{
	TArray<uint32_t> ScriptTemp;
	int lastlump, lump;
	char seqtype = ':';
	FName seqname = NAME_None;
	FName slot = NAME_None;
	int stopsound;
	int delaybase;
	float volumebase;
	int curseq = -1;
	int val;

	// First free the old SNDSEQ data. This allows us to reload this for each level
	// and specify a level specific SNDSEQ lump!
	S_ClearSndSeq();

	// be gone, compiler warnings
	stopsound = 0;

	memset (SeqTrans, -1, sizeof(SeqTrans));
	lastlump = 0;

	while (((lump = fileSystem.FindLump ("SNDSEQ", &lastlump)) != -1 || levellump != -1) && levellump != -2)
	{
		if (lump == -1)
		{
			lump = levellump;
			levellump = -2;
		}
		FScanner sc(lump);
		while (sc.GetString ())
		{
			if (*sc.String == ':' || *sc.String == '[')
			{
				if (curseq != -1)
				{
					sc.ScriptError ("S_ParseSndSeq: Nested Script Error");
				}
				seqname = sc.String + 1;
				seqtype = sc.String[0];
				for (curseq = 0; curseq < (int)Sequences.Size(); curseq++)
				{
					if (Sequences[curseq] != NULL && Sequences[curseq]->SeqName == seqname)
					{
						M_Free (Sequences[curseq]);
						Sequences[curseq] = NULL;
						break;
					}
				}
				if (curseq == (int)Sequences.Size())
				{
					Sequences.Push (NULL);
				}
				ScriptTemp.Clear();
				stopsound = 0;
				slot = NAME_None;
				if (seqtype == '[')
				{
					sc.SetCMode (true);
					ScriptTemp.Push (0);	// to be filled when definition is complete
				}
				continue;
			}
			if (curseq == -1)
			{
				continue;
			}
			if (seqtype == '[')
			{
				if (sc.String[0] == ']')
				{ // End of this definition
					ScriptTemp[0] = MakeCommand(SS_CMD_SELECT, (ScriptTemp.Size()-1)/2);
					AddSequence (curseq, seqname, slot, stopsound, ScriptTemp);
					curseq = -1;
					sc.SetCMode (false);
				}
				else
				{ // Add a selection
					sc.UnGet();
					if (sc.CheckNumber())
					{
						ScriptTemp.Push (sc.Number);
						sc.MustGetString();
						ScriptTemp.Push (FName(sc.String).GetIndex());
					}
					else
					{
						seqtype_t seqtype = seqtype_t(sc.MustMatchString (SSStrings + SS_STRING_PLATFORM));
						AssignTranslations (sc, curseq, seqtype);
					}
				}
				continue;
			}
			switch (sc.MustMatchString (SSStrings))
			{
				case SS_STRING_PLAYUNTILDONE:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand(SS_CMD_PLAY, S_FindSound (sc.String)));
					ScriptTemp.Push(MakeCommand(SS_CMD_WAITUNTILDONE, 0));
					break;

				case SS_STRING_PLAY:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand(SS_CMD_PLAY, S_FindSound (sc.String)));
					break;

				case SS_STRING_PLAYTIME:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand(SS_CMD_PLAY, S_FindSound (sc.String)));
					sc.MustGetNumber ();
					ScriptTemp.Push(MakeCommand(SS_CMD_DELAY, sc.Number));
					break;

				case SS_STRING_PLAYREPEAT:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand (SS_CMD_PLAYREPEAT, S_FindSound (sc.String)));
					break;

				case SS_STRING_PLAYLOOP:
					sc.MustGetString ();
					ScriptTemp.Push(MakeCommand (SS_CMD_PLAYLOOP, S_FindSound (sc.String)));
					sc.MustGetNumber ();
					ScriptTemp.Push(sc.Number);
					break;

				case SS_STRING_DELAY:
					sc.MustGetNumber ();
					ScriptTemp.Push(MakeCommand(SS_CMD_DELAY, sc.Number));
					break;

				case SS_STRING_DELAYONCE:
					sc.MustGetNumber ();
					ScriptTemp.Push(MakeCommand(SS_CMD_DELAY, sc.Number));
					ScriptTemp.Push(MakeCommand(SS_CMD_LAST2NOP, 0));
					break;

				case SS_STRING_DELAYRAND:
					sc.MustGetNumber ();
					delaybase = sc.Number;
					ScriptTemp.Push(MakeCommand(SS_CMD_DELAYRAND, sc.Number));
					sc.MustGetNumber ();
					ScriptTemp.Push(max(1, sc.Number - delaybase + 1));
					break;

				case SS_STRING_VOLUME:		// volume is in range 0..100
					sc.MustGetFloat ();
					ScriptTemp.Push(MakeCommand(SS_CMD_VOLUME, int(sc.Float * (65536.f / 100.f))));
					break;

				case SS_STRING_VOLUMEREL:
					sc.MustGetFloat ();
					ScriptTemp.Push(MakeCommand(SS_CMD_VOLUMEREL, int(sc.Float * (65536.f / 100.f))));
					break;

				case SS_STRING_VOLUMERAND:
					sc.MustGetFloat ();
					volumebase = float(sc.Float);
					ScriptTemp.Push(MakeCommand(SS_CMD_VOLUMERAND, int(sc.Float * (65536.f / 100.f))));
					sc.MustGetFloat ();
					ScriptTemp.Push(int((sc.Float - volumebase) * (256/100.f)));
					break;

				case SS_STRING_STOPSOUND:
					sc.MustGetString ();
					stopsound = S_FindSound (sc.String);
					ScriptTemp.Push(MakeCommand(SS_CMD_STOPSOUND, 0));
					break;

				case SS_STRING_NOSTOPCUTOFF:
					stopsound = -1;
					ScriptTemp.Push(MakeCommand(SS_CMD_STOPSOUND, 0));
					break;

				case SS_STRING_ATTENUATION:
					if (sc.CheckFloat())
					{
						val = int(sc.Float*65536.);
					}
					else
					{
						sc.MustGetString ();
						val = sc.MustMatchString(&Attenuations[0].name, sizeof(Attenuations[0])) * 65536;
					}
					ScriptTemp.Push(MakeCommand(SS_CMD_ATTENUATION, val));
					break;

				case SS_STRING_RANDOMSEQUENCE:
					ScriptTemp.Push(MakeCommand(SS_CMD_RANDOMSEQUENCE, 0));
					break;

				case SS_STRING_RESTART:
					ScriptTemp.Push(MakeCommand(SS_CMD_BRANCH, ScriptTemp.Size()));
					break;

				case SS_STRING_END:
					AddSequence (curseq, seqname, slot, stopsound, ScriptTemp);
					curseq = -1;
					break;

				case SS_STRING_PLATFORM:
					AssignTranslations (sc, curseq, SEQ_PLATFORM);
					break;

				case SS_STRING_DOOR:
					AssignTranslations (sc, curseq, SEQ_DOOR);
					break;

				case SS_STRING_ENVIRONMENT:
					AssignTranslations (sc, curseq, SEQ_ENVIRONMENT);
					break;

				case SS_STRING_SLOT:
					sc.MustGetString();
					slot = sc.String;
					break;
			}
		}
		if (curseq > 0)
		{
			sc.ScriptError("End of file encountered before the final sequence ended.");
		}
	}

	if (gameinfo.gametype == GAME_Hexen)
		AssignHexenTranslations ();
}

static void AddSequence (int curseq, FName seqname, FName slot, int stopsound, const TArray<uint32_t> &ScriptTemp)
{
	Sequences[curseq] = (FSoundSequence *)M_Malloc (sizeof(FSoundSequence) + sizeof(uint32_t)*ScriptTemp.Size());
	Sequences[curseq]->SeqName = seqname;
	Sequences[curseq]->Slot = slot;
	Sequences[curseq]->StopSound = FSoundID(stopsound);
	memcpy (Sequences[curseq]->Script, &ScriptTemp[0], sizeof(uint32_t)*ScriptTemp.Size());
	Sequences[curseq]->Script[ScriptTemp.Size()] = MakeCommand(SS_CMD_END, 0);
}

DSeqNode::DSeqNode (FLevelLocals *l, int sequence, int modenum)
: m_CurrentSoundID(0), m_ModeNum(modenum), m_SequenceChoices(0)
{
	Level = l;
	ActivateSequence (sequence);
	if (!Level->SequenceListHead)
	{
		Level->SequenceListHead = this;
		m_Next = m_Prev = nullptr;
	}
	else
	{
		Level->SequenceListHead->m_Prev = this;		GC::WriteBarrier(Level->SequenceListHead, this);
		m_Next = Level->SequenceListHead;			GC::WriteBarrier(this, Level->SequenceListHead);
		Level->SequenceListHead = this;
		m_Prev = nullptr;
	}
	GC::WriteBarrier(this);
	m_ParentSeqNode = m_ChildSeqNode = nullptr;
}

void DSeqNode::ActivateSequence (int sequence)
{
	m_SequencePtr = Sequences[sequence]->Script;
	m_Sequence = sequence;
	m_DelayTics = 0;
	m_StopSound = Sequences[sequence]->StopSound;
	m_CurrentSoundID = 0;
	m_Volume = 1;			// Start at max volume...
	m_Atten = ATTN_IDLE;	// ...and idle attenuation
}

DSeqActorNode::DSeqActorNode(AActor* actor, int sequence, int modenum)
	: DSeqNode(actor->Level, sequence, modenum)
{
	m_Actor = actor;
}

DSeqPolyNode::DSeqPolyNode (FPolyObj *poly, int sequence, int modenum)
	: DSeqNode (poly->Level, sequence, modenum),
	  m_Poly (poly)
{
}

DSeqSectorNode::DSeqSectorNode (sector_t *sec, int chan, int sequence, int modenum)
	: DSeqNode (sec->Level, sequence, modenum),
	  Channel (chan),
	  m_Sector (sec)
{
}

//==========================================================================
//
//  SN_StartSequence
//
//==========================================================================

static bool TwiddleSeqNum (int &sequence, seqtype_t type)
{
	if (type < SEQ_NUMSEQTYPES)
	{
		// [GrafZahl] Needs some range checking:
		// Sector_ChangeSound doesn't do it so this makes invalid sequences play nothing.
		if (sequence >= 0 && sequence < MAX_SNDSEQS)
		{
			sequence = SeqTrans[sequence + type * MAX_SNDSEQS];
		}
		else
		{
			return false;
		}
	}

	return ((size_t)sequence < Sequences.Size() && Sequences[sequence] != NULL);
}

DSeqNode *SN_StartSequence (AActor *actor, int sequence, seqtype_t type, int modenum, bool nostop)
{
	if (!nostop)
	{
		SN_StopSequence (actor); // Stop any previous sequence
	}
	if (TwiddleSeqNum (sequence, type))
	{
		return Create<DSeqActorNode> (actor, sequence, modenum);
	}
	return NULL;
}

DEFINE_ACTION_FUNCTION(AActor, StartSoundSequenceID)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(seq);
	PARAM_INT(type);
	PARAM_INT(modenum);
	PARAM_BOOL(nostop);
	ACTION_RETURN_POINTER(SN_StartSequence(self, seq, seqtype_t(type), modenum, nostop));
}

DSeqNode *SN_StartSequence (sector_t *sector, int chan, int sequence, seqtype_t type, int modenum, bool nostop)
{
	if (!nostop)
	{
		SN_StopSequence (sector, chan);
	}
	if (TwiddleSeqNum (sequence, type))
	{
		return Create<DSeqSectorNode>(sector, chan, sequence, modenum);
	}
	return NULL;
}

DEFINE_ACTION_FUNCTION(_Sector, StartSoundSequenceID)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(chan);
	PARAM_INT(seq);
	PARAM_INT(type);
	PARAM_INT(modenum);
	PARAM_BOOL(nostop);
	ACTION_RETURN_POINTER(SN_StartSequence(self, chan, seq, seqtype_t(type), modenum, nostop));
}

DSeqNode *SN_StartSequence (FPolyObj *poly, int sequence, seqtype_t type, int modenum, bool nostop)
{
	if (!nostop)
	{
		SN_StopSequence (poly);
	}
	if (TwiddleSeqNum (sequence, type))
	{
		return Create<DSeqPolyNode>(poly, sequence, modenum);
	}
	return NULL;
}

//==========================================================================
//
//  SN_StartSequence (named)
//
//==========================================================================

DSeqNode *SN_StartSequence (AActor *actor, const char *seqname, int modenum)
{
	int seqnum = FindSequence(seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (actor, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

DSeqNode *SN_StartSequence (AActor *actor, FName seqname, int modenum)
{
	int seqnum = FindSequence(seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (actor, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

DEFINE_ACTION_FUNCTION(AActor, StartSoundSequence)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME(seq);
	PARAM_INT(modenum);
	ACTION_RETURN_POINTER(SN_StartSequence(self, seq, modenum));
}


DSeqNode *SN_StartSequence (sector_t *sec, int chan, const char *seqname, int modenum)
{
	int seqnum = FindSequence(seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (sec, chan, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

DSeqNode *SN_StartSequence (sector_t *sec, int chan, FName seqname, int modenum)
{
	int seqnum = FindSequence(seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (sec, chan, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

DEFINE_ACTION_FUNCTION(_Sector, StartSoundSequence)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(chan);
	PARAM_NAME(seq);
	PARAM_INT(modenum);
	ACTION_RETURN_POINTER(SN_StartSequence(self, chan, seq, modenum));
}

DSeqNode *SN_StartSequence (FPolyObj *poly, const char *seqname, int modenum)
{
	int seqnum = FindSequence(seqname);
	if (seqnum >= 0)
	{
		return SN_StartSequence (poly, seqnum, SEQ_NOTRANS, modenum);
	}
	return NULL;
}

static int FindSequence (const char *searchname)
{
	FName seqname(searchname, true);

	if (seqname != NAME_None)
	{
		return FindSequence(seqname);
	}
	return -1;
}

static int FindSequence (FName seqname)
{
	for (int i = Sequences.Size(); i-- > 0; )
	{
		if (Sequences[i] != NULL && seqname == Sequences[i]->SeqName)
		{
			return i;
		}
	}
	return -1;
}

//==========================================================================
//
// SN_CheckSequence
//
// Returns the sound sequence playing in the sector on the given channel,
// if any.
//
//==========================================================================

DSeqNode *SN_CheckSequence(sector_t *sector, int chan)
{
	auto Level = sector->Level;
	for (DSeqNode *node = Level->SequenceListHead; node; )
	{
		DSeqNode *next = node->NextSequence();
		if (node->Source() == sector)
		{
			assert(node->IsKindOf(RUNTIME_CLASS(DSeqSectorNode)));
			if ((static_cast<DSeqSectorNode *>(node)->Channel & 7) == chan)
			{
				return node;
			}
		}
		node = next;
	}
	return NULL;
}

DEFINE_ACTION_FUNCTION(_Sector, CheckSoundSequence)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(chan);
	ACTION_RETURN_POINTER(SN_CheckSequence(self, chan));
}

//==========================================================================
//
//  SN_StopSequence
//
//==========================================================================

void SN_StopSequence (AActor *actor)
{
	SN_DoStop (actor->Level, actor);
}

DEFINE_ACTION_FUNCTION(AActor, StopSoundSequence)
{
	PARAM_SELF_PROLOGUE(AActor);
	SN_StopSequence(self);
	return 0;
}

void SN_StopSequence (sector_t *sector, int chan)
{
	DSeqNode *node = SN_CheckSequence(sector, chan);
	if (node != NULL)
	{
		node->StopAndDestroy();
	}
}

DEFINE_ACTION_FUNCTION(_Sector, StopSoundSequence)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(chan);
	SN_StopSequence(self, chan);
	return 0;
}


void SN_StopSequence (FPolyObj *poly)
{
	SN_DoStop (poly->Level, poly);
}

void SN_DoStop (FLevelLocals *Level, void *source)
{
	DSeqNode *node;

	for (node = Level->SequenceListHead; node; )
	{
		DSeqNode *next = node->NextSequence();
		if (node->Source() == source)
		{
			node->StopAndDestroy ();
		}
		node = next;
	}
}

void DSeqActorNode::OnDestroy ()
{
	if (m_StopSound >= 0)
		S_StopSound (m_Actor, CHAN_BODY);
	if (m_StopSound >= 1)
		MakeSound (0, m_StopSound);
	Super::OnDestroy();
}

void DSeqSectorNode::OnDestroy ()
{
	if (m_StopSound >= 0)
		S_StopSound (m_Sector, Channel & 7);
	if (m_StopSound >= 1)
		MakeSound (0, m_StopSound);
	Super::OnDestroy();
}

void DSeqPolyNode::OnDestroy ()
{
	if (m_StopSound >= 0)
		S_StopSound (m_Poly, CHAN_BODY);
	if (m_StopSound >= 1)
		MakeSound (0, m_StopSound);
	Super::OnDestroy();
}

//==========================================================================
//
//  SN_IsMakingLoopingSound
//
//==========================================================================

bool SN_IsMakingLoopingSound (sector_t *sector)
{
	DSeqNode *node;

	for (node = sector->Level->SequenceListHead; node; )
	{
		DSeqNode *next = node->NextSequence();
		if (node->Source() == (void *)sector)
		{
			return !!(static_cast<DSeqSectorNode *>(node)->Channel & CHANF_LOOP);
		}
		node = next;
	}
	return false;
}

DEFINE_ACTION_FUNCTION(_Sector, IsMakingLoopingSound)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_BOOL(SN_IsMakingLoopingSound(self));
}


//==========================================================================
//
//  SN_UpdateActiveSequences
//
//==========================================================================

void DSeqNode::Tick ()
{
	if (m_DelayTics > 0)
	{
		if (--m_DelayTics > 0) return;
	}
	for (;;)
	{
		switch (GetCommand(*m_SequencePtr))
		{
		case SS_CMD_NONE:
			m_SequencePtr++;
			break;

		case SS_CMD_PLAY:
			if (!IsPlaying())
			{
				m_CurrentSoundID = FSoundID(GetData(*m_SequencePtr));
				MakeSound (0, m_CurrentSoundID);
			}
			m_SequencePtr++;
			break;

		case SS_CMD_WAITUNTILDONE:
			if (!IsPlaying())
			{
				m_SequencePtr++;
				m_CurrentSoundID = 0;
			}
			else
			{
				return;
			}
			break;

		case SS_CMD_PLAYREPEAT:
			if (!IsPlaying())
			{
				// Does not advance sequencePtr, so it will repeat as necessary.
				m_CurrentSoundID = FSoundID(GetData(*m_SequencePtr));
				MakeSound (CHANF_LOOP, m_CurrentSoundID);
			}
			return;

		case SS_CMD_PLAYLOOP:
			// Like SS_CMD_PLAYREPEAT, sequencePtr is not advanced, so this
			// command will repeat until the sequence is stopped.
			m_CurrentSoundID = FSoundID(GetData(m_SequencePtr[0]));
			MakeSound (0, m_CurrentSoundID);
			m_DelayTics = m_SequencePtr[1];
			return;

		case SS_CMD_RANDOMSEQUENCE:
			// If there's nothing to choose from, then there's nothing to do here.
			if (m_SequenceChoices.Size() == 0)
			{
				m_SequencePtr++;
			}
			else if (m_ChildSeqNode == NULL)
			{
				int choice = pr_sndseq() % m_SequenceChoices.Size();
				m_ChildSeqNode = SpawnChild (m_SequenceChoices[choice]);
				GC::WriteBarrier(this, m_ChildSeqNode);
				if (m_ChildSeqNode == NULL)
				{ // Failed, so skip to next instruction.
					m_SequencePtr++;
				}
				else
				{ // Copy parameters to the child and link it to this one.
					m_ChildSeqNode->m_Volume = m_Volume;
					m_ChildSeqNode->m_Atten = m_Atten;
					m_ChildSeqNode->m_ParentSeqNode = this;
					return;
				}
			}
			else
			{
				// If we get here, then the child sequence is playing, and it
				// will advance our sequence pointer for us when it finishes.
				return;
			}
			break;

		case SS_CMD_DELAY:
			m_DelayTics = GetData(*m_SequencePtr);
			m_SequencePtr++;
			m_CurrentSoundID = 0;
			return;

		case SS_CMD_DELAYRAND:
			m_DelayTics = GetData(m_SequencePtr[0]) + pr_sndseq(m_SequencePtr[1]);
			m_SequencePtr += 2;
			m_CurrentSoundID = 0;
			return;

		case SS_CMD_VOLUME:
			m_Volume = GetFloatData(*m_SequencePtr);
			m_SequencePtr++;
			break;

		case SS_CMD_VOLUMEREL:
			// like SS_CMD_VOLUME, but the new volume is added to the old volume
			m_Volume += GetFloatData(*m_SequencePtr);
			m_SequencePtr++;
			break;

		case SS_CMD_VOLUMERAND:
			// like SS_CMD_VOLUME, but the new volume is chosen randomly from a range
			m_Volume = GetFloatData(m_SequencePtr[0]) + (pr_sndseq() % m_SequencePtr[1]) / 255.f;
			m_SequencePtr += 2;
			break;

		case SS_CMD_STOPSOUND:
			// Wait until something else stops the sequence
			return;

		case SS_CMD_ATTENUATION:
			m_Atten = GetFloatData(*m_SequencePtr);
			m_SequencePtr++;
			break;

		case SS_CMD_BRANCH:
			m_SequencePtr -= GetData(*m_SequencePtr);
			break;

		case SS_CMD_SELECT:
			{ // Completely transfer control to the choice matching m_ModeNum.
			  // If no match is found, then just advance to the next command
			  // in this sequence, which should be SS_CMD_END.
 				int numchoices = GetData(*m_SequencePtr++);
				int i;

				for (i = 0; i < numchoices; ++i)
				{
					if (m_SequencePtr[i*2] == m_ModeNum)
					{
						int seqnum = FindSequence (ENamedName(m_SequencePtr[i*2+1]));
						if (seqnum >= 0)
						{ // Found a match, and it's a good one too.
							ActivateSequence (seqnum);
							break;
						}
					}
				}
				if (i == numchoices)
				{ // No match (or no good match) was found.
					m_SequencePtr += numchoices * 2;
				}
			}
			break;

		case SS_CMD_LAST2NOP:
			*(m_SequencePtr - 1) = MakeCommand(SS_CMD_NONE, 0);
			*m_SequencePtr = MakeCommand(SS_CMD_NONE, 0);
			m_SequencePtr++;
			break;

		case SS_CMD_END:
			Destroy ();
			return;

		default:
			Printf ("Corrupted sound sequence: %s\n", Sequences[m_Sequence]->SeqName.GetChars());
			Destroy ();
			return;
		}
	}
}

void SN_UpdateActiveSequences (FLevelLocals *Level)
{
	DSeqNode *node;

	if (paused)
	{ // No sequences currently playing/game is paused
		return;
	}
	for (node =Level->SequenceListHead; node; node = node->NextSequence())
	{
		node->Tick ();
	}
}

//==========================================================================
//
//  SN_StopAllSequences
//
//==========================================================================

void SN_StopAllSequences (FLevelLocals *Level)
{
	DSeqNode *node;

	for (node = Level->SequenceListHead; node; )
	{
		DSeqNode *next = node->NextSequence();
		node->m_StopSound = 0; // don't play any stop sounds
		node->Destroy ();
		node = next;
	}
}
	
//==========================================================================
//
//  SN_GetSequenceOffset
//
//==========================================================================

ptrdiff_t SN_GetSequenceOffset (int sequence, int32_t *sequencePtr)
{
	return sequencePtr - Sequences[sequence]->Script;
}

//==========================================================================
//
//  SN_GetSequenceSlot
//
//==========================================================================

FName SN_GetSequenceSlot (int sequence, seqtype_t type)
{
	if (TwiddleSeqNum (sequence, type))
	{
		return Sequences[sequence]->Slot;
	}
	return NAME_None;
}

DEFINE_ACTION_FUNCTION(DSeqNode, GetSequenceSlot)
{
	PARAM_PROLOGUE;
	PARAM_INT(seq);
	PARAM_INT(type);
	ACTION_RETURN_INT(SN_GetSequenceSlot(seq, seqtype_t(type)).GetIndex());
}


//==========================================================================
//
// SN_MarkPrecacheSounds
//
// Marks all sounds played by this sequence for precaching.
//
//==========================================================================

void SN_MarkPrecacheSounds(int sequence, seqtype_t type)
{
	if (TwiddleSeqNum(sequence, type))
	{
		FSoundSequence *seq = Sequences[sequence];

		soundEngine->MarkUsed(seq->StopSound);
		for (int i = 0; GetCommand(seq->Script[i]) != SS_CMD_END; ++i)
		{
			int cmd = GetCommand(seq->Script[i]);
			if (cmd == SS_CMD_PLAY || cmd == SS_CMD_PLAYREPEAT || cmd == SS_CMD_PLAYLOOP)
			{
				soundEngine->MarkUsed(GetData(seq->Script[i]));
			}
		}
	}
}

DEFINE_ACTION_FUNCTION(DSeqNode, MarkPrecacheSounds)
{
	PARAM_PROLOGUE;
	PARAM_INT(seq);
	PARAM_INT(type);
	SN_MarkPrecacheSounds(seq, seqtype_t(type));
	return 0;
}


//==========================================================================
//
//  SN_ChangeNodeData
//
// 	nodeNum zero is the first node
//==========================================================================

void SN_ChangeNodeData (FLevelLocals *Level, int nodeNum, int seqOffset, int delayTics, float volume,
	int currentSoundID)
{
	int i;
	DSeqNode *node;

	i = 0;
	node = Level->SequenceListHead;
	while (node && i < nodeNum)
	{
		node = node->NextSequence();
		i++;
	}
	if (!node)
	{ // reached the end of the list before finding the nodeNum-th node
		return;
	}
	node->ChangeData (seqOffset, delayTics, volume, currentSoundID);
}

void DSeqNode::ChangeData (int seqOffset, int delayTics, float volume, FSoundID currentSoundID)
{
	m_DelayTics = delayTics;
	m_Volume = volume;
	m_SequencePtr += seqOffset;
	m_CurrentSoundID = currentSoundID;
}

//==========================================================================
//
// FindMode
//
// Finds the sequence this sequence and mode combination selects.
//
//==========================================================================

static int FindMode(int seqnum, int mode)
{
	if (seqnum <= 0)
	{ // Sequence does not exist.
		return seqnum;
	}
	FSoundSequence *seq = Sequences[seqnum];
	if (GetCommand(seq->Script[0]) != SS_CMD_SELECT)
	{ // This sequence doesn't select any others.
		return seqnum;
	}
	// Search for the desired mode amongst the selections
	int nummodes = GetData(seq->Script[0]);
	for (int i = 0; i < nummodes; ++i)
	{
		if (seq->Script[1 + i*2] == mode)
		{
			return FindSequence(ENamedName(seq->Script[1 + i*2 + 1]));
		}
	}
	// The mode isn't selected, which means it stays on this sequence.
	return seqnum;
}

//==========================================================================
//
// FindModeDeep
//
// Finds the final sequence this sequence and mode combination selects.
//
//==========================================================================

static int FindModeDeep(int seqnum, int mode)
{
	int newseqnum = FindMode(seqnum, mode);

	while (newseqnum != seqnum)
	{
		seqnum = newseqnum;
		newseqnum = FindMode(seqnum, mode);
	}
	return seqnum;
}

//==========================================================================
//
// SN_AreModesSame
//
// Returns true if mode1 and mode2 represent the same sequence.
//
//==========================================================================

bool SN_AreModesSame(int seqnum, seqtype_t type, int mode1, int mode2)
{
	if (mode1 == mode2)
	{ // Obviously they're the same.
		return true;
	}
	if (TwiddleSeqNum(seqnum, type))
	{
		int mode1seq = FindModeDeep(seqnum, mode1);
		int mode2seq = FindModeDeep(seqnum, mode2);
		return mode1seq == mode2seq;
	}
	// The sequence doesn't exist, so that makes both modes equally nonexistant.
	return true;
}

DEFINE_ACTION_FUNCTION(DSeqNode, AreModesSameID)
{
	PARAM_SELF_PROLOGUE(DSeqNode);
	PARAM_INT(seqnum);
	PARAM_INT(type);
	PARAM_INT(mode);
	ACTION_RETURN_BOOL(SN_AreModesSame(seqnum, seqtype_t(type), mode, self->GetModeNum()));
}

bool SN_AreModesSame(FName name, int mode1, int mode2)
{
	int seqnum = FindSequence(name);
	if (seqnum >= 0)
	{
		return SN_AreModesSame(seqnum, SEQ_NOTRANS, mode1, mode2);
	}
	// The sequence doesn't exist, so that makes both modes equally nonexistant.
	return true;
}

DEFINE_ACTION_FUNCTION(DSeqNode, AreModesSame)
{
	PARAM_SELF_PROLOGUE(DSeqNode);
	PARAM_NAME(seq);
	PARAM_INT(mode);
	ACTION_RETURN_BOOL(SN_AreModesSame(seq, mode, self->GetModeNum()));
}

//==========================================================================
//
// CCMD playsequence
//
// Causes the player to play a sound sequence.
//==========================================================================

CCMD(playsequence)
{
	if (argv.argc() < 2 || argv.argc() > 3)
	{
		Printf ("Usage: playsequence <sound sequence name> [choice number]\n");
	}
	else
	{
		SN_StartSequence (players[consoleplayer].mo, argv[1], argv.argc() > 2 ? atoi(argv[2]) : 0);
	}
}
