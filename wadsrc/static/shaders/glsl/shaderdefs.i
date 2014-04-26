// This file contains common data definitions for both vertex and fragment shader

layout(std140, binding = 1) uniform FrameState
{
	mat4 ViewMatrix;
	mat4 ProjectionMatrix;
	vec4 mCameraPos;
	float mClipHeight;
	int mLightMode;
	int mFogMode;
	int mFixedColormap;				// 0, when no fixed colormap, 1 for a light value, 2 for a color blend
	vec4 mFixedColormapStart;
	vec4 mFixedColormapEnd;
};


layout(std430, binding = 3) buffer ParameterBuffer
{
	readonly vec4 Parameters[];
};

