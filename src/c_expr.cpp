/*
** c_expr.cpp
** Console commands dealing with mathematical expressions
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "c_dispatch.h"
#include "c_cvars.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum EProductionType
{
	PROD_String, PROD_Double
};

struct FProduction
{
	EProductionType Type;
};

struct FStringProd : public FProduction
{
	char Value[1];
};

struct FDoubleProd : public FProduction
{
	double Value;
};

struct FProducer
{
	char Token[4];
	FProduction *(*DoubleProducer) (FDoubleProd *prod1, FDoubleProd *prod2);
	FProduction *(*StringProducer) (FStringProd *prod1, FStringProd *prod2);
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

bool IsFloat (const char *str);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static FProduction *ParseExpression (FCommandLine &argv, int &parsept);
static const char *IsNum (const char *str);
static FStringProd *NewStringProd (const char *str);
static FStringProd *NewStringProd (size_t len);
static FDoubleProd *NewDoubleProd (double val);
static FStringProd *DoubleToString (FProduction *prod);
static FDoubleProd *StringToDouble (FProduction *prod);
void MaybeStringCoerce (FProduction *&prod1, FProduction *&prod2);
void MustStringCoerce (FProduction *&prod1, FProduction *&prod2);
void DoubleCoerce (FProduction *&prod1, FProduction *&prod2);

FProduction *ProdAddDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdAddStr  (FStringProd *prod1, FStringProd *prod2);
FProduction *ProdSubDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdMulDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdDivDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdModDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdPowDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdLTDbl   (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdLTEDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdGTDbl   (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdGTEDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdEqDbl   (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdNeqDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdXorDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdAndDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdOrDbl   (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdLAndDbl (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdLOrDbl  (FDoubleProd *prod1, FDoubleProd *prod2);
FProduction *ProdLTStr   (FStringProd *prod1, FStringProd *prod2);
FProduction *ProdLTEStr  (FStringProd *prod1, FStringProd *prod2);
FProduction *ProdGTStr   (FStringProd *prod1, FStringProd *prod2);
FProduction *ProdGTEStr  (FStringProd *prod1, FStringProd *prod2);
FProduction *ProdEqStr   (FStringProd *prod1, FStringProd *prod2);
FProduction *ProdNeqStr  (FStringProd *prod1, FStringProd *prod2);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FProducer Producers[] =
{
	{ "+",   ProdAddDbl, ProdAddStr },
	{ "-",   ProdSubDbl, NULL },
	{ "*",   ProdMulDbl, NULL },
	{ "/",   ProdDivDbl, NULL },
	{ "%",   ProdModDbl, NULL },
	{ "^",   ProdPowDbl, NULL },
	{ "<",   ProdLTDbl,  ProdLTStr },
	{ "<=",  ProdLTEDbl, ProdLTEStr },
	{ ">",   ProdGTDbl,  ProdGTStr },
	{ ">=",  ProdGTEDbl, ProdGTEStr },
	{ "=",   ProdEqDbl,  ProdEqStr },
	{ "==",  ProdEqDbl,  ProdEqStr },
	{ "!=",  ProdNeqDbl, ProdNeqStr },
	{ "<>",  ProdNeqDbl, ProdNeqStr },
	{ "xor", ProdXorDbl, NULL },
	{ "&",   ProdAndDbl, NULL },
	{ "|",   ProdOrDbl,  NULL },
	{ "&&",  ProdLAndDbl, NULL },
	{ "||",  ProdLOrDbl, NULL }
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// ParseExpression
//
// Builds a production from an expression. The supported syntax is LISP-like
// but without parentheses.
//
//==========================================================================

static FProduction *ParseExpression (FCommandLine &argv, int &parsept)
{
	if (parsept >= argv.argc())
		return NULL;

	const char *token = argv[parsept++];
	FProduction *prod1 = NULL, *prod2 = NULL, *prod3 = NULL;

	if (IsFloat (token))
	{
		return NewDoubleProd (atof(token));
	}
	else if (stricmp (token, "true") == 0)
	{
		return NewDoubleProd (1.0);
	}
	else if (stricmp (token, "false") == 0)
	{
		return NewDoubleProd (0.0);
	}
	else
	{
		for (size_t i = 0; i < countof(Producers); ++i)
		{
			if (strcmp (Producers[i].Token, token) == 0)
			{
				prod1 = ParseExpression (argv, parsept);
				prod2 = ParseExpression (argv, parsept);
				if (prod1 == NULL || prod2 == NULL)
				{
					goto missing;
				}
				if (Producers[i].StringProducer == NULL)
				{
					DoubleCoerce (prod1, prod2);
				}
				else if (Producers[i].DoubleProducer == NULL)
				{
					MustStringCoerce (prod1, prod2);
				}
				else
				{
					MaybeStringCoerce (prod1, prod2);
				}
				if (prod1->Type == PROD_String)
				{
					prod3 = Producers[i].StringProducer ((FStringProd *)prod1, (FStringProd *)prod2);
				}
				else
				{
					prod3 = Producers[i].DoubleProducer ((FDoubleProd *)prod1, (FDoubleProd *)prod2);
				}
				goto done;
			}
		}
		if (strcmp ("!", token) == 0)
		{
			prod1 = ParseExpression (argv, parsept);
			if (prod1 == NULL)
			{
				goto missing;
			}
			if (prod1->Type == PROD_String)
			{
				prod1 = StringToDouble (prod1);
			}
			prod3 = NewDoubleProd (!static_cast<FDoubleProd *>(prod1)->Value);
			goto done;
		}
		return NewStringProd (token);
	}

missing:
	Printf ("Missing argument to %s\n", token);

done:
	if (prod2 != NULL) M_Free (prod2);
	if (prod1 != NULL) M_Free (prod1);
	return prod3;
}

//==========================================================================
//
// IsFloat
//
//==========================================================================

bool IsFloat (const char *str)
{
	const char *pt;
	
	if (*str == '+' || *str == '-')
		str++;

	if (*str == '.')
	{
		pt = str;
	}
	else
	{
		pt = IsNum (str);
		if (pt == NULL)
			return false;
	}
	if (*pt == '.')
	{
		pt = IsNum (pt+1);
		if (pt == NULL)
			return false;
	}
	if (*pt == 'e' || *pt == 'E')
	{
		pt++;
		if (*pt == '+' || *pt == '-')
			pt++;
		pt = IsNum (pt);
	}
	return pt != NULL && *pt == 0;
}

//==========================================================================
//
// IsNum
//
//==========================================================================

static const char *IsNum (const char *str)
{
	const char *start = str;

	while (*str)
	{
		if (*str >= '0' && *str <= '9')
			str++;
		else
			break;
	}

	return (str > start) ? str : NULL;
}

//==========================================================================
//
// NewStringProd (from a string)
//
//==========================================================================

static FStringProd *NewStringProd (const char *str)
{
	FStringProd *prod = (FStringProd *)M_Malloc (sizeof(FStringProd)+strlen(str));
	prod->Type = PROD_String;
	strcpy (prod->Value, str);
	return prod;
}

//==========================================================================
//
// NewStringProd (from a length)
//
//==========================================================================

static FStringProd *NewStringProd (size_t len)
{
	FStringProd *prod = (FStringProd *)M_Malloc (sizeof(FStringProd)+len);
	prod->Type = PROD_String;
	prod->Value[0] = 0;
	return prod;
}

//==========================================================================
//
// NewDoubleProd
//
//==========================================================================

static FDoubleProd *NewDoubleProd (double val)
{
	FDoubleProd *prod = (FDoubleProd *)M_Malloc (sizeof(FDoubleProd));
	prod->Type = PROD_Double;
	prod->Value = val;
	return prod;
}

//==========================================================================
//
// DoubleToString
//
//==========================================================================

static FStringProd *DoubleToString (FProduction *prod)
{
	char buf[128];
	FStringProd *newprod;

	mysnprintf (buf, countof(buf), "%g", static_cast<FDoubleProd *>(prod)->Value);
	newprod = NewStringProd (buf);
	M_Free (prod);
	return newprod;
}

//==========================================================================
//
// StringToDouble
//
//==========================================================================

static FDoubleProd *StringToDouble (FProduction *prod)
{
	FDoubleProd *newprod;
	
	newprod = NewDoubleProd (atof (static_cast<FStringProd *>(prod)->Value));
	M_Free (prod);
	return newprod;
}

//==========================================================================
//
// MaybeStringCoerce
//
// If one of the parameters is a string, convert the other to a string.
//
//==========================================================================

void MaybeStringCoerce (FProduction *&prod1, FProduction *&prod2)
{
	if (prod1->Type == PROD_String)
	{
		if (prod2->Type == PROD_Double)
		{
			prod2 = DoubleToString (prod2);
		}
	}
	else if (prod2->Type == PROD_String)
	{
		prod1 = DoubleToString (prod1);
	}
}

//==========================================================================
//
// MustStringCoerce
//
// Ensures that both parameters are strings
//
//==========================================================================

void MustStringCoerce (FProduction *&prod1, FProduction *&prod2)
{
	if (prod1->Type == PROD_Double)
	{
		prod1 = DoubleToString (prod1);
	}
	if (prod2->Type == PROD_Double)
	{
		prod2 = DoubleToString (prod2);
	}
}

//==========================================================================
//
// DoubleCoerce
//
// Ensures that both parameters are doubles
//
//==========================================================================

void DoubleCoerce (FProduction *&prod1, FProduction *&prod2)
{
	if (prod1->Type == PROD_String)
	{
		prod1 = StringToDouble (prod1);
	}
	if (prod2->Type == PROD_String)
	{
		prod2 = StringToDouble (prod2);
	}
}

//==========================================================================
//
// ProdAddDbl
//
//==========================================================================

FProduction *ProdAddDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value + prod2->Value);
}

//==========================================================================
//
// ProdAddStr
//
//==========================================================================

FProduction *ProdAddStr (FStringProd *prod1, FStringProd *prod2)
{
	size_t len = strlen (prod1->Value) + strlen (prod2->Value) + 1;
	FStringProd *prod = NewStringProd (len);
	strcpy (prod->Value, prod1->Value);
	strcat (prod->Value, prod2->Value);
	return prod;
}

//==========================================================================
//
// ProdSubDbl
//
//==========================================================================

FProduction *ProdSubDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value - prod2->Value);
}

//==========================================================================
//
// ProdMulDbl
//
//==========================================================================

FProduction *ProdMulDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value * prod2->Value);
}

//==========================================================================
//
// ProdDivDbl
//
//==========================================================================

FProduction *ProdDivDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value / prod2->Value);
}

//==========================================================================
//
// ProdModDbl
//
//==========================================================================

FProduction *ProdModDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (fmod (prod1->Value, prod2->Value));
}

//==========================================================================
//
// ProdPowDbl
//
//==========================================================================

FProduction *ProdPowDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (pow (prod1->Value, prod2->Value));
}

//==========================================================================
//
// ProdLTDbl
//
//==========================================================================

FProduction *ProdLTDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value < prod2->Value);
}

//==========================================================================
//
// ProdLTEDbl
//
//==========================================================================

FProduction *ProdLTEDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value <= prod2->Value);
}

//==========================================================================
//
// ProdGTDbl
//
//==========================================================================

FProduction *ProdGTDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value > prod2->Value);
}

//==========================================================================
//
// ProdGTEDbl
//
//==========================================================================

FProduction *ProdGTEDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value >= prod2->Value);
}

//==========================================================================
//
// ProdEqDbl
//
//==========================================================================

FProduction *ProdEqDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value == prod2->Value);
}

//==========================================================================
//
// ProdNeqDbl
//
//==========================================================================

FProduction *ProdNeqDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd (prod1->Value != prod2->Value);
}

//==========================================================================
//
// ProdLTStr
//
//==========================================================================

FProduction *ProdLTStr (FStringProd *prod1, FStringProd *prod2)
{
	return NewDoubleProd (stricmp (prod1->Value, prod2->Value) < 0);
}

//==========================================================================
//
// ProdLTEStr
//
//==========================================================================

FProduction *ProdLTEStr (FStringProd *prod1, FStringProd *prod2)
{
	return NewDoubleProd (stricmp (prod1->Value, prod2->Value) <= 0);
}

//==========================================================================
//
// ProdGTStr
//
//==========================================================================

FProduction *ProdGTStr (FStringProd *prod1, FStringProd *prod2)
{
	return NewDoubleProd (stricmp (prod1->Value, prod2->Value) > 0);
}

//==========================================================================
//
// ProdGTEStr
//
//==========================================================================

FProduction *ProdGTEStr (FStringProd *prod1, FStringProd *prod2)
{
	return NewDoubleProd (stricmp (prod1->Value, prod2->Value) >= 0);
}

//==========================================================================
//
// ProdEqStr
//
//==========================================================================

FProduction *ProdEqStr (FStringProd *prod1, FStringProd *prod2)
{
	return NewDoubleProd (stricmp (prod1->Value, prod2->Value) == 0);
}

//==========================================================================
//
// ProdNeqStr
//
//==========================================================================

FProduction *ProdNeqStr (FStringProd *prod1, FStringProd *prod2)
{
	return NewDoubleProd (stricmp (prod1->Value, prod2->Value) != 0);
}

//==========================================================================
//
// ProdXorDbl
//
//==========================================================================

FProduction *ProdXorDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd ((double)((SQWORD)prod1->Value ^ (SQWORD)prod2->Value));
}

//==========================================================================
//
// ProdAndDbl
//
//==========================================================================

FProduction *ProdAndDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd ((double)((SQWORD)prod1->Value & (SQWORD)prod2->Value));
}

//==========================================================================
//
// ProdOrDbl
//
//==========================================================================

FProduction *ProdOrDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd ((double)((SQWORD)prod1->Value | (SQWORD)prod2->Value));
}

//==========================================================================
//
// ProdLAndDbl
//
//==========================================================================

FProduction *ProdLAndDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd ((double)((SQWORD)prod1->Value && (SQWORD)prod2->Value));
}

//==========================================================================
//
// ProdLOrDbl
//
//==========================================================================

FProduction *ProdLOrDbl (FDoubleProd *prod1, FDoubleProd *prod2)
{
	return NewDoubleProd ((double)((SQWORD)prod1->Value || (SQWORD)prod2->Value));
}


//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
// CCMD test
//
// If <expr> is non-zero, execute <true cmd>.
// If <expr> is zero, execute [false cmd] if specified.
// 
//==========================================================================

CCMD (test)
{
	int parsept = 1;
	FProduction *prod = ParseExpression (argv, parsept);

	if (prod == NULL || parsept >= argv.argc())
	{
		Printf ("Usage: test <expr> <true cmd> [false cmd]\n");
	}
	else
	{
		if (prod->Type == PROD_String)
		{
			prod = StringToDouble (prod);
		}

		if (static_cast<FDoubleProd *>(prod)->Value != 0.0)
		{
			AddCommandString (argv[parsept]);
		}
		else if (++parsept < argv.argc())
		{
			AddCommandString (argv[parsept]);
		}
	}
	if (prod != NULL)
	{
		M_Free (prod);
	}
}

//==========================================================================
//
// CCMD eval
//
// Evaluates an expression and either prints it to the console or stores
// it in an existing cvar.
//
//==========================================================================

CCMD (eval)
{
	if (argv.argc() >= 2)
	{
		int parsept = 1;
		FProduction *prod = ParseExpression (argv, parsept);

		if (prod != NULL)
		{
			if (parsept < argv.argc())
			{
				FBaseCVar *var = FindCVar (argv[parsept], NULL);
				if (var == NULL)
				{
					Printf ("Unknown variable %s\n", argv[parsept]);
				}
				else
				{
					UCVarValue val;

					if (prod->Type == PROD_Double)
					{
						val.Float = (float)static_cast<FDoubleProd *>(prod)->Value;
						var->SetGenericRep (val, CVAR_Float);
					}
					else
					{
						val.String = static_cast<FStringProd *>(prod)->Value;
						var->SetGenericRep (val, CVAR_String);
					}
				}
			}
			else
			{
				if (prod->Type == PROD_Double)
				{
					Printf ("%g\n", static_cast<FDoubleProd *>(prod)->Value);
				}
				else
				{
					Printf ("%s\n", static_cast<FStringProd *>(prod)->Value);
				}
			}
			M_Free (prod);
			return;
		}
	}

	Printf ("Usage: eval <expression> [variable]\n");
}
