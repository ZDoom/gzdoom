uniform float timer;

vec4 ProcessTexel()
{
	vec2 texCoord = vTexCoord.st;

	const float pi = 3.14159265358979323846;

	texCoord.x += sin(pi * 2.0 * (texCoord.y + timer * 0.125)) * 0.1;

	return getTexel(texCoord);
}

