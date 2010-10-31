#include "dobject.h"
#include "sc_man.h"
#include "memarena.h"
#include "zcc_parser.h"
#include "zcc-parse.h"

extern void (* const TreeNodePrinter[NUM_AST_NODE_TYPES])(FString &, ZCC_TreeNode *);

static const char *BuiltInTypeNames[] =
{
	"sint8", "uint8",
	"sint16", "uint16",
	"sint32", "uint32",
	"intauto",

	"bool",
	"float32", "float64", "floatauto",
	"string",
	"vector2",
	"vector3",
	"vector4",
	"name",
	"usertype"
};

static void PrintNode(FString &out, ZCC_TreeNode *node)
{
	assert(TreeNodePrinter[NUM_AST_NODE_TYPES-1] != NULL);
	if (node->NodeType >= 0 && node->NodeType < NUM_AST_NODE_TYPES)
	{
		TreeNodePrinter[node->NodeType](out, node);
	}
	else
	{
		out.AppendFormat("(unknown-node-type-%d)", node->NodeType);
	}
}

static void PrintNodes(FString &out, ZCC_TreeNode *node, char addchar=' ')
{
	ZCC_TreeNode *p;

	if (node == NULL)
	{
		out << "nil";
	}
	else
	{
		out << '(';
		p = node;
		do
		{
			PrintNode(out, p);
			p = p->SiblingNext;
			if (p != node)
			{
				out << ' ';
			}
		} while (p != node);
		out << ')';
	}
	if (addchar != '\0')
	{
		out << addchar;
	}
}

static void PrintBuiltInType(FString &out, EZCCBuiltinType type, bool addspace)
{
	assert(ZCC_NUM_BUILT_IN_TYPES == countof(BuiltInTypeNames));
	if (unsigned(type) >= unsigned(ZCC_NUM_BUILT_IN_TYPES))
	{
		out.AppendFormat("bad-type-%u", type);
	}
	else
	{
		out << BuiltInTypeNames[type];
	}
	if (addspace)
	{
		out << ' ';
	}
}

static void PrintIdentifier(FString &out, ZCC_TreeNode *node)
{
	ZCC_Identifier *inode = (ZCC_Identifier *)node;
	out.AppendFormat("(identifier '%s')", FName(inode->Id).GetChars());
}

static void PrintStringConst(FString &out, FString str)
{
	out << '"';
	for (size_t i = 0; i < str.Len(); ++i)
	{
		if (str[i] == '"')
		{
			out << "\"";
		}
		else if (str[i] == '\\')
		{
			out << "\\\\";
		}
		else if (str[i] >= 32)
		{
			out << str[i];
		}
		else
		{
			out.AppendFormat("\\x%02X", str[i]);
		}
	}
}

static void PrintClass(FString &out, ZCC_TreeNode *node)
{
	ZCC_Class *cnode = (ZCC_Class *)node;
	out << "(class ";
	PrintNodes(out, cnode->ClassName);
	PrintNodes(out, cnode->ParentName);
	PrintNodes(out, cnode->Replaces);
	out.AppendFormat("%08x ", cnode->Flags);
	PrintNodes(out, cnode->Body, ')');
}

static void PrintStruct(FString &out, ZCC_TreeNode *node)
{
	ZCC_Struct *snode = (ZCC_Struct *)node;
	out.AppendFormat("(struct '%s' ", FName(snode->StructName).GetChars());
	PrintNodes(out, snode->Body, ')');
}

static void PrintEnum(FString &out, ZCC_TreeNode *node)
{
	ZCC_Enum *enode = (ZCC_Enum *)node;
	out.AppendFormat("(enum '%s' ", FName(enode->EnumName).GetChars());
	PrintBuiltInType(out, enode->EnumType, true);
	PrintNodes(out, enode->Elements, ')');
}

static void PrintEnumNode(FString &out, ZCC_TreeNode *node)
{
	ZCC_EnumNode *enode = (ZCC_EnumNode *)node;
	out.AppendFormat("(enum-node '%s' ", FName(enode->ElemName));
	PrintNodes(out, enode->ElemValue, ')');
}

static void PrintStates(FString &out, ZCC_TreeNode *node)
{
	ZCC_States *snode = (ZCC_States *)node;
	out << "(states ";
	PrintNodes(out, snode->Body, ')');
}

static void PrintStatePart(FString &out, ZCC_TreeNode *node)
{
	out << "(state-part)";
}

static void PrintStateLabel(FString &out, ZCC_TreeNode *node)
{
	ZCC_StateLabel *snode = (ZCC_StateLabel *)node;
	out.AppendFormat("(state-label '%s')", FName(snode->Label).GetChars());
}

static void PrintStateStop(FString &out, ZCC_TreeNode *node)
{
	out << "(state-stop)";
}

static void PrintStateWait(FString &out, ZCC_TreeNode *node)
{
	out << "(state-wait)";
}

static void PrintStateFail(FString &out, ZCC_TreeNode *node)
{
	out << "(state-fail)";
}

static void PrintStateLoop(FString &out, ZCC_TreeNode *node)
{
	out << "(state-loop)";
}

static void PrintStateGoto(FString &out, ZCC_TreeNode *node)
{
	ZCC_StateGoto *snode = (ZCC_StateGoto *)node;
	out << "(state-goto ";
	PrintNodes(out, snode->Label);
	PrintNodes(out, snode->Offset, ')');
}

static void PrintStateLine(FString &out, ZCC_TreeNode *node)
{
	ZCC_StateLine *snode = (ZCC_StateLine *)node;
	out.AppendFormat("(state-line %c%c%c%c %s %s ",
		snode->Sprite[0], snode->Sprite[1], snode->Sprite[2], snode->Sprite[3],
		snode->bBright ? "bright " : "",
		snode->Frames->GetChars());
	PrintNodes(out, snode->Offset);
	PrintNodes(out, snode->Action, ')');
}

static void PrintVarName(FString &out, ZCC_TreeNode *node)
{
	ZCC_VarName *vnode = (ZCC_VarName *)node;
	out.AppendFormat("(var-name '%s')", FName(vnode->Name).GetChars());
}

static void PrintType(FString &out, ZCC_TreeNode *node)
{
	ZCC_Type *tnode = (ZCC_Type *)node;
	out << "(bad-type ";
	PrintNodes(out, tnode->ArraySize, ')');
}

static void PrintBasicType(FString &out, ZCC_TreeNode *node)
{
	ZCC_BasicType *tnode = (ZCC_BasicType *)node;
	out << "(basic-type ";
	PrintNodes(out, tnode->ArraySize);
	PrintBuiltInType(out, tnode->Type, true);
	PrintNodes(out, tnode->UserType, ')');
}

static void PrintMapType(FString &out, ZCC_TreeNode *node)
{
	ZCC_MapType *tnode = (ZCC_MapType *)node;
	out << "(map-type ";
	PrintNodes(out, tnode->ArraySize);
	PrintNodes(out, tnode->KeyType);
	PrintNodes(out, tnode->ValueType, ')');
}

static void PrintDynArrayType(FString &out, ZCC_TreeNode *node)
{
	ZCC_DynArrayType *tnode = (ZCC_DynArrayType *)node;
	out << "(dyn-array-type ";
	PrintNodes(out, tnode->ArraySize);
	PrintNodes(out, tnode->ElementType, ')');
}

static void PrintClassType(FString &out, ZCC_TreeNode *node)
{
	ZCC_ClassType *tnode = (ZCC_ClassType *)node;
	out << "(class-type ";
	PrintNodes(out, tnode->ArraySize);
	PrintNodes(out, tnode->Restriction, ')');
}

static void PrintExprType(FString &out, EZCCExprType type)
{
	static const char *const types[] =
	{
		"nil ",
		"id ",
		"super ",
		"self ",
		"string-const ",
		"int-const ",
		"uint-const ",
		"float-const ",
		"func-call ",
		"array-access ",
		"member-access ",
		"post-inc ",
		"post-dec ",
		"pre-inc ",
		"pre-dec ",
		"negate ",
		"anti-negate ",
		"bit-not ",
		"bool-not ",
		"size-of ",
		"align-of ",
		"add ",
		"sub ",
		"mul ",
		"div ",
		"mod ",
		"pow ",
		"cross-product ",
		"dot-product ",
		"left-shift ",
		"right-shift ",
		"concat ",
		"lt ",
		"gt ",
		"lteq ",
		"gteq ",
		"ltgteq ",
		"is ",
		"eqeq ",
		"neq ",
		"apreq ",
		"bit-and ",
		"bit-or ",
		"bit-xor ",
		"bool-and ",
		"bool-or ",
		"scope ",
		"trinary ",
	};
	assert(countof(types) == PEX_COUNT_OF);

	if (unsigned(type) < countof(types))
	{
		out << types[type];
	}
	else
	{
		out.AppendFormat("bad-pex-%u ", type);
	}
}

static void PrintExpression(FString &out, ZCC_TreeNode *node)
{
	ZCC_Expression *enode = (ZCC_Expression *)node;
	out << "(expr-";
	PrintExprType(out, enode->Operation);
	out << ')';
}

static void PrintExprID(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExprID *enode = (ZCC_ExprID *)node;
	assert(enode->Operation == PEX_ID);
	out.AppendFormat("(expr-id '%s')", FName(enode->Identifier).GetChars());
}

static void PrintExprString(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExprString *enode = (ZCC_ExprString *)node;
	assert(enode->Operation == PEX_StringConst);
	out.AppendFormat("(expr-string-const ");
	PrintStringConst(out, *enode->Value);
	out << ')';
}

static void PrintExprInt(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExprInt *enode = (ZCC_ExprInt *)node;
	assert(enode->Operation == PEX_IntConst || enode->Operation == PEX_UIntConst);
	out << "(expr-";
	PrintExprType(out, enode->Operation);
	out.AppendFormat("%d)", enode->Value);
}

static void PrintExprFloat(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExprFloat *enode = (ZCC_ExprFloat *)node;
	assert(enode->Operation == PEX_FloatConst);
	out.AppendFormat("(expr-float-const %g)", enode->Value);
}

static void PrintExprFuncCall(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExprFuncCall *enode = (ZCC_ExprFuncCall *)node;
	assert(enode->Operation == PEX_FuncCall);
	out << "(expr-func-call ";
	PrintNodes(out, enode->Function);
	PrintNodes(out, enode->Parameters, ')');
}

static void PrintExprMemberAccess(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExprMemberAccess *enode = (ZCC_ExprMemberAccess *)node;
	assert(enode->Operation == PEX_MemberAccess);
	out << "(expr-member-access ";
	PrintNodes(out, enode->Left);
	out.AppendFormat("'%s')", FName(enode->Right).GetChars());
}

static void PrintExprUnary(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExprUnary *enode = (ZCC_ExprUnary *)node;
	out << "(expr-";
	PrintExprType(out, enode->Operation);
	PrintNodes(out, enode->Operand, ')');
}

static void PrintExprBinary(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExprBinary *enode = (ZCC_ExprBinary *)node;
	out << "(expr-";
	PrintExprType(out, enode->Operation);
	PrintNodes(out, enode->Left);
	PrintNodes(out, enode->Right, ')');
}

static void PrintExprTrinary(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExprTrinary *enode = (ZCC_ExprTrinary *)node;
	out << "(expr-";
	PrintExprType(out, enode->Operation);
	PrintNodes(out, enode->Test);
	PrintNodes(out, enode->Left);
	PrintNodes(out, enode->Right, ')');
}

static void PrintFuncParam(FString &out, ZCC_TreeNode *node)
{
	ZCC_FuncParm *pnode = (ZCC_FuncParm *)node;
	out.AppendFormat("(func-parm %s ", FName(pnode->Label).GetChars());;
	PrintNodes(out, pnode->Value, ')');
}

static void PrintStatement(FString &out, ZCC_TreeNode *node)
{
	out << "(statement)";
}

static void PrintCompoundStmt(FString &out, ZCC_TreeNode *node)
{
	ZCC_CompoundStmt *snode = (ZCC_CompoundStmt *)node;
	out << "(compound-stmt ";
	PrintNodes(out, snode->Content, ')');
}

static void PrintContinueStmt(FString &out, ZCC_TreeNode *node)
{
	out << "(continue-stmt)";
}

static void PrintBreakStmt(FString &out, ZCC_TreeNode *node)
{
	out << "(break-stmt)";
}

static void PrintReturnStmt(FString &out, ZCC_TreeNode *node)
{
	ZCC_ReturnStmt *snode = (ZCC_ReturnStmt *)node;
	out << "(return-stmt ";
	PrintNodes(out, snode->Values, ')');
}

static void PrintExpressionStmt(FString &out, ZCC_TreeNode *node)
{
	ZCC_ExpressionStmt *snode = (ZCC_ExpressionStmt *)node;
	out << "(expression-stmt ";
	PrintNodes(out, snode->Expression, ')');
}

static void PrintIterationStmt(FString &out, ZCC_TreeNode *node)
{
	ZCC_IterationStmt *snode = (ZCC_IterationStmt *)node;
	out << "(iteration-stmt ";
	out << (snode->CheckAt == ZCC_IterationStmt::Start) ? "start " : "end ";
	PrintNodes(out, snode->LoopCondition);
	PrintNodes(out, snode->LoopBumper);
	PrintNodes(out, snode->LoopStatement, true);
}

static void PrintIfStmt(FString &out, ZCC_TreeNode *node)
{
	ZCC_IfStmt *snode = (ZCC_IfStmt *)node;
	out << "(if-stmt ";
	PrintNodes(out, snode->Condition);
	PrintNodes(out, snode->TruePath);
	PrintNodes(out, snode->FalsePath, ')');
}

static void PrintSwitchStmt(FString &out, ZCC_TreeNode *node)
{
	ZCC_SwitchStmt *snode = (ZCC_SwitchStmt *)node;
	out << "(switch-stmt ";
	PrintNodes(out, snode->Condition);
	PrintNodes(out, snode->Content, ')');
}

static void PrintCaseStmt(FString &out, ZCC_TreeNode *node)
{
	ZCC_CaseStmt *snode = (ZCC_CaseStmt *)node;
	out << "(case-stmt ";
	PrintNodes(out, snode->Condition, ')');
}

static void PrintAssignStmt(FString &out, ZCC_TreeNode *node)
{
	ZCC_AssignStmt *snode = (ZCC_AssignStmt *)node;
	out << "(assign-stmt ";
	switch (snode->AssignOp)
	{
	case ZCC_EQ:		out << "= "; break;
	case ZCC_MULEQ:		out << "*= "; break;
	case ZCC_DIVEQ:		out << "/= "; break;
	case ZCC_MODEQ:		out << "%= "; break;
	case ZCC_ADDEQ:		out << "+= "; break;
	case ZCC_SUBEQ:		out << "-= "; break;
	case ZCC_LSHEQ:		out << "<<= "; break;
	case ZCC_RSHEQ:		out << ">>= "; break;
	case ZCC_ANDEQ:		out << "&= "; break;
	case ZCC_OREQ:		out << "|= "; break;
	case ZCC_XOREQ:		out << "^= "; break;
	default:
		out.AppendFormat("assign-op-%d ", snode->AssignOp);
	}
	PrintNodes(out, snode->Dests);
	PrintNodes(out, snode->Sources, ')');
}

static void PrintLocalVarStmt(FString &out, ZCC_TreeNode *node)
{
	ZCC_LocalVarStmt *snode = (ZCC_LocalVarStmt *)node;
	out << "(local-var-stmt ";
	PrintNodes(out, snode->Type);
	PrintNodes(out, snode->Vars);
	PrintNodes(out, snode->Inits, ')');
}

static void PrintFuncParamDecl(FString &out, ZCC_TreeNode *node)
{
	ZCC_FuncParamDecl *dnode = (ZCC_FuncParamDecl *)node;
	out << "(func-param-decl ";
	PrintNodes(out, dnode->Type);
	out.AppendFormat("%s %x)", FName(dnode->Name).GetChars(), dnode->Flags);
}

static void PrintConstantDef(FString &out, ZCC_TreeNode *node)
{
	ZCC_ConstantDef *dnode = (ZCC_ConstantDef *)node;
	out.AppendFormat("(constant-def %s ", FName(dnode->Name).GetChars());
	PrintNodes(out, dnode->Value, ')');
}

static void PrintDeclarator(FString &out, ZCC_TreeNode *node)
{
	ZCC_Declarator *dnode = (ZCC_Declarator *)node;
	out << "(declarator ";
	PrintNodes(out, dnode->Type);
	out.AppendFormat("%x)", dnode->Flags);
}

static void PrintVarDeclarator(FString &out, ZCC_TreeNode *node)
{
	ZCC_VarDeclarator *dnode = (ZCC_VarDeclarator *)node;
	out << "(var-declarator ";
	PrintNodes(out, dnode->Type);
	out.AppendFormat("%x ", dnode->Flags);
	PrintNodes(out, dnode->Names, ')');
}

static void PrintFuncDeclarator(FString &out, ZCC_TreeNode *node)
{
	ZCC_FuncDeclarator *dnode = (ZCC_FuncDeclarator *)node;
	out << "(func-declarator ";
	PrintNodes(out, dnode->Type);
	out.AppendFormat("%x %s ", dnode->Flags, FName(dnode->Name).GetChars());
	PrintNodes(out, dnode->Params, ')');
}

void (* const TreeNodePrinter[NUM_AST_NODE_TYPES])(FString &, ZCC_TreeNode *) =
{
	PrintIdentifier,
	PrintClass,
	PrintStruct,
	PrintEnum,
	PrintEnumNode,
	PrintStates,
	PrintStatePart,
	PrintStateLabel,
	PrintStateStop,
	PrintStateWait,
	PrintStateFail,
	PrintStateLoop,
	PrintStateGoto,
	PrintStateLine,
	PrintVarName,
	PrintType,
	PrintBasicType,
	PrintMapType,
	PrintDynArrayType,
	PrintClassType,
	PrintExpression,
	PrintExprID,
	PrintExprString,
	PrintExprInt,
	PrintExprFloat,
	PrintExprFuncCall,
	PrintExprMemberAccess,
	PrintExprUnary,
	PrintExprBinary,
	PrintExprTrinary,
	PrintFuncParam,
	PrintStatement,
	PrintCompoundStmt,
	PrintContinueStmt,
	PrintBreakStmt,
	PrintReturnStmt,
	PrintExpressionStmt,
	PrintIterationStmt,
	PrintIfStmt,
	PrintSwitchStmt,
	PrintCaseStmt,
	PrintAssignStmt,
	PrintLocalVarStmt,
	PrintFuncParamDecl,
	PrintConstantDef,
	PrintDeclarator,
	PrintVarDeclarator,
	PrintFuncDeclarator
};

FString ZCC_PrintAST(ZCC_TreeNode *root)
{
	FString out;
	PrintNodes(out, root, '\0');
	return out;
}
