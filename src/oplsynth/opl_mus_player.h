#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define USE_WINDOWS_DWORD
#else
#include <SDL.h>
#endif

#include "muslib.h"
#include "files.h"

#define OPL_SAMPLE_RATE			49716.0

class OPLmusicBlock : public musicBlock
{
public:
	OPLmusicBlock();
	~OPLmusicBlock();

	bool ServiceStream(void *buff, int numbytes);
	void ResetChips();

	virtual void Restart();

protected:
	virtual int PlayTick() = 0;

	void Serialize();
	void Unserialize();

	double NextTickIn;
	double SamplesPerTick;
	bool TwoChips;
	bool Looping;

#ifdef _WIN32
	CRITICAL_SECTION ChipAccess;
#else
	SDL_mutex *ChipAccess;
#endif
};

class OPLmusicFile : public OPLmusicBlock
{
public:
	OPLmusicFile(FILE *file, char *musiccache, int len, int maxSamples);
	~OPLmusicFile();

	bool IsValid() const;
	void SetLooping(bool loop);
	void Restart();

protected:
	int PlayTick();

	enum { NotRaw, RDosPlay, IMF } RawPlayer;
	int ScoreLen;
};
