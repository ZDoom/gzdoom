#ifndef EVENTS_H
#define EVENTS_H

#include "dobject.h"

class DStaticEventHandler;

// register
bool E_RegisterHandler(DStaticEventHandler* handler);
// unregister
bool E_UnregisterHandler(DStaticEventHandler* handler);
// find
bool E_CheckHandler(DStaticEventHandler* handler);

// called right after the map has loaded (approximately same time as OPEN ACS scripts)
void E_MapLoaded();
// called when the map is about to unload (approximately same time as UNLOADING ACS scripts)
void E_MapUnloading();
// called on each render frame once.
void E_RenderFrame();
// called before entering each actor's view (including RenderFrame)
void E_RenderCamera();
// called before adding each actor to the render list
void E_RenderBeforeThing(AActor* thing);
// called after adding each actor to the render list
void E_RenderAfterThing(AActor* thing);

class DStaticEventHandler : public DObject // make it a part of normal GC process
{
	DECLARE_CLASS(DStaticEventHandler, DObject)
public:
	DStaticEventHandler* prev;
	DStaticEventHandler* next;
	DStaticEventHandler* unregPrev;
	DStaticEventHandler* unregNext;
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
	// called before entering each actor's view (including RenderFrame)
	virtual void RenderCamera();
	// called before adding each actor to the render list
	virtual void RenderBeforeThing(AActor* thing);
	// called after adding each actor to the render list
	virtual void RenderAfterThing(AActor* thing);
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
	// this makes sense in RenderCamera
	AActor* Camera;
	// this is for RenderBeforeThing and RenderAfterThing
	AActor* CurrentThing;

	void RenderFrame() override;
	void RenderCamera() override;
	void RenderBeforeThing(AActor* thing) override;
	void RenderAfterThing(AActor* thing) override;

	// this is a class that I use to automatically call RenderAfterThing.
	// C++ is really horrible for not providing try-finally statement.
	struct AutoThing
	{
		AActor* thing;
		
		AutoThing(AActor* thing)
		{
			this->thing = thing;
			E_RenderBeforeThing(this->thing);
		}

		~AutoThing()
		{
			E_RenderAfterThing(this->thing);
		}
	};

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