#ifndef __THINGDEF_H
#define __THINGDEF_H

int ParseExpression (bool _not);

int EvalExpressionI (int id, AActor *self);
float EvalExpressionF (int id, AActor *self);
bool EvalExpressionN (int id, AActor *self);

#endif