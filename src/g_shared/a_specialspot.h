#ifndef __A_SPECSPOT_H
#define __A_SPECSPOT_H

#include "actor.h"
#include "tarray.h"

class ASpecialSpot : public AActor
{
	DECLARE_CLASS (ASpecialSpot, AActor)

public:

	void BeginPlay();
	void Destroy() override;
};


struct FSpotList;


class DSpotState : public DThinker
{
	DECLARE_CLASS(DSpotState, DThinker)
	static TObjPtr<DSpotState> SpotState;
	TArray<FSpotList> SpotLists;

public:


	DSpotState ();
	void Destroy() override;
	void Tick ();
	static DSpotState *GetSpotState(bool create = true);
	FSpotList *FindSpotList(PClassActor *type);
	bool AddSpot(ASpecialSpot *spot);
	bool RemoveSpot(ASpecialSpot *spot);
	void Serialize(FSerializer &arc);
	ASpecialSpot *GetNextInList(PClassActor *type, int skipcounter);
	ASpecialSpot *GetSpotWithMinMaxDistance(PClassActor *type, double x, double y, double mindist, double maxdist);
	ASpecialSpot *GetRandomSpot(PClassActor *type, bool onlyonce = false);
};


#endif

