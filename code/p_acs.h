// p_acs.h: ACS Script Stuff

#ifndef __P_ACS_H__
#define __P_ACS_H__

#define STACK_SIZE	200
#define LOCAL_SIZE	10

typedef enum
{
	running,
	suspended,
	delayed,
	tagwait,
	polywait,
	scriptwait,
	scriptwaitpre,
	removeThisThingNow
} scriptstate_e;

typedef enum
{
	spString,
	spNumber,
	spChar
} stringpart_e;

typedef struct script_s
{
	struct script_s *next, *prev;
	int				script;
	int				stack[STACK_SIZE];
	int				sp;
	int				locals[LOCAL_SIZE];
	int				*pc;
	scriptstate_e	state;
	int				statedata;
	mobj_t			*activator;
	line_t			*activationline;
	int				lineSide;
	int				stringstart;
} script_t;

extern script_t *LastScript;
extern script_t *Scripts;	// List of all running scripts
extern script_t *RunningScripts[256];	// Array of all synchronous scripts

void P_RunScripts (void);

// The structure used to control scripts between maps
struct acsdefered_s
{
	struct acsdefered_s *next;

	enum {
		defexecute,
		defexealways,
		defsuspend,
		defterminate
	} type;
	int script;
	int arg0, arg1, arg2;
};
typedef struct acsdefered_s acsdefered_t;

// Known P-codes for ACS scripts

#define PCD_NOP					0
#define PCD_TERMINATE			1
#define PCD_SUSPEND				2
#define PCD_PUSHNUMBER			3
#define PCD_LSPEC1				4
#define PCD_LSPEC2				5
#define PCD_LSPEC3				6
#define PCD_LSPEC4				7
#define PCD_LSPEC5				8
#define PCD_LSPEC1DIRECT		9
#define PCD_LSPEC2DIRECT		10
#define PCD_LSPEC3DIRECT		11
#define PCD_LSPEC4DIRECT		12
#define PCD_LSPEC5DIRECT		13
#define PCD_ADD					14
#define PCD_SUBTRACT			15
#define PCD_MULTIPLY			16
#define PCD_DIVIDE				17
#define PCD_MODULUS				18
#define PCD_EQ					19
#define PCD_NE					20
#define PCD_LT					21
#define PCD_GT					22
#define PCD_LE					23
#define PCD_GE					24
#define PCD_ASSIGNSCRIPTVAR		25
#define PCD_ASSIGNMAPVAR		26
#define PCD_ASSIGNWORLDVAR		27
#define PCD_PUSHSCRIPTVAR		28
#define PCD_PUSHMAPVAR			29
#define PCD_PUSHWORLDVAR		30
#define PCD_ADDSCRIPTVAR		31
#define PCD_ADDMAPVAR			32
#define PCD_ADDWORLDVAR			33
#define PCD_SUBSCRIPTVAR		34
#define PCD_SUBMAPVAR			35
#define PCD_SUBWORLDVAR			36
#define PCD_MULSCRIPTVAR		37
#define PCD_MULMAPVAR			38
#define PCD_MULWORLDVAR			39
#define PCD_DIVSCRIPTVAR		40
#define PCD_DIVMAPVAR			41
#define PCD_DIVWORLDVAR			42
#define PCD_MODSCRIPTVAR		43
#define PCD_MODMAPVAR			44
#define PCD_MODWORLDVAR			45
#define PCD_INCSCRIPTVAR		46
#define PCD_INCMAPVAR			47
#define PCD_INCWORLDVAR			48
#define PCD_DECSCRIPTVAR		49
#define PCD_DECMAPVAR			50
#define PCD_DECWORLDVAR			51
#define PCD_GOTO				52
#define PCD_IFGOTO				53
#define PCD_DROP				54
#define PCD_DELAY				55
#define PCD_DELAYDIRECT			56
#define PCD_RANDOM				57
#define PCD_RANDOMDIRECT		58
#define PCD_THINGCOUNT			59
#define PCD_THINGCOUNTDIRECT	60
#define PCD_TAGWAIT				61
#define PCD_TAGWAITDIRECT		62
#define PCD_POLYWAIT			63
#define PCD_POLYWAITDIRECT		64
#define PCD_CHANGEFLOOR			65
#define PCD_CHANGEFLOORDIRECT	66
#define PCD_CHANGECEILING		67
#define PCD_CHANGECEILINGDIRECT	68
#define PCD_RESTART				69
#define PCD_ANDLOGICAL			70
#define PCD_ORLOGICAL			71
#define PCD_ANDBITWISE			72
#define PCD_ORBITWISE			73
#define PCD_EORBITWISE			74
#define PCD_NEGATELOGICAL		75
#define PCD_LSHIFT				76
#define PCD_RSHIFT				77
#define PCD_UNARYMINUS			78
#define PCD_IFNOTGOTO			79
#define PCD_LINESIDE			80
#define PCD_SCRIPTWAIT			81
#define PCD_SCRIPTWAITDIRECT	82
#define PCD_CLEARLINESPECIAL	83
#define PCD_CASEGOTO			84
#define PCD_BEGINPRINT			85
#define PCD_ENDPRINT			86
#define PCD_PRINTSTRING			87
#define PCD_PRINTNUMBER			88
#define PCD_PRINTCHARACTER		89
#define PCD_PLAYERCOUNT			90
#define PCD_GAMETYPE			91
#define PCD_GAMESKILL			92
#define PCD_TIMER				93
#define PCD_SECTORSOUND			94
#define PCD_AMBIENTSOUND		95
#define PCD_SOUNDSEQUENCE		96
#define PCD_SETLINETEXTURE		97
#define PCD_SETLINEBLOCKING		98
#define PCD_SETLINESPECIAL		99
#define PCD_THINGSOUND			100
#define PCD_ENDPRINTBOLD		101

// Some constants used by ACS scripts

#define LINE_FRONT				0
#define LINE_BACK				1

#define SIDE_FRONT				0
#define SIDE_BACK				1

#define TEXTURE_TOP				0
#define TEXTURE_MIDDLE			1
#define TEXTURE_BOTTOM			2

#define GAME_SINGLE_PLAYER		0
#define GAME_NET_COOPERATIVE	1
#define GAME_NET_DEATHMATCH		2

#define CLASS_FIGHTER			0
#define CLASS_CLERIC			1
#define CLASS_MAGE				2

#define SKILL_VERY_EASY			0
#define SKILL_EASY				1
#define SKILL_NORMAL			2
#define SKILL_HARD				3
#define SKILL_VERY_HARD			4

#endif //__P_ACS_H__