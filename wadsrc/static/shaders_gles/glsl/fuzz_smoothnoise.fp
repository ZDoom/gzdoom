//created by Evil Space Tomato

vec4 ProcessTexel()
{
	vec2 texCoord = vTexCoord.st;
	vec4 basicColor = getTexel(texCoord);

	float texX = sin(mod(texCoord.x * 100.0 + timer*5.0, 3.489)) + texCoord.x / 4.0;
	float texY = cos(mod(texCoord.y * 100.0 + timer*5.0, 3.489)) + texCoord.y / 4.0;
	float vX = (texX/texY)*21.0;
	float vY = (texY/texX)*13.0;


	float test = mod(timer*2.0+(vX + vY), 0.5);
	basicColor.a = basicColor.a * test;

	basicColor.rgb = vec3(0.0,0.0,0.0);
	return basicColor;
}
