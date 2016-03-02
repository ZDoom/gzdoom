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
#include "farchive.h"

// SoundSequenceSlot --------------------------------------------------------

class ASoundSequenceSlot : public AActor
{
	DECLARE_CLASS (ASoundSequenceSlot, AActor)
	HAS_OBJECT_POINTERS
public:
	void Serialize (FArchive &arc);

	TObjPtr<DSeqNode> Sequence;
};

IMPLEMENT_POINTY_CLASS(ASoundSequenceSlot)
	DECLARE_POINTER(Sequence)
END_POINTERS

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
	DECLARE_CLASS (ASoundSequence, AActor)
public:
	void Destroy ();
	void PostBeginPlay ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
	void MarkPrecacheSounds () const;
};

IMPLEMENT_CLASS (ASoundSequence)

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
	FName slot = SN_GetSequenceSlot (args[0], SEQ_ENVIRONMENT);

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
			master = Spawn<ASoundSequenceSlot> (0, 0, 0, NO_REPLACE);
			master->Sequence = SN_StartSequence (master, slot, 0);
			GC::WriteBarrier(master, master->Sequence);
		}
		master->Sequence->AddChoice (args[0], SEQ_ENVIRONMENT);
		Destroy ();
	}
}

//==========================================================================
//
// ASoundSequence :: MarkPrecacheSounds
//
//==========================================================================

void ASoundSequence::MarkPrecacheSounds() const
{
	Super::MarkPrecacheSounds();
	SN_MarkPrecacheSounds(args[0], SEQ_ENVIRONMENT);
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
