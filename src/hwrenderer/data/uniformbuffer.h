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


};
