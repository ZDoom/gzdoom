
layout(push_constant) uniform PushConstants
{
	uint LightStart;
	uint LightEnd;
	int SurfaceIndex;
	int PushPadding1;
	vec3 LightmapOrigin;
	float TextureSize;
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
	float TileX;
	float TileY;
	float TileWidth;
	float TileHeight;
};

layout(location = 0) in vec3 aPosition;
layout(location = 0) out vec3 worldpos;

void main()
{
	worldpos = aPosition;

	// Project to position relative to tile
	vec3 localPos = aPosition - WorldToLocal;
	float x = 1.0 + dot(localPos, ProjLocalToU);
	float y = 1.0 + dot(localPos, ProjLocalToV);

	// Find the position in the output texture
	gl_Position = vec4(vec2(TileX + x, TileY + y) / TextureSize * 2.0 - 1.0, 0.0, 1.0);

	// Clip all surfaces to the edge of the tile (effectly we are applying a viewport/scissor to the tile)
	gl_ClipDistance[0] = x;
	gl_ClipDistance[1] = y;
	gl_ClipDistance[2] = TileWidth + 2.0 - x;
	gl_ClipDistance[3] = TileHeight + 2.0 - y;
}
