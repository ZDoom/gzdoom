class Decal : Actor
{
	native void SpawnDecal();
	
	override void BeginPlay()
	{
		Super.BeginPlay();
		SpawnDecal();
		Destroy();
	}
}
