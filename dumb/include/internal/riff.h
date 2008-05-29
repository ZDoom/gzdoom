#ifndef RIFF_H
#define RIFF_H

struct riff_chunk
{
	unsigned type;
	void * data;
	unsigned size;
};

struct riff
{
	unsigned type;
	unsigned chunk_count;
	struct riff_chunk * chunks;
};

struct riff * riff_parse( unsigned char *, unsigned size, unsigned proper );
void riff_free( struct riff * );

#endif
