
layout(location=0) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D AODepthTexture;

#define KERNEL_RADIUS 3.0

void main()
{
	const float blurSigma = KERNEL_RADIUS * 0.5;
	const float blurFalloff = 1.0 / (2.0 * blurSigma * blurSigma);

	vec2 centerSample = texture(AODepthTexture, TexCoord).xy;

	float centerDepth = centerSample.y;
	float totalAO = centerSample.x;
	float totalW = 1.0;

	for (float r = 1.0; r <= KERNEL_RADIUS; r += 1.0)
	{
		vec4 blurSample = texture(AODepthTexture, TexCoord - InvFullResolution * r);
		float ao = blurSample.x;
		float z = blurSample.y;

		float deltaZ = (z - centerDepth) * BlurSharpness;
		float w = exp2(-r * r * blurFalloff - deltaZ * deltaZ);

		totalAO += w * ao;
		totalW += w;
	}

	for (float r = 1.0; r <= KERNEL_RADIUS; r += 1.0)
	{
		vec4 blurSample = texture(AODepthTexture, InvFullResolution * r + TexCoord);
		float ao = blurSample.x;
		float z = blurSample.y;

		float deltaZ = (z - centerDepth) * BlurSharpness;
		float w = exp2(-r * r * blurFalloff - deltaZ * deltaZ);

		totalAO += w * ao;
		totalW += w;
	}

	float fragAO = totalAO / totalW;

#if defined(BLUR_HORIZONTAL)
	FragColor = vec4(fragAO, centerDepth, 0.0, 1.0);
#else
	FragColor = vec4(pow(clamp(fragAO, 0.0, 1.0), PowExponent), 0.0, 0.0, 1.0);
#endif
}
