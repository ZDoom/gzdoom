// p_acs.h: ACS Script Stuff

#ifndef __P_ACS_H__
#define __P_ACS_H__

#include "dobject.h"

#define STACK_SIZE	200
#define LOCAL_SIZE	10

class DLevelScript : public DObject
{
	DECLARE_SERIAL (DLevelScript, DObject)
public:

	// P-codes for ACS scripts
	enum
	{
		PCD_NOP =					0,
		PCD_TERMINATE =				1,
		PCD_SUSPEND =				2,
		PCD_PUSHNUMBER =			3,
		PCD_LSPEC1 =				4,
		PCD_LSPEC2 =				5,
		PCD_LSPEC3 =				6,
		PCD_LSPEC4 =				7,
		PCD_LSPEC5 =				8,
		PCD_LSPEC1DIRECT =			9,
		PCD_LSPEC2DIRECT =			10,
		PCD_LSPEC3DIRECT =			11,
		PCD_LSPEC4DIRECT =			12,
		PCD_LSPEC5DIRECT =			13,
		PCD_ADD =					14,
		PCD_SUBTRACT =				15,
		PCD_MULTIPLY =				16,
		PCD_DIVIDE =				17,
		PCD_MODULUS =				18,
		PCD_EQ =					19,
		PCD_NE =					20,
		PCD_LT =					21,
		PCD_GT =					22,
		PCD_LE =					23,
		PCD_GE =					24,
		PCD_ASSIGNSCRIPTVAR =		25,
		PCD_ASSIGNMAPVAR =			26,
		PCD_ASSIGNWORLDVAR =		27,
		PCD_PUSHSCRIPTVAR =			28,
		PCD_PUSHMAPVAR =			29,
		PCD_PUSHWORLDVAR =			30,
		PCD_ADDSCRIPTVAR =			31,
		PCD_ADDMAPVAR =				32,
		PCD_ADDWORLDVAR =			33,
		PCD_SUBSCRIPTVAR =			34,
		PCD_SUBMAPVAR =				35,
		PCD_SUBWORLDVAR =			36,
		PCD_MULSCRIPTVAR =			37,
		PCD_MULMAPVAR =				38,
		PCD_MULWORLDVAR =			39,
		PCD_DIVSCRIPTVAR =			40,
		PCD_DIVMAPVAR =				41,
		PCD_DIVWORLDVAR =			42,
		PCD_MODSCRIPTVAR =			43,
		PCD_MODMAPVAR =				44,
		PCD_MODWORLDVAR =			45,
		PCD_INCSCRIPTVAR =			46,
		PCD_INCMAPVAR =				47,
		PCD_INCWORLDVAR =			48,
		PCD_DECSCRIPTVAR =			49,
		PCD_DECMAPVAR =				50,
		PCD_DECWORLDVAR =			51,
		PCD_GOTO =					52,
		PCD_IFGOTO =				53,
		PCD_DROP =					54,
		PCD_DELAY =					55,
		PCD_DELAYDIRECT =			56,
		PCD_RANDOM =				57,
		PCD_RANDOMDIRECT =			58,
		PCD_THINGCOUNT =			59,
		PCD_THINGCOUNTDIRECT =		60,
		PCD_TAGWAIT =				61,
		PCD_TAGWAITDIRECT =			62,
		PCD_POLYWAIT =				63,
		PCD_POLYWAITDIRECT =		64,
		PCD_CHANGEFLOOR =			65,
		PCD_CHANGEFLOORDIRECT =		66,
		PCD_CHANGECEILING =			67,
		PCD_CHANGECEILINGDIRECT =	68,
		PCD_RESTART =				69,
		PCD_ANDLOGICAL =			70,
		PCD_ORLOGICAL =				71,
		PCD_ANDBITWISE =			72,
		PCD_ORBITWISE =				73,
		PCD_EORBITWISE =			74,
		PCD_NEGATELOGICAL =			75,
		PCD_LSHIFT =				76,
		PCD_RSHIFT =				77,
		PCD_UNARYMINUS =			78,
		PCD_IFNOTGOTO =				79,
		PCD_LINESIDE =				80,
		PCD_SCRIPTWAIT =			81,
		PCD_SCRIPTWAITDIRECT =		82,
		PCD_CLEARLINESPECIAL =		83,
		PCD_CASEGOTO =				84,
		PCD_BEGINPRINT =			85,
		PCD_ENDPRINT =				86,
		PCD_PRINTSTRING =			87,
		PCD_PRINTNUMBER =			88,
		PCD_PRINTCHARACTER =		89,
		PCD_PLAYERCOUNT =			90,
		PCD_GAMETYPE =				91,
		PCD_GAMESKILL =				92,
		PCD_TIMER =					93,
		PCD_SECTORSOUND =			94,
		PCD_AMBIENTSOUND =			95,
		PCD_SOUNDSEQUENCE =			96,
		PCD_SETLINETEXTURE =		97,
		PCD_SETLINEBLOCKING =		98,
		PCD_SETLINESPECIAL =		99,
		PCD_THINGSOUND =			100,
		PCD_ENDPRINTBOLD =			101,
		PCD_ACTIVATORSOUND =		102,
		PCD_LOCALAMBIENTSOUND =		103,
		PCD_SETLINEMONSTERBLOCKING =104
	};

	// Some constants used by ACS scripts
	enum {
		LINE_FRONT =			0,
		LINE_BACK =				1
	};
	enum {
		SIDE_FRONT =			0,
		SIDE_BACK =				1
	};
	enum {
		TEXTURE_TOP =			0,
		TEXTURE_MIDDLE =		1,
		TEXTURE_BOTTOM =		2
	};
	enum {
		GAME_SINGLE_PLAYER =	0,
		GAME_NET_COOPERATIVE =	1,
		GAME_NET_DEATHMATCH =	2
	};
	enum {
		CLASS_FIGHTER =			0,
		CLASS_CLERIC =			1,
		CLASS_MAGE =			2
	};
	enum {
		SKILL_VERY_EASY =		0,
		SKILL_EASY =			1,
		SKILL_NORMAL =			2,
		SKILL_HARD =			3,
		SKILL_VERY_HARD =		4
	};
	enum {
		BLOCK_NOTHING =			0,
		BLOCK_CREATURES =		1,
		BLOCK_EVERYTHING =		2
	};

	enum EScriptState
	{
		SCRIPT_Running,
		SCRIPT_Suspended,
		SCRIPT_Delayed,
		SCRIPT_TagWait,
		SCRIPT_PolyWait,
		SCRIPT_ScriptWaitPre,
		SCRIPT_ScriptWait,
		SCRIPT_PleaseRemove
	};

	DLevelScript (AActor *who, line_t *where, int num, int *code,
		int lineSide, int arg0, int arg1, int arg2, int always, bool delay);

	void RunScript ();

	inline void SetState (EScriptState newstate) { state = newstate; }
	inline EScriptState GetState () { return state; }

	void *operator new (size_t size);
	void operator delete (void *block);

protected:
	DLevelScript	*next, *prev;
	int				script;
	int				stack[STACK_SIZE];
	int				sp;
	int				locals[LOCAL_SIZE];
	int				*pc;
	EScriptState	state;
	int				statedata;
	AActor			*activator;
	line_t			*activationline;
	int				lineSide;
	int				stringstart;

	inline void PushToStack (int val);

	void Link ();
	void Unlink ();
	void PutLast ();
	void PutFirst ();
	static int Random (int min, int max);
	static int ThingCount (int type, int tid);
	static void ChangeFlat (int tag, int name, bool floorOrCeiling);
	static int CountPlayers ();
	static void SetLineTexture (int lineid, int side, int position, int name);

private:
	DLevelScript ();

	friend class DACSThinker;
};

inline FArchive &operator<< (FArchive &arc, DLevelScript::EScriptState state)
{
	return arc << (BYTE)state;
}
inline FArchive &operator>> (FArchive &arc, DLevelScript::EScriptState &state)
{
	BYTE in; arc >> in; state = (DLevelScript::EScriptState)in; return arc;
}

inline void DLevelScript::PushToStack (int val)
{
	if (sp == STACK_SIZE)
	{
		Printf (PRINT_HIGH, "Stack overflow in script %d\n", script);
		state = SCRIPT_PleaseRemove;
	}
	stack[sp++] = val;
}

class DACSThinker : public DThinker
{
	DECLARE_SERIAL (DACSThinker, DThinker)
public:
	DACSThinker ();
	~DACSThinker ();

	void RunThink ();

	DLevelScript *RunningScripts[1000];	// Array of all synchronous scripts
	static DACSThinker *ActiveThinker;

private:
	DLevelScript *LastScript;
	DLevelScript *Scripts;				// List of all running scripts

	friend class DLevelScript;
};

// The structure used to control scripts between maps
struct acsdefered_s
{
	struct acsdefered_s *next;

	enum EType
	{
		defexecute,
		defexealways,
		defsuspend,
		defterminate
	} type;
	int script;
	int arg0, arg1, arg2;
};
typedef struct acsdefered_s acsdefered_t;

FArchive &operator<< (FArchive &arc, acsdefered_s *defer);
FArchive &operator>> (FArchive &arc, acsdefered_s* &defer);

#endif //__P_ACS_H__