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

//==========================================================================
//
// ZCCCompiler Constructor
//
//==========================================================================

ZCCCompiler::ZCCCompiler(ZCC_AST &ast, DObject *_outer, PSymbolTable &_symbols)
: Outer(_outer), Symbols(_symbols), AST(ast), ErrorCount(0), WarnCount(0)
{
	// Group top-level nodes by type
	if (ast.TopNode != NULL)
	{
		ZCC_TreeNode *node = ast.TopNode;
		do
		{
			switch (node->NodeType)
			{
			case AST_Class:			Classes.Push(static_cast<ZCC_Class *>(node));			break;
			case AST_Struct:		Structs.Push(static_cast<ZCC_Struct *>(node));			break;
			case AST_Enum:			break;
			case AST_EnumTerminator:break;
			case AST_ConstantDef:	Constants.Push(static_cast<ZCC_ConstantDef *>(node));	break;
			default:				assert(0 && "Unhandled AST node type");					break;
			}
			node = node->SiblingNext;
		}
		while (node != ast.TopNode);
	}
}

//==========================================================================
//
// ZCCCompiler :: Message
//
// Prints a warning or error message, and increments the appropriate
// counter.
//
//==========================================================================

void ZCCCompiler::Message(ZCC_TreeNode *node, EZCCError errnum, const char *msg, ...)
{
	FString composed;

	composed.Format("%s%s, line %d: ",
		errnum & ZCCERR_ERROR ? TEXTCOLOR_RED : TEXTCOLOR_ORANGE,
		node->SourceName->GetChars(), node->SourceLoc);

	va_list argptr;
	va_start(argptr, msg);
	composed.VAppendFormat(msg, argptr);
	va_end(argptr);

	composed += '\n';
	PrintString(PRINT_HIGH, composed);

	if (errnum & ZCCERR_ERROR)
	{
		ErrorCount++;
	}
	else
	{
		WarnCount++;
	}
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
	CompileConstants();
	return ErrorCount;
}

//==========================================================================
//
// ZCCCompiler :: CompileConstants
//
// Make symbols from every constant defined at this level.
//
//==========================================================================

void ZCCCompiler::CompileConstants()
{
	for (unsigned i = 0; i < Constants.Size(); ++i)
	{
		ZCC_ConstantDef *def = Constants[i];
		if (def->Symbol == NULL)
		{
			PSymbolConst *sym = CompileConstant(def);
			if (NULL == Symbols.AddSymbol(sym))
			{
				Message(Constants[i], ERR_symbol_redefinition, "Redefinition of symbol '%s'", FName(def->Name).GetChars());
			}
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

	def->Symbol = (PSymbolConst *)(void *)1;	// mark as being defined (avoid recursion)
	ZCC_Expression *val = Simplify(def->Value);
	def->Value = val;
	PSymbolConst *sym = NULL;
	if (val->NodeType == AST_ExprConstant)
	{
		ZCC_ExprConstant *cval = static_cast<ZCC_ExprConstant *>(val);
		if (cval->Type == TypeString)
		{
			sym = new PSymbolConstString(def->Name, *(cval->StringVal));
		}
		else if (cval->Type->IsA(RUNTIME_CLASS(PInt)))
		{
			sym = new PSymbolConstNumeric(def->Name, cval->Type, cval->IntVal);
		}
		else if (cval->Type->IsA(RUNTIME_CLASS(PFloat)))
		{
			sym = new PSymbolConstNumeric(def->Name, cval->Type, cval->DoubleVal);
		}
		else
		{
			Message(def->Value, ERR_bad_const_def_type, "Bad type for constant definiton");
		}
	}
	else
	{
		Message(def->Value, ERR_const_def_not_constant, "Constant definition requires a constant value");
	}
	if (sym == NULL)
	{
		// Create a dummy constant so we don't make any undefined value warnings.
		sym = new PSymbolConstNumeric(def->Name, TypeError, 0);
	}
	def->Symbol = sym;
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
	if (root->Operation == PEX_ID)
	{
		return IdentifyIdentifier(static_cast<ZCC_ExprID *>(root));
	}
	else if (IsUnaryOp(root->Operation))
	{
		return SimplifyUnary(static_cast<ZCC_ExprUnary *>(root));
	}
	else if (IsBinaryOp(root->Operation))
	{
		return SimplifyBinary(static_cast<ZCC_ExprBinary *>(root));
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
	// First things first: Check the symbol table.
	PSymbol *sym;
	if (NULL != (sym = Symbols.FindSymbol(idnode->Identifier, true)))
	{
		if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConst)))
		{
			return NodeFromSymbolConst(static_cast<PSymbolConst *>(sym), idnode);
		}
	}
	else
	{
	}
	// Identifier didn't refer to anything good, so type error it.
	idnode->Type = TypeError;
	idnode->Operation = PEX_Nil;
	return idnode;
}

//==========================================================================
//
// ZCCCompiler :: NodeFromSymoblConst
//
// Returns a new AST constant node with the symbol's content.
//
//==========================================================================

ZCC_ExprConstant *ZCCCompiler::NodeFromSymbolConst(PSymbolConst *sym, ZCC_ExprID *idnode)
{
	ZCC_ExprConstant *val = static_cast<ZCC_ExprConstant *>(AST.InitNode(sizeof(*val), AST_ExprConstant, idnode));
	val->Operation = PEX_ConstValue;
	if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConstString)))
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
