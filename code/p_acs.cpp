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
#include "c_console.h"
#include "s_sndseq.h"
#include "i_system.h"

#define NEXTWORD	(*(pc)++)
#define STACK(a)	(stack[sp - (a)])

void strbin (char *str);

IMPLEMENT_SERIAL (DACSThinker, DThinker)

DACSThinker *DACSThinker::ActiveThinker = NULL;

DACSThinker::DACSThinker ()
{
	if (ActiveThinker)
	{
		I_Error ("Only one ACSThinker is allowed to exist at a time.\nCheck your code.");
	}
	else
	{
		ActiveThinker = this;
		Scripts = NULL;
		LastScript = NULL;
		for (int i = 0; i < 1000; i++)
			RunningScripts[i] = NULL;
	}
}

DACSThinker::~DACSThinker ()
{
	DLevelScript *script = Scripts;
	while (script)
	{
		DLevelScript *next = script->next;
		delete script;
		script = next;
	}
	Scripts = NULL;
	ActiveThinker = NULL;
}

void DACSThinker::Serialize (FArchive &arc)
{
	if (arc.IsStoring ())
	{
		arc << Scripts << LastScript;
		for (int i = 0; i < 1000; i++)
		{
			if (RunningScripts[i])
				arc << RunningScripts[i] << (WORD)i;
		}
		arc << (DLevelScript *)NULL;
	}
	else
	{
		arc >> Scripts >> LastScript;

		WORD scriptnum;
		DLevelScript *script;
		arc >> script;
		while (script)
		{
			arc >> scriptnum;
			RunningScripts[scriptnum] = script;
			arc >> script;
		}
	}
}

void DACSThinker::RunThink ()
{
	DLevelScript *script = Scripts;

	while (script)
	{
		DLevelScript *next = script->next;
		script->RunScript ();
		script = next;
	}
}

IMPLEMENT_SERIAL (DLevelScript, DObject)

void *DLevelScript::operator new (size_t size)
{
	return Z_Malloc (sizeof(DLevelScript), PU_LEVACS, 0);
}

void DLevelScript::operator delete (void *block)
{
	Z_Free (block);
}

void DLevelScript::Serialize (FArchive &arc)
{
	DWORD i;

	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << next << prev
			<< script
			<< sp
			<< state
			<< statedata
			<< activator
			<< activationline
			<< lineSide
			<< stringstart;

		i = (DWORD)(pc - (int *)level.behavior);
		arc << i;

		for (i = 0; i < STACK_SIZE; i++)
			arc << stack[i];
		for (i = 0; i < LOCAL_SIZE; i++)
			arc << locals[i];
	}
	else
	{
		arc >> next >> prev
			>> script
			>> sp
			>> state
			>> statedata
			>> activator
			>> activationline
			>> lineSide
			>> stringstart
			>> i;
		pc = (int *)level.behavior + i;
		for (i = 0; i < STACK_SIZE; i++)
			arc >> stack[i];
		for (i = 0; i < LOCAL_SIZE; i++)
			arc >> locals[i];
	}
}

DLevelScript::DLevelScript ()
{
	next = prev = NULL;
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;
}

static int *P_FindScript (int script)
{
	// This is based on what I did for GetActionBit().
	// There's probably a better way that doesn't need
	// to use smalltimes, but I don't really care right now.

	if (!level.scripts)
		return NULL;

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

void DLevelScript::Unlink ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
		controller->LastScript = prev;
	if (controller->Scripts == this)
		controller->Scripts = next;
	if (prev)
		prev->next = next;
	if (next)
		next->prev = prev;
}

void DLevelScript::Link ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	next = controller->Scripts;
	if (controller->Scripts)
		controller->Scripts->prev = this;
	prev = NULL;
	controller->Scripts = this;
	if (controller->LastScript == NULL)
		controller->LastScript = this;
}

void DLevelScript::PutLast ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
		return;

	Unlink ();
	if (controller->Scripts == NULL)
	{
		Link ();
	}
	else
	{
		if (controller->LastScript)
			controller->LastScript->next = this;
		prev = controller->LastScript;
		next = NULL;
		controller->LastScript = this;
	}
}

void DLevelScript::PutFirst ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->Scripts == this)
		return;

	Unlink ();
	Link ();
}

int DLevelScript::Random (int min, int max)
{
	int num1, num2, num3, num4;
	unsigned int num;

	num1 = P_Random (pr_acs);
	num2 = P_Random (pr_acs);
	num3 = P_Random (pr_acs);
	num4 = P_Random (pr_acs);

	num = ((num1 << 24) | (num2 << 16) | (num3 << 8) | num4);
	num %= (max - min + 1);
	num += min;
	return (int)num;
}

int DLevelScript::ThingCount (int type, int tid)
{
	AActor *mobj = NULL;
	int count = 0;

	if (type >= NumSpawnableThings)
	{
		return 0;
	}
	else if (type > 0)
	{
		type = SpawnableThings[type];
		if (type == 0)
			return 0;
	}
	
	if (tid)
	{
		mobj = AActor::FindByTID (NULL, tid);
		while (mobj)
		{
			if ((type == 0) || (mobj->type == type && mobj->health > 0))
				count++;
			mobj = mobj->FindByTID (tid);
		}
	}
	else
	{
		AActor *actor;
		TThinkerIterator<AActor> iterator;

		while ( (actor = iterator.Next ()) )
		{
			if (type == 0)
			{
				count++;
			}
			else if (actor->type == type && actor->health > 0)
			{
				count++;
			}
		}
	}
	return count;
}

void DLevelScript::ChangeFlat (int tag, int name, bool floorOrCeiling)
{
	int flat, secnum = -1;

	if (name >= level.strings[0])
		return;

	flat = R_FlatNumForName ((char *)(level.behavior + level.strings[name+1]));

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		if (floorOrCeiling == false)
			sectors[secnum].floorpic = flat;
		else
			sectors[secnum].ceilingpic = flat;
	}
}

int DLevelScript::CountPlayers ()
{
	int count = 0, i;

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			count++;
	
	return count;
}

void DLevelScript::SetLineTexture (int lineid, int side, int position, int name)
{
	int texture, linenum = -1;

	if (name >= level.strings[0])
		return;

	side = (side) ? 1 : 0;

	texture = R_TextureNumForName ((char *)(level.behavior + level.strings[name+1]));

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

void DLevelScript::RunScript ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;
	int runaway = 0;	// used to prevent infinite loops
	int pcd;

	switch (state)
	{
		case SCRIPT_Delayed:
			// Decrement the delay counter and enter state running
			// if it hits 0
			if (--statedata == 0)
				state = SCRIPT_Running;
			break;

		case SCRIPT_TagWait:
			// Wait for tagged sector(s) to go inactive, then enter
			// state running
			{
				int secnum = -1;

				while ((secnum = P_FindSectorFromTag (statedata, secnum)) >= 0)
					if (sectors[secnum].floordata || sectors[secnum].ceilingdata)
						return;

				// If we got here, none of the tagged sectors were busy
				state = SCRIPT_Running;
			}
			break;

		case SCRIPT_PolyWait:
			// Wait for polyobj(s) to stop moving, then enter state running
			if (!PO_Busy (statedata))
			{
				state = SCRIPT_Running;
			}
			break;

		case SCRIPT_ScriptWaitPre:
			// Wait for a script to start running, then enter state scriptwait
			if (controller->RunningScripts[statedata])
				state = SCRIPT_ScriptWait;
			break;

		case SCRIPT_ScriptWait:
			// Wait for a script to stop running, then enter state running
			if (controller->RunningScripts[statedata])
				return;

			state = SCRIPT_Running;
			PutFirst ();
			break;
	}

	while (state == SCRIPT_Running)
	{
		if (++runaway > 500000)
		{
			Printf (PRINT_HIGH, "Runaway script %d terminated\n", script);
			state = SCRIPT_PleaseRemove;
			break;
		}

		switch ( (pcd = NEXTWORD) )
		{
			default:
				Printf (PRINT_HIGH, "Unknown P-Code %d in script %d\n", pcd, script);
				// fall through
			case PCD_TERMINATE:
				state = SCRIPT_PleaseRemove;
				break;

			case PCD_NOP:
				break;

			case PCD_SUSPEND:
				state = SCRIPT_Suspended;
				break;

			case PCD_PUSHNUMBER:
				PushToStack (NEXTWORD);
				break;

			case PCD_LSPEC1:
				LineSpecials[NEXTWORD] (activationline, activator,
										STACK(1), 0, 0, 0, 0);
				sp -= 1;
				break;

			case PCD_LSPEC2:
				LineSpecials[NEXTWORD] (activationline, activator,
										STACK(2), STACK(1), 0, 0, 0);
				sp -= 2;
				break;

			case PCD_LSPEC3:
				LineSpecials[NEXTWORD] (activationline, activator,
										STACK(3), STACK(2), STACK(1), 0, 0);
				sp -= 3;
				break;

			case PCD_LSPEC4:
				LineSpecials[NEXTWORD] (activationline, activator,
										STACK(4), STACK(3), STACK(2),
										STACK(1), 0);
				sp -= 4;
				break;

			case PCD_LSPEC5:
				LineSpecials[NEXTWORD] (activationline, activator,
										STACK(5), STACK(4), STACK(3),
										STACK(2), STACK(1));
				sp -= 5;
				break;

			case PCD_LSPEC1DIRECT:
				LineSpecials[pc[0]] (activationline, activator,
									 pc[1], 0, 0, 0, 0);
				pc += 2;
				break;

			case PCD_LSPEC2DIRECT:
				LineSpecials[pc[0]] (activationline, activator,
									 pc[1], pc[2], 0, 0, 0);
				pc += 3;
				break;

			case PCD_LSPEC3DIRECT:
				LineSpecials[pc[0]] (activationline, activator,
									 pc[1], pc[2], pc[3], 0, 0);
				pc += 4;
				break;

			case PCD_LSPEC4DIRECT:
				LineSpecials[pc[0]] (activationline, activator,
									 pc[1], pc[2], pc[3], pc[4], 0);
				pc += 5;
				break;

			case PCD_LSPEC5DIRECT:
				LineSpecials[pc[0]] (activationline, activator,
									 pc[1], pc[2], pc[3], pc[4], pc[5]);
				pc += 6;
				break;

			case PCD_ADD:
				STACK(2) = STACK(2) + STACK(1);
				sp--;
				break;

			case PCD_SUBTRACT:
				STACK(2) = STACK(2) - STACK(1);
				sp--;
				break;

			case PCD_MULTIPLY:
				STACK(2) = STACK(2) * STACK(1);
				sp--;
				break;

			case PCD_DIVIDE:
				STACK(2) = STACK(2) / STACK(1);
				sp--;
				break;

			case PCD_MODULUS:
				STACK(2) = STACK(2) % STACK(1);
				sp--;
				break;

			case PCD_EQ:
				STACK(2) = (STACK(2) == STACK(1));
				sp--;
				break;

			case PCD_NE:
				STACK(2) = (STACK(2) != STACK(1));
				sp--;
				break;

			case PCD_LT:
				STACK(2) = (STACK(2) < STACK(1));
				sp--;
				break;

			case PCD_GT:
				STACK(2) = (STACK(2) > STACK(1));
				sp--;
				break;

			case PCD_LE:
				STACK(2) = (STACK(2) <= STACK(1));
				sp--;
				break;

			case PCD_GE:
				STACK(2) = (STACK(2) >= STACK(1));
				sp--;
				break;

			case PCD_ASSIGNSCRIPTVAR:
				locals[NEXTWORD] = STACK(1);
				sp--;
				break;


			case PCD_ASSIGNMAPVAR:
				level.vars[NEXTWORD] = STACK(1);
				sp--;
				break;

			case PCD_ASSIGNWORLDVAR:
				WorldVars[NEXTWORD] = STACK(1);
				sp--;
				break;

			case PCD_PUSHSCRIPTVAR:
				PushToStack (locals[NEXTWORD]);
				break;

			case PCD_PUSHMAPVAR:
				PushToStack (level.vars[NEXTWORD]);
				break;

			case PCD_PUSHWORLDVAR:
				PushToStack (WorldVars[NEXTWORD]);
				break;

			case PCD_ADDSCRIPTVAR:
				locals[NEXTWORD] += STACK(1);
				sp--;
				break;

			case PCD_ADDMAPVAR:
				level.vars[NEXTWORD] += STACK(1);
				sp--;
				break;

			case PCD_ADDWORLDVAR:
				WorldVars[NEXTWORD] += STACK(1);
				sp--;
				break;

			case PCD_SUBSCRIPTVAR:
				locals[NEXTWORD] -= STACK(1);
				sp--;
				break;

			case PCD_SUBMAPVAR:
				level.vars[NEXTWORD] -= STACK(1);
				sp--;
				break;

			case PCD_SUBWORLDVAR:
				WorldVars[NEXTWORD] -= STACK(1);
				sp--;
				break;

			case PCD_MULSCRIPTVAR:
				locals[NEXTWORD] *= STACK(1);
				sp--;
				break;

			case PCD_MULMAPVAR:
				level.vars[NEXTWORD] *= STACK(1);
				sp--;
				break;

			case PCD_MULWORLDVAR:
				WorldVars[NEXTWORD] *= STACK(1);
				sp--;
				break;

			case PCD_DIVSCRIPTVAR:
				locals[NEXTWORD] /= STACK(1);
				sp--;
				break;

			case PCD_DIVMAPVAR:
				level.vars[NEXTWORD] /= STACK(1);
				sp--;
				break;

			case PCD_DIVWORLDVAR:
				WorldVars[NEXTWORD] /= STACK(1);
				sp--;
				break;

			case PCD_MODSCRIPTVAR:
				locals[NEXTWORD] %= STACK(1);
				sp--;
				break;

			case PCD_MODMAPVAR:
				level.vars[NEXTWORD] %= STACK(1);
				sp--;
				break;

			case PCD_MODWORLDVAR:
				WorldVars[NEXTWORD] %= STACK(1);
				sp--;
				break;

			case PCD_INCSCRIPTVAR:
				locals[NEXTWORD]++;
				break;

			case PCD_INCMAPVAR:
				level.vars[NEXTWORD]++;
				break;

			case PCD_INCWORLDVAR:
				WorldVars[NEXTWORD]++;
				break;

			case PCD_DECSCRIPTVAR:
				locals[NEXTWORD]--;
				break;

			case PCD_DECMAPVAR:
				level.vars[NEXTWORD]--;
				break;

			case PCD_DECWORLDVAR:
				WorldVars[NEXTWORD]--;
				break;

			case PCD_GOTO:
				pc = (int *)(level.behavior + *(pc));
				break;

			case PCD_IFGOTO:
				if (STACK(1))
					pc = (int *)(level.behavior + *(pc));
				else
					pc++;
				sp--;
				break;

			case PCD_DROP:
				sp--;
				break;

			case PCD_DELAY:
				state = SCRIPT_Delayed;
				statedata = STACK(1);
				sp--;
				break;

			case PCD_DELAYDIRECT:
				state = SCRIPT_Delayed;
				statedata = NEXTWORD;
				break;

			case PCD_RANDOM:
				STACK(2) = Random (STACK(2), STACK(1));
				sp--;
				break;
				
			case PCD_RANDOMDIRECT:
				PushToStack (Random (pc[0], pc[1]));
				pc += 2;
				break;

			case PCD_THINGCOUNT:
				STACK(2) = ThingCount (STACK(2), STACK(1));
				sp--;
				break;

			case PCD_THINGCOUNTDIRECT:
				PushToStack (ThingCount (pc[0], pc[1]));
				pc += 2;
				break;

			case PCD_TAGWAIT:
				state = SCRIPT_TagWait;
				statedata = STACK(1);
				sp--;
				break;

			case PCD_TAGWAITDIRECT:
				state = SCRIPT_TagWait;
				statedata = NEXTWORD;
				break;

			case PCD_POLYWAIT:
				state = SCRIPT_PolyWait;
				statedata = STACK(1);
				sp--;
				break;

			case PCD_POLYWAITDIRECT:
				state = SCRIPT_PolyWait;
				statedata = NEXTWORD;
				break;

			case PCD_CHANGEFLOOR:
				ChangeFlat (STACK(2), STACK(1), 0);
				sp -= 2;
				break;

			case PCD_CHANGEFLOORDIRECT:
				ChangeFlat (pc[0], pc[1], 0);
				pc += 2;
				break;

			case PCD_CHANGECEILING:
				ChangeFlat (STACK(2), STACK(1), 1);
				sp -= 2;
				break;

			case PCD_CHANGECEILINGDIRECT:
				ChangeFlat (pc[0], pc[1], 1);
				pc += 2;
				break;

			case PCD_RESTART:
				pc = (int *)(level.behavior + (P_FindScript (script))[1]);
				break;

			case PCD_ANDLOGICAL:
				STACK(2) = (STACK(2) && STACK(1));
				sp--;
				break;

			case PCD_ORLOGICAL:
				STACK(2) = (STACK(2) || STACK(1));
				sp--;
				break;

			case PCD_ANDBITWISE:
				STACK(2) = (STACK(2) & STACK(1));
				sp--;
				break;

			case PCD_ORBITWISE:
				STACK(2) = (STACK(2) | STACK(1));
				sp--;
				break;

			case PCD_EORBITWISE:
				STACK(2) = (STACK(2) ^ STACK(1));
				sp--;
				break;

			case PCD_NEGATELOGICAL:
				STACK(1) = !STACK(1);
				break;

			case PCD_LSHIFT:
				STACK(2) = (STACK(2) << STACK(1));
				sp--;
				break;

			case PCD_RSHIFT:
				STACK(2) = (STACK(2) >> STACK(1));
				sp--;
				break;

			case PCD_UNARYMINUS:
				STACK(1) = -STACK(1);
				break;

			case PCD_IFNOTGOTO:
				if (!STACK(1))
					pc = (int *)(level.behavior + *(pc));
				else
					pc++;
				sp--;
				break;

			case PCD_LINESIDE:
				PushToStack (lineSide);
				break;

			case PCD_SCRIPTWAIT:
				statedata = STACK(1);
				if (controller->RunningScripts[statedata])
					state = SCRIPT_ScriptWait;
				else
					state = SCRIPT_ScriptWaitPre;
				sp--;
				PutLast ();
				break;

			case PCD_SCRIPTWAITDIRECT:
				state = SCRIPT_ScriptWait;
				statedata = NEXTWORD;
				PutLast ();
				break;

			case PCD_CLEARLINESPECIAL:
				if (activationline)
					activationline->special = 0;
				break;

			case PCD_CASEGOTO:
				if (STACK(1) == NEXTWORD) {
					pc = (int *)(level.behavior + *(pc));
					sp--;
				} else {
					pc++;
				}
				break;

			case PCD_BEGINPRINT:
				stringstart = sp;
				break;

			case PCD_ENDPRINT:
			case PCD_ENDPRINTBOLD:
				if (activator && activator->player == NULL)
				{
					sp = stringstart;
				}
				else
				{
					char work[4096], *workwhere;
					int parse = stringstart;

					workwhere = work;
					while (parse < sp)
					{
						int value = stack[parse++];
						int type = stack[parse++];

						switch (type)
						{
							case 0:		// string
								if (value < level.strings[0])
									workwhere += sprintf (workwhere, "%s",
										level.strings[value+1] + level.behavior);
								break;

							case 1:		// number
								workwhere += sprintf (workwhere, "%d", value);
								break;

							case 2:		// character
								workwhere[0] = value;
								workwhere[1] = 0;
								workwhere++;
								break;

							default:
								break;
						}
					}
					sp = stringstart;
					if (pcd == PCD_ENDPRINTBOLD || activator == NULL ||
						(activator->player - players == consoleplayer))
					{
						strbin (work);
						C_MidPrint (work);
					}
				}
				break;

			case PCD_PRINTSTRING:
				PushToStack (0);
				break;

			case PCD_PRINTNUMBER:
				PushToStack (1);
				break;

			case PCD_PRINTCHARACTER:
				PushToStack (2);
				break;

			case PCD_PLAYERCOUNT:
				PushToStack (CountPlayers ());
				break;

			case PCD_GAMETYPE:
				if (deathmatch.value)
					PushToStack (GAME_NET_DEATHMATCH);
				else if (multiplayer)
					PushToStack (GAME_NET_COOPERATIVE);
				else
					PushToStack (GAME_SINGLE_PLAYER);
				break;
					
			case PCD_GAMESKILL:
				PushToStack ((int)(gameskill.value));
				break;

			case PCD_TIMER:
				PushToStack (level.time);
				break;

			case PCD_SECTORSOUND:
				if (STACK(2) < level.strings[0])
				{
					if (activationline)
					{
						S_Sound (
							activationline->frontsector->soundorg,
							CHAN_BODY,
							(char *)(level.behavior + level.strings[STACK(2)+1]),
							(STACK(1)) / 127,
							ATTN_NORM);
					}
					else
					{
						S_Sound (
							CHAN_BODY,
							(char *)(level.behavior + level.strings[STACK(2)+1]),
							(STACK(1)) / 127,
							ATTN_NORM);
					}
				}
				sp -= 2;
				break;

			case PCD_AMBIENTSOUND:
				if (STACK(2) < level.strings[0])
				{
					S_Sound (CHAN_BODY,
							 (char *)(level.behavior + level.strings[STACK(2)+1]),
							 (STACK(1)) / 127, ATTN_NONE);
				}
				sp -= 2;
				break;

			case PCD_LOCALAMBIENTSOUND:
				if (players[consoleplayer].camera == activator
					&& STACK(2) < level.strings[0])
				{
					S_Sound (CHAN_BODY,
							 (char *)(level.behavior + level.strings[STACK(2)+1]),
							 (STACK(1)) / 127, ATTN_NONE);
				}
				sp -= 2;
				break;

			case PCD_ACTIVATORSOUND:
				if (STACK(2) < level.strings[0])
				{
					S_Sound (activator, CHAN_AUTO,
							 (char *)(level.behavior + level.strings[STACK(2)+1]),
							 (STACK(1)) / 127, ATTN_NORM);
				}
				sp -= 2;
				break;

			case PCD_SOUNDSEQUENCE:
				if (STACK(1) < level.strings[0])
				{
					if (activationline)
					{
						SN_StartSequence (
							activationline->frontsector,
							(char *)(level.behavior + level.strings[STACK(1)+1]));
					}
					else
					{
						//mobj = NULL;
					}
				}
				sp--;
				break;

			case PCD_SETLINETEXTURE:
				SetLineTexture (STACK(4), STACK(3), STACK(2), STACK(1));
				sp -= 4;
				break;

			case PCD_SETLINEBLOCKING:
				{
					int line = -1;

					while ((line = P_FindLineFromID (STACK(2), line)) >= 0)
					{
						switch (STACK(1))
						{
						case BLOCK_NOTHING:
							lines[line].flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);
							break;
						case BLOCK_CREATURES:
						default:
							lines[line].flags &= ~ML_BLOCKEVERYTHING;
							lines[line].flags |= ML_BLOCKING;
							break;
						case BLOCK_EVERYTHING:
							lines[line].flags |= ML_BLOCKING|ML_BLOCKEVERYTHING;
							break;
						}
					}

					sp -= 2;
				}
				break;

			case PCD_SETLINEMONSTERBLOCKING:
				{
					int line = -1;

					while ((line = P_FindLineFromID (STACK(2), line)) >= 0)
					{
						if (STACK(1))
							lines[line].flags |= ML_BLOCKMONSTERS;
						else
							lines[line].flags &= ~ML_BLOCKMONSTERS;
					}

					sp -= 2;
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
					sp -= 7;
				}
				break;

			case PCD_THINGSOUND:
				if (STACK(2) < level.strings[0])
				{
					AActor *spot = AActor::FindByTID (NULL, STACK(3));

					while (spot)
					{
						S_Sound (spot, CHAN_BODY,
								 (char *)(level.behavior + level.strings[STACK(2)+1]),
								 (STACK(1))/127, ATTN_NORM);
						spot = spot->FindByTID (STACK(3));
					}
				}
				sp -= 3;
				break;

		}
	}

	if (state == SCRIPT_PleaseRemove)
	{
		Unlink ();
		if (controller->RunningScripts[script] == this)
			controller->RunningScripts[script] = NULL;
		delete this;
	}
}

static bool P_GetScriptGoing (AActor *who, line_t *where, int num, int *code,
	int lineSide, int arg0, int arg1, int arg2, int always, bool delay)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller && !always && controller->RunningScripts[num])
	{
		if (controller->RunningScripts[num]->GetState () == DLevelScript::SCRIPT_Suspended)
		{
			controller->RunningScripts[num]->SetState (DLevelScript::SCRIPT_Running);
			return true;
		}
		return false;
	}

	new DLevelScript (who, where, num, code, lineSide, arg0, arg1, arg2, always, delay);

	return true;
}

DLevelScript::DLevelScript (AActor *who, line_t *where, int num, int *code, int lineside,
							int arg0, int arg1, int arg2, int always, bool delay)
{
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;

	script = num;
	sp = 0;
	memset (locals, 0, sizeof(locals));
	locals[0] = arg0;
	locals[1] = arg1;
	locals[2] = arg2;
	pc = code;
	activator = who;
	activationline = where;
	lineSide = lineside;
	if (delay) {
		// From Hexen: Give the world some time to set itself up before
		// running open scripts.
		//script->state = SCRIPT_Delayed;
		//script->statedata = TICRATE;
		state = SCRIPT_Running;
	} else {
		state = SCRIPT_Running;
	}

	if (!always)
		DACSThinker::ActiveThinker->RunningScripts[num] = this;

	Link ();

	DPrintf ("Script %d started.\n", num);
}

static void SetScriptState (int script, DLevelScript::EScriptState state)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->RunningScripts[script])
		controller->RunningScripts[script]->SetState (state);
}

void P_DoDeferedScripts (void)
{
	acsdefered_t *def;
	int *scriptdata;

	// Handle defered scripts in this step, too
	def = level.info->defered;
	while (def)
	{
		acsdefered_t *next = def->next;
		switch (def->type)
		{
		case acsdefered_t::defexecute:
		case acsdefered_t::defexealways:
			scriptdata = P_FindScript (def->script);
			if (scriptdata)
			{
				P_GetScriptGoing (NULL, NULL, def->script,
								 (int *)(scriptdata[1] + level.behavior),
								 0, def->arg0, def->arg1, def->arg2,
								 def->type == acsdefered_t::defexealways, true);
			} else
				Printf (PRINT_HIGH, "P_DoDeferredScripts: Unknown script %d\n", def->script);
			break;

		case acsdefered_t::defsuspend:
			SetScriptState (def->script, DLevelScript::SCRIPT_Suspended);
			DPrintf ("Defered suspend of script %d\n", def->script);
			break;

		case acsdefered_t::defterminate:
			SetScriptState (def->script, DLevelScript::SCRIPT_PleaseRemove);
			DPrintf ("Defered terminate of script %d\n", def->script);
			break;
		}
		delete def;
		def = next;
	}
	level.info->defered = NULL;
}

void P_StartOpenScripts (void)
{
	int *script;
	
	if ( (script = level.scripts) )
	{
		int numscripts = *script++;

		for (; numscripts; numscripts--, script += 3)
		{
			if (script[0] >= 1000)
			{
				P_GetScriptGoing (NULL, NULL, script[0] - 1000,
								  (int *)(level.behavior + script[1]), 0, 0, 0, 0, 0, true);
			}
		}
	}
}

static void addDefered (level_info_t *i, acsdefered_t::EType type, int script, int arg0, int arg1, int arg2)
{
	if (i)
	{
		acsdefered_t *def = new acsdefered_s;

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

BOOL P_StartScript (AActor *who, line_t *where, int script, char *map, int lineSide,
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
		addDefered (FindLevelInfo (map),
					always ? acsdefered_t::defexealways : acsdefered_t::defexecute,
					script, arg0, arg1, arg2);
	}
	return false;
}

void P_SuspendScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defsuspend, script, 0, 0, 0);
	else
		SetScriptState (script, DLevelScript::SCRIPT_Suspended);
}

void P_TerminateScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defterminate, script, 0, 0, 0);
	else
		SetScriptState (script, DLevelScript::SCRIPT_PleaseRemove);
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

FArchive &operator<< (FArchive &arc, acsdefered_s *defer)
{
	while (defer)
	{
		arc << (BYTE)defer->type << defer->script
			<< defer->arg0 << defer->arg1 << defer->arg2;
		if (defer->next)
		{
			arc << (BYTE)1;
			defer = defer->next;
		}
	}
	arc << (BYTE)0;
	return arc;
}

FArchive &operator>> (FArchive &arc, acsdefered_s* &defertop)
{
	acsdefered_s **defer = &defertop;
	BYTE inbyte;

	arc >> inbyte;
	while (inbyte)
	{
		*defer = new acsdefered_s;
		arc >> inbyte;
		(*defer)->type = (acsdefered_s::EType)inbyte;
		arc >> (*defer)->script
			>> (*defer)->arg0 >> (*defer)->arg1 >> (*defer)->arg2;
		defer = &((*defer)->next);
		arc >> inbyte;
	}
	*defer = NULL;
	return arc;
}