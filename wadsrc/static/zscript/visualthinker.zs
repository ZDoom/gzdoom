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
	native int16			LightLevel;
	
	native uint16			Flags;
	native int				VisualThinkerFlags;
    
    FlagDef                 FlipOffsetX :       VisualThinkerFlags, 0;
    FlagDef                 FlipOffsetY :       VisualThinkerFlags, 1;
    FlagDef                 XFlip :             VisualThinkerFlags, 2;
    FlagDef                 YFlip :             VisualThinkerFlags, 3;
    FlagDef                 DontInterpolate :   VisualThinkerFlags, 4;
    FlagDef                 AddLightLevel :     VisualThinkerFlags, 5;

	native Color			scolor;

	native Sector			CurSector; // can be null!

	native void SetTranslation(Name trans);
	native void SetRenderStyle(int mode); // see ERenderStyle
	native bool IsFrozen();

	native protected void UpdateSector(); // needs to be called if the thinker is set to a non-ticking statnum and the position is modified (or if Tick is overriden and doesn't call Super.Tick())
	native protected void UpdateSpriteInfo(); // needs to be called every time the texture is updated if the thinker uses SPF_LOCAL_ANIM and is set to a non-ticking statnum (or if Tick is overriden and doesn't call Super.Tick())

	static VisualThinker Spawn(Class<VisualThinker> type, TextureID tex, Vector3 pos, Vector3 vel, double alpha = 1.0, int flags = 0,
						  double roll = 0.0, Vector2 scale = (1,1), Vector2 offset = (0,0), int style = STYLE_Normal, TranslationID trans = 0, int VisualThinkerFlags = 0)
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
            p.VisualThinkerFlags = VisualThinkerFlags;
			p.UpdateSector();
			p.UpdateSpriteInfo();
		}
		return p;
	}
}
