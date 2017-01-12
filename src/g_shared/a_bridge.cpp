#include "actor.h"
#include "info.h"
#include "gi.h"

// Custom bridge --------------------------------------------------------

class ACustomBridge : public AActor
{
	DECLARE_CLASS (ACustomBridge, AActor)
public:
	void Destroy() override;
};

IMPLEMENT_CLASS(ACustomBridge, false, false)

void ACustomBridge::Destroy()
{
	// Hexen originally just set a flag to make the bridge balls remove themselves in A_BridgeOrbit.
	// But this is not safe with custom bridge balls that do not necessarily call that function.
	// So the best course of action is to look for all bridge balls here and destroy them ourselves.
	
	TThinkerIterator<AActor> it;
	AActor *thing;
	
	while ((thing = it.Next()))
	{
		if (thing->target == this)
		{
			thing->Destroy();
		}
	}
	Super::Destroy();
}
