enum ENetCmd
{
	NET_INT8 = 1,
	NET_INT16,
	NET_INT,
	NET_FLOAT,
	NET_DOUBLE,
	NET_STRING,
}

struct NetworkCommand native play version("4.12")
{
    native readonly int Player;
    native readonly Name Command;

    native int ReadInt8();
    native int ReadInt16();
    native int ReadInt();
    native double ReadFloat();
    native double ReadDouble();
    native string ReadString();

    // Wrappers
    native Name ReadName();
    native double ReadMapUnit(); // 16.16 long -> double
    native double ReadAngle(); // BAM long -> double
    native Vector2 ReadVector2();
    native Vector3 ReadVector3();
    native Vector4 ReadVector4();
    native Quat ReadQuat();
    native void ReadIntArray(out Array<int> values, ENetCmd intSize = NET_INT);
    native void ReadDoubleArray(out Array<double> values, bool doublePrecision = true);
    native void ReadStringArray(out Array<string> values, bool skipEmpty = false);
}

class NetworkBuffer native version("4.12")
{
    native void AddInt8(int value);
    native void AddInt16(int value);
    native void AddInt(int value);
    native void AddFloat(double value);
    native void AddDouble(double value);
    native void AddString(string value);

    // Wrappers
    native void AddName(Name value);
    native void AddMapUnit(double value); // double -> 16.16 long
    native void AddAngle(double value); // double -> BAM long
    native void AddVector2(Vector2 value);
    native void AddVector3(Vector3 value);
    native void AddVector4(Vector4 value);
    native void AddQuat(Quat value);
    native void AddIntArray(Array<int> values, ENetCmd intSize = NET_INT);
    native void AddDoubleArray(Array<double> values, bool doublePrecision = true);
    native void AddStringArray(Array<string> values);
}

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
    // for unloaded, name of next map (if any)
    native readonly String NextMap;
    // for thingspawned/thingdied/thingdestroyed/thingground
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
    native readonly State CrushedState;
}

struct PlayerEvent native play version("2.4")
{
    // this is the player number that caused the event.
    // note: you can get player struct from this by using players[e.PlayerNumber]
    native readonly int PlayerNumber;
    // this will be true if we are re-entering the hub level.
    native readonly bool IsReturn;
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
    virtual void OnEngineInitialize() {}
	virtual void WorldLoaded(WorldEvent e) {}
    virtual void WorldUnloaded(WorldEvent e) {}
    virtual void WorldThingSpawned(WorldEvent e) {}
    virtual void WorldThingDied(WorldEvent e) {}
    virtual void WorldThingGround(WorldEvent e) {}
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
    virtual void PlayerSpawned(PlayerEvent e) {}
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
    virtual ui void InterfaceProcess(ConsoleEvent e) {}
    virtual void NetworkProcess(ConsoleEvent e) {}
    version("4.12") virtual void NetworkCommandProcess(NetworkCommand cmd) {}
    
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
    version("4.12") clearscope static native vararg bool SendNetworkCommand(Name cmd, ...);
    version("4.12") clearscope static native bool SendNetworkBuffer(Name cmd, NetworkBuffer buffer);
    clearscope static native void SendInterfaceEvent(int playerNum, string name, int arg1 = 0, int arg2 = 0, int arg3 = 0);
}
