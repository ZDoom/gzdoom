#ifndef THINGDEF_TYPE_H
#define THINGDEF_TYPE_H
//==========================================================================
//
//
//
//==========================================================================

enum ExpValType
{
	VAL_Int,		// integer number
	VAL_Float,		// floating point number
	VAL_Unknown,	// nothing
	VAL_Array,		// Array (very limited right now)
	VAL_Object,		// Object reference
	VAL_Class,		// Class reference
	VAL_Pointer,	// Dereferenced variable (only used for addressing arrays for now.)
	VAL_Sound,		// Sound identifier. Internally it's an int.
	VAL_Name,		// A Name
	VAL_Color,		// A color.
	VAL_State,		// A State pointer

	// only used for accessing external variables to ensure proper conversion
	VAL_Fixed,
	VAL_Angle,
	VAL_Bool,
};

struct FExpressionType
{
	BYTE Type;
	BYTE BaseType;
	WORD size;				// for arrays;
	const PClass *ClassType;

	FExpressionType &operator=(int typeval)
	{
		Type = typeval;
		BaseType = 0;
		size = 0;
		ClassType = NULL;
		return *this;
	}

	FExpressionType &operator=(const PClass *cls)
	{
		Type = VAL_Class;
		BaseType = 0;
		size = 0;
		ClassType = cls;
		return *this;
	}

	bool operator==(int typeval) const
	{
		return Type == typeval;
	}

	bool operator!=(int typeval) const
	{
		return Type != typeval;
	}

	bool isNumeric() const
	{
		return Type == VAL_Float || Type == VAL_Int;
	}

	bool isPointer() const
	{
		return Type == VAL_Object || Type == VAL_Class;
	}

	FExpressionType GetBaseType() const
	{
		FExpressionType ret = *this;
		ret.Type = BaseType;
		ret.BaseType = 0;
		ret.size = 0;
		return ret;
	}


	// currently only used for args[].
	// Needs to be done differently for a generic implementation!
	void MakeArray(int siz)
	{
		BaseType = Type;
		Type = VAL_Array;
		size = siz;
	}

};

#endif
