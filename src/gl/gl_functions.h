#ifndef __GL_FUNCT
#define __GL_FUNCT

#include "v_palette.h"

class AActor;

void gl_PreprocessLevel();
void gl_CleanLevelData();
void gl_LinkLights();
void gl_SetActorLights(AActor *);

#endif
