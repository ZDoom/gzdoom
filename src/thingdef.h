#ifndef __THINGDEF_H
#define __THINGDEF_H

// This class is for storing a name inside a const PClass* field without
// generating compiler warnings. It does not manipulate data in any other
// way.
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

// All state parameters are stored in this array now.
extern TArray<int> StateParameters;
extern TArray<int> JumpParameters;


int ParseExpression (bool _not);

int EvalExpressionI (int id, AActor *self);
float EvalExpressionF (int id, AActor *self);
bool EvalExpressionN (int id, AActor *self);

void ClearStateLabels();
void AddState (const char * statename, FState * state);
FState * FindState(AActor * actor, const PClass * type, const char * name);
void InstallStates(FActorInfo *info, AActor *defaults);
void MakeStateDefines(const FStateLabels *list);

struct FDropItem 
{
	FName Name;
	int probability;
	int amount;
	FDropItem * Next;
};

FDropItem *GetDropItems(const PClass * cls);


// A truly awful hack to get to the state that called an action function
// without knowing whether it has been called from a weapon or actor.
extern FState * CallingState;
int CheckIndex(int paramsize, FState ** pcallstate=NULL);

void A_ExplodeParms(AActor * self);

enum
{
	ACMETA_BASE				= 0x83000,
	ACMETA_DropItems,		// Int (index into DropItemList)
	ACMETA_ExplosionDamage,
	ACMETA_ExplosionRadius,
	ACMETA_DontHurtShooter,
};


// The contents of string must be lowercase (for now, anyway)
int FindLineSpecialEx (const char *string, int *minargs, int *maxargs);



#endif
