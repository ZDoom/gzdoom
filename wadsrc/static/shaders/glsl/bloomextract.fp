
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D SceneTexture;
uniform float ExposureAdjustment;
uniform vec2 Scale;
uniform vec2 Offset;

void main()
{
	vec4 color = texture(SceneTexture, Offset + TexCoord * Scale);
	FragColor = max(vec4(color.rgb * ExposureAdjustment - 1, 1), vec4(0));
}
