#ifndef THINGDEF_TYPE_H
#define THINGDEF_TYPE_H
//==========================================================================
//
//
//
//==========================================================================

enum ExpValType
{
	VAL_Int,
	VAL_Float,
	VAL_Unknown,
	VAL_Array,
	VAL_Object,
	VAL_Class,
	VAL_Pointer,

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