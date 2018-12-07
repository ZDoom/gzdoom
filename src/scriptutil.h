#pragma once


#include <stdarg.h>
#include "name.h"


class ScriptUtil
{
	static void BuildParameters(va_list ap);
	static void RunFunction(FName function, unsigned paramstart, VMReturn &returns);

public:	
	enum
	{
		End,
		Int,
		Pointer,
		Float,
		String,
		Class,
		ACSString,	// convenience helpers taking an ACS string index instead of a string
		ACSClass,
	};

	static int Exec(FName functionname, ...);
	static void Clear();
};
