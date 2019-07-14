
layout(location=0) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D LeftEyeTexture;
layout(binding=1) uniform sampler2D RightEyeTexture;

vec4 ApplyGamma(vec4 c)
{
	vec3 val = c.rgb * Contrast - (Contrast - 1.0) * 0.5;
	val += Brightness * 0.5;
	val = pow(max(val, vec3(0.0)), vec3(InvGamma));
	return vec4(val, c.a);
}

void main()
{
	int thisVerticalPixel = int(gl_FragCoord.y); // Bottom row is typically the right eye, when WindowHeight is even
	bool isLeftEye = (thisVerticalPixel // because we want to alternate eye view on each row
		 + WindowPositionParity // because the window might not be aligned to the screen
		) % 2 == 0;
	vec4 inputColor;
	if (isLeftEye) {
		inputColor = texture(LeftEyeTexture, UVOffset + TexCoord * UVScale);
	}
	else {
		// inputColor = vec4(0, 1, 0, 1);
		inputColor = texture(RightEyeTexture, UVOffset + TexCoord * UVScale);
	}
	FragColor = ApplyGamma(inputColor);
}
