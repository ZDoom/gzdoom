#ifndef __M_ALLOC_H__
#define __M_ALLOC_H__
#include <stdlib.h>
#include <malloc.h>

// These are the same as the same stdlib functions,
// except they bomb out with an error requester
// when they can't get the memory.

void *Malloc (size_t size);
void *Calloc (size_t num, size_t size);
void *Realloc (void *memblock, size_t size);

#endif //__M_ALLOC_H__
