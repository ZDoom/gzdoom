// [RH] p_acs.c: New file to handle ACS scripts

#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "g_level.h"
#include "s_sound.h"
#include "p_acs.h"
#include "p_saveg.h"
#include "p_lnspec.h"
#include "m_random.h"
#include "doomstat.h"
#include "c_consol.h"
#include "s_sndseq.h"

#define NEXTWORD	(*(script->pc)++)
#define STACK(a)	(script->stack[script->sp - (a)])

void strbin (char *str);

script_t *LastScript;
script_t *Scripts;
script_t *RunningScripts[1000];


void P_ClearScripts (void)
{
	int i;

	for (i = 0; i < 1000; i++)
		RunningScripts[i] = NULL;

	Scripts = NULL;
}

static int *P_FindScript (int script)
{
	// This is based on what I did for GetActionBit().
	// There's probably a better way that doesn't need
	// to use smalltimes, but I don't really care right now.

	int min = 0;
	int max = level.scripts[0] - 1;
	int mid = level.scripts[0] / 2;
	int smalltimes = 0;

	do {
		int thisone = level.scripts[mid*3+1] % 1000;

		if (thisone == script) {
			return level.scripts + mid*3 + 1;
		} else if (thisone < script) {
			min = mid;
		} else if (thisone > script) {
			max = mid;
		}
		if (max - min > 1) {
			mid = (max - min) / 2 + min;
		} else if (!smalltimes) {
			smalltimes++;
			mid = (max - mid) + min;
		} else {
			break;
		}
	} while (max - min > 0);

	if ((level.scripts[mid*3+1] % 1000) == script) 
		return level.scripts + mid*3 + 1;

	return NULL;
}

static void unlinkScript (script_t *script)
{
	if (LastScript == script)
		LastScript = script->prev;
	if (Scripts == script)
		Scripts = script->next;
	if (script->prev)
		script->prev->next = script->next;
	if (script->next)
		script->next->prev = script->prev;
}

static void linkScript (script_t *script)
{
	script->next = Scripts;
	if (Scripts)
		Scripts->prev = script;
	script->prev = NULL;
	Scripts = script;
	if (LastScript == NULL)
		LastScript = script;
}

static void putScriptLast (script_t *script)
{
	if (LastScript == script)
		return;

	unlinkScript (script);
	if (Scripts == NULL) {
		linkScript (script);
	} else {
		if (LastScript)
			LastScript->next = script;
		script->prev = LastScript;
		script->next = NULL;
		LastScript = script;
	}
}

static void putScriptFirst (script_t *script)
{
	if (Scripts == script)
		return;

	unlinkScript (script);
	linkScript (script);
}

static __inline void pushToStack (script_t *script, int val)
{
	if (script->sp == STACK_SIZE) {
		Printf (PRINT_HIGH, "Stack overflow in script %d\n", script->script);
		script->state = removeThisThingNow;
	}
	STACK(0) = val;
	script->sp++;
}

static int acs_Random (int min, int max)
{
	int num1, num2, num3, num4;
	unsigned int num;

	num1 = P_Random(pr_acs);
	num2 = P_Random(pr_acs);
	num3 = P_Random(pr_acs);
	num4 = P_Random(pr_acs);

	num = ((num1 << 24) | (num2 << 16) | (num3 << 8) | num4);
	num %= (max - min + 1);
	num += min;
	return (int)num;
}

static int acs_ThingCount (int type, int tid)
{
	mobj_t *mobj = NULL;
	int count = 0;

	if (type >= NumSpawnableThings) {
		return 0;
	} else if (type > 0) {
		type = SpawnableThings[type];
		if (type == 0)
			return 0;
	}
	
	if (tid) {
		while ( (mobj = P_FindMobjByTid (mobj, tid)) ) {
			if ((type == 0) || (mobj->type == type && mobj->health > 0))
				count++;
		}
	} else {
		thinker_t *currentthinker=&thinkercap;

		while ((currentthinker=currentthinker->next)!=&thinkercap)
			if (currentthinker->function.acp1 == (actionf_p1) P_MobjThinker)
				if ((type == 0) ||
					(((mobj_t *)currentthinker)->type == type
					 && ((mobj_t *)currentthinker)->health > 0))
					count++;
	}
	return count;
}

static void acs_ChangeFlat (int tag, int name, int floorOrCeiling)
{
	int flat, secnum = -1;

	if (name >= level.strings[0])
		return;

	flat = R_FlatNumForName (level.behavior + level.strings[name+1]);

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0) {
		if (floorOrCeiling == 0)
			sectors[secnum].floorpic = flat;
		else
			sectors[secnum].ceilingpic = flat;
	}
}

int CountPlayers (void)
{
	int count = 0, i;

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			count++;
	
	return count;
}

static void acs_SetLineTexture (int lineid, int side, int position, int name)
{
	int texture, linenum = -1;

	if (name >= level.strings[0])
		return;

	side = (side) ? 1 : 0;

	texture = R_TextureNumForName (level.behavior + level.strings[name+1]);

	while ((linenum = P_FindLineFromID (lineid, linenum)) >= 0) {
		side_t *sidedef;

		if (lines[linenum].sidenum[side] == -1)
			continue;
		sidedef = sides + lines[linenum].sidenum[side];

		switch (position) {
			case TEXTURE_TOP:
				sidedef->toptexture = texture;
				break;
			case TEXTURE_MIDDLE:
				sidedef->midtexture = texture;
				break;
			case TEXTURE_BOTTOM:
				sidedef->bottomtexture = texture;
				break;
			default:
				break;
		}

	}
}

static void T_ACS (script_t *script)
{
	int runaway = 0;	// used to prevent infinite loops
	int pcd;

	switch (script->state) {
		case delayed:
			// Decrement the delay counter and enter state running
			// if it hits 0
			if (--script->statedata == 0)
				script->state = running;
			break;

		case tagwait:
			// Wait for tagged sector(s) to go inactive, then enter
			// state running
			{
				int secnum = -1;

				while ((secnum = P_FindSectorFromTag (script->statedata, secnum)) >= 0)
					if (sectors[secnum].floordata || sectors[secnum].ceilingdata)
						return;

				// If we got here, none of the tagged sectors were busy
				script->state = running;
			}
			break;

		case polywait:
			// Wait for polyobj(s) to stop moving, then enter state running
			if (!PO_Busy (script->statedata))
			{
				script->state = running;
			}
			break;

		case scriptwaitpre:
			// Wait for a script to start running, then enter state scriptwait
			if (RunningScripts[script->statedata])
				script->state = scriptwait;
			break;

		case scriptwait:
			// Wait for a script to stop running, then enter state running
			if (RunningScripts[script->statedata])
				return;

			script->state = running;
			putScriptFirst (script);
			break;
	}

	while (script->state == running) {
		if (++runaway > 500000) {
			Printf (PRINT_HIGH, "Runaway script %d terminated\n", script->script);
			script->state = removeThisThingNow;
			break;
		}

		switch ( (pcd = NEXTWORD) ) {
			default:
				Printf (PRINT_HIGH, "Unknown P-Code %d in script %d\n", pcd, script->script);
				// fall through
			case PCD_TERMINATE:
				script->state = removeThisThingNow;
				break;

			case PCD_NOP:
				break;

			case PCD_SUSPEND:
				script->state = suspended;
				break;

			case PCD_PUSHNUMBER:
				pushToStack (script, NEXTWORD);
				break;

			case PCD_LSPEC1:
				LineSpecials[NEXTWORD] (script->activationline, script->activator,
										STACK(1), 0, 0, 0, 0);
				script->sp -= 1;
				break;

			case PCD_LSPEC2:
				LineSpecials[NEXTWORD] (script->activationline, script->activator,
										STACK(2), STACK(1), 0, 0, 0);
				script->sp -= 2;
				break;

			case PCD_LSPEC3:
				LineSpecials[NEXTWORD] (script->activationline, script->activator,
										STACK(3), STACK(2), STACK(1), 0, 0);
				script->sp -= 3;
				break;

			case PCD_LSPEC4:
				LineSpecials[NEXTWORD] (script->activationline, script->activator,
										STACK(4), STACK(3), STACK(2),
										STACK(1), 0);
				script->sp -= 4;
				break;

			case PCD_LSPEC5:
				LineSpecials[NEXTWORD] (script->activationline, script->activator,
										STACK(5), STACK(4), STACK(3),
										STACK(2), STACK(1));
				script->sp -= 5;
				break;

			case PCD_LSPEC1DIRECT:
				LineSpecials[script->pc[0]] (script->activationline, script->activator,
											 script->pc[1], 0, 0, 0, 0);
				script->pc += 2;
				break;

			case PCD_LSPEC2DIRECT:
				LineSpecials[script->pc[0]] (script->activationline, script->activator,
											 script->pc[1], script->pc[2], 0, 0, 0);
				script->pc += 3;
				break;

			case PCD_LSPEC3DIRECT:
				LineSpecials[script->pc[0]] (script->activationline, script->activator,
											 script->pc[1], script->pc[2], script->pc[3],
											 0, 0);
				script->pc += 4;
				break;

			case PCD_LSPEC4DIRECT:
				LineSpecials[script->pc[0]] (script->activationline, script->activator,
											 script->pc[1], script->pc[2], script->pc[3],
											 script->pc[4], 0);
				script->pc += 5;
				break;

			case PCD_LSPEC5DIRECT:
				LineSpecials[script->pc[0]] (script->activationline, script->activator,
											 script->pc[1], script->pc[2], script->pc[3],
											 script->pc[4], script->pc[5]);
				script->pc += 6;
				break;

			case PCD_ADD:
				STACK(2) = STACK(2) + STACK(1);
				script->sp--;
				break;

			case PCD_SUBTRACT:
				STACK(2) = STACK(2) - STACK(1);
				script->sp--;
				break;

			case PCD_MULTIPLY:
				STACK(2) = STACK(2) * STACK(1);
				script->sp--;
				break;

			case PCD_DIVIDE:
				STACK(2) = STACK(2) / STACK(1);
				script->sp--;
				break;

			case PCD_MODULUS:
				STACK(2) = STACK(2) % STACK(1);
				script->sp--;
				break;

			case PCD_EQ:
				STACK(2) = (STACK(2) == STACK(1));
				script->sp--;
				break;

			case PCD_NE:
				STACK(2) = (STACK(2) != STACK(1));
				script->sp--;
				break;

			case PCD_LT:
				STACK(2) = (STACK(2) < STACK(1));
				script->sp--;
				break;

			case PCD_GT:
				STACK(2) = (STACK(2) > STACK(1));
				script->sp--;
				break;

			case PCD_LE:
				STACK(2) = (STACK(2) <= STACK(1));
				script->sp--;
				break;

			case PCD_GE:
				STACK(2) = (STACK(2) >= STACK(1));
				script->sp--;
				break;

			case PCD_ASSIGNSCRIPTVAR:
				script->locals[NEXTWORD] = STACK(1);
				script->sp--;
				break;


			case PCD_ASSIGNMAPVAR:
				level.vars[NEXTWORD] = STACK(1);
				script->sp--;
				break;

			case PCD_ASSIGNWORLDVAR:
				WorldVars[NEXTWORD] = STACK(1);
				script->sp--;
				break;

			case PCD_PUSHSCRIPTVAR:
				pushToStack (script, script->locals[NEXTWORD]);
				break;

			case PCD_PUSHMAPVAR:
				pushToStack (script, level.vars[NEXTWORD]);
				break;

			case PCD_PUSHWORLDVAR:
				pushToStack (script, WorldVars[NEXTWORD]);
				break;

			case PCD_ADDSCRIPTVAR:
				script->locals[NEXTWORD] += STACK(1);
				script->sp--;
				break;

			case PCD_ADDMAPVAR:
				level.vars[NEXTWORD] += STACK(1);
				script->sp--;
				break;

			case PCD_ADDWORLDVAR:
				WorldVars[NEXTWORD] += STACK(1);
				script->sp--;
				break;

			case PCD_SUBSCRIPTVAR:
				script->locals[NEXTWORD] -= STACK(1);
				script->sp--;
				break;

			case PCD_SUBMAPVAR:
				level.vars[NEXTWORD] -= STACK(1);
				script->sp--;
				break;

			case PCD_SUBWORLDVAR:
				WorldVars[NEXTWORD] -= STACK(1);
				script->sp--;
				break;

			case PCD_MULSCRIPTVAR:
				script->locals[NEXTWORD] *= STACK(1);
				script->sp--;
				break;

			case PCD_MULMAPVAR:
				level.vars[NEXTWORD] *= STACK(1);
				script->sp--;
				break;

			case PCD_MULWORLDVAR:
				WorldVars[NEXTWORD] *= STACK(1);
				script->sp--;
				break;

			case PCD_DIVSCRIPTVAR:
				script->locals[NEXTWORD] /= STACK(1);
				script->sp--;
				break;

			case PCD_DIVMAPVAR:
				level.vars[NEXTWORD] /= STACK(1);
				script->sp--;
				break;

			case PCD_DIVWORLDVAR:
				WorldVars[NEXTWORD] /= STACK(1);
				script->sp--;
				break;

			case PCD_MODSCRIPTVAR:
				script->locals[NEXTWORD] %= STACK(1);
				script->sp--;
				break;

			case PCD_MODMAPVAR:
				level.vars[NEXTWORD] %= STACK(1);
				script->sp--;
				break;

			case PCD_MODWORLDVAR:
				WorldVars[NEXTWORD] %= STACK(1);
				script->sp--;
				break;

			case PCD_INCSCRIPTVAR:
				script->locals[NEXTWORD]++;
				break;

			case PCD_INCMAPVAR:
				level.vars[NEXTWORD]++;
				break;

			case PCD_INCWORLDVAR:
				WorldVars[NEXTWORD]++;
				break;

			case PCD_DECSCRIPTVAR:
				script->locals[NEXTWORD]--;
				break;

			case PCD_DECMAPVAR:
				level.vars[NEXTWORD]--;
				break;

			case PCD_DECWORLDVAR:
				WorldVars[NEXTWORD]--;
				break;

			case PCD_GOTO:
				script->pc = (int *)(level.behavior + *(script->pc));
				break;

			case PCD_IFGOTO:
				if (STACK(1))
					script->pc = (int *)(level.behavior + *(script->pc));
				else
					script->pc++;
				script->sp--;
				break;

			case PCD_DROP:
				script->sp--;
				break;

			case PCD_DELAY:
				script->state = delayed;
				script->statedata = STACK(1);
				script->sp--;
				break;

			case PCD_DELAYDIRECT:
				script->state = delayed;
				script->statedata = NEXTWORD;
				break;

			case PCD_RANDOM:
				STACK(2) = acs_Random (STACK(2), STACK(1));
				script->sp--;
				break;
				
			case PCD_RANDOMDIRECT:
				pushToStack (script, acs_Random (script->pc[0], script->pc[1]));
				script->pc += 2;
				break;

			case PCD_THINGCOUNT:
				STACK(2) = acs_ThingCount (STACK(2), STACK(1));
				script->sp--;
				break;

			case PCD_THINGCOUNTDIRECT:
				pushToStack (script, acs_ThingCount (script->pc[0], script->pc[1]));
				script->pc += 2;
				break;

			case PCD_TAGWAIT:
				script->state = tagwait;
				script->statedata = STACK(1);
				script->sp--;
				break;

			case PCD_TAGWAITDIRECT:
				script->state = tagwait;
				script->statedata = NEXTWORD;
				break;

			case PCD_POLYWAIT:
				script->state = polywait;
				script->statedata = STACK(1);
				script->sp--;
				break;

			case PCD_POLYWAITDIRECT:
				script->state = polywait;
				script->statedata = NEXTWORD;
				break;

			case PCD_CHANGEFLOOR:
				acs_ChangeFlat (STACK(2), STACK(1), 0);
				script->sp -= 2;
				break;

			case PCD_CHANGEFLOORDIRECT:
				acs_ChangeFlat (script->pc[0], script->pc[1], 0);
				script->pc += 2;
				break;

			case PCD_CHANGECEILING:
				acs_ChangeFlat (STACK(2), STACK(1), 1);
				script->sp -= 2;
				break;

			case PCD_CHANGECEILINGDIRECT:
				acs_ChangeFlat (script->pc[0], script->pc[1], 1);
				script->pc += 2;
				break;

			case PCD_RESTART:
				script->pc = (int *)(level.behavior + (P_FindScript (script->script))[1]);
				break;

			case PCD_ANDLOGICAL:
				STACK(2) = (STACK(2) && STACK(1));
				script->sp--;
				break;

			case PCD_ORLOGICAL:
				STACK(2) = (STACK(2) || STACK(1));
				script->sp--;
				break;

			case PCD_ANDBITWISE:
				STACK(2) = (STACK(2) & STACK(1));
				script->sp--;
				break;

			case PCD_ORBITWISE:
				STACK(2) = (STACK(2) | STACK(1));
				script->sp--;
				break;

			case PCD_EORBITWISE:
				STACK(2) = (STACK(2) ^ STACK(1));
				script->sp--;
				break;

			case PCD_NEGATELOGICAL:
				STACK(1) = !STACK(1);
				break;

			case PCD_LSHIFT:
				STACK(2) = (STACK(2) << STACK(1));
				script->sp--;
				break;

			case PCD_RSHIFT:
				STACK(2) = (STACK(2) >> STACK(1));
				script->sp--;
				break;

			case PCD_UNARYMINUS:
				STACK(1) = -STACK(1);
				script->sp--;
				break;

			case PCD_IFNOTGOTO:
				if (!STACK(1))
					script->pc = (int *)(level.behavior + *(script->pc));
				else
					script->pc++;
				script->sp--;
				break;

			case PCD_LINESIDE:
				pushToStack (script, script->lineSide);
				break;

			case PCD_SCRIPTWAIT:
				script->statedata = STACK(1);
				if (RunningScripts[script->statedata])
					script->state = scriptwait;
				else
					script->state = scriptwaitpre;
				script->sp--;
				putScriptLast (script);
				break;

			case PCD_SCRIPTWAITDIRECT:
				script->state = scriptwait;
				script->statedata = NEXTWORD;
				putScriptLast (script);
				break;

			case PCD_CLEARLINESPECIAL:
				if (script->activationline)
					script->activationline->special = 0;
				break;

			case PCD_CASEGOTO:
				if (STACK(1) == NEXTWORD) {
					script->pc = (int *)(level.behavior + *(script->pc));
					script->sp--;
				} else {
					script->pc++;
				}
				break;

			case PCD_BEGINPRINT:
				script->stringstart = script->sp;
				break;

			case PCD_ENDPRINT:
			case PCD_ENDPRINTBOLD:
				if (script->activator && script->activator->player == NULL) {
					script->sp = script->stringstart;
				} else {
					char work[4096], *workwhere;
					int parse = script->stringstart;

					workwhere = work;
					while (parse < script->sp) {
						int value = script->stack[parse++];
						int type = script->stack[parse++];

						switch (type) {
							case spString:
								if (value < level.strings[0])
									workwhere += sprintf (workwhere, "%s",
										level.strings[value+1] + level.behavior);
								break;

							case spNumber:
								workwhere += sprintf (workwhere, "%d", value);
								break;

							case spChar:
								workwhere[0] = value;
								workwhere[1] = 0;
								workwhere++;
								break;

							default:
								break;
						}
					}
					script->sp = script->stringstart;
					if (pcd == PCD_ENDPRINTBOLD || script->activator == NULL ||
						(script->activator->player - players == consoleplayer))
					{
						strbin (work);
						C_MidPrint (work);
					}
				}
				break;

			case PCD_PRINTSTRING:
				pushToStack (script, spString);
				break;

			case PCD_PRINTNUMBER:
				pushToStack (script, spNumber);
				break;

			case PCD_PRINTCHARACTER:
				pushToStack (script, spChar);
				break;

			case PCD_PLAYERCOUNT:
				pushToStack (script, CountPlayers ());
				break;

			case PCD_GAMETYPE:
				if (deathmatch->value)
					pushToStack (script, GAME_NET_DEATHMATCH);
				else if (netgame)
					pushToStack (script, GAME_NET_COOPERATIVE);
				else
					pushToStack (script, GAME_SINGLE_PLAYER);
				break;
					
			case PCD_GAMESKILL:
				pushToStack (script, (int)(gameskill->value));
				break;

			case PCD_TIMER:
				pushToStack (script, level.time);
				break;

			case PCD_SECTORSOUND:
				if (STACK(2) < level.strings[0]) {
					mobj_t *mobj;

					if (script->activationline)
						mobj = (mobj_t *)&script->activationline->frontsector->soundorg;
					else
						mobj = NULL;
					S_Sound (
						mobj,
						CHAN_BODY,
						level.behavior + level.strings[STACK(2)+1],
						(STACK(1)) / 127,
						ATTN_NORM);
				}
				script->sp -= 2;
				break;

			case PCD_AMBIENTSOUND:
				if (STACK(2) < level.strings[0])
					S_Sound (NULL, CHAN_BODY,
							 level.behavior + level.strings[STACK(2)+1],
							 (STACK(1)) / 127, ATTN_NONE);
				script->sp -= 2;
				break;

			case PCD_SOUNDSEQUENCE:
				if (STACK(1) < level.strings[0]) {
					mobj_t *mobj;

					if (script->activationline)
						mobj = (mobj_t *)&script->activationline->frontsector->soundorg;
					else
						mobj = NULL;
					SN_StartSequenceName (mobj, level.behavior + level.strings[STACK(1)+1]);
				}
				script->sp--;
				break;

			case PCD_SETLINETEXTURE:
				acs_SetLineTexture (STACK(4), STACK(3), STACK(2), STACK(1));
				script->sp -= 4;
				break;

			case PCD_SETLINEBLOCKING:
				{
					int line = -1;

					while ((line = P_FindLineFromID (STACK(2), line)) >= 0) {
						if (STACK(1))
							lines[line].flags |= ML_BLOCKING;
						else
							lines[line].flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);
					}

					script->sp -= 2;
				}
				break;

			case PCD_SETLINESPECIAL:
				{
					int linenum = -1;

					while ((linenum = P_FindLineFromID (STACK(7), linenum)) >= 0) {
						line_t *line = &lines[linenum];

						line->special = STACK(6);
						line->args[0] = STACK(5);
						line->args[1] = STACK(4);
						line->args[2] = STACK(3);
						line->args[3] = STACK(2);
						line->args[4] = STACK(1);
					}
					script->sp -= 7;
				}
				break;

			case PCD_THINGSOUND:
				if (STACK(2) < level.strings[0])
				{
					mobj_t *spot = NULL;

					while ( (spot = P_FindMobjByTid (spot, STACK(3))) )
						S_Sound (spot, CHAN_BODY,
								 level.behavior + level.strings[STACK(2)+1],
								 (STACK(1))/127, ATTN_NORM);
				}
				script->sp -= 3;
				break;

		}
	}

	if (script->state == removeThisThingNow) {
		unlinkScript (script);
		if (RunningScripts[script->script] == script)
			RunningScripts[script->script] = NULL;
		Z_Free (script);
	}
}

void P_RunScripts (void)
{
	script_t *script = Scripts;

	while (script) {
		T_ACS (script);
		script = script->next;
	}
}

static BOOL P_GetScriptGoing (mobj_t *who, line_t *where, int num, int *code,
							  int lineSide, int arg0, int arg1, int arg2, int always, BOOL delay)
{
	script_t *script;

	if (!always)
		if (RunningScripts[num]) {
			if (RunningScripts[num]->state == suspended) {
				RunningScripts[num]->state = running;
				return true;
			}
			return false;
		}

	script = Z_Malloc (sizeof(*script), PU_LEVACS, 0);

	script->script = num;
	script->sp = 0;
	memset (script->locals, 0, sizeof(script->locals));
	script->locals[0] = arg0;
	script->locals[1] = arg1;
	script->locals[2] = arg2;
	script->pc = code;
	script->activator = who;
	script->activationline = where;
	script->lineSide = lineSide;
	if (delay) {
		// From Hexen: Give the world some time to set itself up before
		// running open scripts.
		script->state = delayed;
		script->statedata = TICRATE;
	} else {
		script->state = running;
	}

	if (!always)
		RunningScripts[num] = script;

	linkScript (script);

	DPrintf ("Script %d started.\n", num);

	return true;
}

static void SetScriptState (int script, int state)
{
	if (RunningScripts[script])
		RunningScripts[script]->state = state;
}

void P_DoDeferedScripts (void)
{
	acsdefered_t *def;
	int *scriptdata;

	// Handle defered scripts in this step, too
	def = level.info->defered;
	while (def) {
		acsdefered_t *next = def->next;
		switch (def->type) {
			case defexecute:
			case defexealways:
				scriptdata = P_FindScript (def->script);
				if (scriptdata) {
					P_GetScriptGoing (NULL, NULL, def->script,
									 (int *)(scriptdata[1] + level.behavior),
									 0, def->arg0, def->arg1, def->arg2,
									 def->type == defexealways, true);
				} else
					Printf (PRINT_HIGH, "P_DoDeferredScripts: Unknown script %d\n", def->script);
				break;
			case defsuspend:
				SetScriptState (def->script, suspended);
				DPrintf ("Defered suspend of script %d\n", def->script);
				break;
			case defterminate:
				SetScriptState (def->script, removeThisThingNow);
				DPrintf ("Defered terminate of script %d\n", def->script);
				break;
		}
		Z_Free (def);
		def = next;
	}
	level.info->defered = NULL;
}

void P_StartOpenScripts (void)
{
	int *script;
	
	if ( (script = level.scripts) ) {
		int numscripts = *script++;

		for (; numscripts; numscripts--, script += 3) {
			if (script[0] >= 1000) {
				P_GetScriptGoing (NULL, NULL, script[0] - 1000,
								  (int *)(level.behavior + script[1]), 0, 0, 0, 0, 0, true);
			}
		}
	}
}

static void addDefered (level_info_t *i, int type, int script, int arg0, int arg1, int arg2)
{
	if (i) {
		acsdefered_t *def = Z_Malloc (sizeof(*def), PU_STATIC, 0);

		def->next = i->defered;
		def->type = type;
		def->script = script;
		def->arg0 = arg0;
		def->arg1 = arg1;
		def->arg2 = arg2;
		i->defered = def;
		DPrintf ("Script %d on map %s defered\n", script, i->mapname);
	}
}

BOOL P_StartScript (mobj_t *who, line_t *where, int script, char *map, int lineSide,
					int arg0, int arg1, int arg2, int always)
{
	if (!strnicmp (level.mapname, map, 8)) {
		int *scriptdata = P_FindScript (script);

		if (scriptdata) {
			return P_GetScriptGoing (who, where, script,
									 (int *)(scriptdata[1] + level.behavior),
									 lineSide, arg0, arg1, arg2, always, false);
		} else
			Printf (PRINT_HIGH, "P_StartScript: Unknown script %d\n", script);
	} else {
		addDefered (FindLevelInfo (map), always ? defexealways : defexecute,
					script, arg0, arg1, arg2);
	}
	return false;
}

void P_SuspendScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), defsuspend, script, 0, 0, 0);
	else
		SetScriptState (script, suspended);
}

void P_TerminateScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), defterminate, script, 0, 0, 0);
	else
		SetScriptState (script, removeThisThingNow);
}

void strbin (char *str)
{
	char *p = str, c;
	int i;

	while ( (c = *p++) ) {
		if (c != '\\') {
			*str++ = c;
		} else {
			switch (*p) {
				case 'c':
					*str++ = '\x8a';
					break;
				case 'n':
					*str++ = '\n';
					break;
				case 't':
					*str++ = '\t';
					break;
				case 'r':
					*str++ = '\r';
					break;
				case '\n':
					break;
				case 'x':
				case 'X':
					c = 0;
					p++;
					for (i = 0; i < 2; i++) {
						c <<= 4;
						if (*p >= '0' && *p <= '9')
							c += *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c += 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c += 10 + *p-'A';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = 0;
					for (i = 0; i < 3; i++) {
						c <<= 3;
						if (*p >= '0' && *p <= '7')
							c += *p-'0';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				default:
					*str++ = *p;
					break;
			}
			p++;
		}
	}
	*str = 0;
}

