uniform float timer;

vec4 Process(vec4 color)
{
	vec2 texCoord = gl_TexCoord[0].st;

	const float pi = 3.14159265358979323846;
	vec2 offset = vec2(0,0);

	offset.y = sin(pi * 2.0 * (texCoord.x + timer * 0.125)) * 0.1;
	offset.x = sin(pi * 2.0 * (texCoord.y + timer * 0.125)) * 0.1;

	texCoord += offset;

	return getTexel(texCoord) * color;
}

