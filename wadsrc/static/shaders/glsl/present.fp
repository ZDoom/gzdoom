
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D InputTexture;
uniform float InvGamma;
uniform float Contrast;
uniform float Brightness;
uniform float Saturation;

vec4 ApplyGamma(vec4 c)
{
	vec3 valgray;
	if (Saturation != 1.0) // attempt to cool things a bit, this calculation makes the GPU run really hot
		valgray = (0.3 * c.r + 0.59 * c.g + 0.11 * c.b) * (1 - Saturation) + c.rgb * Saturation;
	else
		valgray = c.rgb;
	vec3 val = valgray * Contrast - (Contrast - 1.0) * 0.5;
	val += Brightness * 0.5;
	val = pow(max(val, vec3(0.0)), vec3(InvGamma));
	return vec4(val, c.a);
}

void main()
{
	FragColor = ApplyGamma(texture(InputTexture, TexCoord));
}
