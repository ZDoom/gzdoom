#pragma once

// Simple lightweight reference counting pointer alternative for std::shared_ptr which stores the reference counter in the handled object itself.

// Base class for handled objects
class RefCountedBase
{
public:
	void IncRef() { refCount++; }
	void DecRef() { if (--refCount <= 0) delete this; }
private:
	int refCount = 0;
protected:
	virtual ~RefCountedBase() = default;
};

// The actual pointer object
template<class T> 
class RefCountedPtr
{
public:
    ~RefCountedPtr() 
	{
        if (ptr) ptr->DecRef();
    }

    RefCountedPtr() : ptr(nullptr) 
	{}
 
    explicit RefCountedPtr(T* p) : ptr(p)
    {
        if (ptr) ptr->IncRef();
    }

	RefCountedPtr(const RefCountedPtr& r) : ptr(r.ptr)
	{
		if (ptr) ptr->IncRef();
	}

	RefCountedPtr(RefCountedPtr&& r) : ptr(r.ptr)
	{
		r.ptr = nullptr;
	}

    RefCountedPtr & operator=(const RefCountedPtr& r) 
    {
		if (this != &r)
		{
            if (ptr) ptr->DecRef();
			ptr = r.ptr;
            if (ptr) ptr->IncRef();
		}
        return *this;
    }
 
    RefCountedPtr& operator=(T* r)
    {
        if (ptr != r)
        {
            if (ptr) ptr->DecRef();
            ptr = r;
            if (ptr) ptr->IncRef();
        }
        return *this;
    }

    RefCountedPtr & operator=(RefCountedPtr&& r)
    {
		if (this != &r)
		{
			if (ptr) ptr->DecRef();
			ptr = r.ptr;
			r.ptr = nullptr;
		}
        return *this;
    }

    bool operator==(T* p) const
    {
        return ptr == p;
    }

    bool operator!=(T* p) const
    {
        return ptr != p;
    }

    bool operator==(const RefCountedPtr &p) const
    {
        return ptr == p.ptr;
    }

    bool operator!=(const RefCountedPtr& p) const
    {
        return ptr != p.ptr;
    }

    T& operator* () const
	{
        return *ptr;
    }
 
    T* operator-> () const 
	{
        return ptr;
    }
 
    T* get() const 
	{
        return ptr;
    }
 
private:
 
    T * ptr;
};