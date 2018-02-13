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

enum LightFlag
{
	LF_SUBTRACTIVE = 1,
	LF_ADDITIVE = 2,
	LF_DONTLIGHTSELF = 4,
	LF_ATTENUATE = 8,
	LF_NOSHADOWMAP = 16,
	LF_DONTLIGHTACTORS = 32,
	LF_SPOT = 64
};

typedef TFlags<LightFlag> LightFlags;
DEFINE_TFLAGS_OPERATORS(LightFlags)


enum ELightType
{
	PointLight,
	PulseLight,
	FlickerLight,
	RandomFlickerLight,
	SectorLight,
	DummyLight,
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
	DECLARE_CLASS(ADynamicLight, AActor)
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
	size_t PointerSubstitution(DObject *old, DObject *notOld);

	void BeginPlay();
	void SetOrigin(double x, double y, double z, bool moving = false);
	void PostBeginPlay();
	void OnDestroy() override;
	void Activate(AActor *activator);
	void Deactivate(AActor *activator);
	void SetOffset(const DVector3 &pos);
	void UpdateLocation();
	bool IsOwned() const { return owned; }
	bool IsActive() const { return !(flags2&MF2_DORMANT); }
	bool IsSubtractive() { return !!(lightflags & LF_SUBTRACTIVE); }
	bool IsAdditive() { return !!(lightflags & LF_ADDITIVE); }
	bool IsSpot() { return !!(lightflags & LF_SPOT); }
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
	uint8_t lighttype;
	bool owned;
	bool halo;
	uint8_t color2[3];
	bool visibletoplayer;
	bool swapped;
	bool shadowmapped;
	int bufferindex;
	LightFlags lightflags;
	DAngle SpotInnerAngle = 10.0;
	DAngle SpotOuterAngle = 25.0;

};
