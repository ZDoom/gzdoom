in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D InputTexture;

void main() {
	vec3 c;
	c.r = texture(InputTexture, TexCoord).r;
	c.g = texture(InputTexture, TexCoord).g * 0;
	c.b = texture(InputTexture, TexCoord).b * 0;
	FragColor = vec4(c, 1.0);
}