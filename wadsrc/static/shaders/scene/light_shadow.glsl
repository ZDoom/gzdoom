
// Check if light is in shadow

#if defined(USE_RAYTRACE)

#if defined(SUPPORTS_RAYQUERY)

bool traceHit(vec3 origin, vec3 direction, float dist)
{
	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, TopLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, 0.01f, direction, dist);
	while(rayQueryProceedEXT(rayQuery)) { }
	return rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT;
}

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

bool traceHit(vec3 origin, vec3 direction, float dist)
{
	return TraceAnyHit(origin, 0.01f, direction, dist);
}

#endif

vec2 softshadow[9 * 3] = vec2[](
	vec2( 0.0, 0.0),
	vec2(-2.0,-2.0),
	vec2( 2.0, 2.0),
	vec2( 2.0,-2.0),
	vec2(-2.0, 2.0),
	vec2(-1.0,-1.0),
	vec2( 1.0, 1.0),
	vec2( 1.0,-1.0),
	vec2(-1.0, 1.0),

	vec2( 0.0, 0.0),
	vec2(-1.5,-1.5),
	vec2( 1.5, 1.5),
	vec2( 1.5,-1.5),
	vec2(-1.5, 1.5),
	vec2(-0.5,-0.5),
	vec2( 0.5, 0.5),
	vec2( 0.5,-0.5),
	vec2(-0.5, 0.5),

	vec2( 0.0, 0.0),
	vec2(-1.25,-1.75),
	vec2( 1.75, 1.25),
	vec2( 1.25,-1.75),
	vec2(-1.75, 1.75),
	vec2(-0.75,-0.25),
	vec2( 0.25, 0.75),
	vec2( 0.75,-0.25),
	vec2(-0.25, 0.75)
);

float traceShadow(vec4 lightpos, int quality)
{
	vec3 origin = pixelpos.xzy;
	vec3 target = lightpos.xzy + 0.01; // nudge light position slightly as Doom maps tend to have their lights perfectly aligned with planes

	vec3 direction = normalize(target - origin);
	float dist = distance(origin, target);

	if (quality == 0)
	{
		return traceHit(origin, direction, dist) ? 0.0 : 1.0;
	}
	else
	{
		vec3 v = (abs(direction.x) > abs(direction.y)) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
		vec3 xdir = normalize(cross(direction, v));
		vec3 ydir = cross(direction, xdir);

		float sum = 0.0;
		int step_count = quality * 9;
		for (int i = 0; i <= step_count; i++)
		{
			vec3 pos = target + xdir * softshadow[i].x + ydir * softshadow[i].y;
			sum += traceHit(origin, normalize(pos - origin), dist) ? 0.0 : 1.0;
		}
		return sum / step_count;
	}
}

float shadowAttenuation(vec4 lightpos, float lightcolorA)
{
	if (lightpos.w > 1000000.0)
		return 1.0; // Sunlight
	return traceShadow(lightpos, uShadowmapFilter);
}

#elif defined(USE_SHADOWMAP)

float shadowDirToU(vec2 dir)
{
	if (abs(dir.y) > abs(dir.x))
	{
		float x = dir.x / dir.y * 0.125;
		if (dir.y >= 0.0)
			return 0.125 + x;
		else
			return (0.50 + 0.125) + x;
	}
	else
	{
		float y = dir.y / dir.x * 0.125;
		if (dir.x >= 0.0)
			return (0.25 + 0.125) - y;
		else
			return (0.75 + 0.125) - y;
	}
}

vec2 shadowUToDir(float u)
{
	u *= 4.0;
	vec2 raydir;
	switch (int(u))
	{
	case 0: raydir = vec2(u * 2.0 - 1.0, 1.0); break;
	case 1: raydir = vec2(1.0, 1.0 - (u - 1.0) * 2.0); break;
	case 2: raydir = vec2(1.0 - (u - 2.0) * 2.0, -1.0); break;
	case 3: raydir = vec2(-1.0, (u - 3.0) * 2.0 - 1.0); break;
	}
	return raydir;
}

float sampleShadowmap(vec3 planePoint, float v)
{
	float bias = 1.0;
	float negD = dot(vWorldNormal.xyz, planePoint);

	vec3 ray = planePoint;

	vec2 isize = textureSize(ShadowMap, 0);
	float scale = float(isize.x) * 0.25;

	// Snap to shadow map texel grid
	if (abs(ray.z) > abs(ray.x))
	{
		ray.y = ray.y / abs(ray.z);
		ray.x = ray.x / abs(ray.z);
		ray.x = (floor((ray.x + 1.0) * 0.5 * scale) + 0.5) / scale * 2.0 - 1.0;
		ray.z = sign(ray.z);
	}
	else
	{
		ray.y = ray.y / abs(ray.x);
		ray.z = ray.z / abs(ray.x);
		ray.z = (floor((ray.z + 1.0) * 0.5 * scale) + 0.5) / scale * 2.0 - 1.0;
		ray.x = sign(ray.x);
	}

	float t = negD / dot(vWorldNormal.xyz, ray) - bias;
	vec2 dir = ray.xz * t;

	float u = shadowDirToU(dir);
	float dist2 = dot(dir, dir);
	return step(dist2, texture(ShadowMap, vec2(u, v)).x);
}

float sampleShadowmapPCF(vec3 planePoint, float v)
{
	float bias = 1.0;
	float negD = dot(vWorldNormal.xyz, planePoint);

	vec3 ray = planePoint;

	if (abs(ray.z) > abs(ray.x))
		ray.y = ray.y / abs(ray.z);
	else
		ray.y = ray.y / abs(ray.x);

	vec2 isize = textureSize(ShadowMap, 0);
	float scale = float(isize.x);
	float texelPos = floor(shadowDirToU(ray.xz) * scale);

	float sum = 0.0;
	float step_count = uShadowmapFilter;

	texelPos -= step_count + 0.5;
	for (float x = -step_count; x <= step_count; x++)
	{
		float u = fract(texelPos / scale);
		vec2 dir = shadowUToDir(u);

		ray.x = dir.x;
		ray.z = dir.y;
		float t = negD / dot(vWorldNormal.xyz, ray) - bias;
		dir = ray.xz * t;

		float dist2 = dot(dir, dir);
		sum += step(dist2, texture(ShadowMap, vec2(u, v)).x);
		texelPos++;
	}
	return sum / (uShadowmapFilter * 2.0 + 1.0);
}

float shadowmapAttenuation(vec4 lightpos, float shadowIndex)
{
	vec3 planePoint = pixelpos.xyz - lightpos.xyz;
	planePoint += 0.01; // nudge light position slightly as Doom maps tend to have their lights perfectly aligned with planes

	if (dot(planePoint.xz, planePoint.xz) < 1.0)
		return 1.0; // Light is too close

	float v = (shadowIndex + 0.5) / 1024.0;

	if (uShadowmapFilter == 0)
	{
		return sampleShadowmap(planePoint, v);
	}
	else
	{
		return sampleShadowmapPCF(planePoint, v);
	}
}

float shadowAttenuation(vec4 lightpos, float lightcolorA)
{
	if (lightpos.w > 1000000.0)
		return 1.0; // Sunlight

	float shadowIndex = abs(lightcolorA) - 1.0;
	if (shadowIndex >= 1024.0)
		return 1.0; // No shadowmap available for this light

	return shadowmapAttenuation(lightpos, shadowIndex);
}

#else

float shadowAttenuation(vec4 lightpos, float lightcolorA)
{
	return 1.0;
}

#endif
