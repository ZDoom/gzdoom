

vec2 GetTexCoord()
{
	vec2 texCoord = vTexCoord.st;

	const float pi = 3.14159265358979323846;
	vec2 offset = vec2(0.0,0.0);

	float siny = sin(pi * 2.0 * (texCoord.y * 2.0 + timer * 0.75)) * 0.03;
	offset.y = siny + sin(pi * 2.0 * (texCoord.x + timer * 0.75)) * 0.03;
	offset.x = siny + sin(pi * 2.0 * (texCoord.x + timer * 0.45)) * 0.02;

	return texCoord + offset;
}

vec4 ProcessTexel()
{
	return getTexel(GetTexCoord());
}

