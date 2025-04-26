/*
** p_saveg.cpp
** Code for serializing the world state in an archive
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
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


#include "p_local.h"
#include "p_spec.h"

// State.
#include "d_player.h"
#include "p_saveg.h"
#include "s_sndseq.h"
#include "a_sharedglobal.h"
#include "r_data/r_interpolate.h"
#include "po_man.h"
#include "p_setup.h"
#include "p_lnspec.h"
#include "p_acs.h"
#include "p_terrain.h"
#include "am_map.h"
#include "sbar.h"
#include "r_utility.h"
#include "r_sky.h"
#include "serializer_doom.h"
#include "serialize_obj.h"
#include "g_levellocals.h"
#include "events.h"
#include "p_destructible.h"
#include "r_sky.h"
#include "version.h"
#include "fragglescript/t_script.h"
#include "s_music.h"
#include "model.h"
#include "d_net.h"

EXTERN_CVAR(Bool, save_formatted)

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
			("flags2", line.flags2, def->flags2)
			("activation", line.activation, def->activation)
			("special", line.special, def->special)
			("alpha", line.alpha, def->alpha)
			("portalindex", line.portalindex, def->portalindex)
			("locknumber", line.locknumber, def->locknumber)
			("health", line.health, def->health);
			// Unless the map loader is changed the sidedef references will not change between map loads so there's no need to save them.
			//.Array("sides", line.sidedef, 2)

		SerializeArgs(arc, "args", line.args, def->args, line.special);
		arc.EndObject();
	}
	return arc;

}

//==========================================================================
//
//
//
//==========================================================================

FSerializer& Serialize(FSerializer& arc, const char* key, TextureManipulation& part, TextureManipulation *def)
{
	if (arc.canSkip() && def != nullptr && !memcmp(&part, def, sizeof(part)))
	{
		return arc;
	}

	if (arc.BeginObject(key))
	{
		arc("addcolor", part.AddColor, def->AddColor)
			("yoffset", part.ModulateColor, def->ModulateColor)
			("xscale", part.BlendColor, def->BlendColor)
			("yscale", part.DesaturationFactor, def->DesaturationFactor)
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
			("flags", part.flags, def->flags)
			("skew", part.skew, def->skew)
			("color1", part.SpecialColors[0], def->SpecialColors[0])
			("color2", part.SpecialColors[1], def->SpecialColors[1])
			("addcolor", part.AdditiveColor, def->AdditiveColor)
			("texturefx", part.TextureFx, def->TextureFx)
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
			// These also remain identical across map loads
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

//============================================================================
//
// Save a sector portal for savegames.
//
//============================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FColormap &port, FColormap *def)
{
	if (arc.canSkip() && def != nullptr && port == *def)
	{
		return arc;
	}

	if (arc.BeginObject(key))
	{
		arc("lightcolor", port.LightColor)
			("fadecolor", port.FadeColor)
			("desaturation", port.Desaturation)
			("fogdensity", port.FogDensity)
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
			("glowcolor", p.GlowColor, def->GlowColor)
			("glowheight", p.GlowHeight, def->GlowHeight)
			("texturefx", p.TextureFx, def->TextureFx)
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
		Serialize(arc, "normal", p.normal, def ? &def->normal : nullptr);
		Serialize(arc, "d", p.D, def ? &def->D : nullptr);
		arc.EndObject();

		if (arc.isReading() && p.normal.Z != 0)
		{
			p.negiC = -1 / p.normal.Z;
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
			("seqtype", p.seqType, def->seqType)
			("seqname", p.SeqName, def->SeqName)
			("friction", p.friction, def->friction)
			("movefactor", p.movefactor, def->movefactor)
			("stairlock", p.stairlock, def->stairlock)
			("prevsec", p.prevsec, def->prevsec)
			("nextsec", p.nextsec, def->nextsec)
			.Array("planes", p.planes, def->planes, 2, true)
			// These cannot change during play.
			//("heightsec", p.heightsec)
			//("bottommap", p.bottommap)
			//("midmap", p.midmap)
			//("topmap", p.topmap)
			//("selfmap", p.selfmap) // todo: if this becomes changeable we need a colormap serializer.
			("damageamount", p.damageamount, def->damageamount)
			("damageinterval", p.damageinterval, def->damageinterval)
			("leakydamage", p.leakydamage, def->leakydamage)
			("damagetype", p.damagetype, def->damagetype)
			("sky", p.skytransfer, def->skytransfer)
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
			("colormap", p.Colormap, def->Colormap)
			.Array("specialcolors", p.SpecialColors, def->SpecialColors, 5, true)
			.Array("additivecolors", p.AdditiveColors, def->AdditiveColors, 5, true)
			("gravity", p.gravity, def->gravity)
			("healthfloor", p.healthfloor, def->healthfloor)
			("healthceiling", p.healthceiling, def->healthceiling)
			("health3d", p.health3d, def->health3d)
			// GZDoom exclusive:
			.Array("reflect", p.reflect, def->reflect, 2, true);

		SerializeTerrain(arc, "floorterrain", p.terrainnum[0], &def->terrainnum[0]);
		SerializeTerrain(arc, "ceilingterrain", p.terrainnum[1], &def->terrainnum[1]);
		arc.EndObject();
	}
	return arc;
}

//==========================================================================
//
// RecalculateDrawnSubsectors
//
// In case the subsector data is unusable this function tries to reconstruct
// if from the linedefs' ML_MAPPED info.
//
//==========================================================================

void FLevelLocals::RecalculateDrawnSubsectors()
{
	for (auto &sub : subsectors)
	{
		for (unsigned int j = 0; j<sub.numlines; j++)
		{
			if (sub.firstline[j].linedef != NULL &&
				(sub.firstline[j].linedef->flags & ML_MAPPED))
			{
				sub.flags |= SSECMF_DRAWN;
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

FSerializer &FLevelLocals::SerializeSubsectors(FSerializer &arc, const char *key)
{
	uint8_t by;
	const char *str;

	auto numsubsectors = subsectors.Size();
	if (arc.isWriting())
	{
		TArray<char> encoded(1 + (numsubsectors + 5) / 6, true);
		int p = 0;
		for (unsigned i = 0; i < numsubsectors; i += 6)
		{
			by = 0;
			for (unsigned j = 0; j < 6; j++)
			{
				if (i + j < numsubsectors && (subsectors[i + j].flags & SSECMF_DRAWN))
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
			auto numvertexes = vertexes.Size();
			arc(nullptr, numvertexes)
				(nullptr, numsubsectors)
				.StringPtr(nullptr, str)
				.EndArray();
		}
	}
	else
	{
		int num_verts, num_subs;
		bool success = false;
		if (arc.BeginArray(key))
		{
			arc(nullptr, num_verts)
				(nullptr, num_subs)
				.StringPtr(nullptr, str)
				.EndArray();

			if (num_verts == (int)vertexes.Size() && num_subs == (int)numsubsectors)
			{
				success = true;
				int sub = 0;
				for (int i = 0; str[i] != 0; i++)
				{
					by = str[i];
					if (by >= '0' && by <= '9') by -= '0';
					else if (by >= 'A' && by <= 'Z') by -= 'A' - 10;
					else if (by >= 'a' && by <= 'z') by -= 'a' - 36;
					else if (by == '-') by = 62;
					else if (by == '+') by = 63;
					else
					{
						success = false;
						break;
					}
					for (int s = 0; s < 6; s++)
					{
						if (sub + s < (int)numsubsectors && (by & (1 << s)))
						{
							subsectors[sub + s].flags |= SSECMF_DRAWN;
						}
					}
					sub += 6;
				}
			}
			if (!success)
			{
				RecalculateDrawnSubsectors();
			}
		}

	}
	return arc;
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

//============================================================================
//
// one polyobject.
//
//============================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, FPolyObj &poly, FPolyObj *def)
{
	if (arc.BeginObject(key))
	{
		DAngle angle = poly.Angle;
		DVector2 delta = poly.StartSpot.pos;
		arc("angle", angle)
			("pos", delta)
			("interpolation", poly.interpolation)
			("blocked", poly.bBlocked)
			("hasportals", poly.bHasPortals)
			("specialdata", poly.specialdata)
			("level", poly.Level)
			.EndObject();

		if (arc.isReading())
		{
			if (poly.OriginalPts.Size() > 0)
			{
				poly.RotatePolyobj(angle, true);
				delta -= poly.StartSpot.pos;
				poly.MovePolyobj(delta, true);
			}
		}
	}
	return arc;
}

//==========================================================================
//
//
//
//==========================================================================

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

//==========================================================================
//
// ArchiveSounds
//
//==========================================================================

void FLevelLocals::SerializeSounds(FSerializer &arc)
{
	if (isPrimaryLevel())
	{
		S_SerializeSounds(arc);
		const char *name = NULL;
		uint8_t order;
		float musvol = MusicVolume;

		if (arc.isWriting())
		{
			order = S_GetMusic(&name);
		}
		arc.StringPtr("musicname", name)
			("musicorder", order)
			("musicvolume", musvol);

		if (arc.isReading())
		{
			if (!S_ChangeMusic(name, order))
				SetMusic();
			SetMusicVolume(musvol);
		}
	}
}

//==========================================================================
//
// P_ArchivePlayers
//
//==========================================================================

void FLevelLocals::SerializePlayers(FSerializer &arc, bool skipload)
{
	int numPlayers, numPlayersNow;
	int i;

	// Count the number of players present right now.
	for (numPlayersNow = 0, i = 0; i < MAXPLAYERS; ++i)
	{
		if (PlayerInGame(i))
		{
			++numPlayersNow;
		}
	}

	if (arc.isWriting())
	{
		// Record the number of players in this save.
		arc("numplayers", numPlayersNow);
		if (arc.BeginArray("players"))
		{
			// Record each player's name, followed by their data.
			for (i = 0; i < MAXPLAYERS; ++i)
			{
				if (PlayerInGame(i))
				{
					if (arc.BeginObject(nullptr))
					{
						FString name = Players[i]->userinfo.GetName();
						arc("playername", name);
						Players[i]->Serialize(arc);
						arc.EndObject();
					}
				}
			}
			arc.EndArray();
		}
	}
	else
	{
		arc("numplayers", numPlayers);

		if (arc.BeginArray("players"))
		{
			// If there is only one player in the game, they go to the
			// first player present, no matter what their name.
			if (numPlayers == 1)
			{
				ReadOnePlayer(arc, skipload);
			}
			else
			{
				ReadMultiplePlayers(arc, numPlayers, skipload);
			}
			arc.EndArray();

			if (!skipload)
			{
				for (unsigned int i = 0u; i < MAXPLAYERS; ++i)
				{
					if (PlayerInGame(i) && Players[i]->mo != nullptr)
						NetworkEntityManager::SetClientNetworkEntity(Players[i]->mo, i);
				}
			}
		}
		if (!skipload && numPlayersNow > numPlayers)
		{
			SpawnExtraPlayers();
		}
		// Redo pitch limits, since the spawned player has them at 0.
		auto p = GetConsolePlayer();
		if (p) p->SendPitchLimits();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FLevelLocals::ReadOnePlayer(FSerializer &arc, bool fromHub)
{
	if (!arc.BeginObject(nullptr))
		return;

	FString name = {};
	arc("playername", name);
	player_t temp = {};
	temp.Serialize(arc);

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (!PlayerInGame(i))
			continue;

		if (!fromHub)
		{
			// This temp player has undefined pitch limits, so set them to something
			// that should leave the pitch stored in the savegame intact when
			// rendering. The real pitch limits will be set by P_SerializePlayers()
			// via a net command, but that won't be processed in time for a screen
			// wipe, so we need something here.
			temp.MaxPitch = temp.MinPitch = temp.mo->Angles.Pitch;
			CopyPlayer(Players[i], &temp, name.GetChars());
		}
		else
		{
			// we need the player actor, so that G_FinishTravel can destroy it later.
			Players[i]->mo = temp.mo;
		}

		break;
	}

	arc.EndObject();
}

//==========================================================================
//
//
//
//==========================================================================

struct NetworkPlayerInfo
{
	FString Name = {};
	player_t Info = {};
	bool bUsed = false;
};

void FLevelLocals::ReadMultiplePlayers(FSerializer &arc, int numPlayers, bool fromHub)
{
	TArray<NetworkPlayerInfo> tempPlayers = {};
	tempPlayers.Reserve(numPlayers);
	TArray<bool> assignedPlayers = {};
	assignedPlayers.Reserve(MAXPLAYERS);

	// Read all the save game players into a temporary array
	for (auto& p : tempPlayers)
	{
		if (arc.BeginObject(nullptr))
		{
			arc("playername", p.Name);
			p.Info.Serialize(arc);
			arc.EndObject();
		}
	}

	// Now try to match players from the savegame with players present
	// based on their names. If two players in the savegame have the
	// same name, then they are assigned to players in the current game
	// on a first-come, first-served basis.
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (!PlayerInGame(i))
			continue;

		for (auto& p : tempPlayers)
		{
			if (!p.bUsed && !p.Name.Compare(Players[i]->userinfo.GetName()))
			{
				// Found a match, so copy our temp player to the real player
				if (!fromHub)
				{
					Printf("Found %s's (%d) data\n", Players[i]->userinfo.GetName(), i);
					CopyPlayer(Players[i], &p.Info, p.Name.GetChars());
				}
				else
				{
					Players[i]->mo = p.Info.mo;
				}

				p.bUsed = true;
				assignedPlayers[i] = true;
				break;
			}
		}
	}

	// Any players that didn't have matching names are assigned to existing
	// players on a first-come, first-served basis.
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (!PlayerInGame(i) || assignedPlayers[i])
			continue;

		for (auto& p : tempPlayers)
		{
			if (!p.bUsed)
			{
				if (!fromHub)
				{
					Printf("Assigned %s (%d) to %s's data\n", Players[i]->userinfo.GetName(), i, p.Name.GetChars());
					CopyPlayer(Players[i], &p.Info, p.Name.GetChars());
				}
				else
				{
					Players[i]->mo = p.Info.mo;
				}

				p.bUsed = true;
				break;
			}
		}
	}

	// Remove any temp players that were not used. Happens if there are now
	// less players in the game than there were in the save
	for (auto& p : tempPlayers)
	{
		if (!p.bUsed)
			p.Info.mo->Destroy();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FLevelLocals::CopyPlayer(player_t *dst, player_t *src, const char *name)
{
	// The userinfo needs to be saved for real players, but it
	// needs to come from the save for bots.
	userinfo_t uibackup;
	userinfo_t uibackup2;

	uibackup.TransferFrom(dst->userinfo);
	uibackup2.TransferFrom(src->userinfo);

	int chasecam = dst->cheats & CF_CHASECAM;	// Remember the chasecam setting
	bool attackdown = dst->attackdown;
	bool usedown = dst->usedown;

	dst->CopyFrom(*src, true);	// To avoid memory leaks at this point the userinfo in src must be empty which is taken care of by the TransferFrom call above.

	dst->cheats |= chasecam;

	if (dst->Bot != nullptr)
	{
		botinfo_t *thebot = BotInfo.botinfo;
		while (thebot && thebot->Name.CompareNoCase(name))
		{
			thebot = thebot->next;
		}
		if (thebot)
		{
			thebot->inuse = BOTINUSE_Yes;
		}
		BotInfo.botnum++;
		dst->userinfo.TransferFrom(uibackup2);
	}
	else
	{
		dst->userinfo.TransferFrom(uibackup);
		// The player class must come from the save, so that the menu reflects the currently playing one.
		dst->userinfo.PlayerClassChanged(src->mo->GetInfo()->DisplayName.GetChars());
	}

	// Validate the skin
	dst->userinfo.SkinNumChanged(R_FindSkin(Skins[dst->userinfo.GetSkin()].Name.GetChars(), dst->CurrentPlayerClass));

	// Make sure the player pawn points to the proper player struct.
	if (dst->mo != nullptr)
	{
		dst->mo->player = dst;
	}

	// Same for the psprites.
	DPSprite *pspr = dst->psprites;
	while (pspr)
	{
		pspr->Owner = dst;

		pspr = pspr->Next;
	}

	// These 2 variables may not be overwritten.
	dst->attackdown = attackdown;
	dst->usedown = usedown;
}


//==========================================================================
//
//
//
//==========================================================================

void FLevelLocals::SpawnExtraPlayers()
{
	// If there are more players now than there were in the savegame,
	// be sure to spawn the extra players.
	int i;

	if (deathmatch || !isPrimaryLevel())
	{
		return;
	}

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (PlayerInGame(i) && Players[i]->mo == NULL)
		{
			Players[i]->playerstate = PST_ENTER;
			SpawnPlayer(&playerstarts[i], i, (flags2 & LEVEL2_PRERAISEWEAPON) ? SPF_WEAPONFULLYUP : 0);
		}
	}
}

//============================================================================
//
//
//
//============================================================================

void FLevelLocals::Serialize(FSerializer &arc, bool hubload)
{
	int i = totaltime;

	if (arc.isWriting())
	{
		arc.Array("checksum", md5, 16);
	}
	else
	{
		// prevent bad things from happening by doing a check on the size of level arrays and the map's entire checksum.
		// The old code happily tried to load savegames with any mismatch here, often causing meaningless errors
		// deep down in the deserializer or just a crash if the few insufficient safeguards were not triggered.
		uint8_t chk[16] = { 0 };
		arc.Array("checksum", chk, 16);
		if (arc.GetSize("linedefs") != lines.Size() ||
			arc.GetSize("sidedefs") != sides.Size() ||
			arc.GetSize("sectors") != sectors.Size() ||
			arc.GetSize("polyobjs") != Polyobjects.Size() ||
			memcmp(chk, md5, 16))
		{
			I_Error("Savegame is from a different level");
		}
	}
	arc("saveversion", SaveVersion);

	// this sets up some static data needed further down which means it must be done first.
	StaticSerializeTranslations(arc);

	if (arc.isReading())
	{
		Thinkers.DestroyAllThinkers();
		ClientsideThinkers.DestroyAllThinkers();
		interpolator.ClearInterpolations();
		arc.ReadObjects(hubload);
		// If there have been object deserialization errors we must absolutely not continue here because scripted objects can do unpredictable things.
		if (arc.mObjectErrors) I_Error("Failed to load savegame");
	}

	arc("multiplayer", multiplayer);

	arc("flags", flags)
		("flags2", flags2)
		("flags3", flags3)
		("fadeto", fadeto)
		("skyspeed1", skyspeed1)
		("skyspeed2", skyspeed2)
		("skymistspeed", skymistspeed)
		("found_secrets", found_secrets)
		("found_items", found_items)
		("killed_monsters", killed_monsters)
		("total_secrets", total_secrets)
		("total_items", total_items)
		("total_monsters", total_monsters)
		("gravity", gravity)
		("aircontrol", aircontrol)
		("teamdamage", teamdamage)
		("maptime", maptime)
		("totaltime", i)
		("skytexture1", skytexture1)
		("skytexture2", skytexture2)
		("skymisttexture", skymisttexture)
		("fogdensity", fogdensity)
		("outsidefogdensity", outsidefogdensity)
		("skyfog", skyfog)
		("deathsequence", deathsequence)
		("bodyqueslot", bodyqueslot)
		("spawnindex", spawnindex)
		.Array("bodyque", bodyque, BODYQUESIZE)
		("corpsequeue", CorpseQueue)
		("spotstate", SpotState)
		("fragglethinker", FraggleScriptThinker)
		("acsthinker", ACSThinker)
		("scrolls", Scrolls)
		("automap", automap)
		("interpolator", interpolator)
		("frozenstate", frozenstate)
		("visualthinkerhead", VisualThinkerHead)
		("actorbehaviors", ActorBehaviors);


	// Hub transitions must keep the current total time
	if (!hubload)
		totaltime = i;

	if (arc.isReading())
	{
		InitSkyMap(this);
		AirControlChanged();
	}

	Behaviors.SerializeModuleStates(arc);
	// The order here is important: First world state, then portal state, then thinkers, and last polyobjects.
	SetCompatLineOnSide(false);	// This flag should not be saved. It solely depends on current compatibility state.
	arc("linedefs", lines, loadlines);
	SetCompatLineOnSide(true);
	arc("sidedefs", sides, loadsides);
	arc("sectors", sectors, loadsectors);
	arc("zones", Zones);
	arc("lineportals", linePortals);
	arc("sectorportals", sectorPortals);
	if (arc.isReading())
	{
		FinalizePortals();
	}

	// [ZZ] serialize health groups
	P_SerializeHealthGroups(this, arc);
	// [ZZ] serialize events
	arc("firstevent", localEventManager->FirstEventHandler)
		("lastevent", localEventManager->LastEventHandler);
	if (arc.isReading()) localEventManager->CallOnRegister();
	Thinkers.SerializeThinkers(arc, hubload);
	arc("polyobjs", Polyobjects);
	SerializeSubsectors(arc, "subsectors");
	StatusBar->SerializeMessages(arc);
	canvasTextureInfo.Serialize(arc);
	SerializePlayers(arc, hubload);
	SerializeSounds(arc);
	arc("sndseqlisthead", SequenceListHead);


	// Regenerate some data that wasn't saved
	if (arc.isReading())
	{
		for (auto &sec : sectors)
		{
			P_Recalculate3DFloors(&sec);
		}
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (PlayerInGame(i) && Players[i]->mo != nullptr)
			{
				FWeaponSlots::SetupWeaponSlots(Players[i]->mo);
			}
		}
		localEventManager->SetOwnerForHandlers();	// This cannot be automated.
		RecreateAllAttachedLights();
		InitPortalGroups(this);

		auto it = GetThinkerIterator<DImpactDecal>(NAME_None, STAT_AUTODECAL);
		ImpactDecalCount = 0;
		while (it.Next()) ImpactDecalCount++;

		automap->UpdateShowAllLines();

	}
	// clean up the static data we allocated
	StaticClearSerializeTranslationsData();

}

//==========================================================================
//
// Archives the current level
//
//==========================================================================

void FLevelLocals::SnapshotLevel()
{
	info->Snapshot.Clean();

	if (info->isValid())
	{
		FDoomSerializer arc(this);

		if (arc.OpenWriter(save_formatted))
		{
			SaveVersion = SAVEVER;
			Serialize(arc, false);
			info->Snapshot = arc.GetCompressedOutput();
		}
	}
}

//==========================================================================
//
// Unarchives the current level based on its snapshot
// The level should have already been loaded and setup.
//
//==========================================================================

void FLevelLocals::UnSnapshotLevel(bool hubLoad)
{
	if (info->Snapshot.mBuffer == nullptr)
		return;

	if (info->isValid())
	{
		FDoomSerializer arc(this);
		if (!arc.OpenReader(&info->Snapshot))
		{
			I_Error("Failed to load savegame");
			return;
		}

		Serialize(arc, hubLoad);
		FromSnapshot = true;

		auto it = GetThinkerIterator<AActor>(NAME_PlayerPawn);
		AActor *pawn, *next;

		next = it.Next();
		while ((pawn = next) != 0)
		{
			next = it.Next();
			if (pawn->player == nullptr || pawn->player->mo == nullptr || !PlayerInGame(pawn->player))
			{
				int i;

				// If this isn't the unmorphed original copy of a player, destroy it, because it's extra.
				for (i = 0; i < MAXPLAYERS; ++i)
				{
					if (PlayerInGame(i) && Players[i]->mo->alternative == pawn)
					{
						break;
					}
				}
				if (i == MAXPLAYERS)
				{
					pawn->Destroy();
				}
			}
		}
		arc.Close();
	}
	// No reason to keep the snapshot around once the level's been entered.
	info->Snapshot.Clean();
	if (hubLoad)
	{
		// Unlock ACS global strings that were locked when the snapshot was made.
		Behaviors.UnlockLevelVarStrings(levelnum);
	}
}

