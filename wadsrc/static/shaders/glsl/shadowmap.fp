
in vec2 TexCoord;
out vec4 FragColor;

struct GPUNode
{
	vec4 plane;
	int children[2];
	int linecount[2];
};

struct GPUSeg
{
	vec2 pos;
	vec2 delta;
	vec4 bSolid;
};

layout(std430, binding = 2) buffer LightNodes
{
	GPUNode bspNodes[];
};

layout(std430, binding = 3) buffer LightSegs
{
	GPUSeg bspSegs[];
};

layout(std430, binding = 4) buffer LightList
{
	vec4 lights[];
};

//===========================================================================
//
// Ray/BSP collision test. Returns where the ray hit something.
//
//===========================================================================

vec2 rayTest(vec2 from, vec2 to)
{
	const int max_iterations = 50;
	const float epsilon = 0.0000001;

	// Avoid wall acne by adding some margin
	vec2 margin = normalize(to - from);

	vec2 raydelta = to - from;
	float raydist2 = dot(raydelta, raydelta);
	vec2 raynormal = vec2(raydelta.y, -raydelta.x);
	float rayd = dot(raynormal, from);

	if (raydist2 < 1.0 || bspNodes.length() == 0)
		return to;

	int nodeIndex = bspNodes.length() - 1;

	for (int iteration = 0; iteration < max_iterations; iteration++)
	{
		GPUNode node = bspNodes[nodeIndex];
		int side = (dot(node.plane, vec4(from, 0.0, 1.0)) > 0.0) ? 1 : 0;
		int linecount = node.linecount[side];
		if (linecount < 0)
		{
			nodeIndex = node.children[side];
		}
		else
		{
			int startLineIndex = node.children[side];

			// Ray/line test each line segment.
			bool hit_line = false;
			for (int i = 0; i < linecount; i++)
			{
				GPUSeg seg = bspSegs[startLineIndex + i];

				float den = dot(raynormal, seg.delta);
				if (abs(den) > epsilon)
				{
					float t_seg = (rayd - dot(raynormal, seg.pos)) / den;
					if (t_seg >= 0.0 && t_seg <= 1.0)
					{
						vec2 seghitdelta = seg.pos + seg.delta * t_seg - from;
						if (dot(raydelta, seghitdelta) > 0.0 && dot(seghitdelta, seghitdelta) < raydist2) // We hit a line segment.
						{
							if (seg.bSolid.x > 0.0) // segment line is one-sided
								return from + seghitdelta;

							// We hit a two-sided segment line. Move to the other side and continue ray tracing.
							from = from + seghitdelta + margin;

							raydelta = to - from;
							raydist2 = dot(raydelta, raydelta);
							raynormal = vec2(raydelta.y, -raydelta.x);
							rayd = dot(raynormal, from);

							if (raydist2 < 1.0 || bspNodes.length() == 0)
								return to;

							nodeIndex = bspNodes.length() - 1;

							hit_line = true;
							break;
						}
					}
				}
			}

			if (!hit_line)
				return to;
		}
	}

	return to;
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
		switch (int(gl_FragCoord.x) / 256)
		{
		case 0: pixelpos = vec2((gl_FragCoord.x - 128.0) / 128.0, 1.0); break;
		case 1: pixelpos = vec2(1.0, (gl_FragCoord.x - 384.0) / 128.0); break;
		case 2: pixelpos = vec2(-(gl_FragCoord.x - 640.0) / 128.0, -1.0); break;
		case 3: pixelpos = vec2(-1.0, -(gl_FragCoord.x - 896.0) / 128.0); break;
		}
		pixelpos = lightpos + pixelpos * radius;

		vec2 hitpos = rayTest(lightpos, pixelpos);
		vec2 delta = hitpos - lightpos;
		float dist2 = dot(delta, delta);

		FragColor = vec4(dist2, 0.0, 0.0, 1.0);
	}
	else
	{
		FragColor = vec4(1.0, 0.0, 0.0, 1.0);
	}
}
