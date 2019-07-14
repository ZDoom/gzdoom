
class Spark : Actor
{
	default
	{
		+NOSECTOR
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
	}
	
	override void Activate (Actor activator)
	{
		Super.Activate (activator);
		DrawSplash (args[0] ? args[0] : 32, Angle, 1);
		A_PlaySound ("world/spark", CHAN_AUTO, 1, false, ATTN_STATIC);
	}
	
}