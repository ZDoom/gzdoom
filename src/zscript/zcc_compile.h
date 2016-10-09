#ifndef ZCC_COMPILE_H
#define ZCC_COMPILE_H

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

struct ZCC_ClassWork
{
	ZCC_Class *cls;
	PSymbolTable TreeNodes;
	PSymbolTreeNode *node;
	TArray<ZCC_Enum *> Enums;
	TArray<ZCC_ConstantDef *> Constants;
	TArray<ZCC_VarDeclarator *> Fields;

	ZCC_ClassWork(ZCC_Class * s, PSymbolTreeNode *n)
	{
		cls = s;
		node = n;
	}

	FName NodeName() const
	{
		return cls->NodeName;
	}

	PClass *Type()
	{
		return cls->Type;
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
	ZCCCompiler(ZCC_AST &tree, DObject *outer, PSymbolTable &symbols, PSymbolTable &outsymbols);
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
	bool CompileFields(PStruct *type, TArray<ZCC_VarDeclarator *> &Fields, PClass *Outer, PSymbolTable *TreeNodes, bool forstruct);
	FString FlagsToString(uint32_t flags);
	PType *DetermineType(PType *outertype, ZCC_VarDeclarator *field, ZCC_Type *ztype, bool allowarraytypes);
	PType *ResolveArraySize(PType *baseType, ZCC_Expression *arraysize, PSymbolTable *sym);
	PType *ResolveUserType(ZCC_BasicType *type, PSymbolTable *sym);

	TArray<ZCC_ConstantDef *> Constants;
	TArray<ZCC_StructWork *> Structs;
	TArray<ZCC_ClassWork *> Classes;

	PSymbolTreeNode *AddTreeNode(FName name, ZCC_TreeNode *node, PSymbolTable *treenodes, bool searchparents = false);

	ZCC_Expression *Simplify(ZCC_Expression *root, PSymbolTable *Symbols);
	ZCC_Expression *SimplifyUnary(ZCC_ExprUnary *unary, PSymbolTable *Symbols);
	ZCC_Expression *SimplifyBinary(ZCC_ExprBinary *binary, PSymbolTable *Symbols);
	ZCC_Expression *SimplifyMemberAccess(ZCC_ExprMemberAccess *dotop, PSymbolTable *Symbols);
	ZCC_Expression *SimplifyFunctionCall(ZCC_ExprFuncCall *callop, PSymbolTable *Symbols);
	ZCC_OpProto *PromoteUnary(EZCCExprType op, ZCC_Expression *&expr);
	ZCC_OpProto *PromoteBinary(EZCCExprType op, ZCC_Expression *&left, ZCC_Expression *&right);

	void PromoteToInt(ZCC_Expression *&expr);
	void PromoteToUInt(ZCC_Expression *&expr);
	void PromoteToDouble(ZCC_Expression *&expr);
	void PromoteToString(ZCC_Expression *&expr);

	ZCC_Expression *ApplyConversion(ZCC_Expression *expr, const PType::Conversion **route, int routelen);
	ZCC_Expression *AddCastNode(PType *type, ZCC_Expression *expr);

	ZCC_Expression *IdentifyIdentifier(ZCC_ExprID *idnode, PSymbolTable *sym);
	ZCC_Expression *NodeFromSymbol(PSymbol *sym, ZCC_Expression *source, PSymbolTable *table);
	ZCC_ExprConstant *NodeFromSymbolConst(PSymbolConst *sym, ZCC_Expression *idnode);
	ZCC_ExprTypeRef *NodeFromSymbolType(PSymbolType *sym, ZCC_Expression *idnode);


	void Warn(ZCC_TreeNode *node, const char *msg, ...);
	void Error(ZCC_TreeNode *node, const char *msg, ...);
	void MessageV(ZCC_TreeNode *node, const char *txtcolor, const char *msg, va_list argptr);

	DObject *Outer;
	PSymbolTable *GlobalTreeNodes;
	PSymbolTable *OutputSymbols;
	ZCC_AST &AST;
	int ErrorCount;
	int WarnCount;
};

void ZCC_InitConversions();

#endif
