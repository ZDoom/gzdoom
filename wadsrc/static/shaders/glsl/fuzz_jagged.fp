//created by Evil Space Tomato
uniform float timer;

vec4 Process(vec4 color)
{
	vec2 texCoord = gl_TexCoord[0].st;

	vec2 texSplat;
	const float pi = 3.14159265358979323846;
	texSplat.x = texCoord.x + mod(sin(pi * 2.0 * (texCoord.y + timer * 2.0)),0.1) * 0.1;
	texSplat.y = texCoord.y + mod(cos(pi * 2.0 * (texCoord.x + timer * 2.0)),0.1) * 0.1;

	vec4 basicColor = getTexel(texSplat) * color;

	float texX = sin(texCoord.x * 100.0 + timer*5.0);
	float texY = cos(texCoord.x * 100.0 + timer*5.0);
	float vX = (texX/texY)*21.0;
	float vY = (texY/texX)*13.0;

	float test = mod(timer*2.0+(vX + vY), 0.5);

	basicColor.a = basicColor.a * test;

	return basicColor;
}
