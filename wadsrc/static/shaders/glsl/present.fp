
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

#define HALFSTEP 1./65.
float DitherMatrix[64] = float[](
	 0.0 / 16.0 + HALFSTEP * 1,  8.0 / 16.0 + HALFSTEP * 1,  2.0 / 16.0 + HALFSTEP * 1, 10.0 / 16.0 + HALFSTEP * 1,
	 0.0 / 16.0 + HALFSTEP * 3,  8.0 / 16.0 + HALFSTEP * 3,  2.0 / 16.0 + HALFSTEP * 3, 10.0 / 16.0 + HALFSTEP * 3,
	12.0 / 16.0 + HALFSTEP * 1,  4.0 / 16.0 + HALFSTEP * 1, 14.0 / 16.0 + HALFSTEP * 1,  6.0 / 16.0 + HALFSTEP * 1,
	12.0 / 16.0 + HALFSTEP * 3,  4.0 / 16.0 + HALFSTEP * 3, 14.0 / 16.0 + HALFSTEP * 3,  6.0 / 16.0 + HALFSTEP * 3,
	 3.0 / 16.0 + HALFSTEP * 1, 11.0 / 16.0 + HALFSTEP * 1,  1.0 / 16.0 + HALFSTEP * 1,  9.0 / 16.0 + HALFSTEP * 1,
	 3.0 / 16.0 + HALFSTEP * 3, 11.0 / 16.0 + HALFSTEP * 3,  1.0 / 16.0 + HALFSTEP * 3,  9.0 / 16.0 + HALFSTEP * 3,
	15.0 / 16.0 + HALFSTEP * 1,  7.0 / 16.0 + HALFSTEP * 1, 13.0 / 16.0 + HALFSTEP * 1,  5.0 / 16.0 + HALFSTEP * 1,
	15.0 / 16.0 + HALFSTEP * 3,  7.0 / 16.0 + HALFSTEP * 3, 13.0 / 16.0 + HALFSTEP * 3,  5.0 / 16.0 + HALFSTEP * 3,
	 0.0 / 16.0 + HALFSTEP * 4,  8.0 / 16.0 + HALFSTEP * 4,  2.0 / 16.0 + HALFSTEP * 4, 10.0 / 16.0 + HALFSTEP * 4,
	 0.0 / 16.0 + HALFSTEP * 2,  8.0 / 16.0 + HALFSTEP * 2,  2.0 / 16.0 + HALFSTEP * 2, 10.0 / 16.0 + HALFSTEP * 2,
	12.0 / 16.0 + HALFSTEP * 4,  4.0 / 16.0 + HALFSTEP * 4, 14.0 / 16.0 + HALFSTEP * 4,  6.0 / 16.0 + HALFSTEP * 4,
	12.0 / 16.0 + HALFSTEP * 2,  4.0 / 16.0 + HALFSTEP * 2, 14.0 / 16.0 + HALFSTEP * 2,  6.0 / 16.0 + HALFSTEP * 2,
	 3.0 / 16.0 + HALFSTEP * 4, 11.0 / 16.0 + HALFSTEP * 4,  1.0 / 16.0 + HALFSTEP * 4,  9.0 / 16.0 + HALFSTEP * 4,
	 3.0 / 16.0 + HALFSTEP * 2, 11.0 / 16.0 + HALFSTEP * 2,  1.0 / 16.0 + HALFSTEP * 2,  9.0 / 16.0 + HALFSTEP * 2,
	15.0 / 16.0 + HALFSTEP * 4,  7.0 / 16.0 + HALFSTEP * 4, 13.0 / 16.0 + HALFSTEP * 4,  5.0 / 16.0 + HALFSTEP * 4,
	15.0 / 16.0 + HALFSTEP * 2,  7.0 / 16.0 + HALFSTEP * 2, 13.0 / 16.0 + HALFSTEP * 2,  5.0 / 16.0 + HALFSTEP * 2
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
