#include "events.h"
#include "virtual.h"
#include "r_utility.h"

DEventHandler* E_FirstEventHandler = nullptr;

void E_RegisterHandler(DEventHandler* handler)
{
	if (handler == nullptr || handler->ObjectFlags & OF_EuthanizeMe)
		return;
	// link into normal list
	handler->prev = nullptr;
	handler->next = E_FirstEventHandler;
	if (handler->next)
		handler->next->prev = handler;
	E_FirstEventHandler = handler;
}

void E_UnregisterHandler(DEventHandler* handler)
{
	if (handler == nullptr || handler->ObjectFlags & OF_EuthanizeMe)
		return;
	// link out of normal list
	if (handler->prev)
		handler->prev->next = handler->next;
	if (handler->next)
		handler->next->prev = handler->prev;
	if (handler == E_FirstEventHandler)
		E_FirstEventHandler = handler->next;
}

void E_MapLoaded()
{
	for (DEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->MapLoaded();
}

void E_MapUnloading()
{
	for (DEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->MapUnloading();
}

void E_RenderFrame()
{
	for (DEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->RenderFrame();
}

void E_RenderCamera()
{
	for (DEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->RenderCamera();
}

void E_RenderBeforeThing(AActor* thing)
{
	for (DEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->RenderBeforeThing(thing);
}

void E_RenderAfterThing(AActor* thing)
{
	for (DEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->RenderAfterThing(thing);
}

// declarations
IMPLEMENT_CLASS(DEventHandler, false, false);
IMPLEMENT_CLASS(DRenderEventHandler, false, false);

DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, ViewPos);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, ViewAngle);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, ViewPitch);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, ViewRoll);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, FracTic);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, Camera);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, CurrentThing);

DEFINE_ACTION_FUNCTION(DEventHandler, Create)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DEventHandler);
	// generate a new object of this type.
	ACTION_RETURN_OBJECT(t->CreateNew());
}

DEFINE_ACTION_FUNCTION(DEventHandler, CreateOnce)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DEventHandler);
	// check if there are already registered handlers of this type.
	for (DEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		if (handler->GetClass() == t) // check precise class
			ACTION_RETURN_OBJECT(nullptr);
	// generate a new object of this type.
	ACTION_RETURN_OBJECT(t->CreateNew());
}

DEFINE_ACTION_FUNCTION(DEventHandler, Find)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DEventHandler);
	for (DEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		if (handler->GetClass() == t) // check precise class
			ACTION_RETURN_OBJECT(handler);
	ACTION_RETURN_OBJECT(nullptr);
}

DEFINE_ACTION_FUNCTION(DEventHandler, Register)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(handler, DEventHandler);
	E_RegisterHandler(handler);
	return 0;
}

DEFINE_ACTION_FUNCTION(DEventHandler, Unregister)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(handler, DEventHandler);
	E_UnregisterHandler(handler);
	return 0;
}

#define DEFINE_EVENT_HANDLER(cls, funcname, args) DEFINE_ACTION_FUNCTION(cls, funcname) \
{ \
	PARAM_SELF_PROLOGUE(cls); \
	return 0; \
} \
void cls::funcname(args) \
{ \
	IFVIRTUAL(cls, funcname) \
	{ \
		if (func == cls##_##funcname##_VMPtr) \
			return; \
		VMValue params[1] = { (cls*)this }; \
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr); \
	} \
}

DEFINE_EVENT_HANDLER(DEventHandler, MapLoaded,)
DEFINE_EVENT_HANDLER(DEventHandler, MapUnloading,)
DEFINE_EVENT_HANDLER(DEventHandler, RenderFrame,)
DEFINE_EVENT_HANDLER(DEventHandler, RenderCamera,)
DEFINE_EVENT_HANDLER(DEventHandler, RenderBeforeThing, AActor*)
DEFINE_EVENT_HANDLER(DEventHandler, RenderAfterThing, AActor*)

//
void DEventHandler::OnDestroy()
{
	E_UnregisterHandler(this);
	DObject::OnDestroy();
}

void DRenderEventHandler::Setup()
{
	ViewPos = ::ViewPos;
	ViewAngle = ::ViewAngle;
	ViewPitch = ::ViewPitch;
	ViewRoll = ::ViewRoll;
	FracTic = ::r_TicFracF;
	Camera = ::camera;
}

void DRenderEventHandler::RenderFrame()
{
	Setup();
	DEventHandler::RenderFrame();
}

void DRenderEventHandler::RenderCamera()
{
	Setup();
	DEventHandler::RenderCamera();
}

void DRenderEventHandler::RenderBeforeThing(AActor* thing)
{
	CurrentThing = thing;
	DEventHandler::RenderBeforeThing(thing);
}

void DRenderEventHandler::RenderAfterThing(AActor* thing)
{
	CurrentThing = thing;
	DEventHandler::RenderAfterThing(thing);
}