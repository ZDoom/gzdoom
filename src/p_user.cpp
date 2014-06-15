// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Player related stuff.
//		Bobbing POV/weapon, movement.
//		Pending weapon.
//
//-----------------------------------------------------------------------------

#include "templates.h"
#include "doomdef.h"
#include "d_event.h"
#include "p_local.h"
#include "doomstat.h"
#include "s_sound.h"
#include "i_system.h"
#include "gi.h"
#include "m_random.h"
#include "p_pspr.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "a_sharedglobal.h"
#include "a_keys.h"
#include "statnums.h"
#include "v_palette.h"
#include "v_video.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sbar.h"
#include "intermission/intermission.h"
#include "c_console.h"
#include "doomdef.h"
#include "c_dispatch.h"
#include "tarray.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "d_net.h"
#include "gstrings.h"
#include "farchive.h"
#include "r_renderer.h"

static FRandom pr_skullpop ("SkullPop");

// [RH] # of ticks to complete a turn180
#define TURN180_TICKS	((TICRATE / 4) + 1)

// Variables for prediction
CVAR (Bool, cl_noprediction, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
static player_t PredictionPlayerBackup;
static BYTE PredictionActorBackup[sizeof(AActor)];
static TArray<sector_t *> PredictionTouchingSectorsBackup;
static TArray<AActor *> PredictionSectorListBackup;
static TArray<msecnode_t *> PredictionSector_sprev_Backup;

// [GRB] Custom player classes
TArray<FPlayerClass> PlayerClasses;

FPlayerClass::FPlayerClass ()
{
	Type = NULL;
	Flags = 0;
}

FPlayerClass::FPlayerClass (const FPlayerClass &other)
{
	Type = other.Type;
	Flags = other.Flags;
	Skins = other.Skins;
}

FPlayerClass::~FPlayerClass ()
{
}

bool FPlayerClass::CheckSkin (int skin)
{
	for (unsigned int i = 0; i < Skins.Size (); i++)
	{
		if (Skins[i] == skin)
			return true;
	}

	return false;
}

//===========================================================================
//
// GetDisplayName
//
//===========================================================================

const char *GetPrintableDisplayName(const PClass *cls)
{ 
	// Fixme; This needs a decent way to access the string table without creating a mess.
	const char *name = cls->Meta.GetMetaString(APMETA_DisplayName);
	return name;
}

bool ValidatePlayerClass(const PClass *ti, const char *name)
{
	if (!ti)
	{
		Printf ("Unknown player class '%s'\n", name);
		return false;
	}
	else if (!ti->IsDescendantOf (RUNTIME_CLASS (APlayerPawn)))
	{
		Printf ("Invalid player class '%s'\n", name);
		return false;
	}
	else if (ti->Meta.GetMetaString (APMETA_DisplayName) == NULL)
	{
		Printf ("Missing displayname for player class '%s'\n", name);
		return false;
	}
	return true;
}

void SetupPlayerClasses ()
{
	FPlayerClass newclass;

	PlayerClasses.Clear();
	for (unsigned i=0; i<gameinfo.PlayerClasses.Size(); i++)
	{
		newclass.Flags = 0;
		newclass.Type = PClass::FindClass(gameinfo.PlayerClasses[i]);

		if (ValidatePlayerClass(newclass.Type, gameinfo.PlayerClasses[i]))
		{
			if ((GetDefaultByType(newclass.Type)->flags6 & MF6_NOMENU))
			{
				newclass.Flags |= PCF_NOMENU;
			}
			PlayerClasses.Push (newclass);
		}
	}
}

CCMD (clearplayerclasses)
{
	if (ParsingKeyConf)
	{
		PlayerClasses.Clear ();
	}
}

CCMD (addplayerclass)
{
	if (ParsingKeyConf && argv.argc () > 1)
	{
		const PClass *ti = PClass::FindClass (argv[1]);

		if (ValidatePlayerClass(ti, argv[1]))
		{
			FPlayerClass newclass;

			newclass.Type = ti;
			newclass.Flags = 0;

			int arg = 2;
			while (arg < argv.argc ())
			{
				if (!stricmp (argv[arg], "nomenu"))
				{
					newclass.Flags |= PCF_NOMENU;
				}
				else
				{
					Printf ("Unknown flag '%s' for player class '%s'\n", argv[arg], argv[1]);
				}

				arg++;
			}

			PlayerClasses.Push (newclass);
		}
	}
}

CCMD (playerclasses)
{
	for (unsigned int i = 0; i < PlayerClasses.Size (); i++)
	{
		Printf ("%3d: Class = %s, Name = %s\n", i,
			PlayerClasses[i].Type->TypeName.GetChars(),
			PlayerClasses[i].Type->Meta.GetMetaString (APMETA_DisplayName));
	}
}


//
// Movement.
//

// 16 pixels of bob
#define MAXBOB			0x100000

FArchive &operator<< (FArchive &arc, player_t *&p)
{
	return arc.SerializePointer (players, (BYTE **)&p, sizeof(*players));
}

// The player_t constructor. Since LogText is not a POD, we cannot just
// memset it all to 0.
player_t::player_t()
: mo(0),
  playerstate(0),
  cls(0),
  DesiredFOV(0),
  FOV(0),
  viewz(0),
  viewheight(0),
  deltaviewheight(0),
  bob(0),
  velx(0),
  vely(0),
  centering(0),
  turnticks(0),
  attackdown(0),
  usedown(0),
  oldbuttons(0),
  health(0),
  inventorytics(0),
  CurrentPlayerClass(0),
  backpack(0),
  fragcount(0),
  lastkilltime(0),
  multicount(0),
  spreecount(0),
  WeaponState(0),
  ReadyWeapon(0),
  PendingWeapon(0),
  cheats(0),
  timefreezer(0),
  refire(0),
  inconsistant(0),
  killcount(0),
  itemcount(0),
  secretcount(0),
  damagecount(0),
  bonuscount(0),
  hazardcount(0),
  poisoncount(0),
  poisoner(0),
  attacker(0),
  extralight(0),
  morphTics(0),
  MorphedPlayerClass(0),
  MorphStyle(0),
  MorphExitFlash(0),
  PremorphWeapon(0),
  chickenPeck(0),
  jumpTics(0),
  onground(0),
  respawn_time(0),
  camera(0),
  air_finished(0),
  savedyaw(0),
  savedpitch(0),
  angle(0),
  dest(0),
  prev(0),
  enemy(0),
  missile(0),
  mate(0),
  last_mate(0),
  t_active(0),
  t_respawn(0),
  t_strafe(0),
  t_react(0),
  t_fight(0),
  t_roam(0),
  t_rocket(0),
  isbot(0),
  first_shot(0),
  sleft(0),
  allround(0),
  oldx(0),
  oldy(0),
  BlendR(0),
  BlendG(0),
  BlendB(0),
  BlendA(0),
  LogText(),
  crouching(0),
  crouchdir(0),
  crouchfactor(0),
  crouchoffset(0),
  crouchviewdelta(0),
  ConversationNPC(0),
  ConversationPC(0),
  ConversationNPCAngle(0),
  ConversationFaceTalker(0)
{
	memset (&cmd, 0, sizeof(cmd));
	memset (frags, 0, sizeof(frags));
	memset (psprites, 0, sizeof(psprites));
	memset (&skill, 0, sizeof(skill));
}

player_t &player_t::operator=(const player_t &p)
{
	mo = p.mo;
	playerstate = p.playerstate;
	cmd = p.cmd;
	original_cmd = p.original_cmd;
	original_oldbuttons = p.original_oldbuttons;
	// Intentionally not copying userinfo!
	cls = p.cls;
	DesiredFOV = p.DesiredFOV;
	FOV = p.FOV;
	viewz = p.viewz;
	viewheight = p.viewheight;
	deltaviewheight = p.deltaviewheight;
	bob = p.bob;
	velx = p.velx;
	vely = p.vely;
	centering = p.centering;
	turnticks = p.turnticks;
	attackdown = p.attackdown;
	usedown = p.usedown;
	oldbuttons = p.oldbuttons;
	health = p.health;
	inventorytics = p.inventorytics;
	CurrentPlayerClass = p.CurrentPlayerClass;
	backpack = p.backpack;
	memcpy(frags, &p.frags, sizeof(frags));
	fragcount = p.fragcount;
	lastkilltime = p.lastkilltime;
	multicount = p.multicount;
	spreecount = p.spreecount;
	WeaponState = p.WeaponState;
	ReadyWeapon = p.ReadyWeapon;
	PendingWeapon = p.PendingWeapon;
	cheats = p.cheats;
	timefreezer = p.timefreezer;
	refire = p.refire;
	inconsistant = p.inconsistant;
	waiting = p.waiting;
	killcount = p.killcount;
	itemcount = p.itemcount;
	secretcount = p.secretcount;
	damagecount = p.damagecount;
	bonuscount = p.bonuscount;
	hazardcount = p.hazardcount;
	poisoncount = p.poisoncount;
	poisontype = p.poisontype;
	poisonpaintype = p.poisonpaintype;
	poisoner = p.poisoner;
	attacker = p.attacker;
	extralight = p.extralight;
	fixedcolormap = p.fixedcolormap;
	fixedlightlevel = p.fixedlightlevel;
	memcpy(psprites, &p.psprites, sizeof(psprites));
	morphTics = p.morphTics;
	MorphedPlayerClass = p.MorphedPlayerClass;
	MorphStyle = p.MorphStyle;
	MorphExitFlash = p.MorphExitFlash;
	PremorphWeapon = p.PremorphWeapon;
	chickenPeck = p.chickenPeck;
	jumpTics = p.jumpTics;
	onground = p.onground;
	respawn_time = p.respawn_time;
	camera = p.camera;
	air_finished = p.air_finished;
	LastDamageType = p.LastDamageType;
	savedyaw = p.savedyaw;
	savedpitch = p.savedpitch;
	angle = p.angle;
	dest = p.dest;
	prev = p.prev;
	enemy = p.enemy;
	missile = p.missile;
	mate = p.mate;
	last_mate = p.last_mate;
	settings_controller = p.settings_controller;
	skill = p.skill;
	t_active = p.t_active;
	t_respawn = p.t_respawn;
	t_strafe = p.t_strafe;
	t_react = p.t_react;
	t_fight = p.t_fight;
	t_roam = p.t_roam;
	t_rocket = p.t_rocket;
	isbot = p.isbot;
	first_shot = p.first_shot;
	sleft = p.sleft;
	allround = p.allround;
	oldx = p.oldx;
	oldy = p.oldy;
	BlendR = p.BlendR;
	BlendG = p.BlendG;
	BlendB = p.BlendB;
	BlendA = p.BlendA;
	LogText = p.LogText;
	MinPitch = p.MinPitch;
	MaxPitch = p.MaxPitch;
	crouching = p.crouching;
	crouchdir = p.crouchdir;
	crouchfactor = p.crouchfactor;
	crouchoffset = p.crouchoffset;
	crouchviewdelta = p.crouchviewdelta;
	weapons = p.weapons;
	ConversationNPC = p.ConversationNPC;
	ConversationPC = p.ConversationPC;
	ConversationNPCAngle = p.ConversationNPCAngle;
	ConversationFaceTalker = p.ConversationFaceTalker;
	return *this;
}

// This function supplements the pointer cleanup in dobject.cpp, because
// player_t is not derived from DObject. (I tried it, and DestroyScan was
// unable to properly determine the player object's type--possibly
// because it gets staticly allocated in an array.)
//
// This function checks all the DObject pointers in a player_t and NULLs any
// that match the pointer passed in. If you add any pointers that point to
// DObject (or a subclass), add them here too.

size_t player_t::FixPointers (const DObject *old, DObject *rep)
{
	APlayerPawn *replacement = static_cast<APlayerPawn *>(rep);
	size_t changed = 0;

	// The construct *& is used in several of these to avoid the read barriers
	// that would turn the pointer we want to check to NULL if the old object
	// is pending deletion.
	if (mo == old)					mo = replacement, changed++;
	if (*&poisoner == old)			poisoner = replacement, changed++;
	if (*&attacker == old)			attacker = replacement, changed++;
	if (*&camera == old)			camera = replacement, changed++;
	if (*&dest == old)				dest = replacement, changed++;
	if (*&prev == old)				prev = replacement, changed++;
	if (*&enemy == old)				enemy = replacement, changed++;
	if (*&missile == old)			missile = replacement, changed++;
	if (*&mate == old)				mate = replacement, changed++;
	if (*&last_mate == old)			last_mate = replacement, changed++;
	if (ReadyWeapon == old)			ReadyWeapon = static_cast<AWeapon *>(rep), changed++;
	if (PendingWeapon == old)		PendingWeapon = static_cast<AWeapon *>(rep), changed++;
	if (*&PremorphWeapon == old)	PremorphWeapon = static_cast<AWeapon *>(rep), changed++;
	if (*&ConversationNPC == old)	ConversationNPC = replacement, changed++;
	if (*&ConversationPC == old)	ConversationPC = replacement, changed++;
	return changed;
}

size_t player_t::PropagateMark()
{
	GC::Mark(mo);
	GC::Mark(poisoner);
	GC::Mark(attacker);
	GC::Mark(camera);
	GC::Mark(dest);
	GC::Mark(prev);
	GC::Mark(enemy);
	GC::Mark(missile);
	GC::Mark(mate);
	GC::Mark(last_mate);
	GC::Mark(ReadyWeapon);
	GC::Mark(ConversationNPC);
	GC::Mark(ConversationPC);
	GC::Mark(PremorphWeapon);
	if (PendingWeapon != WP_NOCHANGE)
	{
		GC::Mark(PendingWeapon);
	}
	return sizeof(*this);
}

void player_t::SetLogNumber (int num)
{
	char lumpname[16];
	int lumpnum;

	mysnprintf (lumpname, countof(lumpname), "LOG%d", num);
	lumpnum = Wads.CheckNumForName (lumpname);
	if (lumpnum == -1)
	{
		// Leave the log message alone if this one doesn't exist.
		//SetLogText (lumpname);
	}
	else
	{
		int length=Wads.LumpLength(lumpnum);
		char *data= new char[length+1];
		Wads.ReadLump (lumpnum, data);
		data[length]=0;
		SetLogText (data);
		delete[] data;
	}
}

void player_t::SetLogText (const char *text)
{
	LogText = text;

	// Print log text to console
	AddToConsole(-1, TEXTCOLOR_GOLD);
	AddToConsole(-1, LogText);
	AddToConsole(-1, "\n");
}

int player_t::GetSpawnClass()
{
	const PClass * type = PlayerClasses[CurrentPlayerClass].Type;
	return static_cast<APlayerPawn*>(GetDefaultByType(type))->SpawnMask;
}

//===========================================================================
//
// player_t :: SendPitchLimits
//
// Ask the local player's renderer what pitch restrictions should be imposed
// and let everybody know. Only sends data for the consoleplayer, since the
// local player is the only one our data is valid for.
//
//===========================================================================

void player_t::SendPitchLimits() const
{
	if (this - players == consoleplayer)
	{
		Net_WriteByte(DEM_SETPITCHLIMIT);
		Net_WriteByte(Renderer->GetMaxViewPitch(false));	// up
		Net_WriteByte(Renderer->GetMaxViewPitch(true));		// down
	}
}

//===========================================================================
//
// APlayerPawn
//
//===========================================================================

IMPLEMENT_POINTY_CLASS (APlayerPawn)
 DECLARE_POINTER(InvFirst)
 DECLARE_POINTER(InvSel)
END_POINTERS

IMPLEMENT_CLASS (APlayerChunk)

void APlayerPawn::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	arc << JumpZ
		<< MaxHealth
		<< RunHealth
		<< SpawnMask
		<< ForwardMove1
		<< ForwardMove2
		<< SideMove1
		<< SideMove2
		<< ScoreIcon
		<< InvFirst
		<< InvSel
		<< MorphWeapon
		<< DamageFade
		<< PlayerFlags
		<< FlechetteType;
	if (SaveVersion < 3829)
	{
		GruntSpeed = 12*FRACUNIT;
		FallingScreamMinSpeed = 35*FRACUNIT;
		FallingScreamMaxSpeed = 40*FRACUNIT;
	}
	else
	{
		arc << GruntSpeed << FallingScreamMinSpeed << FallingScreamMaxSpeed;
	}
	if (SaveVersion >= 4502)
	{
		arc << UseRange;
	}
	if (SaveVersion >= 4503)
	{
		arc << AirCapacity;
	}
}

//===========================================================================
//
// APlayerPawn :: MarkPrecacheSounds
//
//===========================================================================

void APlayerPawn::MarkPrecacheSounds() const
{
	Super::MarkPrecacheSounds();
	S_MarkPlayerSounds(GetSoundClass());
}

//===========================================================================
//
// APlayerPawn :: BeginPlay
//
//===========================================================================

void APlayerPawn::BeginPlay ()
{
	Super::BeginPlay ();
	ChangeStatNum (STAT_PLAYER);

	// Check whether a PWADs normal sprite is to be combined with the base WADs
	// crouch sprite. In such a case the sprites normally don't match and it is
	// best to disable the crouch sprite.
	if (crouchsprite > 0)
	{
		// This assumes that player sprites always exist in rotated form and
		// that the front view is always a separate sprite. So far this is
		// true for anything that exists.
		FString normspritename = sprites[SpawnState->sprite].name;
		FString crouchspritename = sprites[crouchsprite].name;

		int spritenorm = Wads.CheckNumForName(normspritename + "A1", ns_sprites);
		int spritecrouch = Wads.CheckNumForName(crouchspritename + "A1", ns_sprites);
		
		if (spritenorm==-1 || spritecrouch ==-1) 
		{
			// Sprites do not exist so it is best to disable the crouch sprite.
			crouchsprite = 0;
			return;
		}
	
		int wadnorm = Wads.GetLumpFile(spritenorm);
		int wadcrouch = Wads.GetLumpFile(spritenorm);
		
		if (wadnorm > FWadCollection::IWAD_FILENUM && wadcrouch <= FWadCollection::IWAD_FILENUM) 
		{
			// Question: Add an option / disable crouching or do what?
			crouchsprite = 0;
		}
	}
}

//===========================================================================
//
// APlayerPawn :: Tick
//
//===========================================================================

void APlayerPawn::Tick()
{
	if (player != NULL && player->mo == this && player->CanCrouch() && player->playerstate != PST_DEAD)
	{
		height = FixedMul(GetDefault()->height, player->crouchfactor);
	}
	else
	{
		if (health > 0) height = GetDefault()->height;
	}
	Super::Tick();
}

//===========================================================================
//
// APlayerPawn :: PostBeginPlay
//
//===========================================================================

void APlayerPawn::PostBeginPlay()
{
	Super::PostBeginPlay();
	SetupWeaponSlots();

	// Voodoo dolls: restore original floorz/ceilingz logic
	if (player == NULL || player->mo != this)
	{
		dropoffz = floorz = Sector->floorplane.ZatPoint(x, y);
		ceilingz = Sector->ceilingplane.ZatPoint(x, y);
		P_FindFloorCeiling(this, FFCF_ONLYSPAWNPOS);
		z = floorz;
	}
	else
	{
		player->SendPitchLimits();
	}
}

//===========================================================================
//
// APlayerPawn :: SetupWeaponSlots
//
// Sets up the default weapon slots for this player. If this is also the
// local player, determines local modifications and sends those across the
// network. Ignores voodoo dolls.
//
//===========================================================================

void APlayerPawn::SetupWeaponSlots()
{
	if (player != NULL && player->mo == this)
	{
		player->weapons.StandardSetup(GetClass());
		// If we're the local player, then there's a bit more work to do.
		// This also applies if we're a bot and this is the net arbitrator.
		if (player - players == consoleplayer ||
			(player->isbot && consoleplayer == Net_Arbitrator))
		{
			FWeaponSlots local_slots(player->weapons);
			if (player->isbot)
			{ // Bots only need weapons from KEYCONF, not INI modifications.
				P_PlaybackKeyConfWeapons(&local_slots);
			}
			else
			{
				local_slots.LocalSetup(GetClass());
			}
			local_slots.SendDifferences(int(player - players), player->weapons);
		}
	}
}

//===========================================================================
//
// APlayerPawn :: AddInventory
//
//===========================================================================

void APlayerPawn::AddInventory (AInventory *item)
{
	// Adding inventory to a voodoo doll should add it to the real player instead.
	if (player != NULL && player->mo != this && player->mo != NULL)
	{
		player->mo->AddInventory (item);
		return;
	}
	Super::AddInventory (item);

	// If nothing is selected, select this item.
	if (InvSel == NULL && (item->ItemFlags & IF_INVBAR))
	{
		InvSel = item;
	}
}

//===========================================================================
//
// APlayerPawn :: RemoveInventory
//
//===========================================================================

void APlayerPawn::RemoveInventory (AInventory *item)
{
	bool pickWeap = false;

	// Since voodoo dolls aren't supposed to have an inventory, there should be
	// no need to redirect them to the real player here as there is with AddInventory.

	// If the item removed is the selected one, select something else, either the next
	// item, if there is one, or the previous item.
	if (player != NULL)
	{
		if (InvSel == item)
		{
			InvSel = item->NextInv ();
			if (InvSel == NULL)
			{
				InvSel = item->PrevInv ();
			}
		}
		if (InvFirst == item)
		{
			InvFirst = item->NextInv ();
			if (InvFirst == NULL)
			{
				InvFirst = item->PrevInv ();
			}
		}
		if (item == player->PendingWeapon)
		{
			player->PendingWeapon = WP_NOCHANGE;
		}
		if (item == player->ReadyWeapon)
		{
			// If the current weapon is removed, clear the refire counter and pick a new one.
			pickWeap = true;
			player->ReadyWeapon = NULL;
			player->refire = 0;
		}
	}
	Super::RemoveInventory (item);
	if (pickWeap && player->mo == this && player->PendingWeapon == WP_NOCHANGE)
	{
		PickNewWeapon (NULL);
	}
}

//===========================================================================
//
// APlayerPawn :: UseInventory
//
//===========================================================================

bool APlayerPawn::UseInventory (AInventory *item)
{
	const PClass *itemtype = item->GetClass();

	if (player->cheats & CF_TOTALLYFROZEN)
	{ // You can't use items if you're totally frozen
		return false;
	}
	if ((level.flags2 & LEVEL2_FROZEN) && (player == NULL || player->timefreezer == 0))
	{
		// Time frozen
		return false;
	}

	if (!Super::UseInventory (item))
	{
		// Heretic and Hexen advance the inventory cursor if the use failed.
		// Should this behavior be retained?
		return false;
	}
	if (player == &players[consoleplayer])
	{
		S_Sound (this, CHAN_ITEM, item->UseSound, 1, ATTN_NORM);
		StatusBar->FlashItem (itemtype);
	}
	return true;
}

//===========================================================================
//
// APlayerPawn :: BestWeapon
//
// Returns the best weapon a player has, possibly restricted to a single
// type of ammo.
//
//===========================================================================

AWeapon *APlayerPawn::BestWeapon (const PClass *ammotype)
{
	AWeapon *bestMatch = NULL;
	int bestOrder = INT_MAX;
	AInventory *item;
	AWeapon *weap;
	bool tomed = NULL != FindInventory (RUNTIME_CLASS(APowerWeaponLevel2), true);

	// Find the best weapon the player has.
	for (item = Inventory; item != NULL; item = item->Inventory)
	{
		if (!item->IsKindOf (RUNTIME_CLASS(AWeapon)))
			continue;

		weap = static_cast<AWeapon *> (item);

		// Don't select it if it's worse than what was already found.
		if (weap->SelectionOrder > bestOrder)
			continue;

		// Don't select it if its primary fire doesn't use the desired ammo.
		if (ammotype != NULL &&
			(weap->Ammo1 == NULL ||
			 weap->Ammo1->GetClass() != ammotype))
			continue;

		// Don't select it if the Tome is active and this isn't the powered-up version.
		if (tomed && weap->SisterWeapon != NULL && weap->SisterWeapon->WeaponFlags & WIF_POWERED_UP)
			continue;

		// Don't select it if it's powered-up and the Tome is not active.
		if (!tomed && weap->WeaponFlags & WIF_POWERED_UP)
			continue;

		// Don't select it if there isn't enough ammo to use its primary fire.
		if (!(weap->WeaponFlags & WIF_AMMO_OPTIONAL) &&
			!weap->CheckAmmo (AWeapon::PrimaryFire, false))
			continue;

		// Don't select if if there isn't enough ammo as determined by the weapon's author.
		if (weap->MinSelAmmo1 > 0 && (weap->Ammo1 == NULL || weap->Ammo1->Amount < weap->MinSelAmmo1))
			continue;
		if (weap->MinSelAmmo2 > 0 && (weap->Ammo2 == NULL || weap->Ammo2->Amount < weap->MinSelAmmo2))
			continue;

		// This weapon is usable!
		bestOrder = weap->SelectionOrder;
		bestMatch = weap;
	}
	return bestMatch;
}

//===========================================================================
//
// APlayerPawn :: PickNewWeapon
//
// Picks a new weapon for this player. Used mostly for running out of ammo,
// but it also works when an ACS script explicitly takes the ready weapon
// away or the player picks up some ammo they had previously run out of.
//
//===========================================================================

AWeapon *APlayerPawn::PickNewWeapon (const PClass *ammotype)
{
	AWeapon *best = BestWeapon (ammotype);

	if (best != NULL)
	{
		player->PendingWeapon = best;
		if (player->ReadyWeapon != NULL)
		{
			P_DropWeapon(player);
		}
		else if (player->PendingWeapon != WP_NOCHANGE)
		{
			P_BringUpWeapon (player);
		}
	}
	return best;
}


//===========================================================================
//
// APlayerPawn :: CheckWeaponSwitch
//
// Checks if weapons should be changed after picking up ammo
//
//===========================================================================

void APlayerPawn::CheckWeaponSwitch(const PClass *ammotype)
{
	if (!player->userinfo.GetNeverSwitch() &&
		player->PendingWeapon == WP_NOCHANGE && 
		(player->ReadyWeapon == NULL ||
		 (player->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON)))
	{
		AWeapon *best = BestWeapon (ammotype);
		if (best != NULL && (player->ReadyWeapon == NULL ||
			best->SelectionOrder < player->ReadyWeapon->SelectionOrder))
		{
			player->PendingWeapon = best;
		}
	}
}

//===========================================================================
//
// APlayerPawn :: GiveDeathmatchInventory
//
// Gives players items they should have in addition to their default
// inventory when playing deathmatch. (i.e. all keys)
//
//===========================================================================

void APlayerPawn::GiveDeathmatchInventory()
{
	for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
	{
		if (PClass::m_Types[i]->IsDescendantOf (RUNTIME_CLASS(AKey)))
		{
			AKey *key = (AKey *)GetDefaultByType (PClass::m_Types[i]);
			if (key->KeyNumber != 0)
			{
				key = static_cast<AKey *>(Spawn (PClass::m_Types[i], 0,0,0, NO_REPLACE));
				if (!key->CallTryPickup (this))
				{
					key->Destroy ();
				}
			}
		}
	}
}

//===========================================================================
//
// APlayerPawn :: FilterCoopRespawnInventory
//
// When respawning in coop, this function is called to walk through the dead
// player's inventory and modify it according to the current game flags so
// that it can be transferred to the new live player. This player currently
// has the default inventory, and the oldplayer has the inventory at the time
// of death.
//
//===========================================================================

void APlayerPawn::FilterCoopRespawnInventory (APlayerPawn *oldplayer)
{
	AInventory *item, *next, *defitem;

	// If we're losing everything, this is really simple.
	if (dmflags & DF_COOP_LOSE_INVENTORY)
	{
		oldplayer->DestroyAllInventory();
		return;
	}

	if (dmflags &  (DF_COOP_LOSE_KEYS |
					DF_COOP_LOSE_WEAPONS |
					DF_COOP_LOSE_AMMO |
					DF_COOP_HALVE_AMMO |
					DF_COOP_LOSE_ARMOR |
					DF_COOP_LOSE_POWERUPS))
	{
		// Walk through the old player's inventory and destroy or modify
		// according to dmflags.
		for (item = oldplayer->Inventory; item != NULL; item = next)
		{
			next = item->Inventory;

			// If this item is part of the default inventory, we never want
			// to destroy it, although we might want to copy the default
			// inventory amount.
			defitem = FindInventory (item->GetClass());

			if ((dmflags & DF_COOP_LOSE_KEYS) &&
				defitem == NULL &&
				item->IsKindOf(RUNTIME_CLASS(AKey)))
			{
				item->Destroy();
			}
			else if ((dmflags & DF_COOP_LOSE_WEAPONS) &&
				defitem == NULL &&
				item->IsKindOf(RUNTIME_CLASS(AWeapon)))
			{
				item->Destroy();
			}
			else if ((dmflags & DF_COOP_LOSE_ARMOR) &&
				item->IsKindOf(RUNTIME_CLASS(AArmor)))
			{
				if (defitem != NULL)
				{
					item->Destroy();
				}
				else if (item->IsKindOf(RUNTIME_CLASS(ABasicArmor)))
				{
					static_cast<ABasicArmor*>(item)->SavePercent = static_cast<ABasicArmor*>(defitem)->SavePercent;
					item->Amount = defitem->Amount;
				}
				else if (item->IsKindOf(RUNTIME_CLASS(AHexenArmor)))
				{
					static_cast<AHexenArmor*>(item)->Slots[0] = static_cast<AHexenArmor*>(defitem)->Slots[0];
					static_cast<AHexenArmor*>(item)->Slots[1] = static_cast<AHexenArmor*>(defitem)->Slots[1];
					static_cast<AHexenArmor*>(item)->Slots[2] = static_cast<AHexenArmor*>(defitem)->Slots[2];
					static_cast<AHexenArmor*>(item)->Slots[3] = static_cast<AHexenArmor*>(defitem)->Slots[3];
				}
			}
			else if ((dmflags & DF_COOP_LOSE_POWERUPS) &&
				defitem == NULL &&
				item->IsKindOf(RUNTIME_CLASS(APowerupGiver)))
			{
				item->Destroy();
			}
			else if ((dmflags & (DF_COOP_LOSE_AMMO | DF_COOP_HALVE_AMMO)) &&
				item->IsKindOf(RUNTIME_CLASS(AAmmo)))
			{
				if (defitem == NULL)
				{
					if (dmflags & DF_COOP_LOSE_AMMO)
					{
						// Do NOT destroy the ammo, because a weapon might reference it.
						item->Amount = 0;
					}
					else if (item->Amount > 1)
					{
						item->Amount /= 2;
					}
				}
				else
				{
					// When set to lose ammo, you get to keep all your starting ammo.
					// When set to halve ammo, you won't be left with less than your starting amount.
					if (dmflags & DF_COOP_LOSE_AMMO)
					{
						item->Amount = defitem->Amount;
					}
					else if (item->Amount > 1)
					{
						item->Amount = MAX(item->Amount / 2, defitem->Amount);
					}
				}
			}
		}
	}

	// Now destroy the default inventory this player is holding and move
	// over the old player's remaining inventory.
	DestroyAllInventory();
	ObtainInventory (oldplayer);

	player->ReadyWeapon = NULL;
	PickNewWeapon (NULL);
}

//===========================================================================
//
// APlayerPawn :: GetSoundClass
//
//===========================================================================

const char *APlayerPawn::GetSoundClass() const
{
	if (player != NULL &&
		(player->mo == NULL || !(player->mo->flags4 &MF4_NOSKIN)) &&
		(unsigned int)player->userinfo.GetSkin() >= PlayerClasses.Size () &&
		(size_t)player->userinfo.GetSkin() < numskins)
	{
		return skins[player->userinfo.GetSkin()].name;
	}

	// [GRB]
	const char *sclass = GetClass ()->Meta.GetMetaString (APMETA_SoundClass);
	return sclass != NULL ? sclass : "player";
}

//===========================================================================
//
// APlayerPawn :: GetMaxHealth
//
// only needed because Boom screwed up Dehacked.
//
//===========================================================================

int APlayerPawn::GetMaxHealth() const 
{ 
	return MaxHealth > 0? MaxHealth : ((i_compatflags&COMPATF_DEHHEALTH)? 100 : deh.MaxHealth);
}

//===========================================================================
//
// APlayerPawn :: UpdateWaterLevel
//
// Plays surfacing and diving sounds, as appropriate.
//
//===========================================================================

bool APlayerPawn::UpdateWaterLevel (fixed_t oldz, bool splash)
{
	int oldlevel = waterlevel;
	bool retval = Super::UpdateWaterLevel (oldz, splash);
	if (player != NULL)
	{
		if (oldlevel < 3 && waterlevel == 3)
		{ // Our head just went under.
			S_Sound (this, CHAN_VOICE, "*dive", 1, ATTN_NORM);
		}
		else if (oldlevel == 3 && waterlevel < 3)
		{ // Our head just came up.
			if (player->air_finished > level.time)
			{ // We hadn't run out of air yet.
				S_Sound (this, CHAN_VOICE, "*surface", 1, ATTN_NORM);
			}
			// If we were running out of air, then ResetAirSupply() will play *gasp.
		}
	}
	return retval;
}

//===========================================================================
//
// APlayerPawn :: ResetAirSupply
//
// Gives the player a full "tank" of air. If they had previously completely
// run out of air, also plays the *gasp sound. Returns true if the player
// was drowning.
//
//===========================================================================

bool APlayerPawn::ResetAirSupply (bool playgasp)
{
	bool wasdrowning = (player->air_finished < level.time);

	if (playgasp && wasdrowning)
	{
		S_Sound (this, CHAN_VOICE, "*gasp", 1, ATTN_NORM);
	}
	if (level.airsupply> 0 && player->mo->AirCapacity > 0) player->air_finished = level.time + FixedMul(level.airsupply, player->mo->AirCapacity);
	else player->air_finished = INT_MAX;
	return wasdrowning;
}

//===========================================================================
//
// Animations
//
//===========================================================================

void APlayerPawn::PlayIdle ()
{
	if (InStateSequence(state, SeeState))
		SetState (SpawnState);
}

void APlayerPawn::PlayRunning ()
{
	if (InStateSequence(state, SpawnState) && SeeState != NULL)
		SetState (SeeState);
}

void APlayerPawn::PlayAttacking ()
{
	if (MissileState != NULL) SetState (MissileState);
}

void APlayerPawn::PlayAttacking2 ()
{
	if (MeleeState != NULL) SetState (MeleeState);
}

void APlayerPawn::ThrowPoisonBag ()
{
}

//===========================================================================
//
// APlayerPawn :: GiveDefaultInventory
//
//===========================================================================

void APlayerPawn::GiveDefaultInventory ()
{
	if (player == NULL) return;

	// [GRB] Give inventory specified in DECORATE
	player->health = GetDefault ()->health;

	// HexenArmor must always be the first item in the inventory because
	// it provides player class based protection that should not affect
	// any other protection item.
	fixed_t hx[5];
	for(int i=0;i<5;i++)
	{
		hx[i] = GetClass()->Meta.GetMetaFixed(APMETA_Hexenarmor0+i);
	}
	GiveInventoryType (RUNTIME_CLASS(AHexenArmor));
	AHexenArmor *harmor = FindInventory<AHexenArmor>();
	harmor->Slots[4] = hx[0];
	harmor->SlotsIncrement[0] = hx[1];
	harmor->SlotsIncrement[1] = hx[2];
	harmor->SlotsIncrement[2] = hx[3];
	harmor->SlotsIncrement[3] = hx[4];

	// BasicArmor must come right after that. It should not affect any
	// other protection item as well but needs to process the damage
	// before the HexenArmor does.
	ABasicArmor *barmor = Spawn<ABasicArmor> (0,0,0, NO_REPLACE);
	barmor->BecomeItem ();
	barmor->SavePercent = 0;
	barmor->Amount = 0;
	AddInventory (barmor);

	// Now add the items from the DECORATE definition
	FDropItem *di = GetDropItems();

	while (di)
	{
		const PClass *ti = PClass::FindClass (di->Name);
		if (ti)
		{
			AInventory *item = FindInventory (ti);
			if (item != NULL)
			{
				item->Amount = clamp<int>(
					item->Amount + (di->amount ? di->amount : ((AInventory *)item->GetDefault ())->Amount),
					0, item->MaxAmount);
			}
			else
			{
				item = static_cast<AInventory *>(Spawn (ti, 0,0,0, NO_REPLACE));
				item->ItemFlags|=IF_IGNORESKILL;	// no skill multiplicators here
				item->Amount = di->amount;
				if (item->IsKindOf (RUNTIME_CLASS (AWeapon)))
				{
					// To allow better control any weapon is emptied of
					// ammo before being given to the player.
					static_cast<AWeapon*>(item)->AmmoGive1 =
					static_cast<AWeapon*>(item)->AmmoGive2 = 0;
				}
				AActor *check;
				if (!item->CallTryPickup(this, &check))
				{
					if (check != this)
					{
						// Player was morphed. This is illegal at game start.
						// This problem is only detectable when it's too late to do something about it...
						I_Error("Cannot give morph items when starting a game");
					}
					item->Destroy ();
					item = NULL;
				}
			}
			if (item != NULL && item->IsKindOf (RUNTIME_CLASS (AWeapon)) &&
				static_cast<AWeapon*>(item)->CheckAmmo(AWeapon::EitherFire, false))
			{
				player->ReadyWeapon = player->PendingWeapon = static_cast<AWeapon *> (item);
			}
		}
		di = di->Next;
	}
}

void APlayerPawn::MorphPlayerThink ()
{
}

void APlayerPawn::ActivateMorphWeapon ()
{
	const PClass *morphweapon = PClass::FindClass (MorphWeapon);
	player->PendingWeapon = WP_NOCHANGE;
	player->psprites[ps_weapon].sy = WEAPONTOP;

	if (morphweapon == NULL || !morphweapon->IsDescendantOf (RUNTIME_CLASS(AWeapon)))
	{ // No weapon at all while morphed!
		player->ReadyWeapon = NULL;
		P_SetPsprite (player, ps_weapon, NULL);
	}
	else
	{
		player->ReadyWeapon = static_cast<AWeapon *>(player->mo->FindInventory (morphweapon));
		if (player->ReadyWeapon == NULL)
		{
			player->ReadyWeapon = static_cast<AWeapon *>(player->mo->GiveInventoryType (morphweapon));
			if (player->ReadyWeapon != NULL)
			{
				player->ReadyWeapon->GivenAsMorphWeapon = true; // flag is used only by new beastweap semantics in P_UndoPlayerMorph
			}
		}
		if (player->ReadyWeapon != NULL)
		{
			P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetReadyState());
		}
		else
		{
			P_SetPsprite (player, ps_weapon, NULL);
		}
	}
	P_SetPsprite (player, ps_flash, NULL);

	player->PendingWeapon = WP_NOCHANGE;
}

//===========================================================================
//
// APlayerPawn :: Die
//
//===========================================================================

void APlayerPawn::Die (AActor *source, AActor *inflictor, int dmgflags)
{
	Super::Die (source, inflictor, dmgflags);

	if (player != NULL && player->mo == this) player->bonuscount = 0;

	if (player != NULL && player->mo != this)
	{ // Make the real player die, too
		player->mo->Die (source, inflictor, dmgflags);
	}
	else
	{
		if (player != NULL && (dmflags2 & DF2_YES_WEAPONDROP))
		{ // Voodoo dolls don't drop weapons
			AWeapon *weap = player->ReadyWeapon;
			if (weap != NULL)
			{
				AInventory *item;

				// kgDROP - start - modified copy from a_action.cpp
				FDropItem *di = weap->GetDropItems();

				if (di != NULL)
				{
					while (di != NULL)
					{
						if (di->Name != NAME_None)
						{
							const PClass *ti = PClass::FindClass(di->Name);
							if (ti) P_DropItem (player->mo, ti, di->amount, di->probability);
						}
						di = di->Next;
					}
				} else
				// kgDROP - end
				if (weap->SpawnState != NULL &&
					weap->SpawnState != ::GetDefault<AActor>()->SpawnState)
				{
					item = P_DropItem (this, weap->GetClass(), -1, 256);
					if (item != NULL)
					{
						if (weap->AmmoGive1 && weap->Ammo1)
						{
							static_cast<AWeapon *>(item)->AmmoGive1 = weap->Ammo1->Amount;
						}
						if (weap->AmmoGive2 && weap->Ammo2)
						{
							static_cast<AWeapon *>(item)->AmmoGive2 = weap->Ammo2->Amount;
						}
						item->ItemFlags |= IF_IGNORESKILL;
					}
				}
				else
				{
					item = P_DropItem (this, weap->AmmoType1, -1, 256);
					if (item != NULL)
					{
						item->Amount = weap->Ammo1->Amount;
						item->ItemFlags |= IF_IGNORESKILL;
					}
					item = P_DropItem (this, weap->AmmoType2, -1, 256);
					if (item != NULL)
					{
						item->Amount = weap->Ammo2->Amount;
						item->ItemFlags |= IF_IGNORESKILL;
					}
				}
			}
		}
		if (!multiplayer && level.info->deathsequence != NAME_None)
		{
			F_StartIntermission(level.info->deathsequence, FSTATE_EndingGame);
		}
	}
}

//===========================================================================
//
// APlayerPawn :: TweakSpeeds
//
//===========================================================================

void APlayerPawn::TweakSpeeds (int &forward, int &side)
{
	// Strife's player can't run when its healh is below 10
	if (health <= RunHealth)
	{
		forward = clamp(forward, -0x1900, 0x1900);
		side = clamp(side, -0x1800, 0x1800);
	}

	// [GRB]
	if ((unsigned int)(forward + 0x31ff) < 0x63ff)
	{
		forward = FixedMul (forward, ForwardMove1);
	}
	else
	{
		forward = FixedMul (forward, ForwardMove2);
	}
	if ((unsigned int)(side + 0x27ff) < 0x4fff)
	{
		side = FixedMul (side, SideMove1);
	}
	else
	{
		side = FixedMul (side, SideMove2);
	}

	if (!player->morphTics && Inventory != NULL)
	{
		fixed_t factor = Inventory->GetSpeedFactor ();
		forward = FixedMul(forward, factor);
		side = FixedMul(side, factor);
	}
}

//===========================================================================
//
// A_PlayerScream
//
// try to find the appropriate death sound and use suitable 
// replacements if necessary
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PlayerScream)
{
	int sound = 0;
	int chan = CHAN_VOICE;

	if (self->player == NULL || self->DeathSound != 0)
	{
		if (self->DeathSound != 0)
		{
			S_Sound (self, CHAN_VOICE, self->DeathSound, 1, ATTN_NORM);
		}
		else
		{
			S_Sound (self, CHAN_VOICE, "*death", 1, ATTN_NORM);
		}
		return;
	}

	// Handle the different player death screams
	if ((((level.flags >> 15) | (dmflags)) &
		(DF_FORCE_FALLINGZD | DF_FORCE_FALLINGHX)) &&
		self->velz <= -39*FRACUNIT)
	{
		sound = S_FindSkinnedSound (self, "*splat");
		chan = CHAN_BODY;
	}

	if (!sound && self->special1<10)
	{ // Wimpy death sound
		sound = S_FindSkinnedSoundEx (self, "*wimpydeath", self->player->LastDamageType);
	}
	if (!sound && self->health <= -50)
	{
		if (self->health > -100)
		{ // Crazy death sound
			sound = S_FindSkinnedSoundEx (self, "*crazydeath", self->player->LastDamageType);
		}
		if (!sound)
		{ // Extreme death sound
			sound = S_FindSkinnedSoundEx (self, "*xdeath", self->player->LastDamageType);
			if (!sound)
			{
				sound = S_FindSkinnedSoundEx (self, "*gibbed", self->player->LastDamageType);
				chan = CHAN_BODY;
			}
		}
	}
	if (!sound)
	{ // Normal death sound
		sound = S_FindSkinnedSoundEx (self, "*death", self->player->LastDamageType);
	}

	if (chan != CHAN_VOICE)
	{
		for (int i = 0; i < 8; ++i)
		{ // Stop most playing sounds from this player.
		  // This is mainly to stop *land from messing up *splat.
			if (i != CHAN_WEAPON && i != CHAN_VOICE)
			{
				S_StopSound (self, i);
			}
		}
	}
	S_Sound (self, chan, sound, 1, ATTN_NORM);
}


//----------------------------------------------------------------------------
//
// PROC A_SkullPop
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SkullPop)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(spawntype, 0);

	APlayerPawn *mo;
	player_t *player;

	// [GRB] Parameterized version
	if (!spawntype || !spawntype->IsDescendantOf (RUNTIME_CLASS (APlayerChunk)))
	{
		spawntype = PClass::FindClass("BloodySkull");
		if (spawntype == NULL) return;
	}

	self->flags &= ~MF_SOLID;
	mo = (APlayerPawn *)Spawn (spawntype, self->x, self->y, self->z + 48*FRACUNIT, NO_REPLACE);
	//mo->target = self;
	mo->velx = pr_skullpop.Random2() << 9;
	mo->vely = pr_skullpop.Random2() << 9;
	mo->velz = 2*FRACUNIT + (pr_skullpop() << 6);
	// Attach player mobj to bloody skull
	player = self->player;
	self->player = NULL;
	mo->ObtainInventory (self);
	mo->player = player;
	mo->health = self->health;
	mo->angle = self->angle;
	if (player != NULL)
	{
		player->mo = mo;
		if (player->camera == self)
		{
			player->camera = mo;
		}
		player->damagecount = 32;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_CheckSkullDone
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_CheckPlayerDone)
{
	if (self->player == NULL)
	{
		self->Destroy ();
	}
}

//===========================================================================
//
// P_CheckPlayerSprites
//
// Here's the place where crouching sprites are handled.
// R_ProjectSprite() calls this for any players.
//
//===========================================================================

void P_CheckPlayerSprite(AActor *actor, int &spritenum, fixed_t &scalex, fixed_t &scaley)
{
	player_t *player = actor->player;
	int crouchspriteno;

	if (player->userinfo.GetSkin() != 0 && !(actor->flags4 & MF4_NOSKIN))
	{
		// Convert from default scale to skin scale.
		fixed_t defscaleY = actor->GetDefault()->scaleY;
		fixed_t defscaleX = actor->GetDefault()->scaleX;
		scaley = Scale(scaley, skins[player->userinfo.GetSkin()].ScaleY, defscaleY);
		scalex = Scale(scalex, skins[player->userinfo.GetSkin()].ScaleX, defscaleX);
	}

	// Set the crouch sprite?
	if (player->crouchfactor < FRACUNIT*3/4)
	{
		if (spritenum == actor->SpawnState->sprite || spritenum == player->mo->crouchsprite) 
		{
			crouchspriteno = player->mo->crouchsprite;
		}
		else if (!(actor->flags4 & MF4_NOSKIN) &&
				(spritenum == skins[player->userinfo.GetSkin()].sprite ||
				 spritenum == skins[player->userinfo.GetSkin()].crouchsprite))
		{
			crouchspriteno = skins[player->userinfo.GetSkin()].crouchsprite;
		}
		else
		{ // no sprite -> squash the existing one
			crouchspriteno = -1;
		}

		if (crouchspriteno > 0) 
		{
			spritenum = crouchspriteno;
		}
		else if (player->playerstate != PST_DEAD && player->crouchfactor < FRACUNIT*3/4)
		{
			scaley /= 2;
		}
	}
}

/*
==================
=
= P_Thrust
=
= moves the given origin along a given angle
=
==================
*/

void P_SideThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle = (angle - ANGLE_90) >> ANGLETOFINESHIFT;

	player->mo->velx += FixedMul (move, finecosine[angle]);
	player->mo->vely += FixedMul (move, finesine[angle]);
}

void P_ForwardThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	if ((player->mo->waterlevel || (player->mo->flags & MF_NOGRAVITY))
		&& player->mo->pitch != 0)
	{
		angle_t pitch = (angle_t)player->mo->pitch >> ANGLETOFINESHIFT;
		fixed_t zpush = FixedMul (move, finesine[pitch]);
		if (player->mo->waterlevel && player->mo->waterlevel < 2 && zpush < 0)
			zpush = 0;
		player->mo->velz -= zpush;
		move = FixedMul (move, finecosine[pitch]);
	}
	player->mo->velx += FixedMul (move, finecosine[angle]);
	player->mo->vely += FixedMul (move, finesine[angle]);
}

//
// P_Bob
// Same as P_Thrust, but only affects bobbing.
//
// killough 10/98: We apply thrust separately between the real physical player
// and the part which affects bobbing. This way, bobbing only comes from player
// motion, nothing external, avoiding many problems, e.g. bobbing should not
// occur on conveyors, unless the player walks on one, and bobbing should be
// reduced at a regular rate, even on ice (where the player coasts).
//

void P_Bob (player_t *player, angle_t angle, fixed_t move, bool forward)
{
	if (forward
		&& (player->mo->waterlevel || (player->mo->flags & MF_NOGRAVITY))
		&& player->mo->pitch != 0)
	{
		angle_t pitch = (angle_t)player->mo->pitch >> ANGLETOFINESHIFT;
		move = FixedMul (move, finecosine[pitch]);
	}

	angle >>= ANGLETOFINESHIFT;

	player->velx += FixedMul(move, finecosine[angle]);
	player->vely += FixedMul(move, finesine[angle]);
}

/*
==================
=
= P_CalcHeight
=
=
Calculate the walking / running height adjustment
=
==================
*/

void P_CalcHeight (player_t *player) 
{
	int 		angle;
	fixed_t 	bob;
	bool		still = false;

	// Regular movement bobbing
	// (needs to be calculated for gun swing even if not on ground)

	// killough 10/98: Make bobbing depend only on player-applied motion.
	//
	// Note: don't reduce bobbing here if on ice: if you reduce bobbing here,
	// it causes bobbing jerkiness when the player moves from ice to non-ice,
	// and vice-versa.

	if (player->cheats & CF_NOCLIP2)
	{
		player->bob = 0;
	}
	else if ((player->mo->flags & MF_NOGRAVITY) && !player->onground)
	{
		player->bob = FRACUNIT / 2;
	}
	else
	{
		player->bob = DMulScale16 (player->velx, player->velx, player->vely, player->vely);
		if (player->bob == 0)
		{
			still = true;
		}
		else
		{
			player->bob = FixedMul (player->bob, player->userinfo.GetMoveBob());

			if (player->bob > MAXBOB)
				player->bob = MAXBOB;
		}
	}

	fixed_t defaultviewheight = player->mo->ViewHeight + player->crouchviewdelta;

	if (player->cheats & CF_NOVELOCITY)
	{
		player->viewz = player->mo->z + defaultviewheight;

		if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
			player->viewz = player->mo->ceilingz-4*FRACUNIT;

		return;
	}

	if (still)
	{
		if (player->health > 0)
		{
			angle = DivScale13 (level.time, 120*TICRATE/35) & FINEMASK;
			bob = FixedMul (player->userinfo.GetStillBob(), finesine[angle]);
		}
		else
		{
			bob = 0;
		}
	}
	else
	{
		// DivScale 13 because FINEANGLES == (1<<13)
		angle = DivScale13 (level.time, 20*TICRATE/35) & FINEMASK;
		bob = FixedMul (player->bob>>(player->mo->waterlevel > 1 ? 2 : 1), finesine[angle]);
	}

	// move viewheight
	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > defaultviewheight)
		{
			player->viewheight = defaultviewheight;
			player->deltaviewheight = 0;
		}
		else if (player->viewheight < (defaultviewheight>>1))
		{
			player->viewheight = defaultviewheight>>1;
			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}
		
		if (player->deltaviewheight)	
		{
			player->deltaviewheight += FRACUNIT/4;
			if (!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}

	if (player->morphTics)
	{
		bob = 0;
	}
	player->viewz = player->mo->z + player->viewheight + bob;
	if (player->mo->floorclip && player->playerstate != PST_DEAD
		&& player->mo->z <= player->mo->floorz)
	{
		player->viewz -= player->mo->floorclip;
	}
	if (player->viewz > player->mo->ceilingz - 4*FRACUNIT)
	{
		player->viewz = player->mo->ceilingz - 4*FRACUNIT;
	}
	if (player->viewz < player->mo->floorz + 4*FRACUNIT)
	{
		player->viewz = player->mo->floorz + 4*FRACUNIT;
	}
}

/*
=================
=
= P_MovePlayer
=
=================
*/
CUSTOM_CVAR (Float, sv_aircontrol, 0.00390625f, CVAR_SERVERINFO|CVAR_NOSAVE)
{
	level.aircontrol = (fixed_t)(self * 65536.f);
	G_AirControlChanged ();
}

void P_MovePlayer (player_t *player)
{
	ticcmd_t *cmd = &player->cmd;
	APlayerPawn *mo = player->mo;

	// [RH] 180-degree turn overrides all other yaws
	if (player->turnticks)
	{
		player->turnticks--;
		mo->angle += (ANGLE_180 / TURN180_TICKS);
	}
	else
	{
		mo->angle += cmd->ucmd.yaw << 16;
	}

	player->onground = (mo->z <= mo->floorz) || (mo->flags2 & MF2_ONMOBJ) || (mo->BounceFlags & BOUNCE_MBF) || (player->cheats & CF_NOCLIP2);

	// killough 10/98:
	//
	// We must apply thrust to the player and bobbing separately, to avoid
	// anomalies. The thrust applied to bobbing is always the same strength on
	// ice, because the player still "works just as hard" to move, while the
	// thrust applied to the movement varies with 'movefactor'.

	if (cmd->ucmd.forwardmove | cmd->ucmd.sidemove)
	{
		fixed_t forwardmove, sidemove;
		int bobfactor;
		int friction, movefactor;
		int fm, sm;

		movefactor = P_GetMoveFactor (mo, &friction);
		bobfactor = friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;
		if (!player->onground && !(player->mo->flags & MF_NOGRAVITY) && !player->mo->waterlevel)
		{
			// [RH] allow very limited movement if not on ground.
			movefactor = FixedMul (movefactor, level.aircontrol);
			bobfactor = FixedMul (bobfactor, level.aircontrol);
		}

		fm = cmd->ucmd.forwardmove;
		sm = cmd->ucmd.sidemove;
		mo->TweakSpeeds (fm, sm);
		fm = FixedMul (fm, player->mo->Speed);
		sm = FixedMul (sm, player->mo->Speed);

		// When crouching, speed and bobbing have to be reduced
		if (player->CanCrouch() && player->crouchfactor != FRACUNIT)
		{
			fm = FixedMul(fm, player->crouchfactor);
			sm = FixedMul(sm, player->crouchfactor);
			bobfactor = FixedMul(bobfactor, player->crouchfactor);
		}

		forwardmove = Scale (fm, movefactor * 35, TICRATE << 8);
		sidemove = Scale (sm, movefactor * 35, TICRATE << 8);

		if (forwardmove)
		{
			P_Bob (player, mo->angle, (cmd->ucmd.forwardmove * bobfactor) >> 8, true);
			P_ForwardThrust (player, mo->angle, forwardmove);
		}
		if (sidemove)
		{
			P_Bob (player, mo->angle-ANG90, (cmd->ucmd.sidemove * bobfactor) >> 8, false);
			P_SideThrust (player, mo->angle, sidemove);
		}

		if (debugfile)
		{
			fprintf (debugfile, "move player for pl %d%c: (%d,%d,%d) (%d,%d) %d %d w%d [", int(player-players),
				player->cheats&CF_PREDICTING?'p':' ',
				player->mo->x, player->mo->y, player->mo->z,forwardmove, sidemove, movefactor, friction, player->mo->waterlevel);
			msecnode_t *n = player->mo->touching_sectorlist;
			while (n != NULL)
			{
				fprintf (debugfile, "%td ", n->m_sector-sectors);
				n = n->m_tnext;
			}
			fprintf (debugfile, "]\n");
		}

		if (!(player->cheats & CF_PREDICTING) && (forwardmove|sidemove))
		{
			player->mo->PlayRunning ();
		}

		if (player->cheats & CF_REVERTPLEASE)
		{
			player->cheats &= ~CF_REVERTPLEASE;
			player->camera = player->mo;
		}
	}
}		

//==========================================================================
//
// P_FallingDamage
//
//==========================================================================

void P_FallingDamage (AActor *actor)
{
	int damagestyle;
	int damage;
	fixed_t vel;

	damagestyle = ((level.flags >> 15) | (dmflags)) &
		(DF_FORCE_FALLINGZD | DF_FORCE_FALLINGHX);

	if (damagestyle == 0)
		return;
		
	if (actor->floorsector->Flags & SECF_NOFALLINGDAMAGE)
		return;

	vel = abs(actor->velz);

	// Since Hexen falling damage is stronger than ZDoom's, it takes
	// precedence. ZDoom falling damage may not be as strong, but it
	// gets felt sooner.

	switch (damagestyle)
	{
	case DF_FORCE_FALLINGHX:		// Hexen falling damage
		if (vel <= 23*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		if (vel >= 63*FRACUNIT)
		{ // automatic death
			damage = 1000000;
		}
		else
		{
			vel = FixedMul (vel, 16*FRACUNIT/23);
			damage = ((FixedMul (vel, vel) / 10) >> FRACBITS) - 24;
			if (actor->velz > -39*FRACUNIT && damage > actor->health
				&& actor->health != 1)
			{ // No-death threshold
				damage = actor->health-1;
			}
		}
		break;
	
	case DF_FORCE_FALLINGZD:		// ZDoom falling damage
		if (vel <= 19*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		if (vel >= 84*FRACUNIT)
		{ // automatic death
			damage = 1000000;
		}
		else
		{
			damage = ((MulScale23 (vel, vel*11) >> FRACBITS) - 30) / 2;
			if (damage < 1)
			{
				damage = 1;
			}
		}
		break;

	case DF_FORCE_FALLINGST:		// Strife falling damage
		if (vel <= 20*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		// The minimum amount of damage you take from falling in Strife
		// is 52. Ouch!
		damage = vel / 25000;
		break;

	default:
		return;
	}

	if (actor->player)
	{
		S_Sound (actor, CHAN_AUTO, "*land", 1, ATTN_NORM);
		P_NoiseAlert (actor, actor, true);
		if (damage == 1000000 && (actor->player->cheats & (CF_GODMODE | CF_BUDDHA)))
		{
			damage = 999;
		}
	}
	P_DamageMobj (actor, NULL, NULL, damage, NAME_Falling);
}

//==========================================================================
//
// P_DeathThink
//
//==========================================================================

void P_DeathThink (player_t *player)
{
	int dir;
	angle_t delta;
	int lookDelta;

	P_MovePsprites (player);

	player->onground = (player->mo->z <= player->mo->floorz);
	if (player->mo->IsKindOf (RUNTIME_CLASS(APlayerChunk)))
	{ // Flying bloody skull or flying ice chunk
		player->viewheight = 6 * FRACUNIT;
		player->deltaviewheight = 0;
		if (player->onground)
		{
			if (player->mo->pitch > -(int)ANGLE_1*19)
			{
				lookDelta = (-(int)ANGLE_1*19 - player->mo->pitch) / 8;
				player->mo->pitch += lookDelta;
			}
		}
	}
	else if (!(player->mo->flags & MF_ICECORPSE))
	{ // Fall to ground (if not frozen)
		player->deltaviewheight = 0;
		if (player->viewheight > 6*FRACUNIT)
		{
			player->viewheight -= FRACUNIT;
		}
		if (player->viewheight < 6*FRACUNIT)
		{
			player->viewheight = 6*FRACUNIT;
		}
		if (player->mo->pitch < 0)
		{
			player->mo->pitch += ANGLE_1*3;
		}
		else if (player->mo->pitch > 0)
		{
			player->mo->pitch -= ANGLE_1*3;
		}
		if (abs(player->mo->pitch) < ANGLE_1*3)
		{
			player->mo->pitch = 0;
		}
	}
	P_CalcHeight (player);
		
	if (player->attacker && player->attacker != player->mo)
	{ // Watch killer
		dir = P_FaceMobj (player->mo, player->attacker, &delta);
		if (delta < ANGLE_1*10)
		{ // Looking at killer, so fade damage and poison counters
			if (player->damagecount)
			{
				player->damagecount--;
			}
			if (player->poisoncount)
			{
				player->poisoncount--;
			}
		}
		delta /= 8;
		if (delta > ANGLE_1*5)
		{
			delta = ANGLE_1*5;
		}
		if (dir)
		{ // Turn clockwise
			player->mo->angle += delta;
		}
		else
		{ // Turn counter clockwise
			player->mo->angle -= delta;
		}
	}
	else
	{
		if (player->damagecount)
		{
			player->damagecount--;
		}
		if (player->poisoncount)
		{
			player->poisoncount--;
		}
	}		

	if ((player->cmd.ucmd.buttons & BT_USE ||
		((multiplayer || alwaysapplydmflags) && (dmflags & DF_FORCE_RESPAWN))) && !(dmflags2 & DF2_NO_RESPAWN))
	{
		if (level.time >= player->respawn_time || ((player->cmd.ucmd.buttons & BT_USE) && !player->isbot))
		{
			player->cls = NULL;		// Force a new class if the player is using a random class
			player->playerstate = (multiplayer || (level.flags2 & LEVEL2_ALLOWRESPAWN)) ? PST_REBORN : PST_ENTER;
			if (player->mo->special1 > 2)
			{
				player->mo->special1 = 0;
			}
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC P_CrouchMove
//
//----------------------------------------------------------------------------

void P_CrouchMove(player_t * player, int direction)
{
	fixed_t defaultheight = player->mo->GetDefault()->height;
	fixed_t savedheight = player->mo->height;
	fixed_t crouchspeed = direction * CROUCHSPEED;
	fixed_t oldheight = player->viewheight;

	player->crouchdir = (signed char) direction;
	player->crouchfactor += crouchspeed;

	// check whether the move is ok
	player->mo->height = FixedMul(defaultheight, player->crouchfactor);
	if (!P_TryMove(player->mo, player->mo->x, player->mo->y, false, NULL))
	{
		player->mo->height = savedheight;
		if (direction > 0)
		{
			// doesn't fit
			player->crouchfactor -= crouchspeed;
			return;
		}
	}
	player->mo->height = savedheight;

	player->crouchfactor = clamp<fixed_t>(player->crouchfactor, FRACUNIT/2, FRACUNIT);
	player->viewheight = FixedMul(player->mo->ViewHeight, player->crouchfactor);
	player->crouchviewdelta = player->viewheight - player->mo->ViewHeight;

	// Check for eyes going above/below fake floor due to crouching motion.
	P_CheckFakeFloorTriggers(player->mo, player->mo->z + oldheight, true);
}

//----------------------------------------------------------------------------
//
// PROC P_PlayerThink
//
//----------------------------------------------------------------------------

void P_PlayerThink (player_t *player)
{
	ticcmd_t *cmd;

	if (player->mo == NULL)
	{
		I_Error ("No player %td start\n", player - players + 1);
	}

	if (debugfile && !(player->cheats & CF_PREDICTING))
	{
		fprintf (debugfile, "tic %d for pl %td: (%d, %d, %d, %u) b:%02x p:%d y:%d f:%d s:%d u:%d\n",
			gametic, player-players, player->mo->x, player->mo->y, player->mo->z,
			player->mo->angle>>ANGLETOFINESHIFT, player->cmd.ucmd.buttons,
			player->cmd.ucmd.pitch, player->cmd.ucmd.yaw, player->cmd.ucmd.forwardmove,
			player->cmd.ucmd.sidemove, player->cmd.ucmd.upmove);
	}

	// [RH] Zoom the player's FOV
	float desired = player->DesiredFOV;
	// Adjust FOV using on the currently held weapon.
	if (player->playerstate != PST_DEAD &&		// No adjustment while dead.
		player->ReadyWeapon != NULL &&			// No adjustment if no weapon.
		player->ReadyWeapon->FOVScale != 0)		// No adjustment if the adjustment is zero.
	{
		// A negative scale is used to prevent G_AddViewAngle/G_AddViewPitch
		// from scaling with the FOV scale.
		desired *= fabs(player->ReadyWeapon->FOVScale);
	}
	if (player->FOV != desired)
	{
		if (fabsf (player->FOV - desired) < 7.f)
		{
			player->FOV = desired;
		}
		else
		{
			float zoom = MAX(7.f, fabsf(player->FOV - desired) * 0.025f);
			if (player->FOV > desired)
			{
				player->FOV = player->FOV - zoom;
			}
			else
			{
				player->FOV = player->FOV + zoom;
			}
		}
	}
	if (player->inventorytics)
	{
		player->inventorytics--;
	}
	// Don't interpolate the view for more than one tic
	player->cheats &= ~CF_INTERPVIEW;

	// No-clip cheat
	if ((player->cheats & (CF_NOCLIP | CF_NOCLIP2)) == CF_NOCLIP2)
	{ // No noclip2 without noclip
		player->cheats &= ~CF_NOCLIP2;
	}
	if (player->cheats & (CF_NOCLIP | CF_NOCLIP2) || (player->mo->GetDefault()->flags & MF_NOCLIP))
	{
		player->mo->flags |= MF_NOCLIP;
	}
	else
	{
		player->mo->flags &= ~MF_NOCLIP;
	}
	if (player->cheats & CF_NOCLIP2)
	{
		player->mo->flags |= MF_NOGRAVITY;
	}
	else if (!(player->mo->flags2 & MF2_FLY) && !(player->mo->GetDefault()->flags & MF_NOGRAVITY))
	{
		player->mo->flags &= ~MF_NOGRAVITY;
	}
	cmd = &player->cmd;

	// Make unmodified copies for ACS's GetPlayerInput.
	player->original_oldbuttons = player->original_cmd.buttons;
	player->original_cmd = cmd->ucmd;

	if (player->mo->flags & MF_JUSTATTACKED)
	{ // Chainsaw/Gauntlets attack auto forward motion
		cmd->ucmd.yaw = 0;
		cmd->ucmd.forwardmove = 0xc800/2;
		cmd->ucmd.sidemove = 0;
		player->mo->flags &= ~MF_JUSTATTACKED;
	}

	bool totallyfrozen = P_IsPlayerTotallyFrozen(player);

	// [RH] Being totally frozen zeros out most input parameters.
	if (totallyfrozen)
	{
		if (gamestate == GS_TITLELEVEL)
		{
			cmd->ucmd.buttons = 0;
		}
		else
		{
			cmd->ucmd.buttons &= BT_USE;
		}
		cmd->ucmd.pitch = 0;
		cmd->ucmd.yaw = 0;
		cmd->ucmd.roll = 0;
		cmd->ucmd.forwardmove = 0;
		cmd->ucmd.sidemove = 0;
		cmd->ucmd.upmove = 0;
		player->turnticks = 0;
	}
	else if (player->cheats & CF_FROZEN)
	{
		cmd->ucmd.forwardmove = 0;
		cmd->ucmd.sidemove = 0;
		cmd->ucmd.upmove = 0;
	}

	// Handle crouching
	if (player->cmd.ucmd.buttons & BT_JUMP)
	{
		player->cmd.ucmd.buttons &= ~BT_CROUCH;
	}
	if (player->CanCrouch() && player->health > 0 && level.IsCrouchingAllowed())
	{
		if (!totallyfrozen)
		{
			int crouchdir = player->crouching;
		
			if (crouchdir == 0)
			{
				crouchdir = (player->cmd.ucmd.buttons & BT_CROUCH) ? -1 : 1;
			}
			else if (player->cmd.ucmd.buttons & BT_CROUCH)
			{
				player->crouching = 0;
			}
			if (crouchdir == 1 && player->crouchfactor < FRACUNIT &&
				player->mo->z + player->mo->height < player->mo->ceilingz)
			{
				P_CrouchMove(player, 1);
			}
			else if (crouchdir == -1 && player->crouchfactor > FRACUNIT/2)
			{
				P_CrouchMove(player, -1);
			}
		}
	}
	else
	{
		player->Uncrouch();
	}

	player->crouchoffset = -FixedMul(player->mo->ViewHeight, (FRACUNIT - player->crouchfactor));


	if (player->playerstate == PST_DEAD)
	{
		player->Uncrouch();
		P_DeathThink (player);
		return;
	}
	if (player->jumpTics != 0)
	{
		player->jumpTics--;
		if (player->onground && player->jumpTics < -18)
		{
			player->jumpTics = 0;
		}
	}
	if (player->morphTics && !(player->cheats & CF_PREDICTING))
	{
		player->mo->MorphPlayerThink ();
	}

	// [RH] Look up/down stuff
	if (!level.IsFreelookAllowed())
	{
		player->mo->pitch = 0;
	}
	else
	{
		int look = cmd->ucmd.pitch << 16;

		// The player's view pitch is clamped between -32 and +56 degrees,
		// which translates to about half a screen height up and (more than)
		// one full screen height down from straight ahead when view panning
		// is used.
		if (look)
		{
			if (look == -32768 << 16)
			{ // center view
				player->centering = true;
			}
			else if (!player->centering)
			{
				fixed_t oldpitch = player->mo->pitch;
				player->mo->pitch -= look;
				if (look > 0)
				{ // look up
					player->mo->pitch = MAX(player->mo->pitch, player->MinPitch);
					if (player->mo->pitch > oldpitch)
					{
						player->mo->pitch = player->MinPitch;
					}
				}
				else
				{ // look down
					player->mo->pitch = MIN(player->mo->pitch, player->MaxPitch);
					if (player->mo->pitch < oldpitch)
					{
						player->mo->pitch = player->MaxPitch;
					}
				}
			}
		}
	}
	if (player->centering)
	{
		if (abs(player->mo->pitch) > 2*ANGLE_1)
		{
			player->mo->pitch = FixedMul(player->mo->pitch, FRACUNIT*2/3);
		}
		else
		{
			player->mo->pitch = 0;
			player->centering = false;
			if (player - players == consoleplayer)
			{
				LocalViewPitch = 0;
			}
		}
	}

	// [RH] Check for fast turn around
	if (cmd->ucmd.buttons & BT_TURN180 && !(player->oldbuttons & BT_TURN180))
	{
		player->turnticks = TURN180_TICKS;
	}

	// Handle movement
	if (player->mo->reactiontime)
	{ // Player is frozen
		player->mo->reactiontime--;
	}
	else
	{
		P_MovePlayer (player);

		// [RH] check for jump
		if (cmd->ucmd.buttons & BT_JUMP)
		{
			if (player->crouchoffset != 0)
			{
				// Jumping while crouching will force an un-crouch but not jump
				player->crouching = 1;
			}
			else if (player->mo->waterlevel >= 2)
			{
				player->mo->velz = FixedMul(4*FRACUNIT, player->mo->Speed);
			}
			else if (player->mo->flags & MF_NOGRAVITY)
			{
				player->mo->velz = 3*FRACUNIT;
			}
			else if (level.IsJumpingAllowed() && player->onground && player->jumpTics == 0)
			{
				fixed_t jumpvelz = player->mo->JumpZ * 35 / TICRATE;

				// [BC] If the player has the high jump power, double his jump velocity.
				if ( player->cheats & CF_HIGHJUMP )	jumpvelz *= 2;

				player->mo->velz += jumpvelz;
				player->mo->flags2 &= ~MF2_ONMOBJ;
				player->jumpTics = -1;
				if (!(player->cheats & CF_PREDICTING))
					S_Sound(player->mo, CHAN_BODY, "*jump", 1, ATTN_NORM);
			}
		}

		if (cmd->ucmd.upmove == -32768)
		{ // Only land if in the air
			if ((player->mo->flags & MF_NOGRAVITY) && player->mo->waterlevel < 2)
			{
				//player->mo->flags2 &= ~MF2_FLY;
				player->mo->flags &= ~MF_NOGRAVITY;
			}
		}
		else if (cmd->ucmd.upmove != 0)
		{
			// Clamp the speed to some reasonable maximum.
			int magnitude = abs (cmd->ucmd.upmove);
			if (magnitude > 0x300)
			{
				cmd->ucmd.upmove = ksgn (cmd->ucmd.upmove) * 0x300;
			}
			if (player->mo->waterlevel >= 2 || (player->mo->flags2 & MF2_FLY) || (player->cheats & CF_NOCLIP2))
			{
				player->mo->velz = FixedMul(player->mo->Speed, cmd->ucmd.upmove << 9);
				if (player->mo->waterlevel < 2 && !(player->mo->flags & MF_NOGRAVITY))
				{
					player->mo->flags2 |= MF2_FLY;
					player->mo->flags |= MF_NOGRAVITY;
					if ((player->mo->velz <= -39 * FRACUNIT) && !(player->cheats & CF_PREDICTING))
					{ // Stop falling scream
						S_StopSound (player->mo, CHAN_VOICE);
					}
				}
			}
			else if (cmd->ucmd.upmove > 0 && !(player->cheats & CF_PREDICTING))
			{
				AInventory *fly = player->mo->FindInventory (NAME_ArtiFly);
				if (fly != NULL)
				{
					player->mo->UseInventory (fly);
				}
			}
		}
	}

	P_CalcHeight (player);

	if (!(player->cheats & CF_PREDICTING))
	{
		P_PlayerOnSpecial3DFloor (player);
		if (player->mo->Sector->special || player->mo->Sector->damage)
		{
			P_PlayerInSpecialSector (player);
		}
		P_PlayerOnSpecialFlat (player, P_GetThingFloorType (player->mo));
		if (player->mo->velz <= -player->mo->FallingScreamMinSpeed &&
			player->mo->velz >= -player->mo->FallingScreamMaxSpeed && !player->morphTics &&
			player->mo->waterlevel == 0)
		{
			int id = S_FindSkinnedSound (player->mo, "*falling");
			if (id != 0 && !S_IsActorPlayingSomething (player->mo, CHAN_VOICE, id))
			{
				S_Sound (player->mo, CHAN_VOICE, id, 1, ATTN_NORM);
			}
		}
		// check for use
		if (cmd->ucmd.buttons & BT_USE)
		{
			if (!player->usedown)
			{
				player->usedown = true;
				if (!P_TalkFacing(player->mo))
				{
					P_UseLines(player);
				}
			}
		}
		else
		{
			player->usedown = false;
		}
		// Morph counter
		if (player->morphTics)
		{
			if (player->chickenPeck)
			{ // Chicken attack counter
				player->chickenPeck -= 3;
			}
			if (!--player->morphTics)
			{ // Attempt to undo the chicken/pig
				P_UndoPlayerMorph (player, player, MORPH_UNDOBYTIMEOUT);
			}
		}
		// Cycle psprites
		P_MovePsprites (player);

		// Other Counters
		if (player->damagecount)
			player->damagecount--;

		if (player->bonuscount)
			player->bonuscount--;

		if (player->hazardcount)
		{
			player->hazardcount--;
			if (!(level.time & 31) && player->hazardcount > 16*TICRATE)
				P_DamageMobj (player->mo, NULL, NULL, 5, NAME_Slime);
		}

		if (player->poisoncount && !(level.time & 15))
		{
			player->poisoncount -= 5;
			if (player->poisoncount < 0)
			{
				player->poisoncount = 0;
			}
			P_PoisonDamage (player, player->poisoner, 1, true);
		}

		// Apply degeneration.
		if (dmflags2 & DF2_YES_DEGENERATION)
		{
			if ((level.time % TICRATE) == 0 && player->health > deh.MaxHealth)
			{
				if (player->health - 5 < deh.MaxHealth)
					player->health = deh.MaxHealth;
				else
					player->health--;

				player->mo->health = player->health;
			}
		}

		// Handle air supply
		//if (level.airsupply > 0)
		{
			if (player->mo->waterlevel < 3 ||
				(player->mo->flags2 & MF2_INVULNERABLE) ||
				(player->cheats & (CF_GODMODE | CF_NOCLIP2)))
			{
				player->mo->ResetAirSupply ();
			}
			else if (player->air_finished <= level.time && !(level.time & 31))
			{
				P_DamageMobj (player->mo, NULL, NULL, 2 + ((level.time-player->air_finished)/TICRATE), NAME_Drowning);
			}
		}
	}
}

void P_PredictPlayer (player_t *player)
{
	int maxtic;

	if (cl_noprediction ||
		singletics ||
		demoplayback ||
		player->mo == NULL ||
		player != &players[consoleplayer] ||
		player->playerstate != PST_LIVE ||
		!netgame ||
		/*player->morphTics ||*/
		(player->cheats & CF_PREDICTING))
	{
		return;
	}

	maxtic = maketic;

	if (gametic == maxtic)
	{
		return;
	}

	// Save original values for restoration later
	PredictionPlayerBackup = *player;

	AActor *act = player->mo;
	memcpy (PredictionActorBackup, &act->x, sizeof(AActor)-((BYTE *)&act->x-(BYTE *)act));

	act->flags &= ~MF_PICKUP;
	act->flags2 &= ~MF2_PUSHWALL;
	player->cheats |= CF_PREDICTING;

	// The ordering of the touching_sectorlist needs to remain unchanged
	// Also store a copy of all previous sector_thinglist nodes
	msecnode_t *mnode = act->touching_sectorlist;
	msecnode_t *snode;
	PredictionSector_sprev_Backup.Clear();
	PredictionTouchingSectorsBackup.Clear ();

	while (mnode != NULL)
	{
		PredictionTouchingSectorsBackup.Push (mnode->m_sector);

		for (snode = mnode->m_sector->touching_thinglist; snode; snode = snode->m_snext)
		{
			if (snode->m_thing == act)
			{
				PredictionSector_sprev_Backup.Push(snode->m_sprev);
				break;
			}
		}

		mnode = mnode->m_tnext;
	}

	// Keep an ordered list off all actors in the linked sector.
	PredictionSectorListBackup.Clear();
	if (!(act->flags & MF_NOSECTOR))
	{
		AActor *link = act->Sector->thinglist;
		
		while (link != NULL)
		{
			PredictionSectorListBackup.Push(link);
			link = link->snext;
		}
	}

	// Blockmap ordering also needs to stay the same, so unlink the block nodes
	// without releasing them. (They will be used again in P_UnpredictPlayer).
	FBlockNode *block = act->BlockNode;

	while (block != NULL)
	{
		if (block->NextActor != NULL)
		{
			block->NextActor->PrevActor = block->PrevActor;
		}
		*(block->PrevActor) = block->NextActor;
		block = block->NextBlock;
	}
	act->BlockNode = NULL;

	for (int i = gametic; i < maxtic; ++i)
	{
		player->cmd = localcmds[i % LOCALCMDTICS];
		P_PlayerThink (player);
		player->mo->Tick ();
	}
}

extern msecnode_t *P_AddSecnode (sector_t *s, AActor *thing, msecnode_t *nextnode);

void P_UnPredictPlayer ()
{
	player_t *player = &players[consoleplayer];

	if (player->cheats & CF_PREDICTING)
	{
		unsigned int i;
		AActor *act = player->mo;
		AActor *savedcamera = player->camera;

		*player = PredictionPlayerBackup;

		// Restore the camera instead of using the backup's copy, because spynext/prev
		// could cause it to change during prediction.
		player->camera = savedcamera;

		act->UnlinkFromWorld();
		memcpy(&act->x, PredictionActorBackup, sizeof(AActor)-((BYTE *)&act->x - (BYTE *)act));

		// Make the sector_list match the player's touching_sectorlist before it got predicted.
		P_DelSeclist(sector_list);
		sector_list = NULL;
		for (i = PredictionTouchingSectorsBackup.Size(); i-- > 0;)
		{
			sector_list = P_AddSecnode(PredictionTouchingSectorsBackup[i], act, sector_list);
		}

		// The blockmap ordering needs to remain unchanged, too. Right now, act has the right
		// pointers, so temporarily set its MF_NOBLOCKMAP flag so that LinkToWorld() does not
		// mess with them.
		{
			DWORD keepflags = act->flags;
			act->flags |= MF_NOBLOCKMAP;
			act->LinkToWorld();
			act->flags = keepflags;
		}

		// Restore sector links.
		if (!(act->flags & MF_NOSECTOR))
		{
			sector_t *sec = act->Sector;
			AActor *me, *next;
			AActor **link;// , **prev;

			// The thinglist is just a pointer chain. We are restoring the exact same things, so we can NULL the head safely
			sec->thinglist = NULL;

			for (i = PredictionSectorListBackup.Size(); i-- > 0;)
			{
				me = PredictionSectorListBackup[i];
				link = &sec->thinglist;
				next = *link;
				if ((me->snext = next))
					next->sprev = &me->snext;
				me->sprev = link;
				*link = me;
			}

			msecnode_t *snode;

			// Restore sector thinglist order
			for (i = PredictionTouchingSectorsBackup.Size(); i-- > 0;)
			{
				// If we were already the head node, then nothing needs to change
				if (PredictionSector_sprev_Backup[i] == NULL)
					continue;

				for (snode = PredictionTouchingSectorsBackup[i]->touching_thinglist; snode; snode = snode->m_snext)
				{
					if (snode->m_thing == act)
					{
						if (snode->m_sprev)
							snode->m_sprev->m_snext = snode->m_snext;
						else
							snode->m_sector->touching_thinglist = snode->m_snext;
						if (snode->m_snext)
							snode->m_snext->m_sprev = snode->m_sprev;

						snode->m_sprev = PredictionSector_sprev_Backup[i];

						// At the moment, we don't exist in the list anymore, but we do know what our previous node is, so we set its current m_snext->m_sprev to us.
						if (snode->m_sprev->m_snext)
							snode->m_sprev->m_snext->m_sprev = snode;
						snode->m_snext = snode->m_sprev->m_snext;
						snode->m_sprev->m_snext = snode;
						break;
					}
				}
			}
		}

		// Now fix the pointers in the blocknode chain
		FBlockNode *block = act->BlockNode;

		while (block != NULL)
		{
			*(block->PrevActor) = block;
			if (block->NextActor != NULL)
			{
				block->NextActor->PrevActor = &block->NextActor;
			}
			block = block->NextBlock;
		}
	}
}

void player_t::Serialize (FArchive &arc)
{
	int i;
	FString skinname;

	arc << cls
		<< mo
		<< camera
		<< playerstate
		<< cmd;
	if (arc.IsLoading())
	{
		ReadUserInfo(arc, userinfo, skinname);
	}
	else
	{
		WriteUserInfo(arc, userinfo);
	}
	arc << DesiredFOV << FOV
		<< viewz
		<< viewheight
		<< deltaviewheight
		<< bob
		<< velx
		<< vely
		<< centering
		<< health
		<< inventorytics
		<< backpack
		<< fragcount
		<< spreecount
		<< multicount
		<< lastkilltime
		<< ReadyWeapon << PendingWeapon
		<< cheats
		<< refire
		<< inconsistant
		<< killcount
		<< itemcount
		<< secretcount
		<< damagecount
		<< bonuscount
		<< hazardcount
		<< poisoncount
		<< poisoner
		<< attacker
		<< extralight
		<< fixedcolormap << fixedlightlevel
		<< morphTics
		<< MorphedPlayerClass
		<< MorphStyle
		<< MorphExitFlash
		<< PremorphWeapon
		<< chickenPeck
		<< jumpTics
		<< respawn_time
		<< air_finished
		<< turnticks
		<< oldbuttons
		<< isbot
		<< BlendR
		<< BlendG
		<< BlendB
		<< BlendA;
	if (SaveVersion < 3427)
	{
		WORD oldaccuracy, oldstamina;
		arc << oldaccuracy << oldstamina;
		if (mo != NULL)
		{
			mo->accuracy = oldaccuracy;
			mo->stamina = oldstamina;
		}
	}
	if (SaveVersion < 4041)
	{
		// Move weapon state flags from cheats and into WeaponState.
		WeaponState = ((cheats >> 14) & 1) | ((cheats & (0x37 << 24)) >> (24 - 1));
		cheats &= ~((1 << 14) | (0x37 << 24));
	}
	else
	{
		arc << WeaponState;
	}
	arc << LogText
		<< ConversationNPC
		<< ConversationPC
		<< ConversationNPCAngle
		<< ConversationFaceTalker;

	for (i = 0; i < MAXPLAYERS; i++)
		arc << frags[i];
	for (i = 0; i < NUMPSPRITES; i++)
		arc << psprites[i];

	arc << CurrentPlayerClass;

	arc << crouchfactor
		<< crouching 
		<< crouchdir
		<< crouchviewdelta
		<< original_cmd
		<< original_oldbuttons;

	if (SaveVersion >= 3475)
	{
		arc << poisontype << poisonpaintype;
	}
	else if (poisoner != NULL)
	{
		poisontype = poisoner->DamageType;
		poisonpaintype = poisoner->PainType != NAME_None ? poisoner->PainType : poisoner->DamageType;
	}

	if (SaveVersion >= 3599)
	{
		arc << timefreezer;
	}
	else
	{
		cheats &= ~(1 << 15);	// make sure old CF_TIMEFREEZE bit is cleared
	}
	if (SaveVersion < 3640)
	{
		cheats &= ~(1 << 17);	// make sure old CF_REGENERATION bit is cleared
	}
	if (SaveVersion >= 3780)
	{
		arc << settings_controller;
	}
	else
	{
		settings_controller = (this - players == Net_Arbitrator);
	}
	if (SaveVersion >= 4505)
	{
		arc << onground;
	}
	else
	{
		onground = (mo->z <= mo->floorz) || (mo->flags2 & MF2_ONMOBJ) || (mo->BounceFlags & BOUNCE_MBF) || (cheats & CF_NOCLIP2);
	}

	if (isbot)
	{
		arc	<< angle
			<< dest
			<< prev
			<< enemy
			<< missile
			<< mate
			<< last_mate
			<< skill
			<< t_active
			<< t_respawn
			<< t_strafe
			<< t_react
			<< t_fight
			<< t_roam
			<< t_rocket
			<< first_shot
			<< sleft
			<< allround
			<< oldx
			<< oldy;
	}
	else
	{
		dest = prev = enemy = missile = mate = last_mate = NULL;
	}
	if (arc.IsLoading ())
	{
		// If the player reloaded because they pressed +use after dying, we
		// don't want +use to still be down after the game is loaded.
		oldbuttons = ~0;
		original_oldbuttons = ~0;
	}
	if (skinname.IsNotEmpty())
	{
		userinfo.SkinChanged(skinname, CurrentPlayerClass);
	}
}


static FPlayerColorSetMap *GetPlayerColors(FName classname)
{
	const PClass *cls = PClass::FindClass(classname);

	if (cls != NULL)
	{
		FActorInfo *inf = cls->ActorInfo;

		if (inf != NULL)
		{
			return inf->ColorSets;
		}
	}
	return NULL;
}

FPlayerColorSet *P_GetPlayerColorSet(FName classname, int setnum)
{
	FPlayerColorSetMap *map = GetPlayerColors(classname);
	if (map == NULL)
	{
		return NULL;
	}
	return map->CheckKey(setnum);
}

static int STACK_ARGS intcmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

void P_EnumPlayerColorSets(FName classname, TArray<int> *out)
{
	out->Clear();
	FPlayerColorSetMap *map = GetPlayerColors(classname);
	if (map != NULL)
	{
		FPlayerColorSetMap::Iterator it(*map);
		FPlayerColorSetMap::Pair *pair;

		while (it.NextPair(pair))
		{
			out->Push(pair->Key);
		}
		qsort(&(*out)[0], out->Size(), sizeof(int), intcmp);
	}
}

bool P_IsPlayerTotallyFrozen(const player_t *player)
{
	return
		gamestate == GS_TITLELEVEL ||
		player->cheats & CF_TOTALLYFROZEN ||
		((level.flags2 & LEVEL2_FROZEN) && player->timefreezer == 0);
}
