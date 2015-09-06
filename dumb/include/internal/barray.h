#ifndef _B_ARRAY_H_
#define _B_ARRAY_H_

#include <stdlib.h>

#ifdef BARRAY_DECORATE
#define PASTE(a,b) a ## b
#define EVALUATE(a,b) PASTE(a,b)
#define bit_array_create EVALUATE(BARRAY_DECORATE,_bit_array_create)
#define bit_array_destroy EVALUATE(BARRAY_DECORATE,_bit_array_destroy)
#define bit_array_dup EVALUATE(BARRAY_DECORATE,_bit_array_dup)
#define bit_array_reset EVALUATE(BARRAY_DECORATE,_bit_array_reset)
#define bit_array_set EVALUATE(BARRAY_DECORATE,_bit_array_set)
#define bit_array_set_range EVALUATE(BARRAY_DECORATE,_bit_array_set_range)
#define bit_array_test EVALUATE(BARRAY_DECORATE,_bit_array_test)
#define bit_array_test_range EVALUATE(BARRAY_DECORATE,_bit_array_test_range)
#define bit_array_clear EVALUATE(BARRAY_DECORATE,_bit_array_clear)
#define bit_array_clear_range EVALUATE(BARRAY_DECORATE,_bit_array_clear_range)
#define bit_array_merge EVALUATE(BARRAY_DECORATE,_bit_array_merge)
#define bit_array_mask EVALUATE(BARRAY_DECORATE,_bit_array_mask)
#endif

void * bit_array_create(size_t size);
void bit_array_destroy(void * array);
void * bit_array_dup(void * array);

void bit_array_reset(void * array);

void bit_array_set(void * array, size_t bit);
void bit_array_set_range(void * array, size_t bit, size_t count);

int bit_array_test(void * array, size_t bit);
int bit_array_test_range(void * array, size_t bit, size_t count);

void bit_array_clear(void * array, size_t bit);
void bit_array_clear_range(void * array, size_t bit, size_t count);

void bit_array_merge(void * array, void * source, size_t offset);
void bit_array_mask(void * array, void * source, size_t offset);

#endif
