
in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

struct GPUNode
{
	vec2 aabb_min;
	vec2 aabb_max;
	int left;
	int right;
	int line_index;
	int padding;
};

struct GPULine
{
	vec2 pos;
	vec2 delta;
};

layout(std430, binding = 2) buffer LightNodes
{
	GPUNode nodes[];
};

layout(std430, binding = 3) buffer LightLines
{
	GPULine lines[];
};

layout(std430, binding = 4) buffer LightList
{
	vec4 lights[];
};

bool overlapRayAABB(vec2 ray_start2d, vec2 ray_end2d, vec2 aabb_min2d, vec2 aabb_max2d)
{
	// To do: simplify test to use a 2D test
	vec3 ray_start = vec3(ray_start2d, 0.0);
	vec3 ray_end = vec3(ray_end2d, 0.0);
	vec3 aabb_min = vec3(aabb_min2d, -1.0);
	vec3 aabb_max = vec3(aabb_max2d, 1.0);

	vec3 c = (ray_start + ray_end) * 0.5f;
	vec3 w = ray_end - c;
	vec3 h = (aabb_max - aabb_min) * 0.5f; // aabb.extents();

	c -= (aabb_max + aabb_min) * 0.5f; // aabb.center();

	vec3 v = abs(w);

	if (abs(c.x) > v.x + h.x || abs(c.y) > v.y + h.y || abs(c.z) > v.z + h.z)
		return false; // disjoint;

	if (abs(c.y * w.z - c.z * w.y) > h.y * v.z + h.z * v.y ||
		abs(c.x * w.z - c.z * w.x) > h.x * v.z + h.z * v.x ||
		abs(c.x * w.y - c.y * w.x) > h.x * v.y + h.y * v.x)
		return false; // disjoint;

	return true; // overlap;
}

float intersectRayLine(vec2 ray_start, vec2 ray_end, int line_index, vec2 raydelta, float rayd, float raydist2)
{
	const float epsilon = 0.0000001;
	GPULine line = lines[line_index];

	vec2 raynormal = vec2(raydelta.y, -raydelta.x);

	float den = dot(raynormal, line.delta);
	if (abs(den) > epsilon)
	{
		float t_line = (rayd - dot(raynormal, line.pos)) / den;
		if (t_line >= 0.0 && t_line <= 1.0)
		{
			vec2 linehitdelta = line.pos + line.delta * t_line - ray_start;
			float t = dot(raydelta, linehitdelta) / raydist2;
			return t > 0.0 ? t : 1.0;
		}
	}

	return 1.0;
}

bool isLeaf(int node_index)
{
	return nodes[node_index].line_index != -1;
}

float rayTest(vec2 ray_start, vec2 ray_end)
{
	vec2 raydelta = ray_end - ray_start;
	float raydist2 = dot(raydelta, raydelta);
	vec2 raynormal = vec2(raydelta.y, -raydelta.x);
	float rayd = dot(raynormal, ray_start);
	if (raydist2 < 1.0)
		return 1.0;

	float t = 1.0;

	int stack[16];
	int stack_pos = 1;
	stack[0] = nodes.length() - 1;
	while (stack_pos > 0)
	{
		int node_index = stack[stack_pos - 1];

		if (!overlapRayAABB(ray_start, ray_end, nodes[node_index].aabb_min, nodes[node_index].aabb_max))
		{
			stack_pos--;
		}
		else if (isLeaf(node_index))
		{
			t = min(intersectRayLine(ray_start, ray_end, nodes[node_index].line_index, raydelta, rayd, raydist2), t);
			stack_pos--;
		}
		else if (stack_pos == 16)
		{
			stack_pos--; // stack overflow
		}
		else
		{
			stack[stack_pos - 1] = nodes[node_index].left;
			stack[stack_pos] = nodes[node_index].right;
			stack_pos++;
		}
	}

	return t;
}

void main()
{
	int lightIndex = int(gl_FragCoord.y);

	vec4 light = lights[lightIndex];
	float radius = light.w;
	vec2 lightpos = light.xy;

	if (radius > 0.0)
	{
		vec2 pixelpos;
		switch (int(gl_FragCoord.x) / int(ShadowmapQuality/4.0))
		{
		case 0: pixelpos = vec2((gl_FragCoord.x - float(ShadowmapQuality/8.0)) / float(ShadowmapQuality/8.0), 1.0); break;
		case 1: pixelpos = vec2(1.0, (gl_FragCoord.x - float(ShadowmapQuality/4.0 + ShadowmapQuality/8.0)) / float(ShadowmapQuality/8.0)); break;
		case 2: pixelpos = vec2(-(gl_FragCoord.x - float(ShadowmapQuality/2.0 + ShadowmapQuality/8.0)) / float(ShadowmapQuality/8.0), -1.0); break;
		case 3: pixelpos = vec2(-1.0, -(gl_FragCoord.x - float(ShadowmapQuality*3.0/4.0 + ShadowmapQuality/8.0)) / float(ShadowmapQuality/8.0)); break;
		}
		pixelpos = lightpos + pixelpos * radius;

		float t = rayTest(lightpos, pixelpos);
		vec2 delta = (pixelpos - lightpos) * t;
		float dist2 = dot(delta, delta);

		FragColor = vec4(dist2, 0.0, 0.0, 1.0);
	}
	else
	{
		FragColor = vec4(1.0, 0.0, 0.0, 1.0);
	}
}
