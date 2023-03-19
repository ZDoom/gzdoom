
layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

layout(location = 0) out vec4 vTexCoord;
layout(location = 1) out vec4 vColor;
layout(location = 9) out vec3 vLightmap;

#ifndef SIMPLE	// we do not need these for simple shaders
layout(location = 3) in vec4 aVertex2;
layout(location = 4) in vec4 aNormal;
layout(location = 5) in vec4 aNormal2;
layout(location = 6) in vec3 aLightmap;
layout(location = 7) in vec4 aBoneWeight;
layout(location = 8) in uvec4 aBoneSelector;

layout(location = 2) out vec4 pixelpos;
layout(location = 3) out vec3 glowdist;
layout(location = 4) out vec3 gradientdist;
layout(location = 5) out vec4 vWorldNormal;
layout(location = 6) out vec4 vEyeNormal;
#endif

#ifdef NO_CLIPDISTANCE_SUPPORT
layout(location = 7) out vec4 ClipDistanceA;
layout(location = 8) out vec4 ClipDistanceB;
#endif
