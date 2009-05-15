#include "files.h"
#include "doomerrors.h"

class FZipExploder
{
	unsigned int Hold, Bits;
	FileReader *In;
	unsigned int InLeft;

	int READBYTE();

	/****************************************************************
		Huffman tree structures, variables and related routines

		These routines are one-bit-at-a-time decode routines. They
		are not as fast as multi-bit routines, but maybe a bit easier
		to understand and use a lot less memory.

		The tree is folded into a table.

	 ****************************************************************/

	struct HufNode {
		unsigned short b0;		/* 0-branch value + leaf node flag */
		unsigned short b1;		/* 1-branch value + leaf node flag */
		struct HufNode *jump;	/* 1-branch jump address */
	};

	struct HufNode LiteralTree[256];
	struct HufNode DistanceTree[64];
	struct HufNode LengthTree[64];
	struct HufNode *Places;

	unsigned char len;
	short fpos[17];
	int *flens;
	short fmax;

	int IsPat();
	int Rec();
	int DecodeSFValue(struct HufNode *currentTree);
	int CreateTree(struct HufNode *currentTree, int numval, int *lengths);
	int DecodeSF(int *table);
public:
	int Explode(unsigned char *out, unsigned int outsize, FileReader *in, unsigned int insize, int flags);
};

class CExplosionError : CRecoverableError
{
public:
	CExplosionError(const char *message) : CRecoverableError(message) {}
};
