#pragma once

#include "tarray.h"
#include "r_defs.h"
#include "p_trace.h"

// [ZZ] Destructible geometry related
struct FHealthGroup
{
	TArray<sector_t*> sectors;
	TArray<line_t*> lines;
	int health;
	int id;
};

// for P_DamageSector
enum
{
	SECPART_Floor = 0,
	SECPART_Ceiling = 1,
	SECPART_3D = 2
};

struct FLevelLocals;
void P_InitHealthGroups(FLevelLocals *Level);

void P_SetHealthGroupHealth(FHealthGroup* group, int health);
void P_SetHealthGroupHealth(FLevelLocals *Level, int group, int health);

FHealthGroup* P_GetHealthGroup(FLevelLocals *Level, int id);
FHealthGroup* P_GetHealthGroupOrNew(FLevelLocals *Level, int id, int startinghealth);

void P_DamageSector(sector_t* sector, AActor* source, int damage, FName damagetype, int part, DVector3 position, bool isradius, bool dogroups);
void P_DamageLinedef(line_t* line, AActor* source, int damage, FName damagetype, int side, DVector3 position, bool isradius, bool dogroups);

void P_GeometryLineAttack(FTraceResults& trace, AActor* thing, int damage, FName damageType);
void P_GeometryRadiusAttack(AActor* bombspot, AActor* bombsource, int bombdamage, double bombdistance, FName damagetype, double fulldamagedistance);
bool P_ProjectileHitLinedef(AActor* projectile, line_t* line);
bool P_ProjectileHitPlane(AActor* projectile, int part);

bool P_CheckLinedefVulnerable(line_t* line, int side, int part = -1);
bool P_CheckSectorVulnerable(sector_t* sector, int part);

void P_SerializeHealthGroups(FLevelLocals *Level, FSerializer& arc);