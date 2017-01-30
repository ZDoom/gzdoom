#ifndef EVENTS_H
#define EVENTS_H

#include "dobject.h"
#include "serializer.h"

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
// called on each render frame once.
void E_RenderFrame();

// serialization stuff
void E_SerializeEvents(FSerializer& arc);

// ==============================================
//
//  EventHandler - base class
//
// ==============================================

class DStaticEventHandler : public DObject // make it a part of normal GC process
{
	DECLARE_CLASS(DStaticEventHandler, DObject)
public:
	DStaticEventHandler()
	{
		prev = 0;
		next = 0;
		isMapScope = false;
	}

	DStaticEventHandler* prev;
	DStaticEventHandler* next;
	bool isMapScope;
	virtual bool IsStatic() { return true; }

	// serialization handler. let's keep it here so that I don't get lost in serialized/not serialized fields
	void Serialize(FSerializer& arc) override
	{
		Super::Serialize(arc);
		if (arc.isReading())
		{
			Printf("DStaticEventHandler::Serialize: reading object %s\n", GetClass()->TypeName.GetChars());
			isMapScope = true; // unserialized static handler means map scope anyway. other handlers don't get serialized.
		}
		else
		{
			Printf("DStaticEventHandler::Serialize: store object %s\n", GetClass()->TypeName.GetChars());
		}
		/* do nothing */
	}

	// destroy handler. this unlinks EventHandler from the list automatically.
	void OnDestroy() override;

	virtual void WorldLoaded();
	virtual void WorldUnloaded();
	virtual void WorldThingSpawned(AActor*);
	virtual void RenderFrame();
};
class DEventHandler : public DStaticEventHandler
{
	DECLARE_CLASS(DEventHandler, DStaticEventHandler) // TODO: make sure this does not horribly break anything
public:
	bool IsStatic() override { return false; }
};
extern DStaticEventHandler* E_FirstEventHandler;

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

	// serialization handler for our local stuff
	void Serialize(FSerializer& arc) override
	{
		Super::Serialize(arc);
		arc("ViewPos", ViewPos);
		arc("ViewAngle", ViewAngle);
		arc("ViewPitch", ViewPitch);
		arc("ViewRoll", ViewRoll);
		arc("FracTic", FracTic);
		arc("Camera", Camera);
	}
};

class DWorldEvent : public DBaseEvent
{
	DECLARE_CLASS(DWorldEvent, DBaseEvent)
public:
	// for loaded/unloaded
	bool IsSaveGame;
	// for thingspawned, thingdied, thingdestroyed
	AActor* Thing;

	DWorldEvent()
	{
		IsSaveGame = false;
		Thing = nullptr;
	}

	void Serialize(FSerializer& arc) override
	{
		Super::Serialize(arc);
		arc("IsSaveGame", IsSaveGame);
		arc("Thing", Thing);
	}
};

#endif