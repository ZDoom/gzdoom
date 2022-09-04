
class LevelPostProcessor native play
{
	protected native LevelLocals level;

	protected void Apply(Name checksum, String mapname)
	{
	}

	protected native void ClearSectorTags(int sector);
	protected native void AddSectorTag(int sector, int tag);
	protected native void ClearLineIDs(int line);
	protected native void AddLineID(int line, int tag);
	protected native void OffsetSectorPlane(int sector, int plane, double offset);
	protected native void SetSectorPlane(int sector, int plane, vector3 normal, double d);

	const SKILLS_ALL = 31;
	const MODES_ALL = MTF_SINGLE | MTF_COOPERATIVE | MTF_DEATHMATCH;

	protected native uint GetThingCount();
	protected native uint AddThing(int ednum, Vector3 pos, int angle = 0, uint skills = SKILLS_ALL, uint flags = MODES_ALL);

	protected native int GetThingEdNum(uint thing);
	protected native void SetThingEdNum(uint thing, int ednum);

	protected native vector3 GetThingPos(uint thing);
	protected native void SetThingXY(uint thing, double x, double y);
	protected native void SetThingZ(uint thing, double z);

	protected native int GetThingAngle(uint thing);
	protected native void SetThingAngle(uint thing, int angle);

	protected native uint GetThingSkills(uint thing);
	protected native void SetThingSkills(uint thing, uint skills);

	protected native uint GetThingFlags(uint thing);
	protected native void SetThingFlags(uint thing, uint flags);

	protected native int GetThingID(uint thing);
	protected native void SetThingID(uint thing, int id);

	protected native int GetThingSpecial(uint thing);
	protected native void SetThingSpecial(uint thing, int special);

	protected native int GetThingArgument(uint thing, uint index);
	protected native Name GetThingStringArgument(uint thing);
	protected native void SetThingArgument(uint thing, uint index, int value);
	protected native void SetThingStringArgument(uint thing, Name value);

	protected native void SetVertex(uint vertex, double x, double y);
	protected native double, bool GetVertexZ(uint vertex, int plane);
	protected native void SetVertexZ(uint vertex, int plane, double z);
	protected native void RemoveVertexZ(uint vertex, int plane);
	protected native void SetLineVertexes(uint Line, uint v1, uint v2);
	protected native void FlipLineSideRefs(uint Line);
	protected native void SetLineSectorRef(uint line, uint side, uint sector);
	protected native Actor GetDefaultActor(Name actorclass);

	protected void FlipLineVertexes(uint Line)
	{
		uint v1 = level.lines[Line].v1.Index();
		uint v2 = level.lines[Line].v2.Index();
		SetLineVertexes(Line, v2, v1);
	}

	protected void FlipLineCompletely(uint Line)
	{
		FlipLineVertexes(Line);
		FlipLineSideRefs(Line);
	}

	protected void SetWallTexture(int line, int side, int texpart, String texture)
	{
		SetWallTextureID(line, side, texpart, TexMan.CheckForTexture(texture, TexMan.Type_Wall));
	}

	protected void SetWallTextureID(int line, int side, int texpart, TextureID texture)
	{
		level.Lines[line].sidedef[side].SetTexture(texpart, texture);
	}

	protected void SetLineFlags(int line, int setflags, int clearflags = 0)
	{
		level.Lines[line].flags = (level.Lines[line].flags & ~clearflags) | setflags;
	}

	protected void SetLineActivation(int line, int acttype)
	{
		level.Lines[line].activation = acttype;
	}

	protected void ClearLineSpecial(int line)
	{
		level.Lines[line].special = 0;
	}

	protected void SetLineSpecial(int line, int special, int arg1 = 0, int arg2 = 0, int arg3 = 0, int arg4 = 0, int arg5 = 0)
	{
		level.Lines[line].special = special;
		level.Lines[line].args[0] = arg1;
		level.Lines[line].args[1] = arg2;
		level.Lines[line].args[2] = arg3;
		level.Lines[line].args[3] = arg4;
		level.Lines[line].args[4] = arg5;
	}

	protected void SetSectorSpecial(int sectornum, int special)
	{
		level.sectors[sectornum].special = special;
	}

	protected void SetSectorTextureID(int sectornum, int plane, TextureID texture)
	{
		level.sectors[sectornum].SetTexture(plane, texture);
	}

	protected void SetSectorTexture(int sectornum, int plane, String texture)
	{
		SetSectorTextureID(sectornum, plane, TexMan.CheckForTexture(texture, TexMan.Type_Flat));
	}

	protected void SetSectorLight(int sectornum, int newval)
	{
		level.sectors[sectornum].SetLightLevel(newval);
	}

	protected void SetWallYScale(int line, int side, int texpart, double scale)
	{
		level.lines[line].sidedef[side].SetTextureYScale(texpart, scale);
	}
}
