
layout(set = 0, binding = 0) uniform sampler2D Tex;

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

void main()
{
	FragColor = texture(Tex, TexCoord);
}
