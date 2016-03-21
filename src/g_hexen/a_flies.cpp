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
	PARAM_ACTION_PROLOGUE;

	validcount++;
	AActor *other = FindCorpse(self, self->Sector, 5);
	if (other != NULL)
	{
		self->target = other;
		self->SetState(self->FindState("Buzz"));
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_FlyBuzz)
{
	PARAM_ACTION_PROLOGUE;

	AActor *targ = self->target;

	if (targ == NULL || !(targ->flags & MF_CORPSE) || pr_fly() < 5)
	{
		self->SetIdle();
		return 0;
	}

	self->Angles.Yaw = self->AngleTo(targ);
	self->args[0]++;
	if (!P_TryMove(self, self->Pos().XY() + self->Angles.Yaw.ToVector(6), true))
	{
		self->SetIdle(true);
		return 0;
	}
	if (self->args[0] & 2)
	{
		self->Vel.X += (pr_fly() - 128) / 512.;
		self->Vel.Y += (pr_fly() - 128) / 512.;
	}
	int zrand = pr_fly();
	if (targ->Z() + 5. < self->Z() && zrand > 150)
	{
		zrand = -zrand;
	}
	self->Vel.Z = zrand / 512.;
	if (pr_fly() < 40)
	{
		S_Sound(self, CHAN_VOICE, self->ActiveSound, 0.5f, ATTN_STATIC);
	}
	return 0;
}
