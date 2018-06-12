#pragma once

#include <stdlib.h>
#include "hwrenderer/data/uniformbuffer.h"


class GLUniformBuffer : public IUniformBuffer
{
	unsigned mBufferId;
	bool mStaticDraw;

protected:
	GLUniformBuffer(size_t size, bool staticdraw = false);
	~GLUniformBuffer();

	void Bind() override;
	void SetData(const void *data) override;

	unsigned ID() const
	{
		return mBufferId;
	}

}
