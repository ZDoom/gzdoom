// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2000 Simon Howard
// Copyright(C) 2002-2008 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//---------------------------------------------------------------------------
//
// FraggleScript is from SMMU which is under the GPL. Technically, 
// therefore, combining the FraggleScript code with the non-free 
// ZDoom code is a violation of the GPL.
//
// As this may be a problem for you, I hereby grant an exception to my 
// copyright on the SMMU source (including FraggleScript). You may use 
// any code from SMMU in (G)ZDoom, provided that:
//
//    * For any binary release of the port, the source code is also made 
//      available.
//    * The copyright notice is kept on any file containing my code.
//
//

#ifndef __T_SCRIPT_H__
#define __T_SCRIPT_H__

#include "p_setup.h"
#include "p_lnspec.h"
#include "m_fixed.h"
#include "actor.h"

#ifdef _MSC_VER
// This pragma saves 8kb of wasted code.
#pragma pointers_to_members( full_generality, single_inheritance )
#endif

class DRunningScript;



inline bool isop(int c)
{
	return !( ( (c)<='Z' && (c)>='A') || ( (c)<='z' && (c)>='a') || 
		( (c)<='9' && (c)>='0') || ( (c)=='_') );
}

//==========================================================================
//
//
//
//==========================================================================

enum
{
  svt_string,
  svt_int,
  svt_mobj,         // a map object
  svt_function,     // functions are stored as variables
  svt_label,        // labels for goto calls are variables
  svt_const,        // const
  svt_fixed,        // haleyjd: fixed-point int - 8-17 std
  svt_pInt,         // pointer to game int
  svt_pMobj,        // pointer to game mobj
  svt_linespec,		// line special (can be used as both function and constant)
};

//==========================================================================
//
//
//
//==========================================================================

struct svalue_t
{
	int type;
	FString string;
	union
	{
		int i;
		fixed_t f;      // haleyjd: 8-17
		AActor *mobj;
	} value;

	svalue_t()
	{
		type = svt_int;
		value.i = 0;
	}

	svalue_t(const svalue_t & other)
	{
		type = other.type;
		string = other.string;
		value = other.value;
	}
};

int intvalue(const svalue_t & v);
fixed_t fixedvalue(const svalue_t & v);
double floatvalue(const svalue_t & v);
const char *stringvalue(const svalue_t & v);
AActor *actorvalue(const svalue_t &svalue);

//==========================================================================
//
// varoius defines collected in a nicer manner
//
//==========================================================================

enum
{
	VARIABLESLOTS = 16,
	SECTIONSLOTS = 17,
	T_MAXTOKENS = 256,
	TOKENLENGTH = 128,
	MAXARGS = 128,
	MAXSCRIPTS = 257,
};

//==========================================================================
//
// One variable
//
//==========================================================================
struct FParser;

struct DFsVariable : public DObject
{
	DECLARE_CLASS(DFsVariable, DObject)
	HAS_OBJECT_POINTERS

public:
	FString Name;
	TObjPtr<DFsVariable> next;       // for hashing

	int type;       // svt_string or svt_int: same as in svalue_t
	FString string;
	TObjPtr<AActor> actor;

	union value_t
	{
		SDWORD i;
		fixed_t fixed;          // haleyjd: fixed-point
		
		// the following are only used in the global script so we don't need to bother with them
		// when serializing variables.
		int *pI;                // pointer to game int
		AActor **pMobj;         // pointer to game obj
		void (FParser::*handler)();      // for functions
		const FLineSpecial *ls;
	} value;

public:

	DFsVariable(const char *_name = "");

	void GetValue(svalue_t &result);
	void SetValue(const svalue_t &newvalue);
	void Serialize(FArchive &ar);
};

//==========================================================================
//
// hash the variables for speed: this is the hashkey
//
//==========================================================================

inline int variable_hash(const char *n)
{
	return 
              (n[0]? (   ( n[0] + n[1] +   
                   (n[1] ? n[2] +   
				   (n[2] ? n[3]  : 0) : 0) ) % VARIABLESLOTS ) :0);
}


//==========================================================================
//
// Sections
//
//==========================================================================

enum    // section types
{
	st_empty,       // empty {} braces
	st_if,
	st_elseif,
	st_else,
	st_loop,
};

struct DFsSection : public DObject
{
	DECLARE_CLASS(DFsSection, DObject)
	HAS_OBJECT_POINTERS
public:
	int type;
	int start_index;
	int end_index;
	int loop_index;
	TObjPtr<DFsSection> next;        // for hashing

	DFsSection()
	{
		next = NULL;
	}

	void Serialize(FArchive &ar);
};



//==========================================================================
//
// Tokens
//
//==========================================================================

enum tokentype_t
{
	name_,   // a name, eg 'count1' or 'frag'
	number,
	operator_,
	string_,
	unset,
	function          // function name
};

enum    // brace types: where current_section is a { or }
{
	bracket_open,
	bracket_close
};

//==========================================================================
//
// Errors
//
//==========================================================================

class CFsError
{
public:
	// trying to throw strings crashes VC++ badly so we have to use a static buffer. :(
	char msg[2048];

	CFsError(const FString &in)
	{
		strncpy(msg, in, 2047);
		msg[2047]=0;
	}
};

// throw this object to regularly terminate a script's execution.
class CFsTerminator
{
	int fill;
};

//==========================================================================
//
// Scripts
//
//==========================================================================

class DFsScript : public DObject
{
	DECLARE_CLASS(DFsScript, DObject)
	HAS_OBJECT_POINTERS

public:
	// script data

	char *data;
	int scriptnum;  // this script's number
	int len;

	// {} sections

	TObjPtr<DFsSection> sections[SECTIONSLOTS];

	// variables:

	TObjPtr<DFsVariable> variables[VARIABLESLOTS];

	// ptr to the parent script
	// the parent script is the script above this level
	// eg. individual linetrigger scripts are children
	// of the levelscript, which is a child of the
	// global_script
	TObjPtr<DFsScript> parent;

	// haleyjd: 8-17
	// child scripts.
	// levelscript holds ptrs to all of the level's scripts
	// here.
	TObjPtr<DFsScript> children[MAXSCRIPTS];


	TObjPtr<AActor> trigger;        // object which triggered this script

	bool lastiftrue;     // haleyjd: whether last "if" statement was 
	// true or false

	DFsScript();
	void Destroy();
	void Serialize(FArchive &ar);

	DFsVariable *NewVariable(const char *name, int vtype);
	void NewFunction(const char *name, void (FParser::*handler)());

	DFsVariable *VariableForName(const char *name);
	DFsVariable *FindVariable(const char *name);
	void ClearVariables(bool complete= false);
	DFsVariable *NewLabel(char *labelptr);
	char *LabelValue(const svalue_t &v);

	char *SectionStart(const DFsSection *sec);
	char *SectionEnd(const DFsSection *sec);
	char *SectionLoop(const DFsSection *sec);
	void ClearSections();
	void ClearChildren();

	int MakeIndex(const char *p) { return int(p-data); }

	// preprocessor
	int section_hash(const char *b) { return MakeIndex(b) % SECTIONSLOTS; }
	DFsSection *NewSection(const char *brace);
	DFsSection *FindSectionStart(const char *brace);
	DFsSection *FindSectionEnd(const char *brace);
	char *ProcessFindChar(char *data, char find);
	void DryRunScript();
	void Preprocess();
	void ParseInclude(char *lumpname);
	void ParseScript(char *rover = NULL);
	void ParseData(char *rover, char *data, char *end);
};

//==========================================================================
//
// The script parser
//
//==========================================================================

struct FParser
{
	struct operator_t
	{
		const char *string;
		void (FParser::*handler)(svalue_t &, int, int, int); // left, mid, right
		int direction;
	};

	enum
	{
		forward,
		backward
	};

	static operator_t operators[];
	static int num_operators;


	char *LineStart;
	char *Rover;

	char *Tokens[T_MAXTOKENS];
	tokentype_t TokenType[T_MAXTOKENS];
	int NumTokens;
	DFsScript *Script;       // the current script
	DFsSection *Section;
	DFsSection *PrevSection;
	int BraceType;

	int t_argc;                     // number of arguments
	svalue_t *t_argv;               // arguments
	svalue_t t_return;              // returned value
	FString t_func;					// name of current function

	FParser(DFsScript *scr)
	{
		LineStart = NULL;
		Rover = NULL;
		Tokens[0] = new char[scr->len+32];	// 32 for safety. FS seems to need a few bytes more than the script's actual length.
		NumTokens = 0;
		Script = scr;
		Section = PrevSection = NULL;
		BraceType = 0;
	}

	~FParser()
	{
		if (Tokens[0]) delete [] Tokens[0];
	}

	void NextToken();
	char *GetTokens(char *s);
	void PrintTokens();
	void ErrorMessage(FString msg);

	void Run(char *rover, char *data, char *end);
	void RunStatement();
	int FindOperator(int start, int stop, const char *value);
	int FindOperatorBackwards(int start, int stop, const char *value);
	void SimpleEvaluate(svalue_t &, int n);
	void PointlessBrackets(int *start, int *stop);
	void EvaluateExpression(svalue_t &, int start, int stop);
	void EvaluateFunction(svalue_t &, int start, int stop);

	void OPequals(svalue_t &, int, int, int);           // =

	void OPplus(svalue_t &, int, int, int);             // +
	void OPminus(svalue_t &, int, int, int);            // -
	void OPmultiply(svalue_t &, int, int, int);         // *
	void OPdivide(svalue_t &, int, int, int);           // /
	void OPremainder(svalue_t &, int, int, int);        // %

	void OPor(svalue_t &, int, int, int);               // ||
	void OPand(svalue_t &, int, int, int);              // &&
	void OPnot(svalue_t &, int, int, int);              // !

	void OPor_bin(svalue_t &, int, int, int);           // |
	void OPand_bin(svalue_t &, int, int, int);          // &
	void OPnot_bin(svalue_t &, int, int, int);          // ~

	void OPcmp(svalue_t &, int, int, int);              // ==
	void OPnotcmp(svalue_t &, int, int, int);           // !=
	void OPlessthan(svalue_t &, int, int, int);         // <
	void OPgreaterthan(svalue_t &, int, int, int);      // >

	void OPincrement(svalue_t &, int, int, int);        // ++
	void OPdecrement(svalue_t &, int, int, int);        // --

	void OPstructure(svalue_t &, int, int, int);    // in t_vari.c

	void OPlessthanorequal(svalue_t &, int, int, int);     // <=
	void OPgreaterthanorequal(svalue_t &, int, int, int);  // >=

	void spec_brace();
	bool spec_if();
	bool spec_elseif(bool lastif);
	void spec_else(bool lastif);
	void spec_for();
	void spec_while();
	void CreateVariable(int newvar_type, DFsScript *newvar_script, int start, int stop);
	void ParseVarLine(int newvar_type, DFsScript *newvar_script, int start);
	bool spec_variable();
	void spec_script();

	DFsSection *looping_section();
	FString GetFormatString(int startarg);
	bool CheckArgs(int cnt);

	void SF_Print();
	void SF_Rnd();
	void SF_Continue();
	void SF_Break();
	void SF_Goto();
	void SF_Return();
	void SF_Include();
	void SF_Input();
	void SF_Beep();
	void SF_Clock();
	void SF_ExitLevel();
	void SF_Tip();
	void SF_TimedTip();
	void SF_PlayerTip();
	void SF_Message();
	void SF_PlayerMsg();
	void SF_PlayerInGame();
	void SF_PlayerName();
	void SF_PlayerObj();
	void SF_StartScript();      // FPUKE needs to access this
	void SF_ScriptRunning();
	void SF_Wait();
	void SF_TagWait();
	void SF_ScriptWait();
	void SF_ScriptWaitPre();    // haleyjd: new wait types
	void SF_Player();
	void SF_Spawn();
	void SF_RemoveObj();
	void SF_KillObj();
	void SF_ObjX();
	void SF_ObjY();
	void SF_ObjZ();
	void SF_ObjAngle();
	void SF_Teleport();
	void SF_SilentTeleport();
	void SF_DamageObj();
	void SF_ObjSector();
	void SF_ObjHealth();
	void SF_ObjFlag();
	void SF_PushThing();
	void SF_ReactionTime();
	void SF_MobjTarget();
	void SF_MobjMomx();
	void SF_MobjMomy();
	void SF_MobjMomz();
	void SF_PointToAngle();
	void SF_PointToDist();
	void SF_SetCamera();
	void SF_ClearCamera();
	void SF_StartSound();
	void SF_StartSectorSound();
	void SF_FloorHeight();
	void SF_MoveFloor();
	void SF_CeilingHeight();
	void SF_MoveCeiling();
	void SF_LightLevel();
	void SF_FadeLight();
	void SF_FloorTexture();
	void SF_SectorColormap();
	void SF_CeilingTexture();
	void SF_ChangeHubLevel();
	void SF_StartSkill();
	void SF_OpenDoor();
	void SF_CloseDoor();
	void SF_RunCommand();
	void SF_LineTrigger();
	void SF_ChangeMusic();
	void SF_SetLineBlocking();
	void SF_SetLineMonsterBlocking();
	void SF_SetLineTexture();
	void SF_Max();
	void SF_Min();
	void SF_Abs();
	void SF_Gameskill();
	void SF_Gamemode();
	void SF_IsPlayerObj();
	void SF_PlayerKeys();
	void SF_PlayerAmmo();
	void SF_MaxPlayerAmmo();
	void SF_PlayerWeapon();
	void SF_PlayerSelectedWeapon();
	void SF_GiveInventory();
	void SF_TakeInventory();
	void SF_CheckInventory();
	void SF_SetWeapon();
	void SF_MoveCamera();
	void SF_ObjAwaken();
	void SF_AmbientSound();
	void SF_ExitSecret();
	void SF_MobjValue();
	void SF_StringValue();
	void SF_IntValue();
	void SF_FixedValue();
	void SF_SpawnExplosion();
	void SF_RadiusAttack();
	void SF_SetObjPosition();
	void SF_TestLocation();
	void SF_HealObj();  //no pain sound
	void SF_ObjDead();
	void SF_SpawnMissile();
	void SF_MapThingNumExist();
	void SF_MapThings();
	void SF_ObjState();
	void SF_LineFlag();
	void SF_PlayerAddFrag();
	void SF_SkinColor();
	void SF_PlayDemo();
	void SF_CheckCVar();
	void SF_Resurrect();
	void SF_LineAttack();
	void SF_ObjType();
	void SF_Sin();
	void SF_ASin();
	void SF_Cos();
	void SF_ACos();
	void SF_Tan();
	void SF_ATan();
	void SF_Exp();
	void SF_Log();
	void SF_Sqrt();
	void SF_Floor();
	void SF_Pow();
	void SF_NewHUPic();
	void SF_DeleteHUPic();
	void SF_ModifyHUPic();
	void SF_SetHUPicDisplay();
	void SF_SetCorona();
	void SF_Ls();
	void SF_LevelNum();
	void SF_MobjRadius();
	void SF_MobjHeight();
	void SF_ThingCount();
	void SF_SetColor();
	void SF_SpawnShot2();
	void SF_KillInSector();
	void SF_SectorType();
	void SF_SetLineTrigger();
	void SF_ChangeTag();
	void SF_WallGlow();
	void RunLineSpecial(const FLineSpecial *);

	DRunningScript *SaveCurrentScript();

};

//==========================================================================
//
// Running scripts
//
//==========================================================================

enum waittype_e
{
    wt_none,        // not waiting
	wt_delay,       // wait for a set amount of time
	wt_tagwait,     // wait for sector to stop moving
	wt_scriptwait,  // wait for script to finish
	wt_scriptwaitpre, // haleyjd - wait for script to start
};

class DRunningScript : public DObject
{
	DECLARE_CLASS(DRunningScript, DObject)
	HAS_OBJECT_POINTERS

public:
	DRunningScript(AActor *trigger=NULL, DFsScript *owner = NULL, int index = 0) ;
	void Destroy();
	void Serialize(FArchive &arc);

	TObjPtr<DFsScript> script;
	
	// where we are
	int save_point;
	
	int wait_type;
	int wait_data;  // data for wait: tagnum, counter, script number etc
	
	// saved variables
	TObjPtr<DFsVariable> variables[VARIABLESLOTS];
	
	TObjPtr<DRunningScript> prev, next;  // for chain
	TObjPtr<AActor> trigger;
};

//-----------------------------------------------------------------------------
//
// This thinker eliminates the need to call the Fragglescript functions from the main code
//
//-----------------------------------------------------------------------------
class DFraggleThinker : public DThinker
{
	DECLARE_CLASS(DFraggleThinker, DThinker)
	HAS_OBJECT_POINTERS
public:

	TObjPtr<DFsScript> LevelScript;
	TObjPtr<DRunningScript> RunningScripts;
	TArray<TObjPtr<AActor> > SpawnedThings;
	bool nocheckposition;

	DFraggleThinker();
	void Destroy();


	void Serialize(FArchive & arc);
	void Tick();
	size_t PropagateMark();
	size_t PointerSubstitution (DObject *old, DObject *notOld);
	bool wait_finished(DRunningScript *script);
	void AddRunningScript(DRunningScript *runscr);

	static TObjPtr<DFraggleThinker> ActiveThinker;
};

//-----------------------------------------------------------------------------
//
// Global stuff
//
//-----------------------------------------------------------------------------

#include "t_fs.h"

void script_error(const char *s, ...);
void FS_EmulateCmd(char * string);

extern AActor *trigger_obj;
extern DFsScript *global_script; 


#endif

