// Light probes provide ZDRay points of light to sample lighting data from.
// Will despawn from the world as they serve no purpose at runtime.
// Currently used to light Actor sprites.

class LightProbe : Actor native
{
	States
	{
	Spawn:
		LPRO A 0;
		Stop;
	}
}

// Dummy Actors used to hold UDMF keys that ZDRay will parse lighting data from.
// These will simply despawn from the world at map start, as they serve no purpose at runtime.
// Currently only covers the basic point and spot lights.
// Lightmaps currently do not animate, so the various animated lights (pulsating, flickering etc) are skipped for now.

class StaticLightBase : Actor
{
	States
	{
	Spawn:
		TNT1 A 0;
		Stop;
	}
}

class PointLightStatic : StaticLightBase {}
class SpotLightStatic : StaticLightBase {}

// Sunlight Actors to define sunlight properties for the current map

class ZDRaySunlight : Actor {}
