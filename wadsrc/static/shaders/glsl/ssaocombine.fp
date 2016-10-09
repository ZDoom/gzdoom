
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D AODepthTexture;

#if defined(MULTISAMPLE)
uniform sampler2DMS SceneFogTexture;
uniform int SampleCount;
#else
uniform sampler2D SceneFogTexture;
#endif

uniform vec2 Scale;
uniform vec2 Offset;

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
