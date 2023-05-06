
layout(set = 0, binding = 0) uniform sampler2D ShadowMap;
layout(set = 0, binding = 1) uniform sampler2DArray LightMap;
#if defined(USE_RAYTRACE)
#if defined(SUPPORTS_RAYQUERY)
layout(set = 0, binding = 2) uniform accelerationStructureEXT TopLevelAS;
#else
struct CollisionNode
{
	vec3 center;
	float padding1;
	vec3 extents;
	float padding2;
	int left;
	int right;
	int element_index;
	int padding3;
};
layout(std430, set = 0, binding = 2) buffer NodeBuffer
{
	int nodesRoot;
	int nodebufferPadding1;
	int nodebufferPadding2;
	int nodebufferPadding3;
	CollisionNode nodes[];
};
layout(std430, set = 0, binding = 3) buffer VertexBuffer { vec4 vertices[]; };
layout(std430, set = 0, binding = 4) buffer ElementBuffer { int elements[]; };
#endif
#endif

// This must match the HWViewpointUniforms struct
layout(set = 1, binding = 0, std140) uniform ViewpointUBO
{
	mat4 ProjectionMatrix;
	mat4 ViewMatrix;
	mat4 NormalViewMatrix;

	vec4 uCameraPos;
	vec4 uClipLine;

	float uGlobVis;			// uGlobVis = R_GetGlobVis(r_visibility) / 32.0
	int uPalLightLevels;	
	int uViewHeight;		// Software fuzz scaling
	float uClipHeight;
	float uClipHeightDirection;
	int uShadowmapFilter;
		
	int uLightBlendMode;
};

layout(set = 1, binding = 1, std140) uniform MatricesUBO
{
	mat4 ModelMatrix;
	mat4 NormalModelMatrix;
	mat4 TextureMatrix;
};

// This must match the StreamData struct
struct StreamData
{
	vec4 uObjectColor;
	vec4 uObjectColor2;
	vec4 uDynLightColor;
	vec4 uAddColor;
	vec4 uTextureAddColor;
	vec4 uTextureModulateColor;
	vec4 uTextureBlendColor;
	vec4 uFogColor;
	float uDesaturationFactor;
	float uInterpolationFactor;
	float timer; // timer data for material shaders
	int useVertexData;
	vec4 uVertexColor;
	vec4 uVertexNormal;

	vec4 uGlowTopPlane;
	vec4 uGlowTopColor;
	vec4 uGlowBottomPlane;
	vec4 uGlowBottomColor;

	vec4 uGradientTopPlane;
	vec4 uGradientBottomPlane;

	vec4 uSplitTopPlane;
	vec4 uSplitBottomPlane;

	vec4 uDetailParms;
	vec4 uNpotEmulation;

	vec2 uClipSplit;
	vec2 uSpecularMaterial;

	float uLightLevel;
	float uFogDensity;
	float uLightFactor;
	float uLightDist;

	float uAlphaThreshold;
	float padding1;
	float padding2;
	float padding3;
};

layout(set = 1, binding = 2, std140) uniform StreamUBO
{
	StreamData data[MAX_STREAM_DATA];
};

// light buffers
layout(set = 1, binding = 3, std140) uniform LightBufferUBO
{
	vec4 lights[MAX_LIGHT_DATA];
};

// bone matrix buffers
layout(set = 1, binding = 4, std430) buffer BoneBufferSSO
{
	mat4 bones[];
};

// textures
layout(set = 2, binding = 0) uniform sampler2D tex;
layout(set = 2, binding = 1) uniform sampler2D texture2;
layout(set = 2, binding = 2) uniform sampler2D texture3;
layout(set = 2, binding = 3) uniform sampler2D texture4;
layout(set = 2, binding = 4) uniform sampler2D texture5;
layout(set = 2, binding = 5) uniform sampler2D texture6;
layout(set = 2, binding = 6) uniform sampler2D texture7;
layout(set = 2, binding = 7) uniform sampler2D texture8;
layout(set = 2, binding = 8) uniform sampler2D texture9;
layout(set = 2, binding = 9) uniform sampler2D texture10;
layout(set = 2, binding = 10) uniform sampler2D texture11;

// This must match the PushConstants struct
layout(push_constant) uniform PushConstants
{
	int uDataIndex; // streamdata index
	int uLightIndex; // dynamic lights
	int uBoneIndexBase; // bone animation
	int padding;
};

// material types
#if defined(SPECULAR)
#define normaltexture texture2
#define speculartexture texture3
#define brighttexture texture4
#define detailtexture texture5
#define glowtexture texture6
#elif defined(PBR)
#define normaltexture texture2
#define metallictexture texture3
#define roughnesstexture texture4
#define aotexture texture5
#define brighttexture texture6
#define detailtexture texture7
#define glowtexture texture8
#else
#define brighttexture texture2
#define detailtexture texture3
#define glowtexture texture4
#endif

#define uObjectColor data[uDataIndex].uObjectColor
#define uObjectColor2 data[uDataIndex].uObjectColor2
#define uDynLightColor data[uDataIndex].uDynLightColor
#define uAddColor data[uDataIndex].uAddColor
#define uTextureBlendColor data[uDataIndex].uTextureBlendColor
#define uTextureModulateColor data[uDataIndex].uTextureModulateColor
#define uTextureAddColor data[uDataIndex].uTextureAddColor
#define uFogColor data[uDataIndex].uFogColor
#define uDesaturationFactor data[uDataIndex].uDesaturationFactor
#define uInterpolationFactor data[uDataIndex].uInterpolationFactor
#define timer data[uDataIndex].timer
#define useVertexData data[uDataIndex].useVertexData
#define uVertexColor data[uDataIndex].uVertexColor
#define uVertexNormal data[uDataIndex].uVertexNormal
#define uGlowTopPlane data[uDataIndex].uGlowTopPlane
#define uGlowTopColor data[uDataIndex].uGlowTopColor
#define uGlowBottomPlane data[uDataIndex].uGlowBottomPlane
#define uGlowBottomColor data[uDataIndex].uGlowBottomColor
#define uGradientTopPlane data[uDataIndex].uGradientTopPlane
#define uGradientBottomPlane data[uDataIndex].uGradientBottomPlane
#define uSplitTopPlane data[uDataIndex].uSplitTopPlane
#define uSplitBottomPlane data[uDataIndex].uSplitBottomPlane
#define uDetailParms data[uDataIndex].uDetailParms
#define uNpotEmulation data[uDataIndex].uNpotEmulation
#define uClipSplit data[uDataIndex].uClipSplit
#define uSpecularMaterial data[uDataIndex].uSpecularMaterial
#define uLightLevel data[uDataIndex].uLightLevel
#define uFogDensity data[uDataIndex].uFogDensity
#define uLightFactor data[uDataIndex].uLightFactor
#define uLightDist data[uDataIndex].uLightDist
#define uAlphaThreshold data[uDataIndex].uAlphaThreshold

#define VULKAN_COORDINATE_SYSTEM
#define HAS_UNIFORM_VERTEX_DATA

// GLSL spec 4.60, 8.15. Noise Functions
// https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.60.pdf
//  "The noise functions noise1, noise2, noise3, and noise4 have been deprecated starting with version 4.4 of GLSL.
//   When not generating SPIR-V they are defined to return the value 0.0 or a vector whose components are all 0.0.
//   When generating SPIR-V the noise functions are not declared and may not be used."
// However, we need to support mods with custom shaders created for OpenGL renderer
float noise1(float) { return 0; }
vec2 noise2(vec2) { return vec2(0); }
vec3 noise3(vec3) { return vec3(0); }
vec4 noise4(vec4) { return vec4(0); }
