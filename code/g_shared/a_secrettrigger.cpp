#include "actor.h"
#include "g_level.h"
#include "c_console.h"
#include "info.h"
#include "s_sound.h"
#include "d_player.h"

class ASecretTrigger : public AActor
{
	DECLARE_STATELESS_ACTOR (ASecretTrigger, AActor)
public:
	void PostBeginPlay ();
	void Activate (AActor *activator);
};

IMPLEMENT_STATELESS_ACTOR (ASecretTrigger, Any, 9046, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
END_DEFAULTS

void ASecretTrigger::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	level.total_secrets++;
}

void ASecretTrigger::Activate (AActor *activator)
{
	if (activator == players[consoleplayer].camera)
	{
		if (args[0] <= 1)
		{
			C_MidPrint ("A secret is revealed!");
		}
		if (args[0] == 0 || args[0] == 2)
		{
			S_Sound (activator, CHAN_AUTO, "misc/secret", 1, ATTN_NORM);
		}
	}
	level.found_secrets++;
	Destroy ();
}

