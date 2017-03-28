#pragma once
#include "c_cvars.h"
#include "actor.h"
#include "cycler.h"

EXTERN_CVAR(Bool, gl_lights)
EXTERN_CVAR(Bool, gl_attachedlights)

class ADynamicLight;
class FSerializer;
class FLightDefaults;


enum
{
   LIGHT_RED = 0,
   LIGHT_GREEN = 1,
   LIGHT_BLUE = 2,
   LIGHT_INTENSITY = 3,
   LIGHT_SECONDARY_INTENSITY = 4,
   LIGHT_SCALE = 3,
};

// This is as good as something new
#define MF4_SUBTRACTIVE MF4_MISSILEEVENMORE
#define MF4_ADDITIVE MF4_MISSILEMORE
#define MF4_DONTLIGHTSELF MF4_SEESDAGGERS
#define MF4_ATTENUATE MF4_INCOMBAT
#define MF4_NOSHADOWMAP MF4_STANDSTILL
#define MF4_DONTLIGHTACTORS MF4_EXTREMEDEATH

enum ELightType
{
	PointLight,
	PulseLight,
	FlickerLight,
	RandomFlickerLight,
	SectorLight,
	SpotLight,
	ColorPulseLight,
	ColorFlickerLight, 
	RandomColorFlickerLight
};


struct FLightNode
{
	FLightNode ** prevTarget;
	FLightNode * nextTarget;
	FLightNode ** prevLight;
	FLightNode * nextLight;
	ADynamicLight * lightsource;
	union
	{
		side_t * targLine;
		subsector_t * targSubsector;
		void * targ;
	};
};


//
// Base class
//
// [CO] I merged everything together in this one class so that I don't have
// to create and re-create an excessive amount of objects
//

class ADynamicLight : public AActor
{
	friend class FLightDefaults;
	DECLARE_CLASS (ADynamicLight, AActor)
public:
	virtual void Tick();
	void Serialize(FSerializer &arc);
	void PostSerialize();
	uint8_t GetRed() const { return args[LIGHT_RED]; }
	uint8_t GetGreen() const { return args[LIGHT_GREEN]; }
	uint8_t GetBlue() const { return args[LIGHT_BLUE]; }
	float GetRadius() const { return (IsActive() ? m_currentRadius * 2.f : 0.f); }
	void LinkLight();
	void UnlinkLight();
	size_t PointerSubstitution (DObject *old, DObject *notOld);

	void BeginPlay();
	void SetOrigin (double x, double y, double z, bool moving = false);
	void PostBeginPlay();
	void OnDestroy() override;
	void Activate(AActor *activator);
	void Deactivate(AActor *activator);
	void SetOffset(const DVector3 &pos);
	void UpdateLocation();
	bool IsOwned() const { return owned; }
	bool IsActive() const { return !(flags2&MF2_DORMANT); }
	bool IsSubtractive() { return !!(flags4&MF4_SUBTRACTIVE); }
	bool IsAdditive() { return !!(flags4&MF4_ADDITIVE); }
	FState *targetState;
	FLightNode * touching_sides;
	FLightNode * touching_subsectors;
	FLightNode * touching_sector;

private:
	double DistToSeg(const DVector3 &pos, seg_t *seg);
	void CollectWithinRadius(const DVector3 &pos, subsector_t *subSec, float radius);

protected:
	DVector3 m_off;
	float m_currentRadius;
	unsigned int m_lastUpdate;
	FCycler m_cycler;
	subsector_t * subsector;

public:
	int m_tickCount;
	uint8_t lightflags;
	uint8_t lighttype;
	bool owned;
	bool halo;
	uint8_t color2[3];
	bool visibletoplayer;
	bool swapped;
	bool shadowmapped;
	int bufferindex;


};
