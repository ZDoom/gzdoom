#ifndef __A_SHAREDGLOBAL_H__
#define __A_SHAREDGLOBAL_H__

#include "info.h"
#include "actor.h"

class FDecalTemplate;
struct vertex_t;
struct side_t;
struct F3DFloor;

void P_SpawnDirt (AActor *actor, fixed_t radius);
class DBaseDecal *ShootDecal(const FDecalTemplate *tpl, AActor *basisactor, sector_t *sec, fixed_t x, fixed_t y, fixed_t z, angle_t angle, fixed_t tracedist, bool permanent);

class DBaseDecal : public DThinker
{
	DECLARE_CLASS (DBaseDecal, DThinker)
	HAS_OBJECT_POINTERS
public:
	DBaseDecal ();
	DBaseDecal (fixed_t z);
	DBaseDecal (int statnum, fixed_t z);
	DBaseDecal (const AActor *actor);
	DBaseDecal (const DBaseDecal *basis);

	void Serialize (FArchive &arc);
	void Destroy ();
	FTextureID StickToWall (side_t *wall, fixed_t x, fixed_t y, F3DFloor * ffloor);
	fixed_t GetRealZ (const side_t *wall) const;
	void SetShade (DWORD rgb);
	void SetShade (int r, int g, int b);
	void Spread (const FDecalTemplate *tpl, side_t *wall, fixed_t x, fixed_t y, fixed_t z, F3DFloor * ffloor);
	void GetXY (side_t *side, fixed_t &x, fixed_t &y) const;

	static void SerializeChain (FArchive &arc, DBaseDecal **firstptr);

	DBaseDecal *WallNext, **WallPrev;

	fixed_t LeftDistance;
	fixed_t Z;
	fixed_t ScaleX, ScaleY;
	fixed_t Alpha;
	DWORD AlphaColor;
	int Translation;
	FTextureID PicNum;
	DWORD RenderFlags;
	FRenderStyle RenderStyle;
	sector_t * Sector;	// required for 3D floors

protected:
	virtual DBaseDecal *CloneSelf (const FDecalTemplate *tpl, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor) const;
	void CalcFracPos (side_t *wall, fixed_t x, fixed_t y);
	void Remove ();

	static void SpreadLeft (fixed_t r, vertex_t *v1, side_t *feelwall, F3DFloor *ffloor);
	static void SpreadRight (fixed_t r, side_t *feelwall, fixed_t wallsize, F3DFloor *ffloor);
};

class DImpactDecal : public DBaseDecal
{
	DECLARE_CLASS (DImpactDecal, DBaseDecal)
public:
	DImpactDecal (fixed_t z);
	DImpactDecal (side_t *wall, const FDecalTemplate *templ);

	static DImpactDecal *StaticCreate (const char *name, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor, PalEntry color=0);
	static DImpactDecal *StaticCreate (const FDecalTemplate *tpl, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor, PalEntry color=0);

	void BeginPlay ();
	void Destroy ();

	void Serialize (FArchive &arc);
	static void SerializeTime (FArchive &arc);

protected:
	DBaseDecal *CloneSelf (const FDecalTemplate *tpl, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor) const;
	static void CheckMax ();

private:
	DImpactDecal();
};

class ATeleportFog : public AActor
{
	DECLARE_CLASS (ATeleportFog, AActor)
public:
	void PostBeginPlay ();
};

class ASkyViewpoint : public AActor
{
	DECLARE_CLASS (ASkyViewpoint, AActor)
	HAS_OBJECT_POINTERS
public:
	void Serialize (FArchive &arc);
	void BeginPlay ();
	void Destroy ();
	bool bInSkybox;
	bool bAlways;
	TObjPtr<ASkyViewpoint> Mate;
};

// For an EE compatible linedef based definition.
class ASkyCamCompat : public ASkyViewpoint
{
	DECLARE_CLASS (ASkyCamCompat, ASkyViewpoint)

public:
	void BeginPlay ()
	{
		// Do not call the SkyViewpoint's super method because it would trash our setup
		AActor::BeginPlay();
	}
};


class AStackPoint : public ASkyViewpoint
{
	DECLARE_CLASS (AStackPoint, ASkyViewpoint)
public:
	void BeginPlay ();
};

class DFlashFader : public DThinker
{
	DECLARE_CLASS (DFlashFader, DThinker)
	HAS_OBJECT_POINTERS
public:
	DFlashFader (float r1, float g1, float b1, float a1,
				 float r2, float g2, float b2, float a2,
				 float time, AActor *who);
	void Destroy ();
	void Serialize (FArchive &arc);
	void Tick ();
	AActor *WhoFor() { return ForWho; }
	void Cancel ();

protected:
	float Blends[2][4];
	int TotalTics;
	int StartTic;
	TObjPtr<AActor> ForWho;

	void SetBlend (float time);
	DFlashFader ();
};

enum
{
	QF_RELATIVE =		1,
	QF_SCALEDOWN =		1 << 1,
	QF_SCALEUP =		1 << 2,
	QF_MAX =			1 << 3,
	QF_FULLINTENSITY =	1 << 4,
	QF_WAVE =			1 << 5,
};

struct FQuakeJiggers
{
	int IntensityX, IntensityY, IntensityZ;
	int RelIntensityX, RelIntensityY, RelIntensityZ;
	int OffsetX, OffsetY, OffsetZ;
	int RelOffsetX, RelOffsetY, RelOffsetZ;
};

class DEarthquake : public DThinker
{
	DECLARE_CLASS (DEarthquake, DThinker)
	HAS_OBJECT_POINTERS
public:
	DEarthquake(AActor *center, int intensityX, int intensityY, int intensityZ, int duration,
		int damrad, int tremrad, FSoundID quakesfx, int flags, 
		double waveSpeedX, double waveSpeedY, double waveSpeedZ);

	void Serialize (FArchive &arc);
	void Tick ();
	TObjPtr<AActor> m_Spot;
	fixed_t m_TremorRadius, m_DamageRadius;
	int m_Countdown;
	int m_CountdownStart;
	FSoundID m_QuakeSFX;
	int m_Flags;
	fixed_t m_IntensityX, m_IntensityY, m_IntensityZ;
	float m_WaveSpeedX, m_WaveSpeedY, m_WaveSpeedZ;

	fixed_t GetModIntensity(int intensity) const;
	fixed_t GetModWave(double waveMultiplier) const;

	static int StaticGetQuakeIntensities(AActor *viewer, FQuakeJiggers &jiggers);

private:
	DEarthquake ();
};

class AMorphProjectile : public AActor
{
	DECLARE_CLASS (AMorphProjectile, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
	void Serialize (FArchive &arc);

	FNameNoInit	PlayerClass, MonsterClass, MorphFlash, UnMorphFlash;
	int Duration, MorphStyle;
};

class AMorphedMonster : public AActor
{
	DECLARE_CLASS (AMorphedMonster, AActor)
	HAS_OBJECT_POINTERS
public:
	void Tick ();
	void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor, int dmgflags);
	void Destroy ();

	TObjPtr<AActor> UnmorphedMe;
	int UnmorphTime, MorphStyle;
	const PClass *MorphExitFlash;
	ActorFlags FlagsSave;
};

class AMapMarker : public AActor
{
	DECLARE_CLASS(AMapMarker, AActor)
public:
	void BeginPlay ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

class AFastProjectile : public AActor
{
	DECLARE_CLASS(AFastProjectile, AActor)
public:
	void Tick ();
	virtual void Effect();
};


#endif //__A_SHAREDGLOBAL_H__
