
layout(location=0) in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D LeftEyeTexture;
layout(binding=1) uniform sampler2D RightEyeTexture;

vec4 ApplyGamma(vec4 c)
{
	vec3 val = c.rgb * Contrast - (Contrast - 1.0) * 0.5;
	val = (val + Brightness * 0.5) * (WhitePoint - BlackPoint) + BlackPoint;
	val = pow(max(val, vec3(0.0)), vec3(InvGamma));
	return vec4(val, c.a);
}

void main()
{
	int thisHorizontalPixel = int(gl_FragCoord.x); // zero-based column index from left
	bool isLeftEye = (thisHorizontalPixel // because we want to alternate eye view on each column
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
