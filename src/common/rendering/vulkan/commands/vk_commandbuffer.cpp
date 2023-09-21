/*
**  Vulkan backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "vk_commandbuffer.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/vk_renderstate.h"
#include "vulkan/vk_postprocess.h"
#include "vulkan/framebuffers/vk_framebuffer.h"
#include "vulkan/descriptorsets/vk_descriptorset.h"
#include <zvulkan/vulkanswapchain.h>
#include <zvulkan/vulkanbuilders.h>
#include "hw_clock.h"
#include "v_video.h"

extern int rendered_commandbuffers;
int current_rendered_commandbuffers;

extern bool gpuStatActive;
extern bool keepGpuStatActive;
extern FString gpuStatOutput;

VkCommandBufferManager::VkCommandBufferManager(VulkanRenderDevice* fb) : fb(fb)
{
	mCommandPool = CommandPoolBuilder()
		.QueueFamily(fb->GetDevice()->GraphicsFamily)
		.DebugName("mCommandPool")
		.Create(fb->GetDevice());

	for (auto& semaphore : mSubmitSemaphore)
		semaphore.reset(new VulkanSemaphore(fb->GetDevice()));

	for (auto& fence : mSubmitFence)
		fence.reset(new VulkanFence(fb->GetDevice()));

	for (int i = 0; i < maxConcurrentSubmitCount; i++)
		mSubmitWaitFences[i] = mSubmitFence[i]->fence;

	if (fb->GetDevice()->GraphicsTimeQueries)
	{
		mTimestampQueryPool = QueryPoolBuilder()
			.QueryType(VK_QUERY_TYPE_TIMESTAMP, MaxTimestampQueries)
			.Create(fb->GetDevice());

		GetDrawCommands()->resetQueryPool(mTimestampQueryPool.get(), 0, MaxTimestampQueries);
	}
}

VkCommandBufferManager::~VkCommandBufferManager()
{
}

VulkanCommandBuffer* VkCommandBufferManager::GetTransferCommands()
{
	if (!mTransferCommands)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mTransferCommands = mCommandPool->createBuffer();
		mTransferCommands->SetDebugName("TransferCommands");
		mTransferCommands->begin();
	}
	return mTransferCommands.get();
}

VulkanCommandBuffer* VkCommandBufferManager::GetDrawCommands()
{
	if (!mDrawCommands)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mDrawCommands = mCommandPool->createBuffer();
		mDrawCommands->SetDebugName("DrawCommands");
		mDrawCommands->begin();
	}
	return mDrawCommands.get();
}

std::unique_ptr<VulkanCommandBuffer> VkCommandBufferManager::BeginThreadCommands()
{
	std::unique_lock<std::mutex> lock(mMutex);
	auto commands = mCommandPool->createBuffer();
	commands->SetDebugName("ThreadCommands");
	lock.unlock();
	commands->begin();
	return commands;
}

void VkCommandBufferManager::EndThreadCommands(std::unique_ptr<VulkanCommandBuffer> commands)
{
	commands->end();
	std::unique_lock<std::mutex> lock(mMutex);
	mThreadCommands.push_back(std::move(commands));
}

void VkCommandBufferManager::BeginFrame()
{
	if (mNextTimestampQuery > 0)
	{
		GetTransferCommands()->resetQueryPool(mTimestampQueryPool.get(), 0, mNextTimestampQuery);
		mNextTimestampQuery = 0;
	}
}

void VkCommandBufferManager::FlushCommands(VulkanCommandBuffer** commands, size_t count, bool finish, bool lastsubmit)
{
	int currentIndex = mNextSubmit % maxConcurrentSubmitCount;

	if (mNextSubmit >= maxConcurrentSubmitCount)
	{
		vkWaitForFences(fb->GetDevice()->device, 1, &mSubmitFence[currentIndex]->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(fb->GetDevice()->device, 1, &mSubmitFence[currentIndex]->fence);
	}

	QueueSubmit submit;

	for (size_t i = 0; i < count; i++)
		submit.AddCommandBuffer(commands[i]);

	if (mNextSubmit > 0)
		submit.AddWait(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, mSubmitSemaphore[(mNextSubmit - 1) % maxConcurrentSubmitCount].get());

	if (finish && fb->GetFramebufferManager()->PresentImageIndex != -1)
	{
		submit.AddWait(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, fb->GetFramebufferManager()->SwapChainImageAvailableSemaphore.get());
		submit.AddSignal(fb->GetFramebufferManager()->RenderFinishedSemaphore.get());
	}

	if (!lastsubmit)
		submit.AddSignal(mSubmitSemaphore[currentIndex].get());

	submit.Execute(fb->GetDevice(), fb->GetDevice()->GraphicsQueue, mSubmitFence[currentIndex].get());
	mNextSubmit++;
}

void VkCommandBufferManager::FlushCommands(bool finish, bool lastsubmit, bool uploadOnly)
{
	fb->GetDescriptorSetManager()->UpdateBindlessDescriptorSet();

	if (!uploadOnly)
		fb->GetRenderState(0)->EndRenderPass();

	std::unique_lock<std::mutex> lock(mMutex);

	if (mTransferCommands)
	{
		mTransferCommands->end();
		mFlushCommands.push_back(mTransferCommands.get());
		TransferDeleteList->Add(std::move(mTransferCommands));
	}

	if (!uploadOnly)
	{
		if (mDrawCommands)
		{
			mDrawCommands->end();
			mFlushCommands.push_back(mDrawCommands.get());
			DrawDeleteList->Add(std::move(mDrawCommands));
		}

		for (auto& buffer : mThreadCommands)
		{
			mFlushCommands.push_back(buffer.get());
			DrawDeleteList->Add(std::move(buffer));
		}
		mThreadCommands.clear();
	}

	if (!mFlushCommands.empty())
	{
		FlushCommands(mFlushCommands.data(), mFlushCommands.size(), finish, lastsubmit);
		current_rendered_commandbuffers += (int)mFlushCommands.size();
		mFlushCommands.clear();
	}
}

void VkCommandBufferManager::WaitForCommands(bool finish, bool uploadOnly)
{
	if (finish)
	{
		Finish.Reset();
		Finish.Clock();

		fb->GetFramebufferManager()->AcquireImage();
	}

	FlushCommands(finish, true, uploadOnly);

	if (finish)
	{
		fb->FPSLimit();
		fb->GetFramebufferManager()->QueuePresent();
	}

	int numWaitFences = min(mNextSubmit, (int)maxConcurrentSubmitCount);

	if (numWaitFences > 0)
	{
		vkWaitForFences(fb->GetDevice()->device, numWaitFences, mSubmitWaitFences, VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(fb->GetDevice()->device, numWaitFences, mSubmitWaitFences);
	}

	DeleteFrameObjects(uploadOnly);
	mNextSubmit = 0;

	if (finish)
	{
		Finish.Unclock();
		rendered_commandbuffers = current_rendered_commandbuffers;
		current_rendered_commandbuffers = 0;
	}
}

void VkCommandBufferManager::DeleteFrameObjects(bool uploadOnly)
{
	TransferDeleteList = std::make_unique<DeleteList>();
	if (!uploadOnly)
		DrawDeleteList = std::make_unique<DeleteList>();
}

void VkCommandBufferManager::PushGroup(VulkanCommandBuffer* cmdbuffer, const FString& name)
{
	if (!gpuStatActive)
		return;

	if (mNextTimestampQuery < MaxTimestampQueries && fb->GetDevice()->GraphicsTimeQueries)
	{
		TimestampQuery q;
		q.name = name;
		q.startIndex = mNextTimestampQuery++;
		q.endIndex = 0;
		cmdbuffer->writeTimestamp(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, mTimestampQueryPool.get(), q.startIndex);
		mGroupStack.push_back(timeElapsedQueries.size());
		timeElapsedQueries.push_back(q);
	}
}

void VkCommandBufferManager::PopGroup(VulkanCommandBuffer* cmdbuffer)
{
	if (!gpuStatActive || mGroupStack.empty())
		return;

	TimestampQuery& q = timeElapsedQueries[mGroupStack.back()];
	mGroupStack.pop_back();

	if (mNextTimestampQuery < MaxTimestampQueries && fb->GetDevice()->GraphicsTimeQueries)
	{
		q.endIndex = mNextTimestampQuery++;
		cmdbuffer->writeTimestamp(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, mTimestampQueryPool.get(), q.endIndex);
	}
}

void VkCommandBufferManager::UpdateGpuStats()
{
	uint64_t timestamps[MaxTimestampQueries];
	if (mNextTimestampQuery > 0)
		mTimestampQueryPool->getResults(0, mNextTimestampQuery, sizeof(uint64_t) * mNextTimestampQuery, timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

	double timestampPeriod = fb->GetDevice()->PhysicalDevice.Properties.Properties.limits.timestampPeriod;

	gpuStatOutput = "";
	for (auto& q : timeElapsedQueries)
	{
		if (q.endIndex <= q.startIndex)
			continue;

		int64_t timeElapsed = max(static_cast<int64_t>(timestamps[q.endIndex] - timestamps[q.startIndex]), (int64_t)0);
		double timeNS = timeElapsed * timestampPeriod;

		FString out;
		out.Format("%s=%04.2f ms\n", q.name.GetChars(), timeNS / 1000000.0f);
		gpuStatOutput += out;
	}
	timeElapsedQueries.clear();
	mGroupStack.clear();

	gpuStatActive = keepGpuStatActive;
	keepGpuStatActive = false;
}
