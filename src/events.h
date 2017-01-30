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


// ==============================================
//
//  RenderEventHandler - for renderer events
//
// ==============================================

class DStaticRenderEventHandler : public DStaticEventHandler
{
	DECLARE_CLASS(DStaticRenderEventHandler, DStaticEventHandler)
public:
	// these are for all render events
	DVector3 ViewPos;
	DAngle ViewAngle;
	DAngle ViewPitch;
	DAngle ViewRoll;
	double FracTic; // 0..1 value that describes where we are inside the current gametic, render-wise.
	AActor* Camera;

	DStaticRenderEventHandler()
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

	void RenderFrame() override;

private:
	void Setup();
};
class DRenderEventHandler : public DStaticRenderEventHandler
{
	DECLARE_CLASS(DRenderEventHandler, DStaticRenderEventHandler) // TODO: make sure this does not horribly break anythings
public:
	bool IsStatic() override { return false; }
};

// ==============================================
//
//  WorldEventHandler - for world events
//
// ==============================================
class DStaticWorldEventHandler : public DStaticEventHandler
{
	DECLARE_CLASS(DStaticWorldEventHandler, DStaticEventHandler)
public:
	// for WorldLoaded, WorldUnloaded.
	bool IsSaveGame; // this will be true if world event was triggered during savegame loading.
	// for WorldThingSpawned
	AActor* Thing;

	DStaticWorldEventHandler()
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

	void WorldLoaded() override;
	void WorldUnloaded() override;
	void WorldThingSpawned(AActor*) override;

private:
	void Setup();
};
// not sure if anyone wants non-static world handler, but here it is, just in case.
class DWorldEventHandler : public DStaticWorldEventHandler
{
	DECLARE_CLASS(DWorldEventHandler, DStaticWorldEventHandler)
public:
	bool IsStatic() override { return false; }
};

#endif