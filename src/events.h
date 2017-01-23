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
void E_MapLoaded();
// called when the map is about to unload (approximately same time as UNLOADING ACS scripts)
void E_MapUnloading();
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

	// destroy handler. this unlinks EventHandler from the list automatically.
	void OnDestroy() override;

	// this checks if we are /actually/ static, using DObject dynamic typing system.
	static bool IsActuallyStatic(PClass* type);

	// called right after the map has loaded (approximately same time as OPEN ACS scripts)
	virtual void MapLoaded();
	// called when the map is about to unload (approximately same time as UNLOADING ACS scripts)
	virtual void MapUnloading();
	// called on each render frame once.
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