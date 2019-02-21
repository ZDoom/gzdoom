
#include "vk_renderstate.h"
#include "vulkan/system/vk_framebuffer.h"
#include "templates.h"
#include "doomstat.h"
#include "r_data/colormaps.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"

void VkRenderState::ClearScreen()
{
	screen->mViewpoints->Set2D(*this, SCREENWIDTH, SCREENHEIGHT);
	SetColor(0, 0, 0);
	Apply(DT_TriangleStrip);

	/*
	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);
	mCommandBuffer->draw(4, 1, FFlatVertexBuffer::FULLSCREEN_INDEX, 0);
	glEnable(GL_DEPTH_TEST);
	*/
}

void VkRenderState::Draw(int dt, int index, int count, bool apply)
{
	if (apply)
	{
		Apply(dt);
	}
	drawcalls.Clock();
	//mCommandBuffer->draw(count, 1, index, 0);
	drawcalls.Unclock();
}

void VkRenderState::DrawIndexed(int dt, int index, int count, bool apply)
{
	if (apply)
	{
		Apply(dt);
	}
	drawcalls.Clock();
	//mCommandBuffer->drawIndexed(count, 1, 0, index * (int)sizeof(uint32_t), 0);
	drawcalls.Unclock();
}

bool VkRenderState::SetDepthClamp(bool on)
{
	bool lastValue = mLastDepthClamp;
	mLastDepthClamp = on;
	return lastValue;
}

void VkRenderState::SetDepthMask(bool on)
{
}

void VkRenderState::SetDepthFunc(int func)
{
}

void VkRenderState::SetDepthRange(float min, float max)
{
}

void VkRenderState::SetColorMask(bool r, bool g, bool b, bool a)
{
}

void VkRenderState::EnableDrawBufferAttachments(bool on)
{
}

void VkRenderState::SetStencil(int offs, int op, int flags)
{
}

void VkRenderState::SetCulling(int mode)
{
}

void VkRenderState::EnableClipDistance(int num, bool state)
{
}

void VkRenderState::Clear(int targets)
{
}

void VkRenderState::EnableStencil(bool on)
{
}

void VkRenderState::SetScissor(int x, int y, int w, int h)
{
}

void VkRenderState::SetViewport(int x, int y, int w, int h)
{
}

void VkRenderState::EnableDepthTest(bool on)
{
}

void VkRenderState::EnableMultisampling(bool on)
{
}

void VkRenderState::EnableLineSmooth(bool on)
{
}

void VkRenderState::Apply(int dt)
{
#if 0
	auto fb = GetVulkanFrameBuffer();

	bool changingRenderPass = false; // To do: decide if the state matches current bound renderpass

	if (!mCommandBuffer)
	{
		mCommandBuffer = fb->GetDrawCommands();
		changingRenderPass = true;
	}
	else if (changingRenderPass)
	{
		mCommandBuffer->endRenderPass();
	}

	if (changingRenderPass)
	{
		RenderPassBegin beginInfo;
		beginInfo.setRenderPass(renderPass);
		beginInfo.setRenderArea(0, 0, SCREENWIDTH, SCREENHEIGHT);
		beginInfo.setFramebuffer(framebuffer);
		beginInfo.addClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		beginInfo.addClearDepthStencil(0.0f, 0);
		mCommandBuffer->beginRenderPass(beginInfo);
		mCommandBuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	}

	// To do: maybe only push the subset that changed?
	mCommandBuffer->pushConstants(pipelineLayout, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, (uint32_t)sizeof(PushConstants), &mPushConstants);
#endif
}
