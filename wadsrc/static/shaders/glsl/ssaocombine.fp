
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D AODepthTexture;

void main()
{
	float attenutation = texture(AODepthTexture, TexCoord).x;
	FragColor = vec4(attenutation, attenutation, attenutation, 0.0);
}
