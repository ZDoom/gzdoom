
struct Map_I32_I8 native
{
    native void Copy(Map_I32_I8 other);
    native void Move(Map_I32_I8 other);
    native void Swap(Map_I32_I8 other);
    native void Clear();
    native uint CountUsed() const;

    native int Get(int key);
    native bool CheckKey(int key) const;
    
    native version("4.11") int GetIfExists(int key) const;
    native version("4.11") int CheckValue(int key, out bool exists) const;
    
    native void Insert(int key, int value);
    native void InsertNew(int key);
    native void Remove(int key);
}

struct MapIterator_I32_I8 native
{
    native bool Init(Map_I32_I8 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native int GetKey();
    native int GetValue();
    native void SetValue(int value);
}

struct Map_I32_I16 native
{
    native void Copy(Map_I32_I16 other);
    native void Move(Map_I32_I16 other);
    native void Swap(Map_I32_I16 other);
    native void Clear();
    native uint CountUsed() const;

    native int Get(int key);
    native bool CheckKey(int key) const;
    
    native version("4.11") int GetIfExists(int key) const;
    native version("4.11") int CheckValue(int key, out bool exists) const;
    
    native void Insert(int key, int value);
    native void InsertNew(int key);
    native void Remove(int key);
}

struct MapIterator_I32_I16 native
{
    native bool Init(Map_I32_I16 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native int GetKey();
    native int GetValue();
    native void SetValue(int value);
}

struct Map_I32_I32 native
{
    native void Copy(Map_I32_I32 other);
    native void Move(Map_I32_I32 other);
    native void Swap(Map_I32_I32 other);
    native void Clear();
    native uint CountUsed() const;

    native int Get(int key);
    native bool CheckKey(int key) const;
    
    native version("4.11") int GetIfExists(int key) const;
    native version("4.11") int CheckValue(int key, out bool exists) const;
    
    native void Insert(int key, int value);
    native void InsertNew(int key);
    native void Remove(int key);
}

struct MapIterator_I32_I32 native
{
    native bool Init(Map_I32_I32 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native int GetKey();
    native int GetValue();
    native void SetValue(int value);
}

struct Map_I32_F32 native
{
    native void Copy(Map_I32_F32 other);
    native void Move(Map_I32_F32 other);
    native void Swap(Map_I32_F32 other);
    native void Clear();
    native uint CountUsed() const;

    native double Get(int key);
    native bool CheckKey(int key) const;
    
    native version("4.11") double GetIfExists(int key) const;
    native version("4.11") double CheckValue(int key, out bool exists) const;
    
    native void Insert(int key, double value);
    native void InsertNew(int key);
    native void Remove(int key);
}

struct MapIterator_I32_F32 native
{
    native bool Init(Map_I32_F32 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native int GetKey();
    native double GetValue();
    native void SetValue(double value);
}

struct Map_I32_F64 native
{
    native void Copy(Map_I32_F64 other);
    native void Move(Map_I32_F64 other);
    native void Swap(Map_I32_F64 other);
    native void Clear();
    native uint CountUsed() const;

    native double Get(int key);
    native bool CheckKey(int key) const;
    
    native version("4.11") double GetIfExists(int key) const;
    native version("4.11") double CheckValue(int key, out bool exists) const;
    
    native void Insert(int key, double value);
    native void InsertNew(int key);
    native void Remove(int key);
}

struct MapIterator_I32_F64 native
{
    native bool Init(Map_I32_F64 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native int GetKey();
    native double GetValue();
    native void SetValue(double value);
}

struct Map_I32_Obj native
{
    native void Copy(Map_I32_Obj other);
    native void Move(Map_I32_Obj other);
    native void Swap(Map_I32_Obj other);
    native void Clear();
    native uint CountUsed() const;

    native Object Get(int key);
    native bool CheckKey(int key) const;
    
    native version("4.11") Object GetIfExists(int key) const;
    native version("4.11") Object CheckValue(int key, out bool exists) const;
    
    native void Insert(int key, Object value);
    native void InsertNew(int key);
    native void Remove(int key);
}

struct MapIterator_I32_Obj native
{
    native bool Init(Map_I32_Obj other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native int GetKey();
    native Object GetValue();
    native void SetValue(Object value);
}

struct Map_I32_Ptr native
{
    native void Copy(Map_I32_Ptr other);
    native void Move(Map_I32_Ptr other);
    native void Swap(Map_I32_Ptr other);
    native void Clear();
    native uint CountUsed() const;

    native voidptr Get(int key);
    native bool CheckKey(int key) const;
    
    native version("4.11") voidptr GetIfExists(int key) const;
    native version("4.11") voidptr CheckValue(int key, out bool exists) const;
    
    native void Insert(int key, voidptr value);
    native void InsertNew(int key);
    native void Remove(int key);
}

struct MapIterator_I32_Ptr native
{
    native bool Init(Map_I32_Ptr other);
    native bool Next();
    
    native int GetKey();
    native voidptr GetValue();
    native void SetValue(voidptr value);
}

struct Map_I32_Str native
{
    native void Copy(Map_I32_Str other);
    native void Move(Map_I32_Str other);
    native void Swap(Map_I32_Str other);
    native void Clear();
    native uint CountUsed() const;

    native String Get(int key);
    native bool CheckKey(int key) const;
    
    native version("4.11") String GetIfExists(int key) const;
    native version("4.11") String CheckValue(int key, out bool exists) const;
    
    native void Insert(int key, String value);
    native void InsertNew(int key);
    native void Remove(int key);
}

struct MapIterator_I32_Str native
{
    native bool Init(Map_I32_Str other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native int GetKey();
    native String GetValue();
    native void SetValue(String value);
}

// ---------------

struct Map_Str_I8 native
{
    native void Copy(Map_Str_I8 other);
    native void Move(Map_Str_I8 other);
    native void Swap(Map_Str_I8 other);
    native void Clear();
    native uint CountUsed() const;

    native int Get(String key);
    native bool CheckKey(String key) const;
    
    native version("4.11") int GetIfExists(String key) const;
    native version("4.11") int CheckValue(String key, out bool exists) const;
    
    native void Insert(String key, int value);
    native void InsertNew(String key);
    native void Remove(String key);
}

struct MapIterator_Str_I8 native
{
    native bool Init(Map_Str_I8 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native String GetKey();
    native int GetValue();
    native void SetValue(int value);
}

struct Map_Str_I16 native
{
    native void Copy(Map_Str_I16 other);
    native void Move(Map_Str_I16 other);
    native void Swap(Map_Str_I16 other);
    native void Clear();
    native uint CountUsed() const;

    native int Get(String key);
    native bool CheckKey(String key) const;
    
    native version("4.11") int GetIfExists(String key) const;
    native version("4.11") int CheckValue(String key, out bool exists) const;
    
    native void Insert(String key, int value);
    native void InsertNew(String key);
    native void Remove(String key);
}

struct MapIterator_Str_I16 native
{
    native bool Init(Map_Str_I16 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native String GetKey();
    native int GetValue();
    native void SetValue(int value);
}

struct Map_Str_I32 native
{
    native void Copy(Map_Str_I32 other);
    native void Move(Map_Str_I32 other);
    native void Swap(Map_Str_I32 other);
    native void Clear();
    native uint CountUsed() const;

    native int Get(String key);
    native bool CheckKey(String key) const;
    
    native version("4.11") int GetIfExists(String key) const;
    native version("4.11") int CheckValue(String key, out bool exists) const;
    
    native void Insert(String key, int value);
    native void InsertNew(String key);
    native void Remove(String key);
}

struct MapIterator_Str_I32 native
{
    native bool Init(Map_Str_I32 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native String GetKey();
    native int GetValue();
    native void SetValue(int value);
}

struct Map_Str_F32 native
{
    native void Copy(Map_Str_F32 other);
    native void Move(Map_Str_F32 other);
    native void Swap(Map_Str_F32 other);
    native void Clear();
    native uint CountUsed() const;

    native double Get(String key);
    native bool CheckKey(String key) const;
    
    native version("4.11") double GetIfExists(String key) const;
    native version("4.11") double CheckValue(String key, out bool exists) const;
    
    native void Insert(String key, double value);
    native void InsertNew(String key);
    native void Remove(String key);
}

struct MapIterator_Str_F32 native
{
    native bool Init(Map_Str_F32 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native String GetKey();
    native double GetValue();
    native void SetValue(double value);
}

struct Map_Str_F64 native
{
    native void Copy(Map_Str_F64 other);
    native void Move(Map_Str_F64 other);
    native void Swap(Map_Str_F64 other);
    native void Clear();
    native uint CountUsed() const;

    native double Get(String key);
    native bool CheckKey(String key) const;
    
    native version("4.11") double GetIfExists(String key) const;
    native version("4.11") double CheckValue(String key, out bool exists) const;
    
    native void Insert(String key, double value);
    native void InsertNew(String key);
    native void Remove(String key);
}

struct MapIterator_Str_F64 native
{
    native bool Init(Map_Str_F64 other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native String GetKey();
    native double GetValue();
    native void SetValue(double value);
}

struct Map_Str_Obj native
{
    native void Copy(Map_Str_Obj other);
    native void Move(Map_Str_Obj other);
    native void Swap(Map_Str_Obj other);
    native void Clear();
    native uint CountUsed() const;

    native Object Get(String key);
    native bool CheckKey(String key) const;
    
    native version("4.11") Object GetIfExists(String key) const;
    native version("4.11") Object CheckValue(String key, out bool exists) const;
    
    native void Insert(String key, Object value);
    native void InsertNew(String key);
    native void Remove(String key);
}

struct MapIterator_Str_Obj native
{
    native bool Init(Map_Str_Obj other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native String GetKey();
    native Object GetValue();
    native void SetValue(Object value);
}

struct Map_Str_Ptr native
{
    native void Copy(Map_Str_Ptr other);
    native void Move(Map_Str_Ptr other);
    native void Swap(Map_Str_Ptr other);
    native void Clear();
    native uint CountUsed() const;

    native voidptr Get(String key);
    native bool CheckKey(String key) const;
    
    native version("4.11") voidptr GetIfExists(String key) const;
    native version("4.11") voidptr CheckValue(String key, out bool exists) const;
    
    native void Insert(String key, voidptr value);
    native void InsertNew(String key);
    native void Remove(String key);
}

struct MapIterator_Str_Ptr native
{
    native bool Init(Map_Str_Ptr other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native String GetKey();
    native voidptr GetValue();
    native void SetValue(voidptr value);
}

struct Map_Str_Str native
{
    native void Copy(Map_Str_Str other);
    native void Move(Map_Str_Str other);
    native void Swap(Map_Str_Str other);
    native void Clear();
    native uint CountUsed() const;

    native String Get(String key);
    native bool CheckKey(String key) const;
    
    native version("4.11") String GetIfExists(String key) const;
    native version("4.11") String CheckValue(String key, out bool exists) const;
    
    native void Insert(String key, String value);
    native void InsertNew(String key);
    native void Remove(String key);
}

struct MapIterator_Str_Str native
{
    native bool Init(Map_Str_Str other);
    native bool ReInit();
    
    native bool Valid();
    native bool Next();
    
    native String GetKey();
    native String GetValue();
    native void SetValue(String value);
}
