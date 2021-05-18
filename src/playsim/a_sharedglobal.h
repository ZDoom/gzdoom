#ifndef __A_SHAREDGLOBAL_H__
#define __A_SHAREDGLOBAL_H__

#include "info.h"
#include "actor.h"

class FDecalTemplate;
struct vertex_t;
struct side_t;
struct F3DFloor;
class DBaseDecal;
struct SpreadInfo;

DBaseDecal *ShootDecal(FLevelLocals *Level, const FDecalTemplate *tpl, sector_t *sec, double x, double y, double z, DAngle angle, double tracedist, bool permanent);
void SprayDecal(AActor *shooter, const char *name,double distance = 172., DVector3 offset = DVector3(0., 0., 0.), DVector3 direction = DVector3(0., 0., 0.), bool useBloodColor = false, uint32_t decalColor = 0);

class DBaseDecal : public DThinker
{
	DECLARE_CLASS (DBaseDecal, DThinker)
	HAS_OBJECT_POINTERS
public:
	static const int DEFAULT_STAT = STAT_DECAL;
	void Construct(double z = 0);
	void Construct(const AActor *actor);
	void Construct(const DBaseDecal *basis);

	void Serialize(FSerializer &arc);
	void OnDestroy() override;
	virtual void Expired() {}	// For thinkers that can remove their decal. For impact decal bookkeeping.
	FTextureID StickToWall(side_t *wall, double x, double y, F3DFloor * ffloor);
	double GetRealZ (const side_t *wall) const;
	void SetShade (uint32_t rgb);
	void SetShade (int r, int g, int b);
	void SetTranslation(uint32_t trans);
	void Spread (const FDecalTemplate *tpl, side_t *wall, double x, double y, double z, F3DFloor * ffloor);
	void GetXY (side_t *side, double &x, double &y) const;

	DBaseDecal *WallNext = nullptr, *WallPrev = nullptr;

	double LeftDistance = 0;
	double Z;
	double ScaleX = 1, ScaleY = 1;
	double Alpha = 1;
	uint32_t AlphaColor = 0;
	int Translation = 0;
	FTextureID PicNum;
	uint32_t RenderFlags = 0;
	FRenderStyle RenderStyle;
	side_t *Side = nullptr;
	sector_t *Sector = nullptr;

protected:
	virtual DBaseDecal *CloneSelf(const FDecalTemplate *tpl, double x, double y, double z, side_t *wall, F3DFloor * ffloor) const;
	void CalcFracPos(side_t *wall, double x, double y);
	void Remove ();

	static void SpreadLeft (double r, vertex_t *v1, side_t *feelwall, F3DFloor *ffloor, SpreadInfo *spread);
	static void SpreadRight (double r, side_t *feelwall, double wallsize, F3DFloor *ffloor, SpreadInfo *spread);
};

class DImpactDecal : public DBaseDecal
{
	DECLARE_CLASS (DImpactDecal, DBaseDecal)
public:
	static const int DEFAULT_STAT = STAT_AUTODECAL;
	void Construct(double z = 0)
	{
		Super::Construct(z);
	}
	void Construct(side_t *wall, const FDecalTemplate *templ);

	static DImpactDecal *StaticCreate(FLevelLocals *Level, const char *name, const DVector3 &pos, side_t *wall, F3DFloor * ffloor, PalEntry color = 0, uint32_t bloodTranslation = 0);
	static DImpactDecal *StaticCreate(FLevelLocals *Level, const FDecalTemplate *tpl, const DVector3 &pos, side_t *wall, F3DFloor * ffloor, PalEntry color = 0, uint32_t bloodTranslation = 0);

	void BeginPlay ();
	void Expired() override;

protected:
	DBaseDecal *CloneSelf(const FDecalTemplate *tpl, double x, double y, double z, side_t *wall, F3DFloor * ffloor) const;
	void CheckMax ();
};

class DFlashFader : public DThinker
{
	DECLARE_CLASS (DFlashFader, DThinker)
	HAS_OBJECT_POINTERS
public:
	void Construct(float r1, float g1, float b1, float a1,
				 float r2, float g2, float b2, float a2,
				 float time, AActor *who, bool terminate = false);
	void OnDestroy() override;
	void Serialize(FSerializer &arc);
	void Tick ();
	AActor *WhoFor() { return ForWho; }
	void Cancel ();
	

protected:
	float Blends[2][4];
	int TotalTics;
	int RemainingTics;
	TObjPtr<AActor*> ForWho;
	bool Terminate;
	void SetBlend (float time);
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
	DVector3 Intensity;
	DVector3 RelIntensity;
	DVector3 Offset;
	DVector3 RelOffset;
	double RollIntensity, RollWave;
};

class DEarthquake : public DThinker
{
	DECLARE_CLASS (DEarthquake, DThinker)
	HAS_OBJECT_POINTERS
public:
	static const int DEFAULT_STAT = STAT_EARTHQUAKE;
	void Construct(AActor *center, int intensityX, int intensityY, int intensityZ, int duration,
		int damrad, int tremrad, FSoundID quakesfx, int flags, 
		double waveSpeedX, double waveSpeedY, double waveSpeedZ, int falloff, int highpoint, double rollIntensity, double rollWave);

	void Serialize(FSerializer &arc);
	void Tick ();
	TObjPtr<AActor*> m_Spot;
	double m_TremorRadius, m_DamageRadius;
	int m_Countdown;
	int m_CountdownStart;
	FSoundID m_QuakeSFX;
	int m_Flags;
	DVector3 m_Intensity;
	DVector3 m_WaveSpeed;
	double m_Falloff;
	int m_Highpoint, m_MiniCount;
	double m_RollIntensity, m_RollWave;

	double GetModIntensity(double intensity, bool fake = false) const;
	double GetModWave(double ticFrac, double waveMultiplier) const;
	double GetFalloff(double dist) const;

	static int StaticGetQuakeIntensities(double ticFrac, AActor *viewer, FQuakeJiggers &jiggers);
};

#endif //__A_SHAREDGLOBAL_H__
