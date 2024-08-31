#pragma once

#include "zstring.h"

enum EScopeFlags
{
	Scope_All = 0,
	Scope_UI = 1,		// Marks a class that defaults to VARF_UI for its fields/methods
	Scope_Play = 2,	// Marks a class that defaults to VARF_Play for its fields/methods
};

class PClass;
class VMFunction;

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
	static int SideFromObjectFlags(EScopeFlags flags);

	//
	static int FlagsFromSide(int side);
	static EScopeFlags ObjectFlagsFromSide(int side);

	// used for errors
	static const char* StringFromSide(int side);

	// this modifies VARF_ flags and sets the side properly.
	static int ChangeSideInFlags(int flags, int side);
	// this modifies OF_ flags and sets the side properly.
	static EScopeFlags ChangeSideInObjectFlags(EScopeFlags flags, int side);
	FScopeBarrier();
	FScopeBarrier(int flags1, int flags2, const char* name);

	// AddFlags modifies ALLOWED actions by flags1->flags2.
	// This is used for comparing a.b.c.d access - if non-allowed field is seen anywhere in the chain, anything after it is non-allowed.
	// This struct is used so that the logic is in a single place.
	void AddFlags(int flags1, int flags2, const char* name);


	static bool CheckSidesForFunctionPointer(int from, int to);

	// this is called from vmexec.h
	static void ValidateNew(PClass* cls, int scope);
	static void ValidateCall(PClass* selftype, VMFunction *calledfunc, int outerside);

};

