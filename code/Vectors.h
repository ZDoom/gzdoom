// Vector math routines which I took from Quake2's source since they
// make more sense than the way Doom does things. :-)

#ifndef __VECTORS_H__
#define __VECTORS_H__

#include <math.h>
#include "tables.h"

typedef float	vec_t;
typedef vec_t	vec3_t[3];

#define FIXED2FLOAT(f)			((float)(f) / (float)FRACUNIT)
#define FLOAT2FIXED(f)			(fixed_t)((f) * (float)FRACUNIT)

#define DotProduct(x,y)			(x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c)	(c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c)		(c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorCopy(a,b)			(b[0]=a[0],b[1]=a[1],b[2]=a[2])
#define VectorClear(a)			(a[0]=a[1]=a[2]=0)
#define VectorNegate(a,b)		(b[0]=-a[0],b[1]=-a[1],b[2]=-a[2])
#define VectorSet(v, x, y, z)	(v[0]=(x), v[1]=(y), v[2]=(z))
#define VectorFixedSet(v,x,y,z)	(v[0]=FIXED2FLOAT(x), v[1]=FIXED2FLOAT(y), v[2]=FIXED2FLOAT(z))
#define VectorInverse(v)		(v[0]=-v[0],v[1]=-v[1],v[2]=-v[2])

struct mobj_s;

void VectorPosition (const struct mobj_s *thing, vec3_t out);
void FixedAngleToVector (angle_t an, fixed_t pitch, vec3_t v);
vec_t VectorLength (const vec3_t v);
void VectorMA (const vec3_t a, float scale, const vec3_t b, vec3_t out);
void VectorScale (const vec3_t v, float scale, vec3_t out);
void VectorScale2 (vec3_t v, float scale);
int VectorCompare (const vec3_t v1, const vec3_t v2);
vec_t VectorNormalize (vec3_t v);
vec_t VectorNormalize2 (const vec3_t v, vec3_t out);
void CrossProduct (const vec3_t v1, const vec3_t v2, vec3_t cross);
void ProjectPointOnPlane (vec3_t dst, const vec3_t p, const vec3_t normal);
void PerpendicularVector (vec3_t dst, const vec3_t src);
void RotatePointAroundVector (vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);
void R_ConcatRotations (const float in1[3][3], const float in2[3][3], float out[3][3]);

#endif //__VECTORS_H__