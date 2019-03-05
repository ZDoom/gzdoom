
layout(location=0) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;
layout(binding=0) uniform sampler2D SceneTexture;
layout(binding=1) uniform sampler2D ExposureTexture;

void main()
{
	float exposureAdjustment = texture(ExposureTexture, vec2(0.5)).x;
	vec4 color = texture(SceneTexture, Offset + TexCoord * Scale);
	FragColor = max(vec4((color.rgb + vec3(0.001)) * exposureAdjustment - 1, 1), vec4(0));
}
