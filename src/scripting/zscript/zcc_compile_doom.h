#pragma once
#include "zcc_compile.h"

void SetImplicitArgs(TArray<PType*>* args, TArray<uint32_t>* argflags, TArray<FName>* argnames, PContainerType* cls, uint32_t funcflags, int useflags);

class ZCCDoomCompiler : public ZCCCompiler
{
public:
	ZCCDoomCompiler(ZCC_AST &tree, DObject *outer, PSymbolTable &symbols, PNamespace *outnamespace, int lumpnum, const VersionInfo & ver)
		: ZCCCompiler(tree, outer, symbols, outnamespace, lumpnum, ver)
		{}
	int Compile() override;
protected:
	bool PrepareMetaData(PClass *type) override;
	void SetImplicitArgs(TArray<PType*>* args, TArray<uint32_t>* argflags, TArray<FName>* argnames, PContainerType* cls, uint32_t funcflags, int useflags) override
	{
		::SetImplicitArgs(args, argflags, argnames, cls, funcflags, useflags);
	}
private:
	void CompileAllProperties();
	bool CompileProperties(PClass *type, TArray<ZCC_Property *> &Properties, FName prefix);
	bool CompileFlagDefs(PClass *type, TArray<ZCC_FlagDef *> &Properties, FName prefix);
	void DispatchProperty(FPropertyInfo *prop, ZCC_PropertyStmt *property, AActor *defaults, Baggage &bag);
	void DispatchScriptProperty(PProperty *prop, ZCC_PropertyStmt *property, AActor *defaults, Baggage &bag);
	void ProcessDefaultProperty(PClassActor *cls, ZCC_PropertyStmt *prop, Baggage &bag);
	void ProcessDefaultFlag(PClassActor *cls, ZCC_FlagStmt *flg);
	void InitDefaults() override final;
	FxExpression *SetupActionFunction(PClass *cls, ZCC_TreeNode *af, int StateFlags);
	void CompileStates();
	int CheckActionKeyword(ZCC_FuncDeclarator *f, uint32_t &varflags, int useflags, ZCC_StructWork *c);
	
};


