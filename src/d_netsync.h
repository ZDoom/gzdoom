#ifndef __D_NETSYNC_H__
#define __D_NETSYNC_H__

struct NetSyncData {
	DVector3		Pos;
	DVector3		Vel;
	DAngle			SpriteAngle;
	DAngle			SpriteRotation;
	DRotator		Angles;
	DVector2		Scale;				// Scaling values; 1 is normal size
	double			Alpha;				// Since P_CheckSight makes an alpha check this can't be a float. It has to be a double.
	int				sprite;				// used to find patch_t and flip value
	uint8_t			frame;				// sprite frame to draw
	uint8_t			effects;			// [RH] see p_effect.h
	FRenderStyle	RenderStyle;		// Style to draw this actor with
	uint32_t		Translation;
	uint32_t		RenderRequired;		// current renderer must have this feature set
	uint32_t		RenderHidden;		// current renderer must *not* have any of these features
	uint32_t		renderflags;		// Different rendering flags
	double			Floorclip;		// value to use for floor clipping
	DAngle			VisibleStartAngle;
	DAngle			VisibleStartPitch;
	DAngle			VisibleEndAngle;
	DAngle			VisibleEndPitch;
	double			Speed;
	double			FloatSpeed;
	double			CameraHeight;	// Height of camera when used as such
	double			CameraFOV;
	double			StealthAlpha;	// Minmum alpha for MF_STEALTH.

};

#endif //__D_NETSYNC_H__