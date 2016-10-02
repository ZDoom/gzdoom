
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D LeftEyeTexture;
uniform sampler2D RightEyeTexture;
uniform float InvGamma;
uniform float Contrast;
uniform float Brightness;
uniform int WindowHeight;
uniform int VerticalPixelOffset; // top-of-window might not be top-of-screen

vec4 ApplyGamma(vec4 c)
{
	vec3 val = c.rgb * Contrast - (Contrast - 1.0) * 0.5;
	val += Brightness * 0.5;
	val = pow(max(val, vec3(0.0)), vec3(InvGamma));
	return vec4(val, c.a);
}

void main()
{
	int thisVerticalPixel = int(gl_FragCoord.y);
	bool isLeftEye = (thisVerticalPixel + VerticalPixelOffset) % 2 == 0;
	vec4 inputColor;
	if (isLeftEye) {
		inputColor = texture(LeftEyeTexture, TexCoord);
	}
	else {
		// inputColor = vec4(0, 1, 0, 1);
		inputColor = texture(RightEyeTexture, TexCoord);
	}
	FragColor = ApplyGamma(inputColor);
}
