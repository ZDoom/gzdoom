
in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D InputTexture;

vec4 ApplyGamma(vec4 c)
{
	vec3 valgray;
	if (GrayFormula == 0)
		valgray = vec3(c.r + c.g + c.b) * (1 - Saturation) / 3 + c.rgb * Saturation;
	else
		valgray = mix(vec3(dot(c.rgb, vec3(0.3,0.56,0.14))), c.rgb, Saturation);
	vec3 val = valgray * Contrast - (Contrast - 1.0) * 0.5;
	val += Brightness * 0.5;
	val = pow(max(val, vec3(0.0)), vec3(InvGamma));
	return vec4(val, c.a);
}

float halfstep = 1./32.;
float DitherMatrix[16] = float[](
	 0.0 / 16.0 + halfstep,  8.0 / 16.0 + halfstep,  2.0 / 16.0 + halfstep, 10.0 / 16.0 + halfstep,
	12.0 / 16.0 + halfstep,  4.0 / 16.0 + halfstep, 14.0 / 16.0 + halfstep,  6.0 / 16.0 + halfstep,
	 3.0 / 16.0 + halfstep, 11.0 / 16.0 + halfstep,  1.0 / 16.0 + halfstep,  9.0 / 16.0 + halfstep,
	15.0 / 16.0 + halfstep,  7.0 / 16.0 + halfstep, 13.0 / 16.0 + halfstep,  5.0 / 16.0 + halfstep
);

vec4 Dither(vec4 c, float colorscale)
{
	ivec2 pos = ivec2(gl_FragCoord.xy) & 3;
	float threshold = DitherMatrix[pos.x + (pos.y << 2)];
	return vec4(floor(c.rgb * colorscale + threshold) / colorscale, c.a);
}

void main()
{
	FragColor = Dither(ApplyGamma(texture(InputTexture, TexCoord)), 255.0);
}
