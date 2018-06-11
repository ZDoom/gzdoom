
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D SceneTexture;
uniform sampler2D ExposureTexture;

void main()
{
	float exposureAdjustment = texture(ExposureTexture, vec2(0.5)).x;
	vec4 color = texture(SceneTexture, Offset + TexCoord * Scale);
	FragColor = max(vec4((color.rgb + vec3(0.001)) * exposureAdjustment - 1, 1), vec4(0));
}
