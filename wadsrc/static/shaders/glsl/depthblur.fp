
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D AODepthTexture;
uniform float BlurSharpness;
uniform vec2 InvFullResolution;
uniform float PowExponent;

#define KERNEL_RADIUS 3.0

struct CenterPixelData
{
	vec2 UV;
	float Depth;
	float Sharpness;
};

float CrossBilateralWeight(float r, float sampleDepth, CenterPixelData center)
{
	const float blurSigma = KERNEL_RADIUS * 0.5;
	const float blurFalloff = 1.0 / (2.0 * blurSigma * blurSigma);

	float deltaZ = (sampleDepth - center.Depth) * center.Sharpness;

	return exp2(-r * r * blurFalloff - deltaZ * deltaZ);
}

void ProcessSample(float ao, float z, float r, CenterPixelData center, inout float totalAO, inout float totalW)
{
	float w = CrossBilateralWeight(r, z, center);
	totalAO += w * ao;
	totalW += w;
}

void ProcessRadius(vec2 deltaUV, CenterPixelData center, inout float totalAO, inout float totalW)
{
	for (float r = 1; r <= KERNEL_RADIUS; r += 1.0)
	{
		vec2 uv = r * deltaUV + center.UV;
		vec2 aoZ = texture(AODepthTexture, uv).xy;
		ProcessSample(aoZ.x, aoZ.y, r, center, totalAO, totalW);
	}
}

vec2 ComputeBlur(vec2 deltaUV)
{
	vec2 aoZ = texture(AODepthTexture, TexCoord).xy;

	CenterPixelData center;
	center.UV = TexCoord;
	center.Depth = aoZ.y;
	center.Sharpness = BlurSharpness;

	float totalAO = aoZ.x;
	float totalW = 1.0;

	ProcessRadius(deltaUV, center, totalAO, totalW);
	ProcessRadius(-deltaUV, center, totalAO, totalW);

	return vec2(totalAO / totalW, aoZ.y);
}

vec2 BlurX()
{
	return ComputeBlur(vec2(InvFullResolution.x, 0.0));
}

float BlurY()
{
	return pow(clamp(ComputeBlur(vec2(0.0, InvFullResolution.y)).x, 0.0, 1.0), PowExponent);
}

void main()
{
#if defined(BLUR_HORIZONTAL)
	FragColor = vec4(BlurX(), 0.0, 1.0);
#else
	FragColor = vec4(BlurY(), 0.0, 0.0, 1.0);
#endif
}
