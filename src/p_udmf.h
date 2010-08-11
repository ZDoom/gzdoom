#ifndef __P_UDMF_H
#define __P_UDMF_H

#include "sc_man.h"
#include "m_fixed.h"
#include "tables.h"

class UDMFParserBase
{
protected:
	FScanner sc;
	FName namespc;
	int namespace_bits;
	FString parsedString;

	void Skip();
	FName ParseKey(bool checkblock = false, bool *isblock = NULL);
	int CheckInt(const char *key);
	double CheckFloat(const char *key);
	fixed_t CheckFixed(const char *key);
	angle_t CheckAngle(const char *key);
	bool CheckBool(const char *key);
	const char *CheckString(const char *key);

	template<typename T>
	void Flag(T &value, int mask, const char *key)
	{
		if (CheckBool(key))
			value |= mask;
		else
			value &= ~mask;
	}

};

#define BLOCK_ID (ENamedName)-1

#endif