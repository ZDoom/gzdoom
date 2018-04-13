#ifndef COMPATIBILITY_H
#define COMPATIBILITY_H

#include "doomtype.h"
#include "tarray.h"
#include "p_setup.h"

union FMD5Holder
{
	uint8_t Bytes[16];
	uint32_t DWords[4];
	hash_t Hash;
};

struct FCompatValues
{
	int CompatFlags[3];
	unsigned int ExtCommandIndex;
};

struct FMD5HashTraits
{
	hash_t Hash(const FMD5Holder key)
	{
		return key.Hash;
	}
	int Compare(const FMD5Holder left, const FMD5Holder right)
	{
		return left.DWords[0] != right.DWords[0] ||
			   left.DWords[1] != right.DWords[1] ||
			   left.DWords[2] != right.DWords[2] ||
			   left.DWords[3] != right.DWords[3];
	}
};

extern TMap<FMD5Holder, FCompatValues, FMD5HashTraits> BCompatMap;

void ParseCompatibility();
FName CheckCompatibility(MapData *map);
void SetCompatibilityParams(FName);

#endif
