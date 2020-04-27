
struct AutoUseHealthInfo play
{
	Array<Inventory> collectedItems[2];
	int collectedHealth[2];
	
	void AddItemToList(Inventory item, int list)
	{
		collectedItems[list].Push(item);
		collectedHealth[list] += Item.Amount * Item.health;
	}
	
	int UseHealthItems(int list, in out int saveHealth)
	{
		int saved = 0;

		while (collectedItems[list].Size() > 0 && saveHealth > 0)
		{
			int maxhealth = 0;
			int index = -1;

			// Find the largest item in the list
			for(int i = 0; i < collectedItems[list].Size(); i++)
			{
				// Workaround for a deficiency in the expression resolver.
				let item = list == 0? collectedItems[0][i] : collectedItems[1][i];
				if (Item.health > maxhealth)
				{
					index = i;
					maxhealth = Item.health;
				}
			}

			// Now apply the health items, using the same logic as Heretic and Hexen.
			int count = (saveHealth + maxhealth-1) / maxhealth;
			for(int i = 0; i < count; i++)
			{
				saved += maxhealth;
				saveHealth -= maxhealth;
				let item = list == 0? collectedItems[0][index] : collectedItems[1][index];
				if (--item.Amount == 0)
				{
					item.DepleteOrDestroy ();
					collectedItems[list].Delete(index);
					break;
				}
			}
		}
		return saved;
	}
	
}

extend class PlayerPawn
{
	
	//===========================================================================
	//
	// 
	//
	//===========================================================================

	ui void InvNext()
	{
		Inventory next;

		let old = InvSel;
		if (InvSel != NULL)
		{
			if ((next = InvSel.NextInv()) != NULL)
			{
				InvSel = next;
			}
			else
			{
				// Select the first item in the inventory
				InvSel = FirstInv();
			}
			if (InvSel) InvSel.DisplayNameTag();
		}
		player.inventorytics = 5*TICRATE;
		if (old != InvSel)
		{
			A_StartSound("misc/invchange", CHAN_AUTO, CHANF_DEFAULT, 1., ATTN_NONE);
		}
	}

	//===========================================================================
	//
	// PlayerPawn :: InvPrev
	//
	//===========================================================================

	ui void InvPrev()
	{
		Inventory item, newitem;

		let old = InvSel;
		if (InvSel != NULL)
		{
			if ((item = InvSel.PrevInv()) != NULL)
			{
				InvSel = item;
			}
			else
			{
				// Select the last item in the inventory
				item = InvSel;
				while ((newitem = item.NextInv()) != NULL)
				{
					item = newitem;
				}
				InvSel = item;
			}
			if (InvSel) InvSel.DisplayNameTag();
		}
		player.inventorytics = 5*TICRATE;
		if (old != InvSel)
		{
			A_StartSound("misc/invchange", CHAN_AUTO, CHANF_DEFAULT, 1., ATTN_NONE);
		}
	}
	
	//===========================================================================
	//
	// PlayerPawn :: AddInventory
	//
	//===========================================================================

	override void AddInventory (Inventory item)
	{
		// Adding inventory to a voodoo doll should add it to the real player instead.
		if (player != NULL && player.mo != self && player.mo != NULL)
		{
			player.mo.AddInventory (item);
			return;
		}
		Super.AddInventory (item);

		// If nothing is selected, select this item.
		if (InvSel == NULL && item.bInvBar)
		{
			InvSel = item;
		}
	}

	//===========================================================================
	//
	// PlayerPawn :: RemoveInventory
	//
	//===========================================================================

	override void RemoveInventory (Inventory item)
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
				InvSel = item.NextInv ();
				if (InvSel == NULL)
				{
					InvSel = item.PrevInv ();
				}
			}
			if (InvFirst == item)
			{
				InvFirst = item.NextInv ();
				if (InvFirst == NULL)
				{
					InvFirst = item.PrevInv ();
				}
			}
			if (item == player.PendingWeapon)
			{
				player.PendingWeapon = WP_NOCHANGE;
			}
			if (item == player.ReadyWeapon)
			{
				// If the current weapon is removed, clear the refire counter and pick a new one.
				pickWeap = true;
				player.ReadyWeapon = NULL;
				player.refire = 0;
			}
		}
		Super.RemoveInventory (item);
		if (pickWeap && player.mo == self && player.PendingWeapon == WP_NOCHANGE)
		{
			PickNewWeapon (NULL);
		}
	}

	//===========================================================================
	//
	// PlayerPawn :: UseInventory
	//
	//===========================================================================

	override bool UseInventory (Inventory item)
	{
		let itemtype = item.GetClass();

		if (player.cheats & CF_TOTALLYFROZEN)
		{ // You can't use items if you're totally frozen
			return false;
		}
		if (isFrozen())
		{
			// Time frozen
			return false;
		}

		if (!Super.UseInventory (item))
		{
			// Heretic and Hexen advance the inventory cursor if the use failed.
			// Should this behavior be retained?
			return false;
		}
		if (player == players[consoleplayer])
		{
			A_StartSound(item.UseSound, CHAN_ITEM);
 			StatusBar.FlashItem (itemtype);	// Fixme: This shouldn't be called from here, because it is in the UI.
		}
		return true;
	}
	
	//---------------------------------------------------------------------------
	//
	// PROC P_AutoUseHealth
	//
	//---------------------------------------------------------------------------

	void AutoUseHealth(int saveHealth)
	{
		AutoUseHealthInfo collector;
		
		for(Inventory inv = self.Inv; inv != NULL; inv = inv.Inv)
		{
			let hp = HealthPickup(inv);
			if (hp && hp.Amount > 0)
			{
				int mode = hp.autousemode;
				if (mode == 1 || mode == 2) collector.AddItemToList(inv, mode-1);
			}
		}

		bool skilluse = !!G_SkillPropertyInt(SKILLP_AutoUseHealth);

		if (skilluse && collector.collectedHealth[0] >= saveHealth)
		{ 
			// Use quartz flasks
			player.health += collector.UseHealthItems(0, saveHealth);
		}
		else if (collector.collectedHealth[1] >= saveHealth)
		{ 
			// Use mystic urns
			player.health += collector.UseHealthItems(1, saveHealth);
		}
		else if (skilluse && collector.collectedHealth[0] + collector.collectedHealth[1] >= saveHealth)
		{ 
			// Use mystic urns and quartz flasks
			player.health += collector.UseHealthItems(0, saveHealth);
			if (saveHealth > 0) player.health += collector.UseHealthItems(1, saveHealth);
		}
		health = player.health;
	}

	//============================================================================
	//
	// P_AutoUseStrifeHealth
	//
	//============================================================================

	void AutoUseStrifeHealth ()
	{
		Array<Inventory> Items;

		for(Inventory inv = self.Inv; inv != NULL; inv = inv.Inv)
		{
			let hp = HealthPickup(inv);
			if (hp && hp.Amount > 0)
			{
				if (hp.autousemode == 3) Items.Push(inv);
			}
		}
		
		if (!sv_disableautohealth)
		{
			while (Items.Size() > 0)
			{
				int maxhealth = 0;
				int index = -1;

				// Find the largest item in the list
				for(int i = 0; i < Items.Size(); i++)
				{
					if (Items[i].health > maxhealth)
					{
						index = i;
						maxhealth = Items[i].Amount;
					}
				}

				while (player.health < 50)
				{
					let item = Items[index];
					if (item == null || !UseInventory (item))
						break;
				}
				if (player.health >= 50) return;
				// Using all of this item was not enough so delete it and restart with the next best one
				Items.Delete(index);
			}
		}
	}

	//============================================================================
	//
	// Helper for 'useflechette' CCMD.
	//
	//============================================================================

	protected virtual Inventory GetFlechetteItem() 
	{
		// Select from one of arti_poisonbag1-3, whichever the player has
		static const Class<Inventory> bagtypes[] = { 
			"ArtiPoisonBag3",	// use type 3 first because that's the default when the player has none specified.
			"ArtiPoisonBag1",
			"ArtiPoisonBag2"
		};

		if (FlechetteType != NULL)
		{
			let item = FindInventory(FlechetteType);
			if (item != null)
			{
				return item;
			}
		}

		// The default flechette could not be found, or the player had no default. Try all 3 types then.
		for (int j = 0; j < 3; ++j)
		{
			let item = FindInventory(bagtypes[j]);
			if (item != null)
			{
				return item;
			}
		}
		return null;
	}
	
}