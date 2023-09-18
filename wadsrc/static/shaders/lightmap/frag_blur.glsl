
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 fragcolor;

vec4 centerFragColor;

vec4 clampedSample(vec4 f)
{
	return f != vec4(0, 0, 0, 0) ? f : centerFragColor;
}

void main()
{
	ivec2 size = textureSize(tex, 0);
	vec2 texCoord = gl_FragCoord.xy / vec2(size);

	centerFragColor = textureOffset(tex, texCoord, ivec2(0, 0));

#if defined(BLUR_HORIZONTAL)
	fragcolor =
		centerFragColor * 0.5 +
		clampedSample(textureOffset(tex, texCoord, ivec2( 1, 0))) * 0.25 +
		clampedSample(textureOffset(tex, texCoord, ivec2(-1, 0))) * 0.25;
#else
	fragcolor =
		centerFragColor * 0.5 +
		clampedSample(textureOffset(tex, texCoord, ivec2(0, 1))) * 0.25 +
		clampedSample(textureOffset(tex, texCoord, ivec2(0,-1))) * 0.25;
#endif
}
