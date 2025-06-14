#pragma once

#include <string>
#include <vector>

struct SDL_Window;

class SDL2NativeHandle
{
public:
	SDL_Window* window = nullptr;
};
