// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#ifndef __GLC_DYNLIGHT_H
#define __GLC_DYNLIGHT_H

#include "c_cvars.h"
#include "gl/utility/gl_geometric.h"
#include "gl/utility/gl_cycler.h"
#include "actor.h"


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

// This is as good as something new - and it can be set directly in the ActorInfo!
#define MF4_SUBTRACTIVE MF4_MISSILEEVENMORE
#define MF4_ADDITIVE MF4_MISSILEMORE
#define MF4_DONTLIGHTSELF MF4_SEESDAGGERS
#define MF4_ATTENUATE MF4_INCOMBAT

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
	int bufferindex;


};

enum
{
	STAT_DLIGHT=64
};

struct FDynLightData
{
	TArray<float> arrays[3];

	void Clear()
	{
		arrays[0].Clear();
		arrays[1].Clear();
		arrays[2].Clear();
	}

	void Combine(int *siz, int max)
	{
		siz[0] = arrays[0].Size();
		siz[1] = siz[0] + arrays[1].Size();
		siz[2] = siz[1] + arrays[2].Size();
		arrays[0].Resize(arrays[0].Size() + arrays[1].Size() + arrays[2].Size());
		memcpy(&arrays[0][siz[0]], &arrays[1][0], arrays[1].Size() * sizeof(float));
		memcpy(&arrays[0][siz[1]], &arrays[2][0], arrays[2].Size() * sizeof(float));
		siz[0]>>=2;
		siz[1]>>=2;
		siz[2]>>=2;
		if (siz[0] > max) siz[0] = max;
		if (siz[1] > max) siz[1] = max;
		if (siz[2] > max) siz[2] = max;
	}
};



bool gl_GetLight(int group, Plane & p, ADynamicLight * light, bool checkside, FDynLightData &data);
void gl_UploadLights(FDynLightData &data);


#endif
