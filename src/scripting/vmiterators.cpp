//-----------------------------------------------------------------------------
//
// Copyright 2016-2018 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// VM iterators
//
// These classes are thin wrappers which wrap the standars iterators into a DObject
// so that the VM can use them
//
//-----------------------------------------------------------------------------

#include "actor.h"
#include "p_tags.h"
#include "p_maputl.h"
#include "g_levellocals.h"
#include "vm.h"

//==========================================================================
//
// scriptable thinker iterator
//
//==========================================================================

class DThinkerIterator : public DObject, public FThinkerIterator
{
	DECLARE_ABSTRACT_CLASS(DThinkerIterator, DObject)

public:
	DThinkerIterator(FLevelLocals *Level, PClass *cls, int statnum = MAX_STATNUM + 1)
		: FThinkerIterator(Level, cls, statnum)
	{
	}
};

IMPLEMENT_CLASS(DThinkerIterator, true, false);

static DThinkerIterator *CreateThinkerIterator(PClass *type, int statnum)
{
	return Create<DThinkerIterator>(currentVMLevel, type, statnum);
}

DEFINE_ACTION_FUNCTION_NATIVE(DThinkerIterator, Create, CreateThinkerIterator)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(type, DThinker);
	PARAM_INT(statnum);
	ACTION_RETURN_OBJECT(CreateThinkerIterator(type, statnum));
}

static DThinker *NextThinker(DThinkerIterator *self, bool exact)
{
	return self->Next(exact);
}

DEFINE_ACTION_FUNCTION_NATIVE(DThinkerIterator, Next, NextThinker)
{
	PARAM_SELF_PROLOGUE(DThinkerIterator);
	PARAM_BOOL(exact);
	ACTION_RETURN_OBJECT(self->Next(exact));
}

static void ReinitThinker(DThinkerIterator *self)
{
	self->Reinit();
}

DEFINE_ACTION_FUNCTION_NATIVE(DThinkerIterator, Reinit, ReinitThinker)
{
	PARAM_SELF_PROLOGUE(DThinkerIterator);
	self->Reinit();
	return 0;
}

//===========================================================================
//
// scriptable BlockLines iterator
//
//===========================================================================

class DBlockLinesIterator : public DObject
{
	DECLARE_ABSTRACT_CLASS(DBlockLinesIterator, DObject);
	FPortalGroupArray check;

public:
	FMultiBlockLinesIterator iterator;
	FMultiBlockLinesIterator::CheckResult cres;

	
	DBlockLinesIterator(AActor *actor, double checkradius)
		: iterator(check, actor, checkradius)
	{
		cres.line = nullptr;
		cres.Position.Zero();
		cres.portalflags = 0;
	}

	DBlockLinesIterator(double x, double y, double z, double height, double radius, sector_t *sec)
		: iterator(check, currentVMLevel, x, y, z, height, radius, sec)
	{
		cres.line = nullptr;
		cres.Position.Zero();
		cres.portalflags = 0;
	}
};

IMPLEMENT_CLASS(DBlockLinesIterator, true, false);

static DBlockLinesIterator *CreateBLI(AActor *origin, double radius)
{
	return Create<DBlockLinesIterator>(PARAM_NULLCHECK(origin, origin), radius);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBlockLinesIterator, Create, CreateBLI)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT_NOT_NULL(origin, AActor);
	PARAM_FLOAT(radius);
	ACTION_RETURN_OBJECT(Create<DBlockLinesIterator>(origin, radius));
}

static DBlockLinesIterator *CreateBLIFromPos(double x, double y, double z, double h, double radius, sector_t *sec)
{
	return Create<DBlockLinesIterator>(x, y, z, h, radius, sec);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBlockLinesIterator, CreateFromPos, CreateBLIFromPos)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_FLOAT(h);
	PARAM_FLOAT(radius);
	PARAM_POINTER(sec, sector_t);
	ACTION_RETURN_OBJECT(Create<DBlockLinesIterator>(x, y, z, h, radius, sec));
}

static int BLINext(DBlockLinesIterator *self)
{
	return self->iterator.Next(&self->cres);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBlockLinesIterator, Next, BLINext)
{
	PARAM_SELF_PROLOGUE(DBlockLinesIterator);
	ACTION_RETURN_BOOL(BLINext(self));
}

//===========================================================================
//
// scriptable BlockThings iterator
//
//===========================================================================

class DBlockThingsIterator : public DObject
{
	DECLARE_ABSTRACT_CLASS(DBlockThingsIterator, DObject);
	FPortalGroupArray check;
public:
	FMultiBlockThingsIterator iterator;
	FMultiBlockThingsIterator::CheckResult cres;

	DBlockThingsIterator(AActor *origin, double checkradius = -1, bool ignorerestricted = false)
		: iterator(check, origin, checkradius, ignorerestricted)
	{
		cres.thing = nullptr;
		cres.Position.Zero();
		cres.portalflags = 0;
	}

	DBlockThingsIterator(double checkx, double checky, double checkz, double checkh, double checkradius, bool ignorerestricted, sector_t *newsec)
		: iterator(check, currentVMLevel, checkx, checky, checkz, checkh, checkradius, ignorerestricted, newsec)
	{
		cres.thing = nullptr;
		cres.Position.Zero();
		cres.portalflags = 0;
	}
};

IMPLEMENT_CLASS(DBlockThingsIterator, true, false);


static DBlockThingsIterator *CreateBTI(AActor *origin, double radius, bool ignore)
{
	return Create<DBlockThingsIterator>(PARAM_NULLCHECK(origin, origin), radius, ignore);
}


DEFINE_ACTION_FUNCTION_NATIVE(DBlockThingsIterator, Create, CreateBTI)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT_NOT_NULL(origin, AActor);
	PARAM_FLOAT(radius);
	PARAM_BOOL(ignore);
	ACTION_RETURN_OBJECT(Create<DBlockThingsIterator>(origin, radius, ignore));
}

static DBlockThingsIterator *CreateBTIFromPos(double x, double y, double z, double h, double radius, bool ignore)
{
	return Create<DBlockThingsIterator>(x, y, z, h, radius, ignore, nullptr);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBlockThingsIterator, CreateFromPos, CreateBTIFromPos)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_FLOAT(h);
	PARAM_FLOAT(radius);
	PARAM_BOOL(ignore);
	ACTION_RETURN_OBJECT(Create<DBlockThingsIterator>(x, y, z, h, radius, ignore, nullptr));
}

static int NextBTI(DBlockThingsIterator *bti)
{
	return bti->iterator.Next(&bti->cres);
}


DEFINE_ACTION_FUNCTION_NATIVE(DBlockThingsIterator, Next, NextBTI)
{
	PARAM_SELF_PROLOGUE(DBlockThingsIterator);
	ACTION_RETURN_BOOL(NextBTI(self));
}


class DSectorTagIterator : public DObject, public FSectorTagIterator
{
	DECLARE_ABSTRACT_CLASS(DSectorTagIterator, DObject);
public:
	DSectorTagIterator(FTagManager &tm, int tag, line_t *line) : FSectorTagIterator(tm)
	{
		if (line == nullptr) Init(tag);
		else Init(tag, line);
	}
};

IMPLEMENT_CLASS(DSectorTagIterator, true, false);

static DSectorTagIterator *CreateSTI(FLevelLocals *Level, int tag, line_t *line)
{
	return Create<DSectorTagIterator>(Level->tagManager, tag, line);
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, CreateSectorTagIterator, CreateSTI)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(tag);
	PARAM_POINTER(line, line_t);
	ACTION_RETURN_POINTER(CreateSTI(self, tag, line));
}

int NextSTI(DSectorTagIterator *self)
{
	return self->Next();
}

DEFINE_ACTION_FUNCTION_NATIVE(DSectorTagIterator, Next, NextSTI)
{
	PARAM_SELF_PROLOGUE(DSectorTagIterator);
	ACTION_RETURN_INT(self->Next());
}

int NextCompatSTI(DSectorTagIterator *self, bool compat, int secnum)
{
	return self->NextCompat(compat, secnum);
}

DEFINE_ACTION_FUNCTION_NATIVE(DSectorTagIterator, NextCompat, NextCompatSTI)
{
	PARAM_SELF_PROLOGUE(DSectorTagIterator);
	PARAM_BOOL(compat);
	PARAM_INT(secnum);
	ACTION_RETURN_INT(self->NextCompat(compat, secnum));
}

//===========================================================================
//
// scriptable line id iterator
//
//===========================================================================

class DLineIdIterator : public DObject, public FLineIdIterator
{
	DECLARE_ABSTRACT_CLASS(DLineIdIterator, DObject);
public:
	DLineIdIterator(FTagManager &tm, int tag)
		: FLineIdIterator(tm, tag)
	{
	}
};

IMPLEMENT_CLASS(DLineIdIterator, true, false);


static DLineIdIterator *CreateLTI(FLevelLocals *Level, int tag)
{
	return Create<DLineIdIterator>(Level->tagManager, tag);
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, CreateLineIDIterator, CreateLTI)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(tag);
	ACTION_RETURN_POINTER(CreateLTI(self, tag));
}

int NextLTI(DLineIdIterator *self)
{
	return self->Next();
}

DEFINE_ACTION_FUNCTION_NATIVE(DLineIdIterator, Next, NextLTI)
{
	PARAM_SELF_PROLOGUE(DLineIdIterator);
	ACTION_RETURN_INT(self->Next());
}

//===========================================================================
//
// scriptable actor iterator
//
//===========================================================================

class DActorIterator : public DObject, public NActorIterator
{
	DECLARE_ABSTRACT_CLASS(DActorIterator, DObject)

public:
	DActorIterator(AActor **hash, PClassActor *cls = nullptr, int tid = 0)
		: NActorIterator(hash, cls, tid)
	{
	}
};

IMPLEMENT_CLASS(DActorIterator, true, false);

static DActorIterator *CreateActI(FLevelLocals *Level, int tid, PClassActor *type)
{
	return Create<DActorIterator>(Level->TIDHash, type, tid);
}

DEFINE_ACTION_FUNCTION_NATIVE(FLevelLocals, CreateActorIterator, CreateActI)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(tid);
	PARAM_CLASS(type, AActor);
	ACTION_RETURN_OBJECT(CreateActI(self, tid, type));
}

static AActor *NextActI(DActorIterator *self)
{
	return self->Next();
}

DEFINE_ACTION_FUNCTION_NATIVE(DActorIterator, Next, NextActI)
{
	PARAM_SELF_PROLOGUE(DActorIterator);
	ACTION_RETURN_OBJECT(self->Next());
}

static void ReinitActI(DActorIterator *self)
{
	self->Reinit();
}

DEFINE_ACTION_FUNCTION_NATIVE(DActorIterator, Reinit, ReinitActI)
{
	PARAM_SELF_PROLOGUE(DActorIterator);
	self->Reinit();
	return 0;
}

//===========================================================================
//
// Behavior iterator. This can be created in two ways;
// -Across an Actor's behaviors
// -Across all behaviors of a given type
//
//===========================================================================

class DBehaviorIterator : public DObject
{
	DECLARE_ABSTRACT_CLASS(DBehaviorIterator, DObject)
	size_t _index;
	TArray<TObjPtr<DBehavior*>> _behaviors;

public:
	DBehaviorIterator(const AActor& mobj, PClass* type)
	{
		TMap<FName, TObjPtr<DBehavior*>>::ConstIterator it = { mobj.Behaviors };
		TMap<FName, TObjPtr<DBehavior*>>::ConstPair* pair = nullptr;
		while (it.NextPair(pair))
		{
			auto b = pair->Value.Get();
			if (b == nullptr)
				continue;

			if (type == nullptr || b->IsKindOf(type))
				_behaviors.Push(pair->Value);
		}

		Reinit();
	}

	DBehaviorIterator(const FLevelLocals& level, PClass* type, PClass* ownerType)
	{
		for (auto& b : level.ActorBehaviors)
		{
			if (ownerType != nullptr && !b->Owner->IsKindOf(ownerType))
				continue;

			if (type == nullptr || b->IsKindOf(type))
				_behaviors.Push(MakeObjPtr<DBehavior*>(b));
		}

		Reinit();
	}

	DBehavior* Next()
	{
		while (_index < _behaviors.Size())
		{
			auto b = _behaviors[_index++].Get();
			if (b != nullptr)
				return b;
		}

		return nullptr;
	}

	void Reinit() { _index = 0u; }

	void OnDestroy() override
	{
		_behaviors.Reset();
		Super::OnDestroy();
	}
};

IMPLEMENT_CLASS(DBehaviorIterator, true, false);

static DBehaviorIterator* CreateBehaviorItFromActor(AActor* mobj, PClass* type)
{
	return Create<DBehaviorIterator>(*mobj, type);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBehaviorIterator, CreateFrom, CreateBehaviorItFromActor)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT_NOT_NULL(mobj, AActor);
	PARAM_CLASS(type, DBehavior);
	ACTION_RETURN_OBJECT(CreateBehaviorItFromActor(mobj, type));
}

static DBehaviorIterator* CreateBehaviorIt(PClass* type, PClass* ownerType)
{
	return Create<DBehaviorIterator>(*primaryLevel, type, ownerType);
}

DEFINE_ACTION_FUNCTION_NATIVE(DBehaviorIterator, Create, CreateBehaviorIt)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(type, DBehavior);
	PARAM_CLASS(ownerType, AActor);
	ACTION_RETURN_OBJECT(CreateBehaviorIt(type, ownerType));
}

static DBehavior* NextBehavior(DBehaviorIterator* self)
{
	return self->Next();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBehaviorIterator, Next, NextBehavior)
{
	PARAM_SELF_PROLOGUE(DBehaviorIterator);
	ACTION_RETURN_OBJECT(self->Next());
}

static void ReinitBehavior(DBehaviorIterator* self)
{
	self->Reinit();
}

DEFINE_ACTION_FUNCTION_NATIVE(DBehaviorIterator, Reinit, ReinitBehavior)
{
	PARAM_SELF_PROLOGUE(DBehaviorIterator);
	self->Reinit();
	return 0;
}

DEFINE_FIELD_NAMED(DBlockLinesIterator, cres.line, curline);
DEFINE_FIELD_NAMED(DBlockLinesIterator, cres.Position, position);
DEFINE_FIELD_NAMED(DBlockLinesIterator, cres.portalflags, portalflags);

DEFINE_FIELD_NAMED(DBlockThingsIterator, cres.thing, thing);
DEFINE_FIELD_NAMED(DBlockThingsIterator, cres.Position, position);
DEFINE_FIELD_NAMED(DBlockThingsIterator, cres.portalflags, portalflags);
