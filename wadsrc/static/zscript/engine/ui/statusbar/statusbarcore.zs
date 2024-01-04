class HUDFont native ui
{
	native Font mFont;
	native static HUDFont Create(Font fnt, int spacing = 0, EMonospacing monospacing = Mono_Off, int shadowx = 0, int shadowy = 0);
}

class StatusBarCore native ui
{
	enum DI_Flags
	{
		DI_SKIPICON = 0x1,
		DI_SKIPALTICON = 0x2,
		DI_SKIPSPAWN = 0x4,
		DI_SKIPREADY = 0x8,
		DI_ALTICONFIRST = 0x10,
		DI_TRANSLATABLE = 0x20,
		DI_FORCESCALE = 0x40,
		DI_DIM = 0x80,
		DI_DRAWCURSORFIRST = 0x100,	// only for DrawInventoryBar.
		DI_ALWAYSSHOWCOUNT = 0x200,	// only for DrawInventoryBar.
		DI_DIMDEPLETED = 0x400,
		DI_DONTANIMATE = 0x800,		// do not animate the texture
		DI_MIRROR = 0x1000,		// flip the texture horizontally, like a mirror
		DI_ITEM_RELCENTER = 0x2000,
		DI_MIRRORY = 0x40000000,

		DI_SCREEN_AUTO = 0,					// decide based on given offsets.
		DI_SCREEN_MANUAL_ALIGN = 0x4000,	// If this is on, the following flags will have an effect

		DI_SCREEN_TOP = DI_SCREEN_MANUAL_ALIGN,
		DI_SCREEN_VCENTER = 0x8000 | DI_SCREEN_MANUAL_ALIGN,
		DI_SCREEN_BOTTOM = 0x10000 | DI_SCREEN_MANUAL_ALIGN,
		DI_SCREEN_VOFFSET = 0x18000 | DI_SCREEN_MANUAL_ALIGN,
		DI_SCREEN_VMASK = 0x18000 | DI_SCREEN_MANUAL_ALIGN,

		DI_SCREEN_LEFT = DI_SCREEN_MANUAL_ALIGN,
		DI_SCREEN_HCENTER = 0x20000 | DI_SCREEN_MANUAL_ALIGN,
		DI_SCREEN_RIGHT = 0x40000 | DI_SCREEN_MANUAL_ALIGN,
		DI_SCREEN_HOFFSET = 0x60000 | DI_SCREEN_MANUAL_ALIGN,
		DI_SCREEN_HMASK = 0x60000 | DI_SCREEN_MANUAL_ALIGN,

		DI_SCREEN_LEFT_TOP = DI_SCREEN_TOP | DI_SCREEN_LEFT,
		DI_SCREEN_RIGHT_TOP = DI_SCREEN_TOP | DI_SCREEN_RIGHT,
		DI_SCREEN_LEFT_BOTTOM = DI_SCREEN_BOTTOM | DI_SCREEN_LEFT,
		DI_SCREEN_LEFT_CENTER = DI_SCREEN_VCENTER | DI_SCREEN_LEFT,
		DI_SCREEN_RIGHT_BOTTOM = DI_SCREEN_BOTTOM | DI_SCREEN_RIGHT,
		DI_SCREEN_RIGHT_CENTER = DI_SCREEN_VCENTER | DI_SCREEN_RIGHT,
		DI_SCREEN_CENTER = DI_SCREEN_VCENTER | DI_SCREEN_HCENTER,
		DI_SCREEN_CENTER_TOP = DI_SCREEN_TOP | DI_SCREEN_HCENTER,
		DI_SCREEN_CENTER_BOTTOM = DI_SCREEN_BOTTOM | DI_SCREEN_HCENTER,
		DI_SCREEN_OFFSETS = DI_SCREEN_HOFFSET | DI_SCREEN_VOFFSET,

		DI_ITEM_AUTO = 0,		// equivalent with bottom center, which is the default alignment.

		DI_ITEM_TOP = 0x80000,
		DI_ITEM_VCENTER = 0x100000,
		DI_ITEM_BOTTOM = 0,		// this is the default vertical alignment
		DI_ITEM_VOFFSET = 0x180000,
		DI_ITEM_VMASK = 0x180000,

		DI_ITEM_LEFT = 0x200000,
		DI_ITEM_HCENTER = 0,	// this is the default horizontal alignment
		DI_ITEM_RIGHT = 0x400000,
		DI_ITEM_HOFFSET = 0x600000,
		DI_ITEM_HMASK = 0x600000,

		DI_ITEM_LEFT_TOP = DI_ITEM_TOP|DI_ITEM_LEFT,
		DI_ITEM_RIGHT_TOP = DI_ITEM_TOP|DI_ITEM_RIGHT,
		DI_ITEM_LEFT_CENTER = DI_ITEM_CENTER|DI_ITEM_LEFT,
		DI_ITEM_RIGHT_CENTER = DI_ITEM_CENTER|DI_ITEM_RIGHT,
		DI_ITEM_LEFT_BOTTOM = DI_ITEM_BOTTOM|DI_ITEM_LEFT,
		DI_ITEM_RIGHT_BOTTOM = DI_ITEM_BOTTOM|DI_ITEM_RIGHT,
		DI_ITEM_CENTER = DI_ITEM_VCENTER|DI_ITEM_HCENTER,
		DI_ITEM_CENTER_BOTTOM = DI_ITEM_BOTTOM|DI_ITEM_HCENTER,
		DI_ITEM_OFFSETS = DI_ITEM_HOFFSET|DI_ITEM_VOFFSET,

		DI_TEXT_ALIGN_LEFT = 0,
		DI_TEXT_ALIGN_RIGHT = 0x800000,
		DI_TEXT_ALIGN_CENTER = 0x1000000,
		DI_TEXT_ALIGN = 0x1800000,

		DI_ALPHAMAPPED = 0x2000000,
		DI_NOSHADOW = 0x4000000,
		DI_ALWAYSSHOWCOUNTERS = 0x8000000,
		DI_ARTIFLASH = 0x10000000,
		DI_FORCEFILL = 0x20000000,

		// These 2 flags are only used by SBARINFO so these duplicate other flags not used by SBARINFO
		DI_DRAWINBOX = DI_TEXT_ALIGN_RIGHT,
		DI_ALTERNATEONFAIL = DI_TEXT_ALIGN_CENTER,

	};

	enum ENumFlags
	{
		FNF_WHENNOTZERO = 0x1,
		FNF_FILLZEROS = 0x2,
	}

	// These are block properties for the drawers. A child class can set them to have a block of items use the same settings.
	native double Alpha;
	native Vector2 drawOffset;		// can be set by subclasses to offset drawing operations
	native double drawClip[4];		// defines a clipping rectangle (not used yet)
	native bool fullscreenOffsets;	// current screen is displayed with fullscreen behavior.
	native int RelTop;
	native int HorizontalResolution, VerticalResolution;
	native bool CompleteBorder;
	native Vector2 defaultScale;	// factor for fully scaled fullscreen display.


	native static String FormatNumber(int number, int minsize = 0, int maxsize = 0, int format = 0, String prefix = "");
	native double, double, double, double StatusbarToRealCoords(double x, double y=0, double w=0, double h=0);
	native void DrawTexture(TextureID texture, Vector2 pos, int flags = 0, double Alpha = 1., Vector2 box = (-1, -1), Vector2 scale = (1, 1), ERenderStyle style = STYLE_Translucent, Color col = 0xffffffff, int translation = 0, double clipwidth = -1);
	native void DrawImage(String texture, Vector2 pos, int flags = 0, double Alpha = 1., Vector2 box = (-1, -1), Vector2 scale = (1, 1), ERenderStyle style = STYLE_Translucent, Color col = 0xffffffff, int translation = 0, double clipwidth = -1);

	native void DrawTextureRotated(TextureID texid, Vector2 pos, int flags, double angle, double alpha = 1, Vector2 scale = (1, 1), ERenderStyle style = STYLE_Translucent, Color col = 0xffffffff, int translation = 0);
	native void DrawImageRotated(String texid, Vector2 pos, int flags, double angle, double alpha = 1, Vector2 scale = (1, 1), ERenderStyle style = STYLE_Translucent, Color col = 0xffffffff, int translation = 0);

	native void DrawString(HUDFont font, String string, Vector2 pos, int flags = 0, int translation = Font.CR_UNTRANSLATED, double Alpha = 1., int wrapwidth = -1, int linespacing = 4, Vector2 scale = (1, 1), int pt = 0, ERenderStyle style = STYLE_Translucent);
	native double, double, double, double TransformRect(double x, double y, double w, double h, int flags = 0);
	native void Fill(Color col, double x, double y, double w, double h, int flags = 0);
	native void SetClipRect(double x, double y, double w, double h, int flags = 0);

	native void SetSize(int height, int vwidth, int vheight, int hwidth = -1, int hheight = -1);
	native Vector2 GetHUDScale();
	native void BeginStatusBar(bool forceScaled = false, int resW = -1, int resH = -1, int rel = -1);
	native void BeginHUD(double Alpha = 1., bool forcescaled = false, int resW = -1, int resH = -1);

	void ClearClipRect()
	{
		screen.ClearClipRect();
	}

	//============================================================================
	//
	// Returns how much the status bar's graphics extend into the view
	// Used for automap text positioning
	// The parameter specifies how much of the status bar area will be covered
	// by the element requesting this information.
	//
	//============================================================================

	virtual int GetProtrusion(double scaleratio) const
	{
		return 0;
	}

}

//============================================================================
//
// a generic value interpolator for status bar elements that can change
// gradually to their new value.
//
//============================================================================

class LinearValueInterpolator : Object
{
	int mCurrentValue;
	int mMaxChange;

	static LinearValueInterpolator Create(int startval, int maxchange)
	{
		let v = new("LinearValueInterpolator");
		v.mCurrentValue = startval;
		v.mMaxChange = maxchange;
		return v;
	}

	void Reset(int value)
	{
		mCurrentValue = value;
	}

	// This must be called periodically in the status bar's Tick function.
	// Do not call this in the Draw function because that may skip some frames!
	void Update(int destvalue)
	{
		if (mCurrentValue > destvalue)
		{
			mCurrentValue = max(destvalue, mCurrentValue - mMaxChange);
		}
		else
		{
			mCurrentValue = min(destvalue, mCurrentValue + mMaxChange);
		}
	}

	// This must be called in the draw function to retrieve the value for output.
	int GetValue()
	{
		return mCurrentValue;
	}
}

class DynamicValueInterpolator : Object
{
	int mCurrentValue;
	int mMinChange;
	int mMaxChange;
	double mChangeFactor;


	static DynamicValueInterpolator Create(int startval, double changefactor, int minchange, int maxchange)
	{
		let v = new("DynamicValueInterpolator");
		v.mCurrentValue = startval;
		v.mMinChange = minchange;
		v.mMaxChange = maxchange;
		v.mChangeFactor = changefactor;
		return v;
	}

	void Reset(int value)
	{
		mCurrentValue = value;
	}

	// This must be called periodically in the status bar's Tick function.
	// Do not call this in the Draw function because that may skip some frames!
	void Update(int destvalue)
	{
		int diff = int(clamp(abs(destvalue - mCurrentValue) * mChangeFactor, mMinChange, mMaxChange));
		if (mCurrentValue > destvalue)
		{
			mCurrentValue = max(destvalue, mCurrentValue - diff);
		}
		else
		{
			mCurrentValue = min(destvalue, mCurrentValue + diff);
		}
	}

	// This must be called in the draw function to retrieve the value for output.
	int GetValue()
	{
		return mCurrentValue;
	}
}

