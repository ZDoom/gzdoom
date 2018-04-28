#ifndef __GL_WALL_H
#define __GL_WALL_H
//==========================================================================
//
// One wall segment in the draw list
//
//==========================================================================
#include "r_defs.h"
#include "r_data/renderstyle.h"
#include "textures/textures.h"
#include "r_data/colormaps.h"
#include "hwrenderer/scene/hw_drawstructs.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

struct particle_t;

// Light + color

void gl_SetDynSpriteLight(AActor *self, float x, float y, float z, subsector_t *subsec);
void gl_SetDynSpriteLight(AActor *actor, particle_t *particle);
int gl_SetDynModelLight(AActor *self, int dynlightindex);

#endif
