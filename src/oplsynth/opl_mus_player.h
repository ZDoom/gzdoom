#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <SDL.h>
#endif

#include "muslib.h"
#include "files.h"

class OPLmusicBlock : protected musicBlock
{
public:
	OPLmusicBlock (FILE *file, int len, int rate, int maxSamples);
	~OPLmusicBlock ();
	bool IsValid () const;

	bool ServiceStream (void *buff, int numbytes);
	void Restart ();
	void SetLooping (bool loop);
	void ResetChips ();

protected:
	int SampleRate;
	int NextTickIn;
	int SamplesPerTick;
	bool TwoChips;
	bool Looping;

	int *SampleBuff;

#ifdef _WIN32
	CRITICAL_SECTION ChipAccess;
#else
	SDL_mutex *ChipAccess;
#endif
};
