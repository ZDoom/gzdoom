#ifndef __A_SHAREDGLOBAL_H__
#define __A_SHAREDGLOBAL_H__

#include "dobject.h"
#include "info.h"

class AUnknown : public AActor
{
	DECLARE_ACTOR (AUnknown, AActor)
};

class APatrolPoint : public AActor
{
	DECLARE_STATELESS_ACTOR (APatrolPoint, AActor)
};

class ABlood : public AActor
{
	DECLARE_ACTOR (ABlood, AActor)
};

class AMapSpot : public AActor
{
	DECLARE_STATELESS_ACTOR (AMapSpot, AActor)
};

class AMapSpotGravity : public AMapSpot
{
	DECLARE_STATELESS_ACTOR (AMapSpotGravity, AMapSpot)
};

class ARealGibs : public AActor
{
	DECLARE_ACTOR (ARealGibs, AActor)
};

struct side_s;

class ADecal : public AActor
{
	DECLARE_STATELESS_ACTOR (ADecal, AActor)
public:
	void BeginPlay ();
	void Destroy ();
	void StickToWall (side_s *wall);
};

class AImpactDecal : public ADecal
{
	DECLARE_STATELESS_ACTOR (AImpactDecal, ADecal)
public:
	static AImpactDecal *StaticCreate (const char *name, fixed_t x, fixed_t y, fixed_t z, side_s *wall);
	void BeginPlay ();
	void Destroy ();
};

class AWaterSplashBase : public AActor
{
	DECLARE_ACTOR (AWaterSplashBase, AActor)
};

class AWaterSplash : public AActor
{
	DECLARE_ACTOR (AWaterSplash, AActor)
};

class ALavaSplash : public AActor
{
	DECLARE_ACTOR (ALavaSplash, AActor)
};

class ALavaSmoke : public AActor
{
	DECLARE_ACTOR (ALavaSmoke, AActor)
};

class ASludgeSplash : public AActor
{
	DECLARE_ACTOR (ASludgeSplash, AActor)
};

class ASludgeChunk : public AActor
{
	DECLARE_ACTOR (ASludgeChunk, AActor)
};

class AAmbientSound : public AActor
{
	DECLARE_STATELESS_ACTOR (AAmbientSound, AActor)
public:
	void Serialize (FArchive &arc);

	void PostBeginPlay ();
	void RunThink ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);

protected:
	bool bActive;
private:
	void SetTicker (struct AmbientSound *ambient);
	int NextCheck;
};

class ATeleportFog : public AActor
{
	DECLARE_ACTOR (ATeleportFog, AActor)
public:
	void PostBeginPlay ();
};

class ATeleportDest : public AActor
{
	DECLARE_STATELESS_ACTOR (ATeleportDest, AActor)
};

class ASkyViewpoint : public AActor
{
	DECLARE_STATELESS_ACTOR (ASkyViewpoint, AActor)
public:
	void BeginPlay ();
};

#endif //__A_SHAREDGLOBAL_H__