#ifndef RIFF_H
#define RIFF_H

struct riff;

struct riff_chunk
{
	unsigned type;
    int32 offset;
	unsigned size;
    struct riff * nested;
};

struct riff
{
	unsigned type;
	unsigned chunk_count;
	struct riff_chunk * chunks;
};

struct riff * riff_parse( DUMBFILE * f, int32 offset, int32 size, unsigned proper );
void riff_free( struct riff * );

#endif
