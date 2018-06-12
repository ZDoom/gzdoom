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

	unsigned ID() const
	{
		return mBufferId;
	}

};
