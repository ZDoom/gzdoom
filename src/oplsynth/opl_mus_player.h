#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "muslib.h"

class OPLmusicBlock : protected musicBlock
{
public:
	OPLmusicBlock (int handle, int pos, int len, int rate, int maxSamples);
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

	CRITICAL_SECTION ChipAccess;
};
