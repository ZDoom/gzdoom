//created by Evil Space Tomato

void SetupMaterial(inout Material material)
{
	vec2 texCoord = vTexCoord.st;
	vec4 basicColor = getTexel(texCoord);
	vec2 texSize = vec2(textureSize(tex, 0));

	texCoord.x = float( int(texCoord.x * texSize.x) ) / texSize.x;
	texCoord.y = float( int(texCoord.y * texSize.y) ) / texSize.y;

	float texX = sin(mod(texCoord.x * 100.0 + timer*5.0, 3.489)) + texCoord.x / 4.0;
	float texY = cos(mod(texCoord.y * 100.0 + timer*5.0, 3.489)) + texCoord.y / 4.0;
	float vX = (texX/texY)*21.0;
	float vY = (texY/texX)*13.0;

	float test = mod(timer*2.0+(vX + vY), 0.5);
	basicColor.a = basicColor.a * test;
	basicColor.rgb = vec3(0.0,0.0,0.0);
	material.Base = basicColor;
}
