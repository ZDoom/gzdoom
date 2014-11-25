
// Moved from sdl/i_system.cpp

#include <string.h>

#include <SDL.h>

#include "bitmap.h"
#include "v_palette.h"
#include "textures.h"

extern SDL_Surface *cursorSurface;
extern SDL_Rect cursorBlit;

#ifdef USE_XCURSOR
// Xlib has its own GC, so don't let it interfere.
#define GC XGC
#include <X11/Xcursor/Xcursor.h>
#undef GC

bool UseXCursor;
SDL_Cursor *X11Cursor;
SDL_Cursor *FirstCursor;

// Hack! Hack! SDL does not provide a clean way to get the XDisplay.
// On the other hand, there are no more planned updates for SDL 1.2,
// so we should be fine making assumptions.
struct SDL_PrivateVideoData
{
	int local_X11;
	Display *X11_Display;
};

struct SDL_VideoDevice
{
	const char *name;
	int (*functions[9])();
	SDL_VideoInfo info;
	SDL_PixelFormat *displayformatalphapixel;
	int (*morefuncs[9])();
	Uint16 *gamma;
	int (*somefuncs[9])();
	unsigned int texture;				// Only here if SDL was compiled with OpenGL support. Ack!
	int is_32bit;
	int (*itsafuncs[13])();
	SDL_Surface *surfaces[3];
	SDL_Palette *physpal;
	SDL_Color *gammacols;
	char *wm_strings[2];
	int offsets[2];
	SDL_GrabMode input_grab;
	int handles_any_size;
	SDL_PrivateVideoData *hidden;	// Why did they have to bury this so far in?
};

extern SDL_VideoDevice *current_video;
#define SDL_Display (current_video->hidden->X11_Display)

SDL_Cursor *CreateColorCursor(FTexture *cursorpic)
{
	return NULL;
}
#endif

bool I_SetCursor(FTexture *cursorpic)
{
	if (cursorpic != NULL && cursorpic->UseType != FTexture::TEX_Null)
	{
		// Must be no larger than 32x32.
		if (cursorpic->GetWidth() > 32 || cursorpic->GetHeight() > 32)
		{
			return false;
		}

#ifdef USE_XCURSOR
		if (UseXCursor)
		{
			if (FirstCursor == NULL)
			{
				FirstCursor = SDL_GetCursor();
			}
			X11Cursor = CreateColorCursor(cursorpic);
			if (X11Cursor != NULL)
			{
				SDL_SetCursor(X11Cursor);
				return true;
			}
		}
#endif
		if (cursorSurface == NULL)
			cursorSurface = SDL_CreateRGBSurface (0, 32, 32, 32, MAKEARGB(0,255,0,0), MAKEARGB(0,0,255,0), MAKEARGB(0,0,0,255), MAKEARGB(255,0,0,0));

		SDL_ShowCursor(0);
		SDL_LockSurface(cursorSurface);
		BYTE buffer[32*32*4];
		memset(buffer, 0, 32*32*4);
		FBitmap bmp(buffer, 32*4, 32, 32);
		cursorpic->CopyTrueColorPixels(&bmp, 0, 0);
		memcpy(cursorSurface->pixels, bmp.GetPixels(), 32*32*4);
		SDL_UnlockSurface(cursorSurface);
	}
	else
	{
		SDL_ShowCursor(1);

		if (cursorSurface != NULL)
		{
			SDL_FreeSurface(cursorSurface);
			cursorSurface = NULL;
		}
#ifdef USE_XCURSOR
		if (X11Cursor != NULL)
		{
			SDL_SetCursor(FirstCursor);
			SDL_FreeCursor(X11Cursor);
			X11Cursor = NULL;
		}
#endif
	}
	return true;
}

void I_SetMainWindowVisible(bool visible)
{

}

const char* I_GetBackEndName()
{
	return "SDL";
}
