
layout(location=0) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D AODepthTexture;

#define KERNEL_RADIUS 3.0

void AddSample(vec2 blurSample, float r, float centerDepth, inout float totalAO, inout float totalW)
{
	const float blurSigma = KERNEL_RADIUS * 0.5;
	const float blurFalloff = 1.0 / (2.0 * blurSigma * blurSigma);

	float ao = blurSample.x;
	float z = blurSample.y;

	float deltaZ = (z - centerDepth) * BlurSharpness;
	float w = exp2(-r * r * blurFalloff - deltaZ * deltaZ);

	totalAO += w * ao;
	totalW += w;
}

void main()
{
	vec2 centerSample = textureOffset(AODepthTexture, TexCoord, ivec2( 0, 0)).xy;
	float centerDepth = centerSample.y;
	float totalAO = centerSample.x;
	float totalW = 1.0;

#if defined(BLUR_HORIZONTAL)
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2(-3, 0)).xy, 3.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2(-2, 0)).xy, 2.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2(-1, 0)).xy, 1.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2( 1, 0)).xy, 1.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2( 2, 0)).xy, 2.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2( 3, 0)).xy, 3.0, centerDepth, totalAO, totalW);
#else
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2(0, -3)).xy, 3.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2(0, -2)).xy, 2.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2(0, -1)).xy, 1.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2(0,  1)).xy, 1.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2(0,  2)).xy, 2.0, centerDepth, totalAO, totalW);
	AddSample(textureOffset(AODepthTexture, TexCoord, ivec2(0,  3)).xy, 3.0, centerDepth, totalAO, totalW);
#endif

	float fragAO = totalAO / totalW;

#if defined(BLUR_HORIZONTAL)
	FragColor = vec4(fragAO, centerDepth, 0.0, 1.0);
#else
	FragColor = vec4(pow(clamp(fragAO, 0.0, 1.0), PowExponent), 0.0, 0.0, 1.0);
#endif
}
