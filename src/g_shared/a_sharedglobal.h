#ifndef __A_SHAREDGLOBAL_H__
#define __A_SHAREDGLOBAL_H__

#include "info.h"
#include "actor.h"

class FDecalTemplate;
struct vertex_t;
struct side_t;
struct F3DFloor;
class DBaseDecal;

class DBaseDecal *ShootDecal(const FDecalTemplate *tpl, AActor *basisactor, sector_t *sec, double x, double y, double z, DAngle angle, double tracedist, bool permanent);
void SprayDecal(AActor *shooter, const char *name,double distance = 172.);

class DBaseDecal : public DThinker
{
	DECLARE_CLASS (DBaseDecal, DThinker)
	HAS_OBJECT_POINTERS
public:
	DBaseDecal ();
	DBaseDecal(double z);
	DBaseDecal(int statnum, double z);
	DBaseDecal (const AActor *actor);
	DBaseDecal (const DBaseDecal *basis);

	void Serialize(FSerializer &arc);
	void OnDestroy() override;
	FTextureID StickToWall(side_t *wall, double x, double y, F3DFloor * ffloor);
	double GetRealZ (const side_t *wall) const;
	void SetShade (uint32_t rgb);
	void SetShade (int r, int g, int b);
	void Spread (const FDecalTemplate *tpl, side_t *wall, double x, double y, double z, F3DFloor * ffloor);
	void GetXY (side_t *side, double &x, double &y) const;

	DBaseDecal *WallNext, *WallPrev;

	double LeftDistance;
	double Z;
	double ScaleX, ScaleY;
	double Alpha;
	uint32_t AlphaColor;
	int Translation;
	FTextureID PicNum;
	uint32_t RenderFlags;
	FRenderStyle RenderStyle;
	side_t *Side;
	sector_t *Sector;

protected:
	virtual DBaseDecal *CloneSelf(const FDecalTemplate *tpl, double x, double y, double z, side_t *wall, F3DFloor * ffloor) const;
	void CalcFracPos(side_t *wall, double x, double y);
	void Remove ();

	static void SpreadLeft (double r, vertex_t *v1, side_t *feelwall, F3DFloor *ffloor);
	static void SpreadRight (double r, side_t *feelwall, double wallsize, F3DFloor *ffloor);
};

class DImpactDecal : public DBaseDecal
{
	DECLARE_CLASS (DImpactDecal, DBaseDecal)
public:
	DImpactDecal(double z);
	DImpactDecal (side_t *wall, const FDecalTemplate *templ);

	static DImpactDecal *StaticCreate(const char *name, const DVector3 &pos, side_t *wall, F3DFloor * ffloor, PalEntry color = 0);
	static DImpactDecal *StaticCreate(const FDecalTemplate *tpl, const DVector3 &pos, side_t *wall, F3DFloor * ffloor, PalEntry color = 0);

	void BeginPlay ();
	void OnDestroy() override;

protected:
	DBaseDecal *CloneSelf(const FDecalTemplate *tpl, double x, double y, double z, side_t *wall, F3DFloor * ffloor) const;
	static void CheckMax ();

private:
	DImpactDecal();
};

class DFlashFader : public DThinker
{
	DECLARE_CLASS (DFlashFader, DThinker)
	HAS_OBJECT_POINTERS
public:
	DFlashFader (float r1, float g1, float b1, float a1,
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
	int StartTic;
	TObjPtr<AActor*> ForWho;
	bool Terminate;
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
	DEarthquake(AActor *center, int intensityX, int intensityY, int intensityZ, int duration,
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

private:
	DEarthquake ();
};

#endif //__A_SHAREDGLOBAL_H__
