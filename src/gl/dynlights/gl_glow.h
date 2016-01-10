
#ifndef __GL_GLOW
#define __GL_GLOW

struct sector_t;

void gl_InitGlow(const char * lumpnm);
int gl_CheckSpriteGlow(sector_t *sec, int lightlevel, int x, int y, int z);

#endif
