// Vector math routines which I took from Quake2's source since they
// make more sense than the way Doom does things. :-)

#include <math.h>

typedef float	vec_t;
typedef vec_t	vec3_t[3];

#define DotProduct(x,y)			(x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c)	(c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c)		(c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorCopy(a,b)			(b[0]=a[0],b[1]=a[1],b[2]=a[2])
#define VectorClear(a)			(a[0]=a[1]=a[2]=0)
#define VectorNegate(a,b)		(b[0]=-a[0],b[1]=-a[1],b[2]=-a[2])
#define VectorSet(v, x, y, z)	(v[0]=(x), v[1]=(y), v[2]=(z))

struct mobj_s;

void VectorPosition (struct mobj_s *thing, vec3_t out);
void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);
void VectorScale (vec3_t in, float scale, vec3_t out);
vec_t VectorLength (vec3_t v);