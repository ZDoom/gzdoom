
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D Bloom;

void main()
{
	FragColor = vec4(texture(Bloom, TexCoord).rgb, 0.0);
}
