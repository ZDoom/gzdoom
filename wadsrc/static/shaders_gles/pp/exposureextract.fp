
layout(location=0) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D SceneTexture;

void main()
{
	vec4 color = texture(SceneTexture, Offset + TexCoord * Scale);
	FragColor = vec4(max(max(color.r, color.g), color.b), 0.0, 0.0, 1.0);
}
