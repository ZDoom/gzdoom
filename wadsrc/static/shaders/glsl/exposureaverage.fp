
in vec2 TexCoord;
layout(location=0) out vec4 FragColor;
layout(binding=0) uniform sampler2D ExposureTexture;

void main()
{
#if __VERSION__ < 400
	ivec2 size = textureSize(ExposureTexture, 0);
	ivec2 tl = max(ivec2(TexCoord * vec2(size) - 0.5), ivec2(0));
	ivec2 br = min(tl + ivec2(1), size - ivec2(1));
	vec4 values = vec4(
		texelFetch(ExposureTexture, tl, 0).x,
		texelFetch(ExposureTexture, ivec2(tl.x, br.y), 0).x,
		texelFetch(ExposureTexture, ivec2(br.x, tl.y), 0).x,
		texelFetch(ExposureTexture, br, 0).x);
#else
	vec4 values = textureGather(ExposureTexture, TexCoord);
#endif

	FragColor = vec4((values.x + values.y + values.z + values.w) * 0.25, 0.0, 0.0, 1.0);
}
