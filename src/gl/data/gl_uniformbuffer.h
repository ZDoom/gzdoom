#pragma once

#include <stdlib.h>
#include "hwrenderer/data/uniformbuffer.h"


class GLUniformBuffer : public IUniformBuffer
{
	unsigned mBufferId;
	bool mStaticDraw;

public:
	GLUniformBuffer(size_t size, bool staticdraw = false);
	~GLUniformBuffer();

	void SetData(const void *data) override;
	void Bind(int bindingpoint) override;

	unsigned ID() const
	{
		return mBufferId;
	}

};
