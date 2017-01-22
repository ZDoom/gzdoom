#include "events.h"
#include "virtual.h"
#include "r_utility.h"

DEventHandler* E_FirstDEventHandler = nullptr;

void E_RegisterHandler(DEventHandler* handler)
{
	if (handler == nullptr || handler->ObjectFlags & OF_EuthanizeMe)
		return;
	// link into normal list
	handler->prev = nullptr;
	handler->next = E_FirstDEventHandler;
	if (handler->next)
		handler->next->prev = handler;
	E_FirstDEventHandler = handler;
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
	if (handler == E_FirstDEventHandler)
		E_FirstDEventHandler = handler->next;
}

void E_MapLoaded()
{
	for (DEventHandler* handler = E_FirstDEventHandler; handler; handler = handler->next)
		handler->MapLoaded();
}

void E_MapUnloading()
{
	for (DEventHandler* handler = E_FirstDEventHandler; handler; handler = handler->next)
		handler->MapUnloading();
}

void E_RenderFrame()
{
	for (DEventHandler* handler = E_FirstDEventHandler; handler; handler = handler->next)
		handler->RenderFrame();
}

void E_RenderCamera()
{
	for (DEventHandler* handler = E_FirstDEventHandler; handler; handler = handler->next)
		handler->RenderCamera();
}

// declarations
IMPLEMENT_CLASS(DEventHandler, false, false);
IMPLEMENT_CLASS(DRenderEventHandler, false, false);

DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, ViewPos);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, ViewAngle);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, ViewPitch);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, ViewRoll);
DEFINE_FIELD_X(RenderEventHandler, DRenderEventHandler, FracTic);

DEFINE_ACTION_FUNCTION(DEventHandler, Create)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DEventHandler);
	// generate a new object of this type.
	ACTION_RETURN_OBJECT(t->CreateNew());
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

DEFINE_ACTION_FUNCTION(DEventHandler, MapLoaded)
{
	PARAM_SELF_PROLOGUE(DEventHandler);
	// do nothing
	return 0;
}

DEFINE_ACTION_FUNCTION(DEventHandler, MapUnloading)
{
	PARAM_SELF_PROLOGUE(DEventHandler);
	// do nothing
	return 0;
}

DEFINE_ACTION_FUNCTION(DEventHandler, RenderFrame)
{
	PARAM_SELF_PROLOGUE(DEventHandler);
	// do nothing
	return 0;
}

DEFINE_ACTION_FUNCTION(DEventHandler, RenderCamera)
{
	PARAM_SELF_PROLOGUE(DEventHandler);
	// do nothing
	return 0;
}

//
void DEventHandler::OnDestroy()
{
	E_UnregisterHandler(this);
	DObject::OnDestroy();
}

void DEventHandler::MapLoaded()
{
	IFVIRTUAL(DEventHandler, MapLoaded)
	{
		// Without the type cast this picks the 'void *' assignment...
		VMValue params[1] = { (DEventHandler*)this };
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
}

void DEventHandler::MapUnloading()
{
	IFVIRTUAL(DEventHandler, MapUnloading)
	{
		// Without the type cast this picks the 'void *' assignment...
		VMValue params[1] = { (DEventHandler*)this };
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
}

void DEventHandler::RenderFrame()
{
	IFVIRTUAL(DEventHandler, RenderFrame)
	{
		// Without the type cast this picks the 'void *' assignment...
		VMValue params[1] = { (DEventHandler*)this };
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
}

void DEventHandler::RenderCamera()
{
	IFVIRTUAL(DEventHandler, RenderCamera)
	{
		// Without the type cast this picks the 'void *' assignment...
		VMValue params[1] = { (DEventHandler*)this };
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
}

void DRenderEventHandler::Setup()
{
	ViewPos = ::ViewPos;
	ViewAngle = ::ViewAngle;
	ViewPitch = ::ViewPitch;
	ViewRoll = ::ViewRoll;
	FracTic = ::r_TicFracF;
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
