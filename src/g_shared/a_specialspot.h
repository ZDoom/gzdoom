#ifndef __A_SPECSPOT_H
#define __A_SPECSPOT_H

#include "actor.h"
#include "tarray.h"

class ASpecialSpot : public AActor
{
	DECLARE_CLASS (ASpecialSpot, AActor)

public:

	void BeginPlay();
	void Destroy();
};


struct FSpotList;


class DSpotState : public DThinker
{
	DECLARE_CLASS(DSpotState, DThinker)
	static TObjPtr<DSpotState> SpotState;
	TArray<FSpotList *> SpotLists;

public:


	DSpotState ();
	void Destroy ();
	void Tick ();
	static DSpotState *GetSpotState(bool create = true);
	FSpotList *FindSpotList(const PClass *type);
	bool AddSpot(ASpecialSpot *spot);
	bool RemoveSpot(ASpecialSpot *spot);
	void Serialize(FArchive &arc);
	ASpecialSpot *GetNextInList(const PClass *type, int skipcounter);
	ASpecialSpot *GetSpotWithMinMaxDistance(const PClass *type, fixed_t x, fixed_t y, fixed_t mindist, fixed_t maxdist);
	ASpecialSpot *GetRandomSpot(const PClass *type, bool onlyonce = false);
};


#endif

