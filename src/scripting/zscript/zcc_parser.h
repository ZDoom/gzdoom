#ifndef ZCC_PARSER_H
#define ZCC_PARSER_H

#include "memarena.h"
#include "sc_man.h"
#include "types.h"

struct ZCCToken
{
	template <typename... Ts>
	struct TLargest;

	template <typename T>
	struct TLargest<T>
	{
		using Type = T;
	};

	template <typename T, typename U, typename... Ts>
	struct TLargest<T, U, Ts...>
	{
		using Type = typename TLargest<
			typename std::conditional<
				(sizeof(T) > sizeof(U)), T, U
			>::type, Ts...
		>::Type;
	};

	union
	{
		int Int;
		double Float;
		FString *String;
		TLargest<decltype(Int), decltype(Float), decltype(String)>::Type Largest;
	};
	int SourceLoc;

	ENamedName Name() { return ENamedName(Int); }
};

// Variable / Function / Class modifiers
enum
{
	ZCC_Native			= 1 << 0,
	ZCC_Static			= 1 << 1,
	ZCC_Private			= 1 << 2,
	ZCC_Protected		= 1 << 3,
	ZCC_Latent			= 1 << 4,
	ZCC_Final			= 1 << 5,
	ZCC_Meta			= 1 << 6,
	ZCC_Action			= 1 << 7,
	ZCC_Deprecated		= 1 << 8,
	ZCC_ReadOnly		= 1 << 9,
	ZCC_FuncConst		= 1 << 10,
	ZCC_Abstract		= 1 << 11,
	ZCC_Extension		= 1 << 12,
	ZCC_Virtual			= 1 << 13,
	ZCC_Override		= 1 << 14,
	ZCC_Transient		= 1 << 15,
	ZCC_VarArg			= 1 << 16,
	ZCC_UIFlag			= 1 << 17, // there's also token called ZCC_UI
	ZCC_Play			= 1 << 18,
	ZCC_ClearScope		= 1 << 19,
	ZCC_VirtualScope	= 1 << 20,
	ZCC_Version			= 1 << 21,
};

// Function parameter modifiers
enum
{
	ZCC_In				= 1 << 0,
	ZCC_Out				= 1 << 1,
	ZCC_Optional		= 1 << 2,
};


// Syntax tree structures.
enum EZCCTreeNodeType
{
	AST_Identifier,
	AST_Class,
	AST_Struct,
	AST_Enum,
	AST_EnumTerminator,
	AST_States,
	AST_StatePart,
	AST_StateLabel,
	AST_StateStop,
	AST_StateWait,
	AST_StateFail,
	AST_StateLoop,
	AST_StateGoto,
	AST_StateLine,
	AST_VarName,
	AST_VarInit,
	AST_Type,
	AST_BasicType,
	AST_MapType,
	AST_DynArrayType,
	AST_ClassType,
	AST_Expression,
	AST_ExprID,
	AST_ExprTypeRef,
	AST_ExprConstant,
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
	AST_FuncParamDecl,
	AST_ConstantDef,
	AST_Declarator,
	AST_VarDeclarator,
	AST_FuncDeclarator,
	AST_Default,
	AST_FlagStmt,
	AST_PropertyStmt,
	AST_VectorValue,
	AST_DeclFlags,
	AST_ClassCast,
	AST_StaticArrayStatement,
	AST_Property,

	NUM_AST_NODE_TYPES
};

enum EZCCBuiltinType
{
	ZCC_SInt8,
	ZCC_UInt8,
	ZCC_SInt16,
	ZCC_UInt16,		// smaller than 32 bit types are only valid in structs, classes and arrays.
	ZCC_SInt32,
	ZCC_UInt32,
	ZCC_IntAuto,	// for enums, autoselect appropriately sized int

	ZCC_Bool,
	ZCC_Float64,
	ZCC_FloatAuto,	// 32-bit in structs/classes, 64-bit everywhere else
	ZCC_String,
	ZCC_Vector2,
	ZCC_Vector3,
	ZCC_Name,

	ZCC_Color,		// special types for ZDoom.
	ZCC_State,
	ZCC_Sound,

	ZCC_UserType,
	ZCC_NativeType,
	ZCC_Let,

	ZCC_NUM_BUILT_IN_TYPES
};

enum EZCCExprType
{
#define xx(a,z)	PEX_##a,
#include "zcc_exprlist.h"

	PEX_COUNT_OF
};

struct ZCC_TreeNode
{
	// This tree node's siblings are stored in a circular linked list.
	// When you get back to this node, you know you've been through
	// the whole list.
	ZCC_TreeNode *SiblingNext;
	ZCC_TreeNode *SiblingPrev;

	// can't use FScriptPosition, because the string wouldn't have a chance to
	// destruct if we did that.
	FString *SourceName;
	int SourceLump;
	int SourceLoc;

	// Node type is one of the node types above, which corresponds with
	// one of the structures below.
	EZCCTreeNodeType NodeType;

	operator FScriptPosition()
	{
		return FScriptPosition(*SourceName, SourceLoc);
	}

};

void AppendTreeNodeSibling(ZCC_TreeNode *thisnode, ZCC_TreeNode *sibling);

struct ZCC_Identifier : ZCC_TreeNode
{
	ENamedName Id;
};

struct ZCC_NamedNode : ZCC_TreeNode
{
	ENamedName NodeName;
	PSymbolType *Symbol;
};

struct ZCC_Struct : ZCC_NamedNode
{
	uint32_t Flags;
	ZCC_TreeNode *Body;
	PContainerType *Type;
	VersionInfo Version;
};

struct ZCC_Property : ZCC_NamedNode
{
	ZCC_TreeNode *Body;
};

struct ZCC_Class : ZCC_Struct
{
	ZCC_Identifier *ParentName;
	ZCC_Identifier *Replaces;

	PClass *CType() { return static_cast<PClassType *>(Type)->Descriptor; }
};

struct ZCC_Enum : ZCC_NamedNode
{
	EZCCBuiltinType EnumType;
	struct ZCC_ConstantDef *Elements;
};

struct ZCC_EnumTerminator : ZCC_TreeNode
{
};

struct ZCC_States : ZCC_TreeNode
{
	struct ZCC_StatePart *Body;
	ZCC_Identifier *Flags;
};

struct ZCC_StatePart : ZCC_TreeNode
{
};

struct ZCC_StateLabel : ZCC_StatePart
{
	ENamedName Label;
};

struct ZCC_StateStop : ZCC_StatePart
{
};

struct ZCC_StateWait : ZCC_StatePart
{
};

struct ZCC_StateFail : ZCC_StatePart
{
};

struct ZCC_StateLoop : ZCC_StatePart
{
};

struct ZCC_Expression : ZCC_TreeNode
{
	EZCCExprType Operation;
	PType *Type;

	// Repurposes this node as an error node
	void ToErrorNode()
	{
		Type = TypeError;
		Operation = PEX_Nil;
		NodeType = AST_Expression;
	}
};

struct ZCC_StateGoto : ZCC_StatePart
{
	ZCC_Identifier *Qualifier;
	ZCC_Identifier *Label;
	ZCC_Expression *Offset;
};

struct ZCC_StateLine : ZCC_StatePart
{
	FString *Sprite;
	BITFIELD bBright : 1;
	BITFIELD bFast : 1;
	BITFIELD bSlow : 1;
	BITFIELD bNoDelay : 1;
	BITFIELD bCanRaise : 1;
	FString *Frames;
	ZCC_Expression *Duration;
	ZCC_Expression *Offset;
	ZCC_ExprConstant *Lights;
	ZCC_TreeNode *Action;
};

struct ZCC_VarName : ZCC_TreeNode
{
	ENamedName Name;
	ZCC_Expression *ArraySize;	// NULL if not an array
};

struct ZCC_VarInit : ZCC_VarName
{
	ZCC_Expression *Init;
	bool InitIsArray;	// this is needed to distinguish one-element arrays from raw elements.
};

struct ZCC_Type : ZCC_TreeNode
{
	ZCC_Expression *ArraySize;	// NULL if not an array
};

struct ZCC_BasicType : ZCC_Type
{
	EZCCBuiltinType	Type;
	ZCC_Identifier *UserType;
	bool isconst;
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
	ZCC_Identifier *Restriction;
};

struct ZCC_ExprID : ZCC_Expression
{
	ENamedName Identifier;
};

struct ZCC_ExprTypeRef : ZCC_Expression
{
	PType *RefType;
};

struct ZCC_ExprConstant : ZCC_Expression
{
	union
	{
		FString *StringVal;
		int IntVal;
		unsigned int UIntVal;
		double DoubleVal;
	};
};

struct ZCC_FuncParm : ZCC_TreeNode
{
	ZCC_Expression *Value;
	ENamedName Label;
};

struct ZCC_ExprFuncCall : ZCC_Expression
{
	ZCC_Expression *Function;
	ZCC_FuncParm *Parameters;
};

struct ZCC_ClassCast : ZCC_Expression
{
	ENamedName ClassName;
	ZCC_FuncParm *Parameters;
};

struct ZCC_ExprMemberAccess : ZCC_Expression
{
	ZCC_Expression *Left;
	ENamedName Right;
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

struct ZCC_VectorValue : ZCC_Expression
{
	ZCC_Expression *X, *Y, *Z;
};

struct ZCC_Statement : ZCC_TreeNode
{
};

struct ZCC_StaticArrayStatement : ZCC_Statement
{
	ZCC_Type *Type;
	ENamedName Id;
	ZCC_Expression *Values;
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
	ZCC_VarInit *Vars;
};

struct ZCC_FuncParamDecl : ZCC_TreeNode
{
	ZCC_Type *Type;
	ZCC_Expression *Default;
	ENamedName Name;
	int Flags;
};

struct ZCC_DeclFlags : ZCC_TreeNode
{
	ZCC_Identifier *Id;
	VersionInfo Version;
	int Flags;
};

struct ZCC_ConstantDef : ZCC_NamedNode
{
	ZCC_Expression *Value;
	PSymbolConst *Symbol;
	ZCC_Enum *Type;	// gets set when the constant originates from an enum.
};

struct ZCC_Declarator : ZCC_TreeNode
{
	ZCC_Type *Type;
	int Flags;
	VersionInfo Version;
};

// A variable in a class or struct.
struct ZCC_VarDeclarator : ZCC_Declarator
{
	ZCC_VarName *Names;
};

// A function in a class.
struct ZCC_FuncDeclarator : ZCC_Declarator
{
	ZCC_FuncParamDecl *Params;
	ENamedName Name;
	ZCC_Statement *Body;
	ZCC_Identifier *UseFlags;
};

struct ZCC_Default : ZCC_CompoundStmt
{
};

struct ZCC_PropertyStmt : ZCC_Statement
{
	ZCC_Identifier *Prop;
	ZCC_Expression *Values;
};


struct ZCC_FlagStmt : ZCC_Statement
{
	ZCC_Identifier *name;
	bool set;
};

FString ZCC_PrintAST(ZCC_TreeNode *root);


struct ZCC_AST
{
	ZCC_AST() : TopNode(NULL) {}
	ZCC_TreeNode *InitNode(size_t size, EZCCTreeNodeType type, ZCC_TreeNode *basis);

	FSharedStringArena Strings;
	FMemArena SyntaxArena;
	struct ZCC_TreeNode *TopNode;
	VersionInfo ParseVersion;
};

struct ZCCParseState : public ZCC_AST
{
	ZCCParseState(FScanner *scanner = nullptr) : sc(scanner) {}
	ZCC_TreeNode *InitNode(size_t size, EZCCTreeNodeType type);

	FScanner *sc;
};

#endif
