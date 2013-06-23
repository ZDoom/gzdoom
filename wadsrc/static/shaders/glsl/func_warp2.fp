uniform float timer;

vec4 Process(vec4 color)
{
	vec2 texCoord = gl_TexCoord[0].st;

	const float pi = 3.14159265358979323846;
	vec2 offset = vec2(0.0,0.0);

	float siny = sin(pi * 2.0 * (texCoord.y * 2.2 + timer * 0.75)) * 0.03;
	offset.y = siny + sin(pi * 2.0 * (texCoord.x * 0.75 + timer * 0.75)) * 0.03;
	offset.x = siny + sin(pi * 2.0 * (texCoord.x * 1.1 + timer * 0.45)) * 0.02;

	texCoord += offset;

	return getTexel(texCoord) * color;
}

