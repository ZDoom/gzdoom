#include "tarray.h"
#include "dobject.h"
#include "zstring.h"
#include "vm.h"
#include "types.h"
#include "v_draw.h"
#include "maps.h"


//==========================================================================
//
// MAP_GC_WRITE_BARRIER
//
//==========================================================================


#define MAP_GC_WRITE_BARRIER(x) { \
    TMapIterator<typename M::KeyType, DObject*> it(*x);\
    typename M::Pair * p;\
    while(it.NextPair(p)){\
        GC::WriteBarrier(p->Value);\
    }\
}


//==========================================================================
//
// MapCopy
//
//==========================================================================


template<typename M> void MapCopy(M * self, M * other)
{
    if constexpr(std::is_same_v<typename M::ValueType,DObject*>)
    {
        MAP_GC_WRITE_BARRIER(other);
    }
    *self = *other; // how does this handle self->info? TODO double check.
    self->info->rev++;
}


//==========================================================================
//
// MapMove
//
//==========================================================================


template<typename M> void MapMove(M * self, M * other)
{
    if constexpr(std::is_same_v<typename M::ValueType,DObject*>)
    {
        MAP_GC_WRITE_BARRIER(other);
    }
    self->TransferFrom(*other);
    self->info->rev++;
    other->info->rev++;
}


//==========================================================================
//
// MapSwap
//
//==========================================================================


template<typename M> void MapSwap(M * self, M * other)
{
    if constexpr(std::is_same_v<typename M::ValueType,DObject*>)
    {
        MAP_GC_WRITE_BARRIER(other);
    }
    self->Swap(*other);
    self->info->rev++;
    other->info->rev++;
}


//==========================================================================
//
// MapClear
//
//==========================================================================


template<typename M> void MapClear(M * self)
{
    if constexpr(std::is_same_v<typename M::ValueType,DObject*>)
    {
        MAP_GC_WRITE_BARRIER(self);
    }
    self->Clear();
    self->info->rev++;
}


//==========================================================================
//
//
//
//==========================================================================


template<typename T>
using expand_types_vm = 
std::conditional_t<std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t>, uint32_t , /* expand 8/16-bit to 32-bit */
    std::conditional_t<std::is_same_v<T, float> , double , /* expand float to double */
        std::conditional_t<std::is_same_v<T, FString> , const FString & , T>>>; /* change String to String ref */

template<typename M> unsigned int MapCountUsed(M * self)
{
    return self->CountUsed();
}

template<typename M> expand_types_vm<typename M::ValueType> MapGet(M * self,expand_types_vm<typename M::KeyType> key)
{
    typename M::ValueType * v = self->CheckKey(key);
    if (v) {
        return *v;
    }
    else
    {
        typename M::ValueType n {};
        self->Insert(key,n);
        return n;
    }
}

template<typename M> void MapGetString(M * self,expand_types_vm<typename M::KeyType> key, FString &out)
{
    FString * v = self->CheckKey(key);
    if (v) {
        out = *v;
    }
    else
    {
        out = FString();
        self->Insert(key,out);
    }
}

template<typename M> int MapCheckKey(M * self, expand_types_vm<typename M::KeyType> key)
{
    return self->CheckKey(key) != nullptr;
}


//==========================================================================
//
// MapInsert
//
//==========================================================================


template<typename M> void MapInsert(M * self, expand_types_vm<typename M::KeyType> key, expand_types_vm<typename M::ValueType> value)
{
    if constexpr(std::is_same_v<typename M::ValueType, DObject*>)
    {
        MAP_GC_WRITE_BARRIER(self);
        GC::WriteBarrier(value);
    }

    if constexpr(std::is_same_v<typename M::ValueType, float>)
    {
        self->Insert(key,static_cast<float>(value));
    }
    else
    {
        self->Insert(key, value);
    }
    self->info->rev++; // invalidate iterators
}


//==========================================================================
//
//
//
//==========================================================================


template<typename M> void MapInsertNew(M * self, expand_types_vm<typename M::KeyType> key)
{
    if constexpr(std::is_same_v<typename M::ValueType, DObject*>)
    {
        MAP_GC_WRITE_BARRIER(self);
    }
    self->Insert(key,{});
    self->info->rev++; // invalidate iterators
}

template<typename M> void MapRemove(M * self, expand_types_vm<typename M::KeyType> key)
{
    self->Remove(key);
    self->info->rev++; // invalidate iterators
}


//==========================================================================
//
//
//
//==========================================================================


template<typename I, typename M> int MapIteratorInit(I * self, M * other)
{
    return self->Init(*other);
}

template<typename I> int MapIteratorReInit(I * self)
{
    return self->ReInit();
}

template<typename I> int MapIteratorValid(I * self)
{
    return self->Valid();
}

template<typename I> int MapIteratorNext(I * self)
{
    return self->Next();
}

template<typename I> expand_types_vm<typename I::KeyType> MapIteratorGetKey(I * self)
{
    return self->GetKey();
}

template<typename I> void MapIteratorGetKeyString(I * self, FString &out)
{
    out = self->GetKey();
}

template<typename I> expand_types_vm<typename I::ValueType> MapIteratorGetValue(I * self)
{
    return self->GetValue();
}

template<typename I> void MapIteratorGetValueString(I * self, FString &out)
{
    out = self->GetValue();
}

template<typename I> void MapIteratorSetValue(I * self, expand_types_vm<typename I::ValueType> value)
{
    auto & val = self->GetValue();
    if constexpr(std::is_same_v<typename I::ValueType, DObject*>)
    {
        GC::WriteBarrier(val);
        GC::WriteBarrier(value);
    }

    if constexpr(std::is_same_v<typename I::ValueType, float>)
    {
        val = static_cast<float>(value);
    }
    else
    {
        val = value;
    }
}


//==========================================================================
//
//
//
//==========================================================================

#define PARAM_VOIDPOINTER(X) PARAM_POINTER( X , void )
#define PARAM_OBJPOINTER(X) PARAM_OBJECT( X , DObject )

#define _DEF_MAP( name, key_type, value_type, PARAM_KEY, PARAM_VALUE ) \
    typedef ZSMap<key_type , value_type> name;\
    DEFINE_ACTION_FUNCTION_NATIVE( name , Copy , MapCopy< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_POINTER( other, name ); \
        MapCopy(self , other); \
        return 0; \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , Move , MapMove< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_POINTER( other, name ); \
        MapMove(self , other); \
        return 0; \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , Swap , MapSwap< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_POINTER( other, name ); \
        MapSwap(self , other); \
        return 0; \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , Clear , MapClear< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        MapClear(self); \
        return 0; \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name, CountUsed, MapCountUsed< name >) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        ACTION_RETURN_INT( MapCountUsed(self) ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name, CheckKey, MapCheckKey< name >) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_KEY( key ); \
        ACTION_RETURN_BOOL( MapCheckKey(self, key) ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name, Insert, MapInsert< name >) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_KEY( key ); \
        PARAM_VALUE( value ); \
        MapInsert(self, key, value); \
        return 0; \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name, InsertNew, MapInsertNew< name >) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_KEY( key ); \
        MapInsertNew(self, key); \
        return 0; \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name, Remove, MapRemove< name >) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_KEY( key ); \
        MapRemove(self, key); \
        return 0; \
    }

#define DEF_MAP_X_X( name, key_type, value_type, PARAM_KEY, PARAM_VALUE , ACTION_RETURN_VALUE ) \
    _DEF_MAP( name , key_type , value_type , PARAM_KEY , PARAM_VALUE ) \
    DEFINE_ACTION_FUNCTION_NATIVE( name, Get, MapGet< name >) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_KEY( key ); \
        ACTION_RETURN_VALUE( MapGet(self, key) ); \
    }

#define DEF_MAP_X_S( name, key_type, PARAM_KEY ) \
    _DEF_MAP( name , key_type , FString , PARAM_KEY , PARAM_STRING ) \
    DEFINE_ACTION_FUNCTION_NATIVE( name, Get, MapGetString< name >) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_KEY( key ); \
        FString out; \
        MapGetString(self, key, out); \
        ACTION_RETURN_STRING( out ); \
    }

#define COMMA ,

#define _DEF_MAP_IT( map_name , name, key_type, value_type, PARAM_VALUE ) \
    typedef ZSMapIterator<key_type , value_type> name; \
    DEFINE_ACTION_FUNCTION_NATIVE( name , Init , MapIteratorInit< name COMMA map_name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_POINTER( other, map_name ); \
        ACTION_RETURN_BOOL( MapIteratorInit(self , other) ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , ReInit , MapIteratorReInit< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        ACTION_RETURN_BOOL( MapIteratorReInit(self) ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , Valid , MapIteratorValid< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        ACTION_RETURN_BOOL( MapIteratorValid(self) ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , Next , MapIteratorNext< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        ACTION_RETURN_BOOL( MapIteratorNext(self) ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , SetValue , MapIteratorSetValue< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        PARAM_VALUE( value ); \
        MapIteratorSetValue(self, value); \
        return 0; \
    }
/*
    DEFINE_ACTION_FUNCTION_NATIVE( name , GetKey , MapIteratorGetKey< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        ACTION_RETURN_KEY( MapIteratorGetKey(self) ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , GetValue , MapIteratorGetValue< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        ACTION_RETURN_VALUE( MapIteratorGetValue(self) ); \
    } \
*/

#define DEF_MAP_IT_I_X( map_name , name , value_type , PARAM_VALUE , ACTION_RETURN_VALUE ) \
    _DEF_MAP_IT( map_name , name , uint32_t , value_type , PARAM_VALUE ) \
    DEFINE_ACTION_FUNCTION_NATIVE( name , GetKey , MapIteratorGetKey< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        ACTION_RETURN_INT( MapIteratorGetKey(self) ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , GetValue , MapIteratorGetValue< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        ACTION_RETURN_VALUE( MapIteratorGetValue(self) ); \
    }

#define DEF_MAP_IT_S_X( map_name , name , value_type , PARAM_VALUE , ACTION_RETURN_VALUE ) \
    _DEF_MAP_IT( map_name , name , FString , value_type , PARAM_VALUE ) \
    DEFINE_ACTION_FUNCTION_NATIVE( name , GetKey , MapIteratorGetKeyString< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        FString out; \
        MapIteratorGetKeyString(self , out); \
        ACTION_RETURN_STRING( out ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( name , GetValue , MapIteratorGetValue< name > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( name ); \
        ACTION_RETURN_VALUE( MapIteratorGetValue(self) ); \
    }

#define DEF_MAP_IT_I_S() \
    _DEF_MAP_IT( FMap_I32_Str , FMapIterator_I32_Str , uint32_t , FString , PARAM_STRING ) \
    DEFINE_ACTION_FUNCTION_NATIVE( FMapIterator_I32_Str , GetKey , MapIteratorGetKey< FMapIterator_I32_Str > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( FMapIterator_I32_Str ); \
        ACTION_RETURN_INT( MapIteratorGetKey(self) ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( FMapIterator_I32_Str , GetValue , MapIteratorGetValueString< FMapIterator_I32_Str > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( FMapIterator_I32_Str ); \
        FString out; \
        MapIteratorGetValueString(self , out); \
        ACTION_RETURN_STRING( out ); \
    }

#define DEF_MAP_IT_S_S() \
    _DEF_MAP_IT( FMap_Str_Str , FMapIterator_Str_Str , FString , FString , PARAM_STRING ) \
    DEFINE_ACTION_FUNCTION_NATIVE( FMapIterator_Str_Str , GetKey , MapIteratorGetKeyString< FMapIterator_Str_Str > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( FMapIterator_Str_Str ); \
        FString out; \
        MapIteratorGetKeyString(self , out); \
        ACTION_RETURN_STRING( out ); \
    } \
    DEFINE_ACTION_FUNCTION_NATIVE( FMapIterator_Str_Str , GetValue , MapIteratorGetValueString< FMapIterator_Str_Str > ) \
    { \
        PARAM_SELF_STRUCT_PROLOGUE( FMapIterator_Str_Str ); \
        FString out; \
        MapIteratorGetValueString(self , out); \
        ACTION_RETURN_STRING( out ); \
    }



#define DEFINE_MAP_AND_IT_I_X( name , value_type , PARAM_VALUE , ACTION_RETURN_VALUE ) \
    DEF_MAP_X_X( FMap_ ## name , uint32_t , value_type , PARAM_INT , PARAM_VALUE , ACTION_RETURN_VALUE ) \
    DEF_MAP_IT_I_X( FMap_ ## name , FMapIterator_ ## name , value_type , PARAM_VALUE , ACTION_RETURN_VALUE )


#define DEFINE_MAP_AND_IT_S_X( name , value_type , PARAM_VALUE , ACTION_RETURN_VALUE ) \
    DEF_MAP_X_X( FMap_ ## name ,  FString , value_type , PARAM_STRING , PARAM_VALUE , ACTION_RETURN_VALUE ) \
    DEF_MAP_IT_S_X( FMap_ ## name , FMapIterator_ ## name , value_type , PARAM_VALUE , ACTION_RETURN_VALUE )

#define DEFINE_MAP_AND_IT_I_S() \
    DEF_MAP_X_S( FMap_I32_Str , uint32_t , PARAM_INT ) \
    DEF_MAP_IT_I_S()

#define DEFINE_MAP_AND_IT_S_S() \
    DEF_MAP_X_S( FMap_Str_Str , FString , PARAM_STRING ) \
    DEF_MAP_IT_S_S()

DEFINE_MAP_AND_IT_I_X(I32_I8  , uint8_t  , PARAM_INT         , ACTION_RETURN_INT);
DEFINE_MAP_AND_IT_I_X(I32_I16 , uint16_t , PARAM_INT         , ACTION_RETURN_INT);
DEFINE_MAP_AND_IT_I_X(I32_I32 , uint32_t , PARAM_INT         , ACTION_RETURN_INT);
DEFINE_MAP_AND_IT_I_X(I32_F32 , float    , PARAM_FLOAT       , ACTION_RETURN_FLOAT);
DEFINE_MAP_AND_IT_I_X(I32_F64 , double   , PARAM_FLOAT       , ACTION_RETURN_FLOAT);
DEFINE_MAP_AND_IT_I_X(I32_Obj , DObject* , PARAM_OBJPOINTER  , ACTION_RETURN_OBJECT);
DEFINE_MAP_AND_IT_I_X(I32_Ptr , void*    , PARAM_VOIDPOINTER , ACTION_RETURN_POINTER);
DEFINE_MAP_AND_IT_I_S();

DEFINE_MAP_AND_IT_S_X(Str_I8  , uint8_t  , PARAM_INT         , ACTION_RETURN_INT);
DEFINE_MAP_AND_IT_S_X(Str_I16 , uint16_t , PARAM_INT         , ACTION_RETURN_INT);
DEFINE_MAP_AND_IT_S_X(Str_I32 , uint32_t , PARAM_INT         , ACTION_RETURN_INT);
DEFINE_MAP_AND_IT_S_X(Str_F32 , float    , PARAM_FLOAT       , ACTION_RETURN_FLOAT);
DEFINE_MAP_AND_IT_S_X(Str_F64 , double   , PARAM_FLOAT       , ACTION_RETURN_FLOAT);
DEFINE_MAP_AND_IT_S_X(Str_Obj , DObject* , PARAM_OBJPOINTER  , ACTION_RETURN_OBJECT);
DEFINE_MAP_AND_IT_S_X(Str_Ptr , void*    , PARAM_VOIDPOINTER , ACTION_RETURN_POINTER);
DEFINE_MAP_AND_IT_S_S();