#include "serializer.h"
#include "r_local.h"
#include "p_setup.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "a_sharedglobal.h"

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, line_t &line, line_t *def)
{
	if (arc.BeginObject(key))
	{
		arc("flags", line.flags, def->flags)
			("activation", line.activation, def->activation)
			("special", line.special, def->special)
			("alpha", line.alpha, def->alpha)
			.Args("args", line.args, def->args, line.special)
			("portalindex", line.portalindex, def->portalindex)
			// no need to store the sidedef references. Unless the map loader is changed they will not change between map loads.
			//.Array("sides", line.sidedef, 2)
			.EndObject();
	}
	return arc;

}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, side_t::part &part, side_t::part *def)
{
	if (arc.canSkip() && def != nullptr && !memcmp(&part, def, sizeof(part)))
	{
		return arc;
	}

	if (arc.BeginObject(key))
	{
		arc("xoffset", part.xOffset, def->xOffset)
			("yoffset", part.yOffset, def->yOffset)
			("xscale", part.xScale, def->xScale)
			("yscale", part.yScale, def->yScale)
			("texture", part.texture, def->texture)
			("interpolation", part.interpolation)
			.EndObject();
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, side_t &side, side_t *def)
{
	if (arc.BeginObject(key))
	{
		arc.Array("textures", side.textures, def->textures, 3, true)
			("light", side.Light, def->Light)
			("flags", side.Flags, def->Flags)
			//("leftside", side.LeftSide)
			//("rightside", side.RightSide)
			//("index", side.Index)
			("attacheddecals", side.AttachedDecals)
			.EndObject();
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FLinkedSector &ls, FLinkedSector *def)
{
	if (arc.BeginObject(key))
	{
		arc("sector", ls.Sector)
			("type", ls.Type)
			.EndObject();
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, sector_t::splane &p, sector_t::splane *def)
{
	if (arc.canSkip() && def != nullptr && !memcmp(&p, def, sizeof(p)))
	{
		return arc;
	}

	if (arc.BeginObject(key))
	{
		arc("xoffs", p.xform.xOffs, def->xform.xOffs)
			("yoffs", p.xform.yOffs, def->xform.yOffs)
			("xscale", p.xform.xScale, def->xform.xScale)
			("yscale", p.xform.yScale, def->xform.yScale)
			("angle", p.xform.Angle, def->xform.Angle)
			("baseyoffs", p.xform.baseyOffs, def->xform.baseyOffs)
			("baseangle", p.xform.baseAngle, def->xform.baseAngle)
			("flags", p.Flags, def->Flags)
			("light", p.Light, def->Light)
			("texture", p.Texture, def->Texture)
			("texz", p.TexZ, def->TexZ)
			("alpha", p.alpha, def->alpha)
			.EndObject();
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, secplane_t &p, secplane_t *def)
{
	if (arc.canSkip() && def != nullptr && !memcmp(&p, def, sizeof(p)))
	{
		return arc;
	}

	if (arc.BeginObject(key))
	{
		arc("normal", p.normal, def->normal)
			("d", p.D, def->D)
			.EndObject();

		if (arc.isReading() && p.normal.Z != 0)
		{
			p.negiC = 1 / p.normal.Z;
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, sector_t &p, sector_t *def)
{
	if (arc.BeginObject(key))
	{
		arc("floorplane", p.floorplane, def->floorplane)
			("ceilingplane", p.ceilingplane, def->ceilingplane)
			("lightlevel", p.lightlevel, def->lightlevel)
			("special", p.special, def->special)
			("soundtraversed", p.soundtraversed, def->soundtraversed)
			("seqtype", p.seqType, def->seqType)
			("seqname", p.SeqName, def->SeqName)
			("friction", p.friction, def->friction)
			("movefactor", p.movefactor, def->movefactor)
			("stairlock", p.stairlock, def->stairlock)
			("prevsec", p.prevsec, def->prevsec)
			("nextsec", p.nextsec, def->nextsec)
			.Array("planes", p.planes, def->planes, 2, true)
			//("heightsec", p.heightsec)
			//("bottommap", p.bottommap)
			//("midmap", p.midmap)
			//("topmap", p.topmap)
			("damageamount", p.damageamount, def->damageamount)
			("damageinterval", p.damageinterval, def->damageinterval)
			("leakydamage", p.leakydamage, def->leakydamage)
			("damagetype", p.damagetype, def->damagetype)
			("sky", p.sky, def->sky)
			("moreflags", p.MoreFlags, def->MoreFlags)
			("flags", p.Flags, def->Flags)
			.Array("portals", p.Portals, def->Portals, 2, true)
			("zonenumber", p.ZoneNumber, def->ZoneNumber)
			.Array("interpolations", p.interpolations, 4, true)
			("soundtarget", p.SoundTarget)
			("secacttarget", p.SecActTarget)
			("floordata", p.floordata)
			("ceilingdata", p.ceilingdata)
			("lightingdata", p.lightingdata)
			("fakefloor_sectors", p.e->FakeFloor.Sectors)
			("midtexf_lines", p.e->Midtex.Floor.AttachedLines)
			("midtexf_sectors", p.e->Midtex.Floor.AttachedSectors)
			("midtexc_lines", p.e->Midtex.Ceiling.AttachedLines)
			("midtexc_sectors", p.e->Midtex.Ceiling.AttachedSectors)
			("linked_floor", p.e->Linked.Floor.Sectors)
			("linked_ceiling", p.e->Linked.Ceiling.Sectors)
			("colormap", p.ColorMap, def->ColorMap)
			.Terrain("floorterrain", p.terrainnum[0], &def->terrainnum[0])
			.Terrain("ceilingterrain", p.terrainnum[1], &def->terrainnum[1])
			.EndObject();
	}
	return arc;
}


FSerializer &Serialize(FSerializer &arc, const char *key, subsector_t *&ss, subsector_t **)
{
	BYTE by;
	const char *str;

	if (arc.isWriting())
	{
		if (hasglnodes)
		{
			TArray<char> encoded((numsubsectors + 5) / 6);
			int p = 0;
			for (int i = 0; i < numsubsectors; i += 6)
			{
				by = 0;
				for (int j = 0; j < 6; j++)
				{
					if (i + j < numsubsectors && (subsectors[i + j].flags & SSECF_DRAWN))
					{
						by |= (1 << j);
					}
				}
				if (by < 10) by += '0';
				else if (by < 36) by += 'A' - 10;
				else if (by < 62) by += 'a' - 36;
				else if (by == 62) by = '-';
				else if (by == 63) by = '+';
				encoded[p++] = by;
			}
			encoded[p] = 0;
			str = &encoded[0];
			if (arc.BeginArray(key))
			{
				arc(nullptr, numvertexes)
					(nullptr, numsubsectors)
					.StringPtr(nullptr, str)
					.EndArray();
			}
		}
	}
	else
	{
		int num_verts, num_subs;

		if (arc.BeginArray(key))
		{
			arc(nullptr, num_verts)
				(nullptr, num_subs)
				.StringPtr(nullptr, str)
				.EndArray();
		}

	}
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, ReverbContainer *&c, ReverbContainer **def)
{
	int id = (arc.isReading() || c == nullptr) ? 0 : c->ID;
	Serialize(arc, key, id, nullptr);
	if (arc.isReading())
	{
		c = S_FindEnvironment(id);
	}
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, zone_t &z, zone_t *def)
{
	return Serialize(arc, key, z.Environment, nullptr);
}

//============================================================================
//
// Save a line portal for savegames.
//
//============================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FLinePortal &port, FLinePortal *def)
{
	if (arc.BeginObject(key))
	{
		arc("origin", port.mOrigin)
			("destination", port.mDestination)
			("displacement", port.mDisplacement)
			("type", port.mType)
			("flags", port.mFlags)
			("defflags", port.mDefFlags)
			("align", port.mAlign)
			.EndObject();
	}
	return arc;
}

//============================================================================
//
// Save a sector portal for savegames.
//
//============================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FSectorPortal &port, FSectorPortal *def)
{
	if (arc.BeginObject(key))
	{
		arc("type", port.mType)
			("flags", port.mFlags)
			("partner", port.mPartner)
			("plane", port.mPlane)
			("origin", port.mOrigin)
			("destination", port.mDestination)
			("displacement", port.mDisplacement)
			("planez", port.mPlaneZ)
			("skybox", port.mSkybox)
			.EndObject();
	}
	return arc;
}


void DThinker::SaveList(FSerializer &arc, DThinker *node)
{
	if (node != NULL)
	{
		while (!(node->ObjectFlags & OF_Sentinel))
		{
			assert(node->NextThinker != NULL && !(node->NextThinker->ObjectFlags & OF_EuthanizeMe));
			::Serialize<DThinker>(arc, nullptr, node, nullptr);
			node = node->NextThinker;
		}
	}
}

void DThinker::SerializeThinkers(FSerializer &arc, bool hubLoad)
{
	//DThinker *thinker;
	//BYTE stat;
	//int statcount;
	int i;

	if (arc.isWriting())
	{
		arc.BeginArray("thinkers");
		for (i = 0; i <= MAX_STATNUM; i++)
		{
			arc.BeginArray(nullptr);
			SaveList(arc, Thinkers[i].GetHead());
			SaveList(arc, FreshThinkers[i].GetHead());
			arc.EndArray();
		}
		arc.EndArray();
	}
	else
	{
	}
}



void SerializeWorld(FSerializer &arc)
{
	arc.Array("linedefs", lines, &loadlines[0], numlines)
		.Array("sidedefs", sides, &loadsides[0], numsides)
		.Array("sectors", sectors, &loadsectors[0], numsectors)
		("subsectors", subsectors)
		("zones", Zones)
		("lineportals", linePortals)
		("sectorportals", sectorPortals);
}

void DObject::SerializeUserVars(FSerializer &arc)
{
	PSymbolTable *symt;
	FName varname;
	//DWORD count, j;
	int *varloc = NULL;

	symt = &GetClass()->Symbols;

	if (arc.isWriting())
	{
		// Write all fields that aren't serialized by native code.
		//GetClass()->WriteValue(arc, this);
	}
	else 
	{
		//GetClass()->ReadValue(arc, this);
	}
}

void DObject::Serialize(FSerializer &arc)
{
	ObjectFlags |= OF_SerialSuccess;
}

//==========================================================================
//
// AActor :: Serialize
//
//==========================================================================

#define A(a,b) ((a), (b), def->b)

void AActor::Serialize(FSerializer &arc)
{
	int damage = 0;	// just a placeholder until the insanity surrounding the damage property can be fixed
	AActor *def = GetDefault();

	Super::Serialize(arc);

	arc
		.Sprite("sprite", sprite, &def->sprite)
		A("pos", __Pos)
		A("angles", Angles)
		A("frame", frame)
		A("scale", Scale)
		A("renderstyle", RenderStyle)
		A("renderflags", renderflags)
		A("picnum", picnum)
		A("floorpic", floorpic)
		A("ceilingpic", ceilingpic)
		A("tidtohate", TIDtoHate)
		A("lastlookpn", LastLookPlayerNumber)
		("lastlookactor", LastLookActor)
		A("effects", effects)
		A("alpha", Alpha)
		A("fillcolor", fillcolor)
		A("sector", Sector)
		A("floorz", floorz)
		A("ceilingz", ceilingz)
		A("dropoffz", dropoffz)
		A("floorsector", floorsector)
		A("ceilingsector", ceilingsector)
		A("radius", radius)
		A("height", Height)
		A("ppassheight", projectilepassheight)
		A("vel", Vel)
		A("tics", tics)
		A("state", state)
		("damage", damage)
		.Terrain("floorterrain", floorterrain, &def->floorterrain)
		A("projectilekickback", projectileKickback)
		A("flags", flags)
		A("flags2", flags2)
		A("flags3", flags3)
		A("flags4", flags4)
		A("flags5", flags5)
		A("flags6", flags6)
		A("flags7", flags7)
		A("weaponspecial", weaponspecial)
		A("special1", special1)
		A("special2", special2)
		A("specialf1", specialf1)
		A("specialf2", specialf2)
		A("health", health)
		A("movedir", movedir)
		A("visdir", visdir)
		A("movecount", movecount)
		A("strafecount", strafecount)
		("target", target)
		("lastenemy", lastenemy)
		("lastheard", LastHeard)
		A("reactiontime", reactiontime)
		A("threshold", threshold)
		A("player", player)
		A("spawnpoint", SpawnPoint)
		A("spawnangle", SpawnAngle)
		A("starthealth", StartHealth)
		A("skillrespawncount", skillrespawncount)
		("tracer", tracer)
		A("floorclip", Floorclip)
		A("tid", tid)
		A("special", special)
		.Args("args", args, def->args, special)
		A("accuracy", accuracy)
		A("stamina", stamina)
		("goal", goal)
		A("waterlevel", waterlevel)
		A("minmissilechance", MinMissileChance)
		A("spawnflags", SpawnFlags)
		("inventory", Inventory)
		A("inventoryid", InventoryID)
		A("floatbobphase", FloatBobPhase)
		A("translation", Translation)
		A("seesound", SeeSound)
		A("attacksound", AttackSound)
		A("paimsound", PainSound)
		A("deathsound", DeathSound)
		A("activesound", ActiveSound)
		A("usesound", UseSound)
		A("bouncesound", BounceSound)
		A("wallbouncesound", WallBounceSound)
		A("crushpainsound", CrushPainSound)
		A("speed", Speed)
		A("floatspeed", FloatSpeed)
		A("mass", Mass)
		A("painchance", PainChance)
		A("spawnstate", SpawnState)
		A("seestate", SeeState)
		A("meleestate", MeleeState)
		A("missilestate", MissileState)
		A("maxdropoffheight", MaxDropOffHeight)
		A("maxstepheight", MaxStepHeight)
		A("bounceflags", BounceFlags)
		A("bouncefactor", bouncefactor)
		A("wallbouncefactor", wallbouncefactor)
		A("bouncecount", bouncecount)
		A("maxtargetrange", maxtargetrange)
		A("meleethreshold", meleethreshold)
		A("meleerange", meleerange)
		A("damagetype", DamageType)
		A("damagetypereceived", DamageTypeReceived)
		A("paintype", PainType)
		A("deathtype", DeathType)
		A("gravity", Gravity)
		A("fastchasestrafecount", FastChaseStrafeCount)
		("master", master)
		A("smokecounter", smokecounter)
		("blockingmobj", BlockingMobj)
		A("blockingline", BlockingLine)
		A("visibletoteam", VisibleToTeam)
		A("pushfactor", pushfactor)
		A("species", Species)
		A("score", Score)
		A("designatedteam", DesignatedTeam)
		A("lastpush", lastpush)
		A("lastbump", lastbump)
		A("painthreshold", PainThreshold)
		A("damagefactor", DamageFactor)
		A("damagemultiply", DamageMultiply)
		A("waveindexxy", WeaveIndexXY)
		A("weaveindexz", WeaveIndexZ)
		A("pdmgreceived", PoisonDamageReceived)
		A("pdurreceived", PoisonDurationReceived)
		A("ppreceived", PoisonPeriodReceived)
		("poisoner", Poisoner)
		A("posiondamage", PoisonDamage)
		A("poisonduration", PoisonDuration)
		A("poisonperiod", PoisonPeriod)
		A("poisondamagetype", PoisonDamageType)
		A("poisondmgtypereceived", PoisonDamageTypeReceived)
		A("conversationroot", ConversationRoot)
		A("conversation", Conversation)
		A("friendplayer", FriendPlayer)
		A("telefogsourcetype", TeleFogSourceType)
		A("telefogdesttype", TeleFogDestType)
		A("ripperlevel", RipperLevel)
		A("riplevelmin", RipLevelMin)
		A("riplevelmax", RipLevelMax)
		A("devthreshold", DefThreshold)
		A("spriteangle", SpriteAngle)
		A("spriterotation", SpriteRotation)
		("alternative", alternative)
		A("tag", Tag);


}


CCMD(writejson)
{
	DWORD t = I_MSTime();
	FSerializer arc;
	arc.OpenWriter();
	arc.BeginObject(nullptr);
	DThinker::SerializeThinkers(arc, false);
	SerializeWorld(arc);
	arc.WriteObjects();
	arc.EndObject();
	DWORD tt = I_MSTime();
	Printf("JSON generation took %d ms\n", tt - t);
	FILE *f = fopen("out.json", "wb");
	unsigned siz;
	const char *str = arc.GetOutput(&siz);
	fwrite(str, 1, siz, f);
	fclose(f);
	/*
	DWORD ttt = I_MSTime();
	Printf("JSON save took %d ms\n", ttt - tt);
	FDocument doc(arc.w->mOutString.GetString(), arc.w->mOutString.GetSize());
	DWORD tttt = I_MSTime();
	Printf("JSON parse took %d ms\n", tttt - ttt);
	*/
}
