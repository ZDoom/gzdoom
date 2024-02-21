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
#include "vmintern.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "gi.h"
#include "actor.h"
#include "c_dispatch.h"
#include "d_net.h"
#include "g_game.h"
#include "info.h"
#include "utf8.h"

EventManager staticEventManager;

static int ListGetInt(VMVa_List& tags)
{
	if (tags.curindex < tags.numargs)
	{
		if (tags.reginfo[tags.curindex] == REGT_FLOAT)
			return static_cast<int>(tags.args[tags.curindex++].f);

		if (tags.reginfo[tags.curindex] == REGT_INT)
			return tags.args[tags.curindex++].i;

		ThrowAbortException(X_OTHER, "Invalid parameter in network command function, int expected");
	}

	return TAG_DONE;
}

static double ListGetDouble(VMVa_List& tags)
{
	if (tags.curindex < tags.numargs)
	{
		if (tags.reginfo[tags.curindex] == REGT_FLOAT)
			return tags.args[tags.curindex++].f;

		if (tags.reginfo[tags.curindex] == REGT_INT)
			return tags.args[tags.curindex++].i;

		ThrowAbortException(X_OTHER, "Invalid parameter in network command function, float expected");
	}

	return TAG_DONE;
}

static const FString* ListGetString(VMVa_List& tags)
{
	if (tags.curindex < tags.numargs)
	{
		if (tags.reginfo[tags.curindex] == REGT_STRING)
			return &tags.args[tags.curindex++].s();

		ThrowAbortException(X_OTHER, "Invalid parameter in network command function, string expected");
	}

	return nullptr;
}

IMPLEMENT_CLASS(DNetworkBuffer, false, false);

void DNetworkBuffer::AddInt8(int byte)
{
	++_size;
	_buffer.Push({ NET_INT8, byte });
}

void DNetworkBuffer::AddInt16(int word)
{
	_size += 2u;
	_buffer.Push({ NET_INT16, word });
}

void DNetworkBuffer::AddInt(int msg)
{
	_size += 4u;
	_buffer.Push({ NET_INT, msg });
}

void DNetworkBuffer::AddFloat(double msg)
{
	_size += 4u;
	_buffer.Push({ NET_FLOAT, msg });
}

void DNetworkBuffer::AddDouble(double msg)
{
	_size += 8u;
	_buffer.Push({ NET_DOUBLE, msg });
}

void DNetworkBuffer::AddString(const FString& msg)
{
	_size += msg.Len() + 1u;
	_buffer.Push({ NET_STRING, msg });
}

void DNetworkBuffer::OnDestroy()
{
	Super::OnDestroy();

	_buffer.Reset();
}

void DNetworkBuffer::Serialize(FSerializer& arc)
{
	Super::Serialize(arc);

	if (arc.isWriting())
	{
		if (!_buffer.Size())
			return;

		if (arc.BeginArray("types"))
		{
			for (const auto& value : _buffer)
			{
				int i = value.GetType();
				arc(nullptr, i);
			}

			arc.EndArray();
		}

		if (arc.BeginArray("values"))
		{
			for (const auto& value : _buffer)
			{
				switch (value.GetType())
				{
					case NET_DOUBLE:
					case NET_FLOAT:
					{
						double f = value.GetDouble();
						arc(nullptr, f);
						break;
					}

					case NET_STRING:
					{
						FString s = value.GetString();
						arc(nullptr, s);
						break;
					}

					default:
					{
						int i = value.GetInt();
						arc(nullptr, i);
						break;
					}
				}
			}

			arc.EndArray();
		}
	}
	else
	{
		TArray<int> types = {};
		arc("types", types);

		if (!types.Size())
			return;

		if (arc.BeginArray("values"))
		{
			// Something got corrupted, don't repopulate the buffer.
			if (arc.ArraySize() != types.Size())
			{
				arc.EndArray();
				return;
			}

			for (int type : types)
			{
				switch (type)
				{
					case NET_INT8:
					{
						int i = 0;
						arc(nullptr, i);
						AddInt8(i);
						break;
					}

					case NET_INT16:
					{
						int i = 0;
						arc(nullptr, i);
						AddInt16(i);
						break;
					}

					case NET_INT:
					{
						int i = 0;
						arc(nullptr, i);
						AddInt(i);
						break;
					}

					case NET_FLOAT:
					{
						double f = 0.0;
						arc(nullptr, f);
						AddFloat(f);
						break;
					}

					case NET_DOUBLE:
					{
						double f = 0.0;
						arc(nullptr, f);
						AddDouble(f);
						break;
					}

					case NET_STRING:
					{
						FString s = {};
						arc(nullptr, s);
						AddString(s);
						break;
					}
				}
			}

			arc.EndArray();
		}
	}
}

void EventManager::CallOnRegister()
{
	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
	{
		handler->OnRegister();
	}
}

bool EventManager::RegisterHandler(DStaticEventHandler* handler)
{
	if (handler == nullptr || handler->ObjectFlags & OF_EuthanizeMe)
		return false;
	if (CheckHandler(handler))
		return false;

	handler->OnRegister();
	handler->owner = this;
	
	// link into normal list
	// update: link at specific position based on order.
	DStaticEventHandler* before = nullptr;
	for (DStaticEventHandler* existinghandler = FirstEventHandler; existinghandler; existinghandler = existinghandler->next)
	{
		if (existinghandler->Order > handler->Order)
		{
			before = existinghandler;
			break;
		}
	}

	// (Yes, all those write barriers here are really needed!)
	if (before != nullptr)
	{
		// if before is not null, link it before the existing handler.
		// note that before can be first handler, check for this.
		if (before->prev != nullptr)
		{
			before->prev->next = handler;
			GC::WriteBarrier(before->prev, handler);
		}
		handler->next = before;
		GC::WriteBarrier(handler, before);
		handler->prev = before->prev;
		GC::WriteBarrier(handler, before->prev);
		before->prev = handler;
		GC::WriteBarrier(before, handler);
		if (before == FirstEventHandler)
		{
			FirstEventHandler = handler;
			GC::WriteBarrier(handler);
		}
	}
	else
	{
		// so if before is null, it means add last.
		// it can also mean that we have no handlers at all yet.
		handler->prev = LastEventHandler;
		GC::WriteBarrier(handler, LastEventHandler);
		handler->next = nullptr;
		if (FirstEventHandler == nullptr) FirstEventHandler = handler;
		LastEventHandler = handler;
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

bool EventManager::UnregisterHandler(DStaticEventHandler* handler)
{
	if (handler == nullptr || handler->ObjectFlags & OF_EuthanizeMe)
		return false;
	if (!CheckHandler(handler))
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
	if (handler == FirstEventHandler)
	{
		FirstEventHandler = handler->next;
		GC::WriteBarrier(handler->next);
	}
	if (handler == LastEventHandler)
	{
		LastEventHandler = handler->prev;
		GC::WriteBarrier(handler->prev);
	}
	if (handler->IsStatic())
	{
		handler->ObjectFlags &= ~OF_Transient;
		handler->Destroy();
	}
	return true;
}

bool EventManager::SendNetworkEvent(FString name, int arg1, int arg2, int arg3, bool manual)
{
	if (gamestate != GS_LEVEL && gamestate != GS_TITLELEVEL)
		return false;

	Net_WriteInt8(DEM_NETEVENT);
	Net_WriteString(name.GetChars());
	Net_WriteInt8(3);
	Net_WriteInt32(arg1);
	Net_WriteInt32(arg2);
	Net_WriteInt32(arg3);
	Net_WriteInt8(manual);

	return true;
}

bool EventManager::SendNetworkCommand(const FName& cmd, VMVa_List& args)
{
	if (gamestate != GS_LEVEL && gamestate != GS_TITLELEVEL)
		return false;

	// Calculate the size of the message so we know where it ends.
	unsigned int bytes = 0u;
	int tag = ListGetInt(args);
	while (tag != TAG_DONE)
	{
		switch (tag)
		{
			case NET_INT8:
				++bytes;
				break;

			case NET_INT16:
				bytes += 2u;
				break;

			case NET_INT:
			case NET_FLOAT:
				bytes += 4u;
				break;

			case NET_DOUBLE:
				bytes += 8u;
				break;

			case NET_STRING:
			{
				++bytes; // Strings will always consume at least one byte.
				const FString* str = ListGetString(args);
				if (str != nullptr)
					bytes += str->Len();
				break;
			}
		}

		if (tag != NET_STRING)
			++args.curindex;

		tag = ListGetInt(args);
	}

	Net_WriteInt8(DEM_ZSC_CMD);
	Net_WriteString(cmd.GetChars());
	Net_WriteInt16(bytes);

	constexpr char Default[] = "";

	args.curindex = 0;
	tag = ListGetInt(args);
	while (tag != TAG_DONE)
	{
		switch (tag)
		{
			default:
				++args.curindex;
				break;

			case NET_INT8:
				Net_WriteInt8(ListGetInt(args));
				break;

			case NET_INT16:
				Net_WriteInt16(ListGetInt(args));
				break;

			case NET_INT:
				Net_WriteInt32(ListGetInt(args));
				break;

			case NET_FLOAT:
				Net_WriteFloat(ListGetDouble(args));
				break;

			case NET_DOUBLE:
				Net_WriteDouble(ListGetDouble(args));
				break;

			case NET_STRING:
			{
				const FString* str = ListGetString(args);
				if (str != nullptr)
					Net_WriteString(str->GetChars());
				else
					Net_WriteString(Default); // Still have to send something here to be read correctly.
				break;
			}
		}

		tag = ListGetInt(args);
	}

	return true;
}

bool EventManager::SendNetworkBuffer(const FName& cmd, const DNetworkBuffer* buffer)
{
	if (gamestate != GS_LEVEL && gamestate != GS_TITLELEVEL)
		return false;

	Net_WriteInt8(DEM_ZSC_CMD);
	Net_WriteString(cmd.GetChars());
	Net_WriteInt16(buffer != nullptr ? buffer->GetBytes() : 0);

	if (buffer != nullptr)
	{
		for (unsigned int i = 0u; i < buffer->GetBufferSize(); ++i)
		{
			const auto& value = buffer->GetValue(i);
			switch (value.GetType())
			{
				case NET_INT8:
					Net_WriteInt8(value.GetInt());
					break;

				case NET_INT16:
					Net_WriteInt16(value.GetInt());
					break;

				case NET_INT:
					Net_WriteInt32(value.GetInt());
					break;

				case NET_FLOAT:
					Net_WriteFloat(value.GetDouble());
					break;

				case NET_DOUBLE:
					Net_WriteDouble(value.GetDouble());
					break;

				case NET_STRING:
					Net_WriteString(value.GetString());
					break;
			}
		}
	}

	return true;
}

bool EventManager::CheckHandler(DStaticEventHandler* handler)
{
	for (DStaticEventHandler* lhandler = FirstEventHandler; lhandler; lhandler = lhandler->next)
		if (handler == lhandler) return true;
	return false;
}

bool EventManager::IsStaticType(PClass* type)
{
	assert(type != nullptr);
	assert(type->IsDescendantOf(RUNTIME_CLASS(DStaticEventHandler)));
	return !type->IsDescendantOf(RUNTIME_CLASS(DEventHandler));
}

static PClass* GetHandlerClass(const FString& typeName)
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

void EventManager::InitHandler(PClass* type)
{
	// check if type already exists, don't add twice.
	bool typeExists = false;
	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
	{
		if (handler->IsA(type))
		{
			typeExists = true;
			break;
		}
	}

	if (typeExists) return;
	DStaticEventHandler* handler = (DStaticEventHandler*)type->CreateNew();
	RegisterHandler(handler);
}

void EventManager::InitStaticHandlers(FLevelLocals *l, bool map)
{
	// don't initialize map handlers if restoring from savegame.
	if (savegamerestore)
		return;

	// just make sure
	Shutdown();
	Level = l;

	// initialize event handlers from gameinfo
	for (const FString& typeName : gameinfo.EventHandlers)
	{
		PClass* type = GetHandlerClass(typeName);
		// don't init the really global stuff here on startup initialization.
		// don't init map-local global stuff here on level setup.
		if (map == IsStaticType(type))
			continue;
		InitHandler(type);
	}

	if (!map) 
		return;

	// initialize event handlers from mapinfo
	for (const FString& typeName : Level->info->EventHandlers)
	{
		PClass* type = GetHandlerClass(typeName);
		if (IsStaticType(type))
			I_Error("Fatal: invalid event handler class %s in MAPINFO!\nMap-specific event handlers cannot be static.\n", typeName.GetChars());
		InitHandler(type);
	}
}

void EventManager::Shutdown()
{
	// delete handlers.
	TArray<DStaticEventHandler *> list;
	for (DStaticEventHandler* handler = LastEventHandler; handler; handler = handler->prev)
	{
		list.Push(handler);
	}
	for (auto handler : list)
	{
		handler->Destroy();
	}
	FirstEventHandler = LastEventHandler = nullptr;
}

#define DEFINE_EVENT_LOOPER(name, play) void EventManager::name() \
{ \
	if (ShouldCallStatic(play)) staticEventManager.name(); \
	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next) \
		handler->name(); \
}

void EventManager::OnEngineInitialize()
{
	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
	{
		if (handler->IsStatic())
			handler->OnEngineInitialize();
	}
}

// note for the functions below.
// *Unsafe is executed on EVERY map load/close, including savegame loading, etc.
// There is no point in allowing non-static handlers to receive unsafe event separately, as there is no point in having static handlers receive safe event.
// Because the main point of safe WorldLoaded/Unloading is that it will be preserved in savegames.
void EventManager::WorldLoaded()
{
	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
	{
		if (!handler->IsStatic() && savegamerestore) continue; // don't execute WorldLoaded for handlers loaded from the savegame.
		handler->WorldLoaded();
	}
}

void EventManager::WorldUnloaded(const FString& nextmap)
{
	for (DStaticEventHandler* handler = LastEventHandler; handler; handler = handler->prev)
	{
		handler->WorldUnloaded(nextmap);
	}
}

bool EventManager::ShouldCallStatic(bool forplay)
{
	return this != &staticEventManager && Level == primaryLevel;
}

void EventManager::WorldThingSpawned(AActor* actor)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;

	if (ShouldCallStatic(true)) staticEventManager.WorldThingSpawned(actor);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingSpawned(actor);
}

void EventManager::WorldThingDied(AActor* actor, AActor* inflictor)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;

	if (ShouldCallStatic(true)) staticEventManager.WorldThingDied(actor, inflictor);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingDied(actor, inflictor);
}

void EventManager::WorldThingGround(AActor* actor, FState* st)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;

	if (ShouldCallStatic(true)) staticEventManager.WorldThingGround(actor, st);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingGround(actor, st);
}

void EventManager::WorldThingRevived(AActor* actor)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;

	if (ShouldCallStatic(true)) staticEventManager.WorldThingRevived(actor);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingRevived(actor);
}

void EventManager::WorldThingDamaged(AActor* actor, AActor* inflictor, AActor* source, int damage, FName mod, int flags, DAngle angle)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;

	if (ShouldCallStatic(true)) staticEventManager.WorldThingDamaged(actor, inflictor, source, damage, mod, flags, angle);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->WorldThingDamaged(actor, inflictor, source, damage, mod, flags, angle);
}

void EventManager::WorldThingDestroyed(AActor* actor)
{
	// don't call anything if actor was destroyed on PostBeginPlay/BeginPlay/whatever.
	if (actor->ObjectFlags & OF_EuthanizeMe)
		return;
	// don't call anything for non-spawned things (i.e. those that were created, but immediately destroyed)
	// this is because Destroyed should be reverse of Spawned. we don't want to catch random inventory give failures.
	if (!(actor->ObjectFlags & OF_Spawned))
		return;

	for (DStaticEventHandler* handler = LastEventHandler; handler; handler = handler->prev)
		handler->WorldThingDestroyed(actor);

	if (ShouldCallStatic(true)) staticEventManager.WorldThingDestroyed(actor);
}

void EventManager::WorldLinePreActivated(line_t* line, AActor* actor, int activationType, bool* shouldactivate)
{
	if (ShouldCallStatic(true)) staticEventManager.WorldLinePreActivated(line, actor, activationType, shouldactivate);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->WorldLinePreActivated(line, actor, activationType, shouldactivate);
}

void EventManager::WorldLineActivated(line_t* line, AActor* actor, int activationType)
{
	if (ShouldCallStatic(true)) staticEventManager.WorldLineActivated(line, actor, activationType);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->WorldLineActivated(line, actor, activationType);
}

int EventManager::WorldSectorDamaged(sector_t* sector, AActor* source, int damage, FName damagetype, int part, DVector3 position, bool isradius)
{
	if (ShouldCallStatic(true)) staticEventManager.WorldSectorDamaged(sector, source, damage, damagetype, part, position, isradius);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		damage = handler->WorldSectorDamaged(sector, source, damage, damagetype, part, position, isradius);
	return damage;
}

int EventManager::WorldLineDamaged(line_t* line, AActor* source, int damage, FName damagetype, int side, DVector3 position, bool isradius)
{
	if (ShouldCallStatic(true)) staticEventManager.WorldLineDamaged(line, source, damage, damagetype, side, position, isradius);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		damage = handler->WorldLineDamaged(line, source, damage, damagetype, side, position, isradius);
	return damage;
}

void EventManager::PlayerEntered(int num, bool fromhub)
{
	// this event can happen during savegamerestore. make sure that local handlers don't receive it.
	// actually, global handlers don't want it too.
	if (savegamerestore && !fromhub)
		return;

	if (ShouldCallStatic(true)) staticEventManager.PlayerEntered(num, fromhub);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->PlayerEntered(num, fromhub);
}

void EventManager::PlayerSpawned(int num)
{
	if (ShouldCallStatic(true)) staticEventManager.PlayerSpawned(num);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->PlayerSpawned(num);
}

void EventManager::PlayerRespawned(int num)
{
	if (ShouldCallStatic(true)) staticEventManager.PlayerRespawned(num);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->PlayerRespawned(num);
}

void EventManager::PlayerDied(int num)
{
	if (ShouldCallStatic(true)) staticEventManager.PlayerDied(num);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->PlayerDied(num);
}

void EventManager::PlayerDisconnected(int num)
{
	for (DStaticEventHandler* handler = LastEventHandler; handler; handler = handler->prev)
		handler->PlayerDisconnected(num);

	if (ShouldCallStatic(true)) staticEventManager.PlayerDisconnected(num);
}

bool EventManager::Responder(const event_t* ev)
{
	bool uiProcessorsFound = false;

	// FIRST, check if there are UI processors
	// if there are, block mouse input
	for (DStaticEventHandler* handler = LastEventHandler; handler; handler = handler->prev)
	{
		if (handler->IsUiProcessor)
		{
			uiProcessorsFound = true;
			break;
		}
	}

	// if this was an input mouse event (occasionally happens) we need to block it without showing it to the modder.
	bool isUiMouseEvent = (ev->type == EV_GUI_Event && ev->subtype >= EV_GUI_FirstMouseEvent && ev->subtype <= EV_GUI_LastMouseEvent);
	bool isInputMouseEvent = (ev->type == EV_Mouse) || // input mouse movement
		((ev->type == EV_KeyDown || ev->type == EV_KeyUp) && ev->data1 >= KEY_MOUSE1 && ev->data1 <= KEY_MOUSE8); // or input mouse click

	if (isInputMouseEvent && uiProcessorsFound)
		return true; // block event from propagating

	if (ev->type == EV_GUI_Event)
	{
		// iterate handlers back to front by order, and give them this event.
		for (DStaticEventHandler* handler = LastEventHandler; handler; handler = handler->prev)
		{
			if (handler->IsUiProcessor)
			{
				if (isUiMouseEvent && !handler->RequireMouse)
					continue; // don't provide mouse event if not requested
				if (handler->UiProcess(ev))
					return true; // event was processed
			}
		}
	}
	else
	{
		// not sure if we want to handle device changes, but whatevs.
		for (DStaticEventHandler* handler = LastEventHandler; handler; handler = handler->prev)
		{
			// do not process input events for UI
			if (handler->IsUiProcessor)
				continue;
			if (handler->InputProcess(ev))
				return true; // event was processed
		}
	}
	if (ShouldCallStatic(false) && staticEventManager.Responder(ev)) return true;

	return false;
}

void EventManager::NetCommand(FNetworkCommand& cmd)
{
	if (ShouldCallStatic(false)) staticEventManager.NetCommand(cmd);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->NetCommandProcess(cmd);
}

void EventManager::Console(int player, FString name, int arg1, int arg2, int arg3, bool manual, bool ui)
{
	if (ShouldCallStatic(false)) staticEventManager.Console(player, name, arg1, arg2, arg3, manual, ui);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->ConsoleProcess(player, name, arg1, arg2, arg3, manual, ui);
}

void EventManager::RenderOverlay(EHudState state)
{
	if (ShouldCallStatic(false)) staticEventManager.RenderOverlay(state);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->RenderOverlay(state);
}

void EventManager::RenderUnderlay(EHudState state)
{
	if (ShouldCallStatic(false)) staticEventManager.RenderUnderlay(state);

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		handler->RenderUnderlay(state);
}

bool EventManager::CheckUiProcessors()
{
	if (ShouldCallStatic(false))
	{
		if (staticEventManager.CheckUiProcessors()) return true;
	}

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		if (handler->IsUiProcessor)
			return true;

	return false;
}

bool EventManager::CheckRequireMouse()
{
	if (ShouldCallStatic(false)) 
	{
		if (staticEventManager.CheckRequireMouse()) return true;
	}

	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
		if (handler->IsUiProcessor && handler->RequireMouse)
			return true;

	return false;
}

bool EventManager::CheckReplacement( PClassActor *replacee, PClassActor **replacement )
{
	bool final = false;

	// This is play scope but unlike in-game events needs to be handled like UI by static handlers.
	if (ShouldCallStatic(false)) final = staticEventManager.CheckReplacement(replacee, replacement);

	for (DStaticEventHandler *handler = FirstEventHandler; handler; handler = handler->next)
		handler->CheckReplacement(replacee,replacement,&final);
	return final;
}

bool EventManager::CheckReplacee(PClassActor **replacee, PClassActor *replacement)
{
	bool final = false;
	if (ShouldCallStatic(false)) final = staticEventManager.CheckReplacee(replacee, replacement);

	for (DStaticEventHandler *handler = FirstEventHandler; handler; handler = handler->next)
		handler->CheckReplacee(replacee, replacement, &final);
	return final;
}

void EventManager::NewGame()
{
	//This is called separately for static and local handlers, so no forwarding here.
	for (DStaticEventHandler* handler = FirstEventHandler; handler; handler = handler->next)
	{
		handler->NewGame();
	}
}

// normal event loopers (non-special, argument-less)
DEFINE_EVENT_LOOPER(RenderFrame, false)
DEFINE_EVENT_LOOPER(WorldLightning, true)
DEFINE_EVENT_LOOPER(WorldTick, true)
DEFINE_EVENT_LOOPER(UiTick, false)
DEFINE_EVENT_LOOPER(PostUiTick, false)

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
DEFINE_FIELD_X(WorldEvent, FWorldEvent, NextMap);
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
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamageSectorPart);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamageLine);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamageSector);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamageLineSide);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamagePosition);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, DamageIsRadius);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, NewDamage);
DEFINE_FIELD_X(WorldEvent, FWorldEvent, CrushedState);

DEFINE_FIELD_X(PlayerEvent, FPlayerEvent, PlayerNumber);
DEFINE_FIELD_X(PlayerEvent, FPlayerEvent, IsReturn);

DEFINE_FIELD_X(ConsoleEvent, FConsoleEvent, Player)
DEFINE_FIELD_X(ConsoleEvent, FConsoleEvent, Name)
DEFINE_FIELD_X(ConsoleEvent, FConsoleEvent, Args)
DEFINE_FIELD_X(ConsoleEvent, FConsoleEvent, IsManual)

DEFINE_FIELD_X(ReplaceEvent, FReplaceEvent, Replacee)
DEFINE_FIELD_X(ReplaceEvent, FReplaceEvent, Replacement)
DEFINE_FIELD_X(ReplaceEvent, FReplaceEvent, IsFinal)

DEFINE_FIELD_X(ReplacedEvent, FReplacedEvent, Replacee)
DEFINE_FIELD_X(ReplacedEvent, FReplacedEvent, Replacement)
DEFINE_FIELD_X(ReplacedEvent, FReplacedEvent, IsFinal)

DEFINE_FIELD(FNetworkCommand, Player)
DEFINE_FIELD(FNetworkCommand, Command)

static int NativeReadInt8(FNetworkCommand* const self) { return self->ReadInt8(); }

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadInt8, NativeReadInt8)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_INT(self->ReadInt8());
}

static int NativeReadInt16(FNetworkCommand* const self) { return self->ReadInt16(); }

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadInt16, NativeReadInt16)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_INT(self->ReadInt16());
}

static int NativeReadInt(FNetworkCommand* const self) { return self->ReadInt(); }

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadInt, NativeReadInt)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_INT(self->ReadInt());
}

static double NativeReadFloat(FNetworkCommand* const self) { return self->ReadFloat(); }

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadFloat, NativeReadFloat)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_FLOAT(self->ReadFloat());
}

static double NativeReadDouble(FNetworkCommand* const self) { return self->ReadDouble(); }

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadDouble, NativeReadDouble)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_FLOAT(self->ReadDouble());
}

static void NativeReadString(FNetworkCommand* const self, FString* const result)
{
	const auto str = self->ReadString();
	if (str != nullptr)
		*result = str;
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadString, NativeReadString)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	FString res = {};
	NativeReadString(self, &res);
	ACTION_RETURN_STRING(res);
}

static int NativeReadName(FNetworkCommand* const self)
{
	FName res = NAME_None;
	const auto str = self->ReadString();
	if (str != nullptr)
		res = str;

	return res.GetIndex();
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadName, NativeReadName)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_INT(NativeReadName(self));
}

static double NativeReadMapUnit(FNetworkCommand* const self)
{
	constexpr double FixedToFloat = 1.0 / (1 << 16);
	return self->ReadInt() * FixedToFloat;
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadMapUnit, NativeReadMapUnit)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_FLOAT(NativeReadMapUnit(self));
}

static double NativeReadAngle(FNetworkCommand* const self) { return DAngle::fromBam(self->ReadInt()).Degrees(); }

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadAngle, NativeReadAngle)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_FLOAT(DAngle::fromBam(self->ReadInt()).Degrees());
}

static void NativeReadVector2(FNetworkCommand* const self, DVector2* const result)
{
	result->X = self->ReadDouble();
	result->Y = self->ReadDouble();
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadVector2, NativeReadVector2)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_VEC2(DVector2(self->ReadDouble(), self->ReadDouble()));
}

static void NativeReadVector3(FNetworkCommand* const self, DVector3* const result)
{
	result->X = self->ReadDouble();
	result->Y = self->ReadDouble();
	result->Z = self->ReadDouble();
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadVector3, NativeReadVector3)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_VEC3(DVector3(self->ReadDouble(), self->ReadDouble(), self->ReadDouble()));
}

static void NativeReadVector4(FNetworkCommand* const self, DVector4* const result)
{
	result->X = self->ReadDouble();
	result->Y = self->ReadDouble();
	result->Z = self->ReadDouble();
	result->W = self->ReadDouble();
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadVector4, NativeReadVector4)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_VEC4(DVector4(self->ReadDouble(), self->ReadDouble(), self->ReadDouble(), self->ReadDouble()));
}

static void NativeReadQuat(FNetworkCommand* const self, DQuaternion* const result)
{
	result->X = self->ReadDouble();
	result->Y = self->ReadDouble();
	result->Z = self->ReadDouble();
	result->W = self->ReadDouble();
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadQuat, NativeReadQuat)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_QUAT(DQuaternion(self->ReadDouble(), self->ReadDouble(), self->ReadDouble(), self->ReadDouble()));
}

static void NativeReadIntArray(FNetworkCommand* const self, TArray<int>* const values, const int type)
{
	const unsigned int size = self->ReadInt();
	for (unsigned int i = 0u; i < size; ++i)
	{
		switch (type)
		{
		case NET_INT8:
			values->Push(self->ReadInt8());
			break;

		case NET_INT16:
			values->Push(self->ReadInt16());
			break;

		default:
			values->Push(self->ReadInt());
			break;
		}
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadIntArray, NativeReadIntArray)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);
	PARAM_OUTPOINTER(values, TArray<int>);
	PARAM_INT(type);

	NativeReadIntArray(self, values, type);
	return 0;
}

static void NativeReadDoubleArray(FNetworkCommand* const self, TArray<double>* const values, const bool doublePrecision)
{
	const unsigned int size = self->ReadInt();
	for (unsigned int i = 0u; i < size; ++i)
	{
		if (doublePrecision)
			values->Push(self->ReadDouble());
		else
			values->Push(self->ReadFloat());
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadDoubleArray, NativeReadDoubleArray)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);
	PARAM_OUTPOINTER(values, TArray<double>);
	PARAM_BOOL(doublePrecision);

	NativeReadDoubleArray(self, values, doublePrecision);
	return 0;
}

static void NativeReadStringArray(FNetworkCommand* const self, TArray<FString>* const values, const bool skipEmpty)
{
	const unsigned int size = self->ReadInt();
	for (unsigned int i = 0u; i < size; ++i)
	{
		FString res = {};
		const auto str = self->ReadString();
		if (str != nullptr)
			res = str;

		if (!skipEmpty || !res.IsEmpty())
			values->Push(res);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, ReadStringArray, NativeReadStringArray)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);
	PARAM_OUTPOINTER(values, TArray<FString>);
	PARAM_BOOL(skipEmpty);

	NativeReadStringArray(self, values, skipEmpty);
	return 0;
}

static int EndOfStream(FNetworkCommand* const self) { return self->EndOfStream(); }

DEFINE_ACTION_FUNCTION_NATIVE(FNetworkCommand, EndOfStream, EndOfStream)
{
	PARAM_SELF_STRUCT_PROLOGUE(FNetworkCommand);

	ACTION_RETURN_BOOL(self->EndOfStream());
}

static void AddInt8(DNetworkBuffer* const self, const int value) { self->AddInt8(value); }

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddInt8, AddInt8)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_INT(value);
	self->AddInt8(value);
	return 0;
}

static void AddInt16(DNetworkBuffer* const self, const int value) { self->AddInt16(value); }

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddInt16, AddInt16)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_INT(value);
	self->AddInt16(value);
	return 0;
}

static void AddInt(DNetworkBuffer* const self, const int value) { self->AddInt(value); }

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddInt, AddInt)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_INT(value);
	self->AddInt(value);
	return 0;
}

static void AddFloat(DNetworkBuffer* const self, const double value) { self->AddFloat(value); }

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddFloat, AddFloat)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_FLOAT(value);
	self->AddFloat(value);
	return 0;
}

static void AddDouble(DNetworkBuffer* const self, const double value) { self->AddDouble(value); }

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddDouble, AddDouble)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_FLOAT(value);
	self->AddDouble(value);
	return 0;
}

static void AddString(DNetworkBuffer* const self, const FString& value) { self->AddString(value); }

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddString, AddString)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_STRING(value);
	self->AddString(value);
	return 0;
}

static void AddName(DNetworkBuffer* const self, const int value) { FName x = ENamedName(value);  self->AddString(x.GetChars()); }

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddName, AddName)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_NAME(value);
	self->AddString(value.GetChars());
	return 0;
}

static void AddMapUnit(DNetworkBuffer* const self, const double value)
{
	constexpr int FloatToFixed = 1 << 16;
	self->AddInt(static_cast<int>(value * FloatToFixed));
}

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddMapUnit, AddMapUnit)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_FLOAT(value);

	AddMapUnit(self, value);
	return 0;
}

static void AddAngle(DNetworkBuffer* const self, const double value) { self->AddInt(DAngle::fromDeg(value).BAMs()); }

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddAngle, AddAngle)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_ANGLE(value);
	self->AddInt(value.BAMs());
	return 0;
}

static void AddVector2(DNetworkBuffer* const self, const double x, const double y)
{
	self->AddDouble(x);
	self->AddDouble(y);
}

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddVector2, AddVector2)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	self->AddDouble(x);
	self->AddDouble(y);
	return 0;
}

static void AddVector3(DNetworkBuffer* const self, const double x, const double y, const double z)
{
	self->AddDouble(x);
	self->AddDouble(y);
	self->AddDouble(z);
}

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddVector3, AddVector3)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	self->AddDouble(x);
	self->AddDouble(y);
	self->AddDouble(z);
	return 0;
}

static void AddVector4(DNetworkBuffer* const self, const double x, const double y, const double z, const double w)
{
	self->AddDouble(x);
	self->AddDouble(y);
	self->AddDouble(z);
	self->AddDouble(w);
}

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddVector4, AddVector4)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_FLOAT(w);
	self->AddDouble(x);
	self->AddDouble(y);
	self->AddDouble(z);
	self->AddDouble(w);
	return 0;
}

static void AddQuat(DNetworkBuffer* const self, const double x, const double y, const double z, const double w)
{
	self->AddDouble(x);
	self->AddDouble(y);
	self->AddDouble(z);
	self->AddDouble(w);
}

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddQuat, AddQuat)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_FLOAT(w);
	self->AddDouble(x);
	self->AddDouble(y);
	self->AddDouble(z);
	self->AddDouble(w);
	return 0;
}

static void AddIntArray(DNetworkBuffer* const self, TArray<int>* const values, const int type)
{
	const unsigned int size = values->Size();
	self->AddInt(size);
	for (unsigned int i = 0u; i < size; ++i)
	{
		switch (type)
		{
		case NET_INT8:
			self->AddInt8((*values)[i]);
			break;

		case NET_INT16:
			self->AddInt16((*values)[i]);
			break;

		default:
			self->AddInt((*values)[i]);
			break;
		}
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddIntArray, AddIntArray)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_POINTER(values, TArray<int>);
	PARAM_INT(type);

	AddIntArray(self, values, type);
	return 0;
}

static void AddDoubleArray(DNetworkBuffer* const self, TArray<double>* const values, const bool doublePrecision)
{
	const unsigned int size = values->Size();
	self->AddInt(size);
	for (unsigned int i = 0u; i < size; ++i)
	{
		if (doublePrecision)
			self->AddDouble((*values)[i]);
		else
			self->AddFloat((*values)[i]);
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddDoubleArray, AddDoubleArray)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_POINTER(values, TArray<double>);
	PARAM_BOOL(doublePrecision);

	AddDoubleArray(self, values, doublePrecision);
	return 0;
}

static void AddStringArray(DNetworkBuffer* const self, TArray<FString>* const values)
{
	const unsigned int size = values->Size();
	self->AddInt(size);
	for (unsigned int i = 0u; i < size; ++i)
		self->AddString((*values)[i]);
}

DEFINE_ACTION_FUNCTION_NATIVE(DNetworkBuffer, AddStringArray, AddStringArray)
{
	PARAM_SELF_PROLOGUE(DNetworkBuffer);
	PARAM_POINTER(values, TArray<FString>);

	AddStringArray(self, values);
	return 0;
}

DEFINE_ACTION_FUNCTION(DStaticEventHandler, SetOrder)
{
	PARAM_SELF_PROLOGUE(DStaticEventHandler);
	PARAM_INT(order);

	self->Order = order;
	return 0;
}

DEFINE_ACTION_FUNCTION(DEventHandler, SendNetworkCommand)
{
	PARAM_PROLOGUE;
	PARAM_NAME(cmd);
	PARAM_VA_POINTER(va_reginfo);

	VMVa_List args = { param + 1, 0, numparam - 2, va_reginfo + 1 };
	ACTION_RETURN_BOOL(currentVMLevel->localEventManager->SendNetworkCommand(cmd, args));
}

DEFINE_ACTION_FUNCTION(DEventHandler, SendNetworkBuffer)
{
	PARAM_PROLOGUE;
	PARAM_NAME(cmd);
	PARAM_OBJECT(buffer, DNetworkBuffer);

	ACTION_RETURN_BOOL(currentVMLevel->localEventManager->SendNetworkBuffer(cmd, buffer));
}

DEFINE_ACTION_FUNCTION(DEventHandler, SendNetworkEvent)
{
	PARAM_PROLOGUE;
	PARAM_STRING(name);
	PARAM_INT(arg1);
	PARAM_INT(arg2);
	PARAM_INT(arg3);
	//

	ACTION_RETURN_BOOL(currentVMLevel->localEventManager->SendNetworkEvent(name, arg1, arg2, arg3, false));
}

DEFINE_ACTION_FUNCTION(DEventHandler, SendInterfaceEvent)
{
	PARAM_PROLOGUE;
	PARAM_INT(playerNum);
	PARAM_STRING(name);
	PARAM_INT(arg1);
	PARAM_INT(arg2);
	PARAM_INT(arg3);

	if (playerNum == consoleplayer)
		primaryLevel->localEventManager->Console(-1, name, arg1, arg2, arg3, false, true);

	return 0;
}

DEFINE_ACTION_FUNCTION(DEventHandler, Find)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DStaticEventHandler);
	for (DStaticEventHandler* handler = currentVMLevel->localEventManager->FirstEventHandler; handler; handler = handler->next)
		if (handler->GetClass() == t) // check precise class
			ACTION_RETURN_OBJECT(handler);
	ACTION_RETURN_OBJECT(nullptr);
}

// we might later want to change this
DEFINE_ACTION_FUNCTION(DStaticEventHandler, Find)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(t, DStaticEventHandler);
	for (DStaticEventHandler* handler = staticEventManager.FirstEventHandler; handler; handler = handler->next)
		if (handler->GetClass() == t) // check precise class
			ACTION_RETURN_OBJECT(handler);
	ACTION_RETURN_OBJECT(nullptr);
}


// Check if the handler implements a callback for this event. This is used to avoid setting up the data for any event that the handler won't process
static bool isEmpty(VMFunction *func)
{
	auto code = static_cast<VMScriptFunction *>(func)->Code;
	return (code == nullptr || code->word == (0x00808000|OP_RET));
}

static bool isEmptyInt(VMFunction *func)
{
	auto code = static_cast<VMScriptFunction *>(func)->Code;
	return (code == nullptr || code->word == (0x00048000|OP_RET));
}

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
		if (isEmpty(func)) return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

void DStaticEventHandler::OnUnregister()
{
	IFVIRTUAL(DStaticEventHandler, OnUnregister)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

FWorldEvent EventManager::SetupWorldEvent()
{
	FWorldEvent e;
	e.IsSaveGame = savegamerestore;
	e.IsReopen = Level->FromSnapshot && !savegamerestore; // each one by itself isnt helpful, but with hub load we have savegamerestore==0 and level.FromSnapshot==1.
	e.DamageAngle = nullAngle;
	return e;
}

void DStaticEventHandler::OnEngineInitialize()
{
	IFVIRTUAL(DStaticEventHandler, OnEngineInitialize)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}


void DStaticEventHandler::WorldLoaded()
{
	IFVIRTUAL(DStaticEventHandler, WorldLoaded)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldUnloaded(const FString& nextmap)
{
	IFVIRTUAL(DStaticEventHandler, WorldUnloaded)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
		e.NextMap = nextmap;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldThingSpawned(AActor* actor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingSpawned)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
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
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
		e.Thing = actor;
		e.Inflictor = inflictor;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldThingGround(AActor* actor, FState* st)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingGround)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
		e.Thing = actor;
		e.CrushedState = st;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}


void DStaticEventHandler::WorldThingRevived(AActor* actor)
{
	IFVIRTUAL(DStaticEventHandler, WorldThingRevived)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
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
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
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
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
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
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
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
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
		e.Thing = actor;
		e.ActivatedLine = line;
		e.ActivationType = activationType;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

int DStaticEventHandler::WorldSectorDamaged(sector_t* sector, AActor* source, int damage, FName damagetype, int part, DVector3 position, bool isradius)
{
	IFVIRTUAL(DStaticEventHandler, WorldSectorDamaged)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return damage;
		FWorldEvent e = owner->SetupWorldEvent();
		e.DamageSource = source;
		e.DamageSector = sector;
		e.NewDamage = e.Damage = damage;
		e.DamageType = damagetype;
		e.DamageSectorPart = part;
		e.DamagePosition = position;
		e.DamageIsRadius = isradius;

		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
		return e.NewDamage;
	}

	return damage;
}

int DStaticEventHandler::WorldLineDamaged(line_t* line, AActor* source, int damage, FName damagetype, int side, DVector3 position, bool isradius)
{
	IFVIRTUAL(DStaticEventHandler, WorldLineDamaged)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return damage;
		FWorldEvent e = owner->SetupWorldEvent();
		e.DamageSource = source;
		e.DamageLine = line;
		e.NewDamage = e.Damage = damage;
		e.DamageType = damagetype;
		e.DamageLineSide = side;
		e.DamagePosition = position;
		e.DamageIsRadius = isradius;

		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
		return e.NewDamage;
	}

	return damage;
}

void DStaticEventHandler::WorldLightning()
{
	IFVIRTUAL(DStaticEventHandler, WorldLightning)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FWorldEvent e = owner->SetupWorldEvent();
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::WorldTick()
{
	IFVIRTUAL(DStaticEventHandler, WorldTick)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

FRenderEvent EventManager::SetupRenderEvent()
{
	FRenderEvent e;
    auto &vp = r_viewpoint;
	e.ViewPos = vp.Pos;
	e.ViewAngle = vp.Angles.Yaw;
	e.ViewPitch = vp.Angles.Pitch;
	e.ViewRoll = vp.Angles.Roll;
	e.FracTic = vp.TicFrac;
	e.Camera = vp.camera;
	return e;
}

void DStaticEventHandler::RenderFrame()
{
	/* This is intentionally and permanently disabled.
	IFVIRTUAL(DStaticEventHandler, RenderFrame)
	{
		// don't create excessive DObjects if not going to be processed anyway
	 	if (isEmpty(func)) return;
		FRenderEvent e = owner->SetupRenderEvent;
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
	*/
}

void DStaticEventHandler::RenderOverlay(EHudState state)
{
	IFVIRTUAL(DStaticEventHandler, RenderOverlay)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FRenderEvent e = owner->SetupRenderEvent();
		e.HudState = int(state);
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::RenderUnderlay(EHudState state)
{
	IFVIRTUAL(DStaticEventHandler, RenderUnderlay)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FRenderEvent e = owner->SetupRenderEvent();
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
		if (isEmpty(func)) return;
		FPlayerEvent e = { num, fromhub };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::PlayerSpawned(int num)
{
	IFVIRTUAL(DStaticEventHandler, PlayerSpawned)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FPlayerEvent e = { num, false };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

void DStaticEventHandler::PlayerRespawned(int num)
{
	IFVIRTUAL(DStaticEventHandler, PlayerRespawned)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
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
		if (isEmpty(func)) return;
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
		if (isEmpty(func)) return;
		FPlayerEvent e = { num, false };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
	}
}

bool DStaticEventHandler::UiProcess(const event_t* ev)
{
	IFVIRTUAL(DStaticEventHandler, UiProcess)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmptyInt(func)) return false;
		FUiEvent e = ev;

		int processed;
		VMReturn results[1] = { &processed };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, results, 1);
		return !!processed;
	}

	return false;
}

bool DStaticEventHandler::InputProcess(const event_t* ev)
{
	IFVIRTUAL(DStaticEventHandler, InputProcess)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmptyInt(func)) return false;
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
		if (isEmpty(func)) return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

void DStaticEventHandler::PostUiTick()
{
	IFVIRTUAL(DStaticEventHandler, PostUiTick)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

void DStaticEventHandler::NetCommandProcess(FNetworkCommand& cmd)
{
	IFVIRTUAL(DStaticEventHandler, NetworkCommandProcess)
	{
		if (isEmpty(func))
			return;

		VMValue params[] = { this, &cmd };
		VMCall(func, params, 2, nullptr, 0);

		cmd.Reset();
	}
}

void DStaticEventHandler::ConsoleProcess(int player, FString name, int arg1, int arg2, int arg3, bool manual, bool ui)
{
	if (player < 0)
	{
		if (ui)
		{
			IFVIRTUAL(DStaticEventHandler, InterfaceProcess)
			{
				// don't create excessive DObjects if not going to be processed anyway
				if (isEmpty(func)) return;
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
			IFVIRTUAL(DStaticEventHandler, ConsoleProcess)
			{
				// don't create excessive DObjects if not going to be processed anyway
				if (isEmpty(func)) return;
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
	else
	{
		IFVIRTUAL(DStaticEventHandler, NetworkProcess)
		{
			// don't create excessive DObjects if not going to be processed anyway
			if (isEmpty(func)) return;
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

void DStaticEventHandler::CheckReplacement(PClassActor *replacee, PClassActor **replacement, bool *final )
{
	IFVIRTUAL(DStaticEventHandler, CheckReplacement)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FReplaceEvent e = { replacee, *replacement, *final };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
		if ( e.Replacement != replacee ) // prevent infinite recursion
			*replacement = e.Replacement;
		*final = e.IsFinal;
	}
}

void DStaticEventHandler::CheckReplacee(PClassActor **replacee, PClassActor *replacement, bool *final)
{
	IFVIRTUAL(DStaticEventHandler, CheckReplacee)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		FReplacedEvent e = { *replacee, replacement, *final };
		VMValue params[2] = { (DStaticEventHandler*)this, &e };
		VMCall(func, params, 2, nullptr, 0);
		if (e.Replacee != replacement) // prevent infinite recursion
			*replacee = e.Replacee;
		*final = e.IsFinal;
	}
}

void DStaticEventHandler::NewGame()
{
	IFVIRTUAL(DStaticEventHandler, NewGame)
	{
		// don't create excessive DObjects if not going to be processed anyway
		if (isEmpty(func)) return;
		VMValue params[1] = { (DStaticEventHandler*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

//
void DStaticEventHandler::OnDestroy()
{
	if (owner)
		owner->UnregisterHandler(this);
	Super::OnDestroy();
}

// console stuff
// this is kinda like puke, except it distinguishes between local events and playsim events.
CCMD(interfaceevent)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 5)
	{
		Printf("Usage: interfaceevent <name> [arg1] [arg2] [arg3]\n");
	}
	else
	{
		int arg[3] = { 0, 0, 0 };
		int argn = min<int>(argc - 2, countof(arg));
		for (int i = 0; i < argn; i++)
			arg[i] = atoi(argv[2 + i]);
		// call locally
		primaryLevel->localEventManager->Console(-1, argv[1], arg[0], arg[1], arg[2], true, true);
	}
}

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
		int argn = min<int>(argc - 2, countof(arg));
		for (int i = 0; i < argn; i++)
			arg[i] = atoi(argv[2 + i]);
		// call locally
		primaryLevel->localEventManager->Console(-1, argv[1], arg[0], arg[1], arg[2], true, false);
	}
}

CCMD(netevent)
{
	if (gamestate != GS_LEVEL/* && gamestate != GS_TITLELEVEL*/) // not sure if this should work in title level, but probably not, because this is for actual playing
	{
		DPrintf(DMSG_SPAMMY, "netevent cannot be used outside of a map.\n");
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
		int argn = min<int>(argc - 2, countof(arg));
		for (int i = 0; i < argn; i++)
			arg[i] = atoi(argv[2 + i]);
		// call networked
		primaryLevel->localEventManager->SendNetworkEvent(argv[1], arg[0], arg[1], arg[2], true);
	}
}
