
struct RenderEvent native ui version("2.4")
{
    native readonly Vector3 ViewPos;
    native readonly double ViewAngle;
    native readonly double ViewPitch;
    native readonly double ViewRoll;
    native readonly double FracTic;
    native readonly Actor Camera;
}

struct WorldEvent native play version("2.4")
{
    // for loaded/unloaded
    native readonly bool IsSaveGame;
    // this will be true if we are re-entering the hub level.
    native readonly bool IsReopen;
    // for thingspawned/thingdied/thingdestroyed
    native readonly Actor Thing;
    // for thingdied. can be null
    native readonly Actor Inflictor;
    // for thingdamaged, line/sector damaged
    native readonly int Damage;
    native readonly Actor DamageSource;
    native readonly Name DamageType;
    native readonly EDmgFlags DamageFlags;
    native readonly double DamageAngle;
    // for line(pre)activated
    native readonly Line ActivatedLine;
	native readonly int ActivationType;
    native bool ShouldActivate;
    // for line/sector damaged
    native readonly SectorPart DamageSectorPart;
    native readonly Line DamageLine;
    native readonly Sector DamageSector;
    native readonly int DamageLineSide;
    native readonly vector3 DamagePosition;
    native readonly bool DamageIsRadius;
    native int NewDamage;
}

struct PlayerEvent native play version("2.4")
{
    // this is the player number that caused the event.
    // note: you can get player struct from this by using players[e.PlayerNumber]
    native readonly int PlayerNumber;
    // this will be true if we are re-entering the hub level.
    native readonly bool IsReturn;
}

struct UiEvent native ui version("2.4")
{
    // d_gui.h
    enum EGUIEvent
    {
        Type_None,
        Type_KeyDown,
        Type_KeyRepeat,
        Type_KeyUp,
        Type_Char,
        Type_FirstMouseEvent, // ?
            Type_MouseMove,
            Type_LButtonDown,
            Type_LButtonUp,
            Type_LButtonClick,
            Type_MButtonDown,
            Type_MButtonUp,
            Type_MButtonClick,
            Type_RButtonDown,
            Type_RButtonUp,
            Type_RButtonClick,
            Type_WheelUp,
            Type_WheelDown,
            Type_WheelRight, // ???
            Type_WheelLeft, // ???
            Type_BackButtonDown, // ???
            Type_BackButtonUp, // ???
            Type_FwdButtonDown, // ???
            Type_FwdButtonUp, // ???
        Type_LastMouseEvent
    }
    
    // for KeyDown, KeyRepeat, KeyUp
    enum ESpecialGUIKeys
    {
        Key_PgDn        = 1,
        Key_PgUp        = 2,
        Key_Home        = 3,
        Key_End         = 4,
        Key_Left        = 5,
        Key_Right       = 6,
        Key_Alert       = 7,        // ASCII bell
        Key_Backspace   = 8,        // ASCII
        Key_Tab         = 9,        // ASCII
        Key_LineFeed    = 10,       // ASCII
        Key_Down        = 10,
        Key_VTab        = 11,       // ASCII
        Key_Up          = 11,
        Key_FormFeed    = 12,       // ASCII
        Key_Return      = 13,       // ASCII
        Key_F1          = 14,
        Key_F2          = 15,
        Key_F3          = 16,
        Key_F4          = 17,
        Key_F5          = 18,
        Key_F6          = 19,
        Key_F7          = 20,
        Key_F8          = 21,
        Key_F9          = 22,
        Key_F10         = 23,
        Key_F11         = 24,
        Key_F12         = 25,
        Key_Del         = 26,
        Key_Escape      = 27,        // ASCII
        Key_Free1       = 28,
        Key_Free2       = 29,
        Key_Back        = 30,        // browser back key
        Key_CEscape     = 31         // color escape
    }
    
    // 
    native readonly EGUIEvent Type;
    //
    native readonly String KeyString;
    native readonly int KeyChar;
    //
    native readonly int MouseX;
    native readonly int MouseY;
    //
    native readonly bool IsShift;
    native readonly bool IsCtrl;
    native readonly bool IsAlt;
}

struct InputEvent native play version("2.4")
{
    enum EGenericEvent
    {
        Type_None,
        Type_KeyDown,
        Type_KeyUp,
        Type_Mouse,
        Type_GUI, // unused, kept for completeness
        Type_DeviceChange
    }
    
    // ew.
    enum EDoomInputKeys
    {
        Key_Pause = 0xc5, // DIK_PAUSE
        Key_RightArrow = 0xcd, // DIK_RIGHT
        Key_LeftArrow = 0xcb, // DIK_LEFT
        Key_UpArrow  = 0xc8, // DIK_UP
        Key_DownArrow = 0xd0, // DIK_DOWN
        Key_Escape = 0x01, // DIK_ESCAPE
        Key_Enter = 0x1c, // DIK_RETURN
        Key_Space = 0x39, // DIK_SPACE
        Key_Tab  = 0x0f, // DIK_TAB
        Key_F1 = 0x3b, // DIK_F1
        Key_F2 = 0x3c, // DIK_F2
        Key_F3 = 0x3d, // DIK_F3
        Key_F4 = 0x3e, // DIK_F4
        Key_F5 = 0x3f, // DIK_F5
        Key_F6 = 0x40, // DIK_F6
        Key_F7 = 0x41, // DIK_F7
        Key_F8 = 0x42, // DIK_F8
        Key_F9 = 0x43, // DIK_F9
        Key_F10  = 0x44, // DIK_F10
        Key_F11  = 0x57, // DIK_F11
        Key_F12  = 0x58, // DIK_F12
        Key_Grave = 0x29, // DIK_GRAVE

        Key_Backspace = 0x0e, // DIK_BACK

        Key_Equals = 0x0d, // DIK_EQUALS
        Key_Minus = 0x0c, // DIK_MINUS

        Key_LShift = 0x2A, // DIK_LSHIFT
        Key_LCtrl = 0x1d, // DIK_LCONTROL
        Key_LAlt = 0x38, // DIK_LMENU

        Key_RShift = Key_LSHIFT,
        Key_RCtrl = Key_LCTRL,
        Key_RAlt = Key_LALT,

        Key_Ins  = 0xd2, // DIK_INSERT
        Key_Del  = 0xd3, // DIK_DELETE
        Key_End  = 0xcf, // DIK_END
        Key_Home = 0xc7, // DIK_HOME
        Key_PgUp = 0xc9, // DIK_PRIOR
        Key_PgDn = 0xd1, // DIK_NEXT

        Key_Mouse1 = 0x100,
        Key_Mouse2 = 0x101,
        Key_Mouse3 = 0x102,
        Key_Mouse4 = 0x103,
        Key_Mouse5 = 0x104,
        Key_Mouse6 = 0x105,
        Key_Mouse7 = 0x106,
        Key_Mouse8 = 0x107,

        Key_FirstJoyButton = 0x108,
        Key_Joy1 = (Key_FirstJoyButton+0),
        Key_Joy2 = (Key_FirstJoyButton+1),
        Key_Joy3 = (Key_FirstJoyButton+2),
        Key_Joy4 = (Key_FirstJoyButton+3),
        Key_Joy5 = (Key_FirstJoyButton+4),
        Key_Joy6 = (Key_FirstJoyButton+5),
        Key_Joy7 = (Key_FirstJoyButton+6),
        Key_Joy8 = (Key_FirstJoyButton+7),
        Key_LastJoyButton = 0x187,
        Key_JoyPOV1_Up = 0x188,
        Key_JoyPOV1_Right = 0x189,
        Key_JoyPOV1_Down = 0x18a,
        Key_JoyPOV1_Left = 0x18b,
        Key_JoyPOV2_Up = 0x18c,
        Key_JoyPOV3_Up = 0x190,
        Key_JoyPOV4_Up = 0x194,

        Key_MWheelUp = 0x198,
        Key_MWheelDown = 0x199,
        Key_MWheelRight = 0x19A,
        Key_MWheelLeft = 0x19B,

        Key_JoyAxis1Plus = 0x19C,
        Key_JoyAxis1Minus = 0x19D,
        Key_JoyAxis2Plus = 0x19E,
        Key_JoyAxis2Minus = 0x19F,
        Key_JoyAxis3Plus = 0x1A0,
        Key_JoyAxis3Minus = 0x1A1,
        Key_JoyAxis4Plus = 0x1A2,
        Key_JoyAxis4Minus = 0x1A3,
        Key_JoyAxis5Plus = 0x1A4,
        Key_JoyAxis5Minus = 0x1A5,
        Key_JoyAxis6Plus = 0x1A6,
        Key_JoyAxis6Minus = 0x1A7,
        Key_JoyAxis7Plus = 0x1A8,
        Key_JoyAxis7Minus = 0x1A9,
        Key_JoyAxis8Plus = 0x1AA,
        Key_JoyAxis8Minus = 0x1AB,
        Num_JoyAxisButtons = 8,

        Key_Pad_LThumb_Right = 0x1AC,
        Key_Pad_LThumb_Left = 0x1AD,
        Key_Pad_LThumb_Down = 0x1AE,
        Key_Pad_LThumb_Up = 0x1AF,

        Key_Pad_RThumb_Right = 0x1B0,
        Key_Pad_RThumb_Left = 0x1B1,
        Key_Pad_RThumb_Down = 0x1B2,
        Key_Pad_RThumb_Up = 0x1B3,

        Key_Pad_DPad_Up = 0x1B4,
        Key_Pad_DPad_Down = 0x1B5,
        Key_Pad_DPad_Left = 0x1B6,
        Key_Pad_DPad_Right = 0x1B7,
        Key_Pad_Start = 0x1B8,
        Key_Pad_Back = 0x1B9,
        Key_Pad_LThumb = 0x1BA,
        Key_Pad_RThumb = 0x1BB,
        Key_Pad_LShoulder = 0x1BC,
        Key_Pad_RShoulder = 0x1BD,
        Key_Pad_LTrigger = 0x1BE,
        Key_Pad_RTrigger = 0x1BF,
        Key_Pad_A = 0x1C0,
        Key_Pad_B = 0x1C1,
        Key_Pad_X = 0x1C2,
        Key_Pad_Y = 0x1C3,

        Num_Keys = 0x1C4
    }
    
    //
    native readonly EGenericEvent Type;
    // 
    native readonly int KeyScan; // as in EDoomInputKeys enum
    native readonly String KeyString;
    native readonly int KeyChar; // ASCII char (if any)
    //
    native readonly int MouseX;
    native readonly int MouseY;
}

struct ConsoleEvent native version("2.4")
{
    // for net events, this will be the activator.
    // for UI events, this is always -1, and you need to check if level is loaded and use players[consoleplayer].
    native readonly int Player;
    // this is the name and args as specified in SendNetworkEvent or event/netevent CCMDs
    native readonly String Name;
    native readonly int Args[3];
    // this will be true if the event is fired from the console by event/netevent CCMD
    native readonly bool IsManual;
}

struct ReplaceEvent native version("2.4")
{
	native readonly Class<Actor> Replacee;
	native Class<Actor> Replacement;
	native bool IsFinal;
}

struct ReplacedEvent native version("3.7")
{
	native Class<Actor> Replacee;
	native readonly Class<Actor> Replacement;
	native bool IsFinal;
}

class StaticEventHandler : Object native play version("2.4")
{
    // static event handlers CAN register other static event handlers.
    // unlike EventHandler.Create that will not create them.
    clearscope static native StaticEventHandler Find(Class<StaticEventHandler> type); // just for convenience. who knows.
    
    // these are called when the handler gets registered or unregistered
    // you can set Order/IsUiProcessor here.
    virtual void OnRegister() {}
    virtual void OnUnregister() {}

    // actual handlers are here
	virtual void WorldLoaded(WorldEvent e) {}
    virtual void WorldUnloaded(WorldEvent e) {}
    virtual void WorldThingSpawned(WorldEvent e) {}
    virtual void WorldThingDied(WorldEvent e) {}
    virtual void WorldThingRevived(WorldEvent e) {}
    virtual void WorldThingDamaged(WorldEvent e) {}
    virtual void WorldThingDestroyed(WorldEvent e) {}
    virtual void WorldLinePreActivated(WorldEvent e) {}
    virtual void WorldLineActivated(WorldEvent e) {}
    virtual void WorldSectorDamaged(WorldEvent e) {}
    virtual void WorldLineDamaged(WorldEvent e) {}
    virtual void WorldLightning(WorldEvent e) {} // for the sake of completeness.
    virtual void WorldTick() {}

    //
    //virtual ui void RenderFrame(RenderEvent e) {}
    virtual ui void RenderOverlay(RenderEvent e) {}
    virtual ui void RenderUnderlay(RenderEvent e) {}
    
    //
    virtual void PlayerEntered(PlayerEvent e) {}
    virtual void PlayerRespawned(PlayerEvent e) {}
    virtual void PlayerDied(PlayerEvent e) {}
    virtual void PlayerDisconnected(PlayerEvent e) {}
    
    //
	virtual ui bool UiProcess(UiEvent e) { return false; }
	virtual ui bool InputProcess(InputEvent e) { return false; }
    virtual ui void UiTick() {}
    virtual ui void PostUiTick() {}
    
    //
    virtual ui void ConsoleProcess(ConsoleEvent e) {}
    virtual void NetworkProcess(ConsoleEvent e) {}
    
    //
    virtual void CheckReplacement(ReplaceEvent e) {}
	virtual void CheckReplacee(ReplacedEvent e) {}

    //
    virtual  void NewGame() {}

    // this value will be queried on Register() to decide the relative order of this handler to every other.
    // this is most useful in UI systems.
    // default is 0.
    native readonly int Order;
    native void SetOrder(int order);
    // this value will be queried on user input to decide whether to send UiProcess to this handler.
    native bool IsUiProcessor;
    // this value determines whether mouse input is required.
    native bool RequireMouse;
}

class EventHandler : StaticEventHandler native version("2.4")
{
    clearscope static native StaticEventHandler Find(class<StaticEventHandler> type);
    clearscope static native void SendNetworkEvent(String name, int arg1 = 0, int arg2 = 0, int arg3 = 0);
}
