// 'None' must always be the first name.
xx(None)

xx(Super)
xx(Object)

// Hexen sound sequence names
xx(Platform)
xx(PlatformMetal)
xx(Silence)
xx(Lava)
xx(Water)
xx(Ice)
xx(Earth)
xx(PlatformMetal2)
xx(DoorNormal)
xx(DoorHeavy)
xx(DoorMetal)
xx(DoorCreak)
xx(DoorMetal2)
xx(Wind)

// Special bosses A_BossDeath knows about
xx(Fatso)
xx(Arachnotron)
xx(BaronOfHell)
xx(Cyberdemon)
xx(SpiderMastermind)
xx(Ironlich)
xx(Minotaur)
xx(Sorcerer2)

// P_SpawnMapThing checks for these as health items (I smell a FIXME)
xx(Berserk)
xx(Soulsphere)
xx(Megasphere)		// also counts as armor for P_SpawnMapThing

// Standard player classes
xx(DoomPlayer)
xx(HereticPlayer)
xx(StrifePlayer)
xx(FighterPlayer)
xx(ClericPlayer)
xx(MagePlayer)
xx(ChickenPlayer)
xx(PigPlayer)

// Flechette names for the different Hexen player classes
xx(ArtiPoisonBag1)
xx(ArtiPoisonBag2)
xx(ArtiPoisonBag3)

// Strife quests
xx(QuestItem)

// Auto-usable health items
xx(ArtiHealth)
xx(ArtiSuperHealth)
xx(MedicalKit)
xx(MedPatch)

// The Wings of Wrath
xx(ArtiFly)

// Doom ammo types
xx(Clip)
xx(Shell)
xx(RocketAmmo)
xx(Cell)

// Weapon names for the Strife status bar
xx(StrifeCrossbow)
xx(AssaultGun)
xx(FlameThrower)
xx(MiniMissileLauncher)
xx(StrifeGrenadeLauncher)
xx(Mauler)

xx(Chicken)
xx(Pig)


// Damage types
xx(Fire)
//xx(Ice)	already defined above
xx(Disintegrate)
#if 0
xx(Water)
xx(Slime)
xx(Crush)
xx(Telefrag)
xx(Falling)
xx(Suicide)
xx(Exit)
xx(Railgun)
xx(Poison)
xx(Electric)
xx(BFGSplash)
xx(DrainLife)	// A weapon like the Sigil that drains your life away.
xx(Massacre)	// For death by a cheater!
//(Melee)		already defined above, so don't define it again


// Standard animator names.
xx(Spawn)
xx(See)
xx(Pain)
xx(Melee)
xx(Missile)
xx(Crash)
xx(Death)
xx(Raise)
xx(Wound)

// Weapon animator names.
xx(Up)
xx(Down)
xx(Ready)
xx(Flash)
xx(Attack)
xx(HoldAttack)
xx(AltAttack)
xx(AltHoldAttack)

// Special death name for getting killed excessively. Could be used as
// a damage type if you wanted to force an extreme death.
xx(Extreme)

// Compatible death names for the decorate parser.
xx(XDeath)
xx(BDeath)
xx(IDeath)
xx(EDeath)

// State names used by ASwitchableDecoration
xx(Active)
xx(Inactive)
#endif
