#pragma once

#include "zstring.h"

//
// [ZZ] this really should be in codegen.h, but vmexec needs to access it
struct FScopeBarrier
{
	bool callable;
	bool readable;
	bool writable;

	// this is the error message
	FString callerror;
	FString readerror;
	FString writeerror;

	// this is used to make the error message.
	enum Side
	{
		Side_PlainData = 0,
		Side_UI = 1,
		Side_Play = 2,
		Side_Virtual = 3, // do NOT change the value
		Side_Clear = 4
	};
	int sidefrom;
	int sidelast;

	// Note: the same object can't be both UI and Play. This is checked explicitly in the field construction and will cause esoteric errors here if found.
	static int SideFromFlags(int flags);

	// same as above, but from object flags
	static int SideFromObjectFlags(int flags);

	//
	static int FlagsFromSide(int side);
	static int ObjectFlagsFromSide(int side);
	
	// used for errors
	static const char* StringFromSide(int side);

	// this modifies VARF_ flags and sets the side properly.
	static int ChangeSideInFlags(int flags, int side);
	// this modifies OF_ flags and sets the side properly.
	static int ChangeSideInObjectFlags(int flags, int side);
	FScopeBarrier();
	FScopeBarrier(int flags1, int flags2, const char* name);

	// AddFlags modifies ALLOWED actions by flags1->flags2.
	// This is used for comparing a.b.c.d access - if non-allowed field is seen anywhere in the chain, anything after it is non-allowed.
	// This struct is used so that the logic is in a single place.
	void AddFlags(int flags1, int flags2, const char* name);

	// this is called from vmexec.h
	static void ValidateNew(PClass* cls, int scope);
	static void ValidateCall(PClass* selftype, VMFunction *calledfunc, int outerside);

};

