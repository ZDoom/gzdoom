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

#include "t_script.h"
#include "p_lnspec.h"
#include "a_keys.h"
#include "d_player.h"
#include "p_spec.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "doomerrors.h"
#include "doomstat.h"
#include "farchive.h"

//==========================================================================
//
// global variables
// These two are the last remaining ones:
// - The global script contains static data so it must be global
// - The trigger is referenced by a global variable. However, it is set 
//   each time a script is started so that's not a problem.
//
//==========================================================================

DFsScript *global_script;
AActor *trigger_obj;

//==========================================================================
//
//
//
//==========================================================================

#define DECLARE_16_POINTERS(v, i) \
	DECLARE_POINTER(v[i]) \
	DECLARE_POINTER(v[i+1]) \
	DECLARE_POINTER(v[i+2]) \
	DECLARE_POINTER(v[i+3]) \
	DECLARE_POINTER(v[i+4]) \
	DECLARE_POINTER(v[i+5]) \
	DECLARE_POINTER(v[i+6]) \
	DECLARE_POINTER(v[i+7]) \
	DECLARE_POINTER(v[i+8]) \
	DECLARE_POINTER(v[i+9]) \
	DECLARE_POINTER(v[i+10]) \
	DECLARE_POINTER(v[i+11]) \
	DECLARE_POINTER(v[i+12]) \
	DECLARE_POINTER(v[i+13]) \
	DECLARE_POINTER(v[i+14]) \
	DECLARE_POINTER(v[i+15]) \

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_POINTY_CLASS(DFsScript)
	DECLARE_POINTER(parent)
	DECLARE_POINTER(trigger)
	DECLARE_16_POINTERS(sections, 0)
	DECLARE_POINTER(sections[16])
	DECLARE_16_POINTERS(variables, 0)
	DECLARE_16_POINTERS(children, 0)
	DECLARE_16_POINTERS(children, 16)
	DECLARE_16_POINTERS(children, 32)
	DECLARE_16_POINTERS(children, 48)
	DECLARE_16_POINTERS(children, 64)
	DECLARE_16_POINTERS(children, 80)
	DECLARE_16_POINTERS(children, 96)
	DECLARE_16_POINTERS(children, 112)
	DECLARE_16_POINTERS(children, 128)
	DECLARE_16_POINTERS(children, 144)
	DECLARE_16_POINTERS(children, 160)
	DECLARE_16_POINTERS(children, 176)
	DECLARE_16_POINTERS(children, 192)
	DECLARE_16_POINTERS(children, 208)
	DECLARE_16_POINTERS(children, 224)
	DECLARE_16_POINTERS(children, 240)
	DECLARE_POINTER(children[256])
END_POINTERS

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
		children[j]=NULL;
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
	
	for(i=0; i<SECTIONSLOTS; i++) sections[i] = NULL;
	for(i=0; i<VARIABLESLOTS; i++) variables[i] = NULL;
	for(i=0; i<MAXSCRIPTS; i++)	children[i] = NULL;

	data = NULL;
	scriptnum = -1;
	len = 0;
	parent = NULL;
	trigger = NULL;
	lastiftrue = false;
}

//==========================================================================
//
//
//
//==========================================================================

void DFsScript::Destroy()
{
	ClearVariables(true);
	ClearSections();
	ClearChildren();
	parent = NULL;
	if (data != NULL) delete [] data;
	data = NULL;
	parent = NULL;
	trigger = NULL;
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DFsScript::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	// don't save a reference to the global script
	if (parent == global_script) parent = NULL;

	arc << data << scriptnum << len << parent << trigger << lastiftrue;
	for(int i=0; i< SECTIONSLOTS; i++) arc << sections[i];
	for(int i=0; i< VARIABLESLOTS; i++) arc << variables[i];
	for(int i=0; i< MAXSCRIPTS; i++) arc << children[i];

	if (parent == NULL) parent = global_script;
}

//==========================================================================
//
// run_script
//
// the function called by t_script.c
//
//==========================================================================

void DFsScript::ParseScript(char *position)
{
	if (position == NULL) 
	{
		lastiftrue = false;
		position = data;
	}

	// check for valid position
	if(position < data || position > data+len)
    {
		Printf("script %d: trying to continue from point outside script!\n", scriptnum);
		return;
    }
	
	trigger_obj = trigger;  // set trigger
	
	try
	{
		FParser parse(this);
		parse.Run(position, data, data + len);
	}
	catch (CRecoverableError &err)
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

IMPLEMENT_POINTY_CLASS(DRunningScript)
	DECLARE_POINTER(prev)
	DECLARE_POINTER(next)
	DECLARE_POINTER(trigger)
	DECLARE_16_POINTERS(variables, 0)
END_POINTERS

//==========================================================================
//
//
//
//==========================================================================

DRunningScript::DRunningScript(AActor *trigger, DFsScript *owner, int index) 
{
	prev = next = NULL;
	script = owner;
	GC::WriteBarrier(this, script);
	save_point = index;
	wait_type = wt_none;
	wait_data = 0;

	this->trigger = trigger;
	if (owner == NULL)
	{
		for(int i=0; i< VARIABLESLOTS; i++) variables[i] = NULL;
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

void DRunningScript::Destroy()
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
		variables[i] = NULL;
    }
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DRunningScript::Serialize(FArchive &arc)
{
	Super::Serialize(arc);

	arc << script << save_point << wait_type << wait_data << prev << next << trigger;
	for(int i=0; i< VARIABLESLOTS; i++) arc << variables[i];
}


//==========================================================================
//
// The main thinker
//
//==========================================================================
IMPLEMENT_POINTY_CLASS(DFraggleThinker)
	DECLARE_POINTER(RunningScripts)
	DECLARE_POINTER(LevelScript)
END_POINTERS

TObjPtr<DFraggleThinker> DFraggleThinker::ActiveThinker;

//==========================================================================
//
//
//
//==========================================================================

DFraggleThinker::DFraggleThinker() 
: DThinker(STAT_SCRIPTS)
{
	if (ActiveThinker)
	{
		I_Error ("Only one FraggleThinker is allowed to exist at a time.\nCheck your code.");
	}
	else
	{
		ActiveThinker = this;
		RunningScripts = new DRunningScript;
		LevelScript = new DFsScript;
		LevelScript->parent = global_script;
		GC::WriteBarrier(this, RunningScripts);
		GC::WriteBarrier(this, LevelScript);
		nocheckposition = false;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DFraggleThinker::Destroy()
{
	DRunningScript *p = RunningScripts;
	while (p)
	{
		DRunningScript *q = p;
		p = p->next;
		q->prev = q->next = NULL;
		q->Destroy();
	}
	RunningScripts = NULL;

	LevelScript->Destroy();
	LevelScript = NULL;

	SpawnedThings.Clear();
	ActiveThinker = NULL;
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DFraggleThinker::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << LevelScript << RunningScripts << SpawnedThings << nocheckposition;
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
			FSectorTagIterator itr(script->wait_data);
			while ((secnum = itr.Next()) >= 0)
			{
				sector_t *sec = &sectors[secnum];
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
				current->variables[i] = NULL;
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
			current->script->ParseScript (current->script->data + current->save_point);

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

size_t DFraggleThinker::PointerSubstitution (DObject *old, DObject *notOld)
{
	size_t changed = Super::PointerSubstitution(old, notOld);
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

void T_PreprocessScripts()
{
	DFraggleThinker *th = DFraggleThinker::ActiveThinker;
	if (th)
	{
		// run the levelscript first
		// get the other scripts
		
		// levelscript started by player 0 'superplayer'
		th->LevelScript->trigger = players[0].mo;
		
		th->LevelScript->Preprocess();
		th->LevelScript->ParseScript();
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool T_RunScript(int snum, AActor * t_trigger)
{
	DFraggleThinker *th = DFraggleThinker::ActiveThinker;
	if (th)
	{
		// [CO] It is far too dangerous to start the script right away.
		// Better queue it for execution for the next time
		// the runningscripts are checked.

		if(snum < 0 || snum >= MAXSCRIPTS) return false;

		DFsScript *script = th->LevelScript->children[snum];
		if(!script)	return false;
	
		DRunningScript *runscr = new DRunningScript(t_trigger, script, 0);
		// hook into chain at start
		th->AddRunningScript(runscr);
		return true;
	}
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

void FS_Close()
{
	if (global_script != NULL)
	{
		GC::DelSoftRoot(global_script);
		global_script->Destroy();
		global_script = NULL;
	}
}

void T_Init()
{
	void init_functions();

	if (global_script == NULL)
	{
		global_script = new DFsScript;
		GC::AddSoftRoot(global_script);
		init_functions();
	}
}

//==========================================================================
//
//
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
		T_RunScript(atoi(argv[1]), players[consoleplayer].mo);
	}
}
