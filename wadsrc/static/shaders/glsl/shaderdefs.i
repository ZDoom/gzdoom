// This file contains common data definitions for both vertex and fragment shader

uniform vec4 uCameraPos;

uniform int uTextureMode;

// colors
uniform vec4 uObjectColor;
uniform vec4 uDynLightColor;
uniform vec4 uFogColor;
uniform float uDesaturationFactor;

// Fixed colormap stuff
uniform int uFixedColormap;				// 0, when no fixed colormap, 1 for a light value, 2 for a color blend, 3 for a fog layer
uniform vec4 uFixedColormapStart;
uniform vec4 uFixedColormapRange;

// Glowing walls stuff
uniform vec4 uGlowTopPlane;
uniform vec4 uGlowTopColor;
uniform vec4 uGlowBottomPlane;
uniform vec4 uGlowBottomColor;

// Lighting + Fog
uniform vec4 uLightAttr;
#define uLightLevel uLightAttr.a
#define uFogDensity uLightAttr.b
#define uLightFactor uLightAttr.g
#define uLightDist uLightAttr.r
uniform int uFogEnabled;

// dynamic lights
uniform ivec4 uLightRange;


// redefine the matrix names to what they actually represent.
#define ModelMatrix  gl_TextureMatrix[7]
#define ViewMatrix gl_ModelViewMatrix
#define ProjectionMatrix gl_ProjectionMatrix
#define TextureMatrix gl_TextureMatrix[0]

