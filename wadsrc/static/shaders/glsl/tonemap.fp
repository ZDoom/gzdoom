
#version 330

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D InputTexture;
uniform float ExposureAdjustment;

vec3 Linear(vec3 c)
{
	//return pow(c, 2.2);
	return c * c; // cheaper, but assuming gamma of 2.0 instead of 2.2
}

vec3 sRGB(vec3 c)
{
	//return pow(c, vec3(1.0 / 2.2));
	return sqrt(c); // cheaper, but assuming gamma of 2.0 instead of 2.2
}

vec3 TonemapLinear(vec3 color)
{
	return sRGB(color);
}

vec3 TonemapReinhard(vec3 color)
{
	color = color / (1 + color);
	return sRGB(color);
}

vec3 TonemapHejlDawson(vec3 color)
{
	vec3 x = max(vec3(0), color - 0.004);
	return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06); // no sRGB needed
}

vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 TonemapUncharted2(vec3 color)
{
	float W = 11.2;
	float ExposureBias = 2.0;
	vec3 curr = Uncharted2Tonemap(ExposureBias * color);
	vec3 whiteScale = vec3(1) / Uncharted2Tonemap(vec3(W));
	return sRGB(curr * whiteScale);
}

void main()
{
	vec3 color = texture(InputTexture, TexCoord).rgb;
	color = color * ExposureAdjustment;
	color = Linear(color); // needed because gzdoom's scene texture is not linear at the moment
	FragColor = vec4(TonemapUncharted2(color), 1.0);
}
