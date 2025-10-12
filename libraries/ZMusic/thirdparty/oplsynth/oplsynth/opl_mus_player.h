#pragma once
#include <mutex>
#include <vector>
#include <string>
#include "musicblock.h"

class OPLmusicBlock : public musicBlock
{
public:
	OPLmusicBlock(int core, int numchips);
	virtual ~OPLmusicBlock();

	bool ServiceStream(void *buff, int numbytes);
	void ResetChips(int numchips);

	virtual void Restart();

protected:
	virtual int PlayTick() = 0;
	void OffsetSamples(float *buff, int count);

	uint8_t *score;
	uint8_t *scoredata;
	double NextTickIn;
	double SamplesPerTick;
	double LastOffset;
	int playingcount;
	int NumChips;
	int currentCore;
	bool Looping;
	bool FullPan;
};

class OPLmusicFile : public OPLmusicBlock
{
public:
	OPLmusicFile(const void *data, size_t length, int core, int numchips, const char *&errormessage);
	virtual ~OPLmusicFile();

	bool IsValid() const;
	void SetLooping(bool loop);
	void Restart();

protected:
	OPLmusicFile(int core, int numchips) : OPLmusicBlock(core, numchips) {}
	int PlayTick();

	enum { RDosPlay, IMF, DosBox1, DosBox2 } RawPlayer;
	int ScoreLen;
	int WhichChip;
};
