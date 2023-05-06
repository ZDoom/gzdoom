
layout(location = 0) out vec2 TexCoord;

vec2 positions[3] = vec2[](
	vec2(-1.0, -1.0),
	vec2( 3.0, -1.0),
	vec2(-1.0,  3.0)
);

vec2 uvs[3] = vec2[](
	vec2(0.0, 0.0),
	vec2(2.0, 0.0),
	vec2(0.0, 2.0)
);

void main()
{
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	TexCoord = uvs[gl_VertexIndex];
}
