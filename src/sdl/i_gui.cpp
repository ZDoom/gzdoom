
// Moved from sdl/i_system.cpp

#include <string.h>

#include <SDL.h>

#include "bitmap.h"
#include "v_palette.h"
#include "textures.h"


extern SDL_Surface *cursorSurface;
extern SDL_Rect cursorBlit;


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
