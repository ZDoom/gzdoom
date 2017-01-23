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
void E_WorldUnloading();
// called right after the map has loaded (every time, UNSAFE VERSION)
void E_WorldLoadedUnsafe();
// called right before the map is unloaded (every time, UNSAFE VERSION)
void E_WorldUnloadingUnsafe();
// called on each render frame once.
void E_RenderFrame();

// serialization stuff
void E_SerializeEvents(FSerializer& arc);

class DStaticEventHandler : public DObject // make it a part of normal GC process
{
	DECLARE_CLASS(DStaticEventHandler, DObject)
public:
	DStaticEventHandler* prev;
	DStaticEventHandler* next;
	DStaticEventHandler* unregPrev;
	DStaticEventHandler* unregNext;
	bool isMapScope; // this is only used with IsStatic=true
	virtual bool IsStatic() { return true; }

	// serialization handler. let's keep it here so that I don't get lost in serialized/not serialized fields
	void Serialize(FSerializer& arc) override
	{
		Super::Serialize(arc);
		if (arc.isReading())
		{
			Printf("DStaticEventHandler::Serialize: reading object %s\n", GetClass()->TypeName.GetChars());
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
	virtual void WorldUnloading();
	virtual void WorldLoadedUnsafe();
	virtual void WorldUnloadingUnsafe();
	virtual void RenderFrame();
};
class DEventHandler : public DStaticEventHandler
{
	DECLARE_CLASS(DEventHandler, DStaticEventHandler) // TODO: make sure this does not horribly break anything
public:
	bool IsStatic() override { return false; }
};
extern DStaticEventHandler* E_FirstEventHandler;

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

#endif