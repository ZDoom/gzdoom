#pragma once

#include "vectors.h"

class DPSprite;
class player_t;
class AActor;
enum area_t : int;
struct FSpriteModelFrame;
struct HWDrawInfo;
class FGameTexture;


struct WeaponPosition2D
{
	float wx, wy;
	float bobx, boby;
	DPSprite *weapon;
};

struct WeaponPosition3D
{
	float wx,
		  wy;

	FVector3 translation,
			 rotation,
			 pivot;

	DPSprite *weapon;
};

struct WeaponLighting
{
	FColormap cm;
	int lightlevel;
	bool isbelow;
};

struct HUDSprite
{
	AActor *owner;
	DPSprite *weapon;
	FGameTexture *texture;
	FSpriteModelFrame *mframe;

	FColormap cm;
	int lightlevel;
	PalEntry ObjectColor;

	FRenderStyle RenderStyle;
	float alpha;
	int OverrideShader;

	float mx, my;
	float dynrgb[3];

	FVector3 rotation, translation, pivot;

	int lightindex;

	void SetBright(bool isbelow);
	bool GetWeaponRenderStyle(DPSprite *psp, AActor *playermo, sector_t *viewsector, WeaponLighting &light);
	bool GetWeaponRect(HWDrawInfo *di, DPSprite *psp, float sx, float sy, player_t *player, double ticfrac);

};
