#ifndef ZCC_COMPILE_H
#define ZCC_COMPILE_H

#include <memory>

struct Baggage;
struct FPropertyInfo;
class AActor;
class FxExpression;
typedef TDeletingArray<FxExpression*> FArgumentList;


struct ZCC_StructWork
{
	PSymbolTable TreeNodes;
	ZCC_Struct *strct;
	ZCC_Class *OuterDef;
	PClass *Outer;
	PSymbolTreeNode *node;
	TArray<ZCC_Enum *> Enums;
	TArray<ZCC_ConstantDef *> Constants;
	TArray<ZCC_VarDeclarator *> Fields;
	TArray<ZCC_FuncDeclarator *> Functions;

	ZCC_StructWork()
	{
	}

	ZCC_StructWork(ZCC_Struct * s, PSymbolTreeNode *n, ZCC_Class *outer)
	{
		strct = s;
		node = n;
		OuterDef = outer;
		Outer = nullptr;
	};

	FName NodeName() const
	{
		return strct->NodeName;
	}

	PStruct *Type()
	{
		return strct->Type;
	}

};

struct ZCC_ClassWork : public ZCC_StructWork
{
	ZCC_Class *cls;
	TArray<ZCC_Default *> Defaults;
	TArray<ZCC_States *> States;

	ZCC_ClassWork(ZCC_Class * s, PSymbolTreeNode *n)
	{
		strct = s;
		cls = s;
		node = n;
		OuterDef = nullptr;
		Outer = nullptr;
	}

	PClass *Type()
	{
		return static_cast<PClass *>(strct->Type);
	}
};

struct ZCC_ConstantWork
{
	ZCC_ConstantDef *node;
	PSymbolTable *outputtable;
};

class ZCCCompiler
{
public:
	ZCCCompiler(ZCC_AST &tree, DObject *outer, PSymbolTable &symbols, PSymbolTable &outsymbols, int lumpnum);
	~ZCCCompiler();
	int Compile();

private:
	void ProcessClass(ZCC_Class *node, PSymbolTreeNode *tnode);
	void ProcessStruct(ZCC_Struct *node, PSymbolTreeNode *tnode, ZCC_Class *outer);
	void CreateStructTypes();
	void CreateClassTypes();
	void CopyConstants(TArray<ZCC_ConstantWork> &dest, TArray<ZCC_ConstantDef*> &Constants, PSymbolTable *ot);
	void CompileAllConstants();
	void AddConstant(ZCC_ConstantWork &constant);
	bool CompileConstant(ZCC_ConstantDef *def, PSymbolTable *Symbols);

	void CompileAllFields();
	bool CompileFields(PStruct *type, TArray<ZCC_VarDeclarator *> &Fields, PClass *Outer, PSymbolTable *TreeNodes, bool forstruct, bool hasnativechildren = false);
	FString FlagsToString(uint32_t flags);
	PType *DetermineType(PType *outertype, ZCC_TreeNode *field, FName name, ZCC_Type *ztype, bool allowarraytypes, bool formember);
	PType *ResolveArraySize(PType *baseType, ZCC_Expression *arraysize, PSymbolTable *sym);
	PType *ResolveUserType(ZCC_BasicType *type, PSymbolTable *sym);

	void InitDefaults();
	void ProcessDefaultFlag(PClassActor *cls, ZCC_FlagStmt *flg);
	void ProcessDefaultProperty(PClassActor *cls, ZCC_PropertyStmt *flg, Baggage &bag);
	void DispatchProperty(FPropertyInfo *prop, ZCC_PropertyStmt *pex, AActor *defaults, Baggage &bag);
	int GetInt(ZCC_Expression *expr);
	double GetDouble(ZCC_Expression *expr);
	const char *GetString(ZCC_Expression *expr, bool silent = false);
	void CompileFunction(ZCC_StructWork *c, ZCC_FuncDeclarator *f, bool forclass);

	void InitFunctions();
	void CompileStates();
	FxExpression *SetupActionFunction(PClass *cls, ZCC_TreeNode *sl, int stateflags);

	bool SimplifyingConstant;
	TArray<ZCC_ConstantDef *> Constants;
	TArray<ZCC_StructWork *> Structs;
	TArray<ZCC_ClassWork *> Classes;

	PSymbolTreeNode *AddTreeNode(FName name, ZCC_TreeNode *node, PSymbolTable *treenodes, bool searchparents = false);

	ZCC_Expression *Simplify(ZCC_Expression *root, PSymbolTable *Symbols, bool wantconstant);
	ZCC_Expression *DoSimplify(ZCC_Expression *root, PSymbolTable *Symbols);
	ZCC_Expression *SimplifyUnary(ZCC_ExprUnary *unary, PSymbolTable *Symbols);
	ZCC_Expression *SimplifyBinary(ZCC_ExprBinary *binary, PSymbolTable *Symbols);
	ZCC_Expression *SimplifyMemberAccess(ZCC_ExprMemberAccess *dotop, PSymbolTable *Symbols);
	ZCC_Expression *SimplifyFunctionCall(ZCC_ExprFuncCall *callop, PSymbolTable *Symbols);
	ZCC_OpProto *PromoteUnary(EZCCExprType op, ZCC_Expression *&expr);
	ZCC_OpProto *PromoteBinary(EZCCExprType op, ZCC_Expression *&left, ZCC_Expression *&right);

	ZCC_Expression *ApplyConversion(ZCC_Expression *expr, const PType::Conversion **route, int routelen);
	ZCC_Expression *AddCastNode(PType *type, ZCC_Expression *expr);

	ZCC_Expression *IdentifyIdentifier(ZCC_ExprID *idnode, PSymbolTable *sym);
	ZCC_Expression *NodeFromSymbol(PSymbol *sym, ZCC_Expression *source, PSymbolTable *table);
	ZCC_ExprConstant *NodeFromSymbolConst(PSymbolConst *sym, ZCC_Expression *idnode);
	ZCC_ExprTypeRef *NodeFromSymbolType(PSymbolType *sym, ZCC_Expression *idnode);


	void Warn(ZCC_TreeNode *node, const char *msg, ...) GCCPRINTF(3,4);
	void Error(ZCC_TreeNode *node, const char *msg, ...) GCCPRINTF(3,4);
	void MessageV(ZCC_TreeNode *node, const char *txtcolor, const char *msg, va_list argptr);

	FxExpression *ConvertAST(PStruct *cclass, ZCC_TreeNode *ast);
	FxExpression *ConvertNode(ZCC_TreeNode *node);
	FArgumentList &ConvertNodeList(FArgumentList &, ZCC_TreeNode *head);

	DObject *Outer;
	PStruct *ConvertClass;	// class type to be used when resoving symbols while converting an AST
	PSymbolTable *GlobalTreeNodes;
	PSymbolTable *OutputSymbols;
	ZCC_AST &AST;
	int Lump;
};

void ZCC_InitConversions();

#endif
