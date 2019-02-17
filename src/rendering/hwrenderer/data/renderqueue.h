#pragma once

#include <stdint.h>
#include "tflags.h"

// A render queue is what contains all render commands.
// On Vulkan there can be several of them so this interface is needed to allow for the needed parallelism.
// On OpenGL the render state is global so all this will do is to translate the system independent calls into OpenGL API calls.

enum class ColormaskBits
{
  RED = 1,
  GREEN = 2,
  BLUE = 4,
  ALPHA = 8
};

typedef TFlags<ColormaskBits, uint8_t> Colormask;

class IRenderQueue
{
	Colormask mColorMask;


	Colormask GetColorMask() const
	{
		return mColorMask;
	}

	virtual void SetColorMask(Colormask mask) = 0;


};
