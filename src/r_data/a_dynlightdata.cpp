/*
** _gl_dynlight.cpp
** Light definitions for actors.
**
**---------------------------------------------------------------------------
** Copyright 2003 Timothy Stump
** Copyright 2005 Christoph Oelckers
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

#include "r_state.h"
#include "g_levellocals.h"
#include "a_dynlight.h"


//==========================================================================
//
// Dehacked aliasing
//
//==========================================================================

inline PClassActor * GetRealType(PClassActor * ti)
{
	PClassActor *rep = ti->GetReplacement(false);
	if (rep != ti && rep != NULL && rep->IsDescendantOf(NAME_DehackedPickup))
	{
		return rep;
	}
	return ti;
}

TDeletingArray<FLightDefaults *> LightDefaults;

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FLightDefaults::FLightDefaults(FName name, ELightType type)
{
	m_Name = name;
	m_type = type;
}

void FLightDefaults::ApplyProperties(ADynamicLight * light) const
{
	auto oldtype = light->lighttype;

	light->lighttype = m_type;
	light->specialf1 = m_Param;
	light->SetOffset(m_Pos);
	light->halo = m_halo;
	for (int a = 0; a < 3; a++) light->args[a] = clamp<int>((int)(m_Args[a]), 0, 255);
	light->args[LIGHT_INTENSITY] = m_Args[LIGHT_INTENSITY];
	light->args[LIGHT_SECONDARY_INTENSITY] = m_Args[LIGHT_SECONDARY_INTENSITY];
	light->lightflags &= ~(LF_ADDITIVE | LF_SUBTRACTIVE | LF_DONTLIGHTSELF);
	if (m_subtractive) light->lightflags |= LF_SUBTRACTIVE;
	if (m_additive) light->lightflags |= LF_ADDITIVE;
	if (m_dontlightself) light->lightflags |= LF_DONTLIGHTSELF;
	if (m_dontlightactors) light->lightflags |= LF_DONTLIGHTACTORS;
	if (m_spot)
		light->lightflags |= LF_SPOT;
	light->SpotInnerAngle = m_spotInnerAngle;
	light->SpotOuterAngle = m_spotOuterAngle;
	light->m_tickCount = 0;
	if (m_type == PulseLight)
	{
		float pulseTime = float(m_Param / TICRATE);

		light->m_lastUpdate = level.maptime;
		if (m_swapped) light->m_cycler.SetParams(float(light->args[LIGHT_SECONDARY_INTENSITY]), float(light->args[LIGHT_INTENSITY]), pulseTime, oldtype == PulseLight);
		else light->m_cycler.SetParams(float(light->args[LIGHT_INTENSITY]), float(light->args[LIGHT_SECONDARY_INTENSITY]), pulseTime, oldtype == PulseLight);
		light->m_cycler.ShouldCycle(true);
		light->m_cycler.SetCycleType(CYCLE_Sin);
		light->m_currentRadius = (float)light->m_cycler.GetVal();
		if (light->m_currentRadius <= 0) light->m_currentRadius = 1;
		light->swapped = m_swapped;
	}

	switch (m_attenuate)
	{
		case 0: light->lightflags &= ~LF_ATTENUATE; break;
		case 1: light->lightflags |= LF_ATTENUATE; break;
		default: if (level.flags3 & LEVEL3_ATTENUATE)  light->lightflags |= LF_ATTENUATE; else light->lightflags &= ~LF_ATTENUATE; break;
	}
	}


//==========================================================================
//
// light definition file parser
//
//==========================================================================


extern int ScriptDepth;

void AddLightDefaults(FLightDefaults *defaults, double attnFactor)
{
   FLightDefaults *temp;
   unsigned int i;

   // remove duplicates
   for (i = 0; i < LightDefaults.Size(); i++)
   {
      temp = LightDefaults[i];
	  if (temp->GetName() == defaults->GetName())
      {
         delete temp;
         LightDefaults.Delete(i);
         break;
      }
   }
   if (defaults->GetAttenuate())
   {
	   defaults->SetArg(LIGHT_INTENSITY, int(defaults->GetArg(LIGHT_INTENSITY) * attnFactor));
	   defaults->SetArg(LIGHT_SECONDARY_INTENSITY, int(defaults->GetArg(LIGHT_SECONDARY_INTENSITY) * attnFactor));
   }

   LightDefaults.Push(defaults);
}

//==========================================================================
//
//
//
//==========================================================================

FInternalLightAssociation::FInternalLightAssociation(FLightAssociation * asso)
{

	m_AssocLight=NULL;
	for(unsigned int i=0;i<LightDefaults.Size();i++)
	{
		if (LightDefaults[i]->GetName() == asso->Light())
		{
			m_AssocLight = LightDefaults[i];
			break;
		}
	}

	m_sprite=-1;
	m_frame = -1;
	for (unsigned i = 0; i < sprites.Size (); ++i)
	{
		if (strncmp (sprites[i].name, asso->FrameName(), 4) == 0)
		{
			m_sprite = (int)i;
			break;
		}
	}

	// Only handle lights for full frames. 
	// I won't bother with special lights for single rotations
	// because there is no decent use for them!
	if (strlen(asso->FrameName())==5 || asso->FrameName()[5]=='0')
	{
		m_frame = toupper(asso->FrameName()[4])-'A';
	}
}

//==========================================================================
//
// State lights
//
//==========================================================================
static TArray<FName> ParsedStateLights;
TArray<FLightDefaults *> StateLights;

//==========================================================================
//
//
//
//==========================================================================

void InitializeActorLights(TArray<FLightAssociation> &LightAssociations)
{
	for(unsigned int i=0;i<LightAssociations.Size();i++)
	{
		PClassActor * ti = PClass::FindActor(LightAssociations[i].ActorName());
		if (ti)
		{
			ti = GetRealType(ti);
			// put this in the class data arena so that we do not have to worry about deleting it ourselves.
			void *mem = ClassDataAllocator.Alloc(sizeof(FInternalLightAssociation));
			FInternalLightAssociation * iasso = new(mem) FInternalLightAssociation(&LightAssociations[i]);
			if (iasso->Light() != nullptr)
				ti->ActorInfo()->LightAssociations.Push(iasso);
		}
	}

	StateLights.Resize(ParsedStateLights.Size()+1);
	for(unsigned i=0; i<ParsedStateLights.Size();i++)
	{
		if (ParsedStateLights[i] != NAME_None)
		{
			StateLights[i] = (FLightDefaults*)-1;	// something invalid that's not NULL.
			for(unsigned int j=0;j<LightDefaults.Size();j++)
			{
				if (LightDefaults[j]->GetName() == ParsedStateLights[i])
				{
					StateLights[i] = LightDefaults[j];
					break;
				}
			}
		}
		else StateLights[i] = NULL;
	}
	StateLights[StateLights.Size()-1] = NULL;	// terminator
	ParsedStateLights.Clear();
	ParsedStateLights.ShrinkToFit();
}

//==========================================================================
//
//
//
//==========================================================================

void AddStateLight(FState *State, const char *lname)
{
	if (State->Light == 0)
	{
		ParsedStateLights.Push(NAME_None);
		State->Light = ParsedStateLights.Push(FName(lname));
	}
	else
	{
		ParsedStateLights.Push(FName(lname));
	}
}
