#pragma once

#include "zstring.h"

class IUniformBuffer
{
protected:
	size_t mSize;

	IUniformBuffer(size_t size) : mSize(size) {}

public:
	virtual ~IUniformBuffer() {}
	virtual void SetData(const void *data) = 0;
	virtual void Bind(int bindingpoint) = 0;	// This is only here for OpenGL. Vulkan doesn't need the ability to bind this at run time.


};
