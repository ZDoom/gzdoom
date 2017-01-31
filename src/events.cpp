#include "events.h"
#include "virtual.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "gi.h"
#include "v_text.h"
#include "actor.h"

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
		if (!handler->isMapScope) // global (GameInfo) handlers are not serialized.
			handler->ObjectFlags |= OF_Transient;
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
		handler->ObjectFlags &= ~(OF_Fixed|OF_Transient);
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
			!type->IsDescendantOf(RUNTIME_CLASS(DEventHandler)));
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

void E_WorldUnloaded()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (handler->IsStatic() && !handler->isMapScope) continue;
		handler->WorldUnloaded();
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

void E_WorldUnloadedUnsafe()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (!handler->IsStatic() || handler->isMapScope) continue;
		handler->WorldUnloaded();
	}
}

void E_WorldThingSpawned(AActor* actor)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingSpawned(actor);
}

void E_WorldThingDied(AActor* actor, AActor* inflictor)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingDied(actor, inflictor);
}

void E_WorldThingRevived(AActor* actor)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingRevived(actor);
}

void E_WorldThingDamaged(AActor* actor, AActor* inflictor, AActor* source, int damage, FName mod, int flags, DAngle angle)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingDamaged(actor, inflictor, source, damage, mod, flags, angle);
}

void E_WorldThingDestroyed(AActor* actor)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingDestroyed(actor);
}

// normal event loopers (non-special, argument-less)
DEFINE_EVENT_LOOPER(RenderFrame)
DEFINE_EVENT_LOOPER(WorldLightning)
DEFINE_EVENT_LOOPER(WorldTick)

// declarations
IMPLEMENT_CLASS(DStaticEventHandler, false, false);
IMPLEMENT_CLASS(DEventHandler, false, false);
IMPLEMENT_CLASS(DBaseEvent, false, false)
IMPLEMENT_CLASS(DRenderEvent, false, false)
IMPLEMENT_CLASS(DWorldEvent, false, false)

DEFINE_FIELD_X(RenderEvent, DRenderEvent, ViewPos);
DEFINE_FIELD_X(RenderEvent, DRenderEvent, ViewAngle);
DEFINE_FIELD_X(RenderEvent, DRenderEvent, ViewPitch);
DEFINE_FIELD_X(RenderEvent, DRenderEvent, ViewRoll);
DEFINE_FIELD_X(RenderEvent, DRenderEvent, FracTic);
DEFINE_FIELD_X(RenderEvent, DRenderEvent, Camera);

DEFINE_FIELD_X(WorldEvent, DWorldEvent, IsSaveGame);
DEFINE_FIELD_X(WorldEvent, DWorldEvent, Thing);
DEFINE_FIELD_X(WorldEvent, DWorldEvent, Inflictor);
DEFINE_FIELD_X(WorldEvent, DWorldEvent, Damage);
DEFINE_FIELD_X(WorldEvent, DWorldEvent, DamageSource);
DEFINE_FIELD_X(WorldEvent, DWorldEvent, DamageType);
DEFINE_FIELD_X(WorldEvent, DWorldEvent, DamageFlags);
DEFINE_FIELD_X(WorldEvent, DWorldEvent, DamageAngle);

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
			ACTION_RETURN_OBJECT(handler);
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

// for static
DEFINE_ACTION_FUNCTION(DStaticEventHandler, Create)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DStaticEventHandler);
	// static handlers can create any type of object.
	// generate a new object of this type.
	ACTION_RETURN_OBJECT(t->CreateNew());
}

DEFINE_ACTION_FUNCTION(DStaticEventHandler, CreateOnce)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DStaticEventHandler);
	// static handlers can create any type of object.
	// check if there are already registered handlers of this type.
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		if (handler->GetClass() == t) // check precise class
			ACTION_RETURN_OBJECT(handler);
	// generate a new object of this type.
	ACTION_RETURN_OBJECT(t->CreateNew());
}

// we might later want to change this
DEFINE_ACTION_FUNCTION(DStaticEventHandler, Find)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DStaticEventHandler);
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		if (handler->GetClass() == t) // check precise class
			ACTION_RETURN_OBJECT(handler);
	ACTION_RETURN_OBJECT(nullptr);
}

DEFINE_ACTION_FUNCTION(DStaticEventHandler, Register)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(handler, DStaticEventHandler);
	ACTION_RETURN_BOOL(E_RegisterHandler(handler));
}

DEFINE_ACTION_FUNCTION(DStaticEventHandler, Unregister)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(handler, DStaticEventHandler);
	ACTION_RETURN_BOOL(E_UnregisterHandler(handler));
}

#define DEFINE_EMPTY_HANDLER(cls, funcname) DEFINE_ACTION_FUNCTION(cls, funcname) \
{ \
	PARAM_SELF_PROLOGUE(cls); \
	return 0; \
}

DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldLoaded)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldUnloaded)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingSpawned)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingDied)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingRevived)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingDamaged)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingDestroyed)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldLightning)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldTick)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, RenderFrame)

// ===========================================
//
//  Event handlers
//
// ===========================================

static DWorldEvent* E_SetupWorldEvent()
{
	static DWorldEvent* e = nullptr;
	if (!e) e = (DWorldEvent*)RUNTIME_CLASS(DWorldEvent)->CreateNew();
	e->IsSaveGame = savegamerestore;
	e->Thing = nullptr;
	e->Inflictor = nullptr;
	e->Damage = 0;
	e->DamageAngle = 0.0;
	e->DamageFlags = 0;
	e->DamageSource = 0;
	e->DamageType = NAME_None;
	return e;
}

void DStaticEventHandler::WorldLoaded()
{
	IFVIRTUAL(DStaticEventHandler, WorldLoaded)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldLoaded_VMPtr)
			return;
		DWorldEvent* e = E_SetupWorldEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

void DStaticEventHandler::WorldUnloaded()
{
	IFVIRTUAL(DStaticEventHandler, WorldUnloaded)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldUnloaded_VMPtr)
			return;
		DWorldEvent* e = E_SetupWorldEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

void DStaticEventHandler::WorldThingSpawned(AActor* actor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingSpawned)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingSpawned_VMPtr)
			return;
		DWorldEvent* e = E_SetupWorldEvent();
		e->Thing = actor;
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

void DStaticEventHandler::WorldThingDied(AActor* actor, AActor* inflictor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingDied)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingDied_VMPtr)
			return;
		DWorldEvent* e = E_SetupWorldEvent();
		e->Thing = actor;
		e->Inflictor = inflictor;
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

void DStaticEventHandler::WorldThingRevived(AActor* actor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingRevived)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingRevived_VMPtr)
			return;
		DWorldEvent* e = E_SetupWorldEvent();
		e->Thing = actor;
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

void DStaticEventHandler::WorldThingDamaged(AActor* actor, AActor* inflictor, AActor* source, int damage, FName mod, int flags, DAngle angle)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingDamaged)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingDamaged_VMPtr)
			return;
		DWorldEvent* e = E_SetupWorldEvent();
		e->Thing = actor;
		e->Damage = damage;
		e->DamageSource = source;
		e->DamageType = mod;
		e->DamageFlags = flags;
		e->DamageAngle = angle;
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

void DStaticEventHandler::WorldThingDestroyed(AActor* actor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingDestroyed)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingDestroyed_VMPtr)
			return;
		DWorldEvent* e = E_SetupWorldEvent();
		e->Thing = actor;
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

void DStaticEventHandler::WorldLightning()
{
	IFVIRTUAL(DStaticEventHandler, WorldLightning)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldLightning_VMPtr)
			return;
		DWorldEvent* e = E_SetupWorldEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

void DStaticEventHandler::WorldTick()
{
	IFVIRTUAL(DStaticEventHandler, WorldTick)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldTick_VMPtr)
			return;
		DWorldEvent* e = E_SetupWorldEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

static DRenderEvent* E_SetupRenderEvent()
{
	static DRenderEvent* e = nullptr;
	if (!e) e = (DRenderEvent*)RUNTIME_CLASS(DRenderEvent)->CreateNew();
	e->ViewPos = ::ViewPos;
	e->ViewAngle = ::ViewAngle;
	e->ViewPitch = ::ViewPitch;
	e->ViewRoll = ::ViewRoll;
	e->FracTic = ::r_TicFracF;
	e->Camera = ::camera;
	return e;
}

void DStaticEventHandler::RenderFrame()
{
	IFVIRTUAL(DStaticEventHandler, RenderFrame)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_RenderFrame_VMPtr)
			return;
		DRenderEvent* e = E_SetupRenderEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, e };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}

//
void DStaticEventHandler::OnDestroy()
{
	E_UnregisterHandler(this);
	Super::OnDestroy();
}
