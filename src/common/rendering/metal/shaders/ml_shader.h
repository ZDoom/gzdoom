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

#include <memory>
#include "utility/vectors.h"
#include "matrix.h"
#include "name.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include "hwrenderer/data/flatvertices.h"
#include "metal/system/ml_buffer.h"

#import <simd/simd.h>
#include "metal/system/MetalCore.h"


namespace MetalRenderer
{

class FUniform1i
{
public:
    int val;
    void Set(int v)
    {val = v;}
};

class FBufferedUniform1i
{
public:
    int val;
    void Set(int v)
    {val = v;}
};

class FBufferedUniform4i
{
public:
    vector_int4 val;
    void Set(vector_int4 v)
    {val = v;}
};

class FBufferedUniform1f
{
public:
    float val;
    void Set(float v)
    {val = v;}
};

class FBufferedUniform2f
{
public:
    vector_float2 val;
    void Set(vector_float2 v)
    {val = v;}
};

class FBufferedUniform4f
{
public:
    vector_float4 val;
    void Set(vector_float4 v)
    {val = v;}
};

class FUniform4f
{
public:
    vector_float4 val;
    void Set(vector_float4 v)
    {val = v;}
};

class FBufferedUniformPE
{
public:
    FVector4PalEntry mBuffer;
    void Set(FVector4PalEntry v)
    {mBuffer = v;}
};

class MetalMatrixFloat4x4
{
public:
    matrix_float4x4 mat;
    void matrixToMetal (VSMatrix vs)
    {
        mat = matrix_float4x4{simd_float4{vs.mMatrix[0] ,vs.mMatrix[1] ,vs.mMatrix[2], vs.mMatrix[3] },
                              simd_float4{vs.mMatrix[4] ,vs.mMatrix[5] ,vs.mMatrix[6], vs.mMatrix[7] },
                              simd_float4{vs.mMatrix[8] ,vs.mMatrix[9] ,vs.mMatrix[10],vs.mMatrix[11]},
                              simd_float4{vs.mMatrix[12],vs.mMatrix[13],vs.mMatrix[14],vs.mMatrix[15]}};
    }
};

class MTLShader
{
    friend class MTLShaderCollection;
    friend class MTLRenderState;

    unsigned int hShader;
    unsigned int hVertProg;
    unsigned int hFragProg;
    FName mName;

    FBufferedUniform1f muDesaturation;
    FBufferedUniform1i muFogEnabled;
    FBufferedUniform1i muTextureMode;
    FBufferedUniform4f muLightParms;
    FBufferedUniform2f muClipSplit;
    FBufferedUniform1i muLightIndex;
    FBufferedUniformPE muFogColor;
    FBufferedUniform4f muDynLightColor;
    FBufferedUniformPE muObjectColor;
    FBufferedUniformPE muObjectColor2;
    FBufferedUniformPE muAddColor;
    FUniform4f muGlowBottomColor;
    FUniform4f muGlowTopColor;
    FUniform4f muGlowBottomPlane;
    FUniform4f muGlowTopPlane;
    FUniform4f muGradientBottomPlane;
    FUniform4f muGradientTopPlane;
    FUniform4f muSplitBottomPlane;
    FUniform4f muSplitTopPlane;
    FBufferedUniform1f muInterpolationFactor;
    FBufferedUniform1f muAlphaThreshold;
    FBufferedUniform2f muSpecularMaterial;
    FBufferedUniform1f muTimer;
    
    int lights_index;
    MetalMatrixFloat4x4 modelmatrix;
    MetalMatrixFloat4x4 normalmodelmatrix;
    MetalMatrixFloat4x4 texturematrix;

    int currentglowstate = 0;
    int currentgradientstate = 0;
    int currentsplitstate = 0;
    int currentcliplinestate = 0;
    int currentfixedcolormap = 0;
    bool currentTextureMatrixState = true;// by setting the matrix state to 'true' it is guaranteed to be set the first time the render state gets applied.
    bool currentModelMatrixState = true;

public:
    
    MTLShader(const char *name)
        : mName(name)
    {
        hShader = hVertProg = hFragProg = 0;
    }
    
    MTLShader() = default;
    virtual ~MTLShader(){};

    bool Load(const char * name, const char * vert_prog_lump, const char * fragprog, const char * fragprog2, const char * light_fragprog, const char *defines);
    bool Bind();
    unsigned int GetHandle() const { return hShader; }
};


struct MatricesUBO
{
	VSMatrix ModelMatrix;
	VSMatrix NormalModelMatrix;
	VSMatrix TextureMatrix;
};

class ShaderFunctions
{
public:
    ShaderFunctions() = default;
    ShaderFunctions(OBJC_ID(MTLFunction) frag, OBJC_ID(MTLFunction) vert)
    {
        vertFunc = vert;
        fragFunc = frag;
    };

    void setVertexFunction(NSString* name)
    {
        vertFunc = [lib newFunctionWithName:name];
    };
    
    void setFragmentFunction(NSString* name)
    {
        fragFunc = [lib newFunctionWithName:name];
    };
    
private:
    OBJC_ID(MTLFunction) vertFunc;
    OBJC_ID(MTLFunction) fragFunc;
    OBJC_ID(MTLLibrary)  lib;
};

class MTLShaderProgram
{
public:
    ShaderFunctions *funcs;
    OBJC_ID(MTLBuffer) uniforms;
    
    
    MTLShaderProgram()
    {
        funcs = new ShaderFunctions();
    };
    
    ~MTLShaderProgram()
    {
        delete funcs;
    };
    
    void SetShaderFunctions(NSString* vert, NSString* frag)
    {
        funcs->setVertexFunction(vert);
        funcs->setFragmentFunction(frag);
    };
    
    void SetVertexFunctions(NSString* vert)
    {
        funcs->setVertexFunction(vert);
    };
    
    void SetFragmentFunctions(NSString* frag)
    {
        funcs->setFragmentFunction(frag);
    };
    
};

class MTLShaderCollection
{
    TArray<MTLShader*> mMaterialShaders;
    TArray<MTLShader*> mMaterialShadersNAT;
    MTLShader *mEffectShaders[MAX_EFFECTS];

    void Clean();
    void CompileShaders(EPassType passType);
    
public:
    MTLShaderCollection(EPassType passType);
    ~MTLShaderCollection();
    MTLShader *Compile(const char *ShaderName, const char *ShaderPath, const char *LightModePath, const char *shaderdefines, bool usediscard, EPassType passType);
    int Find(const char *mame);
    MTLShader *BindEffect(int effect);

    MTLShader *Get(unsigned int eff, bool alphateston)
    {
        // indices 0-2 match the warping modes, 3 is brightmap, 4 no texture, the following are custom
        if (!alphateston && eff <= 3)
        {
            return mMaterialShadersNAT[eff];    // Non-alphatest shaders are only created for default, warp1+2 and brightmap. The rest won't get used anyway
        }
        if (eff < mMaterialShaders.Size())
        {
            return mMaterialShaders[eff];
        }
        return nullptr;
    }
};

class MTLShaderManager
{
public:
	MTLShaderManager() = default;
	~MTLShaderManager() = default;

    MTLShader *BindEffect(int effect, EPassType passType);
    MTLShader *Get(unsigned int eff, bool alphateston, EPassType passType);

private:
    
    void SetActiveShader(MTLShader *sh);
    MTLShader *mActiveShader = nullptr;
    TArray<MTLShaderCollection*> mPassShaders;

    friend class MTLShader;

};

}
