#include "actor.h"
#include "a_sharedglobal.h"
#include "p_local.h"

// arg0 = Visibility*4 for this skybox

IMPLEMENT_STATELESS_ACTOR (ASkyViewpoint, Any, 9080, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
END_DEFAULTS

// If this actor has no TID, make it the default sky box
void ASkyViewpoint::BeginPlay ()
{
	Super::BeginPlay ();

	if (tid == 0)
	{
		int i;

		for (i = 0; i <numsectors; i++)
		{
			if (sectors[i].SkyBox == NULL)
			{
				sectors[i].SkyBox = this;
			}
		}
	}
}

//---------------------------------------------------------------------------

// arg0 = tid of matching SkyViewpoint
// A value of 0 means to use a regular stretched texture, in case
// there is a default SkyViewpoint in the level.
class ASkyPicker : public AActor
{
	DECLARE_STATELESS_ACTOR (ASkyPicker, AActor)
public:
	void PostBeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (ASkyPicker, Any, 9081, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
END_DEFAULTS

void ASkyPicker::PostBeginPlay ()
{
	Super::PostBeginPlay ();

	if (args[0] == 0)
	{
		subsector->sector->SkyBox = NULL;
	}
	else
	{
		TActorIterator<ASkyViewpoint> iterator (args[0]);
		ASkyViewpoint *box = iterator.Next ();

		if (box != NULL)
		{
			subsector->sector->SkyBox = box;
		}
		else
		{
			Printf ("Can't find SkyViewpoint %d for sector %d\n",
				args[0], subsector->sector - sectors);
		}
	}
	Destroy ();
}

//---------------------------------------------------------------------------

class ASectorSilencer : public AActor
{
	DECLARE_STATELESS_ACTOR (ASectorSilencer, AActor)
public:
	void PostBeginPlay ();
	void Destroy ();
};

IMPLEMENT_STATELESS_ACTOR (ASectorSilencer, Any, 9082, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_None)
END_DEFAULTS

void ASectorSilencer::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	subsector->sector->MoreFlags |= SECF_SILENT;
}

void ASectorSilencer::Destroy ()
{
	subsector->sector->MoreFlags &= ~SECF_SILENT;
	Super::Destroy ();
}
