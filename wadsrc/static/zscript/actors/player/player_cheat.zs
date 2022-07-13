/*
** player_cheat.txt
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2006-2017 Christoph Oelckers
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

extend class PlayerPawn
{
	enum EAll
	{ 
		ALL_NO, 
		ALL_YES, 
		ALL_YESYES 
	}
	
	native void CheatSuicide();

	virtual void CheatGive (String name, int amount)
	{
		int i;
		Class<Inventory> type;
		let player = self.player;

		if (player.mo == NULL || player.health <= 0)
		{
			return;
		}

		int giveall = ALL_NO;
		if (name ~== "all")
		{
			giveall = ALL_YES;
		}
		else if (name ~== "everything")
		{
			giveall = ALL_YESYES;
		}

		if (name ~== "health")
		{
			if (amount > 0)
			{
				health += amount;
				player.health = health;
			}
			else
			{
				player.health = health = GetMaxHealth(true);
			}
		}

		if (giveall || name ~== "backpack")
		{
			// Select the correct type of backpack based on the game
			type = (class<Inventory>)(gameinfo.backpacktype);
			if (type != NULL)
			{
				GiveInventory(type, 1, true);
			}

			if (!giveall)
				return;
		}

		if (giveall || name ~== "ammo")
		{
			// Find every unique type of ammo. Give it to the player if
			// he doesn't have it already, and set each to its maximum.
			for (i = 0; i < AllActorClasses.Size(); ++i)
			{
				let ammotype = (class<Ammo>)(AllActorClasses[i]);

				if (ammotype && GetDefaultByType(ammotype).GetParentAmmo() == ammotype)
				{
					let ammoitem = FindInventory(ammotype);
					if (ammoitem == NULL)
					{
						ammoitem = Inventory(Spawn (ammotype));
						ammoitem.AttachToOwner (self);
						ammoitem.Amount = ammoitem.MaxAmount;
					}
					else if (ammoitem.Amount < ammoitem.MaxAmount)
					{
						ammoitem.Amount = ammoitem.MaxAmount;
					}
				}
			}

			if (!giveall)
				return;
		}

		if (giveall || name ~== "armor")
		{
			if (gameinfo.gametype != GAME_Hexen)
			{
				let armoritem = BasicArmorPickup(Spawn("BasicArmorPickup"));
				armoritem.SaveAmount = 100*deh.BlueAC;
				armoritem.SavePercent = gameinfo.Armor2Percent > 0 ? gameinfo.Armor2Percent * 100 : 50;
				if (!armoritem.CallTryPickup (self))
				{
					armoritem.Destroy ();
				}
			}
			else
			{
				for (i = 0; i < 4; ++i)
				{
					let armoritem = Inventory(Spawn("HexenArmor"));
					armoritem.health = i;
					armoritem.Amount = 0;
					if (!armoritem.CallTryPickup (self))
					{
						armoritem.Destroy ();
					}
				}
			}

			if (!giveall)
				return;
		}

		if (giveall || name ~== "keys")
		{
			for (int i = 0; i < AllActorClasses.Size(); ++i)
			{
				if (AllActorClasses[i] is "Key")
				{
					let keyitem = GetDefaultByType (AllActorClasses[i]);
					if (keyitem.special1 != 0)
					{
						let item = Inventory(Spawn(AllActorClasses[i]));
						if (!item.CallTryPickup (self))
						{
							item.Destroy ();
						}
					}
				}
			}
			if (!giveall)
				return;
		}

		if (giveall || name ~== "weapons")
		{
			let savedpending = player.PendingWeapon;
			for (i = 0; i < AllActorClasses.Size(); ++i)
			{
				let type = (class<Weapon>)(AllActorClasses[i]);
				if (type != null && type != "Weapon" && !type.isAbstract())
				{
					// Don't give replaced weapons unless the replacement was done by Dehacked.
					let rep = GetReplacement(type);
					if (rep == type || rep is "DehackedPickup")
					{
						// Give the weapon only if it is set in a weapon slot.
						if (player.weapons.LocateWeapon(type))
						{
							readonly<Weapon> def = GetDefaultByType (type);
							if (giveall == ALL_YESYES || !def.bCheatNotWeapon)
							{
								GiveInventory(type, 1, true);
							}
						}
					}
				}
			}
			player.PendingWeapon = savedpending;

			if (!giveall)
				return;
		}

		if (giveall || name ~== "artifacts")
		{
			for (i = 0; i < AllActorClasses.Size(); ++i)
			{
				type = (class<Inventory>)(AllActorClasses[i]);
				if (type!= null)
				{
					let def = GetDefaultByType (type);
					if (def.Icon.isValid() && (def.MaxAmount > 1 || def.bAutoActivate == false) &&
						!(type is "PuzzleItem") && !(type is "Powerup") && !(type is "Ammo") &&	!(type is "Armor") && !(type is "Key"))
					{
						// Do not give replaced items unless using "give everything"
						if (giveall == ALL_YESYES || GetReplacement(type) == type)
						{
							GiveInventory(type, amount <= 0 ? def.MaxAmount : amount, true);
						}
					}
				}
			}
			if (!giveall)
				return;
		}

		if (giveall || name ~== "puzzlepieces")
		{
			for (i = 0; i < AllActorClasses.Size(); ++i)
			{
				let type = (class<PuzzleItem>)(AllActorClasses[i]);
				if (type != null)
				{
					let def = GetDefaultByType (type);
					if (def.Icon.isValid())
					{
						// Do not give replaced items unless using "give everything"
						if (giveall == ALL_YESYES || GetReplacement(type) == type)
						{
							GiveInventory(type, amount <= 0 ? def.MaxAmount : amount, true);
						}
					}
				}
			}
			if (!giveall)
				return;
		}

		if (giveall)
			return;

		type = name;
		if (type == NULL)
		{
			if (PlayerNumber() == consoleplayer)
				A_Log(String.Format("Unknown item \"%s\"\n", name));
		}
		else
		{
			GiveInventory(type, amount, true);
		}
		return;
	}

	void CheatTakeType(class<Inventory> deletetype)
	{
		for (int i = 0; i < AllActorClasses.Size(); ++i)
		{
			let type = (class<Inventory>)(AllActorClasses[i]);

			if (type != null && type is deletetype)
			{
				let pack = FindInventory(type);
				if (pack) pack.DepleteOrDestroy();
			}
		}
	}
	
	virtual void CheatTake (String name, int amount)
	{
		bool takeall;
		Class<Inventory> type;
		let player = self.player;


		if (player.mo == NULL || player.health <= 0)
		{
			return;
		}

		takeall = name ~== "all";

		if (!takeall && name ~== "health")
		{
			if (player.mo.health - amount <= 0
				|| player.health - amount <= 0
				|| amount == 0)
			{

				CheatSuicide ();

				if (PlayerNumber() == consoleplayer)
					Console.HideConsole ();

				return;
			}

			if (amount > 0)
			{
				if (player.mo)
				{
					player.mo.health -= amount;
					player.health = player.mo.health;
				}
				else
				{
					player.health -= amount;
				}
			}

			if (!takeall)
				return;
		}

		if (takeall || name ~== "backpack")
		{
			CheatTakeType("BackpackItem");
			if (!takeall)
				return;
		}

		if (takeall || name ~== "ammo")
		{
			CheatTakeType("Ammo");
			if (!takeall)
				return;
		}

		if (takeall || name ~== "armor")
		{
			CheatTakeType("Armor");
			if (!takeall)
				return;
		}
		
		if (takeall || name ~== "keys")
		{
			CheatTakeType("Key");
			if (!takeall)
				return;
		}

		if (takeall || name ~== "weapons")
		{
			CheatTakeType("Weapon");
			CheatTakeType("WeaponHolder");
			player.ReadyWeapon = null;
			player.PendingWeapon = WP_NOCHANGE;

			if (!takeall)
				return;
		}

		if (takeall || name ~== "artifacts")
		{
			for (int i = 0; i < AllActorClasses.Size(); ++i)
			{
				type = (class<Inventory>)(AllActorClasses[i]);
				if (type!= null && !(type is "PuzzleItem") && !(type is "Powerup") && !(type is "Ammo") &&	!(type is "Armor") && !(type is "Key") && !(type is "Weapon"))
				{
					let pack = FindInventory(type);
					if (pack) pack.Destroy();
				}
			}
			if (!takeall)
				return;
		}

		if (takeall || name ~== "puzzlepieces")
		{
			CheatTakeType("PuzzleItem");
			if (!takeall)
				return;
		}

		if (takeall)
			return;

		type = name;
		if (type == NULL)
		{
			if (PlayerNumber() == consoleplayer)
				A_Log(String.Format("Unknown item \"%s\"\n", name));
		}
		else
		{
			TakeInventory(type, max(amount, 1));
		}
		return;
	}
	
	virtual void CheatSetInv(String strng, int amount, bool beyond)
	{
		if (!(strng ~== "health"))
		{
			if (amount <= 0)
			{
				CheatSuicide();
				return;
			}
			if (!beyond) amount = MIN(amount, player.mo.GetMaxHealth(true));
			player.health = player.mo.health = amount;
		}
		else
		{
			class<Inventory> item = strng;
			if (item != null)
			{
				player.mo.SetInventory(item, amount, beyond);
				return;
			}
			Console.Printf("Unknown item \"%s\"\n", strng);
		}
	}
	

	virtual String CheatMorph(class<PlayerPawn> morphClass, bool quickundo)
	{
		let oldclass = GetClass();

		// Set the standard morph style for the current game
		int style = MRF_UNDOBYTOMEOFPOWER;
		if (gameinfo.gametype == GAME_Hexen) style |= MRF_UNDOBYCHAOSDEVICE;

		if (player.morphTics)
		{
			if (UndoPlayerMorph (player))
			{
				if (!quickundo && oldclass != morphclass && MorphPlayer (player, morphclass, 0, style))
				{
					return StringTable.Localize("$TXT_STRANGER");
				}
				return StringTable.Localize("$TXT_NOTSTRANGE");
			}
		}
		else if (MorphPlayer (player, morphclass, 0, style))
		{
			return StringTable.Localize("$TXT_STRANGE");
		}
		return "";
	}
	
	virtual void CheatTakeWeaps()
	{
		if (player.morphTics || health <= 0)
		{
			return;
		}
		// Do not mass-delete directly from the linked list. That can cause problems.
		Array<Inventory> collect;
		// Take away all weapons that are either non-wimpy or use ammo.
		for(let item = Inv; item; item = item.Inv)
		{
			let weap = Weapon(item);
			if (weap && (!weap.bWimpy_Weapon || weap.AmmoType1 != null))
			{
				collect.Push(item);
			}
		}
		// Destroy them in a second loop. We have to look out for indirect destructions here as will happen with powered up weapons.
		for(int i = 0; i < collect.Size(); i++)
		{
			let item = collect[i];
			if (item) item.Destroy();
		}
	}
}
