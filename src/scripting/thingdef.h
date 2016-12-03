#ifndef __THINGDEF_H
#define __THINGDEF_H

#include "doomtype.h"
#include "info.h"
#include "s_sound.h"
#include "sc_man.h"
#include "cmdlib.h"


class FScanner;


//==========================================================================
//
// A flag descriptor
//
//==========================================================================

struct FFlagDef
{
	unsigned int flagbit;
	const char *name;
	int structoffset;
	int fieldsize;
	int varflags;
};

FFlagDef *FindFlag (const PClass *type, const char *part1, const char *part2, bool strict = false);
void HandleDeprecatedFlags(AActor *defaults, PClassActor *info, bool set, int index);
bool CheckDeprecatedFlags(const AActor *actor, PClassActor *info, int index);
const char *GetFlagName(unsigned int flagnum, int flagoffset);
void ModActorFlag(AActor *actor, FFlagDef *fd, bool set);
bool ModActorFlag(AActor *actor, FString &flagname, bool set, bool printerror = true);
INTBOOL CheckActorFlag(const AActor *actor, FFlagDef *fd);
INTBOOL CheckActorFlag(const AActor *owner, const char *flagname, bool printerror = true);

#define FLAG_NAME(flagnum, flagvar) GetFlagName(flagnum, myoffsetof(AActor, flagvar))


//==========================================================================
//
// State parser
//
//==========================================================================
class FxExpression;

struct FStateLabels;

struct FStateDefine
{
	FName Label;
	TArray<FStateDefine> Children;
	FState *State;
	BYTE DefineFlags;
};

class FStateDefinitions
{
	TArray<FStateDefine> StateLabels;
	FState *laststate;
	FState *laststatebeforelabel;
	intptr_t lastlabel;
	TArray<FState> StateArray;
	TArray<FScriptPosition> SourceLines;

	static FStateDefine *FindStateLabelInList(TArray<FStateDefine> &list, FName name, bool create);
	static FStateLabels *CreateStateLabelList(TArray<FStateDefine> &statelist);
	static void MakeStateList(const FStateLabels *list, TArray<FStateDefine> &dest);
	static void RetargetStatePointers(intptr_t count, const char *target, TArray<FStateDefine> & statelist);
	FStateDefine *FindStateAddress(const char *name);
	FState *FindState(const char *name);

	FState *ResolveGotoLabel(AActor *actor, PClassActor *mytype, char *name);
	static void FixStatePointers(PClassActor *actor, TArray<FStateDefine> & list);
	void ResolveGotoLabels(PClassActor *actor, AActor *defaults, TArray<FStateDefine> & list);
public:

	FStateDefinitions()
	{
		laststate = NULL;
		laststatebeforelabel = NULL;
		lastlabel = -1;
	}

	void SetStateLabel(const char *statename, FState *state, BYTE defflags = SDF_STATE);
	void AddStateLabel(const char *statename);
	int GetStateLabelIndex (FName statename);
	void InstallStates(PClassActor *info, AActor *defaults);
	int FinishStates(PClassActor *actor, AActor *defaults);

	void MakeStateDefines(const PClassActor *cls);
	void AddStateDefines(const FStateLabels *list);
	void RetargetStates (intptr_t count, const char *target);

	bool SetGotoLabel(const char *string);
	bool SetStop();
	bool SetWait();
	bool SetLoop();
	int AddStates(FState *state, const char *framechars, const FScriptPosition &sc);
	int GetStateCount() const { return StateArray.Size(); }
};


void SaveStateSourceLines(FState *firststate, TArray<FScriptPosition> &positions);
FScriptPosition & GetStateSource(FState *state);

//==========================================================================
//
// Extra info maintained while defining an actor.
//
//==========================================================================
class DDropItem;

struct Baggage
{
#ifdef _DEBUG
	FString ClassName;	// This is here so that during debugging the class name can be seen
#endif
	PClassActor *Info;
	bool DropItemSet;
	bool StateSet;
	bool fromDecorate;
	int CurrentState;
	int Lumpnum;
	FStateDefinitions statedef;

	DDropItem *DropItemList;

	FScriptPosition ScriptPosition;
};

inline void ResetBaggage (Baggage *bag, PClassActor *stateclass)
{
	bag->DropItemList = NULL;
	bag->DropItemSet = false;
	bag->CurrentState = 0;
	bag->fromDecorate = true;
	bag->StateSet = false;
	bag->statedef.MakeStateDefines(stateclass);
}

//==========================================================================
//
// Action function lookup
//
//==========================================================================

AFuncDesc *FindFunction(PStruct *cls, const char * string);
FieldDesc *FindField(PStruct *cls, const char * string);


FxExpression *ParseExpression(FScanner &sc, PClassActor *cls, bool mustresolve = false);
void ParseStates(FScanner &sc, PClassActor *actor, AActor *defaults, Baggage &bag);
void ParseFunctionParameters(FScanner &sc, PClassActor *cls, TArray<FxExpression *> &out_params,
	PFunction *afd, FString statestring, FStateDefinitions *statedef);
FxExpression *ParseActions(FScanner &sc, FState state, FString statestring, Baggage &bag, bool &endswithret);
class FxVMFunctionCall *ParseAction(FScanner &sc, FState state, FString statestring, Baggage &bag);
FName CheckCastKludges(FName in);
void SetImplicitArgs(TArray<PType *> *args, TArray<DWORD> *argflags, TArray<FName> *argnames, PStruct *cls, DWORD funcflags, int useflags);
PFunction *CreateAnonymousFunction(PClass *containingclass, PType *returntype, int flags);
PFunction *FindClassMemberFunction(PStruct *cls, PStruct *funccls, FName name, FScriptPosition &sc, bool *error);
void CreateDamageFunction(PClassActor *info, AActor *defaults, FxExpression *id, bool fromDecorate, int lumpnum);

//==========================================================================
//
// Property parser
//
//==========================================================================

void HandleActorFlag(FScanner &sc, Baggage &bag, const char *part1, const char *part2, int mod);
FxExpression *ParseParameter(FScanner &sc, PClassActor *cls, PType *type, bool constant);


enum 
{
	DEPF_UNUSED,
	DEPF_FIREDAMAGE,
	DEPF_ICEDAMAGE,
	DEPF_LOWGRAVITY,
	DEPF_LONGMELEERANGE,
	DEPF_SHORTMISSILERANGE,
	DEPF_PICKUPFLASH,
	DEPF_QUARTERGRAVITY,
	DEPF_FIRERESIST,
	DEPF_HERETICBOUNCE,
	DEPF_HEXENBOUNCE,
	DEPF_DOOMBOUNCE,
	DEPF_INTERHUBSTRIP,
};

// Types of old style decorations
enum EDefinitionType
{
	DEF_Decoration,
	DEF_BreakableDecoration,
	DEF_Pickup,
	DEF_Projectile,
};

#if defined(_MSC_VER)
#pragma section(".greg$u",read)

#define MSVC_PSEG __declspec(allocate(".greg$u"))
#define GCC_PSEG
#else
#define MSVC_PSEG
#define GCC_PSEG __attribute__((section(SECTION_GREG))) __attribute__((used))
#endif


union FPropParam
{
	int i;
	double d;
	const char *s;
	FxExpression *exp;
};

typedef void (*PropHandler)(AActor *defaults, PClassActor *info, Baggage &bag, FPropParam *params);

enum ECategory
{
	CAT_PROPERTY,	// Inheritable property
	CAT_INFO		// non-inheritable info (spawn ID, Doomednum, game filter, conversation ID, not usable in ZScript)
};

struct FPropertyInfo
{
	const char *name;
	const char *params;
	const PClass * const *cls;
	PropHandler Handler;
	int category;
};

FPropertyInfo *FindProperty(const char * string);
int MatchString (const char *in, const char **strings);


#define DEFINE_PROPERTY_BASE(name, paramlist, clas, cat) \
	static void Handler_##name##_##paramlist##_##clas(A##clas *defaults, PClassActor *info, Baggage &bag, FPropParam *params); \
	static FPropertyInfo Prop_##name##_##paramlist##_##clas = \
		{ #name, #paramlist, &RUNTIME_CLASS_CASTLESS(A##clas), (PropHandler)Handler_##name##_##paramlist##_##clas, cat }; \
	MSVC_PSEG FPropertyInfo *infoptr_##name##_##paramlist##_##clas GCC_PSEG = &Prop_##name##_##paramlist##_##clas; \
	static void Handler_##name##_##paramlist##_##clas(A##clas *defaults, PClassActor *info, Baggage &bag, FPropParam *params)

#define DEFINE_PREFIXED_PROPERTY_BASE(prefix, name, paramlist, clas, cat) \
	static void Handler_##name##_##paramlist##_##clas(A##clas *defaults, PClassActor *info, Baggage &bag, FPropParam *params); \
	static FPropertyInfo Prop_##name##_##paramlist##_##clas = \
{ #prefix"."#name, #paramlist, &RUNTIME_CLASS_CASTLESS(A##clas), (PropHandler)Handler_##name##_##paramlist##_##clas, cat }; \
	MSVC_PSEG FPropertyInfo *infoptr_##name##_##paramlist##_##clas GCC_PSEG = &Prop_##name##_##paramlist##_##clas; \
	static void Handler_##name##_##paramlist##_##clas(A##clas *defaults, PClassActor *info, Baggage &bag, FPropParam *params)


#define DEFINE_PROPERTY(name, paramlist, clas) DEFINE_PROPERTY_BASE(name, paramlist, clas, CAT_PROPERTY)
#define DEFINE_INFO_PROPERTY(name, paramlist, clas) DEFINE_PROPERTY_BASE(name, paramlist, clas, CAT_INFO)

#define DEFINE_CLASS_PROPERTY(name, paramlist, clas) DEFINE_PREFIXED_PROPERTY_BASE(clas, name, paramlist, clas, CAT_PROPERTY)
#define DEFINE_CLASS_PROPERTY_PREFIX(prefix, name, paramlist, clas) DEFINE_PREFIXED_PROPERTY_BASE(prefix, name, paramlist, clas, CAT_PROPERTY)

#define PROP_PARM_COUNT (params[0].i)

#define PROP_STRING_PARM(var, no) \
	const char *var = params[(no)+1].s;

#define PROP_EXP_PARM(var, no) \
	FxExpression *var = params[(no)+1].exp;

#define PROP_INT_PARM(var, no) \
	int var = params[(no)+1].i;

#define PROP_FLOAT_PARM(var, no) \
	float var = float(params[(no)+1].d);

#define PROP_DOUBLE_PARM(var, no) \
	double var = params[(no)+1].d;

#define PROP_COLOR_PARM(var, no) \
	int var = params[(no)+1].i== 0? params[(no)+2].i : V_GetColor(NULL, params[(no)+2].s);

#endif
