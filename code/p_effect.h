#include "vectors.h"

#define FX_ROCKET			0x00000001
#define FX_GRENADE			0x00000002

#define FX_FOUNTAINMASK		0x00070000
#define FX_FOUNTAINSHIFT	16
#define FX_REDFOUNTAIN		0x00010000
#define FX_GREENFOUNTAIN	0x00020000
#define FX_BLUEFOUNTAIN		0x00030000
#define FX_YELLOWFOUNTAIN	0x00040000
#define FX_PURPLEFOUNTAIN	0x00050000
#define FX_BLACKFOUNTAIN	0x00060000
#define FX_WHITEFOUNTAIN	0x00070000

struct particle_s;

struct particle_s *JitterParticle (int ttl);

void P_ThinkParticles (void);
void P_InitEffects (void);
void P_RunEffects (void);

void P_RunEffect (mobj_t *actor, int effects);

void P_DrawRailTrail (vec3_t start, vec3_t end);
void P_DrawSplash (int count, fixed_t x, fixed_t y, fixed_t z, angle_t angle, int kind);
void P_DrawSplash2 (int count, fixed_t x, fixed_t y, fixed_t z, angle_t angle, int updown, int kind);
void P_DisconnectEffect (mobj_t *actor);