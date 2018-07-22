
in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D SourceTexture;

void main()
{
#if defined(BLUR_HORIZONTAL)
	FragColor =
		textureOffset(SourceTexture, TexCoord, ivec2( 0, 0)) * SampleWeights0 +
		textureOffset(SourceTexture, TexCoord, ivec2( 1, 0)) * SampleWeights1 +
		textureOffset(SourceTexture, TexCoord, ivec2(-1, 0)) * SampleWeights2 +
		textureOffset(SourceTexture, TexCoord, ivec2( 2, 0)) * SampleWeights3 +
		textureOffset(SourceTexture, TexCoord, ivec2(-2, 0)) * SampleWeights4 +
		textureOffset(SourceTexture, TexCoord, ivec2( 3, 0)) * SampleWeights5 +
		textureOffset(SourceTexture, TexCoord, ivec2(-3, 0)) * SampleWeights6;
#else
	FragColor =
		textureOffset(SourceTexture, TexCoord, ivec2(0, 0)) * SampleWeights0 +
		textureOffset(SourceTexture, TexCoord, ivec2(0, 1)) * SampleWeights1 +
		textureOffset(SourceTexture, TexCoord, ivec2(0,-1)) * SampleWeights2 +
		textureOffset(SourceTexture, TexCoord, ivec2(0, 2)) * SampleWeights3 +
		textureOffset(SourceTexture, TexCoord, ivec2(0,-2)) * SampleWeights4 +
		textureOffset(SourceTexture, TexCoord, ivec2(0, 3)) * SampleWeights5 +
		textureOffset(SourceTexture, TexCoord, ivec2(0,-3)) * SampleWeights6;
#endif
}
