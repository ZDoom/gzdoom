layout(location=0) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D InputTexture;
layout(binding=1) uniform sampler2D DepthTexture;

const int vertRes = 200;

void main()
{
	ivec2 ires = textureSize(InputTexture, 0);
	ivec2 ores = ires * vertRes / ires.y;
	float depth = texelFetch(DepthTexture, ivec2(TexCoord * ires), 0).x;

	if (depth == 0)
		FragColor = texture(InputTexture, TexCoord);
	else
		FragColor = texture(InputTexture, floor(TexCoord * ores) / ores);
}
