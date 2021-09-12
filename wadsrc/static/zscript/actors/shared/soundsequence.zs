/*
** a_soundsequence.cpp
** Actors for independently playing sound sequences in a map.
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

class AmbientSound : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+DONTSPLASH
		+NOTONAUTOMAP
	}
	
	native void MarkAmbientSounds();
	override native void Tick();
	override native void Activate(Actor activator);
	override native void Deactivate(Actor activator);
	
	override void BeginPlay ()
	{
		Super.BeginPlay ();
		Activate (NULL);
	}
	
	override void MarkPrecacheSounds()
	{
		Super.MarkPrecacheSounds();
		MarkAmbientSounds();
	}
}

class AmbientSoundNoGravity : AmbientSound
{
	default
	{
		+NOGRAVITY
	}
}

class SoundSequenceSlot : Actor
{
	default
	{
		+NOSECTOR
		+NOBLOCKMAP
		+DONTSPLASH
		+NOTONAUTOMAP
	}
	
	SeqNode sequence;
}

class SoundSequence : Actor
{
	default
	{
		+NOSECTOR
		+NOBLOCKMAP
		+DONTSPLASH
		+NOTONAUTOMAP
	}
	
	//==========================================================================
	//
	// ASoundSequence :: Destroy
	//
	//==========================================================================

	override void OnDestroy ()
	{
		StopSoundSequence ();
		Super.OnDestroy();
	}

	//==========================================================================
	//
	// ASoundSequence :: PostBeginPlay
	//
	//==========================================================================

	override void PostBeginPlay ()
	{
		Name slot = SeqNode.GetSequenceSlot (args[0], SeqNode.ENVIRONMENT);

		if (slot != 'none')
		{ // This is a slotted sound, so add it to the master for that slot
			SoundSequenceSlot master;
			let locator = ThinkerIterator.Create("SoundSequenceSlot");

			while ((master = SoundSequenceSlot(locator.Next ())))
			{
				if (master.Sequence.GetSequenceName() == slot)
				{
					break;
				}
			}
			if (master == NULL)
			{
				master = SoundSequenceSlot(Spawn("SoundSequenceSlot"));
				master.Sequence = master.StartSoundSequence (slot, 0);
			}
			master.Sequence.AddChoice (args[0], SeqNode.ENVIRONMENT);
			Destroy ();
		}
	}

	//==========================================================================
	//
	// ASoundSequence :: MarkPrecacheSounds
	//
	//==========================================================================

	override void MarkPrecacheSounds()
	{
		Super.MarkPrecacheSounds();
		SeqNode.MarkPrecacheSounds(args[0], SeqNode.ENVIRONMENT);
	}

	//==========================================================================
	//
	// ASoundSequence :: Activate
	//
	//==========================================================================

	override void Activate (Actor activator)
	{
		StartSoundSequenceID (args[0], SeqNode.ENVIRONMENT, args[1]);
	}

	//==========================================================================
	//
	// ASoundSequence :: Deactivate
	//
	//==========================================================================

	override void Deactivate (Actor activator)
	{
		StopSoundSequence ();
	}

	
}

// Heretic Sound sequences -----------------------------------------------------------

class HereticSoundSequence1 : SoundSequence
{
	default
	{
		Args 0;
	}
}

class HereticSoundSequence2 : SoundSequence
{
	default
	{
		Args 1;
	}
}

class HereticSoundSequence3 : SoundSequence
{
	default
	{
		Args 2;
	}
}

class HereticSoundSequence4 : SoundSequence
{
	default
	{
		Args 3;
	}
}

class HereticSoundSequence5 : SoundSequence
{
	default
	{
		Args 4;
	}
}

class HereticSoundSequence6 : SoundSequence
{
	default
	{
		Args 5;
	}
}

class HereticSoundSequence7 : SoundSequence
{
	default
	{
		Args 6;
	}
}

class HereticSoundSequence8 : SoundSequence
{
	default
	{
		Args 7;
	}
}

class HereticSoundSequence9 : SoundSequence
{
	default
	{
		Args 8;
	}
}

class HereticSoundSequence10 : SoundSequence
{
	default
	{
		Args 9;
	}
}

