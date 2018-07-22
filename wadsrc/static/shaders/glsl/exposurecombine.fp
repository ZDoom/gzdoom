
in vec2 TexCoord;
layout(location=0) out vec4 FragColor;

layout(binding=0) uniform sampler2D ExposureTexture;

void main()
{
	float light = texture(ExposureTexture, TexCoord).x;
	float exposureAdjustment = 1.0 / max(ExposureBase + light * ExposureScale, ExposureMin);
	FragColor = vec4(exposureAdjustment, 0.0, 0.0, ExposureSpeed);
}
