
in vec2 TexCoord;
out vec4 FragColor;

#if defined(MULTISAMPLE)
uniform sampler2DMS DepthTexture;
uniform sampler2DMS ColorTexture;
uniform int SampleCount;
#else
uniform sampler2D DepthTexture;
uniform sampler2D ColorTexture;
#endif

uniform float LinearizeDepthA;
uniform float LinearizeDepthB;
uniform float InverseDepthRangeA;
uniform float InverseDepthRangeB;
uniform vec2 Scale;
uniform vec2 Offset;

void main()
{
	vec2 uv = Offset + TexCoord * Scale;

#if defined(MULTISAMPLE)
	ivec2 texSize = textureSize(DepthTexture);
#else
	ivec2 texSize = textureSize(DepthTexture, 0);
#endif

	// Use floor here because as we downscale the sampling error has to remain uniform to prevent
	// noise in the depth values.
	ivec2 ipos = ivec2(max(floor(uv * vec2(texSize) - 0.75), vec2(0.0)));

#if defined(MULTISAMPLE)
	float depth = 0.0;
	for (int i = 0; i < SampleCount; i++)
		depth += texelFetch(ColorTexture, ipos, i).a != 0.0 ? texelFetch(DepthTexture, ipos, i).x : 1.0;
	depth /= float(SampleCount);
#else
	float depth = texelFetch(ColorTexture, ipos, 0).a != 0.0 ? texelFetch(DepthTexture, ipos, 0).x : 1.0;
#endif

	float normalizedDepth = clamp(InverseDepthRangeA * depth + InverseDepthRangeB, 0.0, 1.0);
	FragColor = vec4(1.0 / (normalizedDepth * LinearizeDepthA + LinearizeDepthB), 0.0, 0.0, 1.0);
}
