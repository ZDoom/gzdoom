uniform float timer;

vec4 Process(vec4 color)
{
	vec2 texCoord = gl_TexCoord[0].st;

	const float pi = 3.14159265358979323846;

	texCoord.x += sin(pi * 2.0 * (texCoord.y + timer * 0.125)) * 0.1;

	return getTexel(texCoord) * color;
}

