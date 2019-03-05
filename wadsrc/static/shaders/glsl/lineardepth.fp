
layout(location=0) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

#if defined(MULTISAMPLE)
layout(binding=0) uniform sampler2DMS DepthTexture;
layout(binding=1) uniform sampler2DMS ColorTexture;
#else
layout(binding=0) uniform sampler2D DepthTexture;
layout(binding=1) uniform sampler2D ColorTexture;
#endif

float normalizeDepth(float depth)
{
	float normalizedDepth = clamp(InverseDepthRangeA * depth + InverseDepthRangeB, 0.0, 1.0);
	return 1.0 / (normalizedDepth * LinearizeDepthA + LinearizeDepthB);
}

void main()
{
	vec2 uv = Offset + TexCoord * Scale;

#if defined(MULTISAMPLE)
	ivec2 texSize = textureSize(DepthTexture);
#else
	ivec2 texSize = textureSize(DepthTexture, 0);
#endif

	ivec2 ipos = ivec2(max(uv * vec2(texSize), vec2(0.0)));

#if defined(MULTISAMPLE)
	float depth = normalizeDepth(texelFetch(ColorTexture, ipos, SampleIndex).a != 0.0 ? texelFetch(DepthTexture, ipos, SampleIndex).x : 1.0);
#else
	float depth = normalizeDepth(texelFetch(ColorTexture, ipos, 0).a != 0.0 ? texelFetch(DepthTexture, ipos, 0).x : 1.0);
#endif

	FragColor = vec4(depth, 0.0, 0.0, 1.0);
}
