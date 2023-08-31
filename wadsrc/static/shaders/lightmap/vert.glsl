
layout(push_constant) uniform PushConstants
{
	uint LightStart;
	uint LightEnd;
	int SurfaceIndex;
	int PushPadding1;
	vec3 LightmapOrigin;
	float PushPadding2;
	vec3 LightmapStepX;
	float PushPadding3;
	vec3 LightmapStepY;
	float PushPadding4;
};

layout(location = 0) in vec2 aPosition;
layout(location = 0) out vec3 worldpos;

void main()
{
	worldpos = LightmapOrigin + LightmapStepX * aPosition.x + LightmapStepY * aPosition.y;
	gl_Position = vec4(aPosition * 2.0 - 1.0, 0.0, 1.0);
}
