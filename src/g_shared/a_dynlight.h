#pragma once
#include "c_cvars.h"
#include "actor.h"
#include "cycler.h"

EXTERN_CVAR(Bool, gl_lights)
EXTERN_CVAR(Bool, gl_attachedlights)

struct side_t;
struct seg_t;

class ADynamicLight;
class FSerializer;

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

//==========================================================================
//
// Light definitions
//
//==========================================================================
class FLightDefaults
{
public:
	FLightDefaults(FName name, ELightType type);

	void ApplyProperties(ADynamicLight * light) const;
	FName GetName() const { return m_Name; }
	void SetParameter(double p) { m_Param = p; }
	void SetArg(int arg, int val) { m_Args[arg] = val; }
	int GetArg(int arg) { return m_Args[arg]; }
	uint8_t GetAttenuate() const { return m_attenuate; }
	void SetOffset(float* ft) { m_Pos.X = ft[0]; m_Pos.Z = ft[1]; m_Pos.Y = ft[2]; }
	void SetSubtractive(bool subtract) { m_subtractive = subtract; }
	void SetAdditive(bool add) { m_additive = add; }
	void SetDontLightSelf(bool add) { m_dontlightself = add; }
	void SetAttenuate(bool on) { m_attenuate = on; }
	void SetHalo(bool halo) { m_halo = halo; }
	void SetDontLightActors(bool on) { m_dontlightactors = on; }
	void SetSpot(bool spot) { m_spot = spot; }
	void SetSpotInnerAngle(double angle) { m_spotInnerAngle = angle; }
	void SetSpotOuterAngle(double angle) { m_spotOuterAngle = angle; }

	void OrderIntensities()
	{
		if (m_Args[LIGHT_INTENSITY] > m_Args[LIGHT_SECONDARY_INTENSITY])
		{
			std::swap(m_Args[LIGHT_INTENSITY], m_Args[LIGHT_SECONDARY_INTENSITY]);
			m_swapped = true;
		}
	}

protected:
	FName m_Name;
	int m_Args[5] = { 0,0,0,0,0 };
	double m_Param = 0;
	DVector3 m_Pos = { 0,0,0 };
	ELightType m_type;
	int8_t m_attenuate = -1;
	bool m_subtractive = false;
	bool m_additive = false;
	bool m_halo = false;
	bool m_dontlightself = false;
	bool m_dontlightactors = false;
	bool m_swapped = false;
	bool m_spot = false;
	double m_spotInnerAngle = 10.0;
	double m_spotOuterAngle = 25.0;
};

//==========================================================================
//
// Light associations (intermediate parser data)
//
//==========================================================================

class FLightAssociation
{
public:
	//FLightAssociation();
	FLightAssociation(FName actorName, const char *frameName, FName lightName)
		: m_ActorName(actorName), m_AssocLight(lightName)
	{
		strncpy(m_FrameName, frameName, 8);
	}

	FName ActorName() { return m_ActorName; }
	const char *FrameName() { return m_FrameName; }
	FName Light() { return m_AssocLight; }
	void ReplaceLightName(FName newName) { m_AssocLight = newName; }
protected:
	char m_FrameName[8];
	FName m_ActorName, m_AssocLight;
};


//==========================================================================
//
// Light associations per actor class
//
//==========================================================================

class FInternalLightAssociation
{
public:
	FInternalLightAssociation(FLightAssociation * asso);
	int Sprite() const { return m_sprite; }
	int Frame() const { return m_frame; }
	const FLightDefaults *Light() const { return m_AssocLight; }
protected:
	int m_sprite;
	int m_frame;
	FLightDefaults * m_AssocLight;
};



typedef TFlags<LightFlag> LightFlags;
DEFINE_TFLAGS_OPERATORS(LightFlags)


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
	DAngle SpotInnerAngle;
	DAngle SpotOuterAngle;
    
    int mShadowmapIndex;

};
