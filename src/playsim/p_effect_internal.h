#pragma once

#include "palettecontainer.h"
#include "hwrenderer/scene/hw_drawstructs.h"


//===========================================================================
// 
// VisualThinkers
// by Major Cooke
// Credit to phantombeta, RicardoLuis0 & RaveYard for aid
// 
//===========================================================================

class DVisualThinker : public DThinker
{
	DECLARE_CLASS(DVisualThinker, DThinker);
public:
	DVector3		Prev;
	DVector2		Scale,
					Offset;
	float			PrevRoll;
	int16_t			LightLevel;
	FTranslationID	Translation;
	FTextureID		AnimatedTexture;
	sector_t		*cursector;

	bool			bFlipOffsetX,
					bFlipOffsetY,
					bXFlip,
					bYFlip,				// flip the sprite on the x/y axis.
					bDontInterpolate,	// disable all interpolation
					bAddLightLevel;		// adds sector light level to 'LightLevel'

	// internal only variables
	particle_t		PT;
	HWSprite		spr; //in an effort to cache the result. 

	DVisualThinker();
	void Construct();
	void OnDestroy() override;

	static DVisualThinker* NewVisualThinker(FLevelLocals* Level, PClass* type);
	void SetTranslation(FName trname);
	int GetRenderStyle();
	bool isFrozen();
	int GetLightLevel(sector_t *rendersector) const;
	FVector3 InterpolatedPosition(double ticFrac) const;
	float InterpolatedRoll(double ticFrac) const;

	void Tick() override;
	void UpdateSpriteInfo();
	void Serialize(FSerializer& arc) override;

	float GetOffset(bool y) const;
};