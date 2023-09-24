
#if defined(USE_DRAWINDIRECT)

struct LightmapRaytracePC
{
	uint LightStart;
	uint LightEnd;
	int SurfaceIndex;
	int PushPadding1;
	vec3 WorldToLocal;
	float TextureSize;
	vec3 ProjLocalToU;
	float PushPadding2;
	vec3 ProjLocalToV;
	float PushPadding3;
	float TileX;
	float TileY;
	float TileWidth;
	float TileHeight;
};

layout(std430, set = 0, binding = 5) buffer ConstantsBuffer { LightmapRaytracePC constants[]; };

layout(location = 1) out flat int InstanceIndex;

#else

layout(push_constant) uniform PushConstants
{
	uint LightStart;
	uint LightEnd;
	int SurfaceIndex;
	int PushPadding1;
	vec3 WorldToLocal;
	float TextureSize;
	vec3 ProjLocalToU;
	float PushPadding2;
	vec3 ProjLocalToV;
	float PushPadding3;
	float TileX;
	float TileY;
	float TileWidth;
	float TileHeight;
};

#endif

layout(location = 0) in vec3 aPosition;
layout(location = 0) out vec3 worldpos;

void main()
{
#if defined(USE_DRAWINDIRECT)
	vec3 WorldToLocal = constants[gl_InstanceIndex].WorldToLocal;
	float TextureSize = constants[gl_InstanceIndex].TextureSize;
	vec3 ProjLocalToU = constants[gl_InstanceIndex].ProjLocalToU;
	vec3 ProjLocalToV = constants[gl_InstanceIndex].ProjLocalToV;
	float TileX = constants[gl_InstanceIndex].TileX;
	float TileY = constants[gl_InstanceIndex].TileY;
	float TileWidth = constants[gl_InstanceIndex].TileWidth;
	float TileHeight = constants[gl_InstanceIndex].TileHeight;
	InstanceIndex = gl_InstanceIndex;
#endif

	worldpos = aPosition;

	// Project to position relative to tile
	vec3 localPos = aPosition - WorldToLocal;
	float x = dot(localPos, ProjLocalToU);
	float y = dot(localPos, ProjLocalToV);

	// Find the position in the output texture
	gl_Position = vec4(vec2(TileX + x, TileY + y) / TextureSize * 2.0 - 1.0, 0.0, 1.0);

	// Clip all surfaces to the edge of the tile (effectly we are applying a viewport/scissor to the tile)
	gl_ClipDistance[0] = x;
	gl_ClipDistance[1] = y;
	gl_ClipDistance[2] = TileWidth + 2.0 - x;
	gl_ClipDistance[3] = TileHeight + 2.0 - y;
}
