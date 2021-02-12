
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
		A_StartSound ("world/spark", CHAN_AUTO, CHANF_DEFAULT, 1, ATTN_STATIC);
	}
	
}