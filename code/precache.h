#ifndef __PRECACHE_H__
#define __PRECACHE_H__

#include "doomtype.h"
#include <stdio.h>

typedef struct {
	FILE	*file;
	int		start;
	int		end;
} DFILE;

struct cacheentry_s {
	struct	cacheentry_s *left, *right;
	char	*path;
	DFILE	file;
	byte	*data;
	int		usefulness;
};
typedef struct cacheentry_s cacheentry_t;

#define MAX_SOUNDS		256
#define MAX_MODELS		256

void FlushCaches (void);
void FinishCaches (void);
int soundindex (char *path);
int modelindex (char *path);

#endif //__PRECACHE_H__