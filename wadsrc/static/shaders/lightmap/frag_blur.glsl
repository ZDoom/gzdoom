
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(location = 0) in vec3 worldpos;
layout(location = 0) out vec4 fragcolor;

void main()
{
	ivec2 size = textureSize(tex, 0);
	vec2 texCoord = gl_FragCoord.xy / vec2(size);

#if defined(BLUR_HORIZONTAL)
	fragcolor =
		textureOffset(tex, texCoord, ivec2( 0, 0)) * 0.5 +
		textureOffset(tex, texCoord, ivec2( 1, 0)) * 0.25 +
		textureOffset(tex, texCoord, ivec2(-1, 0)) * 0.25;
#else
	fragcolor =
		textureOffset(tex, texCoord, ivec2(0, 0)) * 0.5 +
		textureOffset(tex, texCoord, ivec2(0, 1)) * 0.25 +
		textureOffset(tex, texCoord, ivec2(0,-1)) * 0.25;
#endif
}
