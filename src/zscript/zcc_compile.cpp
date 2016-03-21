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
#include "gdtoa.h"

#define DEFINING_CONST ((PSymbolConst *)(void *)1)

//==========================================================================
//
// ZCCCompiler Constructor
//
//==========================================================================

ZCCCompiler::ZCCCompiler(ZCC_AST &ast, DObject *_outer, PSymbolTable &_symbols)
: Outer(_outer), Symbols(&_symbols), AST(ast), ErrorCount(0), WarnCount(0)
{
	// Group top-level nodes by type
	if (ast.TopNode != NULL)
	{
		ZCC_TreeNode *node = ast.TopNode;
		do
		{
			switch (node->NodeType)
			{
			case AST_Class:
			case AST_Struct:
			case AST_ConstantDef:
				if (AddNamedNode(static_cast<ZCC_NamedNode *>(node)))
				{
					switch (node->NodeType)
					{
					case AST_Class:			Classes.Push(static_cast<ZCC_Class *>(node));			break;
					case AST_Struct:		Structs.Push(static_cast<ZCC_Struct *>(node));			break;
					case AST_ConstantDef:	Constants.Push(static_cast<ZCC_ConstantDef *>(node));	break;
					default: assert(0 && "Default case is just here to make GCC happy. It should never be reached");
					}
				}
				break;

			case AST_Enum:			break;
			case AST_EnumTerminator:break;

			default:
				assert(0 && "Unhandled AST node type");
				break;
			}
			node = node->SiblingNext;
		}
		while (node != ast.TopNode);
	}
}

//==========================================================================
//
// ZCCCompiler :: AddNamedNode
//
// Keeps track of definition nodes by their names. Ensures that all names
// in this scope are unique.
//
//==========================================================================

bool ZCCCompiler::AddNamedNode(ZCC_NamedNode *node)
{
	FName name = node->NodeName;
	PSymbol *check = Symbols->FindSymbol(name, false);
	if (check != NULL)
	{
		assert(check->IsA(RUNTIME_CLASS(PSymbolTreeNode)));
		Error(node, "Attempt to redefine '%s'", name.GetChars());
		Error(static_cast<PSymbolTreeNode *>(check)->Node, " Original definition is here");
		return false;
	}
	else
	{
		Symbols->AddSymbol(new PSymbolTreeNode(name, node));
		return true;
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
	CompileConstants(Constants);
	return ErrorCount;
}

//==========================================================================
//
// ZCCCompiler :: CompileConstants
//
// Make symbols from every constant defined at this level.
//
//==========================================================================

void ZCCCompiler::CompileConstants(const TArray<ZCC_ConstantDef *> &defs)
{
	for (unsigned i = 0; i < defs.Size(); ++i)
	{
		ZCC_ConstantDef *def = defs[i];
		if (def->Symbol == NULL)
		{
			PSymbolConst *sym = CompileConstant(def);
		}
	}
}

//==========================================================================
//
// ZCCCompiler :: CompileConstant
//
// For every constant definition, evaluate its value (which should result
// in a constant), and create a symbol for it. Simplify() uses recursion
// to resolve constants used before their declarations.
//
//==========================================================================

PSymbolConst *ZCCCompiler::CompileConstant(ZCC_ConstantDef *def)
{
	assert(def->Symbol == NULL);

	def->Symbol = DEFINING_CONST;	// avoid recursion
	ZCC_Expression *val = Simplify(def->Value);
	def->Value = val;
	PSymbolConst *sym = NULL;
	if (val->NodeType == AST_ExprConstant)
	{
		ZCC_ExprConstant *cval = static_cast<ZCC_ExprConstant *>(val);
		if (cval->Type == TypeString)
		{
			sym = new PSymbolConstString(def->NodeName, *(cval->StringVal));
		}
		else if (cval->Type->IsA(RUNTIME_CLASS(PInt)))
		{
			sym = new PSymbolConstNumeric(def->NodeName, cval->Type, cval->IntVal);
		}
		else if (cval->Type->IsA(RUNTIME_CLASS(PFloat)))
		{
			sym = new PSymbolConstNumeric(def->NodeName, cval->Type, cval->DoubleVal);
		}
		else
		{
			Error(def->Value, "Bad type for constant definiton");
		}
	}
	else
	{
		Error(def->Value, "Constant definition requires a constant value");
	}
	if (sym == NULL)
	{
		// Create a dummy constant so we don't make any undefined value warnings.
		sym = new PSymbolConstNumeric(def->NodeName, TypeError, 0);
	}
	def->Symbol = sym;
	Symbols->ReplaceSymbol(sym);
	return sym;
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

ZCC_Expression *ZCCCompiler::Simplify(ZCC_Expression *root)
{
	if (root->NodeType == AST_ExprUnary)
	{
		return SimplifyUnary(static_cast<ZCC_ExprUnary *>(root));
	}
	else if (root->NodeType == AST_ExprBinary)
	{
		return SimplifyBinary(static_cast<ZCC_ExprBinary *>(root));
	}
	else if (root->Operation == PEX_ID)
	{
		return IdentifyIdentifier(static_cast<ZCC_ExprID *>(root));
	}
	else if (root->Operation == PEX_MemberAccess)
	{
		return SimplifyMemberAccess(static_cast<ZCC_ExprMemberAccess *>(root));
	}
	else if (root->Operation == PEX_FuncCall)
	{
		return SimplifyFunctionCall(static_cast<ZCC_ExprFuncCall *>(root));
	}
	return root;
}

//==========================================================================
//
// ZCCCompiler :: SimplifyUnary
//
//==========================================================================

ZCC_Expression *ZCCCompiler::SimplifyUnary(ZCC_ExprUnary *unary)
{
	unary->Operand = Simplify(unary->Operand);
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

ZCC_Expression *ZCCCompiler::SimplifyBinary(ZCC_ExprBinary *binary)
{
	binary->Left = Simplify(binary->Left);
	binary->Right = Simplify(binary->Right);
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

ZCC_Expression *ZCCCompiler::SimplifyMemberAccess(ZCC_ExprMemberAccess *dotop)
{
	dotop->Left = Simplify(dotop->Left);

	if (dotop->Left->Operation == PEX_TypeRef)
	{ // Type refs can be evaluated now.
		PType *ref = static_cast<ZCC_ExprTypeRef *>(dotop->Left)->RefType;
		PSymbolTable *symtable;
		PSymbol *sym = ref->Symbols.FindSymbolInTable(dotop->Right, symtable);
		if (sym == NULL)
		{
			Error(dotop, "'%s' is not a valid member", FName(dotop->Right).GetChars());
		}
		else
		{
			ZCC_Expression *expr = NodeFromSymbol(sym, dotop, symtable);
			if (expr == NULL)
			{
				Error(dotop, "Unhandled symbol type encountered");
			}
			else
			{
				return expr;
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

ZCC_Expression *ZCCCompiler::SimplifyFunctionCall(ZCC_ExprFuncCall *callop)
{
	ZCC_FuncParm *parm;
	int parmcount = 0;

	callop->Function = Simplify(callop->Function);
	parm = callop->Parameters;
	if (parm != NULL)
	{
		do
		{
			parmcount++;
			assert(parm->NodeType == AST_FuncParm);
			parm->Value = Simplify(parm->Value);
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

ZCC_Expression *ZCCCompiler::IdentifyIdentifier(ZCC_ExprID *idnode)
{
	// Check the symbol table for the identifier.
	PSymbolTable *table;
	PSymbol *sym = Symbols->FindSymbolInTable(idnode->Identifier, table);
	if (sym != NULL)
	{
		ZCC_Expression *node = NodeFromSymbol(sym, idnode, table);
		if (node != NULL)
		{
			return node;
		}
	}
	else
	{
		Error(idnode, "Unknown identifier '%s'", FName(idnode->Identifier).GetChars());
	}
	// Identifier didn't refer to anything good, so type error it.
	idnode->ToErrorNode();
	return idnode;
}

//==========================================================================
//
// ZCCCompiler :: CompileNode
//
//==========================================================================

PSymbol *ZCCCompiler::CompileNode(ZCC_NamedNode *node)
{
	assert(node != NULL);
	if (node->NodeType == AST_ConstantDef)
	{
		ZCC_ConstantDef *def = static_cast<ZCC_ConstantDef *>(node);
		PSymbolConst *sym = def->Symbol;

		if (sym == DEFINING_CONST)
		{
			Error(node, "Definition of '%s' is infinitely recursive", FName(node->NodeName).GetChars());
			sym = NULL;
		}
		else
		{
			assert(sym == NULL);
			sym = CompileConstant(def);
		}
		return sym;
	}
	else if (node->NodeType == AST_Struct)
	{

	}
	return NULL;
}

//==========================================================================
//
// ZCCCompiler :: NodeFromSymbol
//
//==========================================================================

ZCC_Expression *ZCCCompiler::NodeFromSymbol(PSymbol *sym, ZCC_Expression *source, PSymbolTable *table)
{
	assert(sym != NULL);
	if (sym->IsA(RUNTIME_CLASS(PSymbolTreeNode)))
	{
		PSymbolTable *prevtable = Symbols;
		Symbols = table;
		sym = CompileNode(static_cast<PSymbolTreeNode *>(sym)->Node);
		Symbols = prevtable;
		if (sym == NULL)
		{
			return NULL;
		}
	}
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
