// ====================================================================================================================
// ====================================================================================================================
//  xs_Float.h
//
// Source: "Know Your FPU: Fixing Floating Fast"
//         http://www.stereopsis.com/sree/fpu2006.html
//
// xs_CRoundToInt:  Round toward nearest, but ties round toward even (just like FISTP)
// xs_ToInt:        Round toward zero, just like the C (int) cast
// xs_FloorToInt:   Round down
// xs_CeilToInt:    Round up
// xs_RoundToInt:   Round toward nearest, but ties round up
// ====================================================================================================================
// ====================================================================================================================
#ifndef _xs_FLOAT_H_
#define _xs_FLOAT_H_

// ====================================================================================================================
//  Defines
// ====================================================================================================================
#ifndef _xs_DEFAULT_CONVERSION
#define _xs_DEFAULT_CONVERSION      0
#endif //_xs_DEFAULT_CONVERSION


#if __BIG_ENDIAN__
	#define _xs_iexp_				0
	#define _xs_iman_				1
#else
	#define _xs_iexp_				1       //intel is little endian
	#define _xs_iman_				0
#endif //BigEndian_

#ifdef __GNUC__
#define finline inline
#else
#define finline __forceinline
#endif

union _xs_doubleints
{
	real64 val;
	uint32 ival[2];
};

#if 0
#define _xs_doublecopysgn(a,b)      ((int32*)&a)[_xs_iexp_]&=~(((int32*)&b)[_xs_iexp_]&0x80000000)
#define _xs_doubleisnegative(a)     ((((int32*)&a)[_xs_iexp_])|0x80000000)
#endif

// ====================================================================================================================
//  Constants
// ====================================================================================================================
const real64 _xs_doublemagic			= real64 (6755399441055744.0); 	    //2^52 * 1.5,  uses limited precisicion to floor
const real64 _xs_doublemagicdelta      	= (1.5e-8);                         //almost .5f = .5f + 1e^(number of exp bit)
const real64 _xs_doublemagicroundeps	= (.5f-_xs_doublemagicdelta);       //almost .5f = .5f - 1e^(number of exp bit)


// ====================================================================================================================
//  Prototypes
// ====================================================================================================================
static int32 xs_CRoundToInt      (real64 val, real64 dmr =  _xs_doublemagic);
static int32 xs_ToInt            (real64 val, real64 dme = -_xs_doublemagicroundeps);
static int32 xs_FloorToInt       (real64 val, real64 dme =  _xs_doublemagicroundeps);
static int32 xs_CeilToInt        (real64 val, real64 dme =  _xs_doublemagicroundeps);
static int32 xs_RoundToInt       (real64 val);

//int32 versions
finline static int32 xs_CRoundToInt      (int32 val)   {return val;}
finline static int32 xs_ToInt            (int32 val)   {return val;}



// ====================================================================================================================
//  Fix Class
// ====================================================================================================================
template <int32 N> class xs_Fix
{
public:
    typedef int32 Fix;

    // ====================================================================================================================
    //  Basic Conversion from Numbers
    // ====================================================================================================================
    finline static Fix       ToFix       (int32 val)    {return val<<N;}
    finline static Fix       ToFix       (real64 val)   {return xs_ConvertToFixed(val);}

    // ====================================================================================================================
    //  Basic Conversion to Numbers
    // ====================================================================================================================
    finline static real64    ToReal      (Fix f)        {return real64(f)/real64(1<<N);}
    finline static int32     ToInt       (Fix f)        {return f>>N;}



protected:
    // ====================================================================================================================
    // Helper function - mainly to preserve _xs_DEFAULT_CONVERSION
    // ====================================================================================================================
    finline static int32 xs_ConvertToFixed (real64 val)
    {
    #if _xs_DEFAULT_CONVERSION==0
        return xs_CRoundToInt(val, _xs_doublemagic/(1<<N));
    #else
        return (long)((val)*(1<<N));
    #endif
    }
};





// ====================================================================================================================
// ====================================================================================================================
//  Inline implementation
// ====================================================================================================================
// ====================================================================================================================
finline static int32 xs_CRoundToInt(real64 val, real64 dmr)
{
#if _xs_DEFAULT_CONVERSION==0
	_xs_doubleints uval;
	uval.val = val + dmr;
	return uval.ival[_xs_iman_];
#else
    return int32(floor(val+.5));
#endif
}


// ====================================================================================================================
finline static int32 xs_ToInt(real64 val, real64 dme)
{
    /* unused - something else I tried...
            _xs_doublecopysgn(dme,val);
            return xs_CRoundToInt(val+dme);
            return 0;
    */

#if _MSC_VER >= 1400
	// VC++ 2005's standard cast is a little bit faster than this
	// magic number code. (Which is pretty amazing!) SSE has the
	// fastest C-style float->int conversion, but unfortunately,
	// checking for SSE support every time you need to do a
	// conversion completely negates its performance advantage.
	return int32(val);
#else
#if _xs_DEFAULT_CONVERSION==0
	return (val<0) ?   xs_CRoundToInt(val-dme) : 
					   xs_CRoundToInt(val+dme);
#else
    return int32(val);
#endif
#endif
}


// ====================================================================================================================
finline static int32 xs_FloorToInt(real64 val, real64 dme)
{
#if _xs_DEFAULT_CONVERSION==0
    return xs_CRoundToInt (val - dme);
#else
    return floor(val);
#endif
}


// ====================================================================================================================
finline static int32 xs_CeilToInt(real64 val, real64 dme)
{
#if _xs_DEFAULT_CONVERSION==0
    return xs_CRoundToInt (val + dme);
#else
    return ceil(val);
#endif
}


// ====================================================================================================================
finline static int32 xs_RoundToInt(real64 val)
{
#if _xs_DEFAULT_CONVERSION==0
	// Yes, it is important that two fadds be generated, so you cannot override the dmr
	// passed to xs_CRoundToInt with _xs_doublemagic + _xs_doublemagicdelta. If you do,
	// you'll end up with Banker's Rounding again.
    return xs_CRoundToInt (val + _xs_doublemagicdelta);
#else
    return floor(val+.5);
#endif
}



// ====================================================================================================================
// ====================================================================================================================
//  Unsigned variants
// ====================================================================================================================
// ====================================================================================================================
finline static uint32 xs_CRoundToUInt(real64 val)
{
	return (uint32)xs_CRoundToInt(val);
}

finline static uint32 xs_FloorToUInt(real64 val)
{
	return (uint32)xs_FloorToInt(val);
}

finline static uint32 xs_CeilToUInt(real64 val)
{
	return (uint32)xs_CeilToInt(val);
}

finline static uint32 xs_RoundToUInt(real64 val)
{
	return (uint32)xs_RoundToInt(val);
}



// ====================================================================================================================
// ====================================================================================================================
#endif // _xs_FLOAT_H_