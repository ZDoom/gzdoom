
#include "vk_renderstate.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "templates.h"
#include "doomstat.h"
#include "r_data/colormaps.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/scene/hw_viewpointuniforms.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/data/shaderuniforms.h"

VkRenderState::VkRenderState()
{
}

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
		Apply(dt);
	else if (mDescriptorsChanged)
		BindDescriptorSets();

	drawcalls.Clock();
	//mCommandBuffer->draw(count, 1, index, 0);
	drawcalls.Unclock();
}

void VkRenderState::DrawIndexed(int dt, int index, int count, bool apply)
{
	if (apply)
		Apply(dt);
	else if (mDescriptorsChanged)
		BindDescriptorSets();

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
	auto fb = GetVulkanFrameBuffer();
	auto passManager = fb->GetRenderPassManager();
	auto passSetup = passManager->RenderPassSetup.get();

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
		beginInfo.setRenderPass(passSetup->RenderPass.get());
		beginInfo.setRenderArea(0, 0, SCREENWIDTH, SCREENHEIGHT);
		beginInfo.setFramebuffer(passSetup->Framebuffer.get());
		beginInfo.addClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		beginInfo.addClearDepthStencil(0.0f, 0);
		mCommandBuffer->beginRenderPass(beginInfo);
		mCommandBuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->Pipeline.get());
	}

	// To do: maybe only push the subset that changed?
	static_assert(sizeof(PushConstants) % 16 == 0, "std140 layouts must be 16 byte aligned");
	//mCommandBuffer->pushConstants(passManager->PipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t)sizeof(PushConstants), &mPushConstants);

	VkBuffer vertexBuffers[] = { static_cast<VKVertexBuffer*>(mVertexBuffer)->mBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };
	mCommandBuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);

	mCommandBuffer->bindIndexBuffer(static_cast<VKIndexBuffer*>(mIndexBuffer)->mBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

	BindDescriptorSets();
}

void VkRenderState::Bind(int bindingpoint, uint32_t offset)
{
	if (bindingpoint == VIEWPOINT_BINDINGPOINT)
	{
		mViewpointOffset = offset;
	}
	else if (bindingpoint == LIGHTBUF_BINDINGPOINT)
	{
		mLightBufferOffset = offset;
	}

	mDescriptorsChanged = true;
}

void VkRenderState::BindDescriptorSets()
{
	auto fb = GetVulkanFrameBuffer();
	auto passManager = fb->GetRenderPassManager();

	uint32_t offsets[2] = { mViewpointOffset, mLightBufferOffset };
	mCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, passManager->PipelineLayout.get(), 0, passManager->DynamicSet.get(), 2, offsets);
	mDescriptorsChanged = false;
}

void VkRenderState::EndRenderPass()
{
	if (mCommandBuffer)
	{
		mCommandBuffer->endRenderPass();
		mCommandBuffer = nullptr;
	}
}
