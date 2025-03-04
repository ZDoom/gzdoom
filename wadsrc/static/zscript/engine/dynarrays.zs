// The VM uses 7 integral data types, so for dynamic array support we need one specific set of functions for each of these types.
// Do not use these structs directly, they are incomplete and only needed to create prototypes for the needed functions.

struct DynArray_I8 native unsafe(internal)
{
	native readonly int Size;
	
	native void Copy(DynArray_I8 other);
	native void Move(DynArray_I8 other);
	native void Append (DynArray_I8 other);
	native int Find(int item) const;
	native int Push(int item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, int item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native int Reserve(uint amount);
	native int Max() const;
	native void Clear ();
}

struct DynArray_I16 native unsafe(internal)
{
	native readonly int Size;

	native void Copy(DynArray_I16 other);
	native void Move(DynArray_I16 other);
	native void Append (DynArray_I16 other);
	native int Find(int item) const;
	native int Push(int item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, int item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native int Reserve(uint amount);
	native int Max() const;
	native void Clear ();
}

struct DynArray_I32 native unsafe(internal)
{
	native readonly int Size;

	native void Copy(DynArray_I32 other);
	native void Move(DynArray_I32 other);
	native void Append (DynArray_I32 other);
	native int Find(int item) const;
	native int Push(int item);
	native vararg uint PushV (int item, ...);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, int item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native int Reserve(uint amount);
	native int Max() const;
	native void Clear ();
}

struct DynArray_F32 native unsafe(internal)
{
	native readonly int Size;
	
	native void Copy(DynArray_F32 other);
	native void Move(DynArray_F32 other);
	native void Append (DynArray_F32 other);
	native int Find(double item) const;
	native int Push(double item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, double item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native int Reserve(uint amount);
	native int Max() const;
	native void Clear ();
}

struct DynArray_F64 native unsafe(internal)
{
	native readonly int Size;
	
	native void Copy(DynArray_F64 other);
	native void Move(DynArray_F64 other);
	native void Append (DynArray_F64 other);
	native int Find(double item) const;
	native int Push(double item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, double item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native int Reserve(uint amount);
	native int Max() const;
	native void Clear ();
}

struct DynArray_Ptr native unsafe(internal)
{
	native readonly int Size;
	
	native void Copy(DynArray_Ptr other);
	native void Move(DynArray_Ptr other);
	native void Append (DynArray_Ptr other);
	native int Find(voidptr item) const;
	native int Push(voidptr item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, voidptr item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native int Reserve(uint amount);
	native int Max() const;
	native void Clear ();
}

struct DynArray_Obj native unsafe(internal)
{
	native readonly int Size;
	
	native void Copy(DynArray_Obj other);
	native void Move(DynArray_Obj other);
	native void Append (DynArray_Obj other);
	native int Find(Object item) const;
	native int Push(Object item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, Object item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native int Reserve(uint amount);
	native int Max() const;
	native void Clear ();
}

struct DynArray_String native unsafe(internal)
{
	native readonly int Size;

	native void Copy(DynArray_String other);
	native void Move(DynArray_String other);
	native void Append (DynArray_String other);
	native int Find(String item) const;
	native int Push(String item);
	native vararg uint PushV(String item, ...);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, String item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native int Reserve(uint amount);
	native int Max() const;
	native void Clear ();
}
