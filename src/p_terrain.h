#ifndef __P_TERRAIN_H__
#define __P_TERRAIN_H__

#include "dobject.h"
#include "m_fixed.h"

extern byte *TerrainTypes;

// at game start
void P_InitTerrainTypes ();

struct FSplashDef
{
	const char *Name;
	int SmallSplashSound;
	int NormalSplashSound;
	const TypeInfo *SmallSplash;
	const TypeInfo *SplashBase;
	const TypeInfo *SplashChunk;
	byte ChunkXVelShift;
	byte ChunkYVelShift;
	byte ChunkZVelShift;
	fixed_t ChunkBaseZVel;
	fixed_t SmallSplashClip;
};

struct FTerrainDef
{
	const char *Name;
	int Splash;
	int DamageAmount;
	int DamageMOD;
	int DamageFlags;
	int DamageTimeMask;
	fixed_t FootClip;
	float StepVolume;
	int WalkStepTics;
	int RunStepTics;
	int LeftStepSounds[4];
	int RightStepSounds[4];
	byte NumLeftStepSounds;
	byte NumRightStepSounds;
	bool IsLiquid;
	bool ReducedFriction;
};

extern TArray<FSplashDef> Splashes;
extern TArray<FTerrainDef> Terrains;

#endif //__P_TERRAIN_H__
