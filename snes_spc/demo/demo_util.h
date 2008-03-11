/* General-purpose utilities used by demos */

/* snes_spc 0.9.0 */
#ifndef DEMO_UTIL_H
#define DEMO_UTIL_H

/* commonly used headers */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
	extern "C" {
#endif

/* If str is not NULL, prints it and exits program, otherwise returns */
void error( const char* str );

/* Loads file and returns pointer to data in memory, allocated with malloc().
If size_out != NULL, sets *size_out to size of data. */
unsigned char* load_file( const char* path, long* size_out );

/* Writes data to file */
void write_file( const char* path, void const* in, long size );

#ifdef __cplusplus
	}
#endif

#endif
