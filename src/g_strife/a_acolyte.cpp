/*
#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_strifeglobal.h"
#include "doomdata.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

//============================================================================
//
// A_HideDecepticon
//
// Hide the Acolyte-to-be							->
// Hide the guy transforming into an Acolyte		->
// Hide the transformer								->
// Transformers are Autobots and Decepticons, and
// Decepticons are the bad guys, so...				->
//
// Hide the Decepticon!
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_HideDecepticon)
{
	EV_DoDoor (DDoor::doorClose, NULL, self, 999, 8*FRACUNIT, 0, 0, 0);
	if (self->target != NULL && self->target->player != NULL)
	{
		P_NoiseAlert (self->target, self);
	}
}

//============================================================================
//
// A_AcolyteDie
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_AcolyteDie)
{
	int i;

	// [RH] Disable translucency here.
	self->RenderStyle = STYLE_Normal;

	// Only the Blue Acolyte does extra stuff on death.
	if (self->GetClass()->TypeName != NAME_AcolyteBlue)
		return;

	// Make sure somebody is still alive
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].health > 0)
			break;
	}
	if (i == MAXPLAYERS)
		return;

	// Make sure all the other blue acolytes are dead.
	TThinkerIterator<AActor> iterator(NAME_AcolyteBlue);
	AActor *other;

	while ( (other = iterator.Next ()) )
	{
		if (other != self && other->health > 0)
		{ // Found a living one
			return;
		}
	}

	players[i].mo->GiveInventoryType (QuestItemClasses[6]);
	players[i].SetLogNumber (14);
	S_StopSound (CHAN_VOICE);
	S_Sound (CHAN_VOICE, "svox/voc14", 1, ATTN_NORM);
}

//============================================================================
//
// A_BeShadowyFoe
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BeShadowyFoe)
{
	self->RenderStyle = STYLE_Translucent;
	self->alpha = HR_SHADOW;
	self->flags &= ~MF_FRIENDLY;
}

//============================================================================
//
// A_AcolyteBits
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_AcolyteBits)
{
	if (self->SpawnFlags & MTF_SHADOW)
	{
		CALL_ACTION(A_BeShadowyFoe, self);
	}
	if (self->SpawnFlags & MTF_ALTSHADOW)
	{
		//self->flags |= MF_STRIFEx8000000;
		if (self->flags & MF_SHADOW)
		{
			// I dunno.
		}
		else
		{
			self->RenderStyle.BlendOp = STYLEOP_None;
		}
	}
}
