
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
	vec3 WorldToLocal;
	float PushPadding5;
	vec3 ProjLocalToU;
	float PushPadding6;
	vec3 ProjLocalToV;
	float PushPadding7;
	int TileX;
	int TileY;
	int TileWidth;
	int TileHeight;
};

layout(location = 0) in vec3 aPosition;
layout(location = 0) out vec3 worldpos;

void main()
{
	vec3 localPos = aPosition - WorldToLocal;
	float u = (1.0f + dot(localPos, ProjLocalToU)) / float(TileWidth + 2);
	float v = (1.0f + dot(localPos, ProjLocalToV)) / float(TileHeight + 2);

	worldpos = LightmapOrigin + LightmapStepX * u + LightmapStepY * v;
	gl_Position = vec4(vec2(u, v) * 2.0 - 1.0, 0.0, 1.0);
}
 