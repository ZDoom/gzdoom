
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D InputTexture;
uniform float InvGamma;
uniform float Contrast;
uniform float Brightness;
uniform float Saturation;
uniform int GrayFormula;

vec4 ApplyGamma(vec4 c)
{
	vec3 valgray;
	if (Saturation != 1.0) // attempt to cool things a bit, this calculation makes the GPU run really hot
		if (GrayFormula == 0)
			valgray = vec3(c.r + c.g + c.b) * (1 - Saturation) / 3 + c.rgb * Saturation;
		else
			valgray = vec3(0.3 * c.r + 0.56 * c.g + 0.14 * c.b) * (1 - Saturation) + c.rgb * Saturation;
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
