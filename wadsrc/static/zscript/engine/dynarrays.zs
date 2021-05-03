// The VM uses 7 integral data types, so for dynamic array support we need one specific set of functions for each of these types.
// Do not use these structs directly, they are incomplete and only needed to create prototypes for the needed functions.

struct DynArray_I8 native
{
	native readonly uint Size;
	
	native void Copy(DynArray_I8 other);
	native void Move(DynArray_I8 other);
	native void Append (DynArray_I8 other);
	native uint Find(int item) const;
	native uint Push (int item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, int item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native uint Reserve (uint amount);
	native uint Max () const;
	native void Clear ();
}

struct DynArray_I16 native
{
	native readonly uint Size;

	native void Copy(DynArray_I16 other);
	native void Move(DynArray_I16 other);
	native void Append (DynArray_I16 other);
	native uint Find(int item) const;
	native uint Push (int item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, int item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native uint Reserve (uint amount);
	native uint Max () const;
	native void Clear ();
}

struct DynArray_I32 native
{
	native readonly uint Size;

	native void Copy(DynArray_I32 other);
	native void Move(DynArray_I32 other);
	native void Append (DynArray_I32 other);
	native uint Find(int item) const;
	native uint Push (int item);
	native vararg uint PushV (int item, ...);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, int item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native uint Reserve (uint amount);
	native uint Max () const;
	native void Clear ();
}

struct DynArray_F32 native
{
	native readonly uint Size;
	
	native void Copy(DynArray_F32 other);
	native void Move(DynArray_F32 other);
	native void Append (DynArray_F32 other);
	native uint Find(double item) const;
	native uint Push (double item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, double item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native uint Reserve (uint amount);
	native uint Max () const;
	native void Clear ();
}

struct DynArray_F64 native
{
	native readonly uint Size;
	
	native void Copy(DynArray_F64 other);
	native void Move(DynArray_F64 other);
	native void Append (DynArray_F64 other);
	native uint Find(double item) const;
	native uint Push (double item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, double item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native uint Reserve (uint amount);
	native uint Max () const;
	native void Clear ();
}

struct DynArray_Ptr native
{
	native readonly uint Size;
	
	native void Copy(DynArray_Ptr other);
	native void Move(DynArray_Ptr other);
	native void Append (DynArray_Ptr other);
	native uint Find(voidptr item) const;
	native uint Push (voidptr item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, voidptr item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native uint Reserve (uint amount);
	native uint Max () const;
	native void Clear ();
}

struct DynArray_Obj native
{
	native readonly uint Size;
	
	native void Copy(DynArray_Obj other);
	native void Move(DynArray_Obj other);
	native void Append (DynArray_Obj other);
	native uint Find(Object item) const;
	native uint Push (Object item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, Object item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native uint Reserve (uint amount);
	native uint Max () const;
	native void Clear ();
}

struct DynArray_String native
{
	native readonly uint Size;

	native void Copy(DynArray_String other);
	native void Move(DynArray_String other);
	native void Append (DynArray_String other);
	native uint Find(String item) const;
	native uint Push (String item);
	native bool Pop ();
	native void Delete (uint index, int deletecount = 1);
	native void Insert (uint index, String item);
	native void ShrinkToFit ();
	native void Grow (uint amount);
	native void Resize (uint amount);
	native uint Reserve (uint amount);
	native uint Max () const;
	native void Clear ();
}
