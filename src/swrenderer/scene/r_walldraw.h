
#pragma once

class FTexture;
struct FLightNode;

namespace swrenderer
{
	struct WallSampler
	{
		WallSampler() { }
		WallSampler(int y1, float swal, double yrepeat, fixed_t xoffset, double xmagnitude, FTexture *texture, const BYTE*(*getcol)(FTexture *texture, int x));

		uint32_t uv_pos;
		uint32_t uv_step;
		uint32_t uv_max;

		const BYTE *source;
		const BYTE *source2;
		uint32_t texturefracx;
		uint32_t height;
	};

	void R_DrawWallSegment(FTexture *rw_pic, int x1, int x2, short *walltop, short *wallbottom, float *swall, fixed_t *lwall, double yscale, double top, double bottom, bool mask, FLightNode *light_list = nullptr);
	void R_DrawSkySegment(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, const uint8_t *(*getcol)(FTexture *tex, int col));
}
