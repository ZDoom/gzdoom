#ifndef __SC_MAN_H__
#define __SC_MAN_H__

class FScanner
{
public:
	struct SavedPos
	{
		const char *SavedScriptPtr;
		int SavedScriptLine;
	};

	// Methods ------------------------------------------------------
	FScanner();
	FScanner(const FScanner &other);
	FScanner(int lumpnum);
	~FScanner();

	FScanner &operator=(const FScanner &other);

	void Open(const char *lumpname);
	void OpenFile(const char *filename);
	void OpenMem(const char *name, const char *buffer, int size);
	void OpenLumpNum(int lump);
	void Close();

	void SetCMode(bool cmode);
	void SetEscape(bool esc);
	const SavedPos SavePos();
	void RestorePos(const SavedPos &pos);

	static FString TokenName(int token, const char *string=NULL);

	bool GetString();
	void MustGetString();
	void MustGetStringName(const char *name);
	bool CheckString(const char *name);

	bool GetToken();
	void MustGetAnyToken();
	void TokenMustBe(int token);
	void MustGetToken(int token);
	bool CheckToken(int token);
	bool CheckTokenId(ENamedName id);

	bool GetNumber();
	void MustGetNumber();
	bool CheckNumber();

	bool GetFloat();
	void MustGetFloat();
	bool CheckFloat();

	void UnGet();

	bool Compare(const char *text);
	int MatchString(const char **strings);
	int MustMatchString(const char **strings);
	int GetMessageLine();

	void ScriptError(const char *message, ...);
	void ScriptMessage(const char *message, ...);

	// Members ------------------------------------------------------
	char *String;
	int StringLen;
	int TokenType;
	int Number;
	double Float;
	FName Name;
	int Line;
	bool End;
	bool Crossed;
	int LumpNum;
	FString ScriptName;

protected:
	void PrepareScript();
	void CheckOpen();
	bool ScanString(bool tokens);

	// Strings longer than this minus one will be dynamically allocated.
	static const int MAX_STRING_SIZE = 128;

	bool ScriptOpen;
	FString ScriptBuffer;
	const char *ScriptPtr;
	const char *ScriptEndPtr;
	char StringBuffer[MAX_STRING_SIZE];
	FString BigStringBuffer;
	bool AlreadyGot;
	int AlreadyGotLine;
	bool LastGotToken;
	const char *LastGotPtr;
	int LastGotLine;
	bool CMode;
	bool Escape;
};

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
	TK_Include,
	TK_Fixed_t,
	TK_Angle_t,
	TK_Abs,
	TK_Random,
	TK_Random2,

	TK_LastToken
};


//==========================================================================
//
//
//
//==========================================================================

enum
{
	MSG_WARNING,
	MSG_FATAL,
	MSG_ERROR,
	MSG_DEBUG,
	MSG_LOG,
	MSG_DEBUGLOG
};

//==========================================================================
//
// a class that remembers a parser position
//
//==========================================================================

struct FScriptPosition
{
	static int ErrorCounter;
	FString FileName;
	int ScriptLine;

	FScriptPosition()
	{
		ScriptLine=0;
	}
	FScriptPosition(const FScriptPosition &other);
	FScriptPosition(FString fname, int line);
	FScriptPosition(FScanner &sc);
	FScriptPosition &operator=(const FScriptPosition &other);
	void Message(int severity, const char *message,...) const;
	static void ResetErrorCounter()
	{
		ErrorCounter = 0;
	}
};


#endif //__SC_MAN_H__
