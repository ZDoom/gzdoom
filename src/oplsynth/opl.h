#ifndef OPL_H
#define OPL_H

#include "zstring.h"

// Abstract base class for OPL emulators

class OPLEmul
{
public:
	OPLEmul() {}
	virtual ~OPLEmul() {}

	virtual void Reset() = 0;
	virtual void WriteReg(int reg, int v) = 0;
	virtual void Update(float *buffer, int length) = 0;
	virtual void SetPanning(int c, float left, float right) = 0;
	virtual FString GetVoiceString() { return FString(); }
};

OPLEmul *YM3812Create(bool stereo);
OPLEmul *DBOPLCreate(bool stereo);

#endif