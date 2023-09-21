
layout(push_constant) uniform PushConstants
{
	int SrcTexSize;
	int DestTexSize;
	int Padding1;
	int Padding2;
};

struct TileCopy
{
	ivec2 SrcPos;
	ivec2 DestPos;
	ivec2 TileSize;
	int Padding1, Padding2;
};

layout(std430, set = 0, binding = 1) buffer CopyBuffer
{
	TileCopy tiles[];
};

layout(location = 0) out vec2 TexCoord;

vec2 positions[4] = vec2[](
	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 1.0)
);

void main()
{
	TileCopy tile = tiles[gl_InstanceIndex];
	vec2 uv = positions[gl_VertexIndex];
	vec2 src = (vec2(tile.SrcPos) + uv * vec2(tile.TileSize)) / float(SrcTexSize);
	vec2 dest = (vec2(tile.DestPos) + uv * vec2(tile.TileSize)) / float(DestTexSize);

	gl_Position = vec4(dest * 2.0 - 1.0, 0.0, 1.0);
	TexCoord = src;
}
