
in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D Bloom;

void main()
{
	FragColor = vec4(texture(Bloom, TexCoord).rgb, 0.0);
}
