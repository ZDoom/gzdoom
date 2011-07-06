// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		Rendering of moving objects, sprites.
//
//-----------------------------------------------------------------------------


#ifndef __R_THINGS__
#define __R_THINGS__

// [RH] Particle details
struct particle_t
{
	fixed_t	x,y,z;
	fixed_t velx,vely,velz;
	fixed_t accx,accy,accz;
	BYTE	ttl;
	BYTE	trans;
	BYTE	size;
	BYTE	fade;
	int		color;
	WORD	tnext;
	WORD	snext;
	subsector_t * subsector;
};

extern WORD	NumParticles;
extern WORD	ActiveParticles;
extern WORD	InactiveParticles;
extern particle_t *Particles;

const WORD NO_PARTICLE = 0xffff;

inline particle_t *NewParticle (void)
{
	particle_t *result = NULL;
	if (InactiveParticles != NO_PARTICLE)
	{
		result = Particles + InactiveParticles;
		InactiveParticles = result->tnext;
		result->tnext = ActiveParticles;
		ActiveParticles = WORD(result - Particles);
	}
	return result;
}

void R_InitParticles ();
void R_DeinitParticles ();
void R_ClearParticles ();
void R_DrawParticle (vissprite_t *);
void R_ProjectParticle (particle_t *, const sector_t *sector, int shade, int fakeside);
void R_FindParticleSubsectors ();

extern TArray<WORD>		ParticlesInSubsec;

extern int MaxVisSprites;

extern vissprite_t		**vissprites, **firstvissprite;
extern vissprite_t		**vissprite_p;

// Constant arrays used for psprite clipping
//	and initializing clipping.
extern short			zeroarray[MAXWIDTH];
extern short			screenheightarray[MAXWIDTH];

// vars for R_DrawMaskedColumn
extern short*			mfloorclip;
extern short*			mceilingclip;
extern fixed_t			spryscale;
extern fixed_t			sprtopscreen;
extern bool				sprflipvert;

extern fixed_t			pspritexscale;
extern fixed_t			pspriteyscale;
extern fixed_t			pspritexiscale;


void R_DrawMaskedColumn (const BYTE *column, const FTexture::Span *spans);


void R_CacheSprite (spritedef_t *sprite);
void R_SortVisSprites (int (STACK_ARGS *compare)(const void *, const void *), size_t first);
void R_AddSprites (sector_t *sec, int lightlevel, int fakeside);
void R_AddPSprites ();
void R_DrawSprites ();
void R_InitSprites ();
void R_DeinitSprites ();
void R_ClearSprites ();
void R_DrawMasked ();
void R_DrawRemainingPlayerSprites ();

void R_CheckOffscreenBuffer(int width, int height, bool spansonly);

enum { DVF_OFFSCREEN = 1, DVF_SPANSONLY = 2 };

void R_DrawVoxel(fixed_t dasprx, fixed_t daspry, fixed_t dasprz, angle_t dasprang,
	fixed_t daxscale, fixed_t dayscale, FVoxel *voxobj,
	lighttable_t *colormap, short *daumost, short *dadmost, int minslabz, int maxslabz, int flags);

void R_ClipVisSprite (vissprite_t *vis, int xl, int xh);


#endif
