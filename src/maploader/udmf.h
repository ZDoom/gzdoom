#ifndef __P_UDMF_H
#define __P_UDMF_H

#include "sc_man.h"
#include "m_fixed.h"

class UDMFParserBase
{
protected:
	FScanner sc;
	FName namespc = NAME_None;
	int namespace_bits;
	FString parsedString;
	bool BadCoordinates = false;

	void Skip();
	FName ParseKey(bool checkblock = false, bool *isblock = NULL);
	int CheckInt(const char *key);
	double CheckFloat(const char *key);
	double CheckCoordinate(const char *key);
	DAngle CheckAngle(const char *key);
	bool CheckBool(const char *key);
	const char *CheckString(const char *key);

	template<typename T>
	bool Flag(T &value, int mask, const char *key)
	{
		if (CheckBool(key))
		{
			value |= mask;
			return true;
		}
		else
		{
			value &= ~mask;
			return false;
		}
	}

};

#define BLOCK_ID (ENamedName)-1

#endif