/*
** zcc_compile.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
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

#include "actor.h"
#include "thingdef.h"
#include "sc_man.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "doomerrors.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "m_alloc.h"
#include "zcc_parser.h"
#include "zcc_compile.h"
#include "v_text.h"
#include "p_lnspec.h"
#include "i_system.h"
#include "gdtoa.h"
#include "codegeneration/codegen.h"
#include "vmbuilder.h"

#define DEFINING_CONST ((PSymbolConst *)(void *)1)

//==========================================================================
//
// ZCCCompiler :: ProcessClass
//
//==========================================================================

void ZCCCompiler::ProcessClass(ZCC_Class *cnode, PSymbolTreeNode *treenode)
{
	Classes.Push(new ZCC_ClassWork(static_cast<ZCC_Class *>(cnode), treenode));
	auto cls = Classes.Last();

	auto node = cnode->Body;
	PSymbolTreeNode *childnode;
	ZCC_Enum *enumType = nullptr;

	FString name;
	name << "nodes - " << FName(cnode->NodeName);
	cls->TreeNodes.SetName(name);

	if (cnode->Replaces != nullptr && !static_cast<PClassActor *>(cnode->Type)->SetReplacement(cnode->Replaces->Id))
	{
		Warn(cnode, "Replaced type '%s' not found for %s", FName(cnode->Replaces->Id).GetChars(), cnode->Type->TypeName.GetChars());
	}

	// Need to check if the class actually has a body.
	if (node != nullptr) do
	{
		switch (node->NodeType)
		{
		case AST_Struct:
		case AST_ConstantDef:
		case AST_Enum:
			if ((childnode = AddTreeNode(static_cast<ZCC_NamedNode *>(node)->NodeName, node, &cls->TreeNodes)))
			{
				switch (node->NodeType)
				{
				case AST_Enum:			
					enumType = static_cast<ZCC_Enum *>(node);
					cls->Enums.Push(enumType);
					break;

				case AST_Struct:		
					ProcessStruct(static_cast<ZCC_Struct *>(node), childnode, cls->cls);	
					break;

				case AST_ConstantDef:	
					cls->Constants.Push(static_cast<ZCC_ConstantDef *>(node));	
					cls->Constants.Last()->Type = enumType;
					break;

				default: 
					assert(0 && "Default case is just here to make GCC happy. It should never be reached");
				}
			}
			break;

		case AST_VarDeclarator: 
			cls->Fields.Push(static_cast<ZCC_VarDeclarator *>(node)); 
			break;

		case AST_EnumTerminator:
			enumType = nullptr;
			break;

		case AST_States:
			cls->States.Push(static_cast<ZCC_States *>(node));
			break;

		case AST_FuncDeclarator:
			cls->Functions.Push(static_cast<ZCC_FuncDeclarator *>(node));
			break;

		case AST_Default:
			cls->Defaults.Push(static_cast<ZCC_Default *>(node));
			break;

		default:
			assert(0 && "Unhandled AST node type");
			break;
		}
		node = node->SiblingNext;
	}
	while (node != cnode->Body);
}

//==========================================================================
//
// ZCCCompiler :: ProcessStruct
//
//==========================================================================

void ZCCCompiler::ProcessStruct(ZCC_Struct *cnode, PSymbolTreeNode *treenode, ZCC_Class *outer)
{
	Structs.Push(new ZCC_StructWork(static_cast<ZCC_Struct *>(cnode), treenode, outer));
	ZCC_StructWork *cls = Structs.Last();

	auto node = cnode->Body;
	PSymbolTreeNode *childnode;
	ZCC_Enum *enumType = nullptr;

	// Need to check if the struct actually has a body.
	if (node != nullptr) do
	{
		switch (node->NodeType)
		{
		case AST_ConstantDef:
		case AST_Enum:
			if ((childnode = AddTreeNode(static_cast<ZCC_NamedNode *>(node)->NodeName, node, &cls->TreeNodes)))
			{
				switch (node->NodeType)
				{
				case AST_Enum:
					enumType = static_cast<ZCC_Enum *>(node);
					cls->Enums.Push(enumType);
					break;

				case AST_ConstantDef:
					cls->Constants.Push(static_cast<ZCC_ConstantDef *>(node));
					cls->Constants.Last()->Type = enumType;
					break;

				default:
					assert(0 && "Default case is just here to make GCC happy. It should never be reached");
				}
			}
			break;

		case AST_VarDeclarator: 
			cls->Fields.Push(static_cast<ZCC_VarDeclarator *>(node)); 
			break;

		case AST_EnumTerminator:
			enumType = nullptr;
			break;

		default:
			assert(0 && "Unhandled AST node type");
			break;
		}
		node = node->SiblingNext;
	} 
	while (node != cnode->Body);
}

//==========================================================================
//
// ZCCCompiler Constructor
//
//==========================================================================

ZCCCompiler::ZCCCompiler(ZCC_AST &ast, DObject *_outer, PSymbolTable &_symbols, PSymbolTable &_outsymbols)
	: Outer(_outer), GlobalTreeNodes(&_symbols), OutputSymbols(&_outsymbols), AST(ast)
{
	FScriptPosition::ResetErrorCounter();
	// Group top-level nodes by type
	if (ast.TopNode != NULL)
	{
		ZCC_TreeNode *node = ast.TopNode;
		PSymbolTreeNode *tnode;
		PType *enumType = nullptr;
		ZCC_Enum *zenumType = nullptr;

		do
		{
			switch (node->NodeType)
			{
			case AST_Class:
			case AST_Struct:
			case AST_ConstantDef:
			case AST_Enum:
				if ((tnode = AddTreeNode(static_cast<ZCC_NamedNode *>(node)->NodeName, node, GlobalTreeNodes)))
				{
					switch (node->NodeType)
					{
					case AST_Enum:
						zenumType = static_cast<ZCC_Enum *>(node);
						enumType = NewEnum(zenumType->NodeName, nullptr);
						GlobalSymbols.AddSymbol(new PSymbolType(zenumType->NodeName, enumType));
						break;

					case AST_Class:
						ProcessClass(static_cast<ZCC_Class *>(node), tnode);
						break;

					case AST_Struct:
						ProcessStruct(static_cast<ZCC_Struct *>(node), tnode, nullptr);
						break;

					case AST_ConstantDef:
						Constants.Push(static_cast<ZCC_ConstantDef *>(node));
						Constants.Last()->Type = zenumType;
						break;

					default:
						assert(0 && "Default case is just here to make GCC happy. It should never be reached");
					}
				}
				break;

			case AST_EnumTerminator:
				zenumType = nullptr;
				break;

			default:
				assert(0 && "Unhandled AST node type");
				break;
			}
			node = node->SiblingNext;
		} while (node != ast.TopNode);
	}
}

ZCCCompiler::~ZCCCompiler()
{
	for (auto s : Structs)
	{
		delete s;
	}
	for (auto c : Classes)
	{
		delete c;
	}
	Structs.Clear();
	Classes.Clear();
}

//==========================================================================
//
// ZCCCompiler :: AddTreeNode
//
// Keeps track of definition nodes by their names. Ensures that all names
// in this scope are unique.
//
//==========================================================================

PSymbolTreeNode *ZCCCompiler::AddTreeNode(FName name, ZCC_TreeNode *node, PSymbolTable *treenodes, bool searchparents)
{
	PSymbol *check = treenodes->FindSymbol(name, searchparents);
	if (check != NULL)
	{
		assert(check->IsA(RUNTIME_CLASS(PSymbolTreeNode)));
		Error(node, "Attempt to redefine '%s'", name.GetChars());
		Error(static_cast<PSymbolTreeNode *>(check)->Node, " Original definition is here");
		return nullptr;
	}
	else
	{
		auto sy = new PSymbolTreeNode(name, node);
		FString name;
		treenodes->AddSymbol(sy);
		return sy;
	}
}

//==========================================================================
//
// ZCCCompiler :: Warn
//
// Prints a warning message, and increments WarnCount.
//
//==========================================================================

void ZCCCompiler::Warn(ZCC_TreeNode *node, const char *msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	MessageV(node, TEXTCOLOR_ORANGE, msg, argptr);
	va_end(argptr);

	FScriptPosition::WarnCounter++;
}

//==========================================================================
//
// ZCCCompiler :: Error
//
// Prints an error message, and increments ErrorCount.
//
//==========================================================================

void ZCCCompiler::Error(ZCC_TreeNode *node, const char *msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	MessageV(node, TEXTCOLOR_RED, msg, argptr);
	va_end(argptr);

	FScriptPosition::ErrorCounter++;
}

//==========================================================================
//
// ZCCCompiler :: MessageV
//
// Prints a message, annotated with the source location for the tree node.
//
//==========================================================================

void ZCCCompiler::MessageV(ZCC_TreeNode *node, const char *txtcolor, const char *msg, va_list argptr)
{
	FString composed;

	composed.Format("%s%s, line %d: ", txtcolor, node->SourceName->GetChars(), node->SourceLoc);
	composed.VAppendFormat(msg, argptr);
	composed += '\n';
	PrintString(PRINT_HIGH, composed);
}

//==========================================================================
//
// ZCCCompiler :: Compile
//
// Compile everything defined at this level.
//
//==========================================================================

int ZCCCompiler::Compile()
{
	CreateClassTypes();
	CreateStructTypes();
	CompileAllConstants();
	CompileAllFields();
	InitDefaults();
	InitFunctions();
	CompileStates();
	return FScriptPosition::ErrorCounter;
}

//==========================================================================
//
// ZCCCompiler :: CreateStructTypes
//
// Creates a PStruct for every struct.
//
//==========================================================================

void ZCCCompiler::CreateStructTypes()
{
	for(auto s : Structs)
	{
		s->Outer = s->OuterDef == nullptr? nullptr : s->OuterDef->Type;
		s->strct->Type = NewStruct(s->NodeName(), s->Outer);
		s->strct->Symbol = new PSymbolType(s->NodeName(), s->Type());
		s->Type()->Symbols.SetName(FName(s->NodeName()));
		GlobalSymbols.AddSymbol(s->strct->Symbol);

		for (auto e : s->Enums)
		{
			auto etype = NewEnum(e->NodeName, s->Type());
			s->Type()->Symbols.AddSymbol(new PSymbolType(e->NodeName, etype));
		}
	}
}

//==========================================================================
//
// ZCCCompiler :: CreateClassTypes
//
// Creates a PClass for every class so that we get access to the symbol table
// These will be created with unknown size because for that we need to
// process all fields first, but to do that we need the PClass and some
// other info depending on the PClass.
//
//==========================================================================

void ZCCCompiler::CreateClassTypes()
{
	// we are going to sort the classes array so that entries are sorted in order of inheritance.

	auto OrigClasses = std::move(Classes);
	Classes.Clear();
	bool donesomething = true;
	while (donesomething)
	{
		donesomething = false;
		for (unsigned i = 0; i<OrigClasses.Size(); i++)
		{
			auto c = OrigClasses[i];
			// Check if we got the parent already defined.
			PClass *parent;
			auto ParentName = c->cls->ParentName;

			if (ParentName != nullptr && ParentName->SiblingNext == ParentName) parent = PClass::FindClass(ParentName->Id);
			else if (ParentName == nullptr) parent = RUNTIME_CLASS(DObject);
			else
			{
				// The parent is a dotted name which the type system currently does not handle.
				// Once it does this needs to be implemented here.
				auto p = ParentName;
				FString build;

				do
				{
					if (build.IsNotEmpty()) build += '.';
					build += FName(p->Id);
					p = static_cast<decltype(p)>(p->SiblingNext);
				} while (p != ParentName);
				Error(c->cls, "Qualified name '%s' for base class not supported in '%s'", build.GetChars(), FName(c->NodeName()).GetChars());
				parent = RUNTIME_CLASS(DObject);
			}

			if (parent != nullptr)
			{
				// The parent exists, we may create a type for this class
				if (c->cls->Flags & ZCC_Native)
				{
					// If this is a native class, its own type must also already exist and not be a runtime class.
					auto me = PClass::FindClass(c->NodeName());
					if (me == nullptr)
					{
						Error(c->cls, "Unknown native class %s", c->NodeName().GetChars());
						me = parent->FindClassTentative(c->NodeName());
					}
					else if (me->bRuntimeClass)
					{
						Error(c->cls, "%s is not a native class", c->NodeName().GetChars());
					}
					else
					{
						DPrintf(DMSG_SPAMMY, "Registered %s as native with parent %s\n", me->TypeName.GetChars(), parent->TypeName.GetChars());
					}
					c->cls->Type = me;
				}
				else
				{
					// We will never get here if the name is a duplicate, so we can just do the assignment.
					c->cls->Type = parent->FindClassTentative(c->NodeName());
				}
				c->cls->Symbol = new PSymbolType(c->NodeName(), c->Type());
				GlobalSymbols.AddSymbol(c->cls->Symbol);
				c->Type()->Symbols.SetName(c->NodeName());
				Classes.Push(c);
				OrigClasses.Delete(i--);
				donesomething = true;
			}
			else
			{
				// No base class found. Now check if something in the unprocessed classes matches.
				// If not, print an error. If something is found let's retry again in the next iteration.
				bool found = false;
				for (auto d : OrigClasses)
				{
					if (d->NodeName() == c->cls->ParentName->Id)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					Error(c->cls, "Class %s has unknown base class %s", c->NodeName().GetChars(), FName(c->cls->ParentName->Id).GetChars());
					// create a placeholder so that the compiler can continue looking for errors.
					c->cls->Type = RUNTIME_CLASS(DObject)->FindClassTentative(c->NodeName());
					c->cls->Symbol = new PSymbolType(c->NodeName(), c->Type());
					GlobalSymbols.AddSymbol(c->cls->Symbol);
					c->Type()->Symbols.SetName(c->NodeName());
					Classes.Push(c);
					OrigClasses.Delete(i--);
					donesomething = true;
				}
			}
		}
	}

	// What's left refers to some other class in the list but could not be resolved.
	// This normally means a circular reference.
	for (auto c : OrigClasses)
	{
		Error(c->cls, "Class %s has circular inheritance", FName(c->NodeName()).GetChars());
		c->cls->Type = RUNTIME_CLASS(DObject)->FindClassTentative(c->NodeName());
		c->cls->Symbol = new PSymbolType(c->NodeName(), c->Type());
		c->Type()->Symbols.SetName(FName(c->NodeName()).GetChars());
		GlobalSymbols.AddSymbol(c->cls->Symbol);
		Classes.Push(c);
	}

	// Last but not least: Now that all classes have been created, we can create the symbols for the internal enums and link the treenode symbol tables
	for (auto cd : Classes)
	{
		for (auto e : cd->Enums)
		{
			auto etype = NewEnum(e->NodeName, cd->Type());
			cd->Type()->Symbols.AddSymbol(new PSymbolType(e->NodeName, etype));
		}
		// Link the tree node tables. We only can do this after we know the class relations.
		for (auto cc : Classes)
		{
			if (cc->Type() == cd->Type()->ParentClass)
			{
				cd->TreeNodes.SetParentTable(&cc->TreeNodes);
				break;
			}
		}
	}
}

//==========================================================================
//
// ZCCCompiler :: AddConstants
//
// Helper for CompileAllConstants
//
//==========================================================================

void ZCCCompiler::CopyConstants(TArray<ZCC_ConstantWork> &dest, TArray<ZCC_ConstantDef*> &Constants, PSymbolTable *ot)
{
	for (auto c : Constants)
	{
		dest.Push({ c, ot });
	}
}

//==========================================================================
//
// ZCCCompiler :: CompileAllConstants
//
// Make symbols from every constant defined at all levels.
// Since constants may only depend on other constants this can be done
// without any more involved processing of the AST as a first step.
//
//==========================================================================

void ZCCCompiler::CompileAllConstants()
{
	// put all constants in one list to make resolving this easier.
	TArray<ZCC_ConstantWork> constantwork;

	CopyConstants(constantwork, Constants, OutputSymbols);
	for (auto c : Classes)
	{
		CopyConstants(constantwork, c->Constants, &c->Type()->Symbols);
	}
	for (auto s : Structs)
	{
		CopyConstants(constantwork, s->Constants, &s->Type()->Symbols);
	}

	// Before starting to resolve the list, let's create symbols for all already resolved ones first (i.e. all literal constants), to reduce work.
	for (unsigned i = 0; i<constantwork.Size(); i++)
	{
		if (constantwork[i].node->Value->NodeType == AST_ExprConstant)
		{
			AddConstant(constantwork[i]);
			// Remove the constant from the list
			constantwork.Delete(i);
			i--;
		}
	}
	bool donesomething = true;
	// Now go through this list until no more constants can be resolved. The remaining ones will be non-constant values.
	while (donesomething && constantwork.Size() > 0)
	{
		donesomething = false;
		for (unsigned i = 0; i < constantwork.Size(); i++)
		{
			if (CompileConstant(constantwork[i].node, constantwork[i].outputtable))
			{
				AddConstant(constantwork[i]);
				// Remove the constant from the list
				constantwork.Delete(i);
				i--;
				donesomething = true;
			}
		}
	}
	for (unsigned i = 0; i < constantwork.Size(); i++)
	{
		Error(constantwork[i].node, "%s is not a constant", FName(constantwork[i].node->NodeName).GetChars());
	}
}

//==========================================================================
//
// ZCCCompiler :: AddConstant
//
// Adds a constant to its assigned symbol table
//
//==========================================================================

void ZCCCompiler::AddConstant(ZCC_ConstantWork &constant)
{
	auto def = constant.node;
	auto val = def->Value;
	if (val->NodeType == AST_ExprConstant)
	{
		ZCC_ExprConstant *cval = static_cast<ZCC_ExprConstant *>(val);
		if (cval->Type == TypeString)
		{
			def->Symbol = new PSymbolConstString(def->NodeName, *(cval->StringVal));
		}
		else if (cval->Type->IsA(RUNTIME_CLASS(PInt)))
		{
			// How do we get an Enum type in here without screwing everything up???
			//auto type = def->Type != nullptr ? def->Type : cval->Type;
			def->Symbol = new PSymbolConstNumeric(def->NodeName, cval->Type, cval->IntVal);
		}
		else if (cval->Type->IsA(RUNTIME_CLASS(PFloat)))
		{
			if (def->Type != nullptr)
			{
				Error(def, "Enum members must be integer values");
			}
			def->Symbol = new PSymbolConstNumeric(def->NodeName, cval->Type, cval->DoubleVal);
		}
		else
		{
			Error(def->Value, "Bad type for constant definiton");
			def->Symbol = nullptr;
		}

		if (def->Symbol == nullptr)
		{
			// Create a dummy constant so we don't make any undefined value warnings.
			def->Symbol = new PSymbolConstNumeric(def->NodeName, TypeError, 0);
		}
		constant.outputtable->ReplaceSymbol(def->Symbol);
	}
}

//==========================================================================
//
// ZCCCompiler :: CompileConstant
//
// For every constant definition, evaluate its value (which should result
// in a constant), and create a symbol for it.
//
//==========================================================================

bool ZCCCompiler::CompileConstant(ZCC_ConstantDef *def, PSymbolTable *sym)
{
	assert(def->Symbol == nullptr);

	def->Symbol = DEFINING_CONST;	// avoid recursion
	ZCC_Expression *val = Simplify(def->Value, sym);
	def->Value = val;
	if (def->Symbol == DEFINING_CONST) def->Symbol = nullptr;
	return (val->NodeType == AST_ExprConstant);
}


//==========================================================================
//
// ZCCCompiler :: Simplify
//
// For an expression,
//   Evaluate operators whose arguments are both constants, replacing it
//     with a new constant.
//   For a binary operator with one constant argument, put it on the right-
//     hand operand, where permitted.
//   Perform automatic type promotion.
//
//==========================================================================

ZCC_Expression *ZCCCompiler::Simplify(ZCC_Expression *root, PSymbolTable *sym)
{
	if (root->NodeType == AST_ExprUnary)
	{
		return SimplifyUnary(static_cast<ZCC_ExprUnary *>(root), sym);
	}
	else if (root->NodeType == AST_ExprBinary)
	{
		return SimplifyBinary(static_cast<ZCC_ExprBinary *>(root), sym);
	}
	else if (root->Operation == PEX_ID)
	{
		return IdentifyIdentifier(static_cast<ZCC_ExprID *>(root), sym);
	}
	else if (root->Operation == PEX_MemberAccess)
	{
		return SimplifyMemberAccess(static_cast<ZCC_ExprMemberAccess *>(root), sym);
	}
	else if (root->Operation == PEX_FuncCall)
	{
		return SimplifyFunctionCall(static_cast<ZCC_ExprFuncCall *>(root), sym);
	}
	return root;
}

//==========================================================================
//
// ZCCCompiler :: SimplifyUnary
//
//==========================================================================

ZCC_Expression *ZCCCompiler::SimplifyUnary(ZCC_ExprUnary *unary, PSymbolTable *sym)
{
	unary->Operand = Simplify(unary->Operand, sym);
	if (unary->Operand->Type == nullptr)
	{
		return unary;
	}
	ZCC_OpProto *op = PromoteUnary(unary->Operation, unary->Operand);
	if (op == NULL)
	{ // Oh, poo!
		unary->Type = TypeError;
	}
	else if (unary->Operand->Operation == PEX_ConstValue)
	{
		return op->EvalConst1(static_cast<ZCC_ExprConstant *>(unary->Operand));
	}
	return unary;
}

//==========================================================================
//
// ZCCCompiler :: SimplifyBinary
//
//==========================================================================

ZCC_Expression *ZCCCompiler::SimplifyBinary(ZCC_ExprBinary *binary, PSymbolTable *sym)
{
	binary->Left = Simplify(binary->Left, sym);
	binary->Right = Simplify(binary->Right, sym);
	if (binary->Left->Type == nullptr || binary->Right->Type == nullptr)
	{
		// We do not know yet what this is so we cannot promote it (yet.)
		return binary;
	}
	ZCC_OpProto *op = PromoteBinary(binary->Operation, binary->Left, binary->Right);
	if (op == NULL)
	{
		binary->Type = TypeError;
	}
	else if (binary->Left->Operation == PEX_ConstValue &&
		binary->Right->Operation == PEX_ConstValue)
	{
		return op->EvalConst2(static_cast<ZCC_ExprConstant *>(binary->Left),
							  static_cast<ZCC_ExprConstant *>(binary->Right), AST.Strings);
	}
	return binary;
}

//==========================================================================
//
// ZCCCompiler :: SimplifyMemberAccess
//
//==========================================================================

ZCC_Expression *ZCCCompiler::SimplifyMemberAccess(ZCC_ExprMemberAccess *dotop, PSymbolTable *symt)
{
	PSymbolTable *symtable;

	dotop->Left = Simplify(dotop->Left, symt);

	if (dotop->Left->Operation == PEX_TypeRef)
	{ // Type refs can be evaluated now.
		PType *ref = static_cast<ZCC_ExprTypeRef *>(dotop->Left)->RefType;
		PSymbol *sym = ref->Symbols.FindSymbolInTable(dotop->Right, symtable);
		if (sym != nullptr)
		{
			ZCC_Expression *expr = NodeFromSymbol(sym, dotop, symtable);
			if (expr != nullptr)
			{
				return expr;
			}
		}
	}
	else if (dotop->Left->Operation == PEX_Super)
	{
		symt = symt->GetParentTable();
		if (symt != nullptr)
		{
			PSymbol *sym = symt->FindSymbolInTable(dotop->Right, symtable);
			if (sym != nullptr)
			{
				ZCC_Expression *expr = NodeFromSymbol(sym, dotop, symtable);
				if (expr != nullptr)
				{
					return expr;
				}
			}
		}
	}
	return dotop;
}

//==========================================================================
//
// ZCCCompiler :: SimplifyFunctionCall
//
// This may replace a function call with cast(s), since they look like the
// same thing to the parser.
//
//==========================================================================

ZCC_Expression *ZCCCompiler::SimplifyFunctionCall(ZCC_ExprFuncCall *callop, PSymbolTable *sym)
{
	ZCC_FuncParm *parm;
	int parmcount = 0;

	callop->Function = Simplify(callop->Function, sym);
	parm = callop->Parameters;
	if (parm != NULL)
	{
		do
		{
			parmcount++;
			assert(parm->NodeType == AST_FuncParm);
			parm->Value = Simplify(parm->Value, sym);
			parm = static_cast<ZCC_FuncParm *>(parm->SiblingNext);
		}
		while (parm != callop->Parameters);
	}
	// If the left side is a type ref, then this is actually a cast
	// and not a function call.
	if (callop->Function->Operation == PEX_TypeRef)
	{
		if (parmcount != 1)
		{
			Error(callop, "Type cast requires one parameter");
			callop->ToErrorNode();
		}
		else
		{
			PType *dest = static_cast<ZCC_ExprTypeRef *>(callop->Function)->RefType;
			const PType::Conversion *route[CONVERSION_ROUTE_SIZE];
			int routelen = parm->Value->Type->FindConversion(dest, route, countof(route));
			if (routelen < 0)
			{
				///FIXME: Need real type names
				Error(callop, "Cannot convert type 1 to type 2");
				callop->ToErrorNode();
			}
			else
			{
				ZCC_Expression *val = ApplyConversion(parm->Value, route, routelen);
				assert(val->Type == dest);
				return val;
			}
		}
	}
	return callop;
}

//==========================================================================
//
// ZCCCompiler :: PromoteUnary
//
// Converts the operand into a format preferred by the operator.
//
//==========================================================================

ZCC_OpProto *ZCCCompiler::PromoteUnary(EZCCExprType op, ZCC_Expression *&expr)
{
	if (expr->Type == TypeError)
	{
		return NULL;
	}
	const PType::Conversion *route[CONVERSION_ROUTE_SIZE];
	int routelen = countof(route);
	ZCC_OpProto *proto = ZCC_OpInfo[op].FindBestProto(expr->Type, route, routelen);

	if (proto != NULL)
	{
		expr = ApplyConversion(expr, route, routelen);
	}
	return proto;
}

//==========================================================================
//
// ZCCCompiler :: PromoteBinary
//
// Converts the operands into a format (hopefully) compatible with the
// operator.
//
//==========================================================================

ZCC_OpProto *ZCCCompiler::PromoteBinary(EZCCExprType op, ZCC_Expression *&left, ZCC_Expression *&right)
{
	// If either operand is of type 'error', the result is also 'error'
	if (left->Type == TypeError || right->Type == TypeError)
	{
		return NULL;
	}
	const PType::Conversion *route1[CONVERSION_ROUTE_SIZE], *route2[CONVERSION_ROUTE_SIZE];
	int route1len = countof(route1), route2len = countof(route2);
	ZCC_OpProto *proto = ZCC_OpInfo[op].FindBestProto(left->Type, route1, route1len, right->Type, route2, route2len);
	if (proto != NULL)
	{
		left = ApplyConversion(left, route1, route1len);
		right = ApplyConversion(right, route2, route2len);
	}
	return proto;
}

//==========================================================================
//
// ZCCCompiler :: ApplyConversion
//
//==========================================================================

ZCC_Expression *ZCCCompiler::ApplyConversion(ZCC_Expression *expr, const PType::Conversion **route, int routelen)
{
	for (int i = 0; i < routelen; ++i)
	{
		if (expr->Operation != PEX_ConstValue)
		{
			expr = AddCastNode(route[i]->TargetType, expr);
		}
		else
		{
			route[i]->ConvertConstant(static_cast<ZCC_ExprConstant *>(expr), AST.Strings);
		}
	}
	return expr;
}

//==========================================================================
//
// ZCCCompiler :: AddCastNode
//
//==========================================================================

ZCC_Expression *ZCCCompiler::AddCastNode(PType *type, ZCC_Expression *expr)
{
	assert(expr->Operation != PEX_ConstValue && "Expression must not be constant");
	// TODO: add a node here
	return expr;
}

//==========================================================================
//
// ZCCCompiler :: IdentifyIdentifier
//
// Returns a node that represents what the identifer stands for.
//
//==========================================================================

ZCC_Expression *ZCCCompiler::IdentifyIdentifier(ZCC_ExprID *idnode, PSymbolTable *symt)
{
	// Check the symbol table for the identifier.
	PSymbolTable *table;
	PSymbol *sym = symt->FindSymbolInTable(idnode->Identifier, table);
	// GlobalSymbols cannot be the parent of a class's symbol table so we have to look for global symbols explicitly.
	if (sym == nullptr && symt != &GlobalSymbols) sym = GlobalSymbols.FindSymbolInTable(idnode->Identifier, table);
	if (sym != nullptr)
	{
		ZCC_Expression *node = NodeFromSymbol(sym, idnode, table);
		if (node != NULL)
		{
			return node;
		}
	}
	else
	{
		// Also handle line specials.
		// To call this like a function this needs to be done differently, but for resolving constants this is ok.
		int spec = P_FindLineSpecial(FName(idnode->Identifier).GetChars());
		if (spec != 0)
		{
			ZCC_ExprConstant *val = static_cast<ZCC_ExprConstant *>(AST.InitNode(sizeof(*val), AST_ExprConstant, idnode));
			val->Operation = PEX_ConstValue;
			val->Type = TypeSInt32;
			val->IntVal = spec;
			return val;
		}

		Error(idnode, "Unknown identifier '%s'", FName(idnode->Identifier).GetChars());
		idnode->ToErrorNode();
	}
	return idnode;
}

//==========================================================================
//
// ZCCCompiler :: NodeFromSymbol
//
//==========================================================================

ZCC_Expression *ZCCCompiler::NodeFromSymbol(PSymbol *sym, ZCC_Expression *source, PSymbolTable *table)
{
	assert(sym != nullptr);

	if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConst)))
	{
		return NodeFromSymbolConst(static_cast<PSymbolConst *>(sym), source);
	}
	else if (sym->IsKindOf(RUNTIME_CLASS(PSymbolType)))
	{
		return NodeFromSymbolType(static_cast<PSymbolType *>(sym), source);
	}
	return NULL;
}

//==========================================================================
//
// ZCCCompiler :: NodeFromSymbolConst
//
// Returns a new AST constant node with the symbol's content.
//
//==========================================================================

ZCC_ExprConstant *ZCCCompiler::NodeFromSymbolConst(PSymbolConst *sym, ZCC_Expression *idnode)
{
	ZCC_ExprConstant *val = static_cast<ZCC_ExprConstant *>(AST.InitNode(sizeof(*val), AST_ExprConstant, idnode));
	val->Operation = PEX_ConstValue;
	if (sym == NULL)
	{
		val->Type = TypeError;
		val->IntVal = 0;
	}
	else if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConstString)))
	{
		val->StringVal = AST.Strings.Alloc(static_cast<PSymbolConstString *>(sym)->Str);
		val->Type = TypeString;
	}
	else
	{
		val->Type = sym->ValueType;
		if (val->Type != TypeError)
		{
			assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolConstNumeric)));
			if (sym->ValueType->IsKindOf(RUNTIME_CLASS(PInt)))
			{
				val->IntVal = static_cast<PSymbolConstNumeric *>(sym)->Value;
			}
			else
			{
				assert(sym->ValueType->IsKindOf(RUNTIME_CLASS(PFloat)));
				val->DoubleVal = static_cast<PSymbolConstNumeric *>(sym)->Float;
			}
		}
	}
	return val;
}

//==========================================================================
//
// ZCCCompiler :: NodeFromSymbolType
//
// Returns a new AST type ref node with the symbol's content.
//
//==========================================================================

ZCC_ExprTypeRef *ZCCCompiler::NodeFromSymbolType(PSymbolType *sym, ZCC_Expression *idnode)
{
	ZCC_ExprTypeRef *ref = static_cast<ZCC_ExprTypeRef *>(AST.InitNode(sizeof(*ref), AST_ExprTypeRef, idnode));
	ref->Operation = PEX_TypeRef;
	ref->RefType = sym->Type;
	ref->Type = NewClassPointer(RUNTIME_CLASS(PType));
	return ref;
}

//==========================================================================
//
// ZCCCompiler :: CompileAllFields
//
// builds the internal structure of all classes and structs
//
//==========================================================================

void ZCCCompiler::CompileAllFields()
{
	// Create copies of the arrays which can be altered
	auto Classes = this->Classes;
	auto Structs = this->Structs;

	// first step: Look for native classes with native children.
	// These may not have any variables added to them because it'd clash with the native definitions.
	for (unsigned i = 0; i < Classes.Size(); i++)
	{
		auto c = Classes[i];

		if (c->Type()->Size != TentativeClass && c->Fields.Size() > 0)
		{
			// We need to search the global class table here because not all children may have a scripted definition attached.
			for (auto ac : PClass::AllClasses)
			{
				if (ac->ParentClass == c->Type() && ac->Size != TentativeClass)
				{
					Error(c->cls, "Trying to add fields to class '%s' with native children", c->Type()->TypeName.GetChars());
					Classes.Delete(i--);
					break;
				}
			}
		}
	}
	bool donesomething = true;
	while (donesomething && (Structs.Size() > 0 || Classes.Size() > 0))
	{
		donesomething = false;
		for (unsigned i = 0; i < Structs.Size(); i++)
		{
			if (CompileFields(Structs[i]->Type(), Structs[i]->Fields, Structs[i]->Outer, &Structs[i]->TreeNodes, true))
			{
				// Remove from the list if all fields got compiled.
				Structs.Delete(i--);
				donesomething = true;
			}
		}
		for (unsigned i = 0; i < Classes.Size(); i++)
		{
			auto type = Classes[i]->Type();
			if (type->Size == TentativeClass)
			{
				if (type->ParentClass->Size == TentativeClass)
				{
					// we do not know the parent class's size yet, so skip this class for now.
					continue;
				}
				else
				{
					// Inherit the size of the parent class
					type->Size = Classes[i]->Type()->ParentClass->Size;
				}
			}
			if (CompileFields(type, Classes[i]->Fields, nullptr, &Classes[i]->TreeNodes, false))
			{
				// Remove from the list if all fields got compiled.
				Classes.Delete(i--);
				donesomething = true;
			}
		}
	}
	// This really should never happen, but if it does, let's better print an error.
	for (auto s : Structs)
	{
		Error(s->strct, "Unable to resolve all fields for struct %s", FName(s->NodeName()).GetChars());
	}
	for (auto s : Classes)
	{
		Error(s->cls, "Unable to resolve all fields for class %s", FName(s->NodeName()).GetChars());
	}
}

//==========================================================================
//
// ZCCCompiler :: CompileFields
//
// builds the internal structure of a single class or struct
//
//==========================================================================

bool ZCCCompiler::CompileFields(PStruct *type, TArray<ZCC_VarDeclarator *> &Fields, PClass *Outer, PSymbolTable *TreeNodes, bool forstruct)
{
	while (Fields.Size() > 0)
	{
		auto field = Fields[0];

		PType *fieldtype = DetermineType(type, field, field->Names->Name, field->Type, true);

		// For structs only allow 'deprecated', for classes exclude function qualifiers.
		int notallowed = forstruct? ~ZCC_Deprecated : ZCC_Latent | ZCC_Final | ZCC_Action | ZCC_Static | ZCC_FuncConst | ZCC_Abstract; 

		if (field->Flags & notallowed)
		{
			Error(field, "Invalid qualifiers for %s (%s not allowed)", FName(field->Names->Name).GetChars(), FlagsToString(field->Flags & notallowed));
			field->Flags &= notallowed;
		}
		uint32_t varflags = 0;

		// These map directly to implementation flags.
		if (field->Flags & ZCC_Private) varflags |= VARF_Private;
		if (field->Flags & ZCC_Protected) varflags |= VARF_Protected;
		if (field->Flags & ZCC_Deprecated) varflags |= VARF_Deprecated;
		if (field->Flags & ZCC_ReadOnly) varflags |= VARF_ReadOnly;

		if (field->Flags & ZCC_Native)
		{
			// todo: get the native address of this field.
		}
		if (field->Flags & ZCC_Meta)
		{
			varflags |= VARF_ReadOnly;	// metadata implies readonly
			// todo: this needs to go into the metaclass and needs some handling
		}

		if (field->Type->ArraySize != nullptr)
		{
			fieldtype = ResolveArraySize(fieldtype, field->Type->ArraySize, &type->Symbols);
		}

		auto name = field->Names;
		do
		{
			if (AddTreeNode(name->Name, name, TreeNodes, !forstruct))
			{
				auto thisfieldtype = fieldtype;
				if (name->ArraySize != nullptr)
				{
					thisfieldtype = ResolveArraySize(thisfieldtype, name->ArraySize, &type->Symbols);
				}

				type->AddField(name->Name, thisfieldtype, varflags);
			}
			name = static_cast<ZCC_VarName*>(name->SiblingNext);
		} while (name != field->Names);
		Fields.Delete(0);
	}
	return Fields.Size() == 0;
}

//==========================================================================
//
// ZCCCompiler :: FieldFlagsToString
//
// creates a string for a field's flags
//
//==========================================================================

FString ZCCCompiler::FlagsToString(uint32_t flags)
{
	const char *flagnames[] = { "native", "static", "private", "protected", "latent", "final", "meta", "action", "deprecated", "readonly", "funcconst", "abstract" };
	FString build;

	for (int i = 0; i < 12; i++)
	{
		if (flags & (1 << i))
		{
			if (build.IsNotEmpty()) build += ", ";
			build += flagnames[i];
		}
	}
	return build;
}

//==========================================================================
//
// ZCCCompiler :: DetermineType
//
// retrieves the type for this field, for arrays the type of a single entry.
//
//==========================================================================

PType *ZCCCompiler::DetermineType(PType *outertype, ZCC_TreeNode *field, FName name, ZCC_Type *ztype, bool allowarraytypes)
{
	if (!allowarraytypes && ztype->ArraySize != nullptr)
	{
		Error(field, "%s: Array type not allowed", name.GetChars());
		return TypeError;
	}
	switch (ztype->NodeType)
	{
	case AST_BasicType:
	{
		auto btype = static_cast<ZCC_BasicType *>(ztype);
		switch (btype->Type)
		{
		case ZCC_SInt8:
			return TypeSInt8;

		case ZCC_UInt8:
			return TypeUInt8;

		case ZCC_SInt16:
			return TypeSInt16;

		case ZCC_UInt16:
			return TypeUInt16;

		case ZCC_SInt32:
		case ZCC_IntAuto: // todo: for enums, autoselect appropriately sized int
			return TypeSInt32;

		case ZCC_UInt32:
			return TypeUInt32;

		case ZCC_Bool:
			return TypeBool;

			// Do we really want to allow single precision floats, despite all the problems they cause?
			// These are nearly guaranteed to desync between MSVC and GCC on x87, because GCC does not implement an IEEE compliant mode
		case ZCC_Float32:
		case ZCC_FloatAuto:
			//return TypeFloat32;
		case ZCC_Float64:
			return TypeFloat64;

		case ZCC_String:
			return TypeString;

		case ZCC_Name:
			return TypeName;

		case ZCC_Vector2:
			return TypeVector2;

		case ZCC_Vector3:
			return TypeVector3;

		case ZCC_Vector4:
			// This has almost no use, so we really shouldn't bother.
			Error(field, "vector<4> not implemented for %s", name.GetChars());
			return TypeError;

		case ZCC_State:
			return TypeState;

		case ZCC_Color:
			return TypeColor;

		case ZCC_Sound:
			return TypeSound;

		case ZCC_UserType:
			return ResolveUserType(btype, &outertype->Symbols);
			break;
		}
	}

	case AST_MapType:
		if (allowarraytypes)
		{
			Error(field, "%s: Map types not implemented yet", name.GetChars());
			// Todo: Decide what we allow here and if it makes sense to allow more complex constructs.
			auto mtype = static_cast<ZCC_MapType *>(ztype);
			return NewMap(DetermineType(outertype, field, name, mtype->KeyType, false), DetermineType(outertype, field, name, mtype->ValueType, false));
		}
		break;

	case AST_DynArrayType:
		if (allowarraytypes)
		{
			Error(field, "%s: Dynamic array types not implemented yet", name.GetChars());
			auto atype = static_cast<ZCC_DynArrayType *>(ztype);
			return NewDynArray(DetermineType(outertype, field, name, atype->ElementType, false));
		}
		break;

	case AST_ClassType:
	{
		auto ctype = static_cast<ZCC_ClassType *>(ztype);
		if (ctype->Restriction == nullptr)
		{
			return NewClassPointer(RUNTIME_CLASS(DObject));
		}
		else
		{
			auto sym = outertype->Symbols.FindSymbol(ctype->Restriction->Id, true);
			if (sym == nullptr) sym = GlobalSymbols.FindSymbol(ctype->Restriction->Id, false);
			if (sym == nullptr)
			{
				Error(field, "%s: Unknown identifier", FName(ctype->Restriction->Id).GetChars());
				return TypeError;
			}
			auto typesym = dyn_cast<PSymbolType>(sym);
			if (typesym == nullptr || !typesym->Type->IsKindOf(RUNTIME_CLASS(PClass)))
			{
				Error(field, "%s does not represent a class type", FName(ctype->Restriction->Id).GetChars());
				return TypeError;
			}
			return NewClassPointer(static_cast<PClass *>(typesym->Type));
		}
	}
	}
	return TypeError;
}

//==========================================================================
//
// ZCCCompiler :: ResolveUserType
//
// resolves a user type and returns a matching PType
//
//==========================================================================

PType *ZCCCompiler::ResolveUserType(ZCC_BasicType *type, PSymbolTable *symt)
{
	// Check the symbol table for the identifier.
	PSymbolTable *table;
	PSymbol *sym = symt->FindSymbolInTable(type->UserType->Id, table);
	// GlobalSymbols cannot be the parent of a class's symbol table so we have to look for global symbols explicitly.
	if (sym == nullptr && symt != &GlobalSymbols) sym = GlobalSymbols.FindSymbolInTable(type->UserType->Id, table);
	if (sym != nullptr && sym->IsKindOf(RUNTIME_CLASS(PSymbolType)))
	{
		auto type = static_cast<PSymbolType *>(sym)->Type;
		if (type->IsKindOf(RUNTIME_CLASS(PEnum)))
		{
			return TypeSInt32;	// hack this to an integer until we can resolve the enum mess.
		}
		return type;
	}
	return TypeError;
}


//==========================================================================
//
// ZCCCompiler :: ResolveArraySize
//
// resolves the array size and returns a matching type.
//
//==========================================================================

PType *ZCCCompiler::ResolveArraySize(PType *baseType, ZCC_Expression *arraysize, PSymbolTable *sym)
{
	// The duplicate Simplify call is necessary because if the head node gets replaced there is no way to detect the end of the list otherwise.
	arraysize = Simplify(arraysize, sym);
	ZCC_Expression *val;
	do
	{
		val = Simplify(arraysize, sym);
		if (val->Operation != PEX_ConstValue || !val->Type->IsA(RUNTIME_CLASS(PInt)))
		{
			Error(arraysize, "Array index must be an integer constant");
			return TypeError;
		}
		int size = static_cast<ZCC_ExprConstant *>(val)->IntVal;
		if (size < 1)
		{
			Error(arraysize, "Array size must be positive");
			return TypeError;
		}
		baseType = NewArray(baseType, size);
		val = static_cast<ZCC_Expression *>(val->SiblingNext);
	} while (val != arraysize);
	return baseType;
}

//==========================================================================
//
// ZCCCompiler :: GetInt - Input must be a constant expression
//
//==========================================================================

int ZCCCompiler::GetInt(ZCC_Expression *expr)
{
	if (expr->Type == TypeError)
	{
		return 0;
	}
	const PType::Conversion *route[CONVERSION_ROUTE_SIZE];
	int routelen = expr->Type->FindConversion(TypeSInt32, route, countof(route));
	if (routelen < 0)
	{
		Error(expr, "Cannot convert to integer");
		return 0;
	}
	else
	{
		if (expr->Type->IsKindOf(RUNTIME_CLASS(PFloat)))
		{
			Warn(expr, "Truncation of floating point value");
		}
		auto ex = static_cast<ZCC_ExprConstant *>(ApplyConversion(expr, route, routelen));
		return ex->IntVal;
	}
}

double ZCCCompiler::GetDouble(ZCC_Expression *expr)
{
	if (expr->Type == TypeError)
	{
		return 0;
	}
	const PType::Conversion *route[CONVERSION_ROUTE_SIZE];
	int routelen = expr->Type->FindConversion(TypeFloat64, route, countof(route));
	if (routelen < 0)
	{
		Error(expr, "Cannot convert to float");
		return 0;
	}
	else
	{
		auto ex = static_cast<ZCC_ExprConstant *>(ApplyConversion(expr, route, routelen));
		return ex->DoubleVal;
	}
}

const char *ZCCCompiler::GetString(ZCC_Expression *expr, bool silent)
{
	if (expr->Type == TypeError)
	{
		return 0;
	}
	else if (expr->Type->IsKindOf(RUNTIME_CLASS(PString)))
	{
		return static_cast<ZCC_ExprConstant *>(expr)->StringVal->GetChars();
	}
	else if (expr->Type->IsKindOf(RUNTIME_CLASS(PName)))
	{
		// Ugh... What a mess...
		return FName(ENamedName(static_cast<ZCC_ExprConstant *>(expr)->IntVal)).GetChars();
	}
	else
	{
		if (!silent) Error(expr, "Cannot convert to string");
		return 0;
	}
}

//==========================================================================
//
// Parses an actor property's parameters and calls the handler
//
//==========================================================================

void ZCCCompiler::DispatchProperty(FPropertyInfo *prop, ZCC_PropertyStmt *property, AActor *defaults, Baggage &bag)
{
	static TArray<FPropParam> params;
	static TArray<FString> strings;

	params.Clear();
	strings.Clear();
	params.Reserve(1);
	params[0].i = 0;
	if (prop->params[0] != '0')
	{
		if (property->Values == nullptr)
		{
			Error(property, "%s: arguments missing", prop->name);
			return;
		}
		property->Values = Simplify(property->Values, &bag.Info->Symbols);	// need to do this before the loop so that we can find the head node again.
		const char * p = prop->params;
		auto exp = property->Values;

		while (true)
		{
			FPropParam conv;
			FPropParam pref;

			if (exp->NodeType != AST_ExprConstant)
			{
				// If we get TypeError, there has already been a message from deeper down so do not print another one.
				if (exp->Type != TypeError) Error(exp, "%s: non-constant parameter", prop->name);
				return;
			}
			conv.s = nullptr;
			pref.s = nullptr;
			pref.i = -1;
			switch ((*p) & 223)
			{

			case 'X':	// Expression in parentheses or number. We only support the constant here. The function will have to be handled by a separate property to get past the parser.
				conv.i = GetInt(exp);
				params.Push(conv);
				conv.exp = nullptr;
				break;

			case 'I':
			case 'M':	// special case for morph styles in DECORATE . This expression-aware parser will not need this.
			case 'N':	// special case for thing activations in DECORATE. This expression-aware parser will not need this.
				conv.i = GetInt(exp);
				break;

			case 'F':
				conv.d = GetDouble(exp);
				break;

			case 'Z':	// an optional string. Does not allow any numerical value.
				if (!GetString(exp, true))
				{
					// apply this expression to the next argument on the list.
					params.Push(conv);
					params[0].i++;
					p++;
					continue;
				}
				// fall through

			case 'C':	// this parser accepts colors only in string form.
				pref.i = 1;
			case 'S':
				conv.s = GetString(exp);
				break;

			case 'T': // a filtered string
				conv.s = strings[strings.Reserve(1)] = strbin1(GetString(exp));
				break;

			case 'L':	// Either a number or a list of strings
				if (!GetString(exp, true))
				{
					pref.i = 0;
					conv.i = GetInt(exp);
				}
				else
				{
					pref.i = 1;
					params.Push(pref);
					params[0].i++;

					do
					{
						conv.s = GetString(exp);
						if (conv.s != nullptr)
						{
							params.Push(conv);
							params[0].i++;
						}
						exp = Simplify(static_cast<ZCC_Expression *>(exp->SiblingNext), &bag.Info->Symbols);
					} while (exp != property->Values);
					goto endofparm;
				}
				break;

			default:
				assert(false);
				break;

			}
			if (pref.i != -1)
			{
				params.Push(pref);
				params[0].i++;
			}
			params.Push(conv);
			params[0].i++;
			exp = Simplify(static_cast<ZCC_Expression *>(exp->SiblingNext), &bag.Info->Symbols);
		endofparm:
			p++;
			// Skip the DECORATE 'no comma' marker
			if (*p == '_') p++;

			if (*p == 0)
			{
				if (exp != property->Values)
				{
					Error(property, "Too many values for '%s'", prop->name);
					return;
				}
				break;
			}
			else if (exp == property->Values)
			{
				if (*p < 'a')
				{
					Error(property, "Insufficient parameters for %s", prop->name);
					return;
				}
				break;
			}
		}
	}
	// call the handler
	try
	{
		prop->Handler(defaults, bag.Info, bag, &params[0]);
	}
	catch (CRecoverableError &error)
	{
		Error(property, "%s", error.GetMessage());
	}
}

//==========================================================================
//
// Parses an actor property
//
//==========================================================================

void ZCCCompiler::ProcessDefaultProperty(PClassActor *cls, ZCC_PropertyStmt *prop, Baggage &bag)
{
	auto namenode = prop->Prop;
	FString propname;

	if (namenode->SiblingNext == namenode)
	{
		// a one-name property
		propname = FName(namenode->Id);
	}
	else if (namenode->SiblingNext->SiblingNext == namenode)
	{
		// a two-name property
		propname << FName(namenode->Id) << "." << FName(static_cast<ZCC_Identifier *>(namenode->SiblingNext)->Id);
	}
	else
	{
		Error(prop, "Property name may at most contain two parts");
		return;
	}

	FPropertyInfo *property = FindProperty(propname);

	if (property != nullptr && property->category != CAT_INFO)
	{
		if (cls->IsDescendantOf(*property->cls))
		{
			DispatchProperty(property, prop, (AActor *)bag.Info->Defaults, bag);
		}
		else
		{
			Error(prop, "'%s' requires an actor of type '%s'\n", propname.GetChars(), (*property->cls)->TypeName.GetChars());
		}
	}
	else
	{
		Error(prop, "'%s' is an unknown actor property\n", propname.GetChars());
	}
}

//==========================================================================
//
// Finds a flag and sets or clears it
//
//==========================================================================

void ZCCCompiler::ProcessDefaultFlag(PClassActor *cls, ZCC_FlagStmt *flg)
{
	auto namenode = flg->name;
	const char *n1 = FName(namenode->Id).GetChars(), *n2;

	if (namenode->SiblingNext == namenode)
	{
		// a one-name flag
		n2 = nullptr;
	}
	else if (namenode->SiblingNext->SiblingNext == namenode)
	{
		// a two-name flag
		n2 = FName(static_cast<ZCC_Identifier *>(namenode->SiblingNext)->Id).GetChars();
	}
	else
	{
		Error(flg, "Flag name may at most contain two parts");
		return;
	}

	auto fd = FindFlag(cls, n1, n2, true);
	if (fd != nullptr)
	{
		if (fd->structoffset == -1)
		{
			Warn(flg, "Deprecated flag '%s%s%s' used", n1, n2 ? "." : "", n2 ? n2 : "");
			HandleDeprecatedFlags((AActor*)cls->Defaults, cls, flg->set, fd->flagbit);
		}
		else
		{
			ModActorFlag((AActor*)cls->Defaults, fd, flg->set);
		}
	}
	else
	{
		Error(flg, "Unknown flag '%s%s%s'", n1, n2 ? "." : "", n2 ? n2 : "");
	}
}

//==========================================================================
//
// Parses the default list
//
//==========================================================================

void ZCCCompiler::InitDefaults()
{
	for (auto c : Classes)
	{
		// This may be removed if the conditions change, but right now only subclasses of Actor can define a Default block.
		if (!c->Type()->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			if (c->Defaults.Size()) Error(c->cls, "%s: Non-actor classes may not have defaults", c->Type()->TypeName.GetChars());
		}
		else
		{
			// This should never happen.
			if (c->Type()->Defaults != nullptr)
			{
				Error(c->cls, "%s already has defaults", c->Type()->TypeName.GetChars());
			}
			// This can only occur if a native parent is not initialized. In all other cases the sorting of the class list should prevent this from ever happening.
			else if (c->Type()->ParentClass->Defaults == nullptr && c->Type() != RUNTIME_CLASS(AActor))
			{
				Error(c->cls, "Parent class %s of %s is not initialized", c->Type()->ParentClass->TypeName.GetChars(), c->Type()->TypeName.GetChars());
			}
			else
			{
				// Copy the parent's defaults and meta data.
				auto ti = static_cast<PClassActor *>(c->Type());
				ti->InitializeNativeDefaults();
				ti->ParentClass->DeriveData(ti);


				Baggage bag;
			#ifdef _DEBUG
				bag.ClassName = c->Type()->TypeName;
			#endif
				bag.Info = ti;
				bag.DropItemSet = false;
				bag.StateSet = false;
				bag.fromZScript = true;
				bag.CurrentState = 0;
				bag.Lumpnum = c->cls->SourceLump;
				bag.DropItemList = nullptr;
				bag.ScriptPosition.StrictErrors = true;
				// The actual script position needs to be set per property.

				for (auto d : c->Defaults)
				{
					auto content = d->Content;
					if (content != nullptr) do
					{
						switch (content->NodeType)
						{
						case AST_PropertyStmt:
							bag.ScriptPosition.FileName = *content->SourceName;
							bag.ScriptPosition.ScriptLine = content->SourceLoc;
							ProcessDefaultProperty(ti, static_cast<ZCC_PropertyStmt *>(content), bag);
							break;

						case AST_FlagStmt:
							ProcessDefaultFlag(ti, static_cast<ZCC_FlagStmt *>(content));
							break;
						}
						content = static_cast<decltype(content)>(content->SiblingNext);
					} while (content != d->Content);
				}
				if (bag.DropItemSet)
				{
					bag.Info->SetDropItems(bag.DropItemList);
				}
			}
		}
	}
}

//==========================================================================
//
// Parses the functions list
//
//==========================================================================

void ZCCCompiler::InitFunctions()
{
	TArray<PType *> rets(1);
	TArray<PType *> args;
	TArray<uint32_t> argflags;
	TArray<FName> argnames;

	for (auto c : Classes)
	{
		for (auto f : c->Functions)
		{
			rets.Clear();
			args.Clear();
			argflags.Clear();
			// For the time being, let's not allow overloading. This may be reconsidered later but really just adds an unnecessary amount of complexity here.
			if (AddTreeNode(f->Name, f, &c->TreeNodes, false))
			{
				auto t = f->Type;
				if (t != nullptr)
				{
					do
					{
						auto type = DetermineType(c->Type(), f, f->Name, t, false);
						if (type->IsKindOf(RUNTIME_CLASS(PStruct)))
						{
							// structs and classes only get passed by pointer.
							type = NewPointer(type);
						}
						// TBD: disallow certain types? For now, let everything pass that isn't an array.
						rets.Push(type);
						t = static_cast<decltype(t)>(t->SiblingNext);
					} while (t != f->Type);
				}

				int notallowed = ZCC_Latent | ZCC_Meta | ZCC_ReadOnly | ZCC_FuncConst | ZCC_Abstract;

				if (f->Flags & notallowed)
				{
					Error(f, "Invalid qualifiers for %s (%s not allowed)", FName(f->Name).GetChars(), FlagsToString(f->Flags & notallowed));
					f->Flags &= notallowed;
				}
				uint32_t varflags = VARF_Method;
				AFuncDesc *afd = nullptr;

				// map to implementation flags.
				if (f->Flags & ZCC_Private) varflags |= VARF_Private;
				if (f->Flags & ZCC_Protected) varflags |= VARF_Protected;
				if (f->Flags & ZCC_Deprecated) varflags |= VARF_Deprecated;
				if (f->Flags & ZCC_Action) varflags |= VARF_Action|VARF_Final;	// Action implies Final.
				if (f->Flags & ZCC_Static) varflags = (varflags & ~VARF_Method) | VARF_Final;	// Static implies Final.
				if ((f->Flags & (ZCC_Action | ZCC_Static)) == (ZCC_Action | ZCC_Static))
				{
					Error(f, "%s: Action and Static on the same function is not allowed.", FName(f->Name).GetChars());
					varflags |= VARF_Method;
				}

				if (f->Flags & ZCC_Native)
				{
					varflags |= VARF_Native;
					afd = FindFunction(FName(f->Name).GetChars());
					if (afd == nullptr)
					{
						Error(f, "The function '%s' has not been exported from the executable.", FName(f->Name).GetChars());
					}
				}
				SetImplicitArgs(&args, &argflags, &argnames, c->Type(), varflags);
				auto p = f->Params;
				if (p != nullptr)
				{
					do
					{
						if (p->Type != nullptr)
						{
							auto type = DetermineType(c->Type(), p, f->Name, p->Type, false);
							int flags;
							if (p->Flags & ZCC_In) flags |= VARF_In;
							if (p->Flags & ZCC_Out) flags |= VARF_Out;
							if (p->Default != nullptr)
							{
								auto val = Simplify(p->Default, &c->Type()->Symbols);
								flags |= VARF_Optional;
								if (val->Operation != PEX_ConstValue)
								{
									Error(c->cls, "Default parameter %s is not constant in %s", FName(p->Name).GetChars(), FName(f->Name).GetChars());
								}
								// Todo: Store and handle the default value (native functions will discard it anyway but for scripted ones this should be done decently.)
							}
							// TBD: disallow certain types? For now, let everything pass that isn't an array.
							args.Push(type);
							argflags.Push(flags);
						}
						else
						{
							args.Push(nullptr);
							argflags.Push(0);
						}
						p = static_cast<decltype(p)>(p->SiblingNext);
					} while (p != f->Params);
				}

				PFunction *sym = new PFunction(c->Type(), f->Name);
				sym->AddVariant(NewPrototype(rets, args), argflags, argnames, afd == nullptr? nullptr : *(afd->VMPointer), varflags);
				c->Type()->Symbols.ReplaceSymbol(sym);

				// todo: Check inheritance.
				// todo: Process function bodies.
			}
		}
	}
}

//==========================================================================
//
// very complicated check for random duration.
//
//==========================================================================

static bool CheckRandom(ZCC_Expression *duration)
{
	if (duration->NodeType != AST_ExprFuncCall) return false;
	auto func = static_cast<ZCC_ExprFuncCall *>(duration);
	if (func->Function == nullptr) return false;
	if (func->Function->NodeType != AST_ExprID) return false;
	auto f2 = static_cast<ZCC_ExprID *>(func->Function);
	return f2->Identifier == NAME_Random;
}

//==========================================================================
//
// Sets up the action function call
//
//==========================================================================
FxExpression *ZCCCompiler::SetupActionFunction(PClassActor *cls, ZCC_TreeNode *af)
{
	// We have 3 cases to consider here:
	// 1. An action function without parameters. This can be called directly
	// 2. An action functon with parameters or a non-action function. This needs to be wrapped into a helper function to set everything up.
	// 3. An anonymous function.

	// 1. and 2. are exposed through AST_ExprFunctionCall
	if (af->NodeType == AST_ExprFuncCall)
	{
		auto fc = static_cast<ZCC_ExprFuncCall *>(af);
		assert(fc->Function->NodeType == AST_ExprID);
		auto id = static_cast<ZCC_ExprID *>(fc->Function);

		// We must skip ACS_NamedExecuteWithResult here, because this name both exists as an action function and as a builtin.
		// The code which gets called from here can easily make use of the builtin, so let's just pass this name without any checks.
		// The actual action function is still needed by DECORATE:
		if (id->Identifier != NAME_ACS_NamedExecuteWithResult)
		{
			PFunction *afd = dyn_cast<PFunction>(cls->Symbols.FindSymbol(id->Identifier, true));
			if (afd != nullptr)
			{
				if (fc->Parameters == nullptr && (afd->Variants[0].Flags & VARF_Action))
				{
					// We can use this function directly without wrapping it in a caller.
					return new FxVMFunctionCall(afd, nullptr, *af, true);
				}
			}
			else
			{
				// it may also be an action special so check that first before printing an error.
				if (!P_FindLineSpecial(FName(id->Identifier).GetChars()))
				{
					Error(af, "%s: action function not found in %s", FName(id->Identifier).GetChars(), cls->TypeName.GetChars());
					return nullptr;
				}
				// Action specials fall through to the code generator.
			}
		}
	}
	ConvertClass = cls;
	return ConvertAST(af);

	//Error(af, "Complex action functions not supported yet.");
	//return nullptr;

	/*
	bool hasfinalret;
	tcall->Code = ParseActions(sc, state, statestring, bag, hasfinalret);
	if (!hasfinalret && tcall->Code != nullptr)
	{
	static_cast<FxSequence *>(tcall->Code)->Add(new FxReturnStatement(nullptr, sc));
	}
	*/

}

//==========================================================================
//
// Compile the states
//
//==========================================================================

void ZCCCompiler::CompileStates()
{
	for (auto c : Classes)
	{
		if (!c->Type()->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			Error(c->cls, "%s: States can only be defined for actors.", c->Type()->TypeName.GetChars());
			continue;
		}
		FString statename;	// The state builder wants the label as one complete string, not separated into tokens.
		FStateDefinitions statedef;
		statedef.MakeStateDefines(dyn_cast<PClassActor>(c->Type()->ParentClass));

		for (auto s : c->States)
		{
			auto st = s->Body;
			if (st != nullptr) do
			{
				switch (st->NodeType)
				{
				case AST_StateLabel:
				{
					auto sl = static_cast<ZCC_StateLabel *>(st);
					statename = FName(sl->Label);
					statedef.AddStateLabel(statename);
					break;
				}
				case AST_StateLine:
				{
					auto sl = static_cast<ZCC_StateLine *>(st);
					FState state;
					memset(&state, 0, sizeof(state));
					if (sl->Sprite->Len() != 4)
					{
						Error(sl, "Sprite name must be exactly 4 characters. Found '%s'", sl->Sprite->GetChars());
					}
					else
					{
						state.sprite = GetSpriteIndex(sl->Sprite->GetChars());
					}
					// It is important to call CheckRandom before Simplify, because Simplify will resolve the function's name to nonsense
					// and there is little point fixing it because it is essentially useless outside of resolving constants.
					if (CheckRandom(sl->Duration))
					{
						auto func = static_cast<ZCC_ExprFuncCall *>(Simplify(sl->Duration, &c->Type()->Symbols));
						if (func->Parameters == func->Parameters->SiblingNext || func->Parameters != func->Parameters->SiblingNext->SiblingNext)
						{
							Error(sl, "Random duration requires exactly 2 parameters");
						}
						int v1 = GetInt(func->Parameters->Value);
						int v2 = GetInt(static_cast<ZCC_FuncParm *>(func->Parameters->SiblingNext)->Value);
						if (v1 > v2) std::swap(v1, v2);
						state.Tics = (int16_t)clamp<int>(v1, 0, INT16_MAX);
						state.TicRange = (uint16_t)clamp<int>(v2 - v1, 0, UINT16_MAX);
					}
					else
					{
						auto duration = Simplify(sl->Duration, &c->Type()->Symbols);
						if (duration->Operation == PEX_ConstValue)
						{
							state.Tics = (int16_t)clamp<int>(GetInt(duration), -1, INT16_MAX);
							state.TicRange = 0;
						}
						else
						{
							Error(sl, "Duration is not a constant");
						}
					}
					state.Fullbright = sl->bBright;
					state.Fast = sl->bFast;
					state.Slow = sl->bSlow;
					state.CanRaise = sl->bCanRaise;
					if ((state.NoDelay = sl->bNoDelay))
					{
						if (statedef.GetStateLabelIndex(NAME_Spawn) != statedef.GetStateCount())
						{
							Warn(sl, "NODELAY only has an effect on the first state after 'Spawn:'");
						}
					}
					if (sl->Offset != nullptr)
					{
						auto o1 = static_cast<ZCC_Expression *>(Simplify(sl->Offset, &c->Type()->Symbols));
						auto o2 = static_cast<ZCC_Expression *>(Simplify(static_cast<ZCC_Expression *>(o1->SiblingNext), &c->Type()->Symbols));

						if (o1->Operation != PEX_ConstValue || o2->Operation != PEX_ConstValue)
						{
							Error(o1, "State offsets must be constant");
						}
						else
						{
							state.Misc1 = GetInt(o1);
							state.Misc2 = GetInt(o2);
						}
					}
#ifdef DYNLIGHT
					if (sl->Lights != nullptr)
					{
						auto l = sl->Lights;
						do
						{
							AddStateLight(&state, GetString(l));
							l = static_cast<decltype(l)>(l->SiblingNext);
						} while (l != sl->Lights);
					}
#endif

					if (sl->Action != nullptr)
					{
						auto code = SetupActionFunction(static_cast<PClassActor *>(c->Type()), sl->Action);
						if (code != nullptr)
						{
							auto funcsym = CreateAnonymousFunction(c->Type(), nullptr, VARF_Method | VARF_Action);
							state.ActionFunc = FunctionBuildList.AddFunction(funcsym, code, FStringf("%s.StateFunction.%d", c->Type()->TypeName.GetChars(), statedef.GetStateCount()), false);
						}
					}

					int count = statedef.AddStates(&state, sl->Frames->GetChars());
					if (count < 0)
					{
						Error(sl, "Invalid frame character string '%s'", sl->Frames->GetChars());
						count = -count;
					}
					break;
				}
				case AST_StateGoto:
				{
					auto sg = static_cast<ZCC_StateGoto *>(st);
					statename = "";
					if (sg->Qualifier != nullptr)
					{
						statename << FName(sg->Qualifier->Id) << "::";
					}
					auto part = sg->Label;
					do
					{
						statename << FName(part->Id) << '.';
						part = static_cast<decltype(part)>(part->SiblingNext);
					} while (part != sg->Label);
					statename.Truncate((long)statename.Len() - 1);	// remove the last '.' in the label name
					if (sg->Offset != nullptr)
					{
						auto ofs = Simplify(sg->Offset, &c->Type()->Symbols);
						if (ofs->Operation != PEX_ConstValue)
						{
							Error(sg, "Constant offset expected for GOTO");
						}
						else
						{
							int offset = GetInt(ofs);
							if (offset < 0)
							{
								Error(sg, "GOTO offset must be positive");
								offset = 0;
							}
							if (offset > 0)
							{
								statename.AppendFormat("+%d", offset);
							}
						}
					}
					if (!statedef.SetGotoLabel(statename))
					{
						Error(sg, "GOTO before first state");
					}
					break;
				}
				case AST_StateFail:
				case AST_StateWait:
					if (!statedef.SetWait())
					{
						Error(st, "%s before first state", st->NodeType == AST_StateFail ? "Fail" : "Wait");
						continue;
					}
					break;

				case AST_StateLoop:
					if (!statedef.SetLoop())
					{
						Error(st, "LOOP before first state");
						continue;
					}
					break;

				case AST_StateStop:
					if (!statedef.SetStop())
					{
						Error(st, "STOP before first state");
					}
					break;

				default:
					assert(0 && "Bad AST node in state");
				}
				st = static_cast<decltype(st)>(st->SiblingNext);
			} while (st != s->Body);
		}
		try
		{
			static_cast<PClassActor *>(c->Type())->Finalize(statedef);
		}
		catch (CRecoverableError &err)
		{
			Error(c->cls, "%s", err.GetMessage());
		}
	}
}

//==========================================================================
//
// Convert the AST data for the code generator.
//
//==========================================================================

FxExpression *ZCCCompiler::ConvertAST(ZCC_TreeNode *ast)
{
	// FxReturnStatement will have to be done more intelligently, of course.
	return new FxReturnStatement(ConvertNode(ast), *ast);
}

FxExpression *ZCCCompiler::ConvertNode(ZCC_TreeNode *ast)
{
	// Note: Do not call 'Simplify' here because that function tends to destroy identifiers due to lack of context in which to resolve them.
	// The Fx nodes created here will be better suited for that.
	switch (ast->NodeType)
	{
	case AST_ExprFuncCall:
	{
		auto fcall = static_cast<ZCC_ExprFuncCall *>(ast);
		assert(fcall->Function->NodeType == AST_ExprID);	// of course this cannot remain. Right now nothing more complex can come along but later this will have to be decomposed into 'self' and the actual function name.
		auto fname = static_cast<ZCC_ExprID *>(fcall->Function)->Identifier;
		return new FxFunctionCall(fname, ConvertNodeList(fcall->Parameters), *ast);
		//return ConvertFunctionCall(fcall->Function, ConvertNodeList(fcall->Parameters), ConvertClass, *ast);
	}

	case AST_FuncParm:
	{
		auto fparm = static_cast<ZCC_FuncParm *>(ast);
		// ignore the label for now, that's stuff for far later, when a bit more here is working.
		return ConvertNode(fparm->Value);
	}

	case AST_ExprID:
	{
		auto id = static_cast<ZCC_ExprID *>(ast);
		return new FxIdentifier(id->Identifier, *ast);
	}

	case AST_ExprConstant:
	{
		auto cnst = static_cast<ZCC_ExprConstant *>(ast);
		if (cnst->Type->IsA(RUNTIME_CLASS(PName)))
		{
			return new FxConstant(FName(ENamedName(cnst->IntVal)), *ast);
		}
		else if (cnst->Type->IsA(RUNTIME_CLASS(PInt)))
		{
			return new FxConstant(cnst->IntVal, *ast);
		}
		else if (cnst->Type->IsA(RUNTIME_CLASS(PFloat)))
		{
			return new FxConstant(cnst->DoubleVal, *ast);
		}
		else if (cnst->Type->IsA(RUNTIME_CLASS(PString)))
		{
			return new FxConstant(*cnst->StringVal, *ast);
		}
		else
		{
			// can there be other types?
			Error(cnst, "Unknown constant type");
			return new FxConstant(0, *ast);
		}
	}

	default:
		// only for development. I_Error is more convenient here than a normal error.
		I_Error("ConvertNode encountered unsupported node of type %d", ast->NodeType);
		return nullptr;
	}
}


FArgumentList *ZCCCompiler::ConvertNodeList(ZCC_TreeNode *head)
{
	FArgumentList *list = new FArgumentList;
	auto node = head;
	do
	{
		list->Push(ConvertNode(node));
		node = node->SiblingNext;
	} while (node != head);
	return list;
}