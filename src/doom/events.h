#ifndef EVENTS_H
#define EVENTS_H

#include "dobject.h"
#include "serializer.h"
#include "d_event.h"
#include "d_gui.h"
#include "sbar.h"

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
// called in P_ActivateLine before executing special, set shouldactivate to false to prevent activation.
void E_WorldLinePreActivated(line_t* line, AActor* actor, bool* shouldactivate);
// called in P_ActivateLine after successful special execution.
void E_WorldLineActivated(line_t* line, AActor* actor);
// same as ACS SCRIPT_Lightning
void E_WorldLightning();
// this executes on every tick, before everything, only when in valid level and not paused
void E_WorldTick();
// this executes on every tick on UI side, always
void E_UiTick();
// this executes on every tick on UI side, always AND immediately after everything else
void E_PostUiTick();
// called on each render frame once.
void E_RenderFrame();
// called after everything's been rendered, but before console/menus
void E_RenderOverlay(EHudState state);
// this executes when a player enters the level (once). PlayerEnter+inhub = RETURN
void E_PlayerEntered(int num, bool fromhub);
// this executes when a player respawns. includes resurrect cheat.
void E_PlayerRespawned(int num);
// this executes when a player dies (partially duplicating worldthingdied, but whatever)
void E_PlayerDied(int num);
// this executes when a player leaves the game
void E_PlayerDisconnected(int num);
// this executes on events.
bool E_Responder(const event_t* ev); // splits events into InputProcess and UiProcess
// this executes on console/net events.
void E_Console(int player, FString name, int arg1, int arg2, int arg3, bool manual);

// send networked event. unified function.
bool E_SendNetworkEvent(FString name, int arg1, int arg2, int arg3, bool manual);

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
	void WorldLinePreActivated(line_t*, AActor*, bool*);
	void WorldLineActivated(line_t*, AActor*);
	void WorldLightning();
	void WorldTick();

	//
	void RenderFrame();
	void RenderOverlay(EHudState state);

	//
	void PlayerEntered(int num, bool fromhub);
	void PlayerRespawned(int num);
	void PlayerDied(int num);
	void PlayerDisconnected(int num);

	// return true if handled.
	bool InputProcess(const event_t* ev);
	bool UiProcess(const event_t* ev);
	void UiTick();
	void PostUiTick();
	
	// 
	void ConsoleProcess(int player, FString name, int arg1, int arg2, int arg3, bool manual);
};
class DEventHandler : public DStaticEventHandler
{
	DECLARE_CLASS(DEventHandler, DStaticEventHandler) // TODO: make sure this does not horribly break anything
public:
	bool IsStatic() override { return false; }
};
extern DStaticEventHandler* E_FirstEventHandler;
extern DStaticEventHandler* E_LastEventHandler;

struct FRenderEvent
{
	// these are for all render events
	DVector3 ViewPos;
	DAngle ViewAngle;
	DAngle ViewPitch;
	DAngle ViewRoll;
	double FracTic = 0; // 0..1 value that describes where we are inside the current gametic, render-wise.
	AActor* Camera = nullptr;
	int HudState;
};

struct FWorldEvent
{
	// for loaded/unloaded
	bool IsSaveGame = false;
	bool IsReopen = false;
	// for thingspawned, thingdied, thingdestroyed
	AActor* Thing = nullptr; // for thingdied
	AActor* Inflictor = nullptr; // can be null - for damagemobj
	AActor* DamageSource = nullptr; // can be null
	int Damage = 0;
	FName DamageType;
	int DamageFlags = 0;
	DAngle DamageAngle;
	// for line(pre)activated
	line_t* ActivatedLine = nullptr;
	bool ShouldActivate = true;
};

struct FPlayerEvent
{
	// we currently have only one member: player index
	// in ZScript, we have global players[] array from which we can
	//  get both the player itself and player's body,
	//  so no need to pass it here.
	int PlayerNumber;
	// we set this to true if level was reopened (RETURN scripts)
	bool IsReturn;
};

struct FUiEvent
{
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

	FUiEvent(const event_t *ev);
};

struct FInputEvent
{
	// this translates regular event_t events to ZScript (not UI, UI events are sent via DUiEvent and only if requested!)
	EGenericEvent Type = EV_None;
	// for keys
	int KeyScan;
	FString KeyString;
	int KeyChar;
	// for mouse
	int MouseX;
	int MouseY;

	FInputEvent(const event_t *ev);
};

struct FConsoleEvent 
{
	// player that activated this event. note that it's always -1 for non-playsim events (i.e. these not called with netevent)
	int Player;
	//
	FString Name;
	int Args[3];
	//
	bool IsManual;
};

#endif
