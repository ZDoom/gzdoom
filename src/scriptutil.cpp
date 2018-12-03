//-----------------------------------------------------------------------------
//
// Copyright 2018 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
//
// DESCRIPTION: generalized interface for implementing ACS/FS functions
// in ZScript.
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "tarray.h"
#include "dobject.h"
#include "vm.h"
#include "scriptutil.h"
#include "p_acs.h"
#include "actorinlines.h"


static TArray<VMValue> parameters;
static TMap<FName, VMFunction*> functions;

void ScriptUtil::Clear()
{
	parameters.Clear();
	functions.Clear();
}

void ScriptUtil::BuildParameters(va_list ap)
{
	for(int type = va_arg(ap, int); type != End; type = va_arg(ap, int))
	{
		switch (type)
		{
			case Int:
				parameters.Push(VMValue(va_arg(ap, int)));
				break;
				
			case Pointer:
			case Class:		// this is just a pointer.
			case String:	// must be passed by reference to a persistent location!
				parameters.Push(VMValue(va_arg(ap, void*)));
				break;
				
			case Float:
				parameters.Push(VMValue(va_arg(ap, double)));
				break;
				
			case ACSClass:
				parameters.Push(VMValue(PClass::FindActor(FBehavior::StaticLookupString(va_arg(ap, int)))));
				break;
		}
	}
}

void ScriptUtil::RunFunction(FName functionname, unsigned paramstart, VMReturn &returns)
{
	VMFunction *func = nullptr;
	auto check = functions.CheckKey(functionname);
	if (!check)
	{
		func = PClass::FindFunction(NAME_ScriptUtil, functionname);
		if (func == nullptr) 
		{
			I_Error("Call to undefined function ScriptUtil.%s", functionname.GetChars());
		}
		functions.Insert(functionname, func);
	}
	else func = *check;

	VMCall(func, &parameters[paramstart], parameters.Size() - paramstart, &returns, 1);
}

int ScriptUtil::Exec(FName functionname, ...)
{
	unsigned paramstart = parameters.Size();
	va_list ap;
	va_start(ap, functionname);
	try
	{
		BuildParameters(ap);
		int ret = 0;
		VMReturn returns(&ret);
		RunFunction(functionname, paramstart, returns);
		va_end(ap);
		parameters.Clamp(paramstart);
		return ret;
	}
	catch(...)
	{
		va_end(ap);
		parameters.Clamp(paramstart);
		throw;
	}
}
