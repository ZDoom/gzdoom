// Map Marker --------------------------------------------------------------
//
// This class uses the following argument:
//   args[0] == 0, shows the sprite at this actor
//           != 0, shows the sprite for all actors whose TIDs match instead
//
//   args[1] == 0, show the sprite always
//           == 1, show the sprite only after its sector has been drawn
//
//   args[2] == 0, show the sprite with a constant scale
//           == 1, show the sprite with a scale relative to automap zoom
//
// To enable display of the sprite, activate it. To turn off the sprite,
// deactivate it.
//
// All the code to display it is in am_map.cpp.
//
//--------------------------------------------------------------------------

class MapMarker : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
		+INVISIBLE
		Scale 0.5;
	}
	States
	{
	Spawn:
		AMRK A -1;
		Stop;
	}
	
	override void BeginPlay ()
	{
		ChangeStatNum (STAT_MAPMARKER);
	}

	override void Activate (Actor activator)
	{
		bDormant = true;
	}

	override void Deactivate (Actor activator)
	{
		bDormant = false;
	}
	
}
