
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D InputTexture;
uniform float Gamma;
uniform float Contrast;
uniform float Brightness;

vec4 ApplyGamma(vec4 c)
{
	vec3 val = c.rgb * Contrast - (Contrast - 1.0) * 0.5;
	val = pow(val, vec3(1.0 / Gamma));
	val += Brightness * 0.5;
	return vec4(val, c.a);
}

void main()
{
	FragColor = ApplyGamma(texture(InputTexture, TexCoord));
}
