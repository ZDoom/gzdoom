#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define USE_WINDOWS_DWORD
#else
#include <SDL.h>
#endif

#include "muslib.h"
#include "files.h"

class OPLmusicBlock : public musicBlock
{
public:
	OPLmusicBlock (FILE *file, char * musiccache, int len, int rate, int maxSamples);
	~OPLmusicBlock ();
	bool IsValid () const;

	bool ServiceStream (void *buff, int numbytes);
	void Restart ();
	void SetLooping (bool loop);
	void ResetChips ();
	int PlayTick ();

protected:
	int SampleRate;
	int NextTickIn;
	int SamplesPerTick;
	bool TwoChips;
	bool Looping;
	enum { NotRaw, RDosPlay, IMF } RawPlayer;
	int ScoreLen;

	int *SampleBuff;

#ifdef _WIN32
	CRITICAL_SECTION ChipAccess;
#else
	SDL_mutex *ChipAccess;
#endif
};
