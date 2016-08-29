
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D DepthTexture;
uniform float LinearizeDepthA;
uniform float LinearizeDepthB;
uniform float InverseDepthRangeA;
uniform float InverseDepthRangeB;

void main()
{
	float depth = texture(DepthTexture, TexCoord).x;
	float normalizedDepth = clamp(InverseDepthRangeA * depth + InverseDepthRangeB, 0.0, 1.0);
	FragColor = vec4(1.0 / (normalizedDepth * LinearizeDepthA + LinearizeDepthB), 0.0, 0.0, 1.0);
}
