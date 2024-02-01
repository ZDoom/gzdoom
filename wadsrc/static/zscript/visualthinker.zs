Class VisualThinker : Thinker native
{
	native Vector3			Pos,
							Prev;
	native FVector3			Vel;
	native Vector2			Scale,
							Offset;
	native float			Roll,
							PrevRoll,
							Alpha;
	native TextureID		Texture;
	native TranslationID	Translation;
	native uint16			Flags;
	native int16			LightLevel;
	native bool				bFlipOffsetX,
							bFlipOffsetY,
							bXFlip,
							bYFlip,
							bDontInterpolate,
							bAddLightLevel;
	native Color			scolor;

	native Sector			CurSector; // can be null!

	native void SetTranslation(Name trans);
	native void SetRenderStyle(int mode); // see ERenderStyle
	native bool IsFrozen();

	static VisualThinker Spawn(Class<VisualThinker> type, TextureID tex, Vector3 pos, Vector3 vel, double alpha = 1.0, int flags = 0,
						  double roll = 0.0, Vector2 scale = (1,1), Vector2 offset = (0,0), int style = STYLE_Normal, TranslationID trans = 0)
	{
		if (!Level)	return null;

		let p = level.SpawnVisualThinker(type);
		if (p)
		{
			p.Texture = tex;
			p.Pos = pos;
			p.Vel = vel;
			p.Alpha = alpha;
			p.Roll = roll;
			p.Scale = scale;
			p.Offset = offset;
			p.SetRenderStyle(style);
			p.Translation = trans;
			p.Flags = flags;
		}
		return p;
	}
}
