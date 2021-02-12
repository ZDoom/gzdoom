// Temp Small Flame --------------------------------------------------------

class FlameSmallTemp : Actor
{
	Default
	{
		+NOTELEPORT
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		FFSM AB 3 Bright;
		FFSM C 2 Bright A_CountdownArg(0);
		FFSM C 2 Bright;
		FFSM D 3 Bright;
		FFSM E 3 Bright A_CountdownArg(0);
		Loop;
	}
}


// Temp Large Flame ---------------------------------------------------------

class FlameLargeTemp : Actor
{
	Default
	{
		+NOTELEPORT
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		FFLG A 4 Bright;
		FFLG B 4 Bright A_CountdownArg(0);
		FFLG C 4 Bright;
		FFLG D 4 Bright A_CountdownArg(0);
		FFLG E 4 Bright;
		FFLG F 4 Bright A_CountdownArg(0);
		FFLG G 4 Bright;
		FFLG H 4 Bright A_CountdownArg(0);
		FFLG I 4 Bright;
		FFLG J 4 Bright A_CountdownArg(0);
		FFLG K 4 Bright;
		FFLG L 4 Bright A_CountdownArg(0);
		FFLG M 4 Bright;
		FFLG N 4 Bright A_CountdownArg(0);
		FFLG O 4 Bright;
		FFLG P 4 Bright A_CountdownArg(0);
		Goto Spawn+4;
	}
}

// Small Flame --------------------------------------------------------------

class FlameSmall : SwitchableDecoration
{
	Default
	{
		+NOTELEPORT
		+INVISIBLE
		+ZDOOMTRANS
		Radius 15;
		RenderStyle "Add";
	}

	States
	{
	Active:
		FFSM A 0 Bright A_StartSound("Ignite");
	Spawn:
		FFSM A 3 Bright;
		FFSM A 3 Bright A_UnHideThing;
		FFSM ABCDE 3 Bright;
		Goto Spawn+2;
	Inactive:
		FFSM A 2;
		FFSM B 2 A_HideThing;
		FFSM C 200;
		Wait;
	}
}

class FlameSmall2 : FlameSmall
{
}	

// Large Flame --------------------------------------------------------------

class FlameLarge : SwitchableDecoration
{
	Default
	{
		+NOTELEPORT
		+INVISIBLE
		+ZDOOMTRANS
		Radius 15;
		RenderStyle "Add";
	}

	States
	{
	Active:
		FFLG A 0 Bright A_StartSound("Ignite");
	Spawn:
		FFLG A 2 Bright;
		FFLG A 2 Bright A_UnHideThing;
		FFLG ABCDEFGHIJKLMNOP 4 Bright;
		Goto Spawn+6;
	Inactive:
		FFLG DCB 2;
		FFLG A 2 A_HideThing;
		FFLG A 200;
		Wait;
	}
}
		
class FlameLarge2 : FlameLarge
{
}

