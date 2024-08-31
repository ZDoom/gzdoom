
class TeleportFog : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOTELEPORT
		+NOGRAVITY
		+ZDOOMTRANS
		RenderStyle "Add";
	}
	States
	{
	Spawn:
		TFOG ABABCDEFGHIJ 6 Bright;
		Stop;
	
	Raven:
		TELE ABCDEFGHGFEDC 6 Bright;
		Stop;

	Strife:
		TFOG ABCDEFEDCB 6 Bright;
		Stop;
	}
	
	override void PostBeginPlay ()
	{
		Super.PostBeginPlay ();
		A_StartSound ("misc/teleport", CHAN_BODY);
		switch (gameinfo.gametype)
		{
		case GAME_Hexen:
		case GAME_Heretic:
			SetStateLabel("Raven");
			break;

		case GAME_Strife:
			SetStateLabel("Strife");
			break;
			
		default:
			break;
		}
	}
	
}



class TeleportDest : Actor
{
	default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+DONTSPLASH
		+NOTONAUTOMAP
	}
}

class TeleportDest2 : TeleportDest
{
	default
	{
		+NOGRAVITY
	}
}

class TeleportDest3 : TeleportDest2
{
	default
	{
		-NOGRAVITY
	}
}

