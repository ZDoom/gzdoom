/*
** p_effect.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#pragma once

#include "vectors.h"

#define FX_ROCKET			0x00000001
#define FX_GRENADE			0x00000002
#define FX_RESPAWNINVUL		0x00000020
#define FX_VISIBILITYPULSE	0x00000040

struct subsector_t;

// [RH] Particle details

struct particle_t
{
	DVector3 Pos;
	DVector3 Vel;
	DVector3 Acc;
	double	size;
	double	sizestep;
	subsector_t * subsector;
	int32_t	ttl;
	uint8_t	bright;
	bool	notimefreeze;
	float	fadestep;
	float	alpha;
	int		color;
	uint16_t	tnext;
	uint16_t	snext;
};

extern particle_t *Particles;
extern TArray<uint16_t>		ParticlesInSubsec;

const uint16_t NO_PARTICLE = 0xffff;

void P_ClearParticles ();
void P_FindParticleSubsectors ();


class AActor;

particle_t *JitterParticle (int ttl);
particle_t *JitterParticle (int ttl, double drift);

void P_ThinkParticles (void);
void P_SpawnParticle(const DVector3 &pos, const DVector3 &vel, const DVector3 &accel, PalEntry color, double startalpha, int lifetime, double size, double fadestep, double sizestep, int flags = 0);
void P_InitEffects (void);
void P_RunEffects (void);

void P_RunEffect (AActor *actor, int effects);

struct SPortalHit
{
	DVector3 HitPos;
	DVector3 ContPos;
	DVector3 OutDir;
};

void P_DrawRailTrail(AActor *source, TArray<SPortalHit> &portalhits, int color1, int color2, double maxdiff = 0, int flags = 0, PClassActor *spawnclass = NULL, DAngle angle = 0., int duration = 35, double sparsity = 1.0, double drift = 1.0, int SpiralOffset = 270, DAngle pitch = 0.);
void P_DrawSplash (int count, const DVector3 &pos, DAngle angle, int kind);
void P_DrawSplash2 (int count, const DVector3 &pos, DAngle angle, int updown, int kind);
void P_DisconnectEffect (AActor *actor);
