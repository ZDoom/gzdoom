#include "sc_man.h"

// A stub to simulate the interface of .96x's expression evaluator.

int ParseExpression (bool _not)
{
	SC_MustGetFloat();
	if (_not)
	{
		if (sc_Float==0.f) sc_Float=1.f;
		else sc_Float=0.f;
	}
	
	return (int)(fixed_t)(sc_Float * FRACUNIT);
}

int EvalExpressionI (int id, AActor *self)
{
	return id>>FRACBITS;
}

bool EvalExpressionN(int id, AActor * self)
{
	return !EvalExpressionI(id, self);
}

float EvalExpressionF (int id, AActor *self)
{
	return (float)id/FRACUNIT;
}

