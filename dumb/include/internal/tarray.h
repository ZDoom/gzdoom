#ifndef _T_ARRAY_H_
#define _T_ARRAY_H_

#include <stdlib.h>

#ifndef LONG_LONG
#if defined __GNUC__ || defined __INTEL_COMPILER || defined __MWERKS__
#define LONG_LONG long long
#elif defined _MSC_VER || defined __WATCOMC__
#define LONG_LONG __int64
#elif defined __sgi
#define LONG_LONG long long
#else
#error 64-bit integer type unknown
#endif
#endif

void * timekeeping_array_create(size_t size);
void timekeeping_array_destroy(void * array);
void * timekeeping_array_dup(void * array);

void timekeeping_array_reset(void * array, size_t loop_start);

void timekeeping_array_push(void * array, size_t index, LONG_LONG time);
void timekeeping_array_bump(void * array, size_t index);

unsigned int timekeeping_array_get_count(void * array, size_t index);

LONG_LONG timekeeping_array_get_item(void * array, size_t index);

#endif
