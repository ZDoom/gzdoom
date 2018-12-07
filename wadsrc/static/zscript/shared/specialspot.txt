class SpotState : Object native
{
	native static SpotState GetSpotState(bool create = true);
	native SpecialSpot GetNextInList(class<Actor> type, int skipcounter);
	native SpecialSpot GetSpotWithMinMaxDistance(Class<Actor> type, double x, double y, double mindist, double maxdist);
	native SpecialSpot GetRandomSpot(class<Actor> type, bool onlyonce);
	native void AddSpot(SpecialSpot spot);
	native void RemoveSpot(SpecialSpot spot);
	
}

class SpecialSpot : Actor
{
	override void BeginPlay()
	{
		let sstate = SpotState.GetSpotState();
		if (sstate != NULL) sstate.AddSpot(self);
		Super.BeginPlay();
	}

	//----------------------------------------------------------------------------
	//
	//
	//
	//----------------------------------------------------------------------------

	override void OnDestroy()
	{
		let sstate = SpotState.GetSpotState(false);
		if (sstate != NULL) sstate.RemoveSpot(self);
		Super.OnDestroy();
	}
	
	// Mace spawn spot ----------------------------------------------------------

	// Every mace spawn spot will execute this action. The first one
	// will build a list of all mace spots in the level and spawn a
	// mace. The rest of the spots will do nothing.

	void A_SpawnSingleItem(class<Actor> cls, int fail_sp = 0, int fail_co = 0, int fail_dm = 0)
	{
		Actor spot = NULL;
		let state = SpotState.GetSpotState();

		if (state != NULL) spot = state.GetRandomSpot(GetClass(), true);
		if (spot == NULL) return;

		if (!multiplayer && random[SpawnMace]() < fail_sp)
		{ // Sometimes doesn't show up if not in deathmatch
			return;
		}

		if (multiplayer && !deathmatch && random[SpawnMace]() < fail_co)
		{
			return;
		}

		if (deathmatch && random[SpawnMace]() < fail_dm)
		{
			return;
		}

		if (cls == NULL)
		{
			return;
		}

		let spawned = Spawn(cls, self.Pos, ALLOW_REPLACE);

		if (spawned)
		{
			// Setting z in two steps is necessary to proper initialize floorz before using it.
			spawned.SetOrigin (spot.Pos, false);
			spawned.SetZ(spawned.floorz);
			// We want this to respawn.
			if (!bDropped)
			{
				spawned.bDropped = false;
			}
			let inv = Inventory(spawned);
			if (inv)
			{
				inv.SpawnPointClass = GetClass();
			}
		}
	}


}
