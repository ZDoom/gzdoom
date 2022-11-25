#pragma once

#include <memory>
#include "tarray.h"
#include "refcounted.h"

class ZSMapInfo : public RefCountedBase
{
public:
    void * self = nullptr;
    int rev = 0;
};

struct ZSFMap : FMap {
    RefCountedPtr<RefCountedBase> info;
};

template<class KT, class VT>
class ZSMap : public TMap<KT,VT>
{
public:
    RefCountedPtr<ZSMapInfo> info;
    ZSMap() :
        TMap<KT,VT>(), info(new ZSMapInfo)
    {
        info->self = this;
    }
    ~ZSMap()
    {
        info->self = nullptr;
    }
};

template<class KT, class VT>
struct ZSMapIterator
{
    RefCountedPtr<ZSMapInfo> info;
    TMapIterator<KT,VT>  *it = nullptr;
    typename ZSMap<KT,VT>::Pair *p = nullptr;

    typedef KT KeyType;
    typedef VT ValueType;

    int rev = 0;

    ~ZSMapIterator()
    {
        if(it) delete it;
    }

    bool Valid()
    {
        return it && p && info.get() && info->self && info->rev == rev;
    }

    bool ReInit()
    {
        if(info.get() && info->self) {
            if(it) delete it;
            it = new TMapIterator<KT,VT>(*static_cast<ZSMap<KT,VT>*>(info->self));
            rev = info->rev;
            p = nullptr;
            return true;
        }
        return false;
    }

    bool Init(ZSMap<KT,VT> &m)
    {
        info = m.info;
        return ReInit();
    }

    bool Next()
    {
        if(it && info.get() && info->self && info->rev == rev)
        {
            p = nullptr;
            return it->NextPair(p);
        }
        else
        {
            ThrowAbortException(X_FORMAT_ERROR,"MapIterator::Next called from invalid iterator");
        }
    }

    VT& GetValue()
    {
        if(p && info.get() && info->self && info->rev == rev)
        {
            return p->Value;
        }
        else
        {
            ThrowAbortException(X_FORMAT_ERROR,p ? "MapIterator::GetValue called from invalid iterator" :  "MapIterator::GetValue called from invalid position");
        }
    }

    const KT& GetKey()
    {
        if(p && info.get() && info->self && info->rev == rev)
        {
            return p->Key;
        }
        else
        {
            ThrowAbortException(X_FORMAT_ERROR,p ? "MapIterator::GetKey called from invalid iterator" : "MapIterator::GetKey called from invalid position");
        }
    }
};