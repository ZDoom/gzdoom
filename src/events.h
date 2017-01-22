#ifndef EVENTS_H
#define EVENTS_H

#include "dobject.h"

class DEventHandler : public DObject // make it a part of normal GC process
{
	DECLARE_CLASS(DEventHandler, DObject)
public:
	DEventHandler* prev;
	DEventHandler* next;
	DEventHandler* unregPrev;
	DEventHandler* unregNext;

	// destroy handler. this unlinks EventHandler from the list automatically.
	void OnDestroy() override;

	// called right after the map has loaded (approximately same time as OPEN ACS scripts)
	virtual void MapLoaded();
	// called when the map is about to unload (approximately same time as UNLOADING ACS scripts)
	virtual void MapUnloading();
	// called on each render frame once.
	virtual void RenderFrame();
	// called before entering each actor's view (including RenderFrame)
	virtual void RenderCamera();
};
extern DEventHandler* E_FirstEventHandler;

class DRenderEventHandler : public DEventHandler
{
	DECLARE_CLASS(DRenderEventHandler, DEventHandler)
public:
	DVector3 ViewPos;
	DAngle ViewAngle;
	DAngle ViewPitch;
	DAngle ViewRoll;
	double FracTic; // 0..1 value that describes where we are inside the current gametic, render-wise.

	void RenderFrame() override;
	void RenderCamera() override;

private:
	void Setup();
};

// register
void E_RegisterHandler(DEventHandler* handler);
// unregister
void E_UnregisterHandler(DEventHandler* handler);

// called right after the map has loaded (approximately same time as OPEN ACS scripts)
void E_MapLoaded();
// called when the map is about to unload (approximately same time as UNLOADING ACS scripts)
void E_MapUnloading();
// called on each render frame once.
void E_RenderFrame();
// called before entering each actor's view (including RenderFrame)
void E_RenderCamera();

#endif