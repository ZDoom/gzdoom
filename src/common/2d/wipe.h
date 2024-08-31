#ifndef __F_WIPE_H__
#define __F_WIPE_H__

#include "stdint.h"
#include <functional>

class FTexture;

enum
{
	wipe_None,			// don't bother
	wipe_Melt,			// weird screen melt
	wipe_Burn,			// fade in shape of fire
	wipe_Fade,			// crossfade from old to new
	wipe_NUMWIPES
};

void PerformWipe(FTexture* startimg, FTexture* endimg, int wipe_type, bool stopsound, std::function<void()> overlaydrawer);


#endif
