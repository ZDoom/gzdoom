// For NULL states, which aren't owned by any actor, the owner
// is recorded as AActor with the following state. AActor should
// never actually have this many states of its own, so this
// is (relatively) safe.

#define NULL_STATE_INDEX	127

// These are special tokens found in the data stream of an archive.
// Whenever a new object is encountered, it gets created using new and
// is then asked to serialize itself before processing of the previous
// object continues. This can result in some very deep recursion if
// you aren't careful about how you organize your data.

#define NEW_OBJ				((BYTE)1)	// Data for a new object follows
#define NEW_CLS_OBJ			((BYTE)2)	// Data for a new class and object follows
#define OLD_OBJ				((BYTE)3)	// Reference to an old object follows
#define NULL_OBJ			((BYTE)4)	// Load as NULL
#define M1_OBJ				((BYTE)44)	// Load as (DObject*)-1

#define NEW_PLYR_OBJ		((BYTE)5)	// Data for a new player follows
#define NEW_PLYR_CLS_OBJ	((BYTE)6)	// Data for a new class and player follows

#define NEW_NAME			((BYTE)27)	// A new name follows
#define OLD_NAME			((BYTE)28)	// Reference to an old name follows
#define NIL_NAME			((BYTE)33)	// Load as NULL

#define NEW_SPRITE			((BYTE)11)	// A new sprite name follows
#define OLD_SPRITE			((BYTE)12)	// Reference to an old sprite name follows

inline FArchive &operator<< (FArchive &arc, FLinkedSector &link)
{
	arc << link.Sector << link.Type;
	return arc;
}

//
// P_ArchiveWorld
//
void P_SerializeWorld (FArchive &arc)
{
	int i, j;
	sector_t *sec;
	line_t *li;
	zone_t *zn;

	// do sectors
	for (i = 0, sec = sectors; i < numsectors; i++, sec++)
	{
		arc << sec->floorplane
			<< sec->ceilingplane;
		arc << sec->lightlevel;
		arc << sec->special;
		arc << sec->soundtraversed
			<< sec->seqType
			<< sec->friction
			<< sec->movefactor
			<< sec->stairlock
			<< sec->prevsec
			<< sec->nextsec
			<< sec->planes[sector_t::floor]
			<< sec->planes[sector_t::ceiling]
			<< sec->heightsec
			<< sec->bottommap << sec->midmap << sec->topmap
			<< sec->gravity;
		P_SerializeTerrain(arc, sec->terrainnum[0]);
		P_SerializeTerrain(arc, sec->terrainnum[1]);
		arc << sec->damageamount;
		arc << sec->damageinterval
			<< sec->leakydamage
			<< sec->damagetype
			<< sec->sky
			<< sec->MoreFlags
			<< sec->Flags
			<< sec->Portals[sector_t::floor] << sec->Portals[sector_t::ceiling]
			<< sec->ZoneNumber;
		arc	<< sec->interpolations[0]
			<< sec->interpolations[1]
			<< sec->interpolations[2]
			<< sec->interpolations[3]
			<< sec->SeqName;

		sec->e->Serialize(arc);
		if (arc.IsStoring ())
		{
			arc << sec->ColorMap->Color
				<< sec->ColorMap->Fade;
			BYTE sat = sec->ColorMap->Desaturate;
			arc << sat;
		}
		else
		{
			PalEntry color, fade;
			BYTE desaturate;
			arc << color << fade
				<< desaturate;
			sec->ColorMap = GetSpecialLights (color, fade, desaturate);
		}
	}

	// do lines
	for (i = 0, li = lines; i < numlines; i++, li++)
	{
		arc << li->flags
			<< li->activation
			<< li->special
			<< li->alpha;

		if (P_IsACSSpecial(li->special))
		{
			P_SerializeACSScriptNumber(arc, li->args[0], false);
		}
		else
		{
			arc << li->args[0];
		}
		arc << li->args[1] << li->args[2] << li->args[3] << li->args[4];

		arc << li->portalindex;
		for (j = 0; j < 2; j++)
		{
			if (li->sidedef[j] == NULL)
				continue;

			side_t *si = li->sidedef[j];
			arc << si->textures[side_t::top]
				<< si->textures[side_t::mid]
				<< si->textures[side_t::bottom]
				<< si->Light
				<< si->Flags
				<< si->LeftSide
				<< si->RightSide
				<< si->Index;
		}
	}

	// do zones
	unsigned numzones = Zones.Size();
	arc << numzones;

	if (arc.IsLoading())
	{
		Zones.Resize(numzones);
	}

	for (i = 0, zn = &Zones[0]; i < (int)numzones; ++i, ++zn)
	{
		arc << zn->Environment;
	}

	arc << linePortals << sectorPortals;
	P_CollectLinkedPortals();
}

void P_SerializeWorldActors(FArchive &arc)
{
	int i;
	sector_t *sec;
	line_t *line;

	for (i = 0, sec = sectors; i < numsectors; i++, sec++)
	{
		arc << sec->SoundTarget
			<< sec->SecActTarget
			<< sec->floordata
			<< sec->ceilingdata
			<< sec->lightingdata;
	}
	for (auto &s : sectorPortals)
	{
		arc << s.mSkybox;
	}
	for (i = 0, line = lines; i < numlines; i++, line++)
	{
		for (int s = 0; s < 2; s++)
		{
			if (line->sidedef[s] != NULL)
			{
				DBaseDecal::SerializeChain(arc, &line->sidedef[s]->AttachedDecals);
			}
		}
	}
}

void extsector_t::Serialize(FArchive &arc)
{
	arc << FakeFloor.Sectors
		<< Midtex.Floor.AttachedLines 
		<< Midtex.Floor.AttachedSectors
		<< Midtex.Ceiling.AttachedLines
		<< Midtex.Ceiling.AttachedSectors
		<< Linked.Floor.Sectors
		<< Linked.Ceiling.Sectors;
}

FArchive &operator<< (FArchive &arc, side_t::part &p)
{
	arc << p.xOffset << p.yOffset << p.interpolation << p.texture 
		<< p.xScale << p.yScale;// << p.Light;
	return arc;
}

FArchive &operator<< (FArchive &arc, sector_t::splane &p)
{
	arc << p.xform.xOffs << p.xform.yOffs << p.xform.xScale << p.xform.yScale 
		<< p.xform.Angle << p.xform.baseyOffs << p.xform.baseAngle
		<< p.Flags << p.Light << p.Texture << p.TexZ << p.alpha;
	return arc;
}

//==========================================================================
//
// ArchiveSubsectors
//
//==========================================================================
void RecalculateDrawnSubsectors();

void P_SerializeSubsectors(FArchive &arc)
{
	int num_verts, num_subs, num_nodes;	
	BYTE by;

	if (arc.IsStoring())
	{
		if (hasglnodes)
		{
			arc << numvertexes << numsubsectors << numnodes;	// These are only for verification
			for(int i=0;i<numsubsectors;i+=8)
			{
				by = 0;
				for(int j=0;j<8;j++)
				{
					if (i+j<numsubsectors && (subsectors[i+j].flags & SSECF_DRAWN))
					{
						by |= (1<<j);
					}
				}
				arc << by;
			}
		}
		else
		{
			int v = 0;
			arc << v << v << v;
		}
	}
	else
	{
		arc << num_verts << num_subs << num_nodes;
		if (num_verts != numvertexes ||
			num_subs != numsubsectors ||
			num_nodes != numnodes)
		{
			// Nodes don't match - we can't use this info
			for(int i=0;i<num_subs;i+=8)
			{
				// Skip the subsector info.
				arc << by;
			}
			if (hasglnodes)
			{
				RecalculateDrawnSubsectors();
			}
			return;
		}
		else
		{
			for(int i=0;i<numsubsectors;i+=8)
			{
				arc << by;
				for(int j=0;j<8;j++)
				{
					if ((by & (1<<j)) && i+j<numsubsectors)
					{
						subsectors[i+j].flags |= SSECF_DRAWN;
					}
				}
			}
		}
	}
}

FArchive &FArchive::WriteObject (DObject *obj)
{
	player_t *player;
	BYTE id[2];

	if (obj == NULL)
	{
		id[0] = NULL_OBJ;
		Write (id, 1);
	}
	else if (obj == (DObject*)~0)
	{
		id[0] = M1_OBJ;
		Write (id, 1);
	}
	else if (obj->ObjectFlags & OF_EuthanizeMe)
	{
		// Objects that want to die are not saved to the archive, but
		// we leave the pointers to them alone.
		id[0] = NULL_OBJ;
		Write (id, 1);
	}
	else
	{
		PClass *type = obj->GetClass();
		DWORD *classarcid;

		if (type == RUNTIME_CLASS(DObject))
		{
			//I_Error ("Tried to save an instance of DObject.\n"
			//		 "This should not happen.\n");
			id[0] = NULL_OBJ;
			Write (id, 1);
		}
		else if (NULL == (classarcid = ClassToArchive.CheckKey(type)))
		{
			// No instances of this class have been written out yet.
			// Write out the class, then write out the object. If this
			// is an actor controlled by a player, make note of that
			// so that it can be overridden when moving around in a hub.
			if (obj->IsKindOf (RUNTIME_CLASS (AActor)) &&
				(player = static_cast<AActor *>(obj)->player) &&
				player->mo == obj)
			{
				id[0] = NEW_PLYR_CLS_OBJ;
				id[1] = (BYTE)(player - players);
				Write (id, 2);
			}
			else
			{
				id[0] = NEW_CLS_OBJ;
				Write (id, 1);
			}
			WriteClass (type);
//			Printf ("Make class %s (%u)\n", type->Name, m_File->Tell());
			MapObject (obj);
			obj->SerializeUserVars (*this);
			obj->Serialize (*this);
			obj->CheckIfSerialized ();
		}
		else
		{
			// An instance of this class has already been saved. If
			// this object has already been written, save a reference
			// to the saved object. Otherwise, save a reference to the
			// class, then save the object. Again, if this is a player-
			// controlled actor, remember that.
			DWORD *objarcid = ObjectToArchive.CheckKey(obj);

			if (objarcid == NULL)
			{

				if (obj->IsKindOf (RUNTIME_CLASS (AActor)) &&
					(player = static_cast<AActor *>(obj)->player) &&
					player->mo == obj)
				{
					id[0] = NEW_PLYR_OBJ;
					id[1] = (BYTE)(player - players);
					Write (id, 2);
				}
				else
				{
					id[0] = NEW_OBJ;
					Write (id, 1);
				}
				WriteCount (*classarcid);
//				Printf ("Reuse class %s (%u)\n", type->Name, m_File->Tell());
				MapObject (obj);
				obj->SerializeUserVars (*this);
				obj->Serialize (*this);
				obj->CheckIfSerialized ();
			}
			else
			{
				id[0] = OLD_OBJ;
				Write (id, 1);
				WriteCount (*objarcid);
			}
		}
	}
	return *this;
}

void AActor::Serialize(FArchive &arc)
{
	Super::Serialize(arc);

	if (arc.IsStoring())
	{
		arc.WriteSprite(sprite);
	}
	else
	{
		sprite = arc.ReadSprite();
	}

	arc << __Pos
		<< Angles.Yaw
		<< Angles.Pitch
		<< Angles.Roll
		<< frame
		<< Scale
		<< RenderStyle
		<< renderflags
		<< picnum
		<< floorpic
		<< ceilingpic
		<< TIDtoHate
		<< LastLookPlayerNumber
		<< LastLookActor
		<< effects
		<< Alpha
		<< fillcolor
		<< Sector
		<< floorz
		<< ceilingz
		<< dropoffz
		<< floorsector
		<< ceilingsector
		<< radius
		<< Height
		<< projectilepassheight
		<< Vel
		<< tics
		<< state
		<< DamageVal;
	if (DamageVal == 0x40000000 || DamageVal == -1)
	{
		DamageVal = -1;
		DamageFunc = GetDefault()->DamageFunc;
	}
	else
	{
		DamageFunc = nullptr;
	}
	P_SerializeTerrain(arc, floorterrain);
	arc	<< projectileKickback
		<< flags
		<< flags2
		<< flags3
		<< flags4
		<< flags5
		<< flags6
		<< flags7
		<< weaponspecial
		<< special1
		<< special2
		<< specialf1
		<< specialf2
		<< health
		<< movedir
		<< visdir
		<< movecount
		<< strafecount
		<< target
		<< lastenemy
		<< LastHeard
		<< reactiontime
		<< threshold
		<< player
		<< SpawnPoint
		<< SpawnAngle
		<< StartHealth
		<< skillrespawncount
		<< tracer
		<< Floorclip
		<< tid
		<< special;
	if (P_IsACSSpecial(special))
	{
		P_SerializeACSScriptNumber(arc, args[0], false);
	}
	else
	{
		arc << args[0];
	}
	arc << args[1] << args[2] << args[3] << args[4];
	arc << accuracy << stamina;
	arc << goal
		<< waterlevel
		<< MinMissileChance
		<< SpawnFlags
		<< Inventory
		<< InventoryID;
	arc << FloatBobPhase
		<< Translation
		<< SeeSound
		<< AttackSound
		<< PainSound
		<< DeathSound
		<< ActiveSound
		<< UseSound
		<< BounceSound
		<< WallBounceSound
		<< CrushPainSound
		<< Speed
		<< FloatSpeed
		<< Mass
		<< PainChance
		<< SpawnState
		<< SeeState
		<< MeleeState
		<< MissileState
		<< MaxDropOffHeight
		<< MaxStepHeight
		<< BounceFlags
		<< bouncefactor
		<< wallbouncefactor
		<< bouncecount
		<< maxtargetrange
		<< meleethreshold
		<< meleerange
		<< DamageType;
	arc << DamageTypeReceived;
	arc << PainType
		<< DeathType;
	arc	<< Gravity
		<< FastChaseStrafeCount
		<< master
		<< smokecounter
		<< BlockingMobj
		<< BlockingLine
		<< VisibleToTeam // [BB]
		<< pushfactor
		<< Species
		<< Score;
	arc << DesignatedTeam;
	arc << lastpush << lastbump
		<< PainThreshold
		<< DamageFactor;
	arc << DamageMultiply;
	arc << WeaveIndexXY << WeaveIndexZ
		<< PoisonDamageReceived << PoisonDurationReceived << PoisonPeriodReceived << Poisoner
		<< PoisonDamage << PoisonDuration << PoisonPeriod;
	arc << PoisonDamageType << PoisonDamageTypeReceived;
	arc << ConversationRoot << Conversation;
	arc << FriendPlayer;
	arc << TeleFogSourceType
		<< TeleFogDestType;
	arc << RipperLevel
		<< RipLevelMin
		<< RipLevelMax;
	arc << DefThreshold;
	if (SaveVersion >= 4549)
	{
		arc << SpriteAngle;
		arc << SpriteRotation;
	}

	if (SaveVersion >= 4550)
	{
		arc << alternative;
	}

	{
		FString tagstr;
		if (arc.IsStoring() && Tag != NULL && Tag->Len() > 0) tagstr = *Tag;
		arc << tagstr;
		if (arc.IsLoading())
		{
			if (tagstr.Len() == 0) Tag = NULL;
			else Tag = mStringPropertyData.Alloc(tagstr);
		}
	}

	if (arc.IsLoading ())
	{
		touching_sectorlist = NULL;
		LinkToWorld(false, Sector);

		AddToHash ();
		SetShade (fillcolor);
		if (player)
		{
			if (playeringame[player - players] && 
				player->cls != NULL &&
				!(flags4 & MF4_NOSKIN) &&
				state->sprite == GetDefaultByType (player->cls)->SpawnState->sprite)
			{ // Give player back the skin
				sprite = skins[player->userinfo.GetSkin()].sprite;
			}
			if (Speed == 0)
			{
				Speed = GetDefault()->Speed;
			}
		}
		ClearInterpolation();
		UpdateWaterLevel(false);
	}
}

FArchive &operator<< (FArchive &arc, sector_t *&sec)
{
	return arc.SerializePointer (sectors, (BYTE **)&sec, sizeof(*sectors));
}

FArchive &operator<< (FArchive &arc, const sector_t *&sec)
{
	return arc.SerializePointer (sectors, (BYTE **)&sec, sizeof(*sectors));
}

FArchive &operator<< (FArchive &arc, line_t *&line)
{
	return arc.SerializePointer (lines, (BYTE **)&line, sizeof(*lines));
}

FArchive &operator<< (FArchive &arc, vertex_t *&vert)
{
	return arc.SerializePointer (vertexes, (BYTE **)&vert, sizeof(*vertexes));
}

FArchive &operator<< (FArchive &arc, side_t *&side)
{
	return arc.SerializePointer (sides, (BYTE **)&side, sizeof(*sides));
}

FArchive &operator<<(FArchive &arc, DAngle &ang)
{
	arc << ang.Degrees;
	return arc;
}

FArchive &operator<<(FArchive &arc, DVector3 &vec)
{
	arc << vec.X << vec.Y << vec.Z;
	return arc;
}

FArchive &operator<<(FArchive &arc, DVector2 &vec)
{
	arc << vec.X << vec.Y;
	return arc;
}

//==========================================================================
//
//
//==========================================================================

FArchive &operator<< (FArchive &arc, FState *&state)
{
	PClassActor *info;

	if (arc.IsStoring ())
	{
		if (state == NULL)
		{
			arc.UserWriteClass (RUNTIME_CLASS(AActor));
			arc.WriteCount (NULL_STATE_INDEX);
			return arc;
		}

		info = FState::StaticFindStateOwner (state);

		if (info != NULL)
		{
			arc.UserWriteClass (info);
			arc.WriteCount ((DWORD)(state - info->OwnedStates));
		}
		else
		{
			/* this was never working as intended.
			I_Error ("Cannot find owner for state %p:\n"
					 "%s %c%c %3d [%p] -> %p", state,
					 sprites[state->sprite].name,
					 state->GetFrame() + 'A',
					 state->GetFullbright() ? '*' : ' ',
					 state->GetTics(),
					 state->GetAction(),
					 state->GetNextState());
			*/
		}
	}
	else
	{
		PClassActor *info;
		DWORD ofs;

		arc.UserReadClass<PClassActor>(info);
		ofs = arc.ReadCount ();
		if (ofs == NULL_STATE_INDEX && info == RUNTIME_CLASS(AActor))
		{
			state = NULL;
		}
		else
		{
			state = info->OwnedStates + ofs;
		}
	}
	return arc;
}

void DObject::Serialize (FArchive &arc)
{
	ObjectFlags |= OF_SerialSuccess;
}
