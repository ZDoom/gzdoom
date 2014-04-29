// This file contains common data definitions for both vertex and fragment shader

uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;
uniform vec4 uCameraPos;
uniform float uClipHeight;
uniform int uLightMode;
uniform int uFogMode;
uniform int uFixedColormap;				// 0, when no fixed colormap, 1 for a light value, 2 for a color blend
uniform vec4 uFixedColormapStart;
uniform vec4 uFixedColormapRange;


layout(std430, binding = 3) buffer ParameterBuffer
{
	readonly vec4 Parameters[];
};


#define SPECMODE_DEFAULT 0
#define SPECMODE_INFRARED 1
#define SPECMODE_FOGLAYER 2

// settings that can't affect primitive batching for non-translucent stuff

uniform float uAlphaThreshold;		// replacement for hardware alpha testing
uniform int uSpecialMode;			// enables a few special rendering effects (currently: fog layer for subtractive sprites and inverse drawing of sprites in infrared mode)
