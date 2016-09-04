
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
	ivec2 ipos = ivec2(uv * vec2(texSize));
	float depth = 0.0;
	for (int i = 0; i < SampleCount; i++)
		depth += texelFetch(ColorTexture, ipos, i).a != 0.0 ? texelFetch(DepthTexture, ipos, i).x : 1.0;
	depth /= float(SampleCount);
#else
	/*ivec2 texSize = textureSize(DepthTexture, 0);
	ivec2 ipos = ivec2(uv * vec2(texSize));
	if (ipos.x < 0) ipos.x += texSize.x;
	if (ipos.y < 0) ipos.y += texSize.y;
	float depth = texelFetch(ColorTexture, ipos, 0).a != 0.0 ? texelFetch(DepthTexture, ipos, 0).x : 1.0;*/
	float depth = texture(ColorTexture, uv).a != 0.0 ? texture(DepthTexture, uv).x : 1.0;
#endif

	float normalizedDepth = clamp(InverseDepthRangeA * depth + InverseDepthRangeB, 0.0, 1.0);
	FragColor = vec4(1.0 / (normalizedDepth * LinearizeDepthA + LinearizeDepthB), 0.0, 0.0, 1.0);
}
