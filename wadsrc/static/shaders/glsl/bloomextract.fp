
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D SceneTexture;
uniform float ExposureAdjustment;

void main()
{
	vec4 color = texture(SceneTexture, TexCoord);
	FragColor = max(vec4(color.rgb * ExposureAdjustment - 1, 1), vec4(0));
}
