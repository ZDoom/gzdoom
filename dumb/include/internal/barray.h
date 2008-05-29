#ifndef _B_ARRAY_H_
#define _B_ARRAY_H_

#include <stdlib.h>

void * bit_array_create(size_t size);
void bit_array_destroy(void * array);
void * bit_array_dup(void * array);

void bit_array_reset(void * array);

void bit_array_set(void * array, size_t bit);
int bit_array_test(void * array, size_t bit);
int bit_array_test_range(void * array, size_t bit, size_t count);
void bit_array_clear(void * array, size_t bit);

void bit_array_merge(void * array, void * source, size_t offset);
void bit_array_mask(void * array, void * source, size_t offset);

#endif
