// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2010-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_framebuffer.cpp
** Implementation of the non-hardware specific parts of the
** OpenGL frame buffer
**
*/
#include "volk/volk.h"

#include "v_video.h"
#include "m_png.h"
#include "templates.h"
#include "r_videoscale.h"
#include "actor.h"

#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/utility/hw_vrmodes.h"

#include "vk_framebuffer.h"

EXTERN_CVAR(Bool, vid_vsync)


void vulkantest_startup(VulkanDevice *);
void vulkantest_tick();
void vulkantest_present();
void vulkantest_closeit();

//==========================================================================
//
//
//
//==========================================================================

VulkanFrameBuffer::VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev) : 
	Super(hMonitor, fullscreen) 
{
	device = dev;
	vulkantest_startup(dev);
}

VulkanFrameBuffer::~VulkanFrameBuffer()
{
	vulkantest_closeit();
}

//==========================================================================
//
// Initializes the GL renderer
//
//==========================================================================

void VulkanFrameBuffer::InitializeState()
{
	static bool first=true;

	if (first)
	{
	}

	//...
	SetViewportRects(nullptr);
}

//==========================================================================
//
// Updates the screen
//
//==========================================================================

void VulkanFrameBuffer::Update()
{
	twoD.Reset();
	Flush3D.Reset();

	DrawRateStuff();
	Flush3D.Clock();
	vulkantest_tick();
	Flush3D.Unclock();

	Swap();
	CheckBench();

	int initialWidth = GetClientWidth();
	int initialHeight = GetClientHeight();
	int clientWidth = ViewportScaledWidth(initialWidth, initialHeight);
	int clientHeight = ViewportScaledHeight(initialWidth, initialHeight);
	if (clientWidth > 0 && clientHeight > 0 && (GetWidth() != clientWidth || GetHeight() != clientHeight))
	{
		SetVirtualSize(clientWidth, clientHeight);
		V_OutputResized(clientWidth, clientHeight);
		//GLRenderer->mVBO->OutputResized(clientWidth, clientHeight);
	}
}

//===========================================================================
//
// 
//
//===========================================================================

void VulkanFrameBuffer::RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV)
{
	if (!V_IsHardwareRenderer())
	{
		Super::RenderTextureView(tex, Viewpoint, FOV);
	}
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void VulkanFrameBuffer::WriteSavePic(player_t *player, FileWriter *file, int width, int height)
{
	if (!V_IsHardwareRenderer())
		Super::WriteSavePic(player, file, width, height);
}

//===========================================================================
//
// 
//
//===========================================================================

sector_t *VulkanFrameBuffer::RenderView(player_t *player)
{
	return nullptr;
}



//===========================================================================
//
// 
//
//===========================================================================

EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)

uint32_t VulkanFrameBuffer::GetCaps()
{
	if (!V_IsHardwareRenderer())
		return Super::GetCaps();

	// describe our basic feature set
	ActorRenderFeatureFlags FlagSet = RFF_FLATSPRITES | RFF_MODELS | RFF_SLOPE3DFLOORS |
		RFF_TILTPITCH | RFF_ROLLSPRITES | RFF_POLYGONAL | RFF_MATSHADER | RFF_POSTSHADER | RFF_BRIGHTMAP;
	if (r_drawvoxels)
		FlagSet |= RFF_VOXELS;

	if (gl_tonemap != 5) // not running palette tonemap shader
		FlagSet |= RFF_TRUECOLOR;

	return (uint32_t)FlagSet;
}

//==========================================================================
//
// Swap the buffers
//
//==========================================================================

void VulkanFrameBuffer::Swap()
{
	vulkantest_present();
}

//==========================================================================
//
// Enable/disable vertical sync
//
//==========================================================================

void VulkanFrameBuffer::SetVSync(bool vsync)
{
}

//===========================================================================
//
//
//===========================================================================

void VulkanFrameBuffer::CleanForRestart()
{
}

void VulkanFrameBuffer::SetTextureFilterMode()
{
}

IHardwareTexture *VulkanFrameBuffer::CreateHardwareTexture(FTexture *tex) 
{ 
	return nullptr;
}

FModelRenderer *VulkanFrameBuffer::CreateModelRenderer(int mli) 
{
	return nullptr;
}

IUniformBuffer *VulkanFrameBuffer::CreateUniformBuffer(size_t size, bool staticuse)
{
	return nullptr;
}

IShaderProgram *VulkanFrameBuffer::CreateShaderProgram() 
{ 
	return nullptr;
}


void VulkanFrameBuffer::UnbindTexUnit(int no)
{
}

void VulkanFrameBuffer::TextureFilterChanged()
{
}

void VulkanFrameBuffer::BlurScene(float amount)
{
}

void VulkanFrameBuffer::InitForLevel()
{
}

void VulkanFrameBuffer::UpdatePalette()
{
}


//===========================================================================
//
// 
//
//===========================================================================

void VulkanFrameBuffer::SetClearColor(int color)
{
}


void VulkanFrameBuffer::BeginFrame()
{
}

//===========================================================================
// 
//	Takes a screenshot
//
//===========================================================================

void VulkanFrameBuffer::GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma)
{
}

//===========================================================================
// 
// 2D drawing
//
//===========================================================================

void VulkanFrameBuffer::Draw2D()
{
}
