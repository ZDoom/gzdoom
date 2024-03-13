// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2000 Simon Howard
// Copyright(C) 2005-2008 Christoph Oelckers
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
//--------------------------------------------------------------------------
//
// scripting.
//
// delayed scripts, running scripts, console cmds etc in here
// the interface between FraggleScript and the rest of the game
//
// By Simon Howard
//
// (completely redone and cleaned up in 2008 by Christoph Oelckers)
//
//---------------------------------------------------------------------------
//

#include "t_script.h"
#include "a_keys.h"
#include "d_player.h"
#include "p_spec.h"
#include "c_dispatch.h"
#include "serializer.h"
#include "serialize_obj.h"
#include "g_levellocals.h"

//==========================================================================
//
//
//
//==========================================================================

#define IMPLEMENT_16_POINTERS(v, i) \
	IMPLEMENT_POINTER(v[i]) \
	IMPLEMENT_POINTER(v[i+1]) \
	IMPLEMENT_POINTER(v[i+2]) \
	IMPLEMENT_POINTER(v[i+3]) \
	IMPLEMENT_POINTER(v[i+4]) \
	IMPLEMENT_POINTER(v[i+5]) \
	IMPLEMENT_POINTER(v[i+6]) \
	IMPLEMENT_POINTER(v[i+7]) \
	IMPLEMENT_POINTER(v[i+8]) \
	IMPLEMENT_POINTER(v[i+9]) \
	IMPLEMENT_POINTER(v[i+10]) \
	IMPLEMENT_POINTER(v[i+11]) \
	IMPLEMENT_POINTER(v[i+12]) \
	IMPLEMENT_POINTER(v[i+13]) \
	IMPLEMENT_POINTER(v[i+14]) \
	IMPLEMENT_POINTER(v[i+15]) \

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DFsScript, false, true)

IMPLEMENT_POINTERS_START(DFsScript)
	IMPLEMENT_POINTER(parent)
	IMPLEMENT_POINTER(trigger)
	IMPLEMENT_16_POINTERS(sections, 0)
	IMPLEMENT_POINTER(sections[16])
	IMPLEMENT_16_POINTERS(variables, 0)
	IMPLEMENT_16_POINTERS(children, 0)
	IMPLEMENT_16_POINTERS(children, 16)
	IMPLEMENT_16_POINTERS(children, 32)
	IMPLEMENT_16_POINTERS(children, 48)
	IMPLEMENT_16_POINTERS(children, 64)
	IMPLEMENT_16_POINTERS(children, 80)
	IMPLEMENT_16_POINTERS(children, 96)
	IMPLEMENT_16_POINTERS(children, 112)
	IMPLEMENT_16_POINTERS(children, 128)
	IMPLEMENT_16_POINTERS(children, 144)
	IMPLEMENT_16_POINTERS(children, 160)
	IMPLEMENT_16_POINTERS(children, 176)
	IMPLEMENT_16_POINTERS(children, 192)
	IMPLEMENT_16_POINTERS(children, 208)
	IMPLEMENT_16_POINTERS(children, 224)
	IMPLEMENT_16_POINTERS(children, 240)
	IMPLEMENT_POINTER(children[256])
IMPLEMENT_POINTERS_END

//==========================================================================
//
//
//
//==========================================================================

void DFsScript::ClearChildren()
{
	int j;
	for(j=0;j<MAXSCRIPTS;j++) if (children[j])
	{
		children[j]->Destroy();
		children[j]=nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

DFsScript::DFsScript()
{
	int i;
	
	for(i=0; i<SECTIONSLOTS; i++) sections[i] = nullptr;
	for(i=0; i<VARIABLESLOTS; i++) variables[i] = nullptr;
	for(i=0; i<MAXSCRIPTS; i++)	children[i] = nullptr;

	scriptnum = -1;
	len = 0;
	parent = nullptr;
	trigger = nullptr;
	lastiftrue = false;
}

//==========================================================================
//
//
//
//==========================================================================

void DFsScript::OnDestroy()
{
	ClearVariables(true);
	ClearSections();
	ClearChildren();
	parent = nullptr;
	Data.Reset(); // lose the buffer now and don't wait until getting collected.
	parent = nullptr;
	trigger = nullptr;
	Super::OnDestroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DFsScript::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);

	arc("data", Data)
		("scriptnum", scriptnum)
		("len", len)
		("parent", parent)
		("trigger", trigger)
		("lastiftrue", lastiftrue)
		.Array("sections", sections, SECTIONSLOTS)
		.Array("variables", variables, VARIABLESLOTS)
		.Array("children", children, MAXSCRIPTS);
}

//==========================================================================
//
// run_script
//
// the function called by t_script.c
//
//==========================================================================

void DFsScript::ParseScript(char *position, DFraggleThinker *th)
{
	if (position == nullptr) 
	{
		lastiftrue = false;
		position = Data.Data();
	}

	// check for valid position
	if(position < Data.Data() || position > Data.Data() +len)
    {
		Printf("script %d: trying to continue from point outside script!\n", scriptnum);
		return;
    }
	
	th->trigger_obj = trigger;  // set trigger variable. 
	
	try
	{
		FParser parse(th->Level, this);
		parse.Run(position, Data.Data(), Data.Data() + len);
	}
	catch (CFraggleScriptError &err)
	{
		Printf ("%s\n", err.GetMessage());
	}
	
	// dont clear global vars!
	if(scriptnum != -1) ClearVariables();        // free variables
	
	// haleyjd
	lastiftrue = false;
}

//==========================================================================
//
// Running Scripts
//
//==========================================================================

IMPLEMENT_CLASS(DRunningScript, false, true)

IMPLEMENT_POINTERS_START(DRunningScript)
	IMPLEMENT_POINTER(prev)
	IMPLEMENT_POINTER(next)
	IMPLEMENT_POINTER(trigger)
	IMPLEMENT_16_POINTERS(variables, 0)
IMPLEMENT_POINTERS_END

//==========================================================================
//
//
//
//==========================================================================

DRunningScript::DRunningScript(AActor *trigger, DFsScript *owner, int index) 
{
	prev = next = nullptr;
	script = owner;
	GC::WriteBarrier(this, script);
	save_point = index;
	wait_type = wt_none;
	wait_data = 0;

	this->trigger = trigger;
	if (owner == nullptr)
	{
		for(int i=0; i< VARIABLESLOTS; i++) variables[i] = nullptr;
	}
	else
	{
		// save the script variables 
		for(int i=0; i<VARIABLESLOTS; i++)
		{
			variables[i] = owner->variables[i];
			
			if (index == 0)	// we are starting another Script:
			{
				// remove all the variables from the script variable list
				// we only start with the basic labels
				while(variables[i] && variables[i]->type != svt_label)
					variables[i] = variables[i]->next;
			}
			else // a script is being halted
			{
				// remove all the variables from the script variable list
				// to prevent them being removed when the script stops
				while(owner->variables[i] && owner->variables[i]->type != svt_label)
					owner->variables[i] = owner->variables[i]->next;

				GC::WriteBarrier(owner, owner->variables[i]);
			}

			GC::WriteBarrier(this, variables[i]);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DRunningScript::OnDestroy()
{
	int i;
	DFsVariable *current, *next;
	
	for(i=0; i<VARIABLESLOTS; i++)
    {
		current = variables[i];
		
		// go thru this chain
		while(current)
		{
			next = current->next; // save for after freeing
			current->Destroy();
			current = next; // go to next in chain
		}
		variables[i] = nullptr;
    }
	Super::OnDestroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DRunningScript::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("script", script)
		("save_point", save_point)
		("wait_type", wait_type)
		("wait_data", wait_data)
		("prev", prev)
		("next", next)
		("trigger", trigger)
		.Array("variables", variables, VARIABLESLOTS);
}


//==========================================================================
//
// The main thinker
//
//==========================================================================

IMPLEMENT_CLASS(DFraggleThinker, false, true)

IMPLEMENT_POINTERS_START(DFraggleThinker)
	IMPLEMENT_POINTER(RunningScripts)
	IMPLEMENT_POINTER(LevelScript)
	IMPLEMENT_POINTER(GlobalScript)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// This thinker is a little unusual from all the rest, because it
// needs to construct some non-serializable data.
//
// This cannot be done in Construct, but requires an actual constructor,
// so that even a deserialized ionstance is fully set up.
//
//==========================================================================

DFraggleThinker::DFraggleThinker() 
{
	GlobalScript = Create<DFsScript>();
	GC::WriteBarrier(this, GlobalScript);
	InitFunctions();
}

//==========================================================================
//
//
//
//==========================================================================

void DFraggleThinker::Construct()
{
	RunningScripts = Create<DRunningScript>();
	GC::WriteBarrier(this, RunningScripts);
	LevelScript = Create<DFsScript>();
	LevelScript->parent = nullptr;
	GC::WriteBarrier(this, LevelScript);
}

//==========================================================================
//
//
//
//==========================================================================

void DFraggleThinker::OnDestroy()
{
	DRunningScript *p = RunningScripts;
	while (p)
	{
		DRunningScript *q = p;
		p = p->next;
		q->prev = q->next = nullptr;
		q->Destroy();
	}
	RunningScripts = nullptr;

	GlobalScript->Destroy();
	GlobalScript = nullptr;

	LevelScript->Destroy();
	LevelScript = nullptr;

	SpawnedThings.Clear();
	Super::OnDestroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DFraggleThinker::Serialize(FSerializer &arc)
{
	// The global script is not serialized because the data it points to is not serializable.
	Super::Serialize(arc);
	arc("levelscript", LevelScript)
		("runningscripts", RunningScripts)
		("spawnedthings", SpawnedThings);
}

//==========================================================================
//
// PAUSING SCRIPTS
//
//==========================================================================

bool DFraggleThinker::wait_finished(DRunningScript *script)
{
	switch(script->wait_type)
    {
    case wt_none: return true;        // uh? hehe
    case wt_scriptwait:               // waiting for script to finish
		{
			DRunningScript *current;
			for(current = RunningScripts->next; current; current = current->next)
			{
				if(current == script) continue;  // ignore this script
				if(current->script->scriptnum == script->wait_data)
					return false;        // script still running
			}
			return true;        // can continue now
		}
		
    case wt_delay:                          // just count down
		{
			return --script->wait_data <= 0;
		}
		
    case wt_tagwait:
		{
			int secnum;
			auto itr = Level->GetSectorTagIterator(script->wait_data);
			while ((secnum = itr.Next()) >= 0)
			{
				sector_t *sec = &Level->sectors[secnum];
				if(sec->floordata || sec->ceilingdata || sec->lightingdata)
					return false;        // not finished
			}
			return true;
		}
		
    case wt_scriptwaitpre: // haleyjd - wait for script to start
		{
			DRunningScript *current;
			for(current = RunningScripts->next; current; current=current->next)
			{
				if(current == script) continue;  // ignore this script
				if(current->script->scriptnum == script->wait_data)
					return true;        // script is now running
			}
			return false; // no running instances found
		}
		
    default: return true;
    }
	
	return false;
}

//==========================================================================
//
// 
//
//==========================================================================

void DFraggleThinker::Tick()
{
	DRunningScript *current, *next;
	int i;
	
	current = RunningScripts->next;
    
	while(current)
	{
		if(wait_finished(current))
		{
			// copy out the script variables from the
			// runningscript
			
			for(i=0; i<VARIABLESLOTS; i++)
			{
				current->script->variables[i] = current->variables[i];
				GC::WriteBarrier(current->script, current->variables[i]);
				current->variables[i] = nullptr;
			}
			current->script->trigger = current->trigger; // copy trigger
			
			// unhook from chain 
			current->prev->next = current->next;
			GC::WriteBarrier(current->prev, current->next);
			if(current->next) 
			{
				current->next->prev = current->prev;
				GC::WriteBarrier(current->next, current->prev);
			}
			next = current->next;   // save before freeing
			
			// continue the script
			current->script->ParseScript (current->script->Data.Data() + current->save_point, this);

			// free
			current->Destroy();
		}
		else
			next = current->next;
		current = next;   // continue to next in chain
	}
}


//==========================================================================
//
// We have to mark the SpawnedThings array manually because it's not
// in the list of declared pointers.
//
//==========================================================================

size_t DFraggleThinker::PropagateMark()
{
	for(unsigned i=0;i<SpawnedThings.Size();i++)
	{
		GC::Mark(SpawnedThings[i]);
	}
	return Super::PropagateMark();
}

//==========================================================================
//
// Again we have to handle the SpawnedThings array manually because
// it's not in the list of declared pointers.
//
//==========================================================================

size_t DFraggleThinker::PointerSubstitution (DObject *old, DObject *notOld, bool nullOnFail)
{
	size_t changed = Super::PointerSubstitution(old, notOld, nullOnFail);
	for(unsigned i=0;i<SpawnedThings.Size();i++)
	{
		if (SpawnedThings[i] == static_cast<AActor*>(old)) 
		{
			SpawnedThings[i] = static_cast<AActor*>(notOld);
			changed++;
		}
	}
	return changed;
}

//==========================================================================
//
// Adds a running script to the list of running scripts
//
//==========================================================================

void DFraggleThinker::AddRunningScript(DRunningScript *runscr)
{
	runscr->next = RunningScripts->next;
	GC::WriteBarrier(runscr, RunningScripts->next);

	runscr->prev = RunningScripts;
	GC::WriteBarrier(runscr, RunningScripts);

	runscr->prev->next = runscr;
	GC::WriteBarrier(runscr->prev, runscr);

	if(runscr->next)
	{
		runscr->next->prev = runscr;
		GC::WriteBarrier(runscr->next, runscr);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void T_PreprocessScripts(FLevelLocals *Level)
{
	DFraggleThinker *th = Level->FraggleScriptThinker;
	if (th)
	{
		// run the levelscript first
		// get the other scripts
		
		// levelscript started by player 0 'superplayer'
		th->LevelScript->trigger = Level->Players[0]->mo;
		
		th->LevelScript->Preprocess(Level);
		th->LevelScript->ParseScript(nullptr, th);
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool T_RunScript(FLevelLocals *Level, int snum, AActor * t_trigger)
{
	DFraggleThinker *th = Level->FraggleScriptThinker;
	if (th)
	{
		// [CO] It is far too dangerous to start the script right away.
		// Better queue it for execution for the next time
		// the runningscripts are checked.

		if(snum < 0 || snum >= MAXSCRIPTS) return false;

		DFsScript *script = th->LevelScript->children[snum];
		if(!script)	return false;
	
		DRunningScript *runscr = Create<DRunningScript>(t_trigger, script, 0);
		// hook into chain at start
		th->AddRunningScript(runscr);
		return true;
	}
	return false;
}

//==========================================================================
//
// This isn't network safe. FraggleScript as a whole most likely isn't...
//
//==========================================================================

CCMD(fpuke)
{
	int argc = argv.argc();

	if (argc < 2)
	{
		Printf (" fpuke <script>\n");
	}
	else
	{
		T_RunScript(players[consoleplayer].mo->Level, atoi(argv[1]), players[consoleplayer].mo);
	}
}
