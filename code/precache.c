#include "precache.h"
#include "cmdlib.h"

#include <malloc.h>
#include <stdlib.h>

cacheentry_t soundcache[MAX_SOUNDS];
cacheentry_t modelcache[MAX_MODELS];

cacheentry_t *soundtop, *modeltop;

static boolean cachefilling;
static int lastsound;
static int lastmodel;

void dclose (DFILE *file)
{
	if (file->file) {
		fclose (file->file);
		file->file = NULL;
	}
	file->start = file->end = 0;
}

void FlushCaches (void)
{
	int i;

	for (i = 0; i < MAX_SOUNDS; i++) {
		if (soundcache[i].path) {
			free (soundcache[i].path);
			dclose (&soundcache[i].file);
			soundcache[i].left = soundcache[i].right = NULL;
		}
	}

	for (i = 0; i < MAX_MODELS; i++) {
		if (modelcache[i].path) {
			free (modelcache[i].path);
			dclose (&modelcache[i].file);
			modelcache[i].left = modelcache[i].right = NULL;
		}
	}

	cachefilling = true;
	lastsound = lastmodel = 0;
	modeltop = soundtop = NULL;
}

void FinishCaches (void)
{
	cachefilling = false;
}

static int cacheindex (char *path, cacheentry_t *cache, int *lastindex, int maxindex, cacheentry_t **top)
{
	cacheentry_t *entry = *top, *last = NULL;
	int compareval;

	while (entry) {
		last = entry;
		compareval = stricmp (entry->path, path);
		if (compareval == 0) {
			break;
		} else if (compareval < 0) {
			entry = entry->left;
		} else {
			entry = entry->right;
		}
	}

	if (entry) {
		return entry - cache;
	} else if (!cachefilling || *lastindex == maxindex - 1) {
		return 0;
	} else {
		*lastindex += 1;
		cache[*lastindex].path = copystring (path);
		if (last) {
			if (compareval < 0) {
				last->left = &cache[*lastindex];
			} else {
				last->right = &cache[*lastindex];
			}
		} else {
			*top = &cache[*lastindex];
		}
	}
}

int modelindex (char *path)
{
	return cacheindex (path, modelcache, &lastmodel, MAX_MODELS, &modeltop);
}

int soundindex (char *path)
{
	return cacheindex (path, soundcache, &lastsound, MAX_SOUNDS, &soundtop);
}