#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"

// Scripted ambients --------------------------------------------------------

enum
{
	afxcmd_play,		// (sound)
	afxcmd_playabsvol,	// (sound, volume)
	afxcmd_playrelvol,	// (sound, volume)
	afxcmd_delay,		// (ticks)
	afxcmd_delayrand,	// (andbits)
	afxcmd_end			// ()
};

static long AmbSndSeqInit[] =
{ // Startup
	afxcmd_end
};
static long AmbSndSeq1[] =
{ // Scream
	afxcmd_play, (long)"world/amb1",
	afxcmd_end
};
static long AmbSndSeq2[] =
{ // Squish
	afxcmd_play, (long)"world/amb2",
	afxcmd_end
};
static long AmbSndSeq3[] =
{ // Drops
	afxcmd_play, (long)"world/amb3",
	afxcmd_delay, 16,
	afxcmd_delayrand, 31,
	afxcmd_play, (long)"world/amb7",
	afxcmd_delay, 16,
	afxcmd_delayrand, 31,
	afxcmd_play, (long)"world/amb3",
	afxcmd_delay, 16,
	afxcmd_delayrand, 31,
	afxcmd_play, (long)"world/amb7",
	afxcmd_delay, 16,
	afxcmd_delayrand, 31,
	afxcmd_play, (long)"world/amb3",
	afxcmd_delay, 16,
	afxcmd_delayrand, 31,
	afxcmd_play, (long)"world/amb7",
	afxcmd_delay, 16,
	afxcmd_delayrand, 31,
	afxcmd_end
};
static long AmbSndSeq4[] =
{ // SlowFootSteps
	afxcmd_play, (long)"world/amb4",
	afxcmd_delay, 15,
	afxcmd_playrelvol, (long)"world/amb11", -3,
	afxcmd_delay, 15,
	afxcmd_playrelvol, (long)"world/amb4", -3,
	afxcmd_delay, 15,
	afxcmd_playrelvol, (long)"world/amb11", -3,
	afxcmd_delay, 15,
	afxcmd_playrelvol, (long)"world/amb4", -3,
	afxcmd_delay, 15,
	afxcmd_playrelvol, (long)"world/amb11", -3,
	afxcmd_delay, 15,
	afxcmd_playrelvol, (long)"world/amb4", -3,
	afxcmd_delay, 15,
	afxcmd_playrelvol, (long)"world/amb11", -3,
	afxcmd_end
};
static long AmbSndSeq5[] =
{ // Heartbeat
	afxcmd_play, (long)"world/amb5",
	afxcmd_delay, 35,
	afxcmd_play, (long)"world/amb5",
	afxcmd_delay, 35,
	afxcmd_play, (long)"world/amb5",
	afxcmd_delay, 35,
	afxcmd_play, (long)"world/amb5",
	afxcmd_end
};
static long AmbSndSeq6[] =
{ // Bells
	afxcmd_play, (long)"world/amb6",
	afxcmd_delay, 17,
	afxcmd_playrelvol, (long)"world/amb6", -8,
	afxcmd_delay, 17,
	afxcmd_playrelvol, (long)"world/amb6", -8,
	afxcmd_delay, 17,
	afxcmd_playrelvol, (long)"world/amb6", -8,
	afxcmd_end
};
static long AmbSndSeq7[] =
{ // Growl
	afxcmd_play, (long)"world/amb12",
	afxcmd_end
};
static long AmbSndSeq8[] =
{ // Magic
	afxcmd_play, (long)"world/amb8",
	afxcmd_end
};
static long AmbSndSeq9[] =
{ // Laughter
	afxcmd_play, (long)"world/amb9",
	afxcmd_delay, 16,
	afxcmd_playrelvol, (long)"world/amb9", -4,
	afxcmd_delay, 16,
	afxcmd_playrelvol, (long)"world/amb9", -4,
	afxcmd_delay, 16,
	afxcmd_playrelvol, (long)"world/amb10", -4,
	afxcmd_delay, 16,
	afxcmd_playrelvol, (long)"world/amb10", -4,
	afxcmd_delay, 16,
	afxcmd_playrelvol, (long)"world/amb10", -4,
	afxcmd_end
};
static long AmbSndSeq10[] =
{ // FastFootsteps
	afxcmd_play, (long)"world/amb4",
	afxcmd_delay, 8,
	afxcmd_playrelvol, (long)"world/amb11", -3,
	afxcmd_delay, 8,
	afxcmd_playrelvol, (long)"world/amb4", -3,
	afxcmd_delay, 8,
	afxcmd_playrelvol, (long)"world/amb11", -3,
	afxcmd_delay, 8,
	afxcmd_playrelvol, (long)"world/amb4", -3,
	afxcmd_delay, 8,
	afxcmd_playrelvol, (long)"world/amb11", -3,
	afxcmd_delay, 8,
	afxcmd_playrelvol, (long)"world/amb4", -3,
	afxcmd_delay, 8,
	afxcmd_playrelvol, (long)"world/amb11", -3,
	afxcmd_end
};

static long *BaseAmbientSfx[] =
{
	AmbSndSeqInit,
	AmbSndSeq1,		// Scream
	AmbSndSeq2,		// Squish
	AmbSndSeq3,		// Drops
	AmbSndSeq4,		// SlowFootsteps
	AmbSndSeq5,		// Heartbeat
	AmbSndSeq6,		// Bells
	AmbSndSeq7,		// Growl
	AmbSndSeq8,		// Magic
	AmbSndSeq9,		// Laughter
	AmbSndSeq10		// FastFootsteps
};

#define NUMSNDSEQ	(sizeof(BaseAmbientSfx)/sizeof(BaseAmbientSfx[0]))

// Master for all scripted ambient sounds on a level ------------------------
// Only one ambient will ever play at a time.

class AScriptedAmbientMaster : public AActor
{
	DECLARE_STATELESS_ACTOR (AScriptedAmbientMaster, AActor)
public:
	void BeginPlay ();
	void RunThink ();
	void AddAmbient (int sfx);

	void Serialize (FArchive &arc);
protected:
	long *AmbientSfx;
	long *AmbSfxPtr;
	int AmbSfxTics;
	float AmbSfxVolume;
	TArray<long *> LevelAmbientSfx;
private:
	byte LocateSfx (long *ptr);
};

IMPLEMENT_STATELESS_ACTOR (AScriptedAmbientMaster, Heretic, -1, 0)
	PROP_Flags (MF_NOSECTOR|MF_NOBLOCKMAP)
END_DEFAULTS

void AScriptedAmbientMaster::Serialize (FArchive &arc)
{
	byte seq;
	byte ofs;
	DWORD i;
	DWORD numsfx;

	Super::Serialize (arc);
	arc << AmbSfxTics << AmbSfxVolume;

	if (arc.IsStoring ())
	{
		seq = LocateSfx (AmbientSfx);
		ofs = (AmbSfxPtr && AmbientSfx) ? AmbSfxPtr - AmbientSfx : 255;
		arc << seq << ofs;

		arc.WriteCount (LevelAmbientSfx.Size ());
		for (i = 0; i < LevelAmbientSfx.Size (); i++)
		{
			seq = LocateSfx (LevelAmbientSfx[i]);
			arc << seq;
		}
	}
	else
	{
		byte ofs;

		arc << seq << ofs;
		if (seq >= NUMSNDSEQ)
		{
			AmbientSfx = AmbSfxPtr = AmbSndSeqInit;
		}
		else
		{
			AmbientSfx = BaseAmbientSfx[seq];
			AmbSfxPtr = ofs < 255 ? AmbientSfx + ofs : AmbSndSeqInit;
		}
		LevelAmbientSfx.Clear ();
		numsfx = arc.ReadCount ();
		for (i = 0; i < numsfx; i++)
		{
			arc << seq;
			if (seq < NUMSNDSEQ)
			{
				long *ptr = BaseAmbientSfx[seq];
				LevelAmbientSfx.Push (ptr);
			}
		}
	}
}

byte AScriptedAmbientMaster::LocateSfx (long *ptr)
{
	size_t i;

	for (i = 0; i < NUMSNDSEQ; i++)
	{
		if (BaseAmbientSfx[i] == ptr)
			return i;
	}
	return 255;
}

void AScriptedAmbientMaster::BeginPlay ()
{
	Super::BeginPlay ();
	AmbientSfx = AmbSfxPtr = AmbSndSeqInit;
	AmbSfxTics = 10*TICRATE;
	AmbSfxVolume = 0.f;
	LevelAmbientSfx.Clear ();
}

void AScriptedAmbientMaster::AddAmbient (int sfx)
{
	if ((unsigned)sfx >= NUMSNDSEQ)
		return;

	long *ptr = BaseAmbientSfx[sfx];

	LevelAmbientSfx.Push (ptr);
}

void AScriptedAmbientMaster::RunThink ()
{
	// No need to call Super::RunThink(), because the only effect this
	// actor has on the world is aural.

	long cmd;
	const char *sound;
	bool done;

	if (LevelAmbientSfx.Size () == 0)
	{ // No ambient sound sequences on current level
		Destroy ();
		return;
	}
	if (--AmbSfxTics)
	{
		return;
	}
	done = false;
	do
	{
		cmd = *AmbSfxPtr++;
		switch (cmd)
		{
		case afxcmd_play:
			sound = (const char *)(*AmbSfxPtr++);
			AmbSfxVolume = (float)P_Random(pr_afx) / 510.f;
			S_Sound (this, CHAN_ITEM, sound, AmbSfxVolume, ATTN_NONE);
			break;

		case afxcmd_playabsvol:
			sound = (const char *)(*AmbSfxPtr++);
			AmbSfxVolume = (float)*AmbSfxPtr++ / 127.f;
			S_Sound (this, CHAN_ITEM, sound, AmbSfxVolume, ATTN_NONE);
			break;

		case afxcmd_playrelvol:
			sound = (const char *)(*AmbSfxPtr++);
			AmbSfxVolume += (float)*AmbSfxPtr++ / 127.f;
			if (AmbSfxVolume < 0.f)
			{
				AmbSfxVolume = 0.f;
			}
			else if (AmbSfxVolume > 1.f)
			{
				AmbSfxVolume = 1.f;
			}
			S_Sound (this, CHAN_ITEM, sound, AmbSfxVolume, ATTN_NONE);
			break;

		case afxcmd_delay:
			AmbSfxTics = *AmbSfxPtr++;
			done = true;
			break;

		case afxcmd_delayrand:
			AmbSfxTics = P_Random(pr_afx) & (*AmbSfxPtr++);
			done = true;
			break;

		case afxcmd_end:
			AmbSfxTics = 6*TICRATE + P_Random(pr_afx);
			AmbSfxPtr = LevelAmbientSfx[P_Random() % LevelAmbientSfx.Size ()];
			done = true;
			break;

		default:
			DPrintf ("P_AmbientSound: Unknown afxcmd %ld", cmd);
			break;
		}
	} while (done == false);
}

// Individual ambient sound things ------------------------------------------
// They find a master (or create one if none found) and add the corresponding
// ambient sound to its list of effects and then destroy themselves.

class AScriptedAmbient : public AActor
{
	DECLARE_STATELESS_ACTOR (AScriptedAmbient, AActor)
public:
	void PostBeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (AScriptedAmbient, Heretic, -1, 0)
	PROP_Flags (MF_NOSECTOR|MF_NOBLOCKMAP)
END_DEFAULTS

void AScriptedAmbient::PostBeginPlay ()
{
	const size_t ambientNum = health - 1200;
	if (ambientNum < NUMSNDSEQ)
	{
		AScriptedAmbientMaster *master;
		TThinkerIterator<AScriptedAmbientMaster> locater;

		master = locater.Next ();
		if (master == NULL)
			master = Spawn<AScriptedAmbientMaster> (0, 0, 0);
		master->AddAmbient (ambientNum);
	}
	Destroy ();
}

#define ADD_AMBIENT(x) \
	class AScriptedAmbient##x : public AScriptedAmbient { \
		DECLARE_STATELESS_ACTOR (AScriptedAmbient##x, AScriptedAmbient) };\
	IMPLEMENT_STATELESS_ACTOR (AScriptedAmbient##x, Heretic, 1199+x, 0) \
		PROP_SpawnHealth (1199+x) \
	END_DEFAULTS

ADD_AMBIENT (1);
ADD_AMBIENT (2);
ADD_AMBIENT (3);
ADD_AMBIENT (4);
ADD_AMBIENT (5);
ADD_AMBIENT (6);
ADD_AMBIENT (7);
ADD_AMBIENT (8);
ADD_AMBIENT (9);
ADD_AMBIENT (10);
