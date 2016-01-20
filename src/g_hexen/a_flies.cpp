static FRandom pr_fly("GetOffMeFly");

//===========================================================================
//
// FindCorpse
//
// Finds a corpse to buzz around. We can't use a blockmap check because
// corpses generally aren't linked into the blockmap.
//
//===========================================================================

static AActor *FindCorpse(AActor *fly, sector_t *sec, int recurselimit)
{
	AActor *fallback = NULL;
	sec->validcount = validcount;

	// Search the current sector
	for (AActor *check = sec->thinglist; check != NULL; check = check->snext)
	{
		if (check == fly)
			continue;
		if (!(check->flags & MF_CORPSE))
			continue;
		if (!P_CheckSight(fly, check))
			continue;
		fallback = check;
		if (pr_fly(2))	// 50% chance to try to pick a different corpse
			continue;
		return check;
	}
	if (--recurselimit <= 0 || (fallback != NULL && pr_fly(2)))
	{
		return fallback;
	}
	// Try neighboring sectors
	for (int i = 0; i < sec->linecount; ++i)
	{
		line_t *line = sec->lines[i];
		sector_t *sec2 = (line->frontsector == sec) ? line->backsector : line->frontsector;
		if (sec2 != NULL && sec2->validcount != validcount)
		{
			AActor *neighbor = FindCorpse(fly, sec2, recurselimit);
			if (neighbor != NULL)
			{
				return neighbor;
			}
		}
	}
	return fallback;
}

DEFINE_ACTION_FUNCTION(AActor, A_FlySearch)
{
	// The version from the retail beta is not so great for general use:
	// 1. Pick one of the first fifty thinkers at random.
	// 2. Starting from that thinker, find one that is an actor, not itself,
	//    and within sight. Give up after 100 sequential thinkers.
	// It's effectively useless if there are more than 150 thinkers on a map.
	//
	// So search the sectors instead. We can't potentially find something all
	// the way on the other side of the map and we can't find invisible corpses,
	// but at least we aren't crippled on maps with lots of stuff going on.
	validcount++;
	AActor *other = FindCorpse(self, self->Sector, 5);
	if (other != NULL)
	{
		self->target = other;
		self->SetState(self->FindState("Buzz"));
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_FlyBuzz)
{
	AActor *targ = self->target;

	if (targ == NULL || !(targ->flags & MF_CORPSE) || pr_fly() < 5)
	{
		self->SetIdle();
		return;
	}

	angle_t ang = self->AngleTo(targ);
	self->angle = ang;
	self->args[0]++;
	ang >>= ANGLETOFINESHIFT;
	if (!P_TryMove(self, self->X() + 6 * finecosine[ang], self->Y() + 6 * finesine[ang], true))
	{
		self->SetIdle(true);
		return;
	}
	if (self->args[0] & 2)
	{
		self->velx += (pr_fly() - 128) << BOBTOFINESHIFT;
		self->vely += (pr_fly() - 128) << BOBTOFINESHIFT;
	}
	int zrand = pr_fly();
	if (targ->Z() + 5*FRACUNIT < self->Z() && zrand > 150)
	{
		zrand = -zrand;
	}
	self->velz = zrand << BOBTOFINESHIFT;
	if (pr_fly() < 40)
	{
		S_Sound(self, CHAN_VOICE, self->ActiveSound, 0.5f, ATTN_STATIC);
	}
}
