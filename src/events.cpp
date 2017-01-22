#include "events.h"
#include "virtual.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "gi.h"
#include "v_text.h"

DStaticEventHandler* E_FirstEventHandler = nullptr;

bool E_RegisterHandler(DStaticEventHandler* handler)
{
	if (handler == nullptr || handler->ObjectFlags & OF_EuthanizeMe)
		return false;
	if (E_CheckHandler(handler))
		return false;
	// link into normal list
	handler->prev = nullptr;
	handler->next = E_FirstEventHandler;
	if (handler->next)
		handler->next->prev = handler;
	E_FirstEventHandler = handler;
	if (handler->IsStatic() && !handler->isMapScope) handler->ObjectFlags |= OF_Fixed;
	return true;
}

bool E_UnregisterHandler(DStaticEventHandler* handler)
{
	if (handler == nullptr || handler->ObjectFlags & OF_EuthanizeMe)
		return false;
	if (!E_CheckHandler(handler))
		return false;
	// link out of normal list
	if (handler->prev)
		handler->prev->next = handler->next;
	if (handler->next)
		handler->next->prev = handler->prev;
	if (handler == E_FirstEventHandler)
		E_FirstEventHandler = handler->next;
	if (handler->IsStatic() && !handler->isMapScope)
	{
		handler->ObjectFlags |= OF_YesReallyDelete;
		delete handler;
	}
	return true;
}

bool E_CheckHandler(DStaticEventHandler* handler)
{
	for (DStaticEventHandler* lhandler = E_FirstEventHandler; lhandler; lhandler = lhandler->next)
		if (handler == lhandler) return true;
	return false;
}

bool E_IsStaticType(PClass* type)
{
	return (!type->IsDescendantOf(RUNTIME_CLASS(DEventHandler)) &&
			!type->IsDescendantOf(RUNTIME_CLASS(DRenderEventHandler)));
}

static void E_InitStaticHandler(PClass* type, FString typestring, bool map)
{
	if (type == nullptr)
	{
		Printf("%cGWarning: unknown event handler class %s in MAPINFO!", TEXTCOLOR_ESCAPE, typestring.GetChars());
		return;
	}

	if (!E_IsStaticType(type))
	{
		Printf("%cGWarning: invalid event handler class %s in MAPINFO!\nMAPINFO event handlers should inherit Static* directly!", TEXTCOLOR_ESCAPE, typestring.GetChars());
		return;
	}

	// check if type already exists, don't add twice.
	bool typeExists = false;
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (handler->IsA(type))
		{
			typeExists = true;
			break;
		}
	}

	if (typeExists) return;
	DStaticEventHandler* handler = (DStaticEventHandler*)type->CreateNew();
	handler->isMapScope = map;
	E_RegisterHandler(handler);
}

void E_InitStaticHandlers(bool map)
{
	// remove existing
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (handler->IsStatic() && handler->isMapScope == map)
			handler->Destroy();
	}

	// add new
	if (map)
	{
		for (unsigned int i = 0; i < level.info->EventHandlers.Size(); i++)
		{
			FString typestring = level.info->EventHandlers[i];
			PClass* type = PClass::FindClass(typestring);
			E_InitStaticHandler(type, typestring, true);
		}
	}
	else
	{
		for (unsigned int i = 0; i < gameinfo.EventHandlers.Size(); i++)
		{
			FString typestring = gameinfo.EventHandlers[i];
			PClass* type = PClass::FindClass(typestring);
			E_InitStaticHandler(type, typestring, false);
		}
	}
}

void E_MapLoaded()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->MapLoaded();
}

void E_MapUnloading()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->MapUnloading();
}

void E_RenderFrame()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->RenderFrame();
}

void E_RenderCamera()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->RenderCamera();
}

void E_RenderBeforeThing(AActor* thing)
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->RenderBeforeThing(thing);
}

void E_RenderAfterThing(AActor* thing)
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->RenderAfterThing(thing);
}

// declarations
IMPLEMENT_CLASS(DStaticEventHandler, false, false);
IMPLEMENT_CLASS(DStaticRenderEventHandler, false, false);
IMPLEMENT_CLASS(DEventHandler, false, false);
IMPLEMENT_CLASS(DRenderEventHandler, false, false);

DEFINE_FIELD_X(StaticRenderEventHandler, DStaticRenderEventHandler, ViewPos);
DEFINE_FIELD_X(StaticRenderEventHandler, DStaticRenderEventHandler, ViewAngle);
DEFINE_FIELD_X(StaticRenderEventHandler, DStaticRenderEventHandler, ViewPitch);
DEFINE_FIELD_X(StaticRenderEventHandler, DStaticRenderEventHandler, ViewRoll);
DEFINE_FIELD_X(StaticRenderEventHandler, DStaticRenderEventHandler, FracTic);
DEFINE_FIELD_X(StaticRenderEventHandler, DStaticRenderEventHandler, Camera);
DEFINE_FIELD_X(StaticRenderEventHandler, DStaticRenderEventHandler, CurrentThing);


DEFINE_ACTION_FUNCTION(DEventHandler, Create)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DStaticEventHandler);
	// check if type inherits dynamic handlers
	if (E_IsStaticType(t))
	{
		// disallow static types creation with Create()
		ACTION_RETURN_OBJECT(nullptr);
	}
	// generate a new object of this type.
	ACTION_RETURN_OBJECT(t->CreateNew());
}

DEFINE_ACTION_FUNCTION(DEventHandler, CreateOnce)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DStaticEventHandler);
	// check if type inherits dynamic handlers
	if (E_IsStaticType(t))
	{
		// disallow static types creation with Create()
		ACTION_RETURN_OBJECT(nullptr);
	}
	// check if there are already registered handlers of this type.
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		if (handler->GetClass() == t) // check precise class
			ACTION_RETURN_OBJECT(nullptr);
	// generate a new object of this type.
	ACTION_RETURN_OBJECT(t->CreateNew());
}

DEFINE_ACTION_FUNCTION(DEventHandler, Find)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DStaticEventHandler);
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		if (handler->GetClass() == t) // check precise class
			ACTION_RETURN_OBJECT(handler);
	ACTION_RETURN_OBJECT(nullptr);
}

DEFINE_ACTION_FUNCTION(DEventHandler, Register)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(handler, DStaticEventHandler);
	if (handler->IsStatic()) ACTION_RETURN_BOOL(false);
	ACTION_RETURN_BOOL(E_RegisterHandler(handler));
}

DEFINE_ACTION_FUNCTION(DEventHandler, Unregister)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(handler, DStaticEventHandler);
	if (handler->IsStatic()) ACTION_RETURN_BOOL(false);
	ACTION_RETURN_BOOL(E_UnregisterHandler(handler));
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

DEFINE_EVENT_HANDLER(DStaticEventHandler, MapLoaded,)
DEFINE_EVENT_HANDLER(DStaticEventHandler, MapUnloading,)
DEFINE_EVENT_HANDLER(DStaticEventHandler, RenderFrame,)
DEFINE_EVENT_HANDLER(DStaticEventHandler, RenderCamera,)
DEFINE_EVENT_HANDLER(DStaticEventHandler, RenderBeforeThing, AActor*)
DEFINE_EVENT_HANDLER(DStaticEventHandler, RenderAfterThing, AActor*)

//
void DStaticEventHandler::OnDestroy()
{
	E_UnregisterHandler(this);
	DObject::OnDestroy();
}

void DStaticRenderEventHandler::Setup()
{
	ViewPos = ::ViewPos;
	ViewAngle = ::ViewAngle;
	ViewPitch = ::ViewPitch;
	ViewRoll = ::ViewRoll;
	FracTic = ::r_TicFracF;
	Camera = ::camera;
}

void DStaticRenderEventHandler::RenderFrame()
{
	Setup();
	DStaticEventHandler::RenderFrame();
}

void DStaticRenderEventHandler::RenderCamera()
{
	Setup();
	DStaticEventHandler::RenderCamera();
}

void DStaticRenderEventHandler::RenderBeforeThing(AActor* thing)
{
	CurrentThing = thing;
	DStaticEventHandler::RenderBeforeThing(thing);
}

void DStaticRenderEventHandler::RenderAfterThing(AActor* thing)
{
	CurrentThing = thing;
	DStaticEventHandler::RenderAfterThing(thing);
}