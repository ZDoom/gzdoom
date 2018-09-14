#ifndef __GL_CLOCK_H
#define __GL_CLOCK_H

#include "stats.h"
#include "x86.h"
#include "m_fixed.h"

extern glcycle_t RenderWall,SetupWall,ClipWall;
extern glcycle_t RenderFlat,SetupFlat;
extern glcycle_t RenderSprite,SetupSprite;
extern glcycle_t All, Finish, PortalAll, Bsp;
extern glcycle_t ProcessAll, PostProcess;
extern glcycle_t RenderAll;
extern glcycle_t Dirty;
extern glcycle_t drawcalls, twoD, Flush3D;

extern int iter_dlightf, iter_dlight, draw_dlight, draw_dlightf;
extern int rendered_lines,rendered_flats,rendered_sprites,rendered_decals,render_vertexsplit,render_texsplit;
extern int rendered_portals;

extern int vertexcount, flatvertices, flatprimitives;

void ResetProfilingData();
void CheckBench();
void  checkBenchActive();


#endif
