#include "files.h"
#include "doomerrors.h"

class FZipExploder
{
	unsigned int Hold, Bits;
	FileReader *In;
	unsigned int InLeft;

	/****************************************************************
	   Shannon-Fano tree structures, variables and related routines
	 ****************************************************************/

	enum { kNumBitsInLongestCode = 16 };

	struct DecoderBase
	{
		unsigned int Limits[kNumBitsInLongestCode + 2];		// Limits[i] = value limit for symbols with length = i
		unsigned char Positions[kNumBitsInLongestCode + 2];	// Positions[i] = index in Symbols[] of first symbol with length = i
		unsigned char Symbols[1];

		bool SetCodeLengths(const unsigned char *codeLengths, const int kNumSymbols);
	};

	template<int kNumSymbols>
	struct Decoder : DecoderBase
	{
		unsigned char RestOfSymbols[kNumSymbols];
	};

	Decoder<256> LiteralDecoder;
	Decoder<64> DistanceDecoder;
	Decoder<64> LengthDecoder;

	int DecodeSFValue(const DecoderBase &currentTree, const unsigned int kNumSymbols);
	int DecodeSF(unsigned char *table);
public:
	int Explode(unsigned char *out, unsigned int outsize, FileReader *in, unsigned int insize, int flags);
};

class CExplosionError : CRecoverableError
{
public:
	CExplosionError(const char *message) : CRecoverableError(message) {}
};

int ShrinkLoop(unsigned char *out, unsigned int outsize, FileReader *in, unsigned int insize);