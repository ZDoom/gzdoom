#pragma once
#include "fs_files.h"

namespace FileSys {
	
class FZipExploder
{
	unsigned int Hold, Bits;
	FileReader *In;
	unsigned int InLeft;

	/****************************************************************
	   Shannon-Fano tree structures, variables and related routines
	 ****************************************************************/

	struct HuffNode
	{
		unsigned char Value;
		unsigned char Length;
		unsigned short ChildTable;
	};

	struct TableBuilder
	{
		unsigned char Value;
		unsigned char Length;
		unsigned short Code;
	};

	TArray<HuffNode> LiteralDecoder;
	TArray<HuffNode> DistanceDecoder;
	TArray<HuffNode> LengthDecoder;
	unsigned char ReadBuf[256];
	unsigned int bs, be;

	static int buildercmp(const void *a, const void *b);
	void InsertCode(TArray<HuffNode> &decoder, unsigned int pos, int bits, unsigned short code, int len, unsigned char value);
	unsigned int InitTable(TArray<HuffNode> &decoder, int numspots);
	int BuildDecoder(TArray<HuffNode> &decoder, TableBuilder *values, int numvals);
	int DecodeSFValue(const TArray<HuffNode> &currentTree);
	int DecodeSF(TArray<HuffNode> &decoder, int numvals);
public:
	int Explode(unsigned char *out, unsigned int outsize, FileReader &in, unsigned int insize, int flags);
};

int ShrinkLoop(unsigned char *out, unsigned int outsize, FileReader &in, unsigned int insize);
}
