
#if defined(USE_RAYQUERY)
layout(set = 1, binding = 0) uniform accelerationStructureEXT acc;
#else
struct CollisionNode
{
	vec3 center;
	float padding1;
	vec3 extents;
	float padding2;
	int left;
	int right;
	int element_index;
	int padding3;
};
layout(std430, set = 1, binding = 0) buffer NodeBuffer
{
	int nodesRoot;
	int nodebufferPadding1;
	int nodebufferPadding2;
	int nodebufferPadding3;
	CollisionNode nodes[];
};
layout(std430, set = 1, binding = 1) buffer VertexBuffer { vec4 vertices[]; };
layout(std430, set = 1, binding = 2) buffer ElementBuffer { int elements[]; };
#endif

layout(set = 0, binding = 0) uniform Uniforms
{
	vec3 SunDir;
	float Padding1;
	vec3 SunColor;
	float SunIntensity;
};

struct SurfaceInfo
{
	vec3 Normal;
	float Sky;
	float SamplingDistance;
	uint PortalIndex;
	float Padding1, Padding2;
};

struct PortalInfo
{
	mat4 Transformation;
};

struct LightInfo
{
	vec3 Origin;
	float Padding0;
	vec3 RelativeOrigin;
	float Padding1;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	vec3 SpotDir;
	float Padding2;
	vec3 Color;
	float Padding3;
};

layout(set = 0, binding = 1) buffer SurfaceIndexBuffer { uint surfaceIndices[]; };
layout(set = 0, binding = 2) buffer SurfaceBuffer { SurfaceInfo surfaces[]; };
layout(set = 0, binding = 3) buffer LightBuffer { LightInfo lights[]; };
layout(set = 0, binding = 4) buffer PortalBuffer { PortalInfo portals[]; };

layout(push_constant) uniform PushConstants
{
	uint LightStart;
	uint LightEnd;
	int SurfaceIndex;
	int PushPadding1;
	vec3 LightmapOrigin;
	float PushPadding2;
	vec3 LightmapStepX;
	float PushPadding3;
	vec3 LightmapStepY;
	float PushPadding4;
};

layout(location = 0) centroid in vec3 worldpos;
layout(location = 0) out vec4 fragcolor;

vec3 TraceSunLight(vec3 origin);
vec3 TraceLight(vec3 origin, vec3 normal, LightInfo light);
float TraceAmbientOcclusion(vec3 origin, vec3 normal);
vec2 Hammersley(uint i, uint N);
float RadicalInverse_VdC(uint bits);

bool TraceAnyHit(vec3 origin, float tmin, vec3 dir, float tmax);
bool TracePoint(vec3 origin, vec3 target, float tmin, vec3 dir, float tmax);
int TraceFirstHitTriangle(vec3 origin, float tmin, vec3 dir, float tmax);
int TraceFirstHitTriangleT(vec3 origin, float tmin, vec3 dir, float tmax, out float t);

void main()
{
	vec3 normal = surfaces[SurfaceIndex].Normal;
	vec3 origin = worldpos + normal * 0.1;

	vec3 incoming = TraceSunLight(origin);

	for (uint j = LightStart; j < LightEnd; j++)
	{
		incoming += TraceLight(origin, normal, lights[j]);
	}

#if defined(USE_RAYQUERY) // The non-rtx version of TraceFirstHitTriangle is too slow to do AO without the shader getting killed ;(
	incoming.rgb *= TraceAmbientOcclusion(origin, normal);
#endif

	fragcolor = vec4(incoming, 1.0);
}

vec3 TraceLight(vec3 origin, vec3 normal, LightInfo light)
{
	const float minDistance = 0.01;
	vec3 incoming = vec3(0.0);
	float dist = distance(light.RelativeOrigin, origin);
	if (dist > minDistance && dist < light.Radius)
	{
		vec3 dir = normalize(light.RelativeOrigin - origin);

		float distAttenuation = max(1.0 - (dist / light.Radius), 0.0);
		float angleAttenuation = 1.0f;
		if (SurfaceIndex >= 0)
		{
			angleAttenuation = max(dot(normal, dir), 0.0);
		}
		float spotAttenuation = 1.0;
		if (light.OuterAngleCos > -1.0)
		{
			float cosDir = dot(dir, light.SpotDir);
			spotAttenuation = smoothstep(light.OuterAngleCos, light.InnerAngleCos, cosDir);
			spotAttenuation = max(spotAttenuation, 0.0);
		}

		float attenuation = distAttenuation * angleAttenuation * spotAttenuation;
		if (attenuation > 0.0)
		{
			if(TracePoint(origin, light.Origin, minDistance, dir, dist))
			{
				incoming.rgb += light.Color * (attenuation * light.Intensity);
			}
		}
	}

	return incoming;
}

vec3 TraceSunLight(vec3 origin)
{
	const float minDistance = 0.01;
	vec3 incoming = vec3(0.0);
	const float dist = 32768.0;

	int primitiveID = TraceFirstHitTriangle(origin, minDistance, SunDir, dist);
	if (primitiveID != -1)
	{
		SurfaceInfo surface = surfaces[surfaceIndices[primitiveID]];
		incoming.rgb += SunColor * SunIntensity * surface.Sky;
	}
	return incoming;
}

float TraceAmbientOcclusion(vec3 origin, vec3 normal)
{
	const float minDistance = 0.05;
	const float aoDistance = 100;
	const int SampleCount = 2048;

	vec3 N = normal;
	vec3 up = abs(N.x) < abs(N.y) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	float ambience = 0.0f;
	for (uint i = 0; i < SampleCount; i++)
	{
		vec2 Xi = Hammersley(i, SampleCount);
		vec3 H = normalize(vec3(Xi.x * 2.0f - 1.0f, Xi.y * 2.0f - 1.0f, 1.5 - length(Xi)));
		vec3 L = H.x * tangent + H.y * bitangent + H.z * N;

		float hitDistance;
		int primitiveID = TraceFirstHitTriangleT(origin, minDistance, L, aoDistance, hitDistance);
		if (primitiveID != -1)
		{
			SurfaceInfo surface = surfaces[surfaceIndices[primitiveID]];
			if (surface.Sky == 0.0)
			{
				ambience += clamp(hitDistance / aoDistance, 0.0, 1.0);
			}
		}
		else
		{
			ambience += 1.0;
		}
	}
	return ambience / float(SampleCount);
}

vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

#if defined(USE_RAYQUERY)

int TraceFirstHitTriangleNoPortal(vec3 origin, float tmin, vec3 dir, float tmax, out float t)
{
	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, acc, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, tmin, dir, tmax);

	while(rayQueryProceedEXT(rayQuery))
	{
		if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCommittedIntersectionTriangleEXT)
		{
			rayQueryConfirmIntersectionEXT(rayQuery);
		}
	}

	if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
	{
		t = rayQueryGetIntersectionTEXT(rayQuery, true);
		return rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
	}
	else
	{
		t = tmax;
		return -1;
	}
}

/*
bool TraceAnyHit(vec3 origin, float tmin, vec3 dir, float tmax)
{
	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, acc, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, tmin, dir, tmax);
	while(rayQueryProceedEXT(rayQuery)) { }
	return rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT;
}
*/

#else

struct RayBBox
{
	vec3 start, end;
	vec3 c, w, v;
};

RayBBox create_ray(vec3 ray_start, vec3 ray_end)
{
	RayBBox ray;
	ray.start = ray_start;
	ray.end = ray_end;
	ray.c = (ray_start + ray_end) * 0.5;
	ray.w = ray_end - ray.c;
	ray.v = abs(ray.w);
	return ray;
}

bool overlap_bv_ray(RayBBox ray, int a)
{
	vec3 v = ray.v;
	vec3 w = ray.w;
	vec3 h = nodes[a].extents;
	vec3 c = ray.c - nodes[a].center;

	if (abs(c.x) > v.x + h.x ||
		abs(c.y) > v.y + h.y ||
		abs(c.z) > v.z + h.z)
	{
		return false;
	}

	if (abs(c.y * w.z - c.z * w.y) > h.y * v.z + h.z * v.y ||
		abs(c.x * w.z - c.z * w.x) > h.x * v.z + h.z * v.x ||
		abs(c.x * w.y - c.y * w.x) > h.x * v.y + h.y * v.x)
	{
		return false;
	}

	return true;
}

#define FLT_EPSILON 1.192092896e-07F // smallest such that 1.0+FLT_EPSILON != 1.0

float intersect_triangle_ray(RayBBox ray, int a, out float barycentricB, out float barycentricC)
{
	int start_element = nodes[a].element_index;

	vec3 p[3];
	p[0] = vertices[elements[start_element]].xyz;
	p[1] = vertices[elements[start_element + 1]].xyz;
	p[2] = vertices[elements[start_element + 2]].xyz;

	// Moeller-Trumbore ray-triangle intersection algorithm:

	vec3 D = ray.end - ray.start;

	// Find vectors for two edges sharing p[0]
	vec3 e1 = p[1] - p[0];
	vec3 e2 = p[2] - p[0];

	// Begin calculating determinant - also used to calculate u parameter
	vec3 P = cross(D, e2);
	float det = dot(e1, P);

	// Backface check
	//if (det < 0.0f)
	//	return 1.0f;

	// If determinant is near zero, ray lies in plane of triangle
	if (det > -FLT_EPSILON && det < FLT_EPSILON)
		return 1.0f;

	float inv_det = 1.0f / det;

	// Calculate distance from p[0] to ray origin
	vec3 T = ray.start - p[0];

	// Calculate u parameter and test bound
	float u = dot(T, P) * inv_det;

	// Check if the intersection lies outside of the triangle
	if (u < 0.f || u > 1.f)
		return 1.0f;

	// Prepare to test v parameter
	vec3 Q = cross(T, e1);

	// Calculate V parameter and test bound
	float v = dot(D, Q) * inv_det;

	// The intersection lies outside of the triangle
	if (v < 0.f || u + v  > 1.f)
		return 1.0f;

	float t = dot(e2, Q) * inv_det;
	if (t <= FLT_EPSILON)
		return 1.0f;

	// Return hit location on triangle in barycentric coordinates
	barycentricB = u;
	barycentricC = v;
	
	return t;
}

bool is_leaf(int node_index)
{
	return nodes[node_index].element_index != -1;
}

/*
bool TraceAnyHit(vec3 origin, float tmin, vec3 dir, float tmax)
{
	if (tmax <= 0.0f)
		return false;

	RayBBox ray = create_ray(origin, origin + dir * tmax);
	tmin /= tmax;

	int stack[64];
	int stackIndex = 0;
	stack[stackIndex++] = nodesRoot;
	do
	{
		int a = stack[--stackIndex];
		if (overlap_bv_ray(ray, a))
		{
			if (is_leaf(a))
			{
				float baryB, baryC;
				float t = intersect_triangle_ray(ray, a, baryB, baryC);
				if (t >= tmin && t < 1.0)
				{
					return true;
				}
			}
			else
			{
				stack[stackIndex++] = nodes[a].right;
				stack[stackIndex++] = nodes[a].left;
			}
		}
	} while (stackIndex > 0);
	return false;
}
*/

struct TraceHit
{
	float fraction;
	int triangle;
	float b;
	float c;
};

TraceHit find_first_hit(RayBBox ray)
{
	TraceHit hit;
	hit.fraction = 1.0;
	hit.triangle = -1;
	hit.b = 0.0;
	hit.c = 0.0;

	int stack[64];
	int stackIndex = 0;
	stack[stackIndex++] = nodesRoot;
	do
	{
		int a = stack[--stackIndex];
		if (overlap_bv_ray(ray, a))
		{
			if (is_leaf(a))
			{
				float baryB, baryC;
				float t = intersect_triangle_ray(ray, a, baryB, baryC);
				if (t < hit.fraction)
				{
					hit.fraction = t;
					hit.triangle = nodes[a].element_index / 3;
					hit.b = baryB;
					hit.c = baryC;
				}
			}
			else
			{
				stack[stackIndex++] = nodes[a].right;
				stack[stackIndex++] = nodes[a].left;
			}
		}
	} while (stackIndex > 0);
	return hit;
}

int TraceFirstHitTriangleNoPortal(vec3 origin, float tmin, vec3 dir, float tmax, out float tparam)
{
	// Perform segmented tracing to keep the ray AABB box smaller
	vec3 ray_start = origin;
	vec3 ray_end = origin + dir * tmax;
	vec3 ray_dir = dir;
	float tracedist = tmax;
	float segmentlen = max(200.0, tracedist / 20.0);
	for (float t = 0.0; t < tracedist; t += segmentlen)
	{
		float segstart = t;
		float segend = min(t + segmentlen, tracedist);

		RayBBox ray = create_ray(ray_start + ray_dir * segstart, ray_start + ray_dir * segend);
		TraceHit hit = find_first_hit(ray);
		if (hit.fraction < 1.0)
		{
			tparam = hit.fraction = segstart * (1.0 - hit.fraction) + segend * hit.fraction;
			return hit.triangle;
		}
	}

	tparam = tracedist;
	return -1;
}


#endif

int TraceFirstHitTriangleT(vec3 origin, float tmin, vec3 dir, float tmax, out float t)
{
	int primitiveID;
	while(true)
	{
		primitiveID = TraceFirstHitTriangleNoPortal(origin, tmin, dir, tmax, t);

		if(primitiveID < 0)
		{
			break;
		}

		SurfaceInfo surface = surfaces[surfaceIndices[primitiveID]];

		if(surface.PortalIndex == 0)
		{
			break;
		}

		// Portal was hit: Apply transformation onto the ray
		mat4 transformationMatrix = portals[surface.PortalIndex].Transformation;

		origin = (transformationMatrix * vec4(origin + dir * t, 1.0)).xyz;
		dir = (transformationMatrix * vec4(dir, 0.0)).xyz;
		tmax -= t;
	}
	return primitiveID;
}

int TraceFirstHitTriangle(vec3 origin, float tmin, vec3 dir, float tmax)
{
	float t;
	return TraceFirstHitTriangleT(origin, tmin, dir, tmax, t);
}

bool TraceAnyHit(vec3 origin, float tmin, vec3 dir, float tmax)
{
	return TraceFirstHitTriangle(origin, tmin, dir, tmax) >= 0;
}

bool TracePoint(vec3 origin, vec3 target, float tmin, vec3 dir, float tmax)
{
	int primitiveID;
	float t;
	while(true)
	{
		t = tmax;
		primitiveID = TraceFirstHitTriangleNoPortal(origin, tmin, dir, tmax, t);

		origin += dir * t;
		tmax -= t;

		if(primitiveID < 0)
		{
			// We didn't hit anything
			break;
		}

		SurfaceInfo surface = surfaces[surfaceIndices[primitiveID]];

		if(surface.PortalIndex == 0)
		{
			break;
		}

		if(dot(surface.Normal, dir) >= 0.0)
		{
			continue;
		}

		mat4 transformationMatrix = portals[surface.PortalIndex].Transformation;
		origin = (transformationMatrix * vec4(origin, 1.0)).xyz;
		dir = (transformationMatrix * vec4(dir, 0.0)).xyz;

#if defined(USE_RAYQUERY)
#else
		origin += dir * tmin;
		tmax -= tmin;
#endif
	}

	return distance(origin, target) <= 1.0;
}
