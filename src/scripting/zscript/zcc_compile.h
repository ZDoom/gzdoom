#ifndef ZCC_COMPILE_H
#define ZCC_COMPILE_H

#include <memory>
#include "backend/codegen.h"

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
	TArray<ZCC_Property *> Properties;

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

struct ZCC_PropertyWork
{
	ZCC_Property *prop;
	PSymbolTable *outputtable;
};

struct ZCC_ConstantWork
{
	ZCC_ConstantDef *node;
	PStruct *cls;
	PSymbolTable *Outputtable;
	ExpVal constval;
};

class ZCCCompiler
{
public:
	ZCCCompiler(ZCC_AST &tree, DObject *outer, PSymbolTable &symbols, PNamespace *outnamespace, int lumpnum, const VersionInfo & ver);
	~ZCCCompiler();
	int Compile();

private:
	int IntConstFromNode(ZCC_TreeNode *node, PStruct *cls);
	FString StringConstFromNode(ZCC_TreeNode *node, PStruct *cls);
	void ProcessClass(ZCC_Class *node, PSymbolTreeNode *tnode);
	void ProcessStruct(ZCC_Struct *node, PSymbolTreeNode *tnode, ZCC_Class *outer);
	void CreateStructTypes();
	void CreateClassTypes();
	void CopyConstants(TArray<ZCC_ConstantWork> &dest, TArray<ZCC_ConstantDef*> &Constants, PStruct *cls, PSymbolTable *ot);
	void CompileAllConstants();
	void AddConstant(ZCC_ConstantWork &constant);
	bool CompileConstant(ZCC_ConstantWork *def);

	void CompileAllFields();
	bool CompileFields(PStruct *type, TArray<ZCC_VarDeclarator *> &Fields, PClass *Outer, PSymbolTable *TreeNodes, bool forstruct, bool hasnativechildren = false);
	void CompileAllProperties();
	bool CompileProperties(PClass *type, TArray<ZCC_Property *> &Properties, FName prefix);
	FString FlagsToString(uint32_t flags);
	PType *DetermineType(PType *outertype, ZCC_TreeNode *field, FName name, ZCC_Type *ztype, bool allowarraytypes, bool formember);
	PType *ResolveArraySize(PType *baseType, ZCC_Expression *arraysize, PStruct *cls);
	PType *ResolveUserType(ZCC_BasicType *type, PSymbolTable *sym);

	void InitDefaults();
	void ProcessDefaultFlag(PClassActor *cls, ZCC_FlagStmt *flg);
	void ProcessDefaultProperty(PClassActor *cls, ZCC_PropertyStmt *flg, Baggage &bag);
	void DispatchProperty(FPropertyInfo *prop, ZCC_PropertyStmt *pex, AActor *defaults, Baggage &bag);
	void DispatchScriptProperty(PProperty *prop, ZCC_PropertyStmt *pex, AActor *defaults, Baggage &bag);
	void CompileFunction(ZCC_StructWork *c, ZCC_FuncDeclarator *f, bool forclass);

	void InitFunctions();
	void CompileStates();
	FxExpression *SetupActionFunction(PClass *cls, ZCC_TreeNode *sl, int stateflags);

	bool SimplifyingConstant;
	TArray<ZCC_ConstantDef *> Constants;
	TArray<ZCC_StructWork *> Structs;
	TArray<ZCC_ClassWork *> Classes;
	TArray<ZCC_PropertyWork *> Properties;
	VersionInfo mVersion;

	PSymbolTreeNode *AddTreeNode(FName name, ZCC_TreeNode *node, PSymbolTable *treenodes, bool searchparents = false);

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
	PNamespace *OutNamespace;
	ZCC_AST &AST;
	int Lump;
};

void ZCC_InitConversions();

#endif
