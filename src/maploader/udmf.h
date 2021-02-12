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
	int CheckInt(FName key);
	double CheckFloat(FName key);
	double CheckCoordinate(FName key);
	DAngle CheckAngle(FName key);
	bool CheckBool(FName key);
	const char *CheckString(FName key);

	template<typename T>
	bool Flag(T &value, int mask, FName key)
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