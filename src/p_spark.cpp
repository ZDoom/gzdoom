#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_effect.h"
#include "s_sound.h"

class ASpark : public AActor
{
	DECLARE_STATELESS_ACTOR (ASpark, AActor)
public:
	angle_t AngleIncrements ();
	void Activate (AActor *activator);
};

IMPLEMENT_STATELESS_ACTOR (ASpark, Any, 9026, 0)
	PROP_Flags (MF_NOSECTOR|MF_NOBLOCKMAP|MF_NOGRAVITY)
END_DEFAULTS

angle_t ASpark::AngleIncrements ()
{
	return ANGLE_1;
}

void ASpark::Activate (AActor *activator)
{
	char sound[16];

	Super::Activate (activator);
	P_DrawSplash (args[0] ? args[0] : 32, x, y, z, angle, 1);
	sprintf (sound, "world/spark%d", 1+(P_Random(pr_spark) % 3));
	S_Sound (this, CHAN_AUTO, sound, 1, ATTN_STATIC);
}
