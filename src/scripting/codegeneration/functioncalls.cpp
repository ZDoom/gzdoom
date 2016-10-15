#if 0
/*
** cg_functioncall.cpp
**
**---------------------------------------------------------------------------
** Copyright 2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "doomtype.h"
#include "codegen.h"
#include "thingdef.h"
#include "zscript/zcc_parser.h"

// just some rough concepts for now...

//==========================================================================
//
// FindClassMemberFunction
//
// Looks for a name in a class's
//
//==========================================================================

PFunction *FindClassMemberFunction(PClass *cls, FName name, FScriptPosition &sc)
{
	PSymbolTable *symtable;
	auto symbol = cls->Symbols.FindSymbolInTable(name, symtable);
	auto funcsym = dyn_cast<PFunction>(symbol);

	if (symbol != nullptr)
	{
		if (funcsym == nullptr)
		{
			sc.Message(MSG_ERROR, "%s is not a member function of %s", name.GetChars(), cls->TypeName.GetChars());
		}
		else if (funcsym->Flags & VARF_Private && symtable != &cls->Symbols)
		{
			sc.Message(MSG_ERROR, "%s is declared private and not accessible", symbol->SymbolName.GetChars());
		}
		else if (funcsym->Flags & VARF_Deprecated)
		{
			sc.Message(MSG_WARNING, "Call to deprecated function %s", symbol->SymbolName.GetChars());
		}
	}
	return funcsym;
}


//==========================================================================
//
// let's save some work down the line by analyzing the type of function 
// that's being called right here and create appropriate nodes.
// A simple function call expression must be immediately resolvable in
// the context it gets compiled in, if the function name cannot be
// resolved here, it will never.
//
//==========================================================================

FxExpression *ConvertSimpleFunctionCall(ZCC_ExprID *function, FArgumentList *args, PClass *cls, FScriptPosition &sc)
{
	// Priority is as follows:
	//1. member functions of the containing class.
	//2. action specials.
	//3. floating point operations
	//4. global builtins

	if (cls != nullptr)
	{
		// There is an action function ACS_NamedExecuteWithResult which must be ignored here for this to work.
		if (function->Identifier != NAME_ACS_NamedExecuteWithResult)
		{
		{
			args = ConvertNodeList(fcall->Parameters);
			if (args->Size() == 0)
			{
				delete args;
				args = nullptr;
			}

			return new FxMemberunctionCall(new FxSelf(), new FxInvoker(), func, args, sc, false);
		}
	}
	
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *ConvertFunctionCall(ZCC_Expression *function, FArgumentList *args, PClass *cls, FScriptPosition &sc)
{
	// function names can either be
	// - plain identifiers
	// - class members
	// - array syntax for random() calls.
	
	switch(function->NodeType)
	{
		case AST_ExprID:
			return ConvertSimpleFunctionCall(static_cast<ZCC_ExprID *>(function), args, cls, sc);
		
		case AST_ExprMemberAccess:
			return ConvertMemberCall(fcall);
		
		case AST_ExprBinary:
			// Array access syntax is wrapped into a ZCC_ExprBinary object.
			if (fcall->Function->Operation == PEX_ArrayAccess)
			{
				return ConvertArrayFunctionCall(fcall);
			}
			break;

		default:
			break;
	}
	Error(fcall, "Invalid function identifier");
	return new FxNop(*fcall);	// return something so that we can continue
}


			assert(fcall->Function->NodeType == AST_ExprID);	// of course this cannot remain. Right now nothing more complex can come along but later this will have to be decomposed into 'self' and the actual function name.
		auto fname = static_cast<ZCC_ExprID *>(fcall->Function)->Identifier;
		return new FxFunctionCall(nullptr, fname, ConvertNodeList(fcall->Parameters), *ast);

#endif