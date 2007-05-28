#ifndef __SC_MAN_H__
#define __SC_MAN_H__

void SC_Open (const char *name);
void SC_OpenFile (const char *name);
void SC_OpenMem (const char *name, char *buffer, int size);
void SC_OpenLumpNum (int lump, const char *name);
void SC_Close (void);
void SC_SetCMode (bool cmode);
void SC_SetEscape (bool esc);
void SC_SavePos (void);
void SC_RestorePos (void);
FString SC_TokenName (int token, const char *string=NULL);
bool SC_GetString (void);
void SC_MustGetString (void);
void SC_MustGetStringName (const char *name);
bool SC_CheckString (const char *name);
bool SC_GetToken (void);
void SC_MustGetAnyToken (void);
void SC_TokenMustBe (int token);
void SC_MustGetToken (int token);
bool SC_CheckToken (int token);
bool SC_CheckTokenId (ENamedName id);
bool SC_GetNumber (void);
void SC_MustGetNumber (void);
bool SC_CheckNumber (void);
bool SC_CheckFloat (void);
bool SC_GetFloat (void);
void SC_MustGetFloat (void);
void SC_UnGet (void);
//boolean SC_Check(void);
bool SC_Compare (const char *text);
int SC_MatchString (const char **strings);
int SC_MustMatchString (const char **strings);
void STACK_ARGS SC_ScriptError (const char *message, ...);
void SC_SaveScriptState();
void SC_RestoreScriptState();	

enum
{
	TK_Identifier = 257,
	TK_StringConst,
	TK_NameConst,
	TK_IntConst,
	TK_FloatConst,
	TK_Ellipsis,		// ...
	TK_RShiftEq,		// >>=
	TK_URShiftEq,		// >>>=
	TK_LShiftEq,		// <<=
	TK_AddEq,			// +=
	TK_SubEq,			// -=
	TK_MulEq,			// *=
	TK_DivEq,			// /=
	TK_ModEq,			// %=
	TK_AndEq,			// &=
	TK_XorEq,			// ^=
	TK_OrEq,			// |=
	TK_RShift,			// >>
	TK_URShift,			// >>>
	TK_LShift,			// <<
	TK_Incr,			// ++
	TK_Decr,			// --
	TK_AndAnd,			// &&
	TK_OrOr,			// ||
	TK_Leq,				// <=
	TK_Geq,				// >=
	TK_Eq,				// ==
	TK_Neq,				// !=
	TK_Action,
	TK_Break,
	TK_Case,
	TK_Const,
	TK_Continue,
	TK_Default,
	TK_Do,
	TK_Else,
	TK_For,
	TK_If,
	TK_Return,
	TK_Switch,
	TK_Until,
	TK_While,
	TK_Bool,
	TK_Float,
	TK_Double,
	TK_Char,
	TK_Byte,
	TK_SByte,
	TK_Short,
	TK_UShort,
	TK_Int,
	TK_UInt,
	TK_Long,
	TK_ULong,
	TK_Void,
	TK_Struct,
	TK_Class,
	TK_Mode,
	TK_Enum,
	TK_Name,
	TK_String,
	TK_Sound,
	TK_State,
	TK_Color,
	TK_Goto,
	TK_Abstract,
	TK_ForEach,
	TK_True,
	TK_False,
	TK_None,
	TK_New,
	TK_InstanceOf,
	TK_Auto,
	TK_Exec,
	TK_DefaultProperties,
	TK_Native,
	TK_Out,
	TK_Ref,
	TK_Event,
	TK_Static,
	TK_Transient,
	TK_Volatile,
	TK_Final,
	TK_Throws,
	TK_Extends,
	TK_Public,
	TK_Protected,
	TK_Private,
	TK_Dot,
	TK_Cross,
	TK_Ignores,
	TK_Localized,
	TK_Latent,
	TK_Singular,
	TK_Config,
	TK_Coerce,
	TK_Iterator,
	TK_Optional,
	TK_Export,
	TK_Virtual,
	TK_Super,
	TK_Global,
	TK_Self,
	TK_Stop,
	TK_Eval,
	TK_EvalNot,
	TK_Pickup,
	TK_Breakable,
	TK_Projectile,
	TK_Include,

	TK_LastToken
};

extern int sc_TokenType;
extern char *sc_String;
extern unsigned int sc_StringLen;
extern int sc_Number;
extern double sc_Float;
extern FName sc_Name;
extern int sc_Line;
extern bool sc_End;
extern bool sc_Crossed;
extern bool sc_FileScripts;

#endif //__SC_MAN_H__
