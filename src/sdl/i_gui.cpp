
// Moved from sdl/i_system.cpp

#include <string.h>

#include <SDL.h>

#include "bitmap.h"
#include "v_palette.h"
#include "textures.h"

#ifdef USE_XCURSOR
// Xlib has its own GC, so don't let it interfere.
#define GC XGC
#include <X11/Xcursor/Xcursor.h>
#undef GC

bool UseXCursor;
SDL_Cursor *X11Cursor;
SDL_Cursor *FirstCursor;

SDL_Cursor *CreateColorCursor(FTexture *cursorpic)
{
	return NULL;
}
#endif

bool I_SetCursor(FTexture *cursorpic)
{
	static SDL_Cursor *cursor;
	static SDL_Surface *cursorSurface;

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

		SDL_LockSurface(cursorSurface);
		BYTE buffer[32*32*4];
		memset(buffer, 0, 32*32*4);
		FBitmap bmp(buffer, 32*4, 32, 32);
		cursorpic->CopyTrueColorPixels(&bmp, 0, 0);
		memcpy(cursorSurface->pixels, bmp.GetPixels(), 32*32*4);
		SDL_UnlockSurface(cursorSurface);

		if (cursor)
			SDL_FreeCursor (cursor);
		cursor = SDL_CreateColorCursor (cursorSurface, 0, 0);
		SDL_SetCursor (cursor);
	}
	else
	{
		if (cursor)
		{
			SDL_SetCursor (NULL);
			SDL_FreeCursor (cursor);
			cursor = NULL;
		}
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
