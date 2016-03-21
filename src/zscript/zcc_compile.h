#ifndef ZCC_COMPILE_H
#define ZCC_COMPILE_H

class ZCCCompiler
{
public:
	ZCCCompiler(ZCC_AST &tree, DObject *outer, PSymbolTable &symbols);
	int Compile();

private:
	void CompileConstants(const TArray<ZCC_ConstantDef *> &defs);
	PSymbolConst *CompileConstant(ZCC_ConstantDef *def);

	TArray<ZCC_ConstantDef *> Constants;
	TArray<ZCC_Struct *> Structs;
	TArray<ZCC_Class *> Classes;

	bool AddNamedNode(ZCC_NamedNode *node);

	ZCC_Expression *Simplify(ZCC_Expression *root);
	ZCC_Expression *SimplifyUnary(ZCC_ExprUnary *unary);
	ZCC_Expression *SimplifyBinary(ZCC_ExprBinary *binary);
	ZCC_Expression *SimplifyMemberAccess(ZCC_ExprMemberAccess *dotop);
	ZCC_Expression *SimplifyFunctionCall(ZCC_ExprFuncCall *callop);
	ZCC_OpProto *PromoteUnary(EZCCExprType op, ZCC_Expression *&expr);
	ZCC_OpProto *PromoteBinary(EZCCExprType op, ZCC_Expression *&left, ZCC_Expression *&right);

	void PromoteToInt(ZCC_Expression *&expr);
	void PromoteToUInt(ZCC_Expression *&expr);
	void PromoteToDouble(ZCC_Expression *&expr);
	void PromoteToString(ZCC_Expression *&expr);

	ZCC_Expression *ApplyConversion(ZCC_Expression *expr, const PType::Conversion **route, int routelen);
	ZCC_Expression *AddCastNode(PType *type, ZCC_Expression *expr);

	ZCC_Expression *IdentifyIdentifier(ZCC_ExprID *idnode);
	ZCC_Expression *NodeFromSymbol(PSymbol *sym, ZCC_Expression *source, PSymbolTable *table);
	ZCC_ExprConstant *NodeFromSymbolConst(PSymbolConst *sym, ZCC_Expression *idnode);
	ZCC_ExprTypeRef *NodeFromSymbolType(PSymbolType *sym, ZCC_Expression *idnode);
	PSymbol *CompileNode(ZCC_NamedNode *node);


	void Warn(ZCC_TreeNode *node, const char *msg, ...);
	void Error(ZCC_TreeNode *node, const char *msg, ...);
	void MessageV(ZCC_TreeNode *node, const char *txtcolor, const char *msg, va_list argptr);

	DObject *Outer;
	PSymbolTable *Symbols;
	ZCC_AST &AST;
	int ErrorCount;
	int WarnCount;
};

void ZCC_InitConversions();

#endif
