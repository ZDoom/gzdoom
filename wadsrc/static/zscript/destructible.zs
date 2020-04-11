struct HealthGroup native play
{
    deprecated("3.8", "Use Level.FindHealthGroup() instead") static clearscope HealthGroup Find(int id)
	{
		return level.FindHealthGroup(id);
	}
    
    readonly int id;
    readonly int health;
    readonly Array<Sector> sectors;
    readonly Array<Line> lines;
    
    native void SetHealth(int newhealth);
}

enum SectorPart
{
    SECPART_None = -1,
    SECPART_Floor = 0,
    SECPART_Ceiling = 1,
    SECPART_3D = 2
}

struct Destructible native play
{
   
    static native void DamageSector(Sector sec, Actor source, int damage, Name damagetype, SectorPart part, vector3 position, bool isradius);
    static native void DamageLinedef(Line def, Actor source, int damage, Name damagetype, int side, vector3 position, bool isradius);
    
    static native void GeometryLineAttack(TraceResults trace, Actor thing, int damage, Name damagetype);
    static native void GeometryRadiusAttack(Actor bombspot, Actor bombsource, int bombdamage, int bombdistance, Name damagetype, int fulldamagedistance);
    static native bool ProjectileHitLinedef(Actor projectile, Line def);
    static native bool ProjectileHitPlane(Actor projectile, SectorPart part);
    
    static clearscope native bool CheckLinedefVulnerable(Line def, int side, SectorPart part);
    static clearscope native bool CheckSectorVulnerable(Sector sec, SectorPart part);
}