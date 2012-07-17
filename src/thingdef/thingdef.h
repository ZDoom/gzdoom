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
	int flagbit;
	const char *name;
	int structoffset;
};

FFlagDef *FindFlag (const PClass *type, const char *part1, const char *part2);
void HandleDeprecatedFlags(AActor *defaults, PClassActor *info, bool set, int index);
bool CheckDeprecatedFlags(AActor *actor, PClassActor *info, int index);
const char *GetFlagName(int flagnum, int flagoffset);

#define FLAG_NAME(flagnum, flagvar) GetFlagName(flagnum, myoffsetof(AActor, flagvar))


//==========================================================================
//
// State parser
//
//==========================================================================
class FxExpression;

struct FStateLabels;

enum EStateDefineFlags
{
	SDF_NEXT = 0,
	SDF_STATE = 1,
	SDF_STOP = 2,
	SDF_WAIT = 3,
	SDF_LABEL = 4,
	SDF_INDEX = 5,
	SDF_MASK = 7,
	SDF_DEHACKED = 8,	// Identify a state as having been modified by a dehacked lump
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
	FState *laststate;
	FState *laststatebeforelabel;
	intptr_t lastlabel;
	TArray<FState> StateArray;

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
	void InstallStates(PClassActor *info, AActor *defaults);
	int FinishStates(PClassActor *actor, AActor *defaults);

	void MakeStateDefines(const PClassActor *cls);
	void AddStateDefines(const FStateLabels *list);
	void RetargetStates (intptr_t count, const char *target);

	bool SetGotoLabel(const char *string);
	bool SetStop();
	bool SetWait();
	bool SetLoop();
	int AddStates(FState *state, const char *framechars);
	int GetStateCount() const { return StateArray.Size(); }
};

//==========================================================================
//
//
//
//==========================================================================

struct FStateTempCall
{
	FStateTempCall() : ActorClass(NULL), Function(NULL), FirstState(0), NumStates(0) {}

	PClassActor *ActorClass;
	VMFunction *Function;
	TArray<FxExpression *> Parameters;
	int FirstState;
	int NumStates;
};
extern TDeletingArray<FStateTempCall *> StateTempCalls;

struct FStateExpression
{
	FxExpression *expr;
	PClassActor *owner;
	bool constant;
	bool cloned;
};

class FStateExpressions
{
	TArray<FStateExpression> expressions;

public:
	~FStateExpressions() { Clear(); }
	void Clear();
	int Add(FxExpression *x, PClassActor *o, bool c);
	int Reserve(int num, PClassActor *cls);
	void Set(int num, FxExpression *x, bool cloned = false);
	void Copy(int dest, int src, int cnt);
	int ResolveAll();
	FxExpression *Get(int no);
	unsigned int Size() { return expressions.Size(); }
};

extern FStateExpressions StateParams;


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
	bag->StateSet = false;
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
	VMNativeFunction **VMPointer;
};

AFuncDesc *FindFunction(const char * string);


void ParseStates(FScanner &sc, PClassActor *actor, AActor *defaults, Baggage &bag);

PSymbolActionFunction *FindGlobalActionFunction(const char *name);

//==========================================================================
//
// Property parser
//
//==========================================================================

PClassActor *CreateNewActor(const FScriptPosition &sc, FName typeName, FName parentName, bool native);
void SetReplacement(PClassActor *info, FName replaceName);

void HandleActorFlag(FScanner &sc, Baggage &bag, const char *part1, const char *part2, int mod);
void FinishActor(const FScriptPosition &sc, PClassActor *info, Baggage &bag);
FxExpression *ParseParameter(FScanner &sc, PClassActor *cls, char type, bool constant);


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
#pragma section(".areg$u",read)
#pragma section(".greg$u",read)
#pragma section(".mreg$u",read)

#define MSVC_ASEG __declspec(allocate(".areg$u"))
#define GCC_ASEG
#define MSVC_PSEG __declspec(allocate(".greg$u"))
#define GCC_PSEG
#define MSVC_MSEG __declspec(allocate(".mreg$u"))
#define GCC_MSEG
#else
#define MSVC_ASEG
#define GCC_ASEG __attribute__((section(SECTION_AREG)))
#define MSVC_PSEG
#define GCC_PSEG __attribute__((section(SECTION_GREG)))
#define MSVC_MSEG
#define GCC_MSEG __attribute__((section(SECTION_MREG)))
#endif


union FPropParam
{
	int i;
	float f;
	const char *s;
};

typedef void (*PropHandler)(AActor *defaults, PClassActor *info, Baggage &bag, FPropParam *params);

enum ECategory
{
	CAT_PROPERTY,	// Inheritable property
	CAT_INFO		// non-inheritable info (spawn ID, Doomednum, game filter, conversation ID)
};

struct FPropertyInfo
{
	const char *name;
	const char *params;
	const PClass * const *cls;
	PropHandler Handler;
	int category;
};

struct FVariableInfo
{
	const char *name;
	intptr_t address;
	const PClass * const *owner;
};


FPropertyInfo *FindProperty(const char * string);
FVariableInfo *FindVariable(const char * string, const PClass *cls);
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

#define PROP_INT_PARM(var, no) \
	int var = params[(no)+1].i;

#define PROP_FLOAT_PARM(var, no) \
	float var = params[(no)+1].f;

#define PROP_FIXED_PARM(var, no) \
	fixed_t var = fixed_t(params[(no)+1].f * FRACUNIT);

#define PROP_COLOR_PARM(var, no) \
	int var = params[(no)+1].i== 0? params[(no)+2].i : V_GetColor(NULL, params[(no)+2].s);


#define DEFINE_MEMBER_VARIABLE(name, cls) \
	static FVariableInfo GlobalDef__##name = { #name, myoffsetof(cls, name), &RUNTIME_CLASS_CASTLESS(cls) }; \
	MSVC_MSEG FVariableInfo *infoptr_GlobalDef__##name GCC_MSEG = &GlobalDef__##name;

#define DEFINE_MEMBER_VARIABLE_ALIAS(name, alias, cls) \
	static FVariableInfo GlobalDef__##name = { #name, myoffsetof(cls, alias), &RUNTIME_CLASS_CASTLESS(cls) }; \
	MSVC_MSEG FVariableInfo *infoptr_GlobalDef__##name GCC_MSEG = &GlobalDef__##name;

	


struct StateCallData
{
	FState *State;
	bool Result;
};

// Macros to handle action functions. These are here so that I don't have to
// change every single use in case the parameters change.
#define DECLARE_ACTION(name)	extern VMNativeFunction *name##_VMPtr;

// This distinction is here so that CALL_ACTION produces errors when trying to
// access a function that requires parameters.
#define DEFINE_ACTION_FUNCTION(cls, name) \
	static int AF_##name(VM_ARGS); \
	VMNativeFunction *name##_VMPtr; \
	static const AFuncDesc cls##_##name##_Hook = { #name, AF_##name, &name##_VMPtr }; \
	extern AFuncDesc const *const cls##_##name##_HookPtr; \
	MSVC_ASEG AFuncDesc const *const cls##_##name##_HookPtr GCC_ASEG = &cls##_##name##_Hook; \
	static int AF_##name(VM_ARGS)

#define DEFINE_ACTION_FUNCTION_PARAMS(cls, name) DEFINE_ACTION_FUNCTION(cls, name)

//#define DECLARE_PARAMINFO AActor *self, AActor *stateowner, FState *CallingState, int ParameterIndex, StateCallData *statecall
//#define PUSH_PARAMINFO self, stateowner, CallingState, ParameterIndex, statecall

#define CALL_ACTION(name,self) { /*AF_##name(self, self, NULL, 0, NULL)*/ \
		VMValue params[5] = { self, self, VMValue(NULL, ATAG_STATE), VMValue(NULL, ATAG_GENERIC) }; \
		stack->Call(name##_VMPtr, params, countof(params), NULL, 0, NULL); \
	}


int EvalExpressionI (DWORD x, AActor *self);
int EvalExpressionCol (DWORD x, AActor *self);
FSoundID EvalExpressionSnd (DWORD x, AActor *self);
double EvalExpressionF (DWORD x, AActor *self);
fixed_t EvalExpressionFix (DWORD x, AActor *self);
FState *EvalExpressionState (DWORD x, AActor *self);
const PClass *EvalExpressionClass (DWORD x, AActor *self);
FName EvalExpressionName (DWORD x, AActor *self);

#if 0
#define ACTION_PARAM_START(count)

#define ACTION_PARAM_INT(var, i) \
	int var = EvalExpressionI(ParameterIndex+i, self);
#define ACTION_PARAM_BOOL(var,i) \
	bool var = !!EvalExpressionI(ParameterIndex+i, self);
#define ACTION_PARAM_FIXED(var,i) \
	fixed_t var = EvalExpressionFix(ParameterIndex+i, self);
#define ACTION_PARAM_FLOAT(var,i) \
	float var = float(EvalExpressionF(ParameterIndex+i, self));
#define ACTION_PARAM_DOUBLE(var,i) \
	double var = EvalExpressionF(ParameterIndex+i, self);
#define ACTION_PARAM_CLASS(var,i) \
	const PClass *var = EvalExpressionClass(ParameterIndex+i, self);
#define ACTION_PARAM_STATE(var,i) \
	FState *var = EvalExpressionState(ParameterIndex+i, stateowner);
#define ACTION_PARAM_COLOR(var,i) \
	PalEntry var = EvalExpressionCol(ParameterIndex+i, self);
#define ACTION_PARAM_SOUND(var,i) \
	FSoundID var = EvalExpressionSnd(ParameterIndex+i, self);
#define ACTION_PARAM_STRING(var,i) \
	const char *var = EvalExpressionName(ParameterIndex+i, self);
#define ACTION_PARAM_NAME(var,i) \
	FName var = EvalExpressionName(ParameterIndex+i, self);
#define ACTION_PARAM_ANGLE(var,i) \
	angle_t var = angle_t(EvalExpressionF(ParameterIndex+i, self)*ANGLE_90/90.f);
#endif

#define ACTION_SET_RESULT(v) if (statecall != NULL) statecall->Result = v;

// Checks to see what called the current action function
#define ACTION_CALL_FROM_ACTOR() (callingstate == self->state)
#define ACTION_CALL_FROM_WEAPON() (self->player && callingstate != self->state && statecall == NULL)
#define ACTION_CALL_FROM_INVENTORY() (statecall != NULL)
#endif
