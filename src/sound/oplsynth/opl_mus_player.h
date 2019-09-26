#include <mutex>
#include "musicblock.h"

class FileReader;

class OPLmusicBlock : public musicBlock
{
public:
	OPLmusicBlock(int core);
	virtual ~OPLmusicBlock();

	bool ServiceStream(void *buff, int numbytes);
	void ResetChips();

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

	std::mutex ChipAccess;
};

class OPLmusicFile : public OPLmusicBlock
{
public:
	OPLmusicFile(FileReader &reader, int core);
	OPLmusicFile(int core, const OPLmusicFile *source, const char *filename);
	virtual ~OPLmusicFile();

	bool IsValid() const;
	void SetLooping(bool loop);
	void Restart();
	void Dump();

protected:
	OPLmusicFile(int core) : OPLmusicBlock(core) {}
	int PlayTick();

	enum { RDosPlay, IMF, DosBox1, DosBox2 } RawPlayer;
	int ScoreLen;
	int WhichChip;
};
