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
	if (handler->IsStatic())
	{
		handler->ObjectFlags |= OF_Fixed;
	}
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
	if (handler->IsStatic())
	{
		handler->ObjectFlags &= ~OF_Fixed;
		handler->Destroy();
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
	return (type->IsDescendantOf(RUNTIME_CLASS(DStaticEventHandler)) && // make sure it's from our hierarchy at all.
			!type->IsDescendantOf(RUNTIME_CLASS(DEventHandler)) &&
			!type->IsDescendantOf(RUNTIME_CLASS(DRenderEventHandler)));
}

void E_SerializeEvents(FSerializer& arc)
{
	// todo : stuff
	if (arc.BeginArray("eventhandlers"))
	{
		int numlocalhandlers = 0;
		TArray<DStaticEventHandler*> handlers;
		if (arc.isReading())
		{
			numlocalhandlers = arc.ArraySize();
			// delete all current local handlers, if any
			for (DStaticEventHandler* lhandler = E_FirstEventHandler; lhandler; lhandler = lhandler->next)
				if (!lhandler->IsStatic()) lhandler->Destroy();
		}
		else
		{
			for (DStaticEventHandler* lhandler = E_FirstEventHandler; lhandler; lhandler = lhandler->next)
			{
				if (lhandler->IsStatic()) continue;
				numlocalhandlers++;
				handlers.Push(lhandler);
			}
		}

		for (int i = 0; i < numlocalhandlers; i++)
		{
			// serialize the object properly.
			if (arc.isReading())
			{
				// get object and put it into the array
				DStaticEventHandler* lhandler;
				arc(nullptr, lhandler);
				if (lhandler != nullptr)
					handlers.Push(lhandler);
			}
			else
			{
				::Serialize<DStaticEventHandler>(arc, nullptr, handlers[i], nullptr);
			}
		}

		if (arc.isReading())
		{
			// add all newly deserialized handlers into the list
			for (int i = 0; i < numlocalhandlers; i++)
				E_RegisterHandler(handlers[i]);
		}

		arc.EndArray();
	}
}

static void E_InitStaticHandler(PClass* type, FString typestring, bool map)
{
	if (type == nullptr)
	{
		Printf("%cGWarning: unknown event handler class %s in MAPINFO!\n", TEXTCOLOR_ESCAPE, typestring.GetChars());
		return;
	}

	if (!E_IsStaticType(type))
	{
		Printf("%cGWarning: invalid event handler class %s in MAPINFO!\nMAPINFO event handlers should inherit Static* directly!\n", TEXTCOLOR_ESCAPE, typestring.GetChars());
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
	if (savegamerestore)
		return;

	if (map) // don't initialize map handlers if restoring from savegame.
	{
		// delete old map static handlers if any.
		for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		{
			if (handler->IsStatic() && handler->isMapScope)
				handler->Destroy();
		}

		for (unsigned int i = 0; i < level.info->EventHandlers.Size(); i++)
		{
			FString typestring = level.info->EventHandlers[i];
			PClass* type = PClass::FindClass(typestring);
			E_InitStaticHandler(type, typestring, true);
		}
	}
	else
	{
		// delete old static handlers if any.
		for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		{
			if (handler->IsStatic() && !handler->isMapScope)
				handler->Destroy();
		}

		for (unsigned int i = 0; i < gameinfo.EventHandlers.Size(); i++)
		{
			FString typestring = gameinfo.EventHandlers[i];
			PClass* type = PClass::FindClass(typestring);
			E_InitStaticHandler(type, typestring, false);
		}
	}
}

#define DEFINE_EVENT_LOOPER(name) void E_##name() \
{ \
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next) \
		handler->name(); \
}

// note for the functions below.
// *Unsafe is executed on EVERY map load/close, including savegame loading, etc.
// There is no point in allowing non-static handlers to receive unsafe event separately, as there is no point in having static handlers receive safe event.
// Because the main point of safe WorldLoaded/Unloading is that it will be preserved in savegames.
void E_WorldLoaded()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (handler->IsStatic() && !handler->isMapScope) continue;
		if (handler->isMapScope && savegamerestore) continue; // don't execute WorldLoaded for handlers loaded from the savegame.
		handler->WorldLoaded();
	}
}

void E_WorldUnloading()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (handler->IsStatic() && !handler->isMapScope) continue;
		handler->WorldUnloading();
	}
}

void E_WorldLoadedUnsafe()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (!handler->IsStatic() || handler->isMapScope) continue;
		handler->WorldLoaded();
	}
}

void E_WorldUnloadingUnsafe()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (!handler->IsStatic() || handler->isMapScope) continue;
		handler->WorldUnloading();
	}
}

DEFINE_EVENT_LOOPER(RenderFrame)

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

DEFINE_EVENT_HANDLER(DStaticEventHandler, WorldLoaded,)
DEFINE_EVENT_HANDLER(DStaticEventHandler, WorldUnloading,)
DEFINE_EVENT_HANDLER(DStaticEventHandler, RenderFrame, )

//
void DStaticEventHandler::OnDestroy()
{
	E_UnregisterHandler(this);
	Super::OnDestroy();
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
	Super::RenderFrame();
}
