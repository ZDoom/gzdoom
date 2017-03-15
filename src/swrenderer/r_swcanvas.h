
#pragma once

#include "v_video.h"
#include "r_data/colormaps.h"

class SWCanvas
{
public:
	static void DrawTexture(DCanvas *canvas, FTexture *img, DrawParms &parms);
	static void FillSimplePoly(DCanvas *canvas, FTexture *tex, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley, DAngle rotation,
		const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip);
	static void DrawLine(DCanvas *canvas, int x0, int y0, int x1, int y1, int palColor, uint32_t realcolor);
	static void DrawPixel(DCanvas *canvas, int x, int y, int palColor, uint32_t realcolor);
	static void Clear(DCanvas *canvas, int left, int top, int right, int bottom, int palcolor, uint32_t color);
	static void Dim(DCanvas *canvas, PalEntry color, float damount, int x1, int y1, int w, int h);

private:
	static void PUTTRANSDOT(DCanvas *canvas, int xx, int yy, int basecolor, int level);
	static int PalFromRGB(uint32_t rgb);
};
