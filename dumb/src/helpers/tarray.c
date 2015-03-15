#include "internal/tarray.h"

#include <string.h>

	/*
	   Structures which contain the play times of each pattern and row combination in the song,
	   not guaranteed to be valid for the whole song until the loop status is no longer zero.
	   The initial count and restart count will both be zero on song start, then both will be
	   incremented until the song loops. Restart count will be reset to zero on loop for all
	   rows which have a time equal to or greater than the loop start point, so time keeping
	   functions will know which timestamp the song is currently located at.

	   Timestamp lists are guaranteed to be allocated in blocks of 16 timestamps at a time.
	*/

	/*
	   We don't need full timekeeping because the player loop only wants the first play time
	   of the loop start order/row. We also don't really want full timekeeping because it
	   involves a lot of memory allocations, which is also slow.
	*/

#undef FULL_TIMEKEEPING

typedef struct DUMB_IT_ROW_TIME
{
	unsigned int count, restart_count;
#ifndef FULL_TIMEKEEPING
	LONG_LONG first_time;
#else
	LONG_LONG * times;
#endif
} DUMB_IT_ROW_TIME;

void * timekeeping_array_create(size_t size)
{
	size_t * _size = (size_t *) calloc( 1, sizeof(size_t) + sizeof(DUMB_IT_ROW_TIME) * size );
	if ( _size ) {
		*_size = size;
	}
	return _size;
}

void timekeeping_array_destroy(void * array)
{
#ifdef FULL_TIMEKEEPING
	size_t i;
	size_t * size = (size_t *) array;
	DUMB_IT_ROW_TIME * s = (DUMB_IT_ROW_TIME *)(size + 1);

	for (i = 0; i < *size; i++) {
		if (s[i].times) free(s[i].times);
	}
#endif

    free(array);
}

void * timekeeping_array_dup(void * array)
{
	size_t i;
	size_t * size = (size_t *) array;
	DUMB_IT_ROW_TIME * s = (DUMB_IT_ROW_TIME *)(size + 1);
	size_t * new_size = (size_t *) calloc( 1, sizeof(size_t) + sizeof(DUMB_IT_ROW_TIME) * *size );
	if ( new_size ) {
		DUMB_IT_ROW_TIME * new_s = (DUMB_IT_ROW_TIME *)(new_size + 1);

		*new_size = *size;

		for (i = 0; i < *size; i++) {
			new_s[i].count = s[i].count;
			new_s[i].restart_count = s[i].restart_count;

#ifndef FULL_TIMEKEEPING
			new_s[i].first_time = s[i].first_time;
#else
			if ( s[i].times ) {
				size_t time_count = ( s[i].count + 15 ) & ~15;
				new_s[i].times = (LONG_LONG *) malloc( sizeof(LONG_LONG) * time_count );
				if ( new_s[i].times == (void *)0 ) {
					timekeeping_array_destroy( new_size );
					return (void *) 0;
				}
				memcpy( new_s[i].times, s[i].times, sizeof(LONG_LONG) * s[i].count );
			}
#endif
		}
	}

	return new_size;
}

void timekeeping_array_reset(void * array, size_t loop_start)
{
	size_t i;
	size_t * size = (size_t *) array;
	DUMB_IT_ROW_TIME * s = (DUMB_IT_ROW_TIME *)(size + 1);

	DUMB_IT_ROW_TIME * s_loop_start = s + loop_start;
	LONG_LONG loop_start_time;

	if ( loop_start >= *size || s_loop_start->count < 1 ) return;

#ifndef FULL_TIMEKEEPING
	loop_start_time = s_loop_start->first_time;
#else
	loop_start_time = s_loop_start->times[0];
#endif

	for ( i = 0; i < *size; i++ ) {
#ifndef FULL_TIMEKEEPING
		if ( s[i].count && s[i].first_time >= loop_start_time ) {
#else
		if ( s[i].count && s[i].times[0] >= loop_start_time ) {
#endif
			s[i].restart_count = 0;
		}
	}
}

void timekeeping_array_push(void * array, size_t index, LONG_LONG time)
{
#ifdef FULL_TIMEKEEPING
	size_t i;
    size_t time_count;
#endif
    size_t * size = (size_t *) array;
	DUMB_IT_ROW_TIME * s = (DUMB_IT_ROW_TIME *)(size + 1);

	if (index >= *size) return;

#ifndef FULL_TIMEKEEPING
	if ( !s[index].count++ )
		s[index].first_time = time;
#else
	time_count = ( s[index].count + 16 ) & ~15;

	s[index].times = (LONG_LONG *) realloc( s[index].times, sizeof(LONG_LONG) * time_count );

	s[index].times[s[index].count++] = time;
#endif
}

void timekeeping_array_bump(void * array, size_t index)
{
	size_t * size = (size_t *) array;
	DUMB_IT_ROW_TIME * s = (DUMB_IT_ROW_TIME *)(size + 1);

	if (index >= *size) return;

	s[index].restart_count++;
}

unsigned int timekeeping_array_get_count(void * array, size_t index)
{
	size_t * size = (size_t *) array;
	DUMB_IT_ROW_TIME * s = (DUMB_IT_ROW_TIME *)(size + 1);

	if (index >= *size) return 0;

	return s[index].count;
}

LONG_LONG timekeeping_array_get_item(void * array, size_t index)
{
	size_t * size = (size_t *) array;
	DUMB_IT_ROW_TIME * s = (DUMB_IT_ROW_TIME *)(size + 1);

	if (index >= *size || s[index].restart_count >= s[index].count) return 0;

#ifndef FULL_TIMEKEEPING
	return s[index].first_time;
#else
	return s[index].times[s[index].restart_count];
#endif
}
