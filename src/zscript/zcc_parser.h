struct ZCCParseState
{
	ZCCParseState(FScanner &scanner) : sc(scanner)
	{
	}

	FScanner &sc;
	FSharedStringArena Strings;
	FMemArena SyntaxArena;
};

union ZCCToken
{
	int Int;
	double Float;
	FString *String;
};

// Syntax tree structures.
enum EZCCTreeNodeType
{
	AST_Class,
	AST_Struct,
	AST_Enum,
	AST_EnumNode,
	AST_States,
	AST_StateLabel,
	AST_StateGoto,
	AST_StateLine,
	AST_VarName,
	AST_Type,
	AST_BasicType,
	AST_MapType,
	AST_DynArrayType,
	AST_ClassType,
	AST_Expression,
	AST_ExprID,
	AST_ExprString,
	AST_ExprInt,
	AST_ExprFloat,
	AST_ExprFuncCall,
	AST_ExprMemberAccess,
	AST_ExprUnary,
	AST_ExprBinary,
	AST_ExprTrinary,
	AST_FuncParm,
	AST_Statement,
	AST_CompoundStmt,
	AST_ContinueStmt,
	AST_BreakStmt,
	AST_ReturnStmt,
	AST_ExpressionStmt,
	AST_IterationStmt,
	AST_IfStmt,
	AST_SwitchStmt,
	AST_CaseStmt,
	AST_AssignStmt,
	AST_LocalVarStmt,
};


enum EZCCExprType
{
	PEX_ID,
	PEX_Super,
	PEX_Self,
	PEX_StringConst,
	PEX_IntConst,
	PEX_UIntConst,
	PEX_FloatConst,
	PEX_FuncCall,
	PEX_ArrayAccess,
	PEX_MemberAccess,
	PEX_PostInc,
	PEX_PostDec,

	PEX_PreInc,
	PEX_PreDec,
	PEX_Negate,
	PEX_AntiNegate,
	PEX_BitNot,
	PEX_BoolNot,
	PEX_SizeOf,
	PEX_AlignOf,

	PEX_Add,
	PEX_Sub,
	PEX_Mul,
	PEX_Div,
	PEX_Mod,
	PEX_Pow,
	PEX_CrossProduct,
	PEX_DotProduct,
	PEX_LeftShift,
	PEX_RightShift,
	PEX_Concat,

	PEX_LT,
	PEX_GT,
	PEX_LTEQ,
	PEX_GTEQ,
	PEX_LTGTEQ,
	PEX_Is,

	PEX_EQEQ,
	PEX_NEQ,
	PEX_APREQ,

	PEX_BitAnd,
	PEX_BitOr,
	PEX_BitXor,
	PEX_BoolAnd,
	PEX_BoolOr,

	PEX_Scope,

	PEX_Trinary
};

struct ZCC_TreeNode
{
	// This tree node's siblings are stored in a circular linked list.
	// When you get back to this node, you know you've been through
	// the whole list.
	ZCC_TreeNode *SiblingNext;
	ZCC_TreeNode *SiblingPrev;

	// Node type is one of the node types above, which corresponds with
	// one of the structures below.
	EZCCTreeNodeType NodeType;

	// Appends a sibling to this node's sibling list.
	void AppendSibling(ZCC_TreeNode *sibling)
	{
		if (sibling == NULL)
		{
			return;
		}

		// The new sibling node should only be in a list with itself.
		assert(sibling->SiblingNext == sibling && sibling->SiblingNext == sibling);

		// Check integrity of our sibling list.
		assert(SiblingPrev->SiblingNext == this);
		assert(SiblingNext->SiblingPrev == this);

		SiblingPrev->SiblingNext = sibling;
		sibling->SiblingPrev = SiblingPrev;
		SiblingPrev = sibling;
		sibling->SiblingNext = this;
	}
};

struct ZCC_Class : ZCC_TreeNode
{
	FName ClassName;
	FName ParentName;
	VM_UWORD Flags;
	ZCC_TreeNode *Body;
};

struct ZCC_Struct : ZCC_TreeNode
{
	FName StructName;
	ZCC_TreeNode *Body;
};

struct ZCC_Enum : ZCC_TreeNode
{
	FName EnumName;
	struct ZCC_EnumNode *Elements;
};

struct ZCC_EnumNode : ZCC_TreeNode
{
	FName ElemName;
	ZCC_TreeNode *ElemValue;
};

struct ZCC_States : ZCC_TreeNode
{
	ZCC_TreeNode *Body;
};

struct ZCC_StateLabel : ZCC_TreeNode
{
	FName Label;
};

struct ZCC_StateGoto : ZCC_TreeNode
{
	ZCC_TreeNode *Label;
	ZCC_TreeNode *Offset;
};

struct ZCC_StateLine : ZCC_TreeNode
{
	char Sprite[4];
	FString *Frames;
	BITFIELD bBright:1;
	BITFIELD bHasOffset:1;
	int OffsetX;
	int OffsetY;
	ZCC_TreeNode *Action;
};

struct ZCC_Expression : ZCC_TreeNode
{
	EZCCExprType Operation;
};

struct ZCC_VarName : ZCC_TreeNode
{
	bool bIsArray;
	ZCC_Expression *ArraySize;
	ENamedName Name;
};

struct ZCC_Type : ZCC_TreeNode
{
	BITFIELD bIsArray:1;
	ZCC_Expression *ArraySize;
};

struct ZCC_BasicType : ZCC_Type
{
	PType *Type;
	FName TypeName;
};

struct ZCC_MapType : ZCC_Type
{
	ZCC_Type *KeyType;
	ZCC_Type *ValueType;
};

struct ZCC_DynArrayType : ZCC_Type
{
	ZCC_Type *ElementType;
};

struct ZCC_ClassType : ZCC_Type
{
	FName Restriction;
};

struct ZCC_ExprID : ZCC_Expression
{
	FName Identifier;
};

struct ZCC_ExprString : ZCC_Expression
{
	FString *Value;
};

struct ZCC_ExprInt : ZCC_Expression
{
	int Value;
};

struct ZCC_ExprFloat : ZCC_Expression
{
	double Value;
};

struct ZCC_FuncParm : ZCC_TreeNode
{
	ZCC_Expression *Value;
	FName Label;
};

struct ZCC_ExprFuncCall : ZCC_Expression
{
	ZCC_Expression *Function;
	ZCC_FuncParm *Parameters;
};

struct ZCC_ExprMemberAccess : ZCC_Expression
{
	ZCC_Expression *Left;
	FName Right;
};

struct ZCC_ExprUnary : ZCC_Expression
{
	ZCC_Expression *Operand;
};

struct ZCC_ExprBinary : ZCC_Expression
{
	ZCC_Expression *Left;
	ZCC_Expression *Right;
};

struct ZCC_ExprTrinary : ZCC_Expression
{
	ZCC_Expression *Test;
	ZCC_Expression *Left;
	ZCC_Expression *Right;
};

struct ZCC_Statement : ZCC_TreeNode
{
};

struct ZCC_CompoundStmt : ZCC_Statement
{
	ZCC_Statement *Content;
};

struct ZCC_ContinueStmt : ZCC_Statement
{
};

struct ZCC_BreakStmt : ZCC_Statement
{
};

struct ZCC_ReturnStmt : ZCC_Statement
{
	ZCC_Expression *Values;
};

struct ZCC_ExpressionStmt : ZCC_Statement
{
	ZCC_Expression *Expression;
};

struct ZCC_IterationStmt : ZCC_Statement
{
	ZCC_Expression *LoopCondition;
	ZCC_Statement *LoopStatement;
	ZCC_Statement *LoopBumper;

	// Should the loop condition be checked at the
	// start of the loop (before the LoopStatement)
	// or at the end (after the LoopStatement)?
	enum { Start, End } CheckAt;
};

struct ZCC_IfStmt : ZCC_Statement
{
	ZCC_Expression *Condition;
	ZCC_Statement *TruePath;
	ZCC_Statement *FalsePath;
};

struct ZCC_SwitchStmt : ZCC_Statement
{
	ZCC_Expression *Condition;
	ZCC_Statement *Content;
};

struct ZCC_CaseStmt : ZCC_Statement
{
	// A NULL Condition represents the default branch
	ZCC_Expression *Condition;
};

struct ZCC_AssignStmt : ZCC_Statement
{
	ZCC_Expression *Dests;
	ZCC_Expression *Sources;
	int AssignOp;
};

struct ZCC_LocalVarStmt : ZCC_Statement
{
	ZCC_Type *Type;
	ZCC_VarName *Vars;
	ZCC_Expression *Inits;
};
