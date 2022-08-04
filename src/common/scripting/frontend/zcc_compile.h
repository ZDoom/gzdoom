#ifndef ZCC_COMPILE_H
#define ZCC_COMPILE_H

#include <memory>
#include "codegen.h"

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
	TArray<ZCC_StaticArrayStatement *> Arrays;

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

	PContainerType *Type()
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
	TArray<ZCC_FlagDef *> FlagDefs;

	ZCC_ClassWork(ZCC_Class * s, PSymbolTreeNode *n)
	{
		strct = s;
		cls = s;
		node = n;
		OuterDef = nullptr;
		Outer = nullptr;
	}

	PClass *ClassType()
	{
		return static_cast<PClassType *>(strct->Type)->Descriptor;
	}
};

struct ZCC_MixinWork
{
	PSymbolTable TreeNodes;
	ZCC_MixinDef *mixin;
	PSymbolTreeNode *node;

	ZCC_MixinWork()
	{
	}

	ZCC_MixinWork(ZCC_MixinDef *m, PSymbolTreeNode *n)
	{
		mixin = m;
		node = n;
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
	PContainerType *cls;
	PSymbolTable *Outputtable;
	ExpVal constval;
};

class ZCCCompiler
{
public:
	ZCCCompiler(ZCC_AST &tree, DObject *outer, PSymbolTable &symbols, PNamespace *outnamespace, int lumpnum, const VersionInfo & ver);
	virtual ~ZCCCompiler();
	virtual int Compile();

protected:
	const char * GetStringConst(FxExpression *ex, FCompileContext &ctx);
	virtual int CheckActionKeyword(ZCC_FuncDeclarator* f, uint32_t &varflags, int useflags, ZCC_StructWork *c) { return -1; } // stock implementation does not support this.
	virtual bool PrepareMetaData(PClass *type) { return false; }
	virtual void SetImplicitArgs(TArray<PType*>* args, TArray<uint32_t>* argflags, TArray<FName>* argnames, PContainerType* cls, uint32_t funcflags, int useflags);

	int IntConstFromNode(ZCC_TreeNode *node, PContainerType *cls);
	FString StringConstFromNode(ZCC_TreeNode *node, PContainerType *cls);
	ZCC_MixinDef *ResolveMixinStmt(ZCC_MixinStmt *mixinStmt, EZCCMixinType type);
	void ProcessClass(ZCC_Class *node, PSymbolTreeNode *tnode);
	void ProcessStruct(ZCC_Struct *node, PSymbolTreeNode *tnode, ZCC_Class *outer);
	void ProcessMixin(ZCC_MixinDef *cnode, PSymbolTreeNode *treenode);
	void CreateStructTypes();
	void CreateClassTypes();
	void CopyConstants(TArray<ZCC_ConstantWork> &dest, TArray<ZCC_ConstantDef*> &Constants, PContainerType *cls, PSymbolTable *ot);
	void CompileAllConstants();
	void AddConstant(ZCC_ConstantWork &constant);
	bool CompileConstant(ZCC_ConstantWork *def);
	void CompileArrays(ZCC_StructWork *work);

	void CompileAllFields();
	bool CompileFields(PContainerType *type, TArray<ZCC_VarDeclarator *> &Fields, PClass *Outer, PSymbolTable *TreeNodes, bool forstruct, bool hasnativechildren = false);
	FString FlagsToString(uint32_t flags);
	PType *DetermineType(PType *outertype, ZCC_TreeNode *field, FName name, ZCC_Type *ztype, bool allowarraytypes, bool formember);
	PType *ResolveArraySize(PType *baseType, ZCC_Expression *arraysize, PContainerType *cls, bool *nosize);
	PType *ResolveUserType(ZCC_BasicType *type, ZCC_Identifier *id, PSymbolTable *sym, bool nativetype);
	static FString UserTypeName(ZCC_BasicType *type);
	TArray<ZCC_StructWork *> OrderStructs();
	void AddStruct(TArray<ZCC_StructWork *> &new_order, ZCC_StructWork *struct_def);
	ZCC_StructWork *StructTypeToWork(const PStruct *type) const;

	void CompileFunction(ZCC_StructWork *c, ZCC_FuncDeclarator *f, bool forclass);

	void InitFunctions();
	virtual void InitDefaults();

	TArray<ZCC_ConstantDef *> Constants;
	TArray<ZCC_StructWork *> Structs;
	TArray<ZCC_ClassWork *> Classes;
	TArray<ZCC_MixinWork *> Mixins;
	TArray<ZCC_PropertyWork *> Properties;
	VersionInfo mVersion;

	PSymbolTreeNode *AddTreeNode(FName name, ZCC_TreeNode *node, PSymbolTable *treenodes, bool searchparents = false);

	ZCC_Expression *NodeFromSymbol(PSymbol *sym, ZCC_Expression *source, PSymbolTable *table);
	ZCC_ExprConstant *NodeFromSymbolConst(PSymbolConst *sym, ZCC_Expression *idnode);
	ZCC_ExprTypeRef *NodeFromSymbolType(PSymbolType *sym, ZCC_Expression *idnode);


	void Warn(ZCC_TreeNode *node, const char *msg, ...) GCCPRINTF(3,4);
	void Error(ZCC_TreeNode *node, const char *msg, ...) GCCPRINTF(3,4);
	void MessageV(ZCC_TreeNode *node, const char *txtcolor, const char *msg, va_list argptr);

	FxExpression *ConvertAST(PContainerType *cclass, ZCC_TreeNode *ast);
	FxExpression *ConvertNode(ZCC_TreeNode *node, bool substitute= false);
	FxExpression *ConvertImplicitScopeNode(ZCC_TreeNode *node, ZCC_Statement *nested);
	FArgumentList &ConvertNodeList(FArgumentList &, ZCC_TreeNode *head);

	DObject *Outer;
	PContainerType *ConvertClass;	// class type to be used when resoving symbols while converting an AST
	PSymbolTable *GlobalTreeNodes;
	PNamespace *OutNamespace;
	ZCC_AST &AST;
	int Lump;
};

void ZCC_InitConversions();

#endif
