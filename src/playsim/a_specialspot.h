#ifndef __A_SPECSPOT_H
#define __A_SPECSPOT_H

#include "actor.h"
#include "tarray.h"

struct FSpotList;


class DSpotState : public DObject
{
	DECLARE_CLASS(DSpotState, DObject)
	TArray<FSpotList> SpotLists;

public:


	DSpotState ();
	void OnDestroy() override;
	void Tick ();
	static DSpotState *GetSpotState(bool create = true);
	FSpotList *FindSpotList(PClassActor *type);
	bool AddSpot(AActor *spot);
	bool RemoveSpot(AActor *spot);
	void Serialize(FSerializer &arc);
	AActor *GetNextInList(PClassActor *type, int skipcounter);
	AActor *GetSpotWithMinMaxDistance(PClassActor *type, double x, double y, double mindist, double maxdist);
	AActor *GetRandomSpot(PClassActor *type, bool onlyonce = false);
};


#endif

