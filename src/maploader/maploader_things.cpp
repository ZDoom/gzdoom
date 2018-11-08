

//===========================================================================
//
// SpawnMapThing
//
//===========================================================================
CVAR(Bool, dumpspawnedthings, false, 0)

AActor *SpawnMapThing(int index, FMapThing *mt, int position)
{
	AActor *spawned = P_SpawnMapThing(mt, position);
	if (dumpspawnedthings)
	{
		Printf("%5d: (%5f, %5f, %5f), doomednum = %5d, flags = %04x, type = %s\n",
			index, mt->pos.X, mt->pos.Y, mt->pos.Z, mt->EdNum, mt->flags, 
			spawned? spawned->GetClass()->TypeName.GetChars() : "(none)");
	}
	T_AddSpawnedThing(spawned);
	return spawned;
}

//===========================================================================
//
// SetMapThingUserData
//
//===========================================================================

static void SetMapThingUserData(AActor *actor, unsigned udi)
{
	if (actor == NULL)
	{
		return;
	}
	while (MapThingsUserData[udi].Key != NAME_None)
	{
		FName varname = MapThingsUserData[udi].Key;
		PField *var = dyn_cast<PField>(actor->GetClass()->FindSymbol(varname, true));

		if (var == NULL || (var->Flags & (VARF_Native|VARF_Private|VARF_Protected|VARF_Static)) || !var->Type->isScalar())
		{
			DPrintf(DMSG_WARNING, "%s is not a writable user variable in class %s\n", varname.GetChars(),
				actor->GetClass()->TypeName.GetChars());
		}
		else
		{ // Set the value of the specified user variable.
			void *addr = reinterpret_cast<uint8_t *>(actor) + var->Offset;
			if (var->Type == TypeString)
			{
				var->Type->InitializeValue(addr, &MapThingsUserData[udi].StringVal);
			}
			else if (var->Type->isFloat())
			{
				var->Type->SetValue(addr, MapThingsUserData[udi].FloatVal);
			}
			else if (var->Type->isInt() || var->Type == TypeBool)
			{
				var->Type->SetValue(addr, MapThingsUserData[udi].IntVal);
			}
		}

		udi++;
	}
}

//===========================================================================
//
// P_LoadThings
//
//===========================================================================

uint16_t MakeSkill(int flags)
{
	uint16_t res = 0;
	if (flags & 1) res |= 1+2;
	if (flags & 2) res |= 4;
	if (flags & 4) res |= 8+16;
	return res;
}

void P_LoadThings (MapData * map)
{
	int	lumplen = map->Size(ML_THINGS);
	int numthings = lumplen / sizeof(mapthing_t);

	char *mtp;
	mapthing_t *mt;

	mtp = new char[lumplen];
	map->Read(ML_THINGS, mtp);
	mt = (mapthing_t*)mtp;

	MapThingsConverted.Resize(numthings);
	FMapThing *mti = &MapThingsConverted[0];

	// [RH] ZDoom now uses Hexen-style maps as its native format.
	//		Since this is the only place where Doom-style Things are ever
	//		referenced, we translate them into a Hexen-style thing.
	for (int i=0 ; i < numthings; i++, mt++)
	{
		// [RH] At this point, monsters unique to Doom II were weeded out
		//		if the IWAD wasn't for Doom II. P_SpawnMapThing() can now
		//		handle these and more cases better, so we just pass it
		//		everything and let it decide what to do with them.

		// [RH] Need to translate the spawn flags to Hexen format.
		short flags = LittleShort(mt->options);

		memset (&mti[i], 0, sizeof(mti[i]));

		mti[i].Gravity = 1;
		mti[i].Conversation = 0;
		mti[i].SkillFilter = MakeSkill(flags);
		mti[i].ClassFilter = 0xffff;	// Doom map format doesn't have class flags so spawn for all player classes
		mti[i].RenderStyle = STYLE_Count;
		mti[i].Alpha = -1;
		mti[i].Health = 1;
		mti[i].FloatbobPhase = -1;

		mti[i].pos.X = LittleShort(mt->x);
		mti[i].pos.Y = LittleShort(mt->y);
		mti[i].angle = LittleShort(mt->angle);
		mti[i].EdNum = LittleShort(mt->type);
		mti[i].info = DoomEdMap.CheckKey(mti[i].EdNum);


#ifndef NO_EDATA
		if (mti[i].info != NULL && mti[i].info->Special == SMT_EDThing)
		{
			ProcessEDMapthing(&mti[i], flags);
		}
		else
#endif
		{
			flags &= ~MTF_SKILLMASK;
			mti[i].flags = (short)((flags & 0xf) | 0x7e0);
			if (gameinfo.gametype == GAME_Strife)
			{
				mti[i].flags &= ~MTF_AMBUSH;
				if (flags & STF_SHADOW)			mti[i].flags |= MTF_SHADOW;
				if (flags & STF_ALTSHADOW)		mti[i].flags |= MTF_ALTSHADOW;
				if (flags & STF_STANDSTILL)		mti[i].flags |= MTF_STANDSTILL;
				if (flags & STF_AMBUSH)			mti[i].flags |= MTF_AMBUSH;
				if (flags & STF_FRIENDLY)		mti[i].flags |= MTF_FRIENDLY;
			}
			else
			{
				if (flags & BTF_BADEDITORCHECK)
				{
					flags &= 0x1F;
				}
				if (flags & BTF_NOTDEATHMATCH)	mti[i].flags &= ~MTF_DEATHMATCH;
				if (flags & BTF_NOTCOOPERATIVE)	mti[i].flags &= ~MTF_COOPERATIVE;
				if (flags & BTF_FRIENDLY)		mti[i].flags |= MTF_FRIENDLY;
			}
			if (flags & BTF_NOTSINGLE)			mti[i].flags &= ~MTF_SINGLE;
		}
	}
	delete [] mtp;
}

//===========================================================================
//
// [RH]
// P_LoadThings2
//
// Same as P_LoadThings() except it assumes Things are
// saved Hexen-style. Position also controls which single-
// player start spots are spawned by filtering out those
// whose first parameter don't match position.
//
//===========================================================================

void P_LoadThings2 (MapData * map)
{
	int	lumplen = map->Size(ML_THINGS);
	int numthings = lumplen / sizeof(mapthinghexen_t);

	char *mtp;

	MapThingsConverted.Resize(numthings);
	FMapThing *mti = &MapThingsConverted[0];

	mtp = new char[lumplen];
	map->Read(ML_THINGS, mtp);
	mapthinghexen_t *mth = (mapthinghexen_t*)mtp;

	for(int i = 0; i< numthings; i++)
	{
		memset (&mti[i], 0, sizeof(mti[i]));

		mti[i].thingid = LittleShort(mth[i].thingid);
		mti[i].pos.X = LittleShort(mth[i].x);
		mti[i].pos.Y = LittleShort(mth[i].y);
		mti[i].pos.Z = LittleShort(mth[i].z);
		mti[i].angle = LittleShort(mth[i].angle);
		mti[i].EdNum = LittleShort(mth[i].type);
		mti[i].info = DoomEdMap.CheckKey(mti[i].EdNum);
		mti[i].flags = LittleShort(mth[i].flags);
		mti[i].special = mth[i].special;
		for(int j=0;j<5;j++) mti[i].args[j] = mth[i].args[j];
		mti[i].SkillFilter = MakeSkill(mti[i].flags);
		mti[i].ClassFilter = (mti[i].flags & MTF_CLASS_MASK) >> MTF_CLASS_SHIFT;
		mti[i].flags &= ~(MTF_SKILLMASK|MTF_CLASS_MASK);
		if (level.flags2 & LEVEL2_HEXENHACK)
		{
			mti[i].flags &= 0x7ff;	// mask out Strife flags if playing an original Hexen map.
		}

		mti[i].Gravity = 1;
		mti[i].RenderStyle = STYLE_Count;
		mti[i].Alpha = -1;
		mti[i].Health = 1;
		mti[i].FloatbobPhase = -1;
		mti[i].friendlyseeblocks = -1;
	}
	delete[] mtp;
}

//===========================================================================
//
//
//
//===========================================================================

void P_SpawnThings (int position)
{
	int numthings = MapThingsConverted.Size();

	for (int i=0; i < numthings; i++)
	{
		AActor *actor = SpawnMapThing (i, &MapThingsConverted[i], position);
		unsigned *udi = MapThingsUserDataIndex.CheckKey((unsigned)i);
		if (udi != NULL)
		{
			SetMapThingUserData(actor, *udi);
		}
	}
}



