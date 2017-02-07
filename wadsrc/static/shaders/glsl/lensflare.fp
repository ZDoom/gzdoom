in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D InputTexture;

uniform vec4 Scale;
uniform vec4 Bias;

void main() {
	FragColor = texture(InputTexture, TexCoord);//max(vec4(0.0), texture(InputTexture, TexCoord) + Bias) * Scale;
}