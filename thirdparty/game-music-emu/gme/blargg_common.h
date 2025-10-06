// Sets up common environment for Shay Green's libraries.
// To change configuration options, modify blargg_config.h, not this file.

#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

#include "blargg_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

// BLARGG_RESTRICT: equivalent to restrict, where supported
#if (defined(__GNUC__) && (__GNUC__ >= 3)) || \
    (defined(_MSC_VER) && (_MSC_VER >= 1100))
	#define BLARGG_RESTRICT __restrict
#else
	#define BLARGG_RESTRICT
#endif

// STATIC_CAST(T,expr): Used in place of static_cast<T> (expr)
#ifndef STATIC_CAST
	#define STATIC_CAST(T,expr) ((T) (expr))
#endif

#if !defined(_MSC_VER) || _MSC_VER >= 1910
	#define blaarg_static_assert(cond, msg) static_assert(cond, msg)
#else
	#define blaarg_static_assert(cond, msg) assert(cond)
#endif

// blargg_err_t (0 on success, otherwise error string)
#ifndef blargg_err_t
	typedef const char* blargg_err_t;
#endif

// Apply minus sign to unsigned type and prevent the warning being shown
template<typename T>
inline T uMinus(T in)
{
	return ~(in - 1);
}

// blargg_vector - very lightweight vector of POD types (no constructor/destructor)
template<class T>
class blargg_vector {
	T* begin_;
	size_t size_;
public:
	blargg_vector() : begin_( 0 ), size_( 0 ) { }
	~blargg_vector() { free( begin_ ); }
	size_t size() const { return size_; }
	T* begin() const { return begin_; }
	T* end() const { return begin_ + size_; }
	blargg_err_t resize( size_t n )
	{
		void* p = realloc( begin_, n * sizeof (T) );
		if ( !p && n )
			return "Out of memory";
		begin_ = (T*) p;
		size_ = n;
		return 0;
	}
	void clear() { free( begin_ ); begin_ = nullptr; size_ = 0; }
	T& operator [] ( size_t n ) const
	{
		assert( n <= size_ ); // <= to allow past-the-end value
		return begin_ [n];
	}
};

// Use to force disable exceptions for allocations of a class
#include <new>
#ifndef BLARGG_DISABLE_NOTHROW
	#define BLARGG_DISABLE_NOTHROW \
		void* operator new ( size_t s ) noexcept { return malloc( s ); }\
		void* operator new ( size_t s, const std::nothrow_t& ) noexcept { return malloc( s ); }\
		void operator delete ( void* p ) noexcept { free( p ); }\
		void operator delete ( void* p, const std::nothrow_t&) noexcept { free( p ); }
#endif

// Use to force disable exceptions for a specific allocation no matter what class
#define BLARGG_NEW new (std::nothrow)

// BLARGG_4CHAR('a','b','c','d') = 'abcd' (four character integer constant)
#define BLARGG_4CHAR( a, b, c, d ) \
	((a&0xFF)*0x1000000L + (b&0xFF)*0x10000L + (c&0xFF)*0x100L + (d&0xFF))

#define BLARGG_2CHAR( a, b ) \
	((a&0xFF)*0x100L + (b&0xFF))

// blargg_long/blargg_ulong = at least 32 bits, int if it's big enough

#if INT_MAX < 0x7FFFFFFF || LONG_MAX == 0x7FFFFFFF
	typedef long blargg_long;
#else
	typedef int blargg_long;
#endif

#if UINT_MAX < 0xFFFFFFFF || ULONG_MAX == 0xFFFFFFFF
	typedef unsigned long blargg_ulong;
#else
	typedef unsigned blargg_ulong;
#endif

// int8_t etc.
#include <stdint.h>

#endif
