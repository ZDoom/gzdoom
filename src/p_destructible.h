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
	SECPART_Ceiling = 1
};

void P_InitHealthGroups();

void P_SetHealthGroupHealth(int group, int health);

FHealthGroup* P_GetHealthGroup(int id);
FHealthGroup* P_GetHealthGroupOrNew(int id, int startinghealth);

void P_DamageSector(sector_t* sector, AActor* source, int damage, FName damagetype, int part, DVector3 position, bool dogroups = true);
void P_DamageLinedef(line_t* line, AActor* source, int damage, FName damagetype, int side, DVector3 position, bool dogroups = true);

void P_GeometryLineAttack(FTraceResults& trace, AActor* thing, int damage, FName damageType);
void P_GeometryRadiusAttack(AActor* bombspot, AActor* bombsource, int bombdamage, int bombdistance, FName damagetype, int fulldamagedistance);

bool P_CheckLinedefVulnerable(line_t* line, int side, int part = -1);
bool P_CheckSectorVulnerable(sector_t* sector, int part);