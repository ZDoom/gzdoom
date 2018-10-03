
in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

// A node in an AABB binary tree with lines stored in the leaf nodes
struct GPUNode
{
	vec2 aabb_min;  // Min xy values for the axis-aligned box containing the node and its subtree
	vec2 aabb_max;  // Max xy values
	int left;       // Left subnode index
	int right;      // Right subnode index
	int line_index; // Line index if it is a leaf node, otherwise -1
	int padding;    // Unused - maintains 16 byte alignment
};

// 2D line segment, referenced by leaf nodes
struct GPULine
{
	vec2 pos;       // Line start position
	vec2 delta;     // Line end position - line start position
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

// Overlap test between line segment and axis-aligned bounding box. Returns true if they overlap.
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

// Intersection test between two line segments.
// Returns the intersection point as a value between 0-1 on the ray line segment. 1.0 if there was no hit.
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

// Returns true if an AABB tree node is a leaf node. Leaf nodes contains a line.
bool isLeaf(int node_index)
{
	return nodes[node_index].line_index != -1;
}

// Perform ray intersection test between the ray line segment and all the lines in the AABB binary tree.
// Returns the intersection point as a value between 0-1 on the ray line segment. 1.0 if there was no hit.
float rayTest(vec2 ray_start, vec2 ray_end)
{
	vec2 raydelta = ray_end - ray_start;
	float raydist2 = dot(raydelta, raydelta);
	vec2 raynormal = vec2(raydelta.y, -raydelta.x);
	float rayd = dot(raynormal, ray_start);
	if (raydist2 < 1.0)
		return 1.0;

	float t = 1.0;

	// Walk the AABB binary tree searching for nodes touching the ray line segment's AABB box.
	// When it reaches a leaf node, use a line segment intersection test to see if we got a hit.

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
	// Find the light that belongs to this texel in the shadowmap texture we output to:

	int lightIndex = int(gl_FragCoord.y);

	vec4 light = lights[lightIndex];
	float radius = light.w;
	vec2 lightpos = light.xy;

	if (radius > 0.0)
	{
		// We found an active light. Calculate the ray direction for the texel.
		//
		// The texels are laid out so that there are four projections:
		//
		// * top-left to top-right
		// * top-right to bottom-right
		// * bottom-right to bottom-left
		// * bottom-left to top-left
		//
		vec2 raydir;
		float u = gl_FragCoord.x / ShadowmapQuality * 4.0;
		switch (int(u))
		{
		case 0: raydir = vec2(u * 2.0 - 1.0, 1.0); break;
		case 1: raydir = vec2(1.0, 1.0 - (u - 1.0) * 2.0); break;
		case 2: raydir = vec2(1.0 - (u - 2.0) * 2.0, -1.0); break;
		case 3: raydir = vec2(-1.0, (u - 3.0) * 2.0 - 1.0); break;
		}

		// Find the position for the ray starting at the light position and travelling until light contribution is zero:
		vec2 pixelpos = lightpos + raydir * radius;

		// Check if we hit any line between the light and the end position:
		float t = rayTest(lightpos, pixelpos);

		// Calculate the square distance for the hit, if any:
		vec2 delta = (pixelpos - lightpos) * t;
		float dist2 = dot(delta, delta);

		FragColor = vec4(dist2, 0.0, 0.0, 1.0);
	}
	else
	{
		FragColor = vec4(1.0, 0.0, 0.0, 1.0);
	}
}
