#pragma once

#include "vectors.h"

class DPSprite;
class player_t;
class AActor;
enum area_t : int;

struct WeaponPosition
{
	float wx, wy;
	float bobx, boby;
	DPSprite *weapon;
};

struct WeaponLighting
{
	FColormap cm;
	int lightlevel;
	bool isbelow;
};

struct WeaponRenderStyle
{
	FRenderStyle RenderStyle;
	float alpha;
	int OverrideShader;
};

struct WeaponRect
{
	FMaterial *tex;
	float x1, y1;
	float x2, y2;
	float u1, v1;
	float u2, v2;
};



bool isBright(DPSprite *psp);
WeaponPosition GetWeaponPosition(player_t *player);
FVector2 BobWeapon(WeaponPosition &weap, DPSprite *psp);
WeaponLighting GetWeaponLighting(sector_t *viewsector, const DVector3 &pos, int FixedColormap, area_t in_area, const DVector3 &playerpos );
WeaponRenderStyle GetWeaponRenderStyle(DPSprite *psp, AActor *playermo);
bool GetWeaponRect(DPSprite *psp, float sx, float sy, player_t *player, WeaponRect &rc);
