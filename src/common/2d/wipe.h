#ifndef __F_WIPE_H__
#define __F_WIPE_H__

#include "stdint.h"
#include <functional>

class FTexture;
int wipe_CalcBurn(uint8_t *buffer, int width, int height, int density);

enum
{
	wipe_None,			// don't bother
	wipe_Melt,			// weird screen melt
	wipe_Burn,			// fade in shape of fire
	wipe_Fade,			// crossfade from old to new
	wipe_NUMWIPES
};

class Wiper
{
protected:
	FGameTexture *startScreen = nullptr, *endScreen = nullptr;
public:
	virtual ~Wiper();
	virtual bool Run(int ticks) = 0;
	virtual void SetTextures(FGameTexture *startscreen, FGameTexture *endscreen)
	{
		startScreen = startscreen;
		endScreen = endscreen;
	}
	
	static Wiper *Create(int type);
};

void PerformWipe(FTexture* startimg, FTexture* endimg, int wipe_type, bool stopsound, std::function<void()> overlaydrawer);


#endif
