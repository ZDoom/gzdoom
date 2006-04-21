/*
** a_soundsequence.cpp
** Actors for independantly playing sound sequences in a map.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
** A SoundSequence actor has two modes of operation:
**
**   1. If the sound sequence assigned to it has a slot, then a separate
**      SoundSequenceSlot actor is spawned (if not already present), and
**      this actor's sound sequence is added to its list of choices. This
**      actor is then destroyed, never to be heard from again. The sound
**      sequence for the slot is automatically played on the new
**      SoundSequenceSlot actor, and it should at some point execute the
**      randomsequence command so that it can pick one of the other
**      sequences to play. The slot sequence should also end with restart
**      so that more than one sequence will have a chance to play.
**
**      In this mode, it is very much like world $ambient sounds defined
**      in SNDINFO but more flexible.
**
**   2. If the sound sequence assigned to it has no slot, then it will play
**      the sequence when activated and cease playing the sequence when
**      deactivated.
**
**      In this mode, it is very much like point $ambient sounds defined
**      in SNDINFO but more flexible.
**
** To assign a sound sequence, set the SoundSequence's first argument to
** the ID of the corresponding environment sequence you want to use. If
** that sequence is a multiple-choice sequence, then the second argument
** selects which choice it picks.
*/

#include <stddef.h>

#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "s_sndseq.h"

// SoundSequenceSlot --------------------------------------------------------

class ASoundSequenceSlot : public AActor
{
	DECLARE_STATELESS_ACTOR (ASoundSequenceSlot, AActor)
public:
	void Serialize (FArchive &arc);

	DSeqNode *Sequence;
};

IMPLEMENT_STATELESS_ACTOR (ASoundSequenceSlot, Any, -1, 0)
	PROP_Flags (MF_NOSECTOR|MF_NOBLOCKMAP)
END_DEFAULTS

//==========================================================================
//
// ASoundSequenceSlot :: Serialize
//
//==========================================================================

void ASoundSequenceSlot::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Sequence;
}

// SoundSequence ------------------------------------------------------------

class ASoundSequence : public AActor
{
	DECLARE_STATELESS_ACTOR (ASoundSequence, AActor)
public:
	void Destroy ();
	void PostBeginPlay ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

IMPLEMENT_STATELESS_ACTOR (ASoundSequence, Any, 14066, 0)
	PROP_Flags (MF_NOSECTOR|MF_NOBLOCKMAP)
END_DEFAULTS

//==========================================================================
//
// ASoundSequence :: Destroy
//
//==========================================================================

void ASoundSequence::Destroy ()
{
	SN_StopSequence (this);
	Super::Destroy();
}

//==========================================================================
//
// ASoundSequence :: PostBeginPlay
//
//==========================================================================

void ASoundSequence::PostBeginPlay ()
{
	name slot = SN_GetSequenceSlot (args[0], SEQ_ENVIRONMENT);

	if (slot != NAME_None)
	{ // This is a slotted sound, so add it to the master for that slot
		ASoundSequenceSlot *master;
		TThinkerIterator<ASoundSequenceSlot> locator;

		while (NULL != (master = locator.Next ()))
		{
			if (master->Sequence->GetSequenceName() == slot)
			{
				break;
			}
		}
		if (master == NULL)
		{
			master = Spawn<ASoundSequenceSlot> (0, 0, 0);
			master->Sequence = SN_StartSequence (master, slot, 0);
		}
		master->Sequence->AddChoice (args[0], SEQ_ENVIRONMENT);
		Destroy ();
	}
}

//==========================================================================
//
// ASoundSequence :: Activate
//
//==========================================================================

void ASoundSequence::Activate (AActor *activator)
{
	SN_StartSequence (this, args[0], SEQ_ENVIRONMENT, args[1]);
}

//==========================================================================
//
// ASoundSequence :: Deactivate
//
//==========================================================================

void ASoundSequence::Deactivate (AActor *activator)
{
	SN_StopSequence (this);
}

//==========================================================================
//
// Predefined sound sequence actors for Heretic. These use health as the
// sequence ID rather than args[0].
//
//==========================================================================

class AHereticSoundSequence : public ASoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence, ASoundSequence)
public:
	void PostBeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence, Heretic, -1, 0)
END_DEFAULTS

//==========================================================================
//
// AHereticSoundSequence :: PostBeginPlay
//
//==========================================================================

void AHereticSoundSequence::PostBeginPlay ()
{
	args[0] = health - 1200;
	Super::PostBeginPlay();
}

// SoundSequence1 -----------------------------------------------------------

class AHereticSoundSequence1 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence1, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence1, Heretic, 1200, 0)
	PROP_SpawnHealth (1200)
END_DEFAULTS

// SoundSequence2 -----------------------------------------------------------

class AHereticSoundSequence2 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence2, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence2, Heretic, 1201, 0)
	PROP_SpawnHealth (1201)
END_DEFAULTS

// SoundSequence3 -----------------------------------------------------------

class AHereticSoundSequence3 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence3, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence3, Heretic, 1202, 0)
	PROP_SpawnHealth (1202)
END_DEFAULTS

// SoundSequence4 -----------------------------------------------------------

class AHereticSoundSequence4 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence4, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence4, Heretic, 1203, 0)
	PROP_SpawnHealth (1203)
END_DEFAULTS

// SoundSequence5 -----------------------------------------------------------

class AHereticSoundSequence5 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence5, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence5, Heretic, 1204, 0)
	PROP_SpawnHealth (1204)
END_DEFAULTS

// SoundSequence6 -----------------------------------------------------------

class AHereticSoundSequence6 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence6, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence6, Heretic, 1205, 0)
	PROP_SpawnHealth (1205)
END_DEFAULTS

// SoundSequence7 -----------------------------------------------------------

class AHereticSoundSequence7 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence7, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence7, Heretic, 1206, 0)
	PROP_SpawnHealth (1206)
END_DEFAULTS

// SoundSequence8 -----------------------------------------------------------

class AHereticSoundSequence8 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence8, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence8, Heretic, 1207, 0)
	PROP_SpawnHealth (1207)
END_DEFAULTS

// SoundSequence9 -----------------------------------------------------------

class AHereticSoundSequence9 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence9, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence9, Heretic, 1208, 0)
	PROP_SpawnHealth (1208)
END_DEFAULTS

// SoundSequence10 ----------------------------------------------------------

class AHereticSoundSequence10 : public AHereticSoundSequence
{
	DECLARE_STATELESS_ACTOR (AHereticSoundSequence10, AHereticSoundSequence)
};

IMPLEMENT_STATELESS_ACTOR (AHereticSoundSequence10, Heretic, 1209, 0)
	PROP_SpawnHealth (1209)
END_DEFAULTS
