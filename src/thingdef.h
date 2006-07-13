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


int ParseExpression (bool _not);

int EvalExpressionI (int id, AActor *self);
float EvalExpressionF (int id, AActor *self);
bool EvalExpressionN (int id, AActor *self);


struct FDropItem 
{
	FName Name;
	int probability;
	int amount;
	FDropItem * Next;
};

FDropItem *GetDropItems(AActor * actor);


// A truly awful hack to get to the state that called an action function
// without knowing whether it has been called from a weapon or actor.
extern FState * CallingState;
int CheckIndex(int paramsize, FState ** pcallstate=NULL);




#endif
