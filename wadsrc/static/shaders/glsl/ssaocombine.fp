
in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D AODepthTexture;

#if defined(MULTISAMPLE)
layout(binding=1) uniform sampler2DMS SceneFogTexture;
#else
layout(binding=1) uniform sampler2D SceneFogTexture;
#endif

void main()
{
	vec2 uv = Offset + TexCoord * Scale;

#if defined(MULTISAMPLE)
	ivec2 texSize = textureSize(SceneFogTexture);
#else
	ivec2 texSize = textureSize(SceneFogTexture, 0);
#endif
	ivec2 ipos = ivec2(uv * vec2(texSize));

#if defined(MULTISAMPLE)
	vec3 fogColor = vec3(0.0);
	for (int i = 0; i < SampleCount; i++)
		fogColor += texelFetch(SceneFogTexture, ipos, i).rgb;
	fogColor /= float(SampleCount);
#else
	vec3 fogColor = texelFetch(SceneFogTexture, ipos, 0).rgb;
#endif

	float attenutation = texture(AODepthTexture, TexCoord).x;
	FragColor = vec4(fogColor, 1.0 - attenutation);
}
