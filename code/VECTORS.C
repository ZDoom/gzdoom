#include "vectors.h"
#include "p_mobj.h"

// [RH] Convert a thing's position into a vec3_t
void VectorPosition (mobj_t *thing, vec3_t out)
{
	out[0] = (float)thing->x / 65536.0f;
	out[1] = (float)thing->y / 65536.0f;
	out[2] = (float)thing->z / 65536.0f;
}

// Taken from Q2
void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}

void VectorScale (vec3_t in, float scale, vec3_t out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

vec_t VectorLength(vec3_t v)
{
	int		i;
	float	length;
	
	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = (float) sqrt (length);		// FIXME

	return length;
}
