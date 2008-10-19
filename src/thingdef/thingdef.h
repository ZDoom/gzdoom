#ifndef __THINGDEF_H
#define __THINGDEF_H

#include "doomtype.h"
#include "info.h"


class FScanner;


//==========================================================================
//
// A flag descriptor
//
//==========================================================================

struct FFlagDef
{
	int flagbit;
	const char *name;
	int structoffset;
};

FFlagDef *FindFlag (const PClass *type, const char *part1, const char *part2);
void HandleDeprecatedFlags(AActor *defaults, FActorInfo *info, bool set, int index);


//==========================================================================
//
// This class is for storing a name inside a const PClass* field without
// generating compiler warnings. It does not manipulate data in any other
// way.
//
//==========================================================================
class fuglyname : public FName
{
public:
	fuglyname() : FName() {}
	fuglyname(const char *foo) : FName(foo) {}
	operator const PClass *()
	{
		return reinterpret_cast<const PClass *>(size_t(int(*this)));
	}
	fuglyname &operator= (const PClass *foo)
	{
		FName *p = this;
		*p = ENamedName(reinterpret_cast<size_t>(foo));
		return *this;
	}
};


//==========================================================================
//
// State parser
//
//==========================================================================

extern TArray<int> StateParameters;
extern TArray<FName> JumpParameters;

struct FStateLabels;

enum EStateDefineFlags
{
	SDF_NEXT = 0,
	SDF_STATE = 1,
	SDF_STOP = 2,
	SDF_WAIT = 3,
	SDF_LABEL = 4,
	SDF_INDEX = 5,
};

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

	static FStateDefine *FindStateLabelInList(TArray<FStateDefine> &list, FName name, bool create);
	static FStateLabels *CreateStateLabelList(TArray<FStateDefine> &statelist);
	static void MakeStateList(const FStateLabels *list, TArray<FStateDefine> &dest);
	static void RetargetStatePointers (intptr_t count, const char *target, TArray<FStateDefine> & statelist);
	FStateDefine *FindStateAddress(const char *name);
	FState *FindState(const char *name);

	FState *ResolveGotoLabel (AActor *actor, const PClass *mytype, char *name);
	static void FixStatePointers (FActorInfo *actor, TArray<FStateDefine> & list);
	void ResolveGotoLabels (FActorInfo *actor, AActor *defaults, TArray<FStateDefine> & list);

public:


	void ClearStateLabels()
	{
		StateLabels.Clear();
	}

	void AddState (const char * statename, FState * state, BYTE defflags = SDF_STATE);
	void InstallStates(FActorInfo *info, AActor *defaults);
	int FinishStates (FActorInfo *actor, AActor *defaults, TArray<FState> &StateArray);

	void MakeStateDefines(const PClass *cls);
	void AddStateDefines(const FStateLabels *list);
	void RetargetStates (intptr_t count, const char *target);

};

//==========================================================================
//
// Extra info maintained while defining an actor.
//
//==========================================================================
struct FDropItem;

struct Baggage
{
#ifdef _DEBUG
	FString ClassName;	// This is here so that during debugging the class name can be seen
#endif
	FActorInfo *Info;
	bool DropItemSet;
	bool StateSet;
	int CurrentState;
	int Lumpnum;
	FStateDefinitions statedef;
	TArray<FState> StateArray;

	FDropItem *DropItemList;
};

inline void ResetBaggage (Baggage *bag, const PClass *stateclass)
{
	bag->DropItemList = NULL;
	bag->DropItemSet = false;
	bag->CurrentState = 0;
	bag->StateSet = false;
	bag->StateArray.Clear();
	bag->statedef.MakeStateDefines(stateclass);
}

//==========================================================================
//
// Action function lookup
//
//==========================================================================

struct AFuncDesc
{
	const char *Name;
	actionf_p Function;
};

AFuncDesc * FindFunction(const char * string);



FState *P_GetState(AActor *self, FState *CallingState, int offset);
int ParseStates(FScanner &sc, FActorInfo *actor, AActor *defaults, Baggage &bag);

PSymbolActionFunction *FindGlobalActionFunction(const char *name);

//==========================================================================
//
// Property parser
//
//==========================================================================

void ParseActorProperty(FScanner &sc, Baggage &bag);
void HandleActorFlag(FScanner &sc, Baggage &bag, const char *part1, const char *part2, int mod);
void ParseActorFlag (FScanner &sc, Baggage &bag, int mod);
void FinishActor(FScanner &sc, FActorInfo *info, Baggage &bag);

void ParseConstant (FScanner &sc, PSymbolTable *symt, PClass *cls);
void ParseVariable (FScanner &sc, PSymbolTable *symt, PClass *cls);
void ParseEnum (FScanner &sc, PSymbolTable *symt, PClass *cls);
int ParseParameter(FScanner &sc, PClass *cls, char type, bool constant);


int ParseExpression (FScanner &sc, bool _not, PClass *cls);

bool IsExpressionConst(int id);
int EvalExpressionI (int id, AActor *self);
double EvalExpressionF (int id, AActor *self);
fixed_t EvalExpressionFix (int id, AActor *self);

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
};

enum
{
	ACMETA_BASE				= 0x83000,
	ACMETA_DropItems,		// Int (index into DropItemList)
	ACMETA_ExplosionDamage,
	ACMETA_ExplosionRadius,
	ACMETA_DontHurtShooter,
	ACMETA_MeleeSound,
	ACMETA_MeleeDamage,
	ACMETA_MissileName,
	ACMETA_MissileHeight,
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
#pragma data_seg(".areg$u")
#pragma data_seg(".greg$u")
#pragma data_seg(".mreg$u")
#pragma data_seg()

#define MSVC_ASEG __declspec(allocate(".areg$u"))
#define GCC_ASEG
#define MSVC_PSEG __declspec(allocate(".greg$u"))
#define GCC_PSEG
#define MSVC_MSEG __declspec(allocate(".mreg$u"))
#define GCC_MSEG
#else
#define MSVC_ASEG
#define GCC_ASEG __attribute__((section(AREG_SECTION)))
#define MSVC_PSEG
#define GCC_PSEG __attribute__((section(GREG_SECTION)))
#define MSVC_MSEG
#define GCC_MSEG __attribute__((section(MREG_SECTION)))
#endif


union FPropParam
{
	int i;
	float f;
	const char *s;
};

typedef void (*PropHandler)(AActor *defaults, Baggage &bag, FPropParam *params);

enum ECategory
{
	CAT_PROPERTY,	// Inheritable property
	CAT_INFO		// non-inheritable info (spawn ID, Doomednum, game filter, conversation ID)
};

struct FPropertyInfo
{
	const char *name;
	const char *params;
	const PClass *cls;
	PropHandler Handler;
	int category;
};

struct FVariableInfo
{
	const char *name;
	intptr_t address;
	const PClass *owner;
};


FPropertyInfo *FindProperty(const char * string);
FVariableInfo *FindVariable(const char * string, const PClass *cls);
int MatchString (const char *in, const char **strings);


#define DEFINE_PROPERTY_BASE(name, paramlist, clas, cat) \
	static void Handler_##name##_##paramlist##_##clas(A##clas *defaults, Baggage &bag, FPropParam *params); \
	static FPropertyInfo Prop_##name##_##paramlist##_##clas = \
		{ #name, #paramlist, RUNTIME_CLASS(A##clas), (PropHandler)Handler_##name##_##paramlist##_##clas, cat }; \
	MSVC_PSEG FPropertyInfo *infoptr_##name##_##paramlist##_##clas GCC_PSEG = &Prop_##name##_##paramlist##_##clas; \
	static void Handler_##name##_##paramlist##_##clas(A##clas *defaults, Baggage &bag, FPropParam *params)

#define DEFINE_PREFIXED_PROPERTY_BASE(prefix, name, paramlist, clas, cat) \
	static void Handler_##name##_##paramlist##_##clas(A##clas *defaults, Baggage &bag, FPropParam *params); \
	static FPropertyInfo Prop_##name##_##paramlist##_##clas = \
{ #prefix"."#name, #paramlist, RUNTIME_CLASS(A##clas), (PropHandler)Handler_##name##_##paramlist##_##clas, cat }; \
	MSVC_PSEG FPropertyInfo *infoptr_##name##_##paramlist##_##clas GCC_PSEG = &Prop_##name##_##paramlist##_##clas; \
	static void Handler_##name##_##paramlist##_##clas(A##clas *defaults, Baggage &bag, FPropParam *params)


#define DEFINE_PROPERTY(name, paramlist, clas) DEFINE_PROPERTY_BASE(name, paramlist, clas, CAT_PROPERTY)
#define DEFINE_INFO_PROPERTY(name, paramlist, clas) DEFINE_PROPERTY_BASE(name, paramlist, clas, CAT_INFO)

#define DEFINE_CLASS_PROPERTY(name, paramlist, clas) DEFINE_PREFIXED_PROPERTY_BASE(clas, name, paramlist, clas, CAT_PROPERTY)
#define DEFINE_CLASS_PROPERTY_PREFIX(prefix, name, paramlist, clas) DEFINE_PREFIXED_PROPERTY_BASE(prefix, name, paramlist, clas, CAT_PROPERTY)

#define PROP_PARM_COUNT (params[0].i)

#define PROP_STRING_PARM(var, no) \
	const char *var = params[(no)+1].s;

#define PROP_INT_PARM(var, no) \
	int var = params[(no)+1].i;

#define PROP_FLOAT_PARM(var, no) \
	float var = params[(no)+1].f;

#define PROP_FIXED_PARM(var, no) \
	fixed_t var = fixed_t(params[(no)+1].f * FRACUNIT);

#define PROP_COLOR_PARM(var, no) \
	int var = params[(no)+1].i== 0? params[(no)+2].i : V_GetColor(NULL, params[(no)+2].s);


#define DEFINE_GLOBAL_VARIABLE(name) \
	static FVariableInfo GlobalDef__##name = { #name, intptr_t(&name), NULL }; \
	MSVC_MSEG FVariableInfo *infoptr_GlobalDef__##name = &GlobalDef__##name;

#define DEFINE_MEMBER_VARIABLE(name, cls) \
	static FVariableInfo GlobalDef__##name = { #name, myoffsetof(cls, name), RUNTIME_CLASS(cls) }; \
	MSVC_MSEG FVariableInfo *infoptr_GlobalDef__##name = &GlobalDef__##name;

	


struct StateCallData
{
	FState *State;
	AActor *Item;
	bool Result;
};

// Macros to handle action functions. These are here so that I don't have to
// change every single use in case the parameters change.
#define DECLARE_ACTION(name) void AF_##name(AActor *self, FState *, int, StateCallData *);

// This distinction is here so that CALL_ACTION produces errors when trying to
// access a function that requires parameters.
#define DEFINE_ACTION_FUNCTION(cls, name) \
	void AF_##name (AActor *self, FState *, int, StateCallData *); \
	static AFuncDesc info_##cls##_##name = { #name, AF_##name }; \
	MSVC_ASEG AFuncDesc *infoptr_##cls##_##name GCC_ASEG = &info_##cls##_##name; \
	void AF_##name (AActor *self, FState *, int, StateCallData *statecall)

#define DEFINE_ACTION_FUNCTION_PARAMS(cls, name) \
	void AFP_##name (AActor *self, FState *CallingState, int ParameterIndex, StateCallData *statecall); \
	static AFuncDesc info_##cls##_##name = { #name, AFP_##name }; \
	MSVC_ASEG AFuncDesc *infoptr_##cls##_##name GCC_ASEG = &info_##cls##_##name; \
	void AFP_##name (AActor *self, FState *CallingState, int ParameterIndex, StateCallData *statecall)

#define DECLARE_PARAMINFO FState *CallingState, int ParameterIndex, StateCallData *statecall
#define PUSH_PARAMINFO CallingState, ParameterIndex, statecall

#define CALL_ACTION(name,self) AF_##name(self, NULL, 0, NULL)

#define ACTION_PARAM_START(count)

#define ACTION_PARAM_CONST(var, i) \
	int var = StateParameters[ParameterIndex+i];
#define ACTION_PARAM_INT(var, i) \
	int var = EvalExpressionI(StateParameters[ParameterIndex+i], self);
#define ACTION_PARAM_BOOL(var,i) \
	bool var = !!EvalExpressionI(StateParameters[ParameterIndex+i], self);
#define ACTION_PARAM_FIXED(var,i) \
	fixed_t var = EvalExpressionFix(StateParameters[ParameterIndex+i], self);
#define ACTION_PARAM_FLOAT(var,i) \
	float var = EvalExpressionF(StateParameters[ParameterIndex+i], self);
#define ACTION_PARAM_CLASS(var,i) \
	const PClass *var = PClass::FindClass(ENamedName(StateParameters[ParameterIndex+i]));
#define ACTION_PARAM_STATE(var,i) \
	int var = StateParameters[ParameterIndex+i];
#define ACTION_PARAM_COLOR(var,i) \
	PalEntry var = StateParameters[ParameterIndex+i];
#define ACTION_PARAM_SOUND(var,i) \
	FSoundID var = StateParameters[ParameterIndex+i];
#define ACTION_PARAM_STRING(var,i) \
	const char *var = FName(ENamedName(StateParameters[ParameterIndex+i]));
#define ACTION_PARAM_NAME(var,i) \
	FName var = ENamedName(StateParameters[ParameterIndex+i]);
#define ACTION_PARAM_VARARG(var, i) \
	int *var = &StateParameters[ParameterIndex+i];

#define ACTION_PARAM_ANGLE(var,i) \
	angle_t var = angle_t(EvalExpressionF(StateParameters[ParameterIndex+i], self)*ANGLE_90/90.f);

#define ACTION_SET_RESULT(v) if (statecall != NULL) statecall->Result = v;

// Checks to see what called the current action function
#define ACTION_CALL_FROM_ACTOR() (CallingState == self->state)
#define ACTION_CALL_FROM_WEAPON() (self->player && CallingState != self->state && statecall == NULL)
#define ACTION_CALL_FROM_INVENTORY() (statecall != NULL)
#endif
