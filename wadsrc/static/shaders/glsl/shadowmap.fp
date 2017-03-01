
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

//===========================================================================
//
// Ray/BSP collision test. Returns 0 if the ray hit a line, 1 otherwise.
//
//===========================================================================

float rayTest(vec2 from, vec2 to)
{
	const int max_iterations = 50;
	const float epsilon = 0.0000001;

	// Avoid wall acne by adding some margin
	vec2 margin = normalize(to - from);
	to -= margin;

	vec2 raydelta = to - from;
	float raydist2 = dot(raydelta, raydelta);
	vec2 raynormal = vec2(raydelta.y, -raydelta.x);
	float rayd = dot(raynormal, from);

	if (raydist2 < 1.0 || bspNodes.length() == 0)
		return 1.0;

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
								return 0.0;

							// We hit a two-sided segment line. Move to the other side and continue ray tracing.
							from = from + seghitdelta + margin;

							raydelta = to - from;
							raydist2 = dot(raydelta, raydelta);
							raynormal = vec2(raydelta.y, -raydelta.x);
							rayd = dot(raynormal, from);

							if (raydist2 < 1.0 || bspNodes.length() == 0)
								return 1.0;

							nodeIndex = bspNodes.length() - 1;

							hit_line = true;
							break;
						}
					}
				}
			}

			if (!hit_line)
				return 1.0;
		}
	}

	return 0.0;
}

void main()
{
	FragColor = vec4(rayTest(vec2(0.0, 0.0), vec2(1.0, 1.0));
}
