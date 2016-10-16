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
//
//
//==========================================================================

FxExpression *ConvertFunctionCall(ZCC_Expression *function, FArgumentList *args, PClass *cls, FScriptPosition &sc)
{
	
	switch(function->NodeType)
	{
		case AST_ExprID:
			return new FxFunctionCall( ConvertSimpleFunctionCall(static_cast<ZCC_ExprID *>(function), args, cls, sc);
		
		case AST_ExprMemberAccess:
			return ConvertMemberCall(fcall);
		
		case AST_ExprBinary:
			// Array access syntax is wrapped into a ZCC_ExprBinary object.
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