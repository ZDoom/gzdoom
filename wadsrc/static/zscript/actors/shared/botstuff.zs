
class CajunBodyNode : Actor
{
	Default
	{
		+NOSECTOR
		+NOGRAVITY
		+INVISIBLE
	}
}

class CajunTrace : Actor
{
	Default
	{
		Speed 12;
		Radius 6;
		Height 8;
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOGRAVITY
		+NOTELEPORT
	}
}

class Bot native
{
	struct ticcmd
	{
		uint	buttons;
		short	pitch;			// up/down
		short	yaw;			// left/right
		short	roll;			// "tilt"
		short	forwardmove;
		short	sidemove;
		short	upmove;
		int16			consistancy;	// checks for net game		
	}

	native Actor dest;
	virtual void Thinker(ticcmd cmd)
	{
		// change cmd.whatever here to what you want the player to do, see struct above
	}
}
