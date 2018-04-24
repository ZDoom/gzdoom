/*
**
**
**---------------------------------------------------------------------------
** Copyright 2017 ZZYZX
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
#include "events.h"
#include "vm.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "gi.h"
#include "actor.h"
#include "c_dispatch.h"
#include "d_net.h"

DStaticEventHandler* E_FirstEventHandler = nullptr;
DStaticEventHandler* E_LastEventHandler = nullptr;

bool E_RegisterHandler(DStaticEventHandler* handler)
{
	if (handler == nullptr || handler->ObjectFlags & OF_EuthanizeMe)
		return false;
	if (E_CheckHandler(handler))
		return false;

	handler->OnRegister();
	
	// link into normal list
	// update: link at specific position based on order.
	DStaticEventHandler* before = nullptr;
	for (DStaticEventHandler* existinghandler = E_FirstEventHandler; existinghandler; existinghandler = existinghandler->next)
	{
		if (existinghandler->Order > handler->Order)
		{
			before = existinghandler;
			break;
		}
	}

	// 1. MyHandler2->1:
	//    E_FirstEventHandler = MyHandler2, E_LastEventHandler = MyHandler2
	// 2. MyHandler3->2:
	//    E_FirstEventHandler = MyHandler2, E_LastEventHandler = MyHandler3

	// (Yes, all those write barriers here are really needed!)
	if (before != nullptr)
	{
		// if before is not null, link it before the existing handler.
		// note that before can be first handler, check for this.
		handler->next = before;
		GC::WriteBarrier(handler, before);
		handler->prev = before->prev;
		GC::WriteBarrier(handler, before->prev);
		before->prev = handler;
		GC::WriteBarrier(before, handler);
		if (before == E_FirstEventHandler)
		{
			E_FirstEventHandler = handler;
			GC::WriteBarrier(handler);
		}
	}
	else
	{
		// so if before is null, it means add last.
		// it can also mean that we have no handlers at all yet.
		handler->prev = E_LastEventHandler;
		GC::WriteBarrier(handler, E_LastEventHandler);
		handler->next = nullptr;
		if (E_FirstEventHandler == nullptr)	E_FirstEventHandler = handler;
		E_LastEventHandler = handler;
		GC::WriteBarrier(handler);
		if (handler->prev != nullptr)
		{
			handler->prev->next = handler;
			GC::WriteBarrier(handler->prev, handler);
		}
	}

	if (handler->IsStatic())
	{
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

	handler->OnUnregister();

	// link out of normal list
	if (handler->prev)
	{
		handler->prev->next = handler->next;
		GC::WriteBarrier(handler->prev, handler->next);
	}
	if (handler->next)
	{
		handler->next->prev = handler->prev;
		GC::WriteBarrier(handler->next, handler->prev);
	}
	if (handler == E_FirstEventHandler)
	{
		E_FirstEventHandler = handler->next;
		GC::WriteBarrier(handler->next);
	}
	if (handler == E_LastEventHandler)
	{
		E_LastEventHandler = handler->prev;
		GC::WriteBarrier(handler->prev);
	}
	if (handler->IsStatic())
	{
		handler->ObjectFlags &= ~OF_Transient;
		handler->Destroy();
	}
	return true;
}

bool E_SendNetworkEvent(FString name, int arg1, int arg2, int arg3, bool manual)
{
	if (gamestate != GS_LEVEL)
		return false;

	Net_WriteByte(DEM_NETEVENT);
	Net_WriteString(name);
	Net_WriteByte(3);
	Net_WriteLong(arg1);
	Net_WriteLong(arg2);
	Net_WriteLong(arg3);
	Net_WriteByte(manual);

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
	assert(type != nullptr);
	assert(type->IsDescendantOf(RUNTIME_CLASS(DStaticEventHandler)));
	return !type->IsDescendantOf(RUNTIME_CLASS(DEventHandler));
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

static PClass* E_GetHandlerClass(const FString& typeName)
{
	PClass* type = PClass::FindClass(typeName);

	if (type == nullptr)
	{
		I_Error("Fatal: unknown event handler class %s", typeName.GetChars());
	}
	else if (!type->IsDescendantOf(RUNTIME_CLASS(DStaticEventHandler)))
	{
		I_Error("Fatal: event handler class %s is not derived from StaticEventHandler", typeName.GetChars());
	}

	return type;
}

static void E_InitHandler(PClass* type)
{
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
	E_RegisterHandler(handler);
}

void E_InitStaticHandlers(bool map)
{
	// don't initialize map handlers if restoring from savegame.
	if (savegamerestore)
		return;

	// just make sure
	E_Shutdown(map);

	// initialize event handlers from gameinfo
	for (const FString& typeName : gameinfo.EventHandlers)
	{
		PClass* type = E_GetHandlerClass(typeName);
		// don't init the really global stuff here on startup initialization.
		// don't init map-local global stuff here on level setup.
		if (map == E_IsStaticType(type))
			continue;
		E_InitHandler(type);
	}

	if (!map) 
		return;

	// initialize event handlers from mapinfo
	for (const FString& typeName : level.info->EventHandlers)
	{
		PClass* type = E_GetHandlerClass(typeName);
		if (E_IsStaticType(type))
			I_Error("Fatal: invalid event handler class %s in MAPINFO!\nMap-specific event handlers cannot be static.\n", typeName.GetChars());
		E_InitHandler(type);
	}
}

void E_Shutdown(bool map)
{
	// delete handlers.
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (handler->IsStatic() == !map)
			handler->Destroy();
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
		if (handler->IsStatic()) continue;
		if (savegamerestore) continue; // don't execute WorldLoaded for handlers loaded from the savegame.
		handler->WorldLoaded();
	}
}

void E_WorldUnloaded()
{
	for (DStaticEventHandler* handler = E_LastEventHandler; handler; handler = handler->prev)
	{
		if (handler->IsStatic()) continue;
		handler->WorldUnloaded();
	}
}

void E_WorldLoadedUnsafe()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
	{
		if (!handler->IsStatic()) continue;
		handler->WorldLoaded();
	}
}

void E_WorldUnloadedUnsafe()
{
	for (DStaticEventHandler* handler = E_LastEventHandler; handler; handler = handler->prev)
	{
		if (!handler->IsStatic()) continue;
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
	// don't call anything for non-spawned things (i.e. those that were created, but immediately destroyed)
	// this is because Destroyed should be reverse of Spawned. we don't want to catch random inventory give failures.
	if (!(actor->ObjectFlags & OF_Spawned))
		return;
	for (DStaticEventHandler* handler = E_LastEventHandler; handler; handler = handler->prev)
		handler->WorldThingDestroyed(actor);
}

void E_WorldLinePreActivated(line_t* line, AActor* actor, int activationType, bool* shouldactivate)
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->WorldLinePreActivated(line, actor, activationType, shouldactivate);
}

void E_WorldLineActivated(line_t* line, AActor* actor, int activationType)
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->WorldLineActivated(line, actor, activationType);
}

void E_PlayerEntered(int num, bool fromhub)
{
	// this event can happen during savegamerestore. make sure that local handlers don't receive it.
	// actually, global handlers don't want it too.
	if (savegamerestore && !fromhub)
		return;

	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->PlayerEntered(num, fromhub);
}

void E_PlayerRespawned(int num)
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->PlayerRespawned(num);
}

void E_PlayerDied(int num)
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->PlayerDied(num);
}

void E_PlayerDisconnected(int num)
{
	for (DStaticEventHandler* handler = E_LastEventHandler; handler; handler = handler->prev)
		handler->PlayerDisconnected(num);
}

bool E_Responder(const event_t* ev)
{
	bool uiProcessorsFound = false;

	if (ev->type == EV_GUI_Event)
	{
		// iterate handlers back to front by order, and give them this event.
		for (DStaticEventHandler* handler = E_LastEventHandler; handler; handler = handler->prev)
		{
			if (handler->IsUiProcessor)
			{
				uiProcessorsFound = true;
				if (handler->UiProcess(ev))
					return true; // event was processed
			}
		}
	}
	else
	{
		// not sure if we want to handle device changes, but whatevs.
		for (DStaticEventHandler* handler = E_LastEventHandler; handler; handler = handler->prev)
		{
			if (handler->IsUiProcessor)
				uiProcessorsFound = true;
			if (handler->InputProcess(ev))
				return true; // event was processed
		}
	}

	return (uiProcessorsFound && (ev->type == EV_Mouse)); // mouse events are eaten by the event system if there are any uiprocessors.
}

void E_Console(int player, FString name, int arg1, int arg2, int arg3, bool manual)
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->ConsoleProcess(player, name, arg1, arg2, arg3, manual);
}

void E_RenderOverlay(EHudState state)
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		handler->RenderOverlay(state);
}

bool E_CheckUiProcessors()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		if (handler->IsUiProcessor)
			return true;

	return false;
}

bool E_CheckRequireMouse()
{
	for (DStaticEventHandler* handler = E_FirstEventHandler; handler; handler = handler->next)
		if (handler->IsUiProcessor && handler->RequireMouse)
			return true;

	return false;
}

// normal event loopers (non-special, argument-less)
DEFINE_EVENT_LOOPER(RenderFrame)
DEFINE_EVENT_LOOPER(WorldLightning)
DEFINE_EVENT_LOOPER(WorldTick)
DEFINE_EVENT_LOOPER(UiTick)
DEFINE_EVENT_LOOPER(PostUiTick)

// declarations
IMPLEMENT_CLASS(DStaticEventHandler, false, true);

IMPLEMENT_POINTERS_START(DStaticEventHandler)
IMPLEMENT_POINTER(next)
IMPLEMENT_POINTER(prev)
IMPLEMENT_POINTERS_END

IMPLEMENT_CLASS(DEventHandler, false, false);

DEFINE_FIELD_X(StaticEventHandler, DStaticEventHandler, Order);
DEFINE_FIELD_X(StaticEventHandler, DStaticEventHandler, IsUiProcessor);
DEFINE_FIELD_X(StaticEventHandler, DStaticEventHandler, RequireMouse);

DEFINE_FIELD_X(RenderEvent, FRenderEvent, ViewPos);
DEFINE_FIELD_X(RenderEvent, FRenderEvent, ViewAngle);
DEFINE_FIELD_X(RenderEvent, FRenderEvent, ViewPitch);
DEFINE_FIELD_X(RenderEvent, FRenderEvent, ViewRoll);
DEFINE_FIELD_X(RenderEvent, FRenderEvent, FracTic);
DEFINE_FIELD_X(RenderEvent, FRenderEvent, Camera);

DEFINE_FIELD_X(WorldEvent, FWorldEvent, IsSaveGame);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, IsReopen);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, Thing);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, Inflictor);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, Damage);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamageSource);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamageType);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamageFlags);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamageAngle);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, ActivatedLine);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, ActivationType);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, ShouldActivate);

DEFINE_FIELD_X(PlayerEvent, FPlayerEvent, PlayerNumber);
DEFINE_FIELD_X(PlayerEvent, FPlayerEvent, IsReturn);

DEFINE_FIELD_X(UiEvent, FUiEvent, Type);
DEFINE_FIELD_X(UiEvent, FUiEvent, KeyString);
DEFINE_FIELD_X(UiEvent, FUiEvent, KeyChar);
DEFINE_FIELD_X(UiEvent, FUiEvent, MouseX);
DEFINE_FIELD_X(UiEvent, FUiEvent, MouseY);
DEFINE_FIELD_X(UiEvent, FUiEvent, IsShift);
DEFINE_FIELD_X(UiEvent, FUiEvent, IsAlt);
DEFINE_FIELD_X(UiEvent, FUiEvent, IsCtrl);

DEFINE_FIELD_X(InputEvent, FInputEvent, Type);
DEFINE_FIELD_X(InputEvent, FInputEvent, KeyScan);
DEFINE_FIELD_X(InputEvent, FInputEvent, KeyString);
DEFINE_FIELD_X(InputEvent, FInputEvent, KeyChar);
DEFINE_FIELD_X(InputEvent, FInputEvent, MouseX);
DEFINE_FIELD_X(InputEvent, FInputEvent, MouseY);

DEFINE_FIELD_X(ConsoleEvent, FConsoleEvent, Player)
DEFINE_FIELD_X(ConsoleEvent, FConsoleEvent, Name)
DEFINE_FIELD_X(ConsoleEvent, FConsoleEvent, Args)
DEFINE_FIELD_X(ConsoleEvent, FConsoleEvent, IsManual)

DEFINE_ACTION_FUNCTION(DStaticEventHandler, SetOrder)
{
	PARAM_SELF_PROLOGUE(DStaticEventHandler);
	PARAM_INT(order);

	if (E_CheckHandler(self))
		return 0;

	self->Order = order;
	return 0;
}

DEFINE_ACTION_FUNCTION(DEventHandler, SendNetworkEvent)
{
	PARAM_PROLOGUE;
	PARAM_STRING(name);
	PARAM_INT_DEF(arg1);
	PARAM_INT_DEF(arg2);
	PARAM_INT_DEF(arg3);
	//

	ACTION_RETURN_BOOL(E_SendNetworkEvent(name, arg1, arg2, arg3, false));
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

#define DEFINE_EMPTY_HANDLER(cls, funcname) DEFINE_ACTION_FUNCTION(cls, funcname) \
{ \
	PARAM_SELF_PROLOGUE(cls); \
	return 0; \
}

DEFINE_EMPTY_HANDLER(DStaticEventHandler, OnRegister)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, OnUnregister)

DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldLoaded)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldUnloaded)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingSpawned)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingDied)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingRevived)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingDamaged)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldThingDestroyed)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldLinePreActivated)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldLineActivated)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldLightning)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, WorldTick)

DEFINE_EMPTY_HANDLER(DStaticEventHandler, RenderFrame)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, RenderOverlay)

DEFINE_EMPTY_HANDLER(DStaticEventHandler, PlayerEntered)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, PlayerRespawned)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, PlayerDied)
DEFINE_EMPTY_HANDLER(DStaticEventHandler, PlayerDisconnected)

DEFINE_EMPTY_HANDLER(DStaticEventHandler, UiProcess);
DEFINE_EMPTY_HANDLER(DStaticEventHandler, InputProcess);
DEFINE_EMPTY_HANDLER(DStaticEventHandler, UiTick);
DEFINE_EMPTY_HANDLER(DStaticEventHandler, PostUiTick);

DEFINE_EMPTY_HANDLER(DStaticEventHandler, ConsoleProcess);
DEFINE_EMPTY_HANDLER(DStaticEventHandler, NetworkProcess);

// ===========================================
//
//  Event handlers
//
// ===========================================

void DStaticEventHandler::OnRegister()
{
	IFVIRTUAL(DStaticEventHandler, OnRegister)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_OnRegister_VMPtr)
			return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

void DStaticEventHandler::OnUnregister()
{
	IFVIRTUAL(DStaticEventHandler, OnUnregister)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_OnUnregister_VMPtr)
			return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

static FWorldEvent E_SetupWorldEvent()
{
	FWorldEvent e;
	e.IsSaveGame = savegamerestore;
	e.IsReopen = level.FromSnapshot && !savegamerestore; // each one by itself isnt helpful, but with hub load we have savegamerestore==0 and level.FromSnapshot==1.
	e.DamageAngle = 0.0;
	return e;
}

void DStaticEventHandler::WorldLoaded()
{
	IFVIRTUAL(DStaticEventHandler, WorldLoaded)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldLoaded_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldUnloaded()
{
	IFVIRTUAL(DStaticEventHandler, WorldUnloaded)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldUnloaded_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldThingSpawned(AActor* actor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingSpawned)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingSpawned_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		e.Thing = actor;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldThingDied(AActor* actor, AActor* inflictor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingDied)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingDied_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		e.Thing = actor;
		e.Inflictor = inflictor;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldThingRevived(AActor* actor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingRevived)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingRevived_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		e.Thing = actor;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldThingDamaged(AActor* actor, AActor* inflictor, AActor* source, int damage, FName mod, int flags, DAngle angle)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingDamaged)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingDamaged_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		e.Thing = actor;
		e.Inflictor = inflictor;
		e.Damage = damage;
		e.DamageSource = source;
		e.DamageType = mod;
		e.DamageFlags = flags;
		e.DamageAngle = angle;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldThingDestroyed(AActor* actor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingDestroyed)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldThingDestroyed_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		e.Thing = actor;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldLinePreActivated(line_t* line, AActor* actor, int activationType, bool* shouldactivate)
{
	IFVIRTUAL(DStaticEventHandler, WorldLinePreActivated)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldLinePreActivated_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		e.Thing = actor;
		e.ActivatedLine = line;
		e.ActivationType = activationType;
		e.ShouldActivate = *shouldactivate;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
		*shouldactivate = e.ShouldActivate;
	}
}

void DStaticEventHandler::WorldLineActivated(line_t* line, AActor* actor, int activationType)
{
	IFVIRTUAL(DStaticEventHandler, WorldLineActivated)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldLineActivated_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		e.Thing = actor;
		e.ActivatedLine = line;
		e.ActivationType = activationType;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldLightning()
{
	IFVIRTUAL(DStaticEventHandler, WorldLightning)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldLightning_VMPtr)
			return;
		FWorldEvent e = E_SetupWorldEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldTick()
{
	IFVIRTUAL(DStaticEventHandler, WorldTick)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_WorldTick_VMPtr)
			return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

static FRenderEvent E_SetupRenderEvent()
{
	FRenderEvent e;
	e.ViewPos = r_viewpoint.Pos;
	e.ViewAngle = r_viewpoint.Angles.Yaw;
	e.ViewPitch = r_viewpoint.Angles.Pitch;
	e.ViewRoll = r_viewpoint.Angles.Roll;
	e.FracTic = r_viewpoint.TicFrac;
	e.Camera = r_viewpoint.camera;
	return e;
}

void DStaticEventHandler::RenderFrame()
{
	IFVIRTUAL(DStaticEventHandler, RenderFrame)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_RenderFrame_VMPtr)
			return;
		FRenderEvent e = E_SetupRenderEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::RenderOverlay(EHudState state)
{
	IFVIRTUAL(DStaticEventHandler, RenderOverlay)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_RenderOverlay_VMPtr)
			return;
		FRenderEvent e = E_SetupRenderEvent();
		e.HudState = int(state);
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::PlayerEntered(int num, bool fromhub)
{
	IFVIRTUAL(DStaticEventHandler, PlayerEntered)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_PlayerEntered_VMPtr)
			return;
		FPlayerEvent e = { num, fromhub };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::PlayerRespawned(int num)
{
	IFVIRTUAL(DStaticEventHandler, PlayerRespawned)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_PlayerRespawned_VMPtr)
			return;
		FPlayerEvent e = { num, false };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::PlayerDied(int num)
{
	IFVIRTUAL(DStaticEventHandler, PlayerDied)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_PlayerDied_VMPtr)
			return;
		FPlayerEvent e = { num, false };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::PlayerDisconnected(int num)
{
	IFVIRTUAL(DStaticEventHandler, PlayerDisconnected)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_PlayerDisconnected_VMPtr)
			return;
		FPlayerEvent e = { num, false };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

FUiEvent::FUiEvent(const event_t *ev)
{
	Type = (EGUIEvent)ev->subtype;
	KeyChar = 0;
	IsShift = false;
	IsAlt = false;
	IsCtrl = false;
	MouseX = 0;
	MouseY = 0;
	// we don't want the modders to remember what weird fields mean what for what events.
	switch (ev->subtype)
	{
	case EV_GUI_None:
		break;
	case EV_GUI_KeyDown:
	case EV_GUI_KeyRepeat:
	case EV_GUI_KeyUp:
		KeyChar = ev->data1;
		KeyString = FString(char(ev->data1));
		IsShift = !!(ev->data3 & GKM_SHIFT);
		IsAlt = !!(ev->data3 & GKM_ALT);
		IsCtrl = !!(ev->data3 & GKM_CTRL);
		break;
	case EV_GUI_Char:
		KeyChar = ev->data1;
		KeyString = FString(char(ev->data1));
		IsAlt = !!ev->data2; // only true for Win32, not sure about SDL
		break;
	default: // mouse event
			 // note: SDL input doesn't seem to provide these at all
			 //Printf("Mouse data: %d, %d, %d, %d\n", ev->x, ev->y, ev->data1, ev->data2);
		MouseX = ev->data1;
		MouseY = ev->data2;
		IsShift = !!(ev->data3 & GKM_SHIFT);
		IsAlt = !!(ev->data3 & GKM_ALT);
		IsCtrl = !!(ev->data3 & GKM_CTRL);
		break;
	}
}

bool DStaticEventHandler::UiProcess(const event_t* ev)
{
	IFVIRTUAL(DStaticEventHandler, UiProcess)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_UiProcess_VMPtr)
			return false;
		FUiEvent e = ev;

		int processed;
		VMReturn results[1] = { &processed };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, results, 1);
		return !!processed;
	}

	return false;
}

FInputEvent::FInputEvent(const event_t *ev)
{
	Type = (EGenericEvent)ev->type;
	// we don't want the modders to remember what weird fields mean what for what events.
	KeyScan = 0;
	KeyChar = 0;
	MouseX = 0;
	MouseY = 0;
	switch (Type)
	{
	case EV_None:
		break;
	case EV_KeyDown:
	case EV_KeyUp:
		KeyScan = ev->data1;
		KeyChar = ev->data2;
		KeyString = FString(char(ev->data1));
		break;
	case EV_Mouse:
		MouseX = ev->x;
		MouseY = ev->y;
		break;
	default:
		break; // EV_DeviceChange = wat?
	}
}

bool DStaticEventHandler::InputProcess(const event_t* ev)
{
	IFVIRTUAL(DStaticEventHandler, InputProcess)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_InputProcess_VMPtr)
			return false;

		FInputEvent e = ev;
		//

		int processed;
		VMReturn results[1] = { &processed };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, results, 1);
		return !!processed;
	}

	return false;
}

void DStaticEventHandler::UiTick()
{
	IFVIRTUAL(DStaticEventHandler, UiTick)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_UiTick_VMPtr)
			return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

void DStaticEventHandler::PostUiTick()
{
	IFVIRTUAL(DStaticEventHandler, PostUiTick)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (func == DStaticEventHandler_PostUiTick_VMPtr)
			return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

void DStaticEventHandler::ConsoleProcess(int player, FString name, int arg1, int arg2, int arg3, bool manual)
{
	if (player < 0)
	{
		IFVIRTUAL(DStaticEventHandler, ConsoleProcess)
		{
			// don't create excessive DObjects if not going to be processed anyway
			if (func == DStaticEventHandler_ConsoleProcess_VMPtr)
				return;
			FConsoleEvent e;

			//
			e.Player = player;
			e.Name = name;
			e.Args[0] = arg1;
			e.Args[1] = arg2;
			e.Args[2] = arg3;
			e.IsManual = manual;

			VMValue params[2] = { (DStaticEventHandler*)this, &e };
			VMCall(func, params, 2, nullptr, 0);
		}
	}
	else
	{
		IFVIRTUAL(DStaticEventHandler, NetworkProcess)
		{
			// don't create excessive DObjects if not going to be processed anyway
			if (func == DStaticEventHandler_NetworkProcess_VMPtr)
				return;
			FConsoleEvent e;

			//
			e.Player = player;
			e.Name = name;
			e.Args[0] = arg1;
			e.Args[1] = arg2;
			e.Args[2] = arg3;
			e.IsManual = manual;

			VMValue params[2] = { (DStaticEventHandler*)this, &e };
			VMCall(func, params, 2, nullptr, 0);
		}
	}
}

//
void DStaticEventHandler::OnDestroy()
{
	E_UnregisterHandler(this);
	Super::OnDestroy();
}

// console stuff
// this is kinda like puke, except it distinguishes between local events and playsim events.
CCMD(event)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 5)
	{
		Printf("Usage: event <name> [arg1] [arg2] [arg3]\n");
	}
	else
	{
		int arg[3] = { 0, 0, 0 };
		int argn = MIN<int>(argc - 2, countof(arg));
		for (int i = 0; i < argn; i++)
			arg[i] = atoi(argv[2 + i]);
		// call locally
		E_Console(-1, argv[1], arg[0], arg[1], arg[2], true);
	}
}

CCMD(netevent)
{
	if (gamestate != GS_LEVEL/* && gamestate != GS_TITLELEVEL*/) // not sure if this should work in title level, but probably not, because this is for actual playing
	{
		Printf("netevent cannot be used outside of a map.\n");
		return;
	}

	int argc = argv.argc();

	if (argc < 2 || argc > 5)
	{
		Printf("Usage: netevent <name> [arg1] [arg2] [arg3]\n");
	}
	else
	{
		int arg[3] = { 0, 0, 0 };
		int argn = MIN<int>(argc - 2, countof(arg));
		for (int i = 0; i < argn; i++)
			arg[i] = atoi(argv[2 + i]);
		// call networked
		E_SendNetworkEvent(argv[1], arg[0], arg[1], arg[2], true);
	}
}
