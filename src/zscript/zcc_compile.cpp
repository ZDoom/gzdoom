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

#include "dobject.h"
#include "sc_man.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "m_alloc.h"
#include "zcc_parser.h"
#include "zcc_compile.h"
#include "v_text.h"
#include "p_lnspec.h"
#include "gdtoa.h"

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

		// todo
		case AST_States:
		case AST_FuncDeclarator:
		case AST_Default:
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
	: Outer(_outer), GlobalTreeNodes(&_symbols), OutputSymbols(&_outsymbols), AST(ast), ErrorCount(0), WarnCount(0)
{
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

	WarnCount++;
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

	ErrorCount++;
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
	return ErrorCount;
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

		PType *fieldtype = DetermineType(type, field, field->Type, true);

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

PType *ZCCCompiler::DetermineType(PType *outertype, ZCC_VarDeclarator *field, ZCC_Type *ztype, bool allowarraytypes)
{
	if (!allowarraytypes && ztype->ArraySize != nullptr)
	{
		Error(field, "%s: Array type not allowed", FName(field->Names->Name).GetChars());
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
			Error(field, "vector<4> not implemented for %s", FName(field->Names->Name).GetChars());
			return TypeError;

		case ZCC_UserType:
			return ResolveUserType(btype, &outertype->Symbols);
			break;
		}
	}

	case AST_MapType:
		if (allowarraytypes)
		{
			Error(field, "%s: Map types not implemented yet", FName(field->Names->Name).GetChars());
			// Todo: Decide what we allow here and if it makes sense to allow more complex constructs.
			auto mtype = static_cast<ZCC_MapType *>(ztype);
			return NewMap(DetermineType(outertype, field, mtype->KeyType, false), DetermineType(outertype, field, mtype->ValueType, false));
		}
		break;

	case AST_DynArrayType:
		if (allowarraytypes)
		{
			Error(field, "%s: Dynamic array types not implemented yet", FName(field->Names->Name).GetChars());
			auto atype = static_cast<ZCC_DynArrayType *>(ztype);
			return NewDynArray(DetermineType(outertype, field, atype->ElementType, false));
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
