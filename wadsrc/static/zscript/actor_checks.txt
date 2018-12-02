extend class Actor
{
	//==========================================================================
	//
	// This file contains all the A_Jump* checker functions, all split up
	// into an actual checker and a simple wrapper around ResolveState.
	//
	//==========================================================================
	
	action state A_JumpIf(bool expression, statelabel label)
	{
		return expression? ResolveState(label) : null;
	}

	
	//==========================================================================
	//
	// 
	//
	//==========================================================================
	
	action state A_JumpIfHealthLower(int health, statelabel label, int ptr_selector = AAPTR_DEFAULT)
	{
		Actor aptr = GetPointer(ptr_selector);
		return aptr && aptr.health < health? ResolveState(label) : null;
	}
	
	//==========================================================================
	//
	// 
	//
	//==========================================================================

	bool CheckIfCloser(Actor targ, double dist, bool noz = false)
	{
		if (!targ) return false;
		return 
			(Distance2D(targ) < dist && (noz || 
			 ((pos.z > targ.pos.z && pos.z - targ.pos.z - targ.height < dist) ||
			  (pos.z <= targ.pos.z && targ.pos.z - pos.z - height < dist)
			 )
			));
	}

	action state A_JumpIfCloser(double distance, statelabel label, bool noz = false)
	{
		Actor targ;

		if (player == NULL)
		{
			targ = target;
		}
		else
		{
			// Does the player aim at something that can be shot?
			targ = AimTarget();
		}
		
		return CheckIfCloser(targ, distance, noz)? ResolveState(label) : null;
	}
	
	action state A_JumpIfTracerCloser(double distance, statelabel label, bool noz = false)
	{
		return CheckIfCloser(tracer, distance, noz)? ResolveState(label) : null;
	}
	
	action state A_JumpIfMasterCloser(double distance, statelabel label, bool noz = false)
	{
		return CheckIfCloser(master, distance, noz)? ResolveState(label) : null;
	}
	
	//==========================================================================
	//
	// 
	//
	//==========================================================================

	action state A_JumpIfTargetOutsideMeleeRange(statelabel label)
	{
		return CheckMeleeRange()? null : ResolveState(label);
	}
	
	action state A_JumpIfTargetInsideMeleeRange(statelabel label)
	{
		return CheckMeleeRange()? ResolveState(label) : null;
	}
	
	//==========================================================================
	//
	// 
	//
	//==========================================================================

	action state A_JumpIfInventory(class<Inventory> itemtype, int itemamount, statelabel label, int owner = AAPTR_DEFAULT)
	{
		return CheckInventory(itemtype, itemamount, owner)? ResolveState(label) : null;
	}

	action state A_JumpIfInTargetInventory(class<Inventory> itemtype, int amount, statelabel label, int forward_ptr = AAPTR_DEFAULT)
	{
		if (target == null) return null;
		return target.CheckInventory(itemtype, amount, forward_ptr)? ResolveState(label) : null;
	}
	
	//==========================================================================
	//
	// rather pointless these days to do it this way.
	//
	//==========================================================================

	bool CheckArmorType(name Type, int amount = 1)
	{
		let myarmor = BasicArmor(FindInventory("BasicArmor"));
		return myarmor != null && myarmor.ArmorType == type && myarmor.Amount >= amount;
	}

	action state A_JumpIfArmorType(name Type, statelabel label, int amount = 1)
	{
		return CheckArmorType(Type, amount)? ResolveState(label) : null;
	}
	
	//==========================================================================
	//
	// 
	//
	//==========================================================================

	native bool CheckIfSeen();
	native bool CheckSightOrRange(double distance, bool two_dimension = false);
	native bool CheckRange(double distance, bool two_dimension = false);
	
	action state A_CheckSight(statelabel label)
	{
		return CheckIfSeen()? ResolveState(label) : null;
	}

	action state A_CheckSightOrRange(double distance, statelabel label, bool two_dimension = false)
	{
		return CheckSightOrRange(distance, two_dimension)? ResolveState(label) : null;
	}
	
	action state A_CheckRange(double distance, statelabel label, bool two_dimension = false)
	{
		return CheckRange(distance, two_dimension)? ResolveState(label) : null;
	}
	
	//==========================================================================
	//
	// 
	//
	//==========================================================================

	action state A_CheckFloor(statelabel label)
	{
		return pos.z <= floorz? ResolveState(label) : null;
	}
	
	action state A_CheckCeiling(statelabel label)
	{
		return pos.z + height >= ceilingz? ResolveState(label) : null;
	}
	
	//==========================================================================
	//
	// since this is deprecated the checker is private.
	//
	//==========================================================================

	private native bool CheckFlag(string flagname, int check_pointer = AAPTR_DEFAULT);

	deprecated("2.3") action state A_CheckFlag(string flagname, statelabel label, int check_pointer = AAPTR_DEFAULT)
	{
		return CheckFlag(flagname, check_pointer)? ResolveState(label) : null;
	}

	//==========================================================================
	//
	// 
	//
	//==========================================================================

	native bool PlayerSkinCheck();
	
	action state A_PlayerSkinCheck(statelabel label)
	{
		return PlayerSkinCheck()? ResolveState(label) : null;
	}

	//==========================================================================
	//
	// 
	//
	//==========================================================================

	action state A_CheckSpecies(statelabel label, name species = 'none', int ptr = AAPTR_DEFAULT)
	{
		Actor aptr = GetPointer(ptr);
		return aptr && aptr.GetSpecies() == species? ResolveState(label) : null;
	}
	
	//==========================================================================
	//
	// 
	//
	//==========================================================================

	native bool CheckLOF(int flags = 0, double range = 0, double minrange = 0, double angle = 0, double pitch = 0, double offsetheight = 0, double offsetwidth = 0, int ptr_target = AAPTR_DEFAULT, double offsetforward = 0);
	native bool CheckIfTargetInLOS (double fov = 0, int flags = 0, double dist_max = 0, double dist_close = 0);
	native bool CheckIfInTargetLOS (double fov = 0, int flags = 0, double dist_max = 0, double dist_close = 0);
	native bool CheckProximity(class<Actor> classname, double distance, int count = 1, int flags = 0, int ptr = AAPTR_DEFAULT);
	native bool CheckBlock(int flags = 0, int ptr = AAPTR_DEFAULT, double xofs = 0, double yofs = 0, double zofs = 0, double angle = 0);

	action state A_CheckLOF(statelabel label, int flags = 0, double range = 0, double minrange = 0, double angle = 0, double pitch = 0, double offsetheight = 0, double offsetwidth = 0, int ptr_target = AAPTR_DEFAULT, double offsetforward = 0)
	{
		return CheckLOF(flags, range, minrange, angle, pitch, offsetheight, offsetwidth, ptr_target, offsetforward)? ResolveState(label) : null;
	}
	
	action state A_JumpIfTargetInLOS (statelabel label, double fov = 0, int flags = 0, double dist_max = 0, double dist_close = 0)
	{
		return CheckIfTargetInLOS(fov, flags, dist_max, dist_close)? ResolveState(label) : null;
	}
	
	action state A_JumpIfInTargetLOS (statelabel label, double fov = 0, int flags = 0, double dist_max = 0, double dist_close = 0)
	{
		return CheckIfInTargetLOS(fov, flags, dist_max, dist_close)? ResolveState(label) : null;
	}
	
	action state A_CheckProximity(statelabel label, class<Actor> classname, double distance, int count = 1, int flags = 0, int ptr = AAPTR_DEFAULT)
	{
		// This one was doing some weird stuff that needs to be preserved.
		state jumpto = ResolveState(label);
		if (!jumpto)
		{
			if (!(flags & (CPXF_SETTARGET | CPXF_SETMASTER | CPXF_SETTRACER)))
			{
				return null;
			}
		}
		return CheckProximity(classname, distance, count, flags, ptr)? jumpto : null;
	}
	
	action state A_CheckBlock(statelabel label, int flags = 0, int ptr = AAPTR_DEFAULT, double xofs = 0, double yofs = 0, double zofs = 0, double angle = 0)
	{
		return CheckBlock(flags, ptr, xofs, yofs, zofs, angle)? ResolveState(label) : null;
	}

	//===========================================================================
	// A_JumpIfHigherOrLower
	//
	// Jumps if a target, master, or tracer is higher or lower than the calling 
	// actor. Can also specify how much higher/lower the actor needs to be than 
	// itself. Can also take into account the height of the actor in question,
	// depending on which it's checking. This means adding height of the
	// calling actor's self if the pointer is higher, or height of the pointer 
	// if its lower.
	//===========================================================================
	
	action state A_JumpIfHigherOrLower(statelabel high, statelabel low, double offsethigh = 0, double offsetlow = 0, bool includeHeight = true, int ptr = AAPTR_TARGET)
	{
		Actor mobj = GetPointer(ptr);

		if (mobj != null && mobj != self) //AAPTR_DEFAULT is completely useless in this regard.
		{
			if ((high) && (mobj.pos.z > ((includeHeight ? Height : 0) + pos.z + offsethigh)))
			{
				return ResolveState(high);
			}
			else if ((low) && (mobj.pos.z + (includeHeight ? mobj.Height : 0)) < (pos.z + offsetlow))
			{
				return ResolveState(low);
			}
		}
		return null;
	}
	
}