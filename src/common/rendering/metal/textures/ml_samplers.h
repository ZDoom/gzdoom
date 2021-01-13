//
//---------------------------------------------------------------------------
//
// Copyright(C) 2020-2021 Eugene Grigorchuk
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

#pragma once
#include "metal/system/MetalCore.h"

namespace MetalRenderer
{
    class MTLSamplerManager
    {
        OBJC_ID(MTLDevice)        mDevice;
        MTLSamplerDescriptor* mDesc[7];
        bool mRepeatMode : 1;
        // We need 6 different samplers: 4 for the different clamping modes,
        // one for 2D-textures and one for voxel textures
        
        //void UnbindAll();
        
        void CreateDesc();
        void CreateSamplers();
        void DestroySamplers();
        void DestroyDesc();
        void Destroy();
        
    public:
        OBJC_ID(MTLSamplerState) mSamplers[7];
        OBJC_ID(MTLSamplerState) currentSampler;
        MTLSamplerManager(OBJC_ID(MTLDevice) device);
        ~MTLSamplerManager(){Destroy();};
        
        uint8_t Bind(int texunit, int num, int lastval);
        void BindToShader(OBJC_ID(MTLRenderCommandEncoder) encoder);
        void SetTextureFilterMode();
        void SetRepeatAddressMode(bool val);
        
        OBJC_ID(MTLSamplerState) Get(int no) const { return mSamplers[no]; }
        
    };

}
