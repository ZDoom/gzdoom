#ifndef _EMUTYPES_H_
#define _EMUTYPES_H_

#ifndef INLINE

#if defined(_MSC_VER)
//#define INLINE __forceinline
#define INLINE __inline
#elif defined(__GNUC__)
#define INLINE __inline__
#elif defined(_MWERKS_)
#define INLINE inline
#else
#define INLINE
#endif

#endif

typedef unsigned int e_uint;
typedef signed int e_int;

typedef unsigned char e_uint8 ;
typedef signed char e_int8 ;

typedef unsigned short e_uint16 ;
typedef signed short e_int16 ;

typedef unsigned int e_uint32 ;
typedef signed int e_int32 ;

#endif
