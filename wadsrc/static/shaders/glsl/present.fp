
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

float halfstep = 1./65.;
float DitherMatrix[64] = float[](
	 0.0 / 16.0 + halfstep * 1,  8.0 / 16.0 + halfstep * 1,  2.0 / 16.0 + halfstep * 1, 10.0 / 16.0 + halfstep * 1,
	 0.0 / 16.0 + halfstep * 3,  8.0 / 16.0 + halfstep * 3,  2.0 / 16.0 + halfstep * 3, 10.0 / 16.0 + halfstep * 3,
	12.0 / 16.0 + halfstep * 1,  4.0 / 16.0 + halfstep * 1, 14.0 / 16.0 + halfstep * 1,  6.0 / 16.0 + halfstep * 1,
	12.0 / 16.0 + halfstep * 3,  4.0 / 16.0 + halfstep * 3, 14.0 / 16.0 + halfstep * 3,  6.0 / 16.0 + halfstep * 3,
	 3.0 / 16.0 + halfstep * 1, 11.0 / 16.0 + halfstep * 1,  1.0 / 16.0 + halfstep * 1,  9.0 / 16.0 + halfstep * 1,
	 3.0 / 16.0 + halfstep * 3, 11.0 / 16.0 + halfstep * 3,  1.0 / 16.0 + halfstep * 3,  9.0 / 16.0 + halfstep * 3,
	15.0 / 16.0 + halfstep * 1,  7.0 / 16.0 + halfstep * 1, 13.0 / 16.0 + halfstep * 1,  5.0 / 16.0 + halfstep * 1,
	15.0 / 16.0 + halfstep * 3,  7.0 / 16.0 + halfstep * 3, 13.0 / 16.0 + halfstep * 3,  5.0 / 16.0 + halfstep * 3,
	 0.0 / 16.0 + halfstep * 4,  8.0 / 16.0 + halfstep * 4,  2.0 / 16.0 + halfstep * 4, 10.0 / 16.0 + halfstep * 4,
	 0.0 / 16.0 + halfstep * 2,  8.0 / 16.0 + halfstep * 2,  2.0 / 16.0 + halfstep * 2, 10.0 / 16.0 + halfstep * 2,
	12.0 / 16.0 + halfstep * 4,  4.0 / 16.0 + halfstep * 4, 14.0 / 16.0 + halfstep * 4,  6.0 / 16.0 + halfstep * 4,
	12.0 / 16.0 + halfstep * 2,  4.0 / 16.0 + halfstep * 2, 14.0 / 16.0 + halfstep * 2,  6.0 / 16.0 + halfstep * 2,
	 3.0 / 16.0 + halfstep * 4, 11.0 / 16.0 + halfstep * 4,  1.0 / 16.0 + halfstep * 4,  9.0 / 16.0 + halfstep * 4,
	 3.0 / 16.0 + halfstep * 2, 11.0 / 16.0 + halfstep * 2,  1.0 / 16.0 + halfstep * 2,  9.0 / 16.0 + halfstep * 2,
	15.0 / 16.0 + halfstep * 4,  7.0 / 16.0 + halfstep * 4, 13.0 / 16.0 + halfstep * 4,  5.0 / 16.0 + halfstep * 4,
	15.0 / 16.0 + halfstep * 2,  7.0 / 16.0 + halfstep * 2, 13.0 / 16.0 + halfstep * 2,  5.0 / 16.0 + halfstep * 2
);

vec4 Dither(vec4 c, float colorscale)
{
	ivec2 pos = ivec2(gl_FragCoord.xy) & 7;
	float threshold = DitherMatrix[pos.x + (pos.y << 3)];
	return vec4(floor(c.rgb * colorscale + threshold) / colorscale, c.a);
}

void main()
{
	FragColor = Dither(ApplyGamma(texture(InputTexture, TexCoord)), 255.0);
}
