#ifndef EVENTS_H
#define EVENTS_H

#include "dobject.h"
#include "serializer.h"
#include "d_event.h"
#include "d_gui.h"

class DStaticEventHandler;

// register
bool E_RegisterHandler(DStaticEventHandler* handler);
// unregister
bool E_UnregisterHandler(DStaticEventHandler* handler);
// find
bool E_CheckHandler(DStaticEventHandler* handler);
// check type
bool E_IsStaticType(PClass* type);
// init static handlers
void E_InitStaticHandlers(bool map);
// shutdown handlers
void E_Shutdown(bool map);

// called right after the map has loaded (approximately same time as OPEN ACS scripts)
void E_WorldLoaded();
// called when the map is about to unload (approximately same time as UNLOADING ACS scripts)
void E_WorldUnloaded();
// called right after the map has loaded (every time, UNSAFE VERSION)
void E_WorldLoadedUnsafe();
// called right before the map is unloaded (every time, UNSAFE VERSION)
void E_WorldUnloadedUnsafe();
// called around PostBeginPlay of each actor.
void E_WorldThingSpawned(AActor* actor);
// called after AActor::Die of each actor.
void E_WorldThingDied(AActor* actor, AActor* inflictor);
// called after AActor::Revive.
void E_WorldThingRevived(AActor* actor);
// called before P_DamageMobj and before AActor::DamageMobj virtuals.
void E_WorldThingDamaged(AActor* actor, AActor* inflictor, AActor* source, int damage, FName mod, int flags, DAngle angle);
// called before AActor::Destroy of each actor.
void E_WorldThingDestroyed(AActor* actor);
// same as ACS SCRIPT_Lightning
void E_WorldLightning();
// this executes on every tick, before everything
void E_WorldTick();
// called on each render frame once.
void E_RenderFrame();
// called after everything's been rendered, but before console/menus
void E_RenderOverlay();
// this executes when a player enters the level (once). PlayerEnter+inhub = RETURN
void E_PlayerEntered(int num, bool fromhub);
// this executes when a player respawns. includes resurrect cheat.
void E_PlayerRespawned(int num);
// this executes when a player dies (partially duplicating worldthingdied, but whatever)
void E_PlayerDied(int num);
// this executes when a player leaves the game
void E_PlayerDisconnected(int num);
// this executes on events.
bool E_Responder(event_t* ev); // splits events into InputProcess and UiProcess
// this executes on console/net events.
void E_Console(int player, FString name, int arg1, int arg2, int arg3);

// send networked event. unified function.
bool E_SendNetworkEvent(FString name, int arg1, int arg2, int arg3);

// check if there is anything that should receive GUI events
bool E_CheckUiProcessors();
// check if we need native mouse due to UiProcessors
bool E_CheckRequireMouse();

// serialization stuff
void E_SerializeEvents(FSerializer& arc);

// ==============================================
//
//  EventHandler - base class
//
// ==============================================

class DStaticEventHandler : public DObject // make it a part of normal GC process
{
	DECLARE_CLASS(DStaticEventHandler, DObject);
	HAS_OBJECT_POINTERS
public:
	DStaticEventHandler()
	{
		prev = 0;
		next = 0;
		Order = 0;
		IsUiProcessor = false;
	}

	DStaticEventHandler* prev;
	DStaticEventHandler* next;
	virtual bool IsStatic() { return true; }

	//
	int Order;
	bool IsUiProcessor;
	bool RequireMouse;

	// serialization handler. let's keep it here so that I don't get lost in serialized/not serialized fields
	void Serialize(FSerializer& arc) override
	{
		Super::Serialize(arc);
		/*
		if (arc.isReading())
		{
			Printf("DStaticEventHandler::Serialize: reading object %s\n", GetClass()->TypeName.GetChars());
		}
		else
		{
			Printf("DStaticEventHandler::Serialize: store object %s\n", GetClass()->TypeName.GetChars());
		}
		*/

		arc("Order", Order);
		arc("IsUiProcessor", IsUiProcessor);
		arc("RequireMouse", RequireMouse);
	}

	// destroy handler. this unlinks EventHandler from the list automatically.
	void OnDestroy() override;

	//
	void OnRegister(); // you can set order and IsUi here.
	void OnUnregister();

	//
	void WorldLoaded();
	void WorldUnloaded();
	void WorldThingSpawned(AActor*);
	void WorldThingDied(AActor*, AActor*);
	void WorldThingRevived(AActor*);
	void WorldThingDamaged(AActor*, AActor*, AActor*, int, FName, int, DAngle);
	void WorldThingDestroyed(AActor*);
	void WorldLightning();
	void WorldTick();

	//
	void RenderFrame();
	void RenderOverlay();

	//
	void PlayerEntered(int num, bool fromhub);
	void PlayerRespawned(int num);
	void PlayerDied(int num);
	void PlayerDisconnected(int num);

	// return true if handled.
	bool InputProcess(event_t* ev);
	bool UiProcess(event_t* ev);
	
	// 
	void ConsoleProcess(int player, FString name, int arg1, int arg2, int arg3);
};
class DEventHandler : public DStaticEventHandler
{
	DECLARE_CLASS(DEventHandler, DStaticEventHandler) // TODO: make sure this does not horribly break anything
public:
	bool IsStatic() override { return false; }
};
extern DStaticEventHandler* E_FirstEventHandler;
extern DStaticEventHandler* E_LastEventHandler;

// we cannot call this DEvent because in ZScript, 'event' is a keyword
class DBaseEvent : public DObject
{
	DECLARE_CLASS(DBaseEvent, DObject)
public:

	DBaseEvent()
	{
		// each type of event is created only once to avoid new/delete hell
		// since from what I remember object creation and deletion results in a lot of GC processing
		// (and we aren't supposed to pass event objects around anyway)
		this->ObjectFlags |= OF_Fixed;
		// we don't want to store events into the savegames because they are global.
		this->ObjectFlags |= OF_Transient;
	}
};

class DRenderEvent : public DBaseEvent
{
	DECLARE_CLASS(DRenderEvent, DBaseEvent)
public:
	// these are for all render events
	DVector3 ViewPos;
	DAngle ViewAngle;
	DAngle ViewPitch;
	DAngle ViewRoll;
	double FracTic; // 0..1 value that describes where we are inside the current gametic, render-wise.
	AActor* Camera;

	DRenderEvent()
	{
		FracTic = 0;
		Camera = nullptr;
	}
};

class DWorldEvent : public DBaseEvent
{
	DECLARE_CLASS(DWorldEvent, DBaseEvent)
public:
	// for loaded/unloaded
	bool IsSaveGame;
	bool IsReopen;
	// for thingspawned, thingdied, thingdestroyed
	AActor* Thing;
	// for thingdied
	AActor* Inflictor; // can be null
	// for damagemobj
	int Damage;
	AActor* DamageSource; // can be null
	FName DamageType;
	int DamageFlags;
	DAngle DamageAngle;

	DWorldEvent()
	{
		IsSaveGame = false;
		IsReopen = false;
		Thing = nullptr;
		Inflictor = nullptr;
		Damage = 0;
		DamageSource = nullptr;
		DamageFlags = 0;
	}
};

class DPlayerEvent : public DBaseEvent
{
	DECLARE_CLASS(DPlayerEvent, DBaseEvent)
public:
	// we currently have only one member: player index
	// in ZScript, we have global players[] array from which we can
	//  get both the player itself and player's body,
	//  so no need to pass it here.
	int PlayerNumber;
	// we set this to true if level was reopened (RETURN scripts)
	bool IsReturn;

	DPlayerEvent()
	{
		PlayerNumber = -1;
		IsReturn = false;
	}
};

class DUiEvent : public DBaseEvent
{
	DECLARE_CLASS(DUiEvent, DBaseEvent)
public:
	// this essentially translates event_t UI events to ZScript.
	EGUIEvent Type;
	// for keys/chars/whatever
	FString KeyString;
	int KeyChar;
	// for mouse
	int MouseX;
	int MouseY;
	// global (?)
	bool IsShift;
	bool IsCtrl;
	bool IsAlt;

	DUiEvent()
	{
		Type = EV_GUI_None;
	}
};

class DInputEvent : public DBaseEvent
{
	DECLARE_CLASS(DInputEvent, DBaseEvent)
public:
	// this translates regular event_t events to ZScript (not UI, UI events are sent via DUiEvent and only if requested!)
	EGenericEvent Type;
	// for keys
	int KeyScan;
	FString KeyString;
	int KeyChar;
	// for mouse
	int MouseX;
	int MouseY;

	DInputEvent()
	{
		Type = EV_None;
	}
};

class DConsoleEvent : public DBaseEvent
{
	DECLARE_CLASS(DConsoleEvent, DBaseEvent)
public:
	// player that activated this event. note that it's always -1 for non-playsim events (i.e. these not called with netevent)
	int Player;
	//
	FString Name;
	int Args[3];

	DConsoleEvent()
	{
		Player = -1;
	}
};

#endif